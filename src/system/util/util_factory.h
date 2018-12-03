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


#ifndef UTIL_FACTORY_H_
#define UTIL_FACTORY_H_

#include "eos_types.h"

#define UTIL_FACTORY_INV_PRODUCT_ID   ((uint64_t)-1LL)
#define UTIL_FACTORY_START_PRODUCT_ID ((uint64_t)0LL)

typedef struct util_factory util_factory_t;

typedef eos_error_t (*util_probe_cb_t) (void* model, void* data);
typedef eos_error_t (*util_manufacture_cb_t) (void* data, void* model, uint64_t model_id,
		void** product, uint64_t product_id);
typedef eos_error_t (*util_dismantle_cb_t) (uint64_t model_id, void** product);

eos_error_t util_factory_create(util_factory_t** factory, util_probe_cb_t probe);
eos_error_t util_factory_destroy(util_factory_t** factory);
eos_error_t util_factory_register (util_factory_t* factory, void* model, uint64_t* model_id,
		util_manufacture_cb_t manufacture, util_dismantle_cb_t dismantle);
eos_error_t util_factory_unregister (util_factory_t* factory, void* model, uint64_t model_id);
eos_error_t util_factory_get_count (util_factory_t* factory, uint32_t* count);
eos_error_t util_factory_manufacture (util_factory_t* factory, void** product, void* data);
eos_error_t util_factory_dismantle (util_factory_t* factory, void** product);

#endif /* UTIL_FACTORY_H_ */
