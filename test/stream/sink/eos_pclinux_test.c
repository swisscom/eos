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


#include "sink_factory.h"
#include "lynx.h"
#include "fsi_file.h"
#include "osi_thread.h"
#include "eos_types.h"
#include "eos_media.h"

#define MODULE_NAME ("pclinux_test")
#include "util_log.h"

#include <stdlib.h>

#define TEST_READ_SZ (188 * 44)

fsi_file_t *file = NULL;

eos_media_es_attr_t lala = {{0, {0, 0}}};

eos_media_desc_t media_desc =
{
	.container = EOS_MEDIA_CONT_MPEGTS,
	.es_cnt = 4,
	.es[0] = {101, EOS_MEDIA_CODEC_H264, "", {{0, {0, 0}}}, true},
	.es[1] = {301, EOS_MEDIA_CODEC_AAC, "ger", {{0, {0, 0}}}, true},
	.es[2] = {302, EOS_MEDIA_CODEC_AAC, "aub", {{0, {0, 0}}}, false},
	.es[3] = {277, EOS_MEDIA_CODEC_TTXT, "ger", {{0, {0, 0}}}, false}
};

void err_exit(int err, int line)
{
	UTIL_GLOGE("err: %d [line: %d]\n", err, line);
	exit(err);
}

void* test_thread(void* arg)
{
	link_io_t *plug = (link_io_t*) arg;
	uint8_t *buff = NULL;
	size_t rd = TEST_READ_SZ;
	eos_error_t ret = EOS_ERROR_OK;

	while(plug->allocate(plug->handle, &buff, &rd, NULL, -1, 0) ==
			EOS_ERROR_OK)
	{
		ret = fsi_file_read(file, buff, &rd);
		if(ret != EOS_ERROR_OK && ret != EOS_ERROR_EOF)
		{
			break;
		}
		if(plug->commit(plug->handle, &buff, rd, NULL, -1, 0) != EOS_ERROR_OK)
		{
			break;
		}
		if(ret == EOS_ERROR_EOF)
		{
			break;
		}
	}
	UTIL_GLOGI("Exiting read thread");

	return NULL;
}

int main(int argc, char** argv)
{
	sink_t *sink;
	link_cap_t caps = LINK_CAP_SINK;
	link_io_t *plug;
	osi_thread_t *t = NULL;
	link_io_type_t input_type = LINK_IO_TYPE_SPROG_TS;
	link_cap_stream_sel_ctrl_t *sel;
	link_cap_av_out_set_ctrl_t *av_ctrl;

	/* kill warning */
	if(argc == 1 || argv == NULL)
	{
		UTIL_GLOGE("Please provide MPEG TS file path!");
		return -1;
	}
	if(sink_factory_manufacture(0, &media_desc, caps, input_type, &sink) != EOS_ERROR_OK)
	{
		err_exit(-1, __LINE__);
	}
	if(sink_factory_manufacture(0, &media_desc, caps, input_type, &sink) == EOS_ERROR_OK)
	{
		err_exit(-1, __LINE__);
	}
	if((sink->caps & LINK_CAP_STREAM_SEL) == 0)
	{
		UTIL_GLOGE("Sink has no stream select cap!");
		err_exit(-2, __LINE__);
	}
	if(sink->command.get_ctrl_funcs(sink, LINK_CAP_STREAM_SEL, (void**)&sel) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Get control callbacks failed");
		err_exit(-3, __LINE__);
	}
	if(sink->command.get_ctrl_funcs(sink, LINK_CAP_AV_OUT_SET, (void**)&av_ctrl) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Get control callbacks failed");
		err_exit(-3, __LINE__);
	}
	if(fsi_file_open(&file, argv[1], F_F_RO, 0) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Failure opening %s!", argv[1]);
		err_exit(-2, __LINE__);
	}
	plug = &sink->plug;
	if(osi_thread_create(&t, NULL, test_thread, plug) != EOS_ERROR_OK)
	{
		err_exit(-3, __LINE__);
	}
	if(sel->select(sink, 1) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Audio select failed");
		err_exit(-3, __LINE__);
	}
	if(sel->select(sink, 0) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Video select failed");
		err_exit(-3, __LINE__);
	}
	sink->command.start(sink);
	osi_time_usleep(15000000);
	if(sel->deselect(sink, 1) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Audio deselect failed");
		err_exit(-3, __LINE__);
	}
	osi_time_usleep(3000000);
	if(sel->select(sink, 2) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Audio select failed");
		err_exit(-3, __LINE__);
	}
	osi_time_usleep(3000000);
	if(sel->deselect(sink, 0) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Video deselect failed");
		err_exit(-3, __LINE__);
	}
	osi_time_usleep(3000000);
	if(sel->select(sink, 0) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Video select failed");
		err_exit(-3, __LINE__);
	}
	if(av_ctrl->vid_scale(sink, 1280, 720) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Video scale failed");
		err_exit(-3, __LINE__);
	}
	osi_time_usleep(3000000);
	if(av_ctrl->vid_move(sink, 320, 320) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Video scale failed");
		err_exit(-3, __LINE__);
	}
	sink->command.pause(sink, false);
	osi_time_usleep(6000000);
	sink->command.flush_buffs(sink);
	sink->command.pause(sink, false);

	if(osi_thread_join(t, NULL) != EOS_ERROR_OK)
	{
		err_exit(-4, __LINE__);
	}
	if(osi_thread_release(&t) != EOS_ERROR_OK)
	{
		err_exit(-5, __LINE__);
	}
	sink->command.stop(sink);
	fsi_file_close(&file);
	if(sink_factory_dismantle(&sink) != EOS_ERROR_OK)
	{
		err_exit(-1, __LINE__);
	}

	return 0;
}

