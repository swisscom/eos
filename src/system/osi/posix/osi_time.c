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


#include <unistd.h>
#include <time.h>

#include "osi_time.h"

#define OSI_TIME_NANO_IN_SEC (1000000000)

static inline uint64_t osi_time_tonano(osi_time_t* time)
{
	uint64_t ret;

	ret = time->nsec;
	ret += (uint64_t)time->sec * OSI_TIME_NANO_IN_SEC;

	return ret;
}

eos_error_t osi_time_add(osi_time_t* first, osi_time_t* second, osi_time_t* sum)
{
	if((first == NULL) || (second == NULL) || (sum == NULL))
	{
		return EOS_ERROR_INVAL;
	}
	sum->sec = (first->nsec + second->nsec) / OSI_TIME_NANO_IN_SEC;
	sum->nsec = (first->nsec + second->nsec) - (sum->sec * OSI_TIME_NANO_IN_SEC);
	sum->sec += first->sec + second->sec;

	return EOS_ERROR_OK;
}

eos_error_t osi_time_get_time(osi_time_t* time)
{
	struct timespec t;
	int err;
	int clk_id = CLOCK_REALTIME;

	if(time == NULL)
	{
		return EOS_ERROR_INVAL;
	}
#ifdef CLOCK_REALTIME_COARSE
	clk_id = CLOCK_REALTIME_COARSE;
#endif
	if((err = clock_gettime(clk_id, &t)) != 0)
	{
		return osi_error_conv(err);
	}
	time->sec = t.tv_sec;
	time->nsec = t.tv_nsec;

	return EOS_ERROR_OK;
}

eos_error_t osi_time_get_timestamp(osi_time_t* timestamp)
{
	struct timespec t;
	int err;
	int clk_id = CLOCK_MONOTONIC;

	if(timestamp == NULL)
	{
		return EOS_ERROR_INVAL;
	}
#ifdef CLOCK_MONOTONIC_COARSE
	clk_id = CLOCK_MONOTONIC_COARSE;
#endif
	if((err = clock_gettime(clk_id, &t)) != 0)
	{
		return osi_error_conv(err);
	}
	timestamp->sec = t.tv_sec;
	timestamp->nsec = t.tv_nsec;

	return EOS_ERROR_OK;
}

eos_error_t osi_time_diff(osi_time_t* first, osi_time_t* second, osi_time_t* diff)
{
	uint64_t first_nano, second_nano, diff_nano;


	if(first == NULL || second == NULL || diff == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	first_nano = osi_time_tonano(first);
	second_nano = osi_time_tonano(second);
	if(first_nano > second_nano)
	{
		return EOS_ERROR_INVAL;
	}
	diff_nano = second_nano - first_nano;
	diff->nsec = diff_nano % OSI_TIME_NANO_IN_SEC;
	diff->sec = (diff_nano - diff->nsec) / OSI_TIME_NANO_IN_SEC;

	return EOS_ERROR_OK;
}

eos_error_t osi_time_usleep(uint32_t micros)
{
	int err;

	if((err = usleep(micros)) != 0)
	{
		return osi_error_conv(err);
	}

	return EOS_ERROR_OK;
}
