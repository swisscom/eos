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


#ifndef EOS_ENGINE_H_
#define EOS_ENGINE_H_

#include "eos_types.h"
#include "eos_media.h"
#include "lynx.h"

typedef enum engine_type
{
	ENGINE_TYPE_TTXT = 1,
	ENGINE_TYPE_HBBTV,
	ENGINE_TYPE_DVB_SUB,
	ENGINE_TYPE_DATA_PROV,
	ENGINE_TYPE_DSMCC,
	ENGINE_TYPE_INVALID
} engine_type_t;

typedef enum engine_data
{
	ENGINE_DATA_STRING = 1,
	ENGINE_DATA_HTML,
	ENGINE_DATA_JSON,
	ENGINE_DATA_BASE64_PNG
} engine_data_t;

typedef void* engine_handle_t;
typedef struct engine engine_t;

typedef struct engine_cb_data
{
	void* cookie;
	eos_error_t (*func) (void* cookie, engine_type_t engine_type, 
	                   engine_data_t data_type, uint8_t* data, uint32_t size);
} engine_cb_data_t;

typedef struct engine_params
{
	eos_media_codec_t codec;
	engine_cb_data_t cb;
} engine_params_t;

typedef struct engine_api
{
	engine_type_t type;
	union
	{
		struct
		{
			eos_error_t (*select) (uint16_t page);
		} ttxt;
		struct
		{
			eos_error_t (*get_red_btn_url) (engine_t* engine, uint8_t** url);
			eos_error_t (*get_red_btn_url_len) (engine_t* engine,
					uint16_t* url_len);
		} hbbtv;
		struct
		{
			eos_error_t (*set_data_prov)(engine_t* engine, link_handle_t link,
					link_cap_data_prov_t *func);
			eos_error_t (*ttxt_enable) (engine_t* engine, bool enable);
			eos_error_t (*ttxt_page_set) (engine_t* engine, uint16_t page,
					uint16_t subpage);
			eos_error_t (*ttxt_page_get) (engine_t* engine, uint16_t* page,
					uint16_t* subpage);
			eos_error_t (*ttxt_next_page_get) (engine_t* engine,
					uint16_t* next);
			eos_error_t (*ttxt_prev_page_get) (engine_t* engine,
					uint16_t* previous);
			eos_error_t (*ttxt_red_page_get) (engine_t* engine,
					uint16_t* red);
			eos_error_t (*ttxt_green_page_get) (engine_t* engine,
					uint16_t* green);
			eos_error_t (*ttxt_blue_page_get) (engine_t* engine,
					uint16_t* blue);
			eos_error_t (*ttxt_yellow_page_get) (engine_t* engine,
					uint16_t* yellow);
			eos_error_t (*ttxt_next_subpage_get) (engine_t* engine,
					uint16_t* next);
			eos_error_t (*ttxt_prev_subpage_get) (engine_t* engine,
					uint16_t* previous);
			eos_error_t (*ttxt_transparency_set) (engine_t* engine,
								uint8_t alpha);
			eos_error_t (*dvbsub_enable) (engine_t* engine, bool enable);
		} data_prov;
	} func;
} engine_api_t;

typedef eos_error_t (*engine_in_hook_t) (engine_t* engine, uint8_t* data,
		uint32_t size);


struct engine
{
	engine_handle_t handle;

	const char* (*name) (void);
	eos_error_t (*probe) (eos_media_codec_t codec);
	eos_error_t (*get_api) (engine_t* engine, engine_api_t* api);
	eos_error_t (*get_hook) (engine_t* engine, engine_in_hook_t* hook);
	eos_error_t (*flush) (engine_t* engine);
	eos_error_t (*enable) (engine_t* engine);
	eos_error_t (*disable) (engine_t* engine);
	eos_error_t (*event_handler) (engine_t* engine, link_ev_t event,
			link_ev_data_t* data);
};

#endif // EOS_ENGINE_H_

