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


#include "eos.h"
#include "eos_version.h"
#include "eos_macro.h"
#include "osi_mutex.h"
#include "osi_memory.h"
#include "eos_types.h"
#include "util_log.h"
#include "chain_manager.h"
#include "set_reg.h"

#include <stdlib.h>
#include <stdio.h>

#define MODULE_NAME "api"
#define EOS_API_LOG_NAME "eos"
#define EOS_VER_MAX (32)
#define EOS_VER_FMT "%u.%u.%u.%x"

static osi_mutex_t *eos_lock = NULL;
static util_log_t *eos_log = NULL;
static eos_cbk_t eos_cbk = NULL;
static void *eos_cbk_cookie = NULL;
static eos_data_cbk_t eos_data_cbk = NULL;
static void *eos_data_cbk_cookie = NULL;
static char eos_ver_str[EOS_VER_MAX] = "";

static eos_error_t eos_check_lock(void);
static eos_error_t eos_check_unlock(void);
static eos_error_t eos_chain_cbk(chain_t* chain, eos_event_t event,
		eos_event_data_t* data, void* cookie);
static eos_error_t eos_chain_data_cbk(uint32_t id,
		engine_type_t engine_type, engine_data_t data_type,
		uint8_t* data, uint32_t size, void* cookie);


eos_error_t eos_init(void)
{
	eos_error_t err = EOS_ERROR_OK;

	UTIL_GLOGI("Staring EOS...");
	if(eos_lock != NULL)
	{
		if(eos_log != NULL)
		{
			UTIL_LOGE(eos_log, "Already initialized!!!");
		}
		return EOS_ERROR_GENERAL;
	}
	err = util_log_create(&eos_log, EOS_API_LOG_NAME);
	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	util_log_set_level(eos_log, UTIL_LOG_LEVEL_INFO | UTIL_LOG_LEVEL_WARN |
			UTIL_LOG_LEVEL_ERR | UTIL_LOG_LEVEL_DBG);
	util_log_set_color(eos_log, true);

	err = osi_mutex_create(&eos_lock);
	if(err != EOS_ERROR_OK)
	{
		UTIL_LOGE(eos_log, "EOS Lock cannot be created");
		goto done;
	}
	chain_manager_module_init();
	osi_memset(eos_ver_str, 0, EOS_VER_MAX);
#ifdef EOS_VERSION_COMMIT
	snprintf(eos_ver_str, EOS_VER_MAX - 1, EOS_VER_FMT, EOS_VERSION_MAJOR,
			EOS_VERSION_MINOR, EOS_VERSION_REVISION, EOS_VERSION_COMMIT);
#else
	snprintf(eos_ver_str, EOS_VER_MAX - 1, EOS_VER_FMT, EOS_VERSION_MAJOR,
			EOS_VERSION_MINOR, EOS_VERSION_REVISION, 0);
#endif
	UTIL_LOGI(eos_log, "EOS (v%s) ready!", eos_ver_str);

done:
	if(err != EOS_ERROR_OK)
	{
		eos_deinit();
	}
	return err;
}

eos_error_t eos_deinit(void)
{
	UTIL_GLOGI("Stopping EOS...");
	if(eos_log != NULL)
	{
		util_log_destroy(&eos_log);
	}
	if(eos_lock != NULL)
	{
		osi_mutex_destroy(&eos_lock);
	}
	UTIL_GLOGI("EOS stopped");

	return EOS_ERROR_OK;
}

eos_error_t eos_set_event_cbk(eos_cbk_t cbk, void* cookie)
{
	eos_error_t err = EOS_ERROR_OK;

	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	if(eos_cbk != NULL)
	{
		UTIL_LOGW(eos_log, "Callback already set. Overwriting!!!");
	}
	eos_cbk = cbk;
	eos_cbk_cookie = cookie;

	return eos_check_unlock();
}

uint64_t eos_get_ver(void)
{
	uint64_t ver = 0LL;
	uint32_t shift = 0LL;
	uint8_t add = 0;

	add = (uint8_t)EOS_VERSION_MAJOR;
	shift = add << EOS_VERSION_MAJOR_SHFT;
	add = (uint8_t)EOS_VERSION_MINOR;
	shift |= add << EOS_VERSION_MINOR_SHFT;
	shift |= EOS_VERSION_REVISION;

	ver = shift;
	ver <<= EOS_VERSION_SHFT;
#ifdef EOS_VERSION_COMMIT
	ver |= EOS_VERSION_COMMIT;
#endif

	return ver;
}

const char* eos_get_ver_str(void)
{
	return eos_ver_str;
}

eos_error_t eos_player_play(char* in_url, char* in_extras, eos_out_t out)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;
	chain_handler_t handler = {eos_chain_cbk, eos_cbk_cookie,
			eos_chain_data_cbk, eos_data_cbk_cookie};

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	err = chain_manager_create(in_url, in_extras, out, &handler);

	if(err == EOS_ERROR_OK)
	{
		err = chain_manager_get(out, &chain);
		if (err == EOS_ERROR_OK)
		{
			err = chain_start(chain);
			chain_manager_release(out, &chain);
		}
	}

	return err;
}

eos_error_t eos_player_stop(eos_out_t out)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;

	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	chain_manager_get(out, &chain);
	if(chain == NULL)
	{
		eos_check_unlock();
		UTIL_LOGW(eos_log, "Chain already stopped?");

		return EOS_ERROR_OK;
	}
	eos_check_unlock();

	err = chain_stop(chain);
	if(err != EOS_ERROR_OK)
	{
		UTIL_LOGW(eos_log, "Chain NOT properly stopped!!!");
	}
	chain_manager_release(out, &chain);

	return chain_manager_destroy(out);
}

eos_error_t eos_player_buffer(eos_out_t out, bool start)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;

	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	chain_manager_get(out, &chain);
	if(chain == NULL)
	{
		eos_check_unlock();
		return EOS_ERROR_INVAL;
	}
	eos_check_unlock();

	if(start)
	{
		err = chain_start_buff(chain);
	}
	else
	{
		err = chain_stop_buff(chain);
	}
	if(err != EOS_ERROR_OK)
	{
		UTIL_LOGW(eos_log, "Chain NOT properly paused!!!");
	}
	chain_manager_release(out, &chain);
	return err;
}

eos_error_t eos_player_trickplay(eos_out_t out, int64_t position,
		int16_t speed)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;
	if (chain_manager_get(out, &chain) != EOS_ERROR_OK)
	{
		return EOS_ERROR_GENERAL;
	}

	err = chain_trickplay(chain, position, speed);

	chain_manager_release(out, &chain);

	return err;
}

eos_error_t eos_player_get_media_desc(eos_out_t out, eos_media_desc_t* desc)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;

	err = eos_check_lock();

	UTIL_LOGI(eos_log, "Get media descriptor...");
	if(err != EOS_ERROR_OK)
	{
		UTIL_LOGE(eos_log, "Get media descriptor [Failure]");
		return err;
	}
	chain_manager_get(out, &chain);
	if(chain == NULL)
	{
		eos_check_unlock();
		UTIL_LOGE(eos_log, "Get media descriptor [Failure]");
		return EOS_ERROR_INVAL;
	}
	eos_check_unlock();

	err = chain_get_streams(chain, desc);
	chain_manager_release(out, &chain);

	UTIL_LOGI(eos_log, "Get media descriptor [Success]");
	return err;
}

eos_error_t eos_player_set_track(eos_out_t out, uint32_t id, bool on)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;

	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	chain_manager_get(out, &chain);
	if(chain == NULL)
	{
		eos_check_unlock();
		return EOS_ERROR_INVAL;
	}
	eos_check_unlock();
	err =  chain_set_track(chain, id, on);
	chain_manager_release(out, &chain);

	return err;
}

eos_error_t eos_player_get_ttxt_pg(eos_out_t out, uint16_t idx, char** json_pg)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;
	eos_media_data_t *data = NULL;
	char *str = NULL;

	if(json_pg == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	chain_manager_get(out, &chain);
	if(chain == NULL)
	{
		eos_check_unlock();
		return EOS_ERROR_INVAL;
	}
	eos_check_unlock();
	err = chain_get_stream_data(chain, EOS_MEDIA_CODEC_TTXT, idx, &data);
	if(err != EOS_ERROR_OK || data == NULL)
	{
		chain_manager_release(out, &chain);
		return err;
	}
	str = osi_malloc(data->size);
	if(str == NULL)
	{
		chain_manager_release(out, &chain);
		osi_free((void**)&data);
		return EOS_ERROR_NOMEM;
	}
	osi_memcpy(str, data->data, data->size);
	*json_pg = str;

	osi_free((void**)&data);
	chain_manager_release(out, &chain);

	return EOS_ERROR_OK;
}

eos_error_t eos_out_vid_scale(eos_out_t out, uint32_t w, uint32_t h)
{
	set_reg_val_t val;

	val.vid_size.w = w;
	val.vid_size.h = h;

	return set_reg_apply(out, SET_REG_OPT_VID_SIZE, &val);
}

eos_error_t eos_out_vid_move(eos_out_t out, uint32_t x, uint32_t y)
{
	set_reg_val_t val;

	val.vid_pos.x = x;
	val.vid_pos.y = y;

	return set_reg_apply(out, SET_REG_OPT_VID_POS, &val);
}

eos_error_t eos_out_aud_mode(eos_out_t out, eos_out_amode_t amode)
{
	set_reg_val_t val;

	val.aud_pt = amode == EOS_OUT_AMODE_STEREO ?
			false : true;

	return set_reg_apply(out, SET_REG_OPT_AUD_MODE, &val);
}

eos_error_t eos_out_vol_leveling(eos_out_t out, bool enable,
		eos_out_vol_lvl_t lvl)
{
	set_reg_val_t val;

	val.vol_lvl.enable = enable;
	val.vol_lvl.lvl = lvl;

	return set_reg_apply(out, SET_REG_OPT_VOL_LVL, &val);
}

eos_error_t eos_data_cbk_set(eos_data_cbk_t cbk, void* cookie)
{
	eos_error_t err = EOS_ERROR_OK;

	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	if(eos_data_cbk != NULL)
	{
		UTIL_LOGW(eos_log, "Data callback already set. Overwriting!!!");
	}
	eos_data_cbk = cbk;
	eos_data_cbk_cookie = cookie;

	return eos_check_unlock();
}

eos_error_t eos_data_ttxt_enable(eos_out_t out, bool enable)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;
	data_mgr_t *data_mgr = NULL;

	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	chain_manager_get(out, &chain);
	if(chain == NULL)
	{
		eos_check_unlock();
		return EOS_ERROR_INVAL;
	}
	eos_check_unlock();
	err = chain_get_data_mgr(chain, &data_mgr);
	if(err != EOS_ERROR_OK)
	{
		chain_manager_release(out, &chain);
		return err;
	}
	err = data_mgr_ttxt_enable(data_mgr, enable);
	chain_manager_release(out, &chain);

	return err;
}

eos_error_t eos_data_ttxt_page_set(eos_out_t out, uint16_t page,
		uint16_t subpage)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;
	data_mgr_t *data_mgr = NULL;

	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	chain_manager_get(out, &chain);
	if(chain == NULL)
	{
		eos_check_unlock();
		return EOS_ERROR_INVAL;
	}
	eos_check_unlock();
	err = chain_get_data_mgr(chain, &data_mgr);
	if(err != EOS_ERROR_OK)
	{
		chain_manager_release(out, &chain);
		return err;
	}
	err = data_mgr_ttxt_page_set(data_mgr, page, subpage);
	chain_manager_release(out, &chain);

	return err;
}

eos_error_t eos_data_ttxt_page_get(eos_out_t out, uint16_t* page,
		uint16_t* subpage)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;
	data_mgr_t *data_mgr = NULL;

	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	chain_manager_get(out, &chain);
	if(chain == NULL)
	{
		eos_check_unlock();
		return EOS_ERROR_INVAL;
	}
	eos_check_unlock();
	err = chain_get_data_mgr(chain, &data_mgr);
	if(err != EOS_ERROR_OK)
	{
		chain_manager_release(out, &chain);
		return err;
	}
	err = data_mgr_ttxt_page_get(data_mgr, page, subpage);
	chain_manager_release(out, &chain);

	return err;
}

eos_error_t eos_data_ttxt_next_page_get(eos_out_t out, uint16_t* next)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;
	data_mgr_t *data_mgr = NULL;

	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	chain_manager_get(out, &chain);
	if(chain == NULL)
	{
		eos_check_unlock();
		return EOS_ERROR_INVAL;
	}
	eos_check_unlock();
	err = chain_get_data_mgr(chain, &data_mgr);
	if(err != EOS_ERROR_OK)
	{
		chain_manager_release(out, &chain);
		return err;
	}
	err = data_mgr_ttxt_next_page_get(data_mgr, next);
	chain_manager_release(out, &chain);

	return err;
}
eos_error_t eos_data_ttxt_prev_page_get(eos_out_t out, uint16_t* previous)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;
	data_mgr_t *data_mgr = NULL;

	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	chain_manager_get(out, &chain);
	if(chain == NULL)
	{
		eos_check_unlock();
		return EOS_ERROR_INVAL;
	}
	eos_check_unlock();
	err = chain_get_data_mgr(chain, &data_mgr);
	if(err != EOS_ERROR_OK)
	{
		chain_manager_release(out, &chain);
		return err;
	}
	err = data_mgr_ttxt_prev_page_get(data_mgr, previous);
	chain_manager_release(out, &chain);

	return err;
}

eos_error_t eos_data_ttxt_red_page_get(eos_out_t out, uint16_t* red)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;
	data_mgr_t *data_mgr = NULL;

	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	chain_manager_get(out, &chain);
	if(chain == NULL)
	{
		eos_check_unlock();
		return EOS_ERROR_INVAL;
	}
	eos_check_unlock();
	err = chain_get_data_mgr(chain, &data_mgr);
	if(err != EOS_ERROR_OK)
	{
		chain_manager_release(out, &chain);
		return err;
	}
	err = data_mgr_ttxt_red_page_get(data_mgr, red);
	chain_manager_release(out, &chain);

	return err;
}

eos_error_t eos_data_ttxt_green_page_get(eos_out_t out, uint16_t* green)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;
	data_mgr_t *data_mgr = NULL;

	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	chain_manager_get(out, &chain);
	if(chain == NULL)
	{
		eos_check_unlock();
		return EOS_ERROR_INVAL;
	}
	eos_check_unlock();
	err = chain_get_data_mgr(chain, &data_mgr);
	if(err != EOS_ERROR_OK)
	{
		chain_manager_release(out, &chain);
		return err;
	}
	err = data_mgr_ttxt_green_page_get(data_mgr, green);
	chain_manager_release(out, &chain);

	return err;
}

eos_error_t eos_data_ttxt_next_subpage_get(eos_out_t out, uint16_t* next)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;
	data_mgr_t *data_mgr = NULL;

	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	chain_manager_get(out, &chain);
	if(chain == NULL)
	{
		eos_check_unlock();
		return EOS_ERROR_INVAL;
	}
	eos_check_unlock();
	err = chain_get_data_mgr(chain, &data_mgr);
	if(err != EOS_ERROR_OK)
	{
		chain_manager_release(out, &chain);
		return err;
	}
	err = data_mgr_ttxt_next_subpage_get(data_mgr, next);
	chain_manager_release(out, &chain);

	return err;
}

eos_error_t eos_data_ttxt_prev_subpage_get(eos_out_t out, uint16_t* previous)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;
	data_mgr_t *data_mgr = NULL;

	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	chain_manager_get(out, &chain);
	if(chain == NULL)
	{
		eos_check_unlock();
		return EOS_ERROR_INVAL;
	}
	eos_check_unlock();
	err = chain_get_data_mgr(chain, &data_mgr);
	if(err != EOS_ERROR_OK)
	{
		chain_manager_release(out, &chain);
		return err;
	}
	err = data_mgr_ttxt_prev_subpage_get(data_mgr, previous);
	chain_manager_release(out, &chain);

	return err;
}


eos_error_t eos_data_ttxt_blue_page_get(eos_out_t out, uint16_t* blue)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;
	data_mgr_t *data_mgr = NULL;

	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	chain_manager_get(out, &chain);
	if(chain == NULL)
	{
		eos_check_unlock();
		return EOS_ERROR_INVAL;
	}
	eos_check_unlock();
	err = chain_get_data_mgr(chain, &data_mgr);
	if(err != EOS_ERROR_OK)
	{
		chain_manager_release(out, &chain);
		return err;
	}
	err = data_mgr_ttxt_blue_page_get(data_mgr, blue);
	chain_manager_release(out, &chain);

	return err;
}

eos_error_t eos_data_ttxt_yellow_page_get(eos_out_t out, uint16_t* yellow)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;
	data_mgr_t *data_mgr = NULL;

	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	chain_manager_get(out, &chain);
	if(chain == NULL)
	{
		eos_check_unlock();
		return EOS_ERROR_INVAL;
	}
	eos_check_unlock();
	err = chain_get_data_mgr(chain, &data_mgr);
	if(err != EOS_ERROR_OK)
	{
		chain_manager_release(out, &chain);
		return err;
	}
	err = data_mgr_ttxt_yellow_page_get(data_mgr, yellow);
	chain_manager_release(out, &chain);

	return err;
}

eos_error_t eos_data_ttxt_transparency_set(eos_out_t out, uint8_t alpha)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;
	data_mgr_t *data_mgr = NULL;

	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	chain_manager_get(out, &chain);
	if(chain == NULL)
	{
		eos_check_unlock();
		return EOS_ERROR_INVAL;
	}
	eos_check_unlock();
	err = chain_get_data_mgr(chain, &data_mgr);
	if(err != EOS_ERROR_OK)
	{
		chain_manager_release(out, &chain);
		return err;
	}
	err = data_mgr_ttxt_transparency_set(data_mgr, alpha);
	chain_manager_release(out, &chain);

	return err;
}

eos_error_t eos_data_hbbtv_uri_get(eos_out_t out, char* uri)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;
	data_mgr_t *data_mgr = NULL;

	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	chain_manager_get(out, &chain);
	if(chain == NULL)
	{
		eos_check_unlock();
		return EOS_ERROR_INVAL;
	}
	eos_check_unlock();
	err = chain_get_data_mgr(chain, &data_mgr);
	if(err != EOS_ERROR_OK)
	{
		chain_manager_release(out, &chain);
		return err;
	}
	err = data_mgr_hbbtv_uri_get(data_mgr, uri);
	chain_manager_release(out, &chain);

	return err;
}

eos_error_t eos_data_dvbsub_enable(eos_out_t out, bool enable)
{
	eos_error_t err = EOS_ERROR_OK;
	chain_t *chain = NULL;
	data_mgr_t *data_mgr = NULL;

	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	chain_manager_get(out, &chain);
	if(chain == NULL)
	{
		eos_check_unlock();
		return EOS_ERROR_INVAL;
	}
	eos_check_unlock();
	err = chain_get_data_mgr(chain, &data_mgr);
	if(err != EOS_ERROR_OK)
	{
		chain_manager_release(out, &chain);
		return err;
	}
	err = data_mgr_dvbsub_enable(data_mgr, enable);
	chain_manager_release(out, &chain);

	return err;
}

static eos_error_t eos_check_lock(void)
{
	if(eos_lock == NULL)
	{
		return EOS_ERROR_GENERAL;
	}

	return osi_mutex_lock(eos_lock);
}

static eos_error_t eos_check_unlock(void)
{
	if(eos_lock == NULL)
	{
		return EOS_ERROR_GENERAL;
	}

	return osi_mutex_unlock(eos_lock);
}

static eos_error_t eos_chain_cbk(chain_t* chain, eos_event_t event,
		eos_event_data_t* data, void* cookie)
{
	eos_out_t out = EOS_OUT_MAIN_AV;
	eos_error_t err = EOS_ERROR_OK;

	EOS_UNUSED(cookie);
	if(eos_cbk != NULL)
	{
		err = chain_get_id(chain, (uint32_t*)&out);
		if(err != EOS_ERROR_OK)
		{
			return err;
		}
		if(out != EOS_OUT_MAIN_AV && out != EOS_OUT_AUX_AV)
		{
			UTIL_LOGE(eos_log, "Unknown chain!");
			return EOS_ERROR_INVAL;
		}

		return eos_cbk(out, event, data, cookie);
	}

	return EOS_ERROR_OK;
}

static eos_error_t eos_chain_data_cbk(uint32_t id,
		engine_type_t engine_type, engine_data_t data_type,
		uint8_t* data, uint32_t size, void* cookie)
{
	eos_error_t err = EOS_ERROR_OK;
	eos_data_cls_t clazz = EOS_DATA_CLS_TTXT;
	eos_data_fmt_t format = EOS_DATA_FMT_RAW;
	eos_data_t cbk_data = NULL;

	EOS_UNUSED(cookie);
	if(id != EOS_OUT_MAIN_AV && id != EOS_OUT_AUX_AV)
	{
		UTIL_LOGE(eos_log, "Unknown chain!");
	}
	err = eos_check_lock();

	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	if(eos_data_cbk == NULL)
	{
		eos_check_unlock();
		return EOS_ERROR_OK;
	}
	switch(engine_type)
	{
	case ENGINE_TYPE_TTXT:
		clazz = EOS_DATA_CLS_TTXT;
		break;
	case ENGINE_TYPE_DVB_SUB:
		clazz = EOS_DATA_CLS_SUB;
		break;
	case ENGINE_TYPE_HBBTV:
		clazz = EOS_DATA_CLS_HBBTV;
		break;
	case ENGINE_TYPE_DSMCC:
		clazz = EOS_DATA_CLS_DSM_CC;
		break;
	default:
		eos_check_unlock();
		return EOS_ERROR_INVAL;
	}
	switch(data_type)
	{
	case ENGINE_DATA_STRING:
		format = EOS_DATA_FMT_RAW;
		break;
	case ENGINE_DATA_JSON:
		format = EOS_DATA_FMT_JSON;
		break;
	case ENGINE_DATA_HTML:
		format = EOS_DATA_FMT_HTML;
		break;
	case ENGINE_DATA_BASE64_PNG:
		format = EOS_DATA_FMT_BASE64_PNG;
		break;
	default:
		eos_check_unlock();
		return EOS_ERROR_INVAL;
	}
	cbk_data = osi_calloc(size);
	if(cbk_data == NULL)
	{
		eos_check_unlock();
		return EOS_ERROR_NOMEM;
	}
	osi_memcpy(cbk_data, data, size);
	eos_check_unlock();
	err = eos_data_cbk(id, clazz, format, cbk_data, size, cookie);
	osi_free(&cbk_data);

	return err;
}
