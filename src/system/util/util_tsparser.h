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


#include "eos_media.h"
#include "eos_types.h"
#include "util_slist.h"

#define INFO_ID_FIRST_FOUND (-1)

typedef struct util_tsparser util_tsparser_t;
typedef struct psi_table_arrival_info
{
	uint8_t first_arrived_sec_num;
	uint8_t last_section_num;
	bool first_section_arrived;
	uint8_t version;
	bool all_sec_arrived;
}psi_table_arrival_info_t;

typedef struct ait_desc_app_info
{
	uint32_t application_control_code;
	uint32_t organisation_id;
	uint32_t application_id;
	char* url_base;
	char* initial_path;
} ait_desc_app_info_t;


eos_error_t util_tsparser_create(util_tsparser_t** tsparser);
eos_error_t util_tsparser_destroy (util_tsparser_t** tsparser);
eos_error_t util_tsparser_extract_pmt_media_desc(uint8_t* pmt, eos_media_desc_t* desc);
eos_error_t util_tsparser_get_media_info (util_tsparser_t* tsparser, uint8_t* ts, uint32_t size, int16_t info_id, eos_media_desc_t* desc);
eos_error_t util_tsparser_check_pid (uint8_t* ts, int16_t pid);
eos_error_t util_tsparser_get_ts_payload_by_pid (uint8_t* ts, uint32_t size, uint8_t** payload, uint8_t* payload_len, int16_t pid);
eos_error_t util_tsparser_contains_packet (uint8_t* ts, uint32_t size, int16_t pid);
eos_error_t util_tsparser_parse_ait(uint8_t* section, psi_table_arrival_info_t* psi_table_arrival_info,
		util_slist_t* app_descriptors, uint32_t size);
eos_error_t util_tsparser_get_ifrm_pts (uint8_t* ts, uint32_t size, uint16_t pid, uint64_t* pts);

