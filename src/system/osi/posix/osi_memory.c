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


#include "osi_memory.h"

#include <stdlib.h>
#include <string.h>


void* osi_malloc(size_t size)
{
	return malloc(size);
}

void osi_free(void** ptr)
{
	free(*ptr);
	*ptr = NULL;
}

void* osi_calloc(size_t size)
{
	return calloc(size, 1);
}

void* osi_realloc(void* ptr, size_t size)
{
	return realloc(ptr, size);
}

void* osi_memset(void* ptr, uint8_t c, size_t size)
{
	return memset(ptr, c, size);
}

void* osi_memmove(void* to, void* from, size_t size)
{
	return memmove(to, from, size);
}

void* osi_memcpy(void* to, void* from, size_t size)
{
	return memcpy(to, from, size);
}

int32_t osi_memcmp(void* adr1, void* adr2, size_t size)
{
	return memcmp(adr1, adr2, size);
}

