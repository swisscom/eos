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


#ifndef SINK_H_
#define SINK_H_

#include <stdint.h>
#include <stdlib.h>

#include "eos_media.h"
#include "eos_error.h"
#include "lynx.h"

#define SINK_MODULE_NAME "sink"
#define SINK_NAME_MAX (64)

typedef struct sink sink_t;

typedef struct sink_factory_id_data
{
	uint64_t caps;
	uint32_t id;
	link_io_type_t input_type;
} sink_factory_id_data_t;

typedef struct sink_command
{
	eos_error_t (*setup)(sink_t* sink, uint32_t id, eos_media_desc_t* media);
	eos_error_t (*start)(sink_t* sink);
	eos_error_t (*stop)(sink_t* sink);
	eos_error_t (*pause)(sink_t* sink, bool buffering);
	eos_error_t (*resume)(sink_t* sink);
	eos_error_t (*flush_buffs)(sink_t* sink);
	link_reg_ev_hnd_t reg_ev_hnd;
	link_get_ctrl_t get_ctrl_funcs;
} sink_command_t;

struct sink
{
	char name[SINK_NAME_MAX];
	uint64_t caps;
	uint32_t id;
	link_io_t plug;
	link_io_type_t plug_type;
	sink_command_t command;
	void *priv;
};

#endif /* SINK_H_ */
