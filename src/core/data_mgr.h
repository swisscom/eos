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


#ifndef DATA_MGR_H_
#define DATA_MGR_H_

#include "eos_error.h"
#include "eos_media.h"
#include "engine.h"

typedef struct data_mgr data_mgr_t;
/* TODO: mutual inclusion with chain.h */
#include "chain.h"

eos_error_t data_mgr_create(data_mgr_t** data_mgr, engine_cb_data_t cb);
eos_error_t data_mgr_destroy(data_mgr_t** data_mgr);
eos_error_t data_mgr_start(data_mgr_t* data_mgr, chain_t* chain, 
		eos_media_desc_t *streams);
eos_error_t data_mgr_set(data_mgr_t* data_mgr, chain_t* chain, 
		eos_media_desc_t *streams, uint32_t id, bool on);
eos_error_t data_mgr_stop(data_mgr_t* data_mgr);
eos_error_t data_mgr_poll(data_mgr_t* data_mgr, eos_media_codec_t codec,
		uint32_t id, eos_media_data_t **data);
eos_error_t data_mgr_hande_event(data_mgr_t* data_mgr, link_ev_t event,
			link_ev_data_t* data);

eos_error_t data_mgr_ttxt_enable(data_mgr_t* data_mgr, bool enable);
eos_error_t data_mgr_ttxt_page_set(data_mgr_t* data_mgr, uint16_t page,
		uint16_t subpage);
eos_error_t data_mgr_ttxt_page_get(data_mgr_t* data_mgr, uint16_t* page,
		uint16_t* subpage);
eos_error_t data_mgr_ttxt_next_page_get(data_mgr_t* data_mgr, uint16_t* next);
eos_error_t data_mgr_ttxt_prev_page_get(data_mgr_t* data_mgr,
		uint16_t* previous);
eos_error_t data_mgr_ttxt_red_page_get(data_mgr_t* data_mgr,
		uint16_t* red);
eos_error_t data_mgr_ttxt_green_page_get(data_mgr_t* data_mgr,
		uint16_t* green);
eos_error_t data_mgr_ttxt_next_subpage_get(data_mgr_t* data_mgr,
		uint16_t* next);
eos_error_t data_mgr_ttxt_prev_subpage_get(data_mgr_t* data_mgr,
		uint16_t* previous);
eos_error_t data_mgr_ttxt_blue_page_get(data_mgr_t* data_mgr,
		uint16_t* block);
eos_error_t data_mgr_ttxt_yellow_page_get(data_mgr_t* data_mgr,
		uint16_t* group);
eos_error_t data_mgr_ttxt_transparency_set(data_mgr_t* data_mgr,
		uint8_t alpha);
eos_error_t data_mgr_hbbtv_uri_get(data_mgr_t* data_mgr, char* uri);
eos_error_t data_mgr_dvbsub_enable(data_mgr_t* data_mgr, bool enable);

#endif /* DATA_MGR_H_ */
