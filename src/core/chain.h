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


#ifndef SRC_CORE_CHAIN_H_
#define SRC_CORE_CHAIN_H_

#include "eos_error.h"
#include "eos_event.h"
#include "source.h"
#include "sink.h"
#include "osi_time.h"
#include "engine.h"

#define CURRENT_POSITION (-1)

typedef struct chain chain_t;
/* TODO: mutual inclusion with data_magr.h */
#include "data_mgr.h"

typedef eos_error_t (*chain_event_cbk_t)(chain_t* chain, eos_event_t event,
		eos_event_data_t* data, void* cookie);
typedef eos_error_t (*chain_data_cbk_t)(uint32_t id,
		engine_type_t engine_type, engine_data_t data_type,
		uint8_t* data, uint32_t size, void* cookie);

typedef struct chain_handler
{
	chain_event_cbk_t event_cbk;
	void* event_cookie;
	chain_data_cbk_t data_cbk;
	void* data_cookie;
} chain_handler_t;

eos_error_t chain_create(chain_t** chain, uint32_t id,
		chain_handler_t* handler);
eos_error_t chain_destroy(chain_t** chain);
eos_error_t chain_get_id(chain_t* chain, uint32_t* id);
eos_error_t chain_set_source(chain_t* chain, source_t* source);
eos_error_t chain_interrupt(chain_t* chain);
eos_error_t chain_lock(chain_t* chain, char* url, char* extras,
		osi_time_t timeout);
eos_error_t chain_unlock(chain_t* chain);
eos_error_t chain_set_track(chain_t* chain, uint32_t id, bool on);
eos_error_t chain_set_sink(chain_t* chain, sink_t* sink);
eos_error_t chain_get_source(chain_t* chain, source_t** source);
eos_error_t chain_get_streams(chain_t* chain, eos_media_desc_t* streams);
eos_error_t chain_get_stream_data(chain_t* chain, eos_media_codec_t codec,
		uint32_t id, eos_media_data_t **data);
eos_error_t chain_load_data_mgr(chain_t* chain);
eos_error_t chain_unload_data_mgr(chain_t* chain);
eos_error_t chain_get_data_mgr(chain_t* chain, data_mgr_t** data_mgr);
eos_error_t chain_get_sink(chain_t* chain, sink_t** sink);
eos_error_t chain_load_playback_controler(chain_t* chain);
eos_error_t chain_unload_pbk_mgr(chain_t* chain);
eos_error_t chain_start(chain_t* chain);
eos_error_t chain_trickplay(chain_t* chain, int32_t position, int16_t speed);
eos_error_t chain_start_buff(chain_t* chain);
eos_error_t chain_stop_buff(chain_t* chain);
eos_error_t chain_stop(chain_t* chain);


#endif /* SRC_CORE_CHAIN_H_ */

