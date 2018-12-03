/***************************************************************************************
Copyright (c) 2015, Swisscom (Switzerland) Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Swisscom nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Architecture and development:
Vladimir Maksovic <Vladimir.Maksovic@swisscom.com>
Milenko Boric Herget <Milenko.BoricHerget@swisscom.com>
Dario Vieceli <Dario.Vieceli@swisscom.com>
***************************************************************************************/


#include "set_reg.h"

#include "eos_macro.h"
#include "osi_memory.h"
#include "osi_mutex.h"
#include "osi_thread.h"
#include "util_msgq.h"
#include "util_slist.h"

#define MODULE_NAME ("settings")
#include "util_log.h"

typedef struct set_reg
{
	osi_mutex_t *lock;
	osi_thread_t *thread;
	util_msgq_t *q;
	util_slist_t listeners;
	util_slist_t items;
} set_reg_t;

typedef struct set_reg_lstnr
{
	uint32_t id;
	set_reg_cbk_t cbk;
	void* cookie;
} set_reg_lstnr_t;

typedef struct set_reg_item
{
	uint32_t id;
	set_reg_opt_t opt;
	set_reg_val_t val;
} set_reg_item_t;

typedef struct set_reg_sys
{
	set_reg_opt_t opt;
	uint32_t id;
	set_reg_cbk_t cbk;
	void *opaque;
} set_reg_sys_t;

static void* set_reg_thread(void* arg);
static void set_reg_free_item(void* item, size_t size);
static bool set_reg_comparator(void* search_param,
		void* data_address);
static bool set_reg_items_comparator(void* search_param,
		void* data_address);
static eos_error_t set_reg_item_store_values(set_reg_item_t* new_value);
static void set_reg_sys_handle(set_reg_opt_t opt, set_reg_val_t* val);

static set_reg_t reg = {NULL, NULL, NULL, {.priv = NULL}, {.priv = NULL}};
static set_reg_sys_t sys_reg[] =
{
		{SET_REG_OPT_VID_POS, 0, NULL, NULL},
		{SET_REG_OPT_VID_SIZE, 0, NULL, NULL},
		{SET_REG_OPT_AUD_MODE, 0, NULL, NULL},
		{SET_REG_OPT_VOL_LVL, 0, NULL, NULL},
		{SET_REG_OPT_LAST, 0, NULL, NULL}
};

CALL_ON_LOAD(set_reg_init)
static void set_reg_init(void)
{
	if(osi_mutex_create(&reg.lock) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Lock could not be created");
		return;
	}
	if(util_msgq_create(&reg.q, 10, set_reg_free_item) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Queue could not be created");
		return;
	}
	if(util_slist_create(&reg.listeners, set_reg_comparator) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("List could not be created");
		return;
	}
	if(util_slist_create(&reg.items, set_reg_items_comparator) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("List could not be created");
		return;
	}
	if(osi_thread_create(&reg.thread, NULL, set_reg_thread, &reg)
			!= EOS_ERROR_OK)
	{
		UTIL_GLOGE("Thread could not be created");
		return;
	}
}

CALL_ON_UNLOAD(set_reg_deinit)
static void set_reg_deinit(void)
{
	util_slist_destroy(&reg.items);
	if(reg.lock != NULL)
	{
		osi_mutex_destroy(&reg.lock);
	}
	if(reg.q != NULL)
	{
		util_msgq_pause(reg.q);
	}
	if(reg.thread != NULL)
	{
		osi_thread_join(reg.thread, NULL);
		osi_thread_release(&reg.thread);
	}
	if (util_slist_destroy(&reg.listeners) != EOS_ERROR_OK)
	{
		UTIL_GLOGW("Unable to destroy listeners list");
	}
	if(reg.q != NULL)
	{
		util_msgq_destroy(&reg.q);
	}

}

eos_error_t set_reg_set_sys_cbk(uint32_t id, set_reg_opt_t opt,
		set_reg_cbk_t cbk, void* opaque)
{
	int i = 0;
	set_reg_sys_t *item = NULL;

	for(i=0; i<SET_REG_OPT_LAST; i++)
	{
		if(sys_reg[i].opt == opt)
		{
			item = &sys_reg[i];
			break;
		}
	}
	if(item == NULL)
	{
		UTIL_GLOGE("Failed to find sys slot for %d", opt);

		return EOS_ERROR_NFOUND;
	}
	if(item->cbk != NULL && cbk != NULL)
	{
		UTIL_GLOGW("!!!!! WARNING: Sys reg for %d already set - "
				"OVERWRITE !!!!!", opt);
	}
	item->id = id;
	item->cbk = cbk;
	item->opaque = opaque;

	return EOS_ERROR_OK;
}

eos_error_t set_reg_fetch(uint32_t id, set_reg_opt_t opt, set_reg_val_t* val)
{
	eos_error_t err = EOS_ERROR_OK;
	set_reg_item_t *item = NULL;
	set_reg_item_t search;


	osi_memset(&search, 0, sizeof(set_reg_item_t));
	search.id = id;
	search.opt = opt;

	if(val == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	
	reg.items.get(reg.items, &search, (void**)&item);
	if(item == NULL)
	{
		UTIL_GLOGW("Fail to find item  for opt =  %d", opt);
		return EOS_ERROR_NFOUND;
	}
	else
	{		
		osi_memcpy(val, &item->val, sizeof(set_reg_val_t));
		return err;
	}

	return err;
}

eos_error_t set_reg_apply(uint32_t id, set_reg_opt_t opt, set_reg_val_t* val)
{
	set_reg_item_t *item = NULL;
	eos_error_t err = EOS_ERROR_OK;

	if(val == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	item = osi_malloc(sizeof(set_reg_item_t));
	if(item == NULL)
	{
		return EOS_ERROR_NOMEM;
	}
	item->id = id;
	item->opt = opt;
	item->val = *val;
	set_reg_sys_handle(opt, val);
	set_reg_item_store_values(item);

	if((err = util_msgq_put(reg.q, item, sizeof(set_reg_item_t), NULL))
			!= EOS_ERROR_OK)
	{
		osi_free((void**)&item);
		return err;
	}

	return EOS_ERROR_OK;
}

eos_error_t set_reg_add_cbk(uint32_t id, set_reg_cbk_t cbk, void* opaque)
{
	set_reg_lstnr_t *listener = NULL;
	set_reg_lstnr_t srch = {id, cbk, opaque};
	eos_error_t err = EOS_ERROR_OK;

	if(cbk == NULL || opaque == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	osi_mutex_lock(reg.lock);

	reg.listeners.get(reg.listeners, &srch, (void**)&listener);
	if(listener != NULL)
	{
		UTIL_GLOGW("Listener is already in list");
		osi_mutex_unlock(reg.lock);
		return EOS_ERROR_OK;
	}

	listener = osi_calloc(sizeof(set_reg_lstnr_t));
	if(listener == NULL)
	{
		return EOS_ERROR_NOMEM;
	}

	listener->id = id;
	listener->cookie = opaque;
	listener->cbk = cbk;

	err = reg.listeners.add(reg.listeners, listener);
	if (err == EOS_ERROR_OK)
	{
		UTIL_GLOGD("Added listener for (%d): %p", id, opaque);
	}

	osi_mutex_unlock(reg.lock);

	return err;
}

eos_error_t set_reg_remove_cbk(uint32_t id, set_reg_cbk_t cbk, void* opaque)
{
	set_reg_lstnr_t srch = {id, cbk, opaque};
	set_reg_lstnr_t *listener = NULL;
	eos_error_t err = EOS_ERROR_OK;

	if(reg.lock == NULL)
	{
		return EOS_ERROR_OK;
	}
	osi_mutex_lock(reg.lock);
	reg.listeners.get(reg.listeners, &srch, (void**)&listener);
	if(listener == NULL)
	{
		UTIL_GLOGW("Fail to find listener for (%d): %p", id, opaque);
		return EOS_ERROR_NFOUND;
	}
	err = reg.listeners.remove(reg.listeners, &srch);
	osi_free((void**)&listener);
	UTIL_GLOGD("Removed listener for (%d): %p", id, opaque);
	osi_mutex_unlock(reg.lock);

	return err;
}

static void* set_reg_thread(void* arg)
{
	set_reg_t *reg = (set_reg_t*)arg;
	set_reg_item_t *item = NULL;
	set_reg_lstnr_t *listener = NULL;
	eos_error_t err;
	size_t sz;

	while(util_msgq_get(reg->q, (void**)&item, &sz, NULL) == EOS_ERROR_OK)
	{
		osi_mutex_lock(reg->lock);
		err = reg->listeners.first(reg->listeners, (void**)&listener);
		if(err != EOS_ERROR_OK)
		{
			osi_mutex_unlock(reg->lock);
			continue;
		}
		do
		{
			if(listener->id == item->id)
			{
				osi_mutex_unlock(reg->lock);
				err = listener->cbk(item->opt, &item->val, listener->cookie);
				if(err != EOS_ERROR_OK)
				{
					UTIL_GLOGW("Callback returned error: %d", err);
				}
				osi_mutex_lock(reg->lock);
			}
			err = reg->listeners.next(reg->listeners, (void**)&listener);
		} while(err == EOS_ERROR_OK);
		osi_mutex_unlock(reg->lock);
		osi_free((void**)&item);
	}
	UTIL_GLOGI("Exiting setting thread");

	return NULL;
}

static void set_reg_free_item(void* item, size_t size)
{
	EOS_UNUSED(size);

	if(item != NULL)
	{
		osi_free(&item);
	}
}

static bool set_reg_comparator(void* search_param,
		void* data_address)
{
	set_reg_lstnr_t *search = NULL;
	set_reg_lstnr_t *cmp = NULL;

	if(search_param == NULL || data_address == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	search = (set_reg_lstnr_t*) search_param;
	cmp = (set_reg_lstnr_t*) data_address;

	return (cmp->id == search->id && cmp->cbk == search->cbk
			&& cmp->cookie == search->cookie);
}

static bool set_reg_items_comparator(void* search_param,
		void* data_address)
{
	set_reg_item_t *search = NULL;
	set_reg_item_t *cmp = NULL;

	if(search_param == NULL || data_address == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	search = (set_reg_item_t*) search_param;
	cmp = (set_reg_item_t*) data_address;
	return (cmp->opt == search->opt && cmp->id == search->id);
}


static eos_error_t set_reg_item_store_values(set_reg_item_t* new_value)
{
	eos_error_t err = EOS_ERROR_OK;
	set_reg_item_t *stored_item = NULL;

	if(new_value == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	reg.items.get(reg.items, new_value, (void**)&stored_item);
	if(stored_item == NULL)
	{
		stored_item = osi_calloc(sizeof(set_reg_item_t));
		if(stored_item == NULL)
		{
			return EOS_ERROR_NOMEM;
		}
		stored_item->id = new_value->id;
		stored_item->opt = new_value->opt;
		switch (new_value->opt)
		{
			case SET_REG_OPT_VID_POS:
				stored_item->val.vid_pos.x = new_value->val.vid_pos.x;
				stored_item->val.vid_pos.y = new_value->val.vid_pos.y;
				break;
			case SET_REG_OPT_VID_SIZE:
				stored_item->val.vid_size.h = new_value->val.vid_size.h;
				stored_item->val.vid_size.w = new_value->val.vid_size.w;
				UTIL_GLOGW("Scale h: %d  w: %d", stored_item->val.vid_size.h,
						stored_item->val.vid_size.w);
				break;
			case SET_REG_OPT_AUD_MODE:
				stored_item->val.aud_pt = new_value->val.aud_pt;
				break;
			case SET_REG_OPT_VOL_LVL:
				stored_item->val.vol_lvl.lvl = new_value->val.vol_lvl.lvl;
				stored_item->val.vol_lvl.enable = new_value->val.vol_lvl.enable;
				break;
			default:
				return EOS_ERR_UNKNOWN;
		}
		err = reg.items.add(reg.items, stored_item);
	}
	else
	{
		switch (stored_item->opt)
		{
			case SET_REG_OPT_VID_POS:
				stored_item->val.vid_pos.x = new_value->val.vid_pos.x;
				stored_item->val.vid_pos.y = new_value->val.vid_pos.y;
				break;
			case SET_REG_OPT_VID_SIZE:
				stored_item->val.vid_size.h = new_value->val.vid_size.h;
				stored_item->val.vid_size.w = new_value->val.vid_size.w;
				break;
			case SET_REG_OPT_AUD_MODE:
				stored_item->val.aud_pt = new_value->val.aud_pt;
				break;
			case SET_REG_OPT_VOL_LVL:
				stored_item->val.vol_lvl.lvl = new_value->val.vol_lvl.lvl;
				stored_item->val.vol_lvl.enable =
						new_value->val.vol_lvl.enable;
				break;
			default:
				return EOS_ERR_UNKNOWN;
		}
	}

	return err;
}

static void set_reg_sys_handle(set_reg_opt_t opt, set_reg_val_t* val)
{
	int i = 0;
	set_reg_sys_t *item = NULL;

	for(i=0; i<SET_REG_OPT_LAST; i++)
	{
		if(sys_reg[i].opt == opt)
		{
			item = &sys_reg[i];
			break;
		}
	}
	if(item == NULL)
	{
		return;
	}
	UTIL_GLOGI("System REG set for %d", opt);
	if(item->cbk != NULL)
	{
		item->cbk(opt, val, item->opaque);
	}
}
