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


#include "sink_factory.h"
#include "util_factory.h"
#include "util_slist.h"
#include "osi_mutex.h"
#include "osi_memory.h"

#include <string.h>

#define MODULE_NAME SINK_MODULE_NAME
#include "util_log.h"

static util_factory_t *factory = NULL;
static util_slist_t list;
static osi_mutex_t *list_lock = NULL;

static eos_error_t sink_factory_identify_cbk(sink_t* model,
		sink_factory_id_data_t* data);
static bool sink_factory_comparator(void* search_param,
		void* data_address);

eos_error_t sink_factory_register(sink_t* model, uint64_t* model_id, sink_manufacture_func_t manufacture,
		sink_dismantle_func_t dismantle)
{
	eos_error_t ret = EOS_ERROR_OK;

	if (model == NULL || manufacture == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (factory == NULL)
	{
		ret = osi_mutex_create(&list_lock);
		if(ret != EOS_ERROR_OK)
		{
			return ret;
		}
		ret = util_slist_create(&list, sink_factory_comparator);
		if(ret != EOS_ERROR_OK)
		{
			osi_mutex_destroy(&list_lock);
			return ret;
		}
		ret = util_factory_create(&factory, (util_probe_cb_t)sink_factory_identify_cbk);
		if(ret != EOS_ERROR_OK)
		{
			osi_mutex_destroy(&list_lock);
			util_slist_destroy(&list);
			return ret;
		}
	}
	ret = util_factory_register(factory, (void*)model, model_id,
			(util_manufacture_cb_t)manufacture, (util_dismantle_cb_t)dismantle);
	if(ret != EOS_ERROR_OK)
	{
		return ret;
	}
	UTIL_GLOGI("Registered sink model: %s", model->name);

	return EOS_ERROR_OK;
}

eos_error_t sink_factory_unregister(sink_t* model, uint64_t model_id)
{
	eos_error_t ret = EOS_ERROR_OK;
	uint32_t count;

	if (model == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (factory == NULL)
	{
		return EOS_ERROR_PERM;
	}
	osi_mutex_lock(list_lock);
	osi_mutex_destroy(&list_lock);
	util_slist_destroy(&list);

	ret = util_factory_unregister(factory, (void*)model, model_id);
	if(ret != EOS_ERROR_OK)
	{
		return ret;
	}
	ret = util_factory_get_count(factory, &count);
	if(ret != EOS_ERROR_OK)
	{
		return ret;
	}
	if(count == 0)
	{
		util_factory_destroy(&factory);
	}

	return EOS_ERROR_OK;
}

eos_error_t sink_factory_manufacture(uint32_t id, eos_media_desc_t* media,
		uint64_t caps, link_io_type_t input_type, sink_t** product)
{
	sink_factory_id_data_t data = {caps, id, input_type};
	eos_error_t err = EOS_ERROR_OK;
	uint32_t *add = NULL;

	if(product == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	osi_mutex_lock(list_lock);
	err = util_factory_manufacture(factory, (void**)product, &data);
	if(err != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Sink manufacture failed: %d", err);
		osi_mutex_unlock(list_lock);
		return err;
	}
	add = (uint32_t*)osi_malloc(sizeof(uint32_t));
	if(add == NULL)
	{
		osi_mutex_unlock(list_lock);
		sink_factory_dismantle(product);
		return EOS_ERROR_NOMEM;
	}
	*add = id;
	if((err = list.add(list, add)) != EOS_ERROR_OK)
	{
		osi_mutex_unlock(list_lock);
		sink_factory_dismantle(product);
		return err;
	}
	if((err = (*product)->command.setup(*product, id, media)) != EOS_ERROR_OK)
	{
		osi_mutex_unlock(list_lock);
		sink_factory_dismantle(product);
		return err;
	}
	osi_mutex_unlock(list_lock);

	return EOS_ERROR_OK;
}

eos_error_t sink_factory_dismantle(sink_t** product)
{
	eos_error_t err = EOS_ERROR_OK;
	uint32_t *id = 0;
	uint32_t search = 0;

	if(product == NULL || *product == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	osi_mutex_lock(list_lock);
	search = (*product)->id;
	err = util_factory_dismantle(factory, (void**)product);
	if(err != EOS_ERROR_OK)
	{
		UTIL_GLOGW("Sink (%s) dismantle failed!", (*product)->name);
	}
	if((err = list.get(list, &search, (void**)&id)) == EOS_ERROR_OK)
	{
		list.remove(list, id);
		osi_free((void**)&id);
	}
	else
	{
		UTIL_GLOGW("Sink (%d) not in the list!", search);
	}
	osi_mutex_unlock(list_lock);

	return EOS_ERROR_OK;
}

eos_error_t sink_factory_identify_cbk(sink_t* model,
		sink_factory_id_data_t* data)
{
	eos_error_t err = EOS_ERROR_OK;
	uint32_t *found;

	if(model == NULL || data == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	if(list.get(list, &data->id, (void**)&found) == EOS_ERROR_OK)
	{
		return EOS_ERROR_PERM;
	}
	if((model->caps & data->caps) && (model->plug_type & data->input_type))
	{
		return EOS_ERROR_OK;
	}

	return EOS_ERROR_NFOUND;
}

static bool sink_factory_comparator(void* search_param,
		void* data_address)
{
	uint32_t search_id;
	uint32_t id;

	if(search_param == NULL || data_address == NULL)
	{
		return false;
	}
	search_id = *(uint32_t*) search_param;
	id = *(uint32_t*) data_address;

	return (id == search_id);
}
