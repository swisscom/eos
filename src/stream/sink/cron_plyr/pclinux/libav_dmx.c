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


#include "libav_dmx.h"
#include "osi_memory.h"
#include "osi_time.h"
#include "eos_types.h"

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME ("libav:dmx")

#define LIBAV_DMX_LOCK(dmx, err) \
		do \
		{ \
			if((err = osi_mutex_lock(dmx->lock)) != EOS_ERROR_OK) \
			{ \
				return err; \
			} \
		} while(0);

#define LIBAV_DMX_UNLOCK(dmx, err) \
		do \
		{ \
			if((err = osi_mutex_unlock(dmx->lock)) != EOS_ERROR_OK) \
			{ \
				UTIL_LOGW(dmx->log, "Error unlocking lock (%d)", err); \
				return err; \
			} \
		} while(0);

static eos_error_t libav_dmx_open_avf(libav_dmx_t* dmx);
static void* libav_dmx_thread(void* arg);
static eos_error_t libav_dmx_check_probe(AVFormatContext* avf_ctx);

eos_error_t libav_dmx_init(libav_dmx_t* dmx, AVIOContext* io_ctx, util_log_t* log)
{
	eos_error_t err = EOS_ERROR_OK;

	if(dmx == NULL || io_ctx == NULL || log == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	osi_memset(dmx, 0, sizeof(libav_dmx_t));
	if((err = osi_bin_sem_create(&dmx->pause_sem, false)) != EOS_ERROR_OK)
	{
		return err;
	}

	if((err = osi_mutex_create(&dmx->lock)) != EOS_ERROR_OK)
	{
		return err;
	}
	dmx->log = log;
	dmx->io_ctx = io_ctx;
	if((err = libav_dmx_open_avf(dmx)) != EOS_ERROR_OK)
	{
		return err;
	}
	dmx->a_idx = LIBAV_DMX_IDX_NA;
	dmx->v_idx = LIBAV_DMX_IDX_NA;
	dmx->inited = true;
	UTIL_LOGI(dmx->log, "DMX inited");

	return EOS_ERROR_OK;
}

eos_error_t libav_dmx_probe(libav_dmx_t* dmx, AVFormatContext** avf_ctx, uint32_t max_size)
{
	eos_error_t err = EOS_ERROR_OK;

	if(dmx == NULL || avf_ctx == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	*avf_ctx = NULL;
	dmx->avf_ctx->probesize = max_size;
	//dmx->avf_ctx->max_analyze_duration2 = max_size;
	dmx->avf_ctx->fps_probe_size = 2;
	if(avformat_find_stream_info(dmx->avf_ctx, NULL) < 0)
	{
		UTIL_LOGE(dmx->log, "avformat_find_stream_info failed!");
		return EOS_ERROR_GENERAL;
	}
	if((err = libav_dmx_check_probe(dmx->avf_ctx)) != EOS_ERROR_OK)
	{
		return err;
	}
	*avf_ctx = dmx->avf_ctx;

	return EOS_ERROR_OK;
}

eos_error_t libav_dmx_set_aud(libav_dmx_t* dmx, int32_t aud_idx)
{
	eos_error_t err = EOS_ERROR_OK;

	if(dmx == NULL || aud_idx < LIBAV_DMX_IDX_NA || dmx->inited == false)
	{
		return EOS_ERROR_INVAL;
	}
	LIBAV_DMX_LOCK(dmx, err);
	if(dmx->avf_ctx == NULL)
	{
		LIBAV_DMX_UNLOCK(dmx, err);
		return EOS_ERROR_PERM;
	}
	if(aud_idx >= (int32_t)dmx->avf_ctx->nb_streams)
	{
		LIBAV_DMX_UNLOCK(dmx, err);
		return EOS_ERROR_OVERFLOW;
	}
	dmx->a_idx = aud_idx;
	LIBAV_DMX_UNLOCK(dmx, err);

	return EOS_ERROR_OK;
}

eos_error_t libav_dmx_set_vid(libav_dmx_t* dmx, int32_t vid_idx)
{
	eos_error_t err = EOS_ERROR_OK;

	if(dmx == NULL || vid_idx < LIBAV_DMX_IDX_NA || dmx->inited == false)
	{
		return EOS_ERROR_INVAL;
	}
	LIBAV_DMX_LOCK(dmx, err);
	if(dmx->avf_ctx == NULL)
	{
		LIBAV_DMX_UNLOCK(dmx, err);
		return EOS_ERROR_PERM;
	}
	if(vid_idx >= (int32_t)dmx->avf_ctx->nb_streams)
	{
		LIBAV_DMX_UNLOCK(dmx, err);
		return EOS_ERROR_OVERFLOW;
	}
	dmx->v_idx = vid_idx;
	LIBAV_DMX_UNLOCK(dmx, err);

	return EOS_ERROR_OK;
}


eos_error_t libav_dmx_setup(libav_dmx_t* dmx, libav_dmx_pkt_cb_t* cb)
{
	eos_error_t err = EOS_ERROR_OK;

	if(dmx == NULL || cb == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	LIBAV_DMX_LOCK(dmx, err);
	if(dmx->cb.handle_a != NULL && dmx->cb.handle_v != NULL &&
			dmx->cb.opaque != NULL)
	{
		UTIL_LOGW(dmx->log, "Overwriting DMX callback...");
	}
	dmx->cb = *cb;
	LIBAV_DMX_UNLOCK(dmx, err);

	return EOS_ERROR_OK;
}

eos_error_t libav_dmx_start(libav_dmx_t* dmx)
{
	if(dmx == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	return osi_thread_create(&dmx->t, NULL, libav_dmx_thread, dmx);
}

eos_error_t libav_dmx_stop(libav_dmx_t* dmx)
{
	eos_error_t err = EOS_ERROR_OK;

	if(dmx == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	dmx->finish = 1;
	if(dmx->t != NULL)
	{
		err = osi_thread_join(dmx->t, NULL);
		if(err != EOS_ERROR_OK)
		{
			UTIL_LOGW(dmx->log, "DMX thread join failed (%d)", err);
		}
		err = osi_thread_release(&dmx->t);
	}

	return err;
}

eos_error_t libav_dmx_pause(libav_dmx_t* dmx)
{
	eos_error_t err = EOS_ERROR_OK;

	if(dmx == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_LOGD(dmx->log, "DMX Pausing...");
	dmx->paused = 1;
	osi_thread_join(dmx->t, NULL);
	osi_thread_release(&dmx->t);
	//avformat_close_input(&dmx->avf_ctx);
	UTIL_LOGD(dmx->log, "DMX Pausing done");

	return err;
}

eos_error_t libav_dmx_resume(libav_dmx_t* dmx)
{
	eos_error_t err = EOS_ERROR_OK;

	if(dmx == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	dmx->paused = 0;
	UTIL_LOGD(dmx->log, "DMX Resuming...");
	err = osi_bin_sem_give(dmx->pause_sem);
	if (err != EOS_ERROR_OK)
	{
		//TODO PRINT
	}
	libav_dmx_start(dmx);
	UTIL_LOGD(dmx->log, "DMX Resume done");

	return err;
}

eos_error_t libav_dmx_deinit(libav_dmx_t* dmx)
{
	if(dmx == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if(dmx->avf_ctx != NULL)
	{
		UTIL_LOGD(dmx->log, "DMX CLOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOSE");
		avformat_close_input(&dmx->avf_ctx);
	}
	if(dmx->lock != NULL)
	{
		osi_mutex_lock(dmx->lock);
		osi_mutex_unlock(dmx->lock);
		osi_mutex_destroy(&dmx->lock);
	}
	if (dmx->pause_sem != NULL)
	{
		osi_bin_sem_destroy(&dmx->pause_sem);
	}
	UTIL_LOGI(dmx->log, "DMX deinited");
	dmx->inited = false;

	return EOS_ERROR_OK;
}

static eos_error_t libav_dmx_open_avf(libav_dmx_t* dmx)
{
	AVInputFormat *input_format;
	AVDictionary *opt = NULL;

	dmx->avf_ctx = avformat_alloc_context();
	if(dmx->avf_ctx == NULL)
	{
		osi_mutex_destroy(&dmx->lock);
		return EOS_ERROR_NOMEM;
	}
	dmx->avf_ctx->flags = AVFMT_FLAG_CUSTOM_IO;
	dmx->avf_ctx->pb = dmx->io_ctx;
	input_format = av_find_input_format("mpegts");
	if(input_format == NULL)
	{
		UTIL_LOGE(dmx->log, "Could not find MPEG-TS input format");
		return EOS_ERROR_GENERAL;
	}
	input_format->flags = AVFMT_NO_BYTE_SEEK;
	if(avformat_open_input(&dmx->avf_ctx, "", input_format, &opt) < 0)
	{
		UTIL_LOGE(dmx->log, "Open input failed!");
		return EOS_ERROR_GENERAL;
	}

	return EOS_ERROR_OK;
}

void* libav_dmx_thread(void* arg)
{
	libav_dmx_t *dmx = (libav_dmx_t*) arg;
	int free_pkt = 0;
	eos_error_t err = EOS_ERROR_OK;
	handle_dmx_data_t func = NULL;

	UTIL_LOGI(dmx->log, "Starting DMX thread");
	osi_memset(&dmx->packet, 0, sizeof(AVPacket));
	while(av_read_frame(dmx->avf_ctx, &dmx->packet) == 0)
	{
		if((err = osi_mutex_lock(dmx->lock)) != EOS_ERROR_OK)
		{
			UTIL_LOGE(dmx->log, "Locking DMX failed: %d", err);
			return NULL;
		}
		if(dmx->paused)
		{
			UTIL_LOGI(dmx->log, "Pausing - DMX exit");
			osi_mutex_unlock(dmx->lock);
			free_pkt = 1;
			break;
		}
		if(dmx->finish)
		{
			osi_mutex_unlock(dmx->lock);
			free_pkt = 1;
			break;
		}
		if(dmx->packet.stream_index == dmx->a_idx)
		{
			func = dmx->cb.handle_a;
		}
		else if(dmx->packet.stream_index == dmx->v_idx)
		{
			func = dmx->cb.handle_v;
		}
		else
		{
			av_packet_unref(&dmx->packet);
			func = NULL;
		}
		if((err = osi_mutex_unlock(dmx->lock)) != EOS_ERROR_OK)
		{
			UTIL_LOGE(dmx->log, "Unlocking DMX failed: %d", err);
			return NULL;
		}
		if(func != NULL)
		{
			if((err = func(dmx->cb.opaque, &dmx->packet)) != EOS_ERROR_OK)
			{
				UTIL_LOGE(dmx->log, "DMX callback returned ERROR: %d", err);
				free_pkt = 1;
				break;
			}
		}
	}
	UTIL_LOGI(dmx->log, "Exiting DMX thread");
	if(free_pkt == 1)
	{
		av_packet_unref(&dmx->packet);
	}

	return NULL;
}

static eos_error_t libav_dmx_check_probe(AVFormatContext* avf_ctx)
{
	bool aud = false, vid = false;

	for(int i = 0; i < (int)avf_ctx->nb_streams; i++)
	{
		if(avf_ctx->streams[i]->codecpar->codec_type
				== AVMEDIA_TYPE_VIDEO)
		{
			vid = true;
			if(avf_ctx->streams[i]->codecpar->width <= 0 ||
					avf_ctx->streams[i]->codecpar->height <= 0)
			{
				return EOS_ERROR_INVAL;
			}
		}
		else if(avf_ctx->streams[i]->codecpar->codec_type
				== AVMEDIA_TYPE_AUDIO)
		{
			aud = true;
		}
	}
	if(!aud || !vid)
	{
		return EOS_ERROR_NFOUND;
	}

	return EOS_ERROR_OK;
}
