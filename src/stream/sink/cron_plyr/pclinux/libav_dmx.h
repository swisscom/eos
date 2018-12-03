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


#ifndef LIBAV_DMX_H_
#define LIBAV_DMX_H_

#include "eos_error.h"
#include "osi_thread.h"
#include "osi_mutex.h"
#include "util_log.h"
#include "eos_types.h"
#include "osi_bin_sem.h"

#include <libavformat/avformat.h>

#define LIBAV_DMX_IDX_NA (-1)

typedef eos_error_t (*handle_dmx_data_t)(void* opaque, AVPacket* a_pkt);

typedef struct libav_dmx_frame_cb
{
	handle_dmx_data_t handle_a;
	handle_dmx_data_t handle_v;
	void *opaque;
} libav_dmx_pkt_cb_t;

typedef struct libav_dmx
{
	osi_thread_t *t;
	AVPacket packet;
	libav_dmx_pkt_cb_t cb;
	AVFormatContext *avf_ctx;
	AVIOContext *io_ctx;
	int32_t a_idx;
	int32_t v_idx;
	bool finish;
	bool inited;
	bool paused;
	osi_bin_sem_t *pause_sem;
	osi_mutex_t *lock;
	util_log_t *log;
} libav_dmx_t;

eos_error_t libav_dmx_init(libav_dmx_t* dmx, AVIOContext* io_ctx, util_log_t* log);
eos_error_t libav_dmx_probe(libav_dmx_t* dmx, AVFormatContext** avf_ctx, uint32_t max_size);
eos_error_t libav_dmx_setup(libav_dmx_t* dmx, libav_dmx_pkt_cb_t* cb);
eos_error_t libav_dmx_set_aud(libav_dmx_t* dmx, int32_t aud_idx);
eos_error_t libav_dmx_set_vid(libav_dmx_t* dmx, int32_t vid_idx);

eos_error_t libav_dmx_start(libav_dmx_t* dmx);
eos_error_t libav_dmx_stop(libav_dmx_t* dmx);
eos_error_t libav_dmx_pause(libav_dmx_t* dmx);
eos_error_t libav_dmx_resume(libav_dmx_t* dmx);
eos_error_t libav_dmx_deinit(libav_dmx_t* dmx);

#endif /* LIBAV_DMX_H_ */
