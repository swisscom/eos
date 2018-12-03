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


#ifndef SRC_CORE_PLAYBACK_CTRL_PROVIDER_H_
#define SRC_CORE_PLAYBACK_CTRL_PROVIDER_H_

#include "eos_types.h"
#include "chain.h"
#include "playback_ctrl.h"

typedef struct playback_ctrl_provider playback_ctrl_provider_t;
typedef enum playback_ctrl_func_type
{
	PLAYBACK_CTRL_START = 1,
	PLAYBACK_CTRL_SELECT,
	PLAYBACK_CTRL_TRICKPLAY,
	PLAYBACK_CTRL_STOP,
	PLAYBACK_CTRL_START_BUFF,
	PLAYBACK_CTRL_STOP_BUFF,
	PLAYBACK_CTRL_HANDLE_EVENT
} playback_ctrl_func_type_t;

struct playback_ctrl_provider
{
	eos_error_t (*probe)(chain_t* chain, playback_ctrl_func_type_t func_type);
	eos_error_t (*assign)(chain_t* chain, playback_ctrl_func_type_t func_type, playback_ctrl_t* ctrl);
};

#endif // SRC_CORE_PLAYBACK_CTRL_PROVIDER_H_

