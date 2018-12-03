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


#ifndef LIBAV_DEC_H_
#define LIBAV_DEC_H_

#include "osi_thread.h"
#include "util_msgq.h"
#include "util_log.h"
#include "eos_types.h"

#include <libavformat/avformat.h>

typedef struct libav_dec_frame_cb
{
	eos_error_t (*handle_a)(void* opaque, AVFrame* a_frame);
	eos_error_t (*handle_v)(void* opaque, AVFrame* v_frame);
	void (*key_frm)(void* opaque, uint64_t pts);
	void *opaque;
} libav_dec_frame_cb_t;

typedef struct libav_dec
{
	osi_thread_t *a_thread;
	osi_thread_t *v_thread;
	AVCodecContext *v_codec_ctx;
	AVCodecContext *a_codec_ctx;
	AVCodec *a_codec;
	AVCodec *v_codec;
	AVFrame *a_frame;
	AVFrame *v_frame;
	libav_dec_frame_cb_t cb;
	util_msgq_t *aud_queue;
	util_msgq_t *vid_queue;
	bool a_finish;
	bool v_finish;
	util_log_t *log;
} libav_dec_t;

eos_error_t libav_dec_init(libav_dec_t* dec, util_msgq_t* aqueue,
		util_msgq_t* vqueue, util_log_t *log);
eos_error_t libav_dec_setup(libav_dec_t* dec, libav_dec_frame_cb_t* cb);
eos_error_t libav_dec_start_aud(libav_dec_t* dec, AVCodecContext* a_codec_ctx);
eos_error_t libav_dec_start_vid(libav_dec_t* dec, AVCodecContext* v_codec_ctx);
eos_error_t libav_dec_stop_aud(libav_dec_t* dec);
eos_error_t libav_dec_stop_vid(libav_dec_t* dec);
eos_error_t libav_dec_pause_aud(libav_dec_t* dec);
eos_error_t libav_dec_pause_vid(libav_dec_t* dec);
eos_error_t libav_dec_resume_aud(libav_dec_t* dec);
eos_error_t libav_dec_resume_vid(libav_dec_t* dec);
eos_error_t libav_dec_flush_aud(libav_dec_t* dec);
eos_error_t libav_dec_flush_vid(libav_dec_t* dec);

#endif /* LIBAV_DEC_H_ */
