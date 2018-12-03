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


#ifndef EOS_EVENT_H_
#define EOS_EVENT_H_

#include "eos_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eos_state
{
	EOS_STATE_STOPPED = 0,
	EOS_STATE_PLAYING,
	EOS_STATE_PAUSED,
	EOS_STATE_BUFFERING,
	EOS_STATE_TRANSITIONING
} eos_state_t;

typedef struct eos_state_event
{
	eos_state_t state;
} eos_state_event_t;

typedef struct eos_play_info_event
{
	uint64_t begin;
	uint64_t position;
	int16_t speed;
	uint64_t end;
} eos_play_info_event_t;

typedef struct eos_err_event
{
	eos_error_t err_code;
} eos_err_event_t;

typedef enum eos_pbk_status_event
{
	EOS_PBK_BOS = 1,
	EOS_PBK_EOS,
	EOS_PBK_HIGH_WM,
	EOS_PBK_LOW_WM
} eos_pbk_status_event_t;

typedef enum eos_conn_reason
{
	EOS_CONN_USER = 1,
	EOS_CONN_WR_ERR,
	EOS_CONN_RD_ERR,
	EOS_CONN_DRM_ERR,
	EOS_CONN_SERVER_ERR
} eos_conn_reason_t;

typedef enum eos_conn_state
{
	EOS_CONNECTED = 1,
	EOS_DISCONNECTED
} eos_conn_state_t;

typedef struct eos_conn_state_event
{
	eos_conn_state_t state;
	eos_conn_reason_t reason;
} eos_conn_state_event_t;

typedef enum eos_event
{
	EOS_EVENT_STATE = 1,
	EOS_EVENT_PLAY_INFO,
	EOS_EVENT_PBK_STATUS,
	EOS_EVENT_CONN_STATE,
	EOS_EVENT_ERR,
	EOS_EVENT_LAST
} eos_event_t;

typedef union eos_event_data
{
	eos_state_event_t state;
	eos_play_info_event_t play_info;
	eos_err_event_t err;
	eos_pbk_status_event_t pbk_status;
	eos_conn_state_event_t conn;
} eos_event_data_t;

typedef eos_error_t (*eos_cbk_t)(eos_out_t out, eos_event_t event,
		eos_event_data_t* data, void* cookie);

#ifdef __cplusplus
}
#endif

#endif /* EOS_EVENT_H_ */
