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


#ifndef SOURCE_H_
#define SOURCE_H_

#include "eos_error.h"
#include "lynx.h"

#include <stdint.h>
#include <stdlib.h>

#define SOURCE_NAME_MAX (64)
#define SOURCE_URI_SCHEME_MAX (32)
#define SOURCE_MODULE_NAME "source"

/**
 * \brief Source capabilities.
 */
typedef enum source_cap
{
	SOURCE_CAP_NONE      = 0x0000000000000000LL,
	SOURCE_CAP_FCC       = 0x0000000000000001LL,
	SOURCE_CAP_TIME_SEEK = 0x0000000000000002LL,
	SOURCE_CAP_BYTE_SEEK = 0x0000000000000004LL
} source_cap_t;

typedef enum source_state
{
	SOURCE_STATE_INITIALIZED = 0,
	SOURCE_STATE_STARTING,
	SOURCE_STATE_STARTED,
	SOURCE_STATE_STOPPING,
	SOURCE_STATE_STOPPED,
	SOURCE_STATE_SUSPENDED
} source_state_t;

typedef void* source_handle_t;
typedef struct source source_t;

struct source
{
	source_handle_t handle;

	const char* (*name) (void);
	eos_error_t (*probe) (char* uri);
	eos_error_t (*prelock) (source_t* source, char* uri);
	// TODO expand with throughput preference
	eos_error_t (*lock) (source_t* source, char* uri, char* extras,
			link_ev_hnd_t event_cb, void* event_cookie);
	eos_error_t (*unlock) (source_t* source);
	eos_error_t (*resume) (source_t* source);
	eos_error_t (*suspend) (source_t* source);
	eos_error_t (*flush_buffers) (source_t* source);
	eos_error_t (*get_output_type) (source_t* source, link_io_type_t* type);
	eos_error_t (*get_capabilities) (source_t* source, uint64_t* capabilities);
	link_reg_ev_hnd_t reg_ev_hnd;
	link_get_ctrl_t get_ctrl_funcs;
	eos_error_t (*assign_output) (source_t* source, link_io_t* next_link_io);
	void (*handle_event)(source_t* source, link_ev_t event, link_ev_data_t* data);
};

#endif /* SOURCE_H_ */

