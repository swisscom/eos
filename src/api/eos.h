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


#ifndef _API_EOS_H_
#define _API_EOS_H_

#include "eos_types.h"
#include "eos_error.h"
#include "eos_event.h"
#include "eos_media.h"
#include "eos_data.h"

#ifdef __cplusplus
extern "C" {
#endif

eos_error_t eos_init(void);
eos_error_t eos_deinit(void);
uint64_t eos_get_ver(void);
const char* eos_get_ver_str(void);
eos_error_t eos_set_event_cbk(eos_cbk_t cbk, void* cookie);

eos_error_t eos_player_play(char* in_url, char* in_extras, eos_out_t out);
eos_error_t eos_player_stop(eos_out_t out);
eos_error_t eos_player_trickplay(eos_out_t out, int64_t position,
		int16_t speed);
eos_error_t eos_player_buffer(eos_out_t out, bool start);

eos_error_t eos_player_get_media_desc(eos_out_t out, eos_media_desc_t* desc);
eos_error_t eos_player_set_track(eos_out_t out, uint32_t id, bool on);
eos_error_t eos_player_get_ttxt_pg(eos_out_t out, uint16_t idx,
		char** json_pg);

eos_error_t eos_out_vid_scale(eos_out_t out, uint32_t w, uint32_t h);
eos_error_t eos_out_vid_move(eos_out_t out, uint32_t x, uint32_t y);
eos_error_t eos_out_aud_mode(eos_out_t out, eos_out_amode_t amode);
eos_error_t eos_out_vol_leveling(eos_out_t out, bool enable,
		eos_out_vol_lvl_t lvl);
eos_error_t eos_out_blank(eos_out_t out);
eos_error_t eos_out_release(eos_out_t out);

eos_error_t eos_data_cbk_set(eos_data_cbk_t cbk, void* cookie);

eos_error_t eos_data_ttxt_enable(eos_out_t out, bool enable);
eos_error_t eos_data_ttxt_page_set(eos_out_t out, uint16_t page,
		uint16_t subpage);
eos_error_t eos_data_ttxt_page_get(eos_out_t out, uint16_t* page,
		uint16_t* subpage);
eos_error_t eos_data_ttxt_next_page_get(eos_out_t out, uint16_t* next);
eos_error_t eos_data_ttxt_prev_page_get(eos_out_t out, uint16_t* previous);
eos_error_t eos_data_ttxt_red_page_get(eos_out_t out, uint16_t* red);
eos_error_t eos_data_ttxt_green_page_get(eos_out_t out, uint16_t* green);
eos_error_t eos_data_ttxt_next_subpage_get(eos_out_t out, uint16_t* next);
eos_error_t eos_data_ttxt_prev_subpage_get(eos_out_t out, uint16_t* previous);
eos_error_t eos_data_ttxt_blue_page_get(eos_out_t out, uint16_t* blue);
eos_error_t eos_data_ttxt_yellow_page_get(eos_out_t out, uint16_t* yellow);
eos_error_t eos_data_ttxt_transparency_set(eos_out_t out, uint8_t alpha);
eos_error_t eos_data_hbbtv_uri_get(eos_out_t out, char* uri);
eos_error_t eos_data_dvbsub_enable(eos_out_t out, bool enable);


#ifdef __cplusplus
}
#endif

#endif /* _API_EOS_H_ */
