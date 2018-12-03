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


#define MODULE_NAME "source:test"

#include "source.h"
#include "source_factory.h"
#include "osi_time.h"
#include "osi_memory.h"
#include "lynx.h"
#include "eos_macro.h"
#include "eos_types.h"
#include "util_log.h"

//#define HC_SOURCE "~/workspace/streams/test_sd.ts"
//#define HC_SOURCE "/home/pajsh/workspace/streams/test_sd.ts"
#define HC_SOURCE "/home/maksovic/temp/test_sd.ts"

bool connected = false;


eos_error_t allocate (link_handle_t handle, uint8_t** buff, size_t* size,
		link_data_ext_info_t* ext_info, int32_t msec, uint16_t id)
{
	EOS_UNUSED(handle)
	EOS_UNUSED(buff)
	EOS_UNUSED(msec)
	EOS_UNUSED(id)
	EOS_UNUSED(ext_info)
//	source_t *source = handle;
	*buff = osi_calloc(*size);
	return EOS_ERROR_OK;
}

eos_error_t commit (link_handle_t handle, uint8_t** buff, size_t size,
		link_data_ext_info_t* ext_info, int32_t msec, uint16_t id)
{
	EOS_UNUSED(handle)
	EOS_UNUSED(size)
	EOS_UNUSED(msec)
	EOS_UNUSED(id)
	EOS_UNUSED(ext_info)
//	source_t *source = handle;
	osi_free((void**)buff);
	return EOS_ERROR_OK;
}

link_io_t lio = 
{
	.allocate = allocate,
	.commit = commit,
	.handle = NULL
};

void event_handler (link_ev_t event, link_ev_data_t* data,
		void* cookie, uint64_t chain_id)
{
	EOS_UNUSED(data)
	EOS_UNUSED(chain_id)

	source_t *source = cookie;
	switch (event)
	{
		case LINK_EV_CONNECTED:
			UTIL_GLOGD("Connected");
			source->assign_output(source, &lio);
			source->resume(source);
			connected = true;
			break;
		case LINK_EV_CONN_LOST:
			connected = false;
		default:
			break;
	}
}


int main(void)
{
	source_t *source = NULL;
	int i = 0;
for (i = 0; i < 000; i++)
{
	source_factory_manufacture("file://"HC_SOURCE, &source);
}
	source_factory_manufacture("file://"HC_SOURCE, &source);
	if (source->lock(source, "file://"HC_SOURCE, NULL, event_handler, source) != EOS_ERROR_OK)
	{
		return -1;
	}
		osi_time_usleep(500000);
	while (connected == true)
	{
		osi_time_usleep(150000);
	}
//	source->start(source);
	source->unlock(source);
	source_factory_dismantle(&source);
	return 0;
}

