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


#ifndef UTIL_ILIST_H_
#define UTIL_ILIST_H_

#include "eos_types.h"

#define UTIL_ILIST_IN(x) ((void*)x)
#define UTIL_ILIST_OUT(x) ((void**)x)

typedef struct util_ilist util_ilist_t;
typedef void (*util_ilist_free_t)(void *element);
typedef bool (*util_ilist_cmpr_t)(void *search_param, void *element);
typedef bool (*util_ilist_for_each_t)(void *cookie, void *element);
typedef struct util_ilist_iter util_ilist_iter_t;
typedef struct util_ilist_iter_priv util_ilist_iter_priv_t;
typedef enum util_slist_traverse
{
	UTIL_ILIST_BEG_END = 0,
	UTIL_ILIST_CURR_END,
	UTIL_ILIST_END_BEG,
	UTIL_ILIST_CURR_BEG
} util_slist_traverse_t;

struct util_ilist_iter
{
	eos_error_t (*get) (util_ilist_iter_t *thiz, void **element);

	eos_error_t (*previous) (util_ilist_iter_t *thiz, void **element);
	eos_error_t (*next) (util_ilist_iter_t *thiz, void **element);
	eos_error_t (*first) (util_ilist_iter_t *thiz, void **element);
	eos_error_t (*last) (util_ilist_iter_t *thiz, void **element);

	eos_error_t (*insert_before) (util_ilist_iter_t *thiz, void *element);
	eos_error_t (*insert_after) (util_ilist_iter_t *thiz, void *element);
	eos_error_t (*insert_first) (util_ilist_iter_t *thiz, void *element);
	eos_error_t (*insert_last) (util_ilist_iter_t *thiz, void *element);

	eos_error_t (*remove) (util_ilist_iter_t *thiz, void **element);
	eos_error_t (*remove_first) (util_ilist_iter_t *thiz, void **element);
	eos_error_t (*remove_last) (util_ilist_iter_t *thiz, void **element);

	eos_error_t (*for_each) (util_ilist_iter_t *thiz, 
			util_slist_traverse_t trav,
			util_ilist_for_each_t for_each_cb, void *cookie);
	eos_error_t (*remove_if) (util_ilist_iter_t *thiz, 
			util_slist_traverse_t trav,
			util_ilist_cmpr_t cmpr_cb, 
		       	void *search_param, void **element);
	eos_error_t (*position_to) (util_ilist_iter_t *thiz,
			util_slist_traverse_t trav,
			util_ilist_cmpr_t cmpr_cb,
		       	void *search_param, void **element);

	eos_error_t (*length) (util_ilist_iter_t *thiz, uint32_t *length);

	//private
	util_ilist_iter_priv_t *priv;
};

eos_error_t util_ilist_create(util_ilist_t **list, util_ilist_free_t free_cb);
eos_error_t util_ilist_destroy(util_ilist_t **list);
eos_error_t util_ilist_iter_create(util_ilist_iter_t **iter, util_ilist_t *list);
eos_error_t util_ilist_iter_validate(util_ilist_iter_t *iter);
eos_error_t util_ilist_iter_destroy(util_ilist_iter_t **iter);


#endif //UTIL_ILIST_H_

