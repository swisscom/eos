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


#include "cron_plyr.h"

#include "eos_macro.h"
#include "osi_memory.h"
#include "osi_mutex.h"

#define MODULE_NAME "cron_dummy"
#include "util_log.h"

#define CRON_PLYR_CAPS (LINK_CAP_SINK | LINK_CAP_STREAM_SEL | \
		LINK_CAP_AV_OUT_SET)
#define CRON_PLYR_PLUG_TYPE (LINK_IO_TYPE_TS | LINK_IO_TYPE_SPROG_TS | \
		LINK_IO_TYPE_MPROG_TS)

#define CRON_PLYR_LOCK(player) osi_mutex_lock(player->lock)
#define CRON_PLYR_UNLOCK(player) osi_mutex_unlock(player->lock)

struct cron_plyr
{
	eos_media_desc_t media;
	cron_plyr_ev_cbk_t cbk;
	void *cbk_data;
	osi_mutex_t *lock;
	bool frm_ev;
};

static eos_error_t cron_plyr_fire_ev(cron_plyr_t* player,
		cron_plyr_ev_t event, cron_ply_ev_data_t* data);
static inline cron_plyr_es_t cron_plyr_conv_es(eos_media_es_t* media_es);

static const char *cron_plyr_name = "dummy";

eos_error_t cron_plyr_sys_init(void)
{
	UTIL_GLOGD("%s", __func__);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_sys_deinit(void)
{
	UTIL_GLOGD("%s", __func__);

	return EOS_ERROR_OK;
}

const char* cron_plyr_sys_get_name(void)
{
	UTIL_GLOGD("%s", __func__);

	return cron_plyr_name;
}

eos_error_t cron_plyr_sys_get_caps(uint64_t* caps)
{
	if(caps == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s", __func__);
	*caps = CRON_PLYR_CAPS;

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_sys_get_plug(link_io_type_t* plug)
{
	if(plug == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s", __func__);
	*plug = CRON_PLYR_PLUG_TYPE;

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_create(cron_plyr_t** player, cron_plyr_disp_t disp)
{
	cron_plyr_t *tmp = NULL;
	eos_error_t err = EOS_ERROR_OK;

	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	tmp = (cron_plyr_t*)osi_calloc(sizeof(cron_plyr_t));
	if(tmp == NULL)
	{
		return EOS_ERROR_NOMEM;
	}
	if((err = osi_mutex_create(&tmp->lock)) != EOS_ERROR_OK)
	{
		goto done;
	}
	osi_memset(&(tmp->media), 0, sizeof(eos_media_desc_t));
	*player = tmp;
	UTIL_GLOGD("Player %d created", disp);

done:
	if(err != EOS_ERROR_OK && tmp != NULL)
	{
		if(tmp->lock != NULL)
		{
			osi_mutex_destroy(&tmp->lock);
		}
	}

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_destroy(cron_plyr_t** player)
{
	cron_plyr_t *tmp = NULL;

	if(player == NULL || *player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	tmp = *player;
	osi_mutex_lock(tmp->lock);
	osi_mutex_unlock(tmp->lock);
	osi_mutex_destroy(&tmp->lock);
	UTIL_GLOGD("Player destroyed");

	osi_free((void**)player);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_rst(cron_plyr_t* player)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	return EOS_ERROR_NIMPLEMENTED;
}

eos_error_t cron_plyr_buff_get(cron_plyr_t* player, uint8_t** buff, size_t* size,
		link_data_ext_info_t* extended_info, int32_t milis, uint16_t id)
{
	if(player == NULL || buff == NULL || size == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	EOS_UNUSED(id);
	EOS_UNUSED(extended_info);
	EOS_UNUSED(milis);

	*buff = osi_malloc(*size);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_buff_cons(cron_plyr_t* player, uint8_t* buff, size_t size,
		link_data_ext_info_t* extended_info, int32_t milis, uint16_t id)
{
	cron_ply_ev_data_t data;

	if(player == NULL || buff == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	EOS_UNUSED(milis);
	EOS_UNUSED(id);
	EOS_UNUSED(extended_info);
	EOS_UNUSED(size);

	osi_free((void**)&buff);
	if(player->frm_ev == false)
	{
		data.first_frm.pts = 0;
		cron_plyr_fire_ev(player, CRON_PLYR_EV_FRM, &data);
		player->frm_ev = true;
	}
	if(extended_info)
	{
		if(extended_info->eos == LINK_ES_EOS_END)
		{
			UTIL_GLOGD("%s: End of stream detected", __func__);
		}
	}

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_set_media_desc(cron_plyr_t* player,
		eos_media_desc_t* media)
{
	uint8_t i = 0;

	if(player == NULL || media == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s", __func__);
	CRON_PLYR_LOCK(player);
	osi_memcpy(&(player->media), media, sizeof(eos_media_desc_t));
	for(i=0; i<player->media.es_cnt; i++)
	{
		player->media.es[i].selected = false;
	}
	CRON_PLYR_UNLOCK(player);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_set_freerun(cron_plyr_t* player, bool enable)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s: %d", __func__, enable);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_get_freerun(cron_plyr_t* player, bool* enable)
{
	if(player == NULL || enable == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s", __func__);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_set_slowdown(cron_plyr_t* player, uint8_t milis_per_frm)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s: %d", __func__, milis_per_frm);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_get_slowdown(cron_plyr_t* player, uint8_t* milis_per_frm)
{
	if(player == NULL || milis_per_frm == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s", __func__);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_set_dec_trshld(cron_plyr_t* player, uint32_t milis)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s: %d", __func__, milis);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_get_dec_trshld(cron_plyr_t* player, uint32_t* milis)
{
	if(player == NULL || milis == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s", __func__);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_set_keep_frm(cron_plyr_t* player, bool enable)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s: %d", __func__, enable);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_get_keep_frm(cron_plyr_t* player, bool* enable)
{
	if(player == NULL || enable == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s", __func__);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_get_synced(cron_plyr_t* player, uint32_t* milis)
{
	EOS_UNUSED(player);
	EOS_UNUSED(milis);
	// TODO: Implement sync data calculation

	return EOS_ERROR_NIMPLEMENTED;
}

eos_error_t cron_plyr_set_sync_time(cron_plyr_t* player, uint32_t millis)
{

	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s: %d", __func__, millis);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_get_av_delay(cron_plyr_t* player, int32_t* millis)
{

	if(player == NULL || millis == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s", __func__);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_set_wm(cron_plyr_t* player, cron_plyr_wm_t wm, uint32_t milis)
{
	EOS_UNUSED(player);
	EOS_UNUSED(wm);
	EOS_UNUSED(milis);

	UTIL_GLOGD("%s", __func__);

	return EOS_ERROR_NIMPLEMENTED;
}

eos_error_t cron_plyr_es_set(cron_plyr_t* player, uint8_t es_idx)
{
	eos_error_t err = EOS_ERROR_OK;

	if(player == NULL || es_idx >= player->media.es_cnt)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s: %d", __func__, es_idx);

	return err;
}

eos_error_t cron_plyr_es_unset(cron_plyr_t* player, uint8_t es_idx)
{
	eos_media_es_t *sel = NULL;

	if(player == NULL || es_idx >= player->media.es_cnt)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s", __func__);
	CRON_PLYR_LOCK(player);
	sel = &(player->media.es[es_idx]);
	if(sel->selected == true)
	{
		sel->selected = false;
	}
	CRON_PLYR_UNLOCK(player);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_es_get(cron_plyr_t* player, uint8_t es_idx,
		cron_plyr_es_t* es)
{
	eos_media_es_t *to_check = NULL;

	if(player == NULL || es_idx >= player->media.es_cnt ||
			es == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s", __func__);
	CRON_PLYR_LOCK(player);
	to_check = &(player->media.es[es_idx]);
	if(to_check->selected == false)
	{
		*es = CRON_PLYR_ES_NONE;
	}
	else
	{
		*es = cron_plyr_conv_es(to_check);
	}
	CRON_PLYR_UNLOCK(player);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_es_start(cron_plyr_t* player, cron_plyr_es_t es)
{
	eos_error_t err = EOS_ERROR_OK;

	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s: %d", __func__, es);

	return err;
}

eos_error_t cron_plyr_es_stop(cron_plyr_t* player, cron_plyr_es_t es)
{
	eos_error_t err = EOS_ERROR_OK;

	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s: %d", __func__, es);

	return err;
}

eos_error_t cron_plyr_es_enable(cron_plyr_t* player,
		cron_plyr_es_t es, bool enabled)
{
	EOS_UNUSED(player);
	EOS_UNUSED(es);
	EOS_UNUSED(enabled);
	UTIL_GLOGD("%s", __func__);
	// TODO: Implement ES enabling

	return EOS_ERROR_NIMPLEMENTED;
}

eos_error_t cron_plyr_reg_ev_hnd(cron_plyr_t* player, cron_plyr_ev_cbk_t cbk,
		void* opaque)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s", __func__);
	CRON_PLYR_LOCK(player);
	if(player->cbk != NULL && cbk != NULL)
	{
		UTIL_GLOGW("Owerwriting event callback!");
	}
	player->cbk = cbk;
	player->cbk_data = opaque;
	CRON_PLYR_UNLOCK(player);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_start(cron_plyr_t* player)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s", __func__);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_stop(cron_plyr_t* player)
{
	eos_error_t err = EOS_ERROR_OK;

	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	player->frm_ev = false;
	UTIL_GLOGD("%s", __func__);

	return err;
}

eos_error_t cron_plyr_pause(cron_plyr_t* player, bool buffering)
{
	eos_error_t err = EOS_ERROR_OK;
	EOS_UNUSED(buffering)

	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s: %d", __func__, buffering);

	return err;
}

eos_error_t cron_plyr_resume(cron_plyr_t* player)
{
	eos_error_t err = EOS_ERROR_OK;

	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s", __func__);

	return err;
}

eos_error_t cron_plyr_pause_resume(cron_plyr_t* player, uint32_t milis)
{
	eos_error_t err = EOS_ERROR_OK;

	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s: %d", __func__, milis);
	osi_time_usleep(OSI_TIME_MSEC_TO_USEC(milis));

	return err;
}

eos_error_t cron_plyr_flush(cron_plyr_t* player)
{
	eos_error_t err = EOS_ERROR_OK;

	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s", __func__);

	return err;

}

eos_error_t cron_plyr_force_sync(cron_plyr_t* player)
{
	EOS_UNUSED(player);
	UTIL_GLOGD("%s", __func__);

	return EOS_ERROR_NIMPLEMENTED;
}

eos_error_t cron_plyr_vid_blank(cron_plyr_t* player)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s", __func__);

	return EOS_ERROR_NIMPLEMENTED;
}

eos_error_t cron_plyr_vid_move(cron_plyr_t* player, uint16_t x, uint16_t y)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s: %d, %d", __func__, x, y);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_vid_scale(cron_plyr_t* player, uint16_t width, uint16_t height)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s: %dx%d", __func__, width, height);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_aud_pt(cron_plyr_t* player, bool enable)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s: %d", __func__, enable);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_vol_leveling(cron_plyr_t* player, bool enable, cron_vol_lvl_t lvl)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("%s: %d, %d", __func__, enable, lvl);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_get_ext(cron_plyr_t* player, link_cap_t cap,
		void** ctrl_funcs)
{
	EOS_UNUSED(player);
	EOS_UNUSED(cap);
	EOS_UNUSED(ctrl_funcs);
	UTIL_GLOGD("%s", __func__);

	return EOS_ERROR_NFOUND;
}

static eos_error_t cron_plyr_fire_ev(cron_plyr_t* player,
		cron_plyr_ev_t event, cron_ply_ev_data_t* data)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if(player->cbk == NULL)
	{
		return EOS_ERROR_OK;
	}

	return player->cbk(player->cbk_data, event, data);
}

static inline cron_plyr_es_t cron_plyr_conv_es(eos_media_es_t* media_es)
{
	if(media_es == NULL)
	{
		return CRON_PLYR_ES_NONE;
	}
	if(EOS_MEDIA_IS_AUD(media_es->codec))
	{
		return CRON_PLYR_ES_AUD;
	}
	if(EOS_MEDIA_IS_VID(media_es->codec))
	{
		return CRON_PLYR_ES_VID;
	}
	if(media_es->codec == EOS_MEDIA_CODEC_TTXT)
	{
		return CRON_PLYR_ES_TTXT;
	}
	if(media_es->codec == EOS_MEDIA_CODEC_DVB_SUB)
	{
		return CRON_PLYR_ES_SUB;
	}
	if(media_es->codec == EOS_MEDIA_CODEC_CC)
	{
		return CRON_PLYR_ES_CC;
	}

	return CRON_PLYR_ES_NONE;
}
