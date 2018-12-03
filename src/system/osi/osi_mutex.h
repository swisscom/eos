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


#ifndef OSI_MUTEX_H_
#define OSI_MUTEX_H_

#include "osi_error.h"
#include "osi_time.h"

#define CHECK_AND_SET(mutex, result, variable, value, condition) \
{ \
	eos_error_t temp_random_error = EOS_ERROR_OK; \
	temp_random_error = osi_mutex_lock(mutex); \
	if (temp_random_error != EOS_ERROR_OK) \
	{ \
		result = (temp_random_error == EOS_ERROR_OK); \
	} \
	else \
	{ \
		result = (condition); \
		if (condition) \
		{ \
			variable = value; \
		} \
		osi_mutex_unlock(mutex); \
	} \
}

typedef struct osi_mutex_handle osi_mutex_t;

eos_error_t osi_mutex_create(osi_mutex_t** mutex);
eos_error_t osi_mutex_destroy(osi_mutex_t** mutex);
eos_error_t osi_mutex_lock(osi_mutex_t* mutex);
eos_error_t osi_mutex_trylock(osi_mutex_t* mutex, const osi_time_t* timeout);
eos_error_t osi_mutex_unlock(osi_mutex_t* mutex);

#endif /* OSI_MUTEX_H_ */
