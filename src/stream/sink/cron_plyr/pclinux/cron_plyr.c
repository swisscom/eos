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
#include "osi_thread.h"
#include "util_rbuff.h"
#include "util_msgq.h"

#define MODULE_NAME "core"
#include "util_log.h"

#include "libav_dmx.h"
#include "libav_dec.h"
#include "alsa_aren.h"
#include "x11_vren.h"

#include <libavformat/avformat.h>
#include <libavutil/log.h>

//#define CRON_PLYR_LIBAV_DBG (0)

#define CRON_PLYR_CAPS (LINK_CAP_SINK | LINK_CAP_STREAM_SEL | \
		LINK_CAP_AV_OUT_SET)
#define CRON_PLYR_PLUG_TYPE (LINK_IO_TYPE_TS | LINK_IO_TYPE_SPROG_TS | \
		LINK_IO_TYPE_MPROG_TS)
/* about 768k rounded to TS packet size */
//#define CRON_PLYR_PROBE_SZ (188 * 800)
#define CRON_PLYR_PROBE_SZ (188 * 10000)

/* about 2M rounded to TS packet size */
#define CRON_PLYR_INBUFF_SZ (188 * 11000)
/* about 4k rounded to TS packet size */
#define CRON_PLYR_IOCTX_SZ (188 * 22)

#define CRON_PLYR_AUD_QUEUE_SZ (10)
#define CRON_PLYR_VID_QUEUE_SZ (50)

#define CRON_PLYR_LOCK(player) osi_mutex_lock(player->lock)
#define CRON_PLYR_UNLOCK(player) osi_mutex_unlock(player->lock)

typedef struct cron_plyr_sync
{
	double t_base;
	double a_start;
	double v_start;
	double a_pts;
	double v_pts;
	uint32_t v_clk_tmt;
	uint32_t threshold;
	osi_mutex_t *lock;
} cron_plyr_sync_t;

struct cron_plyr
{
	eos_media_desc_t media;
	cron_plyr_ev_cbk_t cbk;
	void *cbk_data;
	bool aud_started;
	bool vid_started;
	osi_mutex_t *lock;
	util_rbuff_t *in_rb;
	uint8_t *in_buff;
	bool freerun;
	uint8_t slowdown;
	uint32_t dec_trshld;
	bool keep_frm;

	osi_thread_t *probe_thread;
	uint8_t *probe_buff;
	uint32_t probe_len;
	uint32_t probe_off;
	bool probe_done;
	AVFormatContext *avf_ctx;
	AVIOContext *io_ctx;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,48,101)
	AVCodecContext *a_codec;
	AVCodecContext *v_codec;
#endif
	void *io_ctx_buff;
	util_msgq_t *aud_queue;
	util_msgq_t *vid_queue;
	bool stopped;
	util_log_t *log;

	libav_dmx_t dmx;
	libav_dec_t dec;
	alsa_aren_t aren;
	x11_vren_t vren;
	cron_plyr_sync_t sync;

	uint16_t x;
	uint16_t y;
	uint16_t h;
	uint16_t w;
	bool aud_pt;
};

static const char *cron_plyr_name = "pclinux";

static int cron_plyr_read_pkt(void* opaque, uint8_t* buf, int buf_size);
static void cron_plyr_free_pkt(void* msg_data, size_t msg_size);
static void* cron_plyr_probe(void* arg);
static eos_error_t cron_plyr_dmx_aud(void* opaque, AVPacket* pkt);
static eos_error_t cron_plyr_dmx_vid(void* opaque, AVPacket* pkt);
static eos_error_t cron_plyr_dec_aud(void* opaque, AVFrame* frame);
static eos_error_t cron_plyr_dec_vid(void* opaque, AVFrame* frame);
static void cron_plyr_i_frm(void* opaque, uint64_t pts);
static eos_error_t cron_plyr_clk_vid(void* opaque, x11_frame_t* frame,
		uint32_t* micros, osi_time_t* offset);
static void cron_plyr_libav_log(void*, int, const char*, va_list);
static eos_error_t cron_plyr_fire_ev(cron_plyr_t* player,
		cron_plyr_ev_t event, cron_ply_ev_data_t* data);
static inline eos_error_t cron_plyr_fire_err_ev(cron_plyr_t* player,
		cron_plyr_err_t event);
static inline cron_plyr_es_t cron_plyr_conv_es(eos_media_es_t* media_es);
static inline eos_media_es_t* cron_plyr_get_sel(cron_plyr_t* player,
		cron_plyr_es_t es);

eos_error_t cron_plyr_sys_init(void)
{
#if ( LIBAVFORMAT_VERSION_INT <= AV_VERSION_INT(58,9,100) )
	av_register_all();
#endif
	av_log_set_callback(cron_plyr_libav_log);
	x11_vren_sys_init();

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_sys_deinit(void)
{
	x11_vren_sys_deinit();

	return EOS_ERROR_OK;
}

const char* cron_plyr_sys_get_name(void)
{
	return cron_plyr_name;
}

eos_error_t cron_plyr_sys_get_caps(uint64_t* caps)
{
	if(caps == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	*caps = CRON_PLYR_CAPS;

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_sys_get_plug(link_io_type_t* plug)
{
	if(plug == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	*plug = CRON_PLYR_PLUG_TYPE;

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_create(cron_plyr_t** player, cron_plyr_disp_t disp)
{
	cron_plyr_t *tmp = NULL;
	eos_error_t err = EOS_ERROR_OK;
	util_rbuff_attr_t attr;

	EOS_UNUSED(disp);
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
	osi_memset(&attr, 0, sizeof(util_rbuff_attr_t));
	attr.buff = osi_malloc(CRON_PLYR_INBUFF_SZ);
	if(attr.buff == NULL)
	{
		err = EOS_ERROR_NOMEM;
		goto done;
	}
	attr.size = CRON_PLYR_INBUFF_SZ;
	if((err = util_rbuff_create(&attr, &tmp->in_rb)) != EOS_ERROR_OK)
	{
		goto done;
	}
	tmp->in_buff = attr.buff;

	if((tmp->io_ctx_buff = av_malloc(CRON_PLYR_IOCTX_SZ)) == NULL)
	{
		err = EOS_ERROR_NOMEM;
		goto done;
	}
	if((tmp->io_ctx = avio_alloc_context(tmp->io_ctx_buff,
			CRON_PLYR_IOCTX_SZ, 0, tmp,
			cron_plyr_read_pkt, NULL, NULL)) == NULL)
	{
		err = EOS_ERROR_NOMEM;
		goto done;
	}
	tmp->io_ctx->seekable = 0;
	if((err = util_msgq_create(&tmp->aud_queue, CRON_PLYR_AUD_QUEUE_SZ,
			cron_plyr_free_pkt)) != EOS_ERROR_OK)
	{
		goto done;
	}
	if((err = util_msgq_create(&tmp->vid_queue, CRON_PLYR_VID_QUEUE_SZ,
			cron_plyr_free_pkt)) != EOS_ERROR_OK)
	{
		goto done;
	}
	if((err = util_log_create(&tmp->log, (char*)cron_plyr_name))
			!= EOS_ERROR_OK)
	{
		goto done;
	}
	tmp->aud_started = false;
	tmp->vid_started = false;
	osi_memset(&(tmp->media), 0, sizeof(eos_media_desc_t));
	*player = tmp;

done:
	if(err != EOS_ERROR_OK && tmp != NULL)
	{
		if(tmp->lock != NULL)
		{
			osi_mutex_destroy(&tmp->lock);
		}
		if(tmp->in_buff == NULL)
		{
			osi_free((void**)&tmp->in_buff);
		}
		if(tmp->in_rb != NULL)
		{
			util_rbuff_destroy(&tmp->in_rb);
		}
		if(tmp->io_ctx_buff != NULL && tmp->io_ctx == NULL)
		{
			av_free(tmp->io_ctx);
		}
		if(tmp->io_ctx != NULL)
		{
			avio_context_free(&tmp->io_ctx);
		}
		if(tmp->aud_queue != NULL)
		{
			util_msgq_destroy(&tmp->aud_queue);
		}
		if(tmp->vid_queue != NULL)
		{
			util_msgq_destroy(&tmp->vid_queue);
		}
		if(tmp->log != NULL)
		{
			util_log_destroy(&tmp->log);
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
	osi_free((void**)&tmp->in_buff);
	util_rbuff_destroy(&tmp->in_rb);
	util_msgq_destroy(&tmp->aud_queue);
	util_msgq_destroy(&tmp->vid_queue);
	osi_mutex_unlock(tmp->lock);
	osi_mutex_destroy(&tmp->lock);
	util_log_destroy(&tmp->log);

	av_free(tmp->io_ctx->buffer);
	avio_context_free(&tmp->io_ctx);

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
	return util_rbuff_reserve(player->in_rb, (void**)buff, *size, milis);
}

eos_error_t cron_plyr_buff_cons(cron_plyr_t* player, uint8_t* buff, size_t size,
		link_data_ext_info_t* extended_info, int32_t milis, uint16_t id)
{
	if(player == NULL || buff == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	EOS_UNUSED(milis);
	EOS_UNUSED(id);
	EOS_UNUSED(extended_info);

	return util_rbuff_commit(player->in_rb, buff, size);
}

eos_error_t cron_plyr_set_media_desc(cron_plyr_t* player,
		eos_media_desc_t* media)
{
	uint8_t i = 0;

	if(player == NULL || media == NULL)
	{
		return EOS_ERROR_INVAL;
	}
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
	CRON_PLYR_LOCK(player);
	player->freerun = enable;
	CRON_PLYR_UNLOCK(player);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_get_freerun(cron_plyr_t* player, bool* enable)
{
	if(player == NULL || enable == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	CRON_PLYR_LOCK(player);
	*enable = player->freerun;
	CRON_PLYR_UNLOCK(player);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_set_slowdown(cron_plyr_t* player, uint8_t milis_per_frm)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	CRON_PLYR_LOCK(player);
	player->slowdown = milis_per_frm;
	CRON_PLYR_UNLOCK(player);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_get_slowdown(cron_plyr_t* player, uint8_t* milis_per_frm)
{
	if(player == NULL || milis_per_frm == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	CRON_PLYR_LOCK(player);
	*milis_per_frm = player->slowdown;
	CRON_PLYR_UNLOCK(player);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_set_dec_trshld(cron_plyr_t* player, uint32_t milis)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	CRON_PLYR_LOCK(player);
	player->dec_trshld = milis;
	CRON_PLYR_UNLOCK(player);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_get_dec_trshld(cron_plyr_t* player, uint32_t* milis)
{
	if(player == NULL || milis == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	CRON_PLYR_LOCK(player);
	*milis = player->dec_trshld;
	CRON_PLYR_UNLOCK(player);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_set_keep_frm(cron_plyr_t* player, bool enable)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	CRON_PLYR_LOCK(player);
	player->keep_frm = enable;
	CRON_PLYR_UNLOCK(player);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_get_keep_frm(cron_plyr_t* player, bool* enable)
{
	if(player == NULL || enable == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	CRON_PLYR_LOCK(player);
	*enable = player->keep_frm;
	CRON_PLYR_UNLOCK(player);

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

	osi_mutex_lock(player->sync.lock);
	/* Threshold is defined as: threshold += (av_delay / target_time) * threshold */
	player->sync.threshold += (uint32_t)((((double)
			(OSI_TIME_SEC_TO_USEC(player->sync.v_start-player->sync.a_start))
			/ (double)OSI_TIME_MSEC_TO_USEC(millis))) * player->sync.threshold);
	osi_mutex_unlock(player->sync.lock);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_get_av_delay(cron_plyr_t* player, int32_t* millis)
{

	if(player == NULL || millis == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	*millis =(int32_t) OSI_TIME_SEC_TO_MSEC(player->sync.v_start - player->sync.a_start);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_set_wm(cron_plyr_t* player, cron_plyr_wm_t wm, uint32_t milis)
{
	EOS_UNUSED(player);
	EOS_UNUSED(wm);
	EOS_UNUSED(milis);
	// TODO: Implement buffer watermarking

	return EOS_ERROR_NIMPLEMENTED;
}

eos_error_t cron_plyr_es_set(cron_plyr_t* player, uint8_t es_idx)
{
	eos_error_t err = EOS_ERROR_OK;
	eos_media_es_t *to_select = NULL, *check = NULL;
	cron_plyr_es_t compare = CRON_PLYR_ES_NONE;
	cron_plyr_es_t check_es = CRON_PLYR_ES_NONE;
	uint8_t i = 0;

	if(player == NULL || es_idx >= player->media.es_cnt)
	{
		return EOS_ERROR_INVAL;
	}
	CRON_PLYR_LOCK(player);
	to_select = &(player->media.es[es_idx]);
	if(EOS_MEDIA_IS_VID(to_select->codec) == false
			&& EOS_MEDIA_IS_AUD(to_select->codec) == false)
	{
		CRON_PLYR_UNLOCK(player);
		return EOS_ERROR_NIMPLEMENTED;
	}
	compare = cron_plyr_conv_es(to_select);
	if(to_select->selected == false)
	{
		for(i=0; i<player->media.es_cnt; i++)
		{
			check = &(player->media.es[i]);
			check_es = cron_plyr_conv_es(check);
			if(check->selected == true && compare == check_es)
			{
				UTIL_LOGWTF(player->log, "ES (idx %u) with same type already "
						"selected (idx %u)!!!", es_idx, i);
				err = EOS_ERROR_INVAL;
				break;
			}
		}
		if(err == EOS_ERROR_OK)
		{
			to_select->selected = true;
		}
	}
	CRON_PLYR_UNLOCK(player);

	return err;
}

eos_error_t cron_plyr_es_unset(cron_plyr_t* player, uint8_t es_idx)
{
	eos_media_es_t *sel = NULL;

	if(player == NULL || es_idx >= player->media.es_cnt)
	{
		return EOS_ERROR_INVAL;
	}
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
	int idx = -1;
	double rate = 0.0;
	uint16_t x = 0, y = 0, w = 0, h = 0;
	eos_media_es_t *sel = NULL;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,48,101)
	AVCodec *dec = NULL;
#endif

	if(player == NULL || player->avf_ctx == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if((sel = cron_plyr_get_sel(player, es)) == NULL)
	{
		return EOS_ERROR_NFOUND;
	}
	if(es == CRON_PLYR_ES_VID)
	{
		for(int i = 0; i < (int)player->avf_ctx->nb_streams; i++)
		{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,48,101)
			if(player->avf_ctx->streams[i]->codecpar->codec_type
					== AVMEDIA_TYPE_VIDEO)
#else
			if(player->avf_ctx->streams[i]->codec->codec_type
					== AVMEDIA_TYPE_VIDEO)
#endif
			{
				if(player->avf_ctx->streams[i]->id == (int)sel->id)
				{
					idx = i;
				}
			}
		}
		if(idx == -1)
		{
			return EOS_ERROR_NFOUND;
		}
		libav_dmx_set_vid(&player->dmx, idx);
		player->sync.v_start = player->sync.t_base *
				player->avf_ctx->streams[idx]->start_time;
#if FF_API_R_FRAME_RATE
		rate = player->avf_ctx->streams[idx]->r_frame_rate.den /
				(double)player->avf_ctx->streams[idx]->r_frame_rate.num;
#else
		rate = player->avf_ctx->streams[idx]->avg_frame_rate.den /
						(double)player->avf_ctx->streams[idx]->avg_frame_rate.num;
#endif
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,48,101)
		if(player->avf_ctx->streams[idx]->codecpar->field_order !=
				AV_FIELD_PROGRESSIVE)
#else
		if(player->avf_ctx->streams[idx]->codec->field_order !=
				AV_FIELD_PROGRESSIVE)
#endif
		{
			rate *= 2;
		}
		player->sync.v_clk_tmt = (uint32_t)OSI_TIME_SEC_TO_USEC(rate);
		player->sync.threshold = (uint32_t)OSI_TIME_SEC_TO_USEC(rate);
		UTIL_GLOGD("%f (rate: %f) ", player->sync.v_start -
				player->sync.a_start, rate);
		x = player->x != 0 ? player->x : 0;
		y = player->y != 0 ? player->y : 0;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,48,101)
		h = player->h != 0 ? player->h : (uint16_t)player->avf_ctx->streams[idx]->codecpar->height;
		w = player->w != 0 ? player->w : (uint16_t)player->avf_ctx->streams[idx]->codecpar->width;

		x11_vren_init(&player->vren,w,h,x,y,
				player->avf_ctx->streams[idx]->codecpar->format, player->log,
				(uint16_t)player->avf_ctx->streams[idx]->codecpar->height,
				(uint16_t)player->avf_ctx->streams[idx]->codecpar->width);
		x11_vren_set_clk_cb(&player->vren, cron_plyr_clk_vid, player);

		if(av_find_best_stream(player->avf_ctx, AVMEDIA_TYPE_VIDEO, idx, -1, &dec, 0) < 0)
		{
			return EOS_ERROR_NFOUND;
		}
		if(player->v_codec)
		{
			avcodec_close(player->v_codec);
			avcodec_free_context(&player->v_codec);
		}
		player->v_codec = avcodec_alloc_context3(dec);
		if(player->v_codec == NULL)
		{
			return EOS_ERROR_NOMEM;
		}
		if(avcodec_parameters_to_context(player->v_codec, player->avf_ctx->streams[idx]->codecpar) < 0)
		{
			return EOS_ERROR_GENERAL;
		}
		libav_dec_start_vid(&player->dec, player->v_codec);
#else
		h = player->h != 0 ? player->h : (uint16_t)player->avf_ctx->streams[idx]->codec->height;
		w = player->w != 0 ? player->w : (uint16_t)player->avf_ctx->streams[idx]->codec->width;

		x11_vren_init(&player->vren,w,h,x,y,
				player->avf_ctx->streams[idx]->codec->pix_fmt, player->log,
				(uint16_t)player->avf_ctx->streams[idx]->codec->height,
				(uint16_t)player->avf_ctx->streams[idx]->codec->width);
		x11_vren_set_clk_cb(&player->vren, cron_plyr_clk_vid, player);

		libav_dec_start_vid(&player->dec,
				player->avf_ctx->streams[idx]->codec);
#endif
	}
	else if(es == CRON_PLYR_ES_AUD)
	{
		for(int i = 0; i < (int)player->avf_ctx->nb_streams; i++)
		{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,48,101)
			if(player->avf_ctx->streams[i]->codecpar->codec_type
					== AVMEDIA_TYPE_AUDIO)
#else
				if(player->avf_ctx->streams[i]->codec->codec_type
						== AVMEDIA_TYPE_AUDIO)
#endif
			{
				if(player->avf_ctx->streams[i]->id == (int)sel->id)
				{
					idx = i;
				}
			}
		}
		if(idx == -1)
		{
			return EOS_ERROR_NFOUND;
		}
		libav_dmx_set_aud(&player->dmx, idx);
		player->sync.t_base = av_q2d(player->avf_ctx->streams[idx]->time_base);
		player->sync.a_start = player->sync.t_base *
				player->avf_ctx->streams[idx]->start_time;

		alsa_aren_init(&player->aren);
		alsa_aren_setup(&player->aren);

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,48,101)
		if(av_find_best_stream(player->avf_ctx, AVMEDIA_TYPE_AUDIO, idx, -1, &dec, 0) < 0)
		{
			return EOS_ERROR_NFOUND;
		}
		if(player->a_codec)
		{
			avcodec_close(player->a_codec);
			avcodec_free_context(&player->a_codec);
		}
		player->a_codec = avcodec_alloc_context3(dec);
		if(player->a_codec == NULL)
		{
			return EOS_ERROR_NOMEM;
		}
		if(avcodec_parameters_to_context(player->a_codec, player->avf_ctx->streams[idx]->codecpar) < 0)
		{
			return EOS_ERROR_GENERAL;
		}
		libav_dec_start_aud(&player->dec, player->a_codec);
#else
		libav_dec_start_aud(&player->dec,
				player->avf_ctx->streams[idx]->codec);
#endif
	}

	return err;
}

eos_error_t cron_plyr_es_stop(cron_plyr_t* player, cron_plyr_es_t es)
{
	eos_error_t err = EOS_ERROR_OK;
	eos_media_es_t *sel = NULL;

	CRON_PLYR_LOCK(player);
	if(player->stopped == true)
	{
		CRON_PLYR_UNLOCK(player);
		return EOS_ERROR_OK;
	}
	if((sel = cron_plyr_get_sel(player, es)) == NULL)
	{
		CRON_PLYR_UNLOCK(player);
		return EOS_ERROR_NFOUND;
	}
	if(es == CRON_PLYR_ES_VID)
	{
		util_msgq_flush(player->vid_queue);
		x11_vren_deinit(&player->vren);
		util_msgq_put_urgent(player->vid_queue, NULL, 0);
		libav_dec_stop_vid(&player->dec);
		libav_dmx_set_vid(&player->dmx, LIBAV_DMX_IDX_NA);

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,48,101)
		if(player->v_codec)
		{
			avcodec_close(player->v_codec);
			avcodec_free_context(&player->v_codec);
			player->v_codec = NULL;
		}
#endif
	}
	else if(es == CRON_PLYR_ES_AUD)
	{
		libav_dmx_set_aud(&player->dmx, LIBAV_DMX_IDX_NA);
		util_msgq_put_urgent(player->aud_queue, NULL, 0);
		libav_dec_stop_aud(&player->dec);
		util_msgq_flush(player->aud_queue);
		alsa_aren_deinit(&player->aren);
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,48,101)
		if(player->a_codec)
		{
			avcodec_close(player->a_codec);
			avcodec_free_context(&player->a_codec);
			player->a_codec = NULL;
		}
#endif
	}
	CRON_PLYR_UNLOCK(player);

	return err;
}

eos_error_t cron_plyr_es_enable(cron_plyr_t* player,
		cron_plyr_es_t es, bool enabled)
{
	EOS_UNUSED(player);
	EOS_UNUSED(es);
	EOS_UNUSED(enabled);
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
	CRON_PLYR_LOCK(player);
	if(player->cbk != NULL && cbk != NULL)
	{
		UTIL_LOGW(player->log, "Owerwriting event callback!");
	}
	player->cbk = cbk;
	player->cbk_data = opaque;
	CRON_PLYR_UNLOCK(player);

	return EOS_ERROR_OK;
}

eos_error_t cron_plyr_start(cron_plyr_t* player)
{
	osi_thread_attr_t attr = {OSI_THREAD_JOINABLE};

	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	player->probe_buff = osi_calloc(2 * CRON_PLYR_PROBE_SZ);
	player->probe_len = 0;
	player->probe_off = 0;
	player->probe_done = false;

	return osi_thread_create(&player->probe_thread, &attr,
			cron_plyr_probe, player);
}

eos_error_t cron_plyr_stop(cron_plyr_t* player)
{
	eos_error_t err = EOS_ERROR_OK;

	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	util_rbuff_stop(player->in_rb);
	if((err = osi_thread_join(player->probe_thread, NULL)) != EOS_ERROR_OK)
	{
		// TODO: warning?
	}
	if((err = osi_thread_release(&player->probe_thread)) != EOS_ERROR_OK)
	{

	}
	osi_free((void**)&player->probe_buff);
	cron_plyr_es_stop(player, CRON_PLYR_ES_AUD);
	cron_plyr_es_stop(player, CRON_PLYR_ES_VID);
	util_msgq_pause(player->aud_queue);
	util_msgq_pause(player->vid_queue);
	libav_dmx_stop(&player->dmx);
	libav_dmx_deinit(&player->dmx);
	util_msgq_flush(player->aud_queue);
	util_msgq_flush(player->vid_queue);
	CRON_PLYR_LOCK(player);
	player->stopped = true;
	CRON_PLYR_UNLOCK(player);
	if(player->sync.lock != NULL)
	{
		osi_mutex_destroy(&player->sync.lock);
	}
	util_msgq_resume(player->aud_queue);
	util_msgq_resume(player->vid_queue);

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

	libav_dmx_pause(&player->dmx);
	libav_dec_pause_aud(&player->dec);
	libav_dec_pause_vid(&player->dec);
	alsa_aren_pause(&player->aren);
	x11_vren_pause(&player->vren);

	return err;
}

eos_error_t cron_plyr_resume(cron_plyr_t* player)
{
	eos_error_t err = EOS_ERROR_OK;

	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	libav_dmx_resume(&player->dmx);
	libav_dec_resume_aud(&player->dec);
	libav_dec_resume_vid(&player->dec);
	alsa_aren_resume(&player->aren);
	x11_vren_resume(&player->vren);

	return err;
}

eos_error_t cron_plyr_pause_resume(cron_plyr_t* player, uint32_t milis)
{
	eos_error_t err = EOS_ERROR_OK;

	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	alsa_aren_pause(&player->aren);
	x11_vren_pause(&player->vren);

	osi_time_usleep(OSI_TIME_MSEC_TO_USEC(milis));

	alsa_aren_resume(&player->aren);
	x11_vren_resume(&player->vren);

	return err;


}

eos_error_t cron_plyr_flush(cron_plyr_t* player)
{
	eos_error_t err = EOS_ERROR_OK;

	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("Flushing...");
	util_rbuff_rst(player->in_rb);
	avio_flush(player->io_ctx);
	player->io_ctx->pos = 0LL;
	util_msgq_flush(player->aud_queue);
	util_msgq_flush(player->vid_queue);
	libav_dec_flush_aud(&player->dec);
	libav_dec_flush_vid(&player->dec);
	util_msgq_flush(player->vren.queue);
	alsa_aren_flush(&player->aren);
	UTIL_GLOGD("Flushed");

	return err;

}

eos_error_t cron_plyr_force_sync(cron_plyr_t* player)
{
	EOS_UNUSED(player);
	// TODO: Implement forced synchronization

	return EOS_ERROR_NIMPLEMENTED;
}

eos_error_t cron_plyr_vid_blank(cron_plyr_t* player)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	// TODO: Implement video blanking

	return EOS_ERROR_NIMPLEMENTED;
}

eos_error_t cron_plyr_vid_move(cron_plyr_t* player, uint16_t x, uint16_t y)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	CRON_PLYR_LOCK(player);
	if(cron_plyr_get_sel(player, CRON_PLYR_ES_VID) == NULL ||
			player->probe_done == false)
	{
		player->x = x;
		player->y = y;
		CRON_PLYR_UNLOCK(player);

		return EOS_ERROR_OK;
	}
	CRON_PLYR_UNLOCK(player);

	return x11_vren_move(&player->vren, x, y);
}

eos_error_t cron_plyr_vid_scale(cron_plyr_t* player, uint16_t width, uint16_t height)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	CRON_PLYR_LOCK(player);
	if(cron_plyr_get_sel(player, CRON_PLYR_ES_VID) == NULL ||
			player->probe_done == false)
	{
		player->h = height;
		player->w = width;
		CRON_PLYR_UNLOCK(player);

		return EOS_ERROR_OK;
	}
	CRON_PLYR_UNLOCK(player);

	return x11_vren_scale(&player->vren, width, height);
}

eos_error_t cron_plyr_aud_pt(cron_plyr_t* player, bool enable)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}

//	if(player->aud_id == CRON_PLYR_ES_ID_INV)
//	{
//		player->aud_pt = enable;
//		return EOS_ERROR_OK;
//	}

	EOS_UNUSED(enable);

	return EOS_ERROR_NIMPLEMENTED;
}

eos_error_t cron_plyr_vol_leveling(cron_plyr_t* player, bool enable, cron_vol_lvl_t lvl)
{
	if(player == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	EOS_UNUSED(enable);
	EOS_UNUSED(lvl);

	return EOS_ERROR_NIMPLEMENTED;
}

eos_error_t cron_plyr_get_ext(cron_plyr_t* player, link_cap_t cap,
		void** ctrl_funcs)
{
	EOS_UNUSED(player);
	EOS_UNUSED(cap);
	EOS_UNUSED(ctrl_funcs);

	return EOS_ERROR_NFOUND;
}

int cron_plyr_read_pkt(void *opaque, uint8_t *buf, int buf_size)
{
	cron_plyr_t* player = (cron_plyr_t*)opaque;

	//UTIL_LOGWTF(player->log, "RD PKT len %d", buf_size);
	if(player->probe_off < player->probe_len && player->probe_done)
	{
#if 0
		osi_memcpy(buf, player->probe_buff + player->probe_off, buf_size);
		if((player->probe_off + buf_size) > player->probe_len)
		{
			UTIL_LOGWTF(player->log, "Wrong probe data requested: off %d len %d",
					player->probe_off, player->probe_len);
		}
		player->probe_off += buf_size;

		return buf_size;
#endif
	}
	if(util_rbuff_read_all(player->in_rb, (void*)buf, buf_size,
			UTIL_RBUFF_FOREVER) == EOS_ERROR_OK)
	{
#if 0
		if(!player->probe_done)
		{
			if(player->probe_len + buf_size > CRON_PLYR_PROBE_SZ * 2)
			{
				UTIL_LOGW(player->log, "Overwriting probe buff "
						"(buffer will be dropped!!: off %d len %d",
						player->probe_off, player->probe_len);
				player->probe_len = 0;
			}
			else
			{
				osi_memcpy(player->probe_buff + player->probe_len, buf, buf_size);
				player->probe_len += buf_size;
			}
		}
#endif
		return buf_size;
	}

	return -1;
}

static void cron_plyr_free_pkt(void* msg_data, size_t msg_size)
{
	if(msg_data == NULL || msg_size != sizeof(AVPacket))
	{
		return;
	}
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55,28,1)
	av_packet_unref((AVPacket *)msg_data);
#else
	av_free_packet((AVPacket *)msg_data);
#endif
	osi_free(&msg_data);

	return;
}

static void* cron_plyr_probe(void* arg)
{
	cron_plyr_t* player = (cron_plyr_t*)arg;
	libav_dmx_pkt_cb_t dmx_cb = {NULL, NULL, NULL};
	libav_dec_frame_cb_t dec_cb = {NULL, NULL, NULL, NULL};
	osi_time_t start, end, diff;
	eos_error_t err = EOS_ERROR_OK;

	err = libav_dmx_init(&player->dmx, player->io_ctx, player->log);
	if(err != EOS_ERROR_OK)
	{
		UTIL_LOGE(player->log, "DMX init failed!");
		cron_plyr_fire_err_ev(player, CRON_PLYR_ERR_GEN);
		return NULL;
	}
	osi_time_get_timestamp(&start);
	err = libav_dmx_probe(&player->dmx, &player->avf_ctx, CRON_PLYR_PROBE_SZ);
	if(err != EOS_ERROR_OK)
	{
		libav_dmx_deinit(&player->dmx);
		UTIL_LOGE(player->log, "DMX probe failed!");
		cron_plyr_fire_err_ev(player, CRON_PLYR_ERR_DMX);
		return NULL;
	}
	osi_time_get_timestamp(&end);
	osi_time_diff(&start, &end, &diff);
	CRON_PLYR_LOCK(player);
	player->probe_done = true;
	CRON_PLYR_UNLOCK(player);
	UTIL_LOGD(player->log, "Probing done (max: %d) %ds %dns",
			player->probe_len, diff.sec, diff.nsec);
	dmx_cb.handle_a = cron_plyr_dmx_aud;
	dmx_cb.handle_v = cron_plyr_dmx_vid;
	dmx_cb.opaque = player;
	libav_dmx_setup(&player->dmx, &dmx_cb);

	dec_cb.handle_a = cron_plyr_dec_aud;
	dec_cb.handle_v = cron_plyr_dec_vid;
	dec_cb.key_frm = cron_plyr_i_frm;
	dec_cb.opaque = player;
	libav_dec_init(&player->dec, player->aud_queue, player->vid_queue,
			player->log);
	libav_dec_setup(&player->dec, &dec_cb);

	player->sync.a_pts = 0.0;
	player->sync.v_pts = 0.0;
	player->sync.t_base = 0.0;
	player->sync.a_start = 0.0;
	player->sync.v_start = 0.0;
	player->sync.v_clk_tmt = 0;
	osi_mutex_create(&player->sync.lock);

	libav_dmx_start(&player->dmx);

	player->stopped = false;
	cron_plyr_es_start(player, CRON_PLYR_ES_AUD);
	cron_plyr_es_start(player, CRON_PLYR_ES_VID);

	return NULL;
}

static eos_error_t cron_plyr_dmx_aud(void* opaque, AVPacket* pkt)
{
	cron_plyr_t* player = (cron_plyr_t*)opaque;
	AVPacket *packet = NULL;
	eos_error_t err = EOS_ERROR_OK;

	if(cron_plyr_get_sel(player, CRON_PLYR_ES_AUD) == NULL)
	{
		UTIL_LOGD(player->log, "Ignoring AUD packet: ID invalidated");
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55,28,1)
		av_packet_unref(pkt);
#else
		av_free_packet(pkt);
#endif
		return EOS_ERROR_OK;
	}
	packet = osi_calloc(sizeof(AVPacket));
	*packet = *pkt;
	err = util_msgq_put(player->aud_queue, packet, sizeof(AVPacket), NULL);
	if(err != EOS_ERROR_OK)
	{
		osi_free((void**)&packet);
	}

	return err;
}

static eos_error_t cron_plyr_dmx_vid(void* opaque, AVPacket* pkt)
{
	cron_plyr_t* player = (cron_plyr_t*)opaque;
	AVPacket *packet = NULL;
	eos_error_t err = EOS_ERROR_OK;

	if(cron_plyr_get_sel(player, CRON_PLYR_ES_VID) == NULL)
	{
		UTIL_LOGD(player->log, "Ignoring VID packet: ID invalidated");
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55,28,1)
		av_packet_unref(pkt);
#else
		av_free_packet(pkt);
#endif
		return EOS_ERROR_OK;
	}
	packet = osi_calloc(sizeof(AVPacket));
	*packet = *pkt;
	err = util_msgq_put(player->vid_queue, packet, sizeof(AVPacket), NULL);
	if(err != EOS_ERROR_OK)
	{
		osi_free((void**)&packet);
	}

	return err;
}

/*
 * TODO: SYNC!!!
 */

static eos_error_t cron_plyr_dec_aud(void* opaque, AVFrame* frame)
{
	cron_plyr_t* player = (cron_plyr_t*)opaque;

	osi_mutex_lock(player->sync.lock);
	player->sync.a_pts = frame->pts * player->sync.t_base;
	osi_mutex_unlock(player->sync.lock);

	return alsa_aren_play(&player->aren, frame);
}

static eos_error_t cron_plyr_clk_vid(void* opaque, x11_frame_t* frame,
		uint32_t* micros, osi_time_t* offset)
{
	cron_plyr_t* player = (cron_plyr_t*)opaque;
	double aud_buff = 0.0, delta = 0.0;
	uint32_t s = 0;
	uint32_t micro_offset = 0;

	osi_mutex_lock(player->sync.lock);
	if(cron_plyr_get_sel(player, CRON_PLYR_ES_AUD) != NULL)
	{
		if(alsa_aren_get_buff_dur(&player->aren, &aud_buff)
				!= EOS_ERROR_OK)
		{
			delta = player->sync.v_clk_tmt;
		}
		else
		{
			player->sync.v_pts = frame->pts * player->sync.t_base;
			delta = player->sync.a_pts - aud_buff;
			delta = player->sync.v_pts - delta;
		}
	}
	else
	{
		delta = player->sync.v_pts;
		player->sync.v_pts = frame->pts * player->sync.t_base;
		delta = player->sync.v_pts - delta;

		osi_mutex_unlock(player->sync.lock);

		s = (uint32_t)OSI_TIME_SEC_TO_USEC(delta);
		OSI_TIME_CONVERT_TO_USEC((*offset), micro_offset);
		if(micro_offset > s)
		{
			UTIL_LOGWTF(player->log, "We are too slow in scaling: "
					"sleep %u : offset %u", s, micro_offset);
			s = 0;
		}
		else
		{
			s -= micro_offset;
		}
		*micros = s;

		return EOS_ERROR_OK;

	}
	osi_mutex_unlock(player->sync.lock);

	/* video in front, v pts is smaller */
	if(delta > 0)
	{
		s = (uint32_t)OSI_TIME_SEC_TO_USEC(delta);
		if(s > player->sync.threshold)
		{
			s = player->sync.threshold;
		}
	}
	/* audio in front, v pts is bigger */
	else
	{
		s = 0;
	}

	*micros = s;

	return EOS_ERROR_OK;
}

static eos_error_t cron_plyr_dec_vid(void* opaque, AVFrame* frame)
{
	cron_plyr_t* player = (cron_plyr_t*)opaque;
	x11_frame_t* x11_frm = NULL;
	eos_error_t err = EOS_ERROR_OK;

	x11_vren_frame_alloc(frame, &x11_frm);
	err = x11_vren_queue(&player->vren, x11_frm);

	if(err != EOS_ERROR_OK)
	{
		x11_vren_frame_free(&x11_frm);
	}
	return err;
}

static void cron_plyr_i_frm(void* opaque, uint64_t pts)
{
	cron_plyr_t* player = (cron_plyr_t*)opaque;
	cron_ply_ev_data_t data;

	data.first_frm.pts = pts;
	cron_plyr_fire_ev(player, CRON_PLYR_EV_FRM, &data);
}

static void cron_plyr_libav_log(void* ptr, int level, const char* fmt,
		va_list vl)
{
#ifdef CRON_PLYR_LIBAV_DBG
	static char avlog_line[1024] = "";
	EOS_UNUSED(ptr);
	EOS_UNUSED(level);
	vsnprintf(avlog_line, 1024,
			fmt, vl);
	avlog_line[strlen(avlog_line) - 1] = '\r';
	UTIL_GLOGD(avlog_line);
#else
	EOS_UNUSED(fmt);
	EOS_UNUSED(vl);
	EOS_UNUSED(ptr);
	EOS_UNUSED(level);
#endif
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

static inline eos_error_t cron_plyr_fire_err_ev(cron_plyr_t* player,
		cron_plyr_err_t event)
{
	cron_ply_ev_data_t ev_data;

	ev_data.err.code = event;

	return cron_plyr_fire_ev(player, CRON_PLYR_EV_ERR, &ev_data);
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

static inline eos_media_es_t* cron_plyr_get_sel(cron_plyr_t* player,
		cron_plyr_es_t es)
{
	uint8_t i = 0;
	eos_media_es_t *check = NULL;
	cron_plyr_es_t check_es = CRON_PLYR_ES_NONE;

	for(i=0; i<player->media.es_cnt; i++)
	{
		check = &(player->media.es[i]);
		check_es = cron_plyr_conv_es(check);
		if(check->selected == true && es == check_es)
		{
			return check;
		}
	}

	return NULL;
}
