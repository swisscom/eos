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


#include "libav_dec.h"
#include "osi_memory.h"
#include "eos_types.h"


#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55,28,1)
#include <libavutil/frame.h>
#endif

#define MODULE_NAME ("libav:dec")
#include "util_log.h"

#define LIBAVDEC_FREE_PKT(pkt) \
	do \
	{ \
		av_packet_unref(pkt); \
		osi_free((void**)&pkt); \
	} while(0);

static void* a_dec_thread(void* arg);
static void* v_dec_thread(void* arg);


eos_error_t libav_dec_init(libav_dec_t* dec, util_msgq_t* aqueue,
		util_msgq_t* vqueue, util_log_t *log)
{
	if(dec == NULL || aqueue == NULL || vqueue == NULL || log == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	osi_memset(dec, 0, sizeof(libav_dec_t));
	dec->aud_queue = aqueue;
	dec->vid_queue = vqueue;
	dec->log = log;

	return EOS_ERROR_OK;
}


eos_error_t libav_dec_setup(libav_dec_t* dec, libav_dec_frame_cb_t* cb)
{
	if(dec == NULL || cb == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	dec->cb = *cb;

	return EOS_ERROR_OK;
}

eos_error_t libav_dec_start_aud(libav_dec_t* dec, AVCodecContext* a_codec_ctx)
{
	if(dec == NULL || a_codec_ctx == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	dec->a_codec = avcodec_find_decoder(a_codec_ctx->codec_id);
	if(dec->a_codec == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if(avcodec_open2(a_codec_ctx, dec->a_codec, NULL) < 0)
	{
		return EOS_ERROR_GENERAL;
	}
	dec->a_codec_ctx = a_codec_ctx;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55,28,1)
	dec->a_frame = av_frame_alloc();
#else
	dec->a_frame = avcodec_alloc_frame();
#endif
	dec->a_finish = 0;

	return osi_thread_create(&dec->a_thread, NULL, a_dec_thread, dec);
}

eos_error_t libav_dec_start_vid(libav_dec_t* dec, AVCodecContext* v_codec_ctx)
{
	if(dec == NULL || v_codec_ctx == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	dec->v_codec = avcodec_find_decoder(v_codec_ctx->codec_id);
	if(dec->v_codec == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if(avcodec_open2(v_codec_ctx, dec->v_codec, NULL) < 0)
	{
		return EOS_ERROR_GENERAL;
	}
	dec->v_codec_ctx = v_codec_ctx;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55,28,1)
	dec->v_frame = av_frame_alloc();
#else
	dec->v_frame = avcodec_alloc_frame();
#endif
	dec->v_finish = 0;

	return osi_thread_create(&dec->v_thread, NULL, v_dec_thread, dec);
}

eos_error_t libav_dec_stop_aud(libav_dec_t* dec)
{
	eos_error_t err = EOS_ERROR_OK;

	if(dec == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if(dec->a_thread == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	dec->a_finish = 1;
	err = osi_thread_join(dec->a_thread, NULL);
	if(err != EOS_ERROR_OK)
	{
		UTIL_LOGE(dec->log, "AUD thread join failed?!?");
	}
	osi_thread_release(&dec->a_thread);
	avcodec_close(dec->a_codec_ctx);
	dec->a_codec_ctx = NULL;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55,28,1)
	av_frame_free(&dec->a_frame);
#else
	avcodec_free_frame(&dec->a_frame);
#endif

	dec->a_frame = NULL;

	return EOS_ERROR_OK;
}

eos_error_t libav_dec_stop_vid(libav_dec_t* dec)
{
	eos_error_t err = EOS_ERROR_OK;

	if(dec == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if(dec->v_thread == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	dec->v_finish = 1;
	err = osi_thread_join(dec->v_thread, NULL);
	if(err != EOS_ERROR_OK)
	{
		UTIL_LOGE(dec->log, "VID thread join failed?!?");
	}
	osi_thread_release(&dec->v_thread);
	avcodec_close(dec->v_codec_ctx);
	dec->v_codec_ctx = NULL;
	if(dec->v_frame)
	{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55,28,1)
		av_frame_free(&dec->v_frame);
#else
		avcodec_free_frame(&dec->v_frame);
#endif
		dec->v_frame = NULL;
	}

	return EOS_ERROR_OK;
}

eos_error_t libav_dec_pause_aud(libav_dec_t* dec)
{
	eos_error_t err = EOS_ERROR_OK;

	if(dec == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if(dec->a_thread == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	util_msgq_pause(dec->aud_queue);
	err = osi_thread_join(dec->a_thread, NULL);
	if(err != EOS_ERROR_OK)
	{
		UTIL_LOGE(dec->log, "AUD thread join failed?!?");
	}
	osi_thread_release(&dec->a_thread);
	util_msgq_resume(dec->aud_queue);
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55,28,1)
	av_frame_free(&dec->a_frame);
#else
	avcodec_free_frame(&dec->a_frame);
#endif

	dec->a_frame = NULL;

	return EOS_ERROR_OK;
}

eos_error_t libav_dec_pause_vid(libav_dec_t* dec)
{
	eos_error_t err = EOS_ERROR_OK;

	if(dec == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if(dec->v_thread == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	util_msgq_pause(dec->vid_queue);
	err = osi_thread_join(dec->v_thread, NULL);
	if(err != EOS_ERROR_OK)
	{
		UTIL_LOGE(dec->log, "VID thread join failed?!?");
	}
	osi_thread_release(&dec->v_thread);
	util_msgq_resume(dec->vid_queue);
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55,28,1)
	av_frame_free(&dec->v_frame);
#else
	avcodec_free_frame(&dec->v_frame);
#endif

	dec->v_frame = NULL;

	return EOS_ERROR_OK;
}

eos_error_t libav_dec_resume_aud(libav_dec_t* dec)
{
	if(dec == NULL || dec->a_codec_ctx == NULL)
	{
		return EOS_ERROR_INVAL;
	}
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55,28,1)
	dec->a_frame = av_frame_alloc();
#else
	dec->a_frame = avcodec_alloc_frame();
#endif
	dec->a_finish = 0;

	return osi_thread_create(&dec->a_thread, NULL, a_dec_thread, dec);
}

eos_error_t libav_dec_resume_vid(libav_dec_t* dec)
{
	if(dec == NULL || dec->v_codec_ctx == NULL)
	{
		return EOS_ERROR_INVAL;
	}
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55,28,1)
	dec->v_frame = av_frame_alloc();
#else
	dec->v_frame = avcodec_alloc_frame();
#endif
	dec->v_finish = 0;

	return osi_thread_create(&dec->v_thread, NULL, v_dec_thread, dec);
}

eos_error_t libav_dec_flush_aud(libav_dec_t* dec)
{
	eos_error_t err = EOS_ERROR_OK;

	if(dec == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if(dec->a_codec_ctx != NULL)
	{
		avcodec_flush_buffers(dec->a_codec_ctx);
	}

	return err;

}

eos_error_t libav_dec_flush_vid(libav_dec_t* dec)
{
	eos_error_t err = EOS_ERROR_OK;

	if(dec == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if(dec->v_codec_ctx != NULL)
	{
		avcodec_flush_buffers(dec->v_codec_ctx);
	}

	return err;
}

void* a_dec_thread(void* arg)
{
	libav_dec_t *dec = (libav_dec_t*) arg;
	AVPacket *packet = NULL;
	size_t sz = 0;
	int got_frame = 0, ret = 0;
	char errbuf[256];
	int64_t pts = 0LL;

	UTIL_LOGI(dec->log, "Audio DEC thread started");
	while(util_msgq_get(dec->aud_queue, (void**)&packet, &sz, NULL) == EOS_ERROR_OK)
	{
		if(packet == NULL)
		{
			UTIL_LOGI(dec->log, "NULL PKT received - exit audio DEC thread");
			break;
		}
		if(dec->a_finish == 1)
		{
			LIBAVDEC_FREE_PKT(packet);
			break;
		}
		if(pts == 0LL)
		{
			UTIL_LOGW(dec->log, "First AUD pkt %lld", packet->pts);
		}
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,48,101)
		ret = avcodec_send_packet(dec->a_codec_ctx, packet);
		if(ret < 0)
		{
			av_strerror(ret, errbuf, 256);
			UTIL_LOGW(dec->log, "Error decoding audio (%s)", errbuf);
			LIBAVDEC_FREE_PKT(packet);
			continue;
		}
		ret = avcodec_receive_frame(dec->a_codec_ctx, dec->a_frame);
		if(ret == 0)
		{
			got_frame = true;
		}
#else
		ret = avcodec_decode_audio4(dec->a_codec_ctx, dec->a_frame, &got_frame, packet);
		if(ret < 0)
		{
			av_strerror(ret, errbuf, 256);
			UTIL_LOGW(dec->log, "Error decoding audio (%s)", errbuf);
			LIBAVDEC_FREE_PKT(packet);
			continue;
		}
#endif
		if(got_frame)
		{
			dec->cb.handle_a(dec->cb.opaque, dec->a_frame);
		}
		pts = packet->pts;
		LIBAVDEC_FREE_PKT(packet);
	}
	UTIL_LOGI(dec->log, "Audio DEC thread exited");

	return NULL;
}

void* v_dec_thread(void* arg)
{
	libav_dec_t* dec = (libav_dec_t*) arg;
	AVPacket *packet = NULL;
	int got_frame = 0, ret = 0;
	size_t sz = 0;
	uint8_t frst_key_frm = false;
	char errbuf[256];
	uint32_t skipped = 0;

	UTIL_LOGI(dec->log, "Video DEC thread started");
	while(util_msgq_get(dec->vid_queue, (void**)&packet, &sz, NULL) == EOS_ERROR_OK)
	{
		if(packet == NULL)
		{
			UTIL_LOGI(dec->log, "NULL PKT received - exit video DEC thread");
			break;
		}
		if(dec->v_finish == 1)
		{
			LIBAVDEC_FREE_PKT(packet);
			break;
		}
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57,48,101)
		ret = avcodec_send_packet(dec->v_codec_ctx, packet);
		if(ret < 0)
		{
			av_strerror(ret, errbuf, 256);
			UTIL_LOGW(dec->log, "Error decoding video (%s)", errbuf);
			LIBAVDEC_FREE_PKT(packet);
			continue;
		}
		ret = avcodec_receive_frame(dec->v_codec_ctx, dec->v_frame);
		if(ret == 0)
		{
			got_frame = true;
		}
#else
		ret = avcodec_decode_video2(dec->v_codec_ctx, dec->v_frame, &got_frame, packet);
		if(ret < 0)
		{
			av_strerror(ret, errbuf, 256);
			UTIL_LOGW(dec->log, "Error decoding video (%s)", errbuf);
			LIBAVDEC_FREE_PKT(packet);
			continue;
		}
#endif
		if(got_frame)
		{
			if(frst_key_frm == false && dec->v_frame->key_frame == 0)
			{
				skipped++;
				LIBAVDEC_FREE_PKT(packet);
				continue;
			}
			if(frst_key_frm == false)
			{
				UTIL_LOGI(dec->log, "Found first I-frame (skipped %d)", skipped);
				frst_key_frm = true;
			}
			if(dec->cb.handle_v(dec->cb.opaque, dec->v_frame) != EOS_ERROR_OK)
			{
				LIBAVDEC_FREE_PKT(packet);
				UTIL_LOGW(dec->log, "Video callback returned error -> abandon");

				break;
			}
			if(dec->v_frame->key_frame == 1)
			{
				if(dec->cb.key_frm != NULL)
				{
					dec->cb.key_frm(dec->cb.opaque, dec->v_frame->pts);
				}
			}
		}

		LIBAVDEC_FREE_PKT(packet);
	}
	UTIL_LOGI(dec->log, "Video DEC thread exited");

	return NULL;
}
