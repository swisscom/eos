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


#ifndef UTIL_SEQ_BUFF_H_
#define UTIL_SEQ_BUFF_H_

#include "eos_error.h"
#include "eos_types.h"

typedef struct util_seq_buff util_seq_buff_t;

typedef struct util_seq_buff_cb
{
	void *handle;
	eos_error_t (*consume)(void* handle, uint8_t* address, uint32_t size);
} util_seq_buff_cb_t;

eos_error_t util_seq_buff_create(util_seq_buff_t** sbuff, uint8_t* data, uint32_t size,
                                 uint32_t segment_size, util_seq_buff_cb_t* cb);
eos_error_t util_seq_buff_destroy(util_seq_buff_t** sbuff);

eos_error_t util_seq_buff_allocate(util_seq_buff_t* sbuff, uint32_t size, int32_t timeout,
                                   uint8_t** address, uint32_t* allocated);

eos_error_t util_seq_buff_commit(util_seq_buff_t* sbuff, uint8_t* address, uint32_t size);

eos_error_t util_seq_buff_release(util_seq_buff_t* sbuff, uint8_t* address, uint32_t size);

eos_error_t util_seq_buff_reset(util_seq_buff_t* sbuff, int32_t timeout);

#endif // UTIL_SEQ_BUFF_H_

