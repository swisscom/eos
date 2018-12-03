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


#ifndef X11_VREN_H_
#define X11_VREN_H_

#include "eos_error.h"
#include "util_log.h"
#include "util_msgq.h"
#include "osi_mutex.h"
#include "osi_thread.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xdbe.h>

#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

typedef struct x11_frame
{
    uint8_t *data[AV_NUM_DATA_POINTERS];
    int linesize[AV_NUM_DATA_POINTERS];
    int64_t pts;
} x11_frame_t;

typedef eos_error_t (*x11_clk_cbk_t)(void* opaque, x11_frame_t* frame, uint32_t* micros, osi_time_t* offset);

typedef struct x11_vren
{
	struct SwsContext *sws_ctx;
	XImage *ximg;
	Window win;
	GC gc;
	XdbeBackBuffer db;
	uint16_t src_w;
	uint16_t src_h;
	uint16_t dst_w;
	uint16_t dst_h;
	uint8_t fps;
	osi_mutex_t *lock;
	util_log_t *log;
	util_msgq_t *queue;
	osi_thread_t *thread;
	x11_clk_cbk_t clk_cbk;
	void *clk_opaque;
	enum AVPixelFormat pix_fmt;
} x11_vren_t;

eos_error_t x11_vren_sys_init(void);
eos_error_t x11_vren_sys_deinit(void);

eos_error_t x11_vren_init(x11_vren_t* vren, uint16_t w, uint16_t h, uint16_t x, uint16_t y,
		enum AVPixelFormat pix_fmt, util_log_t* log, uint16_t src_h, uint16_t src_w);
eos_error_t x11_vren_set_clk_cb(x11_vren_t* vren, x11_clk_cbk_t clk_cbk,
		void *cookie);
eos_error_t x11_vren_frame_alloc(AVFrame *av, x11_frame_t** frame);
eos_error_t x11_vren_frame_free(x11_frame_t** frame);
eos_error_t x11_vren_queue(x11_vren_t* vren, x11_frame_t* frame);
eos_error_t x11_vren_pause(x11_vren_t* vren);
eos_error_t x11_vren_resume(x11_vren_t* vren);
eos_error_t x11_vren_frm_cnt(x11_vren_t* vren, uint32_t* count);
eos_error_t x11_vren_move(x11_vren_t* vren, uint16_t x, uint16_t y);
eos_error_t x11_vren_scale(x11_vren_t* vren, uint16_t w, uint16_t h);
eos_error_t x11_vren_deinit(x11_vren_t* vren);

#endif /* X11_VREN_H_ */
