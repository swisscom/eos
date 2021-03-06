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


#ifndef UTIL_SINGLY_LINKED_LIST_H_
#define UTIL_SINGLY_LINKED_LIST_H_

#include <stdint.h>

#include "eos_types.h"

typedef struct util_slist util_slist_t;
typedef struct util_slist_priv util_slist_priv_t;
typedef bool (*util_slist_cmpr_t)(void* search_param, void* data_address);

struct util_slist
{
	eos_error_t (*add) (util_slist_t thiz, void* data_address);
	eos_error_t (*remove) (util_slist_t thiz, void* search_param);
	eos_error_t (*get) (util_slist_t thiz, void* search_param, void** data_address);
	eos_error_t (*next) (util_slist_t thiz, void** data_address);
	eos_error_t (*first) (util_slist_t thiz, void** data_address);
	eos_error_t (*last) (util_slist_t thiz, void** data_address);
	eos_error_t (*count) (util_slist_t thiz, int32_t* count);

	//private
	util_slist_priv_t *priv;
};

eos_error_t util_slist_create(util_slist_t* list, util_slist_cmpr_t comparator);
eos_error_t util_slist_destroy(util_slist_t* list);


#endif //UTIL_SINGLY_LINKED_LIST_H_

