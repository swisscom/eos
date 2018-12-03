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


#include "sink_cron_plyr.h"
#include "eos_macro.h"
#include "osi_memory.h"
#include "osi_mutex.h"
#include "sink_factory.h"
#include "set_reg.h"

#define MODULE_NAME ("core")
#include "util_log.h"

#include <stdio.h>
#include <string.h>

#define SINK_CRON_PLYR_CHK_HND(sink) \
do \
{ \
	if(sink == NULL || sink == &sink_cron_plyr) \
	{ \
		return EOS_ERROR_INVAL; \
	} \
} while(0)
#define SINK_CRON_PLYR_GET_PRIV(sink) ((sink_cron_plyr_priv_t*) sink->priv)
#define SINK_CRON_PLYR_NAME "cron_plyr"
#define SINK_CRON_SYNC_COEFF 2
#define SINK_CRON_PLYR_LOCK(priv) (osi_mutex_lock(priv->lock))
#define SINK_CRON_PLYR_UNLOCK(priv) (osi_mutex_unlock(priv->lock))

typedef enum cron_plyr_state
{
	CRON_PLYR_STATE_IDLE,
	CRON_PLYR_STATE_STARTED,
	CRON_PLYR_STATE_STOPPED,
	CRON_PLYR_STATE_PAUSED,
	CRON_PLYR_STATE_BUFFERING
} cron_plyr_state_t;

typedef struct sink_cron_plyr_priv
{
	uint64_t magic;
	uint64_t product_id;
	link_ev_hnd_t handler;
	void *hnd_cookie;
	cron_plyr_t *player;
	osi_mutex_t *lock;
	cron_plyr_state_t state;
	util_log_t *log;
	bool frst_I_frame_rcvd;
} sink_cron_plyr_priv_t;

static eos_error_t sink_cron_plyr_init(sink_t* sink,
		sink_factory_id_data_t* data);
static eos_error_t sink_cron_plyr_deinit(sink_t* sink);

static eos_error_t sink_cron_plyr_start(sink_t* sink);
static eos_error_t sink_cron_plyr_stop(sink_t* sink);
static eos_error_t sink_cron_plyr_pause(sink_t* sink, bool buffering);
static eos_error_t sink_cron_plyr_resume(sink_t* sink);
static eos_error_t sink_cron_plyr_flush_buffs(sink_t* sink);
static eos_error_t sink_cron_plyr_get_ctrl(link_handle_t link, link_cap_t cap,
		void** ctrl_funcs);
static eos_error_t sink_cron_plyr_reg_ev_hnd(link_handle_t link,
		link_ev_hnd_t handler, void* cookie);
static eos_error_t sink_cron_plyr_ev_cbk(void* opaque, cron_plyr_ev_t event,
		cron_ply_ev_data_t* data);
static eos_error_t sink_cron_plyr_setup(sink_t* sink, uint32_t id,
		eos_media_desc_t* media);
static eos_error_t sink_cron_plyr_manufacture(sink_factory_id_data_t* data,
		sink_t* model, uint64_t model_id,
		sink_t** product, uint64_t product_id);
static eos_error_t sink_cron_plyr_dismantle(uint64_t model_id, sink_t** clone);

static eos_error_t sink_cron_plyr_alloc(link_handle_t opaque, uint8_t** buff,
		size_t* size, link_data_ext_info_t* extended_info,
		int32_t msec, uint16_t id);
static eos_error_t sink_cron_plyr_commit(link_handle_t opaque, uint8_t** buff,
		size_t size, link_data_ext_info_t* extended_info,
		int32_t msec, uint16_t id);

static eos_error_t sink_cron_plyr_select(link_handle_t link, uint8_t es_idx);
static eos_error_t sink_cron_plyr_deselect(link_handle_t link, uint8_t es_idx);
static eos_error_t sink_cron_plyr_enable(link_handle_t link, uint8_t es_idx);
static eos_error_t sink_cron_plyr_disable(link_handle_t link, uint8_t es_idx);

static eos_error_t sink_cron_plyr_vid_move(link_handle_t link, uint16_t x,
		uint16_t y);
static eos_error_t sink_cron_plyr_vid_scale(link_handle_t link, uint16_t w,
		uint16_t h);
static eos_error_t sink_cron_plyr_aud_pass_trgh(link_handle_t link,
		bool enable);
static eos_error_t sink_cron_plyr_vol_leveling(link_handle_t link,
		bool enable, eos_out_vol_lvl_t lvl);
static eos_error_t sink_cron_plyr_set_reg(set_reg_opt_t opt,
		set_reg_val_t* val, void* cookie);
static inline void sink_cron_plyr_set_state(sink_cron_plyr_priv_t* priv,
		cron_plyr_state_t state, bool lock);
static inline bool sink_cron_plyr_is_state(sink_cron_plyr_priv_t* priv,
		cron_plyr_state_t state, bool lock);
static eos_error_t sink_cron_plyr_set_reg_sys(set_reg_opt_t opt,
		set_reg_val_t* val, void* opaque);


static sink_command_t sink_cron_plyr_cmd =
{
	.setup = sink_cron_plyr_setup,
	.start = sink_cron_plyr_start,
	.stop = sink_cron_plyr_stop,
	.pause = sink_cron_plyr_pause,
	.resume = sink_cron_plyr_resume,
	.flush_buffs = sink_cron_plyr_flush_buffs,
	.get_ctrl_funcs = sink_cron_plyr_get_ctrl,
	.reg_ev_hnd = sink_cron_plyr_reg_ev_hnd,
};

static link_cap_stream_sel_ctrl_t sink_cron_plyr_sel =
{
	.select = sink_cron_plyr_select,
	.deselect = sink_cron_plyr_deselect,
	.enable = sink_cron_plyr_enable,
	.disable = sink_cron_plyr_disable
};

static link_cap_av_out_set_ctrl_t sink_cron_plyr_av_set =
{
	.vid_move = sink_cron_plyr_vid_move,
	.vid_scale = sink_cron_plyr_vid_scale,
	.aud_pass_trgh = sink_cron_plyr_aud_pass_trgh,
	.vol_leveling = sink_cron_plyr_vol_leveling
};

static sink_t sink_cron_plyr;

CALL_ON_LOAD(sink_cron_plyr_reg)
static void sink_cron_plyr_reg(void)
{
	sink_cron_plyr_priv_t *priv = NULL;

	sink_cron_plyr_init(&sink_cron_plyr, NULL);

	priv = SINK_CRON_PLYR_GET_PRIV((&sink_cron_plyr));

	sink_factory_register(&sink_cron_plyr, &priv->magic,
			sink_cron_plyr_manufacture, sink_cron_plyr_dismantle);
	cron_plyr_sys_init();
	set_reg_set_sys_cbk(0, SET_REG_OPT_AUD_MODE, sink_cron_plyr_set_reg_sys,
			NULL);
}

CALL_ON_UNLOAD(sink_cron_plyr_unreg)
static void sink_cron_plyr_unreg(void)
{
	sink_cron_plyr_priv_t *priv = SINK_CRON_PLYR_GET_PRIV((&sink_cron_plyr));

	sink_factory_unregister(&sink_cron_plyr, priv->magic);
	sink_cron_plyr_deinit(&sink_cron_plyr);
	cron_plyr_sys_deinit();
	set_reg_set_sys_cbk(0, SET_REG_OPT_AUD_MODE, NULL, NULL);
}


static eos_error_t sink_cron_plyr_init(sink_t* sink,
		sink_factory_id_data_t* data)
{
	sink_cron_plyr_priv_t *priv = NULL;
	eos_error_t err = EOS_ERROR_OK;

	if(sink == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	osi_memset(sink, 0, sizeof(sink_t));
	strcpy(sink->name, SINK_CRON_PLYR_NAME);
	if(cron_plyr_sys_get_name() != NULL)
	{
		sprintf(sink->name, "%s:%s", SINK_CRON_PLYR_NAME,
				cron_plyr_sys_get_name());
	}
	cron_plyr_sys_get_caps(&sink->caps);
	cron_plyr_sys_get_plug(&sink->plug_type);
	sink->command = sink_cron_plyr_cmd;
	priv = (sink_cron_plyr_priv_t*)osi_calloc(sizeof(sink_cron_plyr_priv_t));
	if(priv == NULL)
	{
		return EOS_ERROR_NOMEM;
	}
	sink->priv = priv;
	sink->plug.handle = sink->priv;
	sink->plug.allocate = sink_cron_plyr_alloc;
	sink->plug.commit = sink_cron_plyr_commit;
	if(data != NULL)
	{
		err = cron_plyr_create(&priv->player, data->id == 0 ?
				CRON_PLYR_DISP_MAIN : CRON_PLYR_DISP_PIP);
		if(err != EOS_ERROR_OK)
		{
			osi_free((void**)&priv);
			return err;
		}
		cron_plyr_reg_ev_hnd(priv->player,sink_cron_plyr_ev_cbk, priv);
	}
	err = osi_mutex_create(&priv->lock);
	if(err != EOS_ERROR_OK)
	{
		osi_free((void**)&priv);
		return err;
	}
	err = util_log_create(&priv->log, SINK_CRON_PLYR_NAME);
	if(err != EOS_ERROR_OK)
	{
		osi_mutex_destroy(&priv->lock);
		osi_free((void**)&priv);
		return err;
	}
	priv->state = CRON_PLYR_STATE_IDLE;

	return EOS_ERROR_OK;
}

static eos_error_t sink_cron_plyr_deinit(sink_t* sink)
{
	sink_cron_plyr_priv_t *priv = NULL;

	if(sink == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	priv = SINK_CRON_PLYR_GET_PRIV(sink);
	if(priv->player != NULL)
	{
		cron_plyr_destroy(&priv->player);
	}
	if (priv->lock != NULL)
	{
		SINK_CRON_PLYR_LOCK(priv);
		SINK_CRON_PLYR_UNLOCK(priv);
		osi_mutex_destroy(&priv->lock);
	}
	if (priv->log != NULL)
	{
		util_log_destroy(&priv->log);
	}
	osi_free((void**)&priv);

	return EOS_ERROR_OK;
}

cron_plyr_t* sink_cron_plyr_get_plyr(sink_t* sink)
{
	sink_cron_plyr_priv_t *priv = NULL;

	if(sink == NULL)
	{
		return NULL;
	}
	priv = SINK_CRON_PLYR_GET_PRIV(sink);
	if(priv->player != NULL)
	{
		return priv->player;
	}

	return NULL;
}

static eos_error_t sink_cron_plyr_start(sink_t* sink)
{
	sink_cron_plyr_priv_t *priv = NULL;
	eos_error_t err = EOS_ERROR_OK;

	if(sink == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	priv = SINK_CRON_PLYR_GET_PRIV((sink));
	if(priv->player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	SINK_CRON_PLYR_LOCK(priv);
	if(sink_cron_plyr_is_state(priv, CRON_PLYR_STATE_STARTED, false))
	{
		UTIL_LOGW(priv->log, "Ignoring Start: Already started");
		SINK_CRON_PLYR_UNLOCK(priv);
		return EOS_ERROR_OK;
	}
	if((err = cron_plyr_start(priv->player)) != EOS_ERROR_OK)
	{
		sink_cron_plyr_set_state(priv, CRON_PLYR_STATE_STOPPED, false);
	}
	else
	{
		priv->frst_I_frame_rcvd = false;
		sink_cron_plyr_set_state(priv, CRON_PLYR_STATE_STARTED, false);
	}
	SINK_CRON_PLYR_UNLOCK(priv);

	return err;
}

static eos_error_t sink_cron_plyr_stop(sink_t* sink)
{
	sink_cron_plyr_priv_t *priv = NULL;
	eos_error_t err = EOS_ERROR_OK;

	if(sink == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	priv = SINK_CRON_PLYR_GET_PRIV((sink));
	if(priv->player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	SINK_CRON_PLYR_LOCK(priv);
	if(sink_cron_plyr_is_state(priv, CRON_PLYR_STATE_STOPPED, false))
	{
		SINK_CRON_PLYR_UNLOCK(priv);
		UTIL_LOGW(priv->log, "Ignoring Stop: Already stopped");
		return EOS_ERROR_OK;
	}
	UTIL_LOGW(priv->log, "Forcing event handler to NULL");
	priv->handler = NULL;
	if((err = cron_plyr_stop(priv->player)) != EOS_ERROR_OK)
	{
		UTIL_LOGE(priv->log, "Stop failed (%d)", err);
	}
	else
	{
		sink_cron_plyr_set_state(priv, CRON_PLYR_STATE_STOPPED, false);
	}
	SINK_CRON_PLYR_UNLOCK(priv);

	return err;
}

static eos_error_t sink_cron_plyr_pause(sink_t* sink, bool buffering)
{
	sink_cron_plyr_priv_t *priv = NULL;
	eos_error_t err = EOS_ERROR_OK;

	if(sink == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	priv = SINK_CRON_PLYR_GET_PRIV(sink);
	if(priv->player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	SINK_CRON_PLYR_LOCK(priv);
	if(sink_cron_plyr_is_state(priv, CRON_PLYR_STATE_IDLE, false)
			|| sink_cron_plyr_is_state(priv, CRON_PLYR_STATE_STOPPED, false))
	{
		SINK_CRON_PLYR_UNLOCK(priv);
		UTIL_LOGW(priv->log, "Ignoring Pause: Stream not started");
		return EOS_ERROR_OK;
	}

	if((err = cron_plyr_pause(priv->player, buffering)) == EOS_ERROR_OK)
	{
		sink_cron_plyr_set_state(priv, buffering ? CRON_PLYR_STATE_BUFFERING
				: CRON_PLYR_STATE_PAUSED, false);
	}

	SINK_CRON_PLYR_UNLOCK(priv);
	return err;
}

static eos_error_t sink_cron_plyr_resume(sink_t* sink)
{
	sink_cron_plyr_priv_t *priv = NULL;
	eos_error_t err = EOS_ERROR_OK;

	if(sink == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	priv = SINK_CRON_PLYR_GET_PRIV(sink);
	if(priv->player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	SINK_CRON_PLYR_LOCK(priv);
	if(sink_cron_plyr_is_state(priv, CRON_PLYR_STATE_IDLE, false)
			|| sink_cron_plyr_is_state(priv, CRON_PLYR_STATE_STOPPED, false))
	{
		SINK_CRON_PLYR_UNLOCK(priv);
		UTIL_LOGW(priv->log, "Ignoring Resume: Stream not started");
		return EOS_ERROR_OK;
	}

	if((err = cron_plyr_resume(priv->player)) == EOS_ERROR_OK)
	{
		sink_cron_plyr_set_state(priv, CRON_PLYR_STATE_STARTED, false);
	}

	SINK_CRON_PLYR_UNLOCK(priv);
	return err;
}

static eos_error_t sink_cron_plyr_flush_buffs(sink_t* sink)
{
	sink_cron_plyr_priv_t *priv = NULL;
	eos_error_t error = EOS_ERROR_OK;
	cron_plyr_state_t tmp_state;

	if(sink == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	priv = SINK_CRON_PLYR_GET_PRIV((sink));
	if(priv->player == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	SINK_CRON_PLYR_LOCK(priv);

	tmp_state = priv->state;
	priv->state = CRON_PLYR_STATE_PAUSED;
	error = cron_plyr_flush(priv->player);
	priv->state = tmp_state;

	SINK_CRON_PLYR_UNLOCK(priv);

	return error;
}

static eos_error_t sink_cron_plyr_get_ctrl(link_handle_t link, link_cap_t cap,
		void** ctrl_funcs)
{
	sink_t* sink = (sink_t*) link;
	sink_cron_plyr_priv_t *priv = NULL;

	if(sink == NULL || ctrl_funcs == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	priv = SINK_CRON_PLYR_GET_PRIV(sink);
	if(priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	switch(cap)
	{
	case LINK_CAP_STREAM_SEL:
		*ctrl_funcs = &sink_cron_plyr_sel;
		break;
	case LINK_CAP_AV_OUT_SET:
		*ctrl_funcs = &sink_cron_plyr_av_set;
		break;
	default:
		return cron_plyr_get_ext(link, cap, ctrl_funcs);
	}

	return EOS_ERROR_OK;
}

static eos_error_t sink_cron_plyr_reg_ev_hnd(link_handle_t link,
		link_ev_hnd_t handler, void* cookie)
{
	sink_t *sink;
	sink_cron_plyr_priv_t *priv = NULL;

	if(link == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	sink = (sink_t*)link;
	priv = SINK_CRON_PLYR_GET_PRIV((sink));
	if(priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}
// TODO Add event handler mutex
//	SINK_CRON_PLYR_LOCK(priv);
	priv->handler = handler;
	priv->hnd_cookie = cookie;
//	SINK_CRON_PLYR_UNLOCK(priv);

	return EOS_ERROR_OK;
}

static eos_error_t sink_cron_plyr_ev_cbk(void* opaque, cron_plyr_ev_t event,
		cron_ply_ev_data_t* data)
{
	sink_cron_plyr_priv_t *priv = NULL;
	link_ev_data_t ev_data;
	int32_t mills = 0;

	if(opaque == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	priv = (sink_cron_plyr_priv_t*) opaque;
	if(priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	SINK_CRON_PLYR_LOCK(priv);
	if(priv->handler == NULL)
	{
		SINK_CRON_PLYR_UNLOCK(priv);
		return EOS_ERROR_OK;
	}
	switch(event)
	{
	case CRON_PLYR_EV_FRM:
		ev_data.frame.pts = data->first_frm.pts;
		ev_data.frame.key_frm = true;
		if(!priv->frst_I_frame_rcvd)
		{
			cron_plyr_get_av_delay(priv->player, &mills);			
			cron_plyr_set_sync_time(priv->player,
					SINK_CRON_SYNC_COEFF * mills);
			priv->frst_I_frame_rcvd = true;
		}
		priv->handler(LINK_EV_FRAME_DISP, &ev_data, priv->hnd_cookie,
				priv->product_id);
		break;
	case CRON_PLYR_EV_VDEC_INFO:
		UTIL_GLOGD("Video info: w = %u h = %u frame rate = %u/%u",
				data->vdec_info.width, data->vdec_info.height,
				data->vdec_info.frame_rate_num,
				data->vdec_info.frame_rate_den);
		break;
	case CRON_PLYR_EV_VDEC_RES:
		UTIL_GLOGD("Video resolution changed to w = %u, h = %u",
				data->vdec_res.width, data->vdec_res.height);
		break;
	case CRON_PLYR_EV_ERR:
		/* TODO: switch/case error code */
		ev_data.err.desc = LINK_PBK_ERR_SYS;
		priv->handler(LINK_EV_PBK_ERR, &ev_data, priv->hnd_cookie,
				priv->product_id);
		break;
	case CRON_PLYR_EV_VOUT_BEGIN:
		priv->handler(LINK_EV_BOS, &ev_data, priv->hnd_cookie,
				priv->product_id);
		break;
	case CRON_PLYR_EV_VOUT_END:
		priv->handler(LINK_EV_EOS, &ev_data, priv->hnd_cookie,
				priv->product_id);
		break;
	case CRON_PLYR_EV_WM:
		if(data->wm.wm == CRON_PLYR_WM_LOW)
		{
			priv->handler(LINK_EV_LOW_WM, &ev_data, priv->hnd_cookie,
					priv->product_id);
		}
		else if(data->wm.wm == CRON_PLYR_WM_NORMAL)
		{
			priv->handler(LINK_EV_NORMAL_WM, &ev_data, priv->hnd_cookie,
					priv->product_id);
		}
		else
		{
			priv->handler(LINK_EV_HIGH_WM, &ev_data, priv->hnd_cookie,
					priv->product_id);
		}
		break;
	default:
		/* TODO: all events... */
		break;
	}
	SINK_CRON_PLYR_UNLOCK(priv);

	return EOS_ERROR_OK;
}

static eos_error_t sink_cron_plyr_setup(sink_t* sink, uint32_t id,
		eos_media_desc_t* media)
{
	eos_error_t err = EOS_ERROR_OK;
	set_reg_val_t val ;
	sink_cron_plyr_priv_t *priv = NULL;
	cron_vol_lvl_t cron_lvl = CRON_VOL_LVL_NORMAL;

	if(sink == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	sink->id = id;

	priv = SINK_CRON_PLYR_GET_PRIV(sink);
	if(priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	err = cron_plyr_set_media_desc(priv->player, media);
	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	err = set_reg_fetch(sink->id, SET_REG_OPT_VID_POS, &val);
	if(err == EOS_ERROR_OK)
	{
		cron_plyr_vid_move(priv->player, val.vid_pos.x, val.vid_pos.y);
	}
	err = set_reg_fetch(sink->id, SET_REG_OPT_VID_SIZE, &val);
	if(err == EOS_ERROR_OK)
	{
		cron_plyr_vid_scale(priv->player, val.vid_size.w, val.vid_size.h);
	}
	err = set_reg_fetch(sink->id, SET_REG_OPT_AUD_MODE, &val);
	if(err == EOS_ERROR_OK)
	{
		cron_plyr_aud_pt(priv->player, val.aud_pt);
	}
	err = set_reg_fetch(sink->id, SET_REG_OPT_VOL_LVL, &val);
	if(err == EOS_ERROR_OK)
	{
		switch (val.vol_lvl.lvl)
		{
			case EOS_OUT_VOL_LVL_LIGHT:
				cron_lvl = CRON_VOL_LVL_LIGHT;
				break;
			case EOS_OUT_VOL_LVL_NORMAL:
				cron_lvl = CRON_VOL_LVL_NORMAL;
				break;
			case EOS_OUT_VOL_LVL_HEAVY:
				cron_lvl = CRON_VOL_LVL_HEAVY;
				break;
			default:
				cron_lvl = CRON_VOL_LVL_NORMAL;
				break;
		}
		cron_plyr_vol_leveling(priv->player, val.vol_lvl.enable,
				cron_lvl);
	}

	return set_reg_add_cbk(id, sink_cron_plyr_set_reg, sink);
}

static eos_error_t sink_cron_plyr_manufacture(sink_factory_id_data_t* data,
		sink_t* model, uint64_t model_id,
		sink_t** product, uint64_t product_id)
{
	if(model == NULL || product == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if(model != &sink_cron_plyr ||
			model_id != ((sink_cron_plyr_priv_t*)sink_cron_plyr.priv)->magic)
	{
		return EOS_ERROR_INVAL;
	}
	if((*product = osi_calloc(sizeof(sink_t))) == NULL)
	{
		return EOS_ERROR_NOMEM;
	}
	sink_cron_plyr_init(*product, data);
	((sink_cron_plyr_priv_t*)(*product)->priv)->magic = model_id;
	((sink_cron_plyr_priv_t*)(*product)->priv)->product_id = product_id;

	return EOS_ERROR_OK;
}

static eos_error_t sink_cron_plyr_dismantle(uint64_t model_id, sink_t** clone)
{
	uint32_t id = 0;
	void *cookie = NULL;

	if(clone == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if(model_id != ((sink_cron_plyr_priv_t*)sink_cron_plyr.priv)->magic)
	{
		return EOS_ERROR_INVAL;
	}
	if(*clone == NULL || *clone == &sink_cron_plyr)
	{
		return EOS_ERROR_INVAL;
	}
	id = (*clone)->id;
	cookie = *clone;
	sink_cron_plyr_deinit(*clone);
	osi_free((void**)clone);
	set_reg_remove_cbk(id, sink_cron_plyr_set_reg, cookie);

	return EOS_ERROR_OK;
}

static eos_error_t sink_cron_plyr_alloc(link_handle_t opaque,
		uint8_t** buff, size_t* size, link_data_ext_info_t* extended_info,
		int32_t msec, uint16_t id)
{
	sink_cron_plyr_priv_t *priv = NULL;

	if(opaque == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	priv = (sink_cron_plyr_priv_t*)opaque;
	if(priv->player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if (priv->state == CRON_PLYR_STATE_PAUSED)
	{
		return EOS_ERROR_AGAIN;
	}

	return cron_plyr_buff_get(priv->player, buff, size, extended_info,
			msec, id);
}

static eos_error_t sink_cron_plyr_commit(link_handle_t opaque,
		uint8_t** buff, size_t size, link_data_ext_info_t* extended_info,
		int32_t msec, uint16_t id)
{
	sink_cron_plyr_priv_t *priv = NULL;

	if(opaque == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	priv = (sink_cron_plyr_priv_t*)opaque;
	if(priv->player == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (priv->state == CRON_PLYR_STATE_PAUSED)
	{
		return EOS_ERROR_AGAIN;
	}

	return cron_plyr_buff_cons(priv->player, *buff, size, extended_info,
			msec, id);
}

static eos_error_t sink_cron_plyr_select(link_handle_t link, uint8_t es_idx)
{
	sink_t* sink = (sink_t*) link;
	sink_cron_plyr_priv_t *priv = NULL;
	eos_error_t err = EOS_ERROR_OK;
	cron_plyr_es_t cron_es = CRON_PLYR_ES_NONE;

	if(sink == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	priv = SINK_CRON_PLYR_GET_PRIV(sink);
	err = cron_plyr_es_set(priv->player, es_idx);
	if(err != EOS_ERROR_OK)
	{
		UTIL_LOGE(priv->log, "Failed to set ES (%d)", err);
		return err;
	}
	cron_plyr_es_get(priv->player, es_idx, &cron_es);

	osi_mutex_lock(priv->lock);
	if(priv->state == CRON_PLYR_STATE_STARTED && (cron_es == CRON_PLYR_ES_AUD
			|| cron_es == CRON_PLYR_ES_VID))
	{
		err = cron_plyr_es_start(priv->player, cron_es);
		if(err != EOS_ERROR_OK)
		{
			cron_plyr_es_unset(priv->player, es_idx);
			osi_mutex_unlock(priv->lock);
			UTIL_LOGE(priv->log, "Failed to start ES (%d)", err);
			return err;
		}
	}
	osi_mutex_unlock(priv->lock);

	return EOS_ERROR_OK;
}

static eos_error_t sink_cron_plyr_deselect(link_handle_t link, uint8_t es_idx)
{
	sink_t* sink = (sink_t*) link;
	cron_plyr_es_t cron_es = CRON_PLYR_ES_NONE;
	sink_cron_plyr_priv_t *priv = NULL;
	eos_error_t err = EOS_ERROR_OK;

	if(sink == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	priv = SINK_CRON_PLYR_GET_PRIV(sink);
	/* Is this ES already selected? */
	cron_plyr_es_get(priv->player, es_idx, &cron_es);
	if(cron_es == CRON_PLYR_ES_NONE)
	{
		UTIL_LOGE(priv->log, "ES not selected %u", es_idx);
		return EOS_ERROR_PERM;
	}
	osi_mutex_lock(priv->lock);
	if(priv->state == CRON_PLYR_STATE_STARTED)
	{
		err = cron_plyr_es_stop(priv->player, cron_es);
		if(err != EOS_ERROR_OK)
		{
			osi_mutex_unlock(priv->lock);
			UTIL_LOGE(priv->log, "Failed to stop ES (%d)", err);
			return err;
		}
	}
	osi_mutex_unlock(priv->lock);
	cron_plyr_es_unset(priv->player, es_idx);

	return EOS_ERROR_OK;
}

static eos_error_t sink_cron_plyr_enable(link_handle_t link, uint8_t es_idx)
{
	sink_t* sink = (sink_t*) link;
	cron_plyr_es_t cron_es = CRON_PLYR_ES_NONE;
	sink_cron_plyr_priv_t *priv = NULL;
	eos_error_t err = EOS_ERROR_OK;

	UTIL_GLOGI("Enable ...");

	if(sink == NULL)
	{
		UTIL_GLOGE("Enable [Failure]");
		return EOS_ERROR_INVAL;
	}
	priv = SINK_CRON_PLYR_GET_PRIV(sink);
	/* Is this ES already selected? */
	cron_plyr_es_get(priv->player, es_idx, &cron_es);
	if(cron_es == CRON_PLYR_ES_NONE)
	{
		UTIL_LOGE(priv->log, "ES not selected %u", es_idx);
		UTIL_GLOGE("Enable [Failure]");
		return EOS_ERROR_PERM;
	}
	osi_mutex_lock(priv->lock);
	if ((priv->state == CRON_PLYR_STATE_STARTED)
			|| (priv->state == CRON_PLYR_STATE_PAUSED)
			|| (priv->state == CRON_PLYR_STATE_BUFFERING))
	{
		err = cron_plyr_es_enable(priv->player, cron_es, true);
		if(err != EOS_ERROR_OK)
		{
			osi_mutex_unlock(priv->lock);
			UTIL_LOGE(priv->log, "Failed to enable ES (%d)", err);
			UTIL_GLOGE("Enable [Failure]");
			return err;
		}
	}
	osi_mutex_unlock(priv->lock);

	return EOS_ERROR_OK;
}

static eos_error_t sink_cron_plyr_disable(link_handle_t link, uint8_t es_idx)
{
	sink_t* sink = (sink_t*) link;
	cron_plyr_es_t cron_es = CRON_PLYR_ES_NONE;
	sink_cron_plyr_priv_t *priv = NULL;
	eos_error_t err = EOS_ERROR_OK;

	UTIL_GLOGI("Disable ...");

	if(sink == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	priv = SINK_CRON_PLYR_GET_PRIV(sink);
	/* Is this ES already selected? */
	cron_plyr_es_get(priv->player, es_idx, &cron_es);
	if(cron_es == CRON_PLYR_ES_NONE)
	{
		UTIL_LOGE(priv->log, "ES not selected %u", es_idx);
		return EOS_ERROR_PERM;
	}
	osi_mutex_lock(priv->lock);
	if ((priv->state == CRON_PLYR_STATE_STARTED)
			|| (priv->state == CRON_PLYR_STATE_PAUSED)
			|| (priv->state == CRON_PLYR_STATE_BUFFERING))
	{
		err = cron_plyr_es_enable(priv->player, cron_es, false);
		if(err != EOS_ERROR_OK)
		{
			osi_mutex_unlock(priv->lock);
			UTIL_LOGE(priv->log, "Failed to disable ES (%d)", err);
			return err;
		}
	}
	osi_mutex_unlock(priv->lock);
	
	return EOS_ERROR_OK;
}

static eos_error_t sink_cron_plyr_vid_move(link_handle_t link,
		uint16_t x, uint16_t y)
{
	sink_t* sink = (sink_t*) link;
	sink_cron_plyr_priv_t *priv = NULL;

	if(sink == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	priv = SINK_CRON_PLYR_GET_PRIV(sink);

	return cron_plyr_vid_move(priv->player, x, y);
}

static eos_error_t sink_cron_plyr_vid_scale(link_handle_t link,
		uint16_t w, uint16_t h)
{
	sink_t* sink = (sink_t*) link;
	sink_cron_plyr_priv_t *priv = NULL;

	if(sink == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	priv = SINK_CRON_PLYR_GET_PRIV(sink);

	return cron_plyr_vid_scale(priv->player, w, h);
}

static eos_error_t sink_cron_plyr_aud_pass_trgh(link_handle_t link,
		bool enable)
{
	sink_t* sink = (sink_t*) link;
	sink_cron_plyr_priv_t *priv = NULL;

	if(sink == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	priv = SINK_CRON_PLYR_GET_PRIV(sink);

	return cron_plyr_aud_pt(priv->player, enable);
}

static eos_error_t sink_cron_plyr_vol_leveling(link_handle_t link,
		bool enable, eos_out_vol_lvl_t lvl)
{
	sink_t* sink = (sink_t*) link;
	sink_cron_plyr_priv_t *priv = NULL;
	cron_vol_lvl_t cron_lvl = CRON_VOL_LVL_NORMAL;

	if(sink == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	priv = SINK_CRON_PLYR_GET_PRIV(sink);

	switch (lvl)
	{
		case EOS_OUT_VOL_LVL_LIGHT:
			cron_lvl = CRON_VOL_LVL_LIGHT;
			break;
		case EOS_OUT_VOL_LVL_NORMAL:
			cron_lvl = CRON_VOL_LVL_NORMAL;
			break;
		case EOS_OUT_VOL_LVL_HEAVY:
			cron_lvl = CRON_VOL_LVL_HEAVY;
			break;
		default:
			cron_lvl = CRON_VOL_LVL_NORMAL;
			break;
	}

	return cron_plyr_vol_leveling(priv->player, enable, cron_lvl);
}

static eos_error_t sink_cron_plyr_set_reg(set_reg_opt_t opt,
		set_reg_val_t* val, void* cookie)
{
	sink_t* sink = (sink_t*) cookie;

	if(val == NULL || cookie == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	switch(opt)
	{
	case SET_REG_OPT_VID_POS:
		return sink_cron_plyr_vid_move(sink, val->vid_pos.x,
				val->vid_pos.y);
	case SET_REG_OPT_VID_SIZE:
		return sink_cron_plyr_vid_scale(sink, val->vid_size.w,
						val->vid_size.h);
	case SET_REG_OPT_AUD_MODE:
		return sink_cron_plyr_aud_pass_trgh(sink, val->aud_pt);
	case SET_REG_OPT_VOL_LVL:
		return sink_cron_plyr_vol_leveling(sink,
				val->vol_lvl.enable, val->vol_lvl.lvl);
	default:
		break;
	}

	return EOS_ERROR_OK;
}

static inline void sink_cron_plyr_set_state(sink_cron_plyr_priv_t* priv,
		cron_plyr_state_t state, bool lock)
{
	if(lock)
	{
		SINK_CRON_PLYR_LOCK(priv);
	}
	priv->state = state;
	if(lock)
	{
		SINK_CRON_PLYR_UNLOCK(priv);
	}
}

static inline bool sink_cron_plyr_is_state(sink_cron_plyr_priv_t* priv,
		cron_plyr_state_t state, bool lock)
{
	bool is = false;

	if(lock)
	{
		SINK_CRON_PLYR_LOCK(priv);
	}
	priv->state == state ? (is = true) : (is = false);
	if(lock)
	{
		SINK_CRON_PLYR_UNLOCK(priv);
	}

	return is;
}

static eos_error_t sink_cron_plyr_set_reg_sys(set_reg_opt_t opt,
		set_reg_val_t* val, void* opaque)
{
	EOS_UNUSED(opaque);

	if(opt == SET_REG_OPT_AUD_MODE)
	{
		UTIL_GLOGD("Pass trough system set to %u", val->aud_pt);

		return cron_plyr_aud_pt(NULL, val->aud_pt);
	}

	return EOS_ERROR_OK;
}
