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


#ifndef SRC_CORE_PBK_MGR_H_
#define SRC_CORE_PBK_MGR_H_

#include "eos_types.h"
#include "eos_error.h"
#include "chain.h"

typedef struct pbk_mgr pbk_mgr_t;

struct pbk_mgr
{
	eos_error_t (*start)(chain_t* chain);
	eos_error_t (*play)(chain_t* chain, int64_t position, int16_t speed);
	eos_error_t (*stop)(pbk_mgr_t* pbk_mgr);
};


eos_error_t pbk_mgr_create(pbk_mgr_t** pbk_mgr);
eos_error_t pbk_mgr_destroy(pbk_mgr_t** pbk_mgr);
eos_error_t pbk_mgr_set_chain(pbk_mgr_t* pbk_mgr, chain_t* chain);
//eos_error_t pbk_mgr_set_cbks(pbk_mgr_t* pbk_mgr, pbk_mgr_cbks_t* cbks);
eos_error_t pbk_mgr_play(pbk_mgr_t* pbk_mgr);
eos_error_t pbk_mgr_can_pause(pbk_mgr_t* pbk_mgr, bool* can);
eos_error_t pbk_mgr_pause(pbk_mgr_t* pbk_mgr);
eos_error_t pbk_mgr_can_set_speed(pbk_mgr_t* pbk_mgr, bool* can);
eos_error_t pbk_mgr_set_speed(pbk_mgr_t* pbk_mgr, int16_t speed);
eos_error_t pbk_mgr_can_seek(pbk_mgr_t* pbk_mgr, bool* can);
eos_error_t pbk_mgr_seek(pbk_mgr_t* pbk_mgr, uint32_t seconds);
eos_error_t pbk_mgr_stop(pbk_mgr_t* pbk_mgr);

#endif /* SRC_CORE_PBK_MGR_H_ */

