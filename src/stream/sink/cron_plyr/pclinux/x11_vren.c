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


#include "x11_vren.h"
#include "osi_memory.h"
#define MODULE_NAME ("x11")
#include "util_log.h"

#include <X11/Xutil.h>

#define X11_VREN_WIDTH  (1920)
#define X11_VREN_HEIGHT (1080)
#define X11_FRAME_CNT   (50)

#define X11_VREN_DEST_PIX_FMT (AV_PIX_FMT_RGB32)
#define X11_VREN_SWS_FLG (SWS_BICUBIC)

static Display *disp = NULL;
static Window root = 0;
static XWindowAttributes root_attr;


static XImage* get_ximg(Display* d, Visual* v, int width, int height);
static void set_fullscr(Window win);
static void frame_free_cbk(void* msg_data, size_t msg_size);
static void* render_thread(void* arg);

eos_error_t x11_vren_sys_init(void)
{
	if(XInitThreads() == 0)
	{
		UTIL_GLOGD("Threaded X not supported!!!");
		return EOS_ERROR_GENERAL;
	}
	disp = XOpenDisplay(NULL);
	if(disp == NULL)
	{
		return EOS_ERROR_GENERAL;
	}
	root = 	XCreateSimpleWindow(disp, XDefaultRootWindow(disp), 0, 0,
			X11_VREN_WIDTH, X11_VREN_HEIGHT, 0, WhitePixel(disp, 0),
			WhitePixel(disp, 0));
	set_fullscr(root);
	XMapWindow(disp, root);
	/*
	 * Force moving to (0,0) because of multi-head configurations
	 * (TODO: use XRRCrtcInfo to detect screen configuration)
	 */
	XMoveWindow(disp, root, 0, 0);
	XFlush(disp);

	XGetWindowAttributes(disp, root, &root_attr);
	UTIL_GLOGD("Created Root Window: %d", root);

	return EOS_ERROR_OK;
}

eos_error_t x11_vren_sys_deinit(void)
{
	XUnmapWindow(disp, root);
	XDestroyWindow(disp, root);
	XCloseDisplay(disp);
	UTIL_GLOGD("Root Window destroyed");

	return EOS_ERROR_OK;
}


eos_error_t x11_vren_init(x11_vren_t* vren, uint16_t w, uint16_t h, uint16_t x, uint16_t y,
		enum AVPixelFormat pix_fmt, util_log_t* log, uint16_t src_h, uint16_t src_w)
{

	eos_error_t err = EOS_ERROR_OK;

	if(disp == NULL)
	{
		UTIL_GLOGE("X Display not initialized!!!");
	}
	if(vren == NULL || log == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if(w == 0 || h == 0)
	{
		UTIL_GLOGWTF("Unknown window size!!!");
		return EOS_ERROR_INVAL;
	}
	osi_memset(vren, 0, sizeof(x11_vren_t));
	vren->src_w = src_w;
	vren->src_h = src_h;
	if(w > X11_VREN_WIDTH)
	{
		vren->dst_w = X11_VREN_WIDTH;
	}
	else
	{
		vren->dst_w = w;
	}
	if(h > X11_VREN_HEIGHT)
	{
		vren->dst_h = X11_VREN_HEIGHT;
	}
	else
	{
		vren->dst_h = h;
	}
	vren->pix_fmt = pix_fmt;

	if((err = osi_mutex_create(&vren->lock)) != EOS_ERROR_OK)
	{
		goto cleanup;
	}
	if((err = util_msgq_create(&vren->queue, X11_FRAME_CNT, frame_free_cbk))
			!= EOS_ERROR_OK)
	{
		goto cleanup;
	}
	if((err = osi_thread_create(&vren->thread, NULL, render_thread, vren))
			!= EOS_ERROR_OK)
	{
		goto cleanup;
	}
	vren->win = XCreateSimpleWindow(disp, root, x, y,
			vren->dst_w, vren->dst_h, 0, BlackPixel (disp, 0),
			/*BlackPixel(disp, 0)*/0);
	XMapWindow(disp, vren->win);
	vren->gc = XCreateGC(disp, vren->win, 0, NULL);
	if(vren->gc == NULL)
	{
		UTIL_LOGE(log, "Create GC failed!!!");
		err = EOS_ERROR_GENERAL;
		goto cleanup;
	}
	vren->db = XdbeAllocateBackBufferName(disp, vren->win, XdbeBackground);
	UTIL_GLOGD("Created Window: %d", vren->win);

	vren->ximg = get_ximg(disp, root_attr.visual, root_attr.width,
			root_attr.height);
	if(vren->ximg == NULL)
	{
		err = EOS_ERROR_NOMEM;
		goto cleanup;
	}
	vren->sws_ctx = sws_getCachedContext(NULL, vren->src_w, vren->src_h, pix_fmt,
			vren->dst_w, vren->dst_h, X11_VREN_DEST_PIX_FMT, X11_VREN_SWS_FLG,
			NULL, NULL, NULL);


	return EOS_ERROR_OK;

cleanup:
	if(vren->lock)
	{
		osi_mutex_destroy(&vren->lock);
	}
	if(vren->queue)
	{
		util_msgq_destroy(&vren->queue);
	}
	if(vren->gc)
	{
		XFreeGC(disp, vren->gc);
	}
	if(vren->ximg)
	{
		XDestroyImage(vren->ximg);
		XdbeDeallocateBackBufferName(disp, vren->db);
	}
	if(vren->win)
	{
		XUnmapWindow(disp, vren->win);
		XDestroyWindow(disp, vren->win);
	}
	if(vren->sws_ctx)
	{
		sws_freeContext(vren->sws_ctx);
	}

	return err;
}

eos_error_t x11_vren_set_clk_cb(x11_vren_t* vren, x11_clk_cbk_t clk_cbk,
		void *cookie)
{
	if(vren == NULL || clk_cbk == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	osi_mutex_lock(vren->lock);
	vren->clk_cbk = clk_cbk;
	vren->clk_opaque = cookie;
	osi_mutex_unlock(vren->lock);

	return EOS_ERROR_OK;
}

eos_error_t x11_vren_deinit(x11_vren_t* vren)
{
	if(vren == NULL || vren->queue == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	util_msgq_pause(vren->queue);
	osi_thread_join(vren->thread, NULL);
	osi_thread_release(&vren->thread);
	osi_mutex_lock(vren->lock);
	sws_freeContext(vren->sws_ctx);
	XPutImage(disp, XDefaultRootWindow(disp), vren->gc, vren->ximg, 0, 0, 0, 0,
			vren->ximg->width, vren->ximg->height);
	XDestroyImage(vren->ximg);
	XFreeGC(disp, vren->gc);
	XdbeDeallocateBackBufferName(disp, vren->db);
	XUnmapWindow(disp, vren->win);
	XDestroyWindow(disp, vren->win);
	util_msgq_destroy(&vren->queue);
	osi_mutex_unlock(vren->lock);
	osi_mutex_destroy(&vren->lock);

	return EOS_ERROR_OK;
}

eos_error_t x11_vren_pause(x11_vren_t* vren)
{
	eos_error_t err = EOS_ERROR_OK;

	util_msgq_pause(vren->queue);
	osi_thread_join(vren->thread, NULL);
	osi_thread_release(&vren->thread);
	util_msgq_resume(vren->queue);

	return err;
}

eos_error_t x11_vren_resume(x11_vren_t* vren)
{
	eos_error_t err = EOS_ERROR_OK;

	if((err = osi_thread_create(&vren->thread, NULL, render_thread, vren))
			!= EOS_ERROR_OK)
	{
		UTIL_LOGW(vren->log, "Thread create failed with %d", err);
	}

	return err;
}

eos_error_t x11_vren_queue(x11_vren_t* vren, x11_frame_t *frame)
{
	eos_error_t err = EOS_ERROR_OK;

	err = util_msgq_put(vren->queue, frame, sizeof(x11_frame_t), NULL);

	return err;
}

eos_error_t x11_vren_frm_cnt(x11_vren_t* vren, uint32_t* count)
{
	if(vren == NULL || count == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	return util_msgq_count(vren->queue, count);
}

eos_error_t x11_vren_prepare(x11_vren_t* vren, x11_frame_t *v_frame)
{
	if(vren == NULL || v_frame == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	sws_scale(vren->sws_ctx, (const uint8_t **)v_frame->data,
			v_frame->linesize, 0, vren->src_h,
			(uint8_t* const *)&vren->ximg->data, &vren->ximg->bytes_per_line);

	return EOS_ERROR_OK;
}

eos_error_t x11_vren_show(x11_vren_t* vren)
{
	XdbeSwapInfo swap_info[1] = {{vren->win, XdbeBackground}};
	int err = 0;

	XdbeBeginIdiom(disp);
	XPutImage(disp, vren->db, vren->gc, vren->ximg, 0, 0, 0, 0,
			vren->ximg->width, vren->ximg->height);
	XdbeEndIdiom(disp);
	if((err = XdbeSwapBuffers(disp, swap_info, 1)) == 0)
	{
		UTIL_GLOGE("Rendering failed with %d", err);
		return EOS_ERROR_GENERAL;
	}
	XFlush(disp);

	return EOS_ERROR_OK;
}

eos_error_t x11_vren_move(x11_vren_t* vren, uint16_t x, uint16_t y)
{
	if(vren == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	osi_mutex_lock(vren->lock);
	if(x > root_attr.width || y > root_attr.height)
	{
		UTIL_LOGW(vren->log, "Moving params wrong (X:%d Y:%d)", x, y);
	}
	XMoveWindow(disp, vren->win, x, y);
	osi_mutex_unlock(vren->lock);
	XFlush(disp);

	return EOS_ERROR_OK;
}

eos_error_t x11_vren_scale(x11_vren_t* vren,
		uint16_t w, uint16_t h)
{
	if(vren == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if(w > root_attr.width || h > root_attr.height)
	{
		UTIL_LOGE(vren->log, "Scaling params wrong (W:%d H:%d)", w, h);
		return EOS_ERROR_INVAL;
	}
	osi_mutex_lock(vren->lock);
	XResizeWindow(disp, vren->win, w, h);
	XFlush(disp);
	sws_freeContext(vren->sws_ctx);
	vren->dst_w = w;
	vren->dst_h = h;
	vren->sws_ctx = sws_getCachedContext(NULL, vren->src_w, vren->src_h, vren->pix_fmt,
			w, h, X11_VREN_DEST_PIX_FMT, X11_VREN_SWS_FLG,
			NULL, NULL, NULL);
	osi_mutex_unlock(vren->lock);

	return EOS_ERROR_OK;
}

static XImage* get_ximg(Display* d, Visual* v, int width, int height)
{
	XImage *ximg;

	ximg = XCreateImage(d, v, 24, ZPixmap, 0, NULL, width, height, 8, 0);
	ximg->data = osi_calloc(ximg->bytes_per_line * height);

	return ximg;
}

static void set_fullscr(Window win)
{
	XEvent xev;
	Atom wm_state;
	Atom fullscreen;

	osi_memset(&xev, 0, sizeof(xev));

    wm_state   = XInternAtom(disp, "_NET_WM_STATE", False);
    fullscreen = XInternAtom(disp, "_NET_WM_STATE_FULLSCREEN", False);
    xev.type = ClientMessage;
    xev.xclient.window = win;
    xev.xclient.message_type = wm_state;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 1;
    xev.xclient.data.l[1] = fullscreen;
    xev.xclient.data.l[2] = 0;
    XSendEvent(disp, win == root ? XDefaultRootWindow(disp) : root, False,
	   SubstructureRedirectMask | SubstructureNotifyMask,
       &xev);
    XFlush(disp);
}

static void frame_free_cbk(void* msg_data, size_t msg_size)
{
	x11_frame_t* frame = NULL;

	if(msg_data == NULL || msg_size != sizeof(x11_frame_t))
	{
		return;
	}
	frame = (x11_frame_t*) msg_data;
	x11_vren_frame_free(&frame);
}

static void* render_thread(void* arg)
{
	x11_vren_t *vren = (x11_vren_t*) arg;
	size_t sz = 0;
	x11_frame_t* frame = NULL;
	uint32_t micros = 0;
	osi_time_t start, end, offset;

	UTIL_LOGD(vren->log, "Renderer thread started");
	while(util_msgq_get(vren->queue, (void**)&frame, &sz, NULL) == EOS_ERROR_OK)
	{
		if(frame == NULL)
		{
			UTIL_LOGW(vren->log, "Ignoring NULL frame");
			continue;
		}
		osi_mutex_lock(vren->lock);
		if(osi_time_get_timestamp(&start) != EOS_ERROR_OK)
		{
			UTIL_LOGW(vren->log, "Start timestamp get failed!!");
		}
		if(x11_vren_prepare(vren, frame) != EOS_ERROR_OK)
		{
			UTIL_LOGW(vren->log, "VREN prepare failed!!!");
		}
		if(osi_time_get_timestamp(&end) != EOS_ERROR_OK)
		{
			UTIL_LOGW(vren->log, "End timestamp get failed!!");
		}
		osi_time_diff(&start, &end, &offset);
		vren->clk_cbk(vren->clk_opaque, frame, &micros, &offset);
		osi_time_usleep(micros);
		x11_vren_show(vren);
		x11_vren_frame_free(&frame);
		osi_mutex_unlock(vren->lock);
	}
	UTIL_LOGD(vren->log, "Renderer thread exited");

	return NULL;
}

eos_error_t x11_vren_frame_alloc(AVFrame *av, x11_frame_t** frame)
{
	x11_frame_t* tmp = NULL;
	uint8_t shift = 0;

	if(av == NULL || frame == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	tmp = osi_calloc(sizeof(x11_frame_t));
	if(tmp == NULL)
	{
		*frame = NULL;
		return EOS_ERROR_NOMEM;
	}
	tmp->pts = av->pts;
	for(int i=0; i<AV_NUM_DATA_POINTERS; i++)
	{
		if(av->data[i] == NULL)
		{
			break;
		}
		shift = i == 0 ? 0 : 1;
		tmp->data[i] = osi_calloc((av->linesize[i] * av->height >> shift) + 8);
		osi_memcpy(tmp->data[i], av->data[i], av->linesize[i] * av->height  >> shift);
		tmp->linesize[i] = av->linesize[i];
	}
	*frame = tmp;

	return EOS_ERROR_OK;
}


eos_error_t x11_vren_frame_free(x11_frame_t** frame)
{
	x11_frame_t* tmp = NULL;

	if(frame == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if(*frame == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	tmp = *frame;
	for(int i=0; i<AV_NUM_DATA_POINTERS; i++)
	{
		if(tmp->data[i] == NULL)
		{
			break;
		}
		osi_free((void**)&tmp->data[i]);
	}
	osi_free((void**)frame);

	return EOS_ERROR_OK;
}
