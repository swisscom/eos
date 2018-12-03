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


#ifndef OSI_TIME_H_
#define OSI_TIME_H_

#include <inttypes.h>

#include "osi_error.h"

#define OSI_TIME_MICROS (1000000L)

#define OSI_TIME_USEC_TO_NSEC(usec) ((usec) * 1000LL)
#define OSI_TIME_MSEC_TO_USEC(msec) ((msec) * 1000LL)
#define OSI_TIME_MSEC_TO_NSEC(msec) (OSI_TIME_USEC_TO_NSEC(OSI_TIME_MSEC_TO_USEC(msec)))
#define OSI_TIME_SEC_TO_MSEC(sec) ((sec) * 1000LL)
#define OSI_TIME_SEC_TO_USEC(sec) (OSI_TIME_MSEC_TO_USEC(OSI_TIME_SEC_TO_MSEC(sec)))
#define OSI_TIME_SEC_TO_NSEC(sec) (OSI_TIME_MSEC_TO_NSEC(OSI_TIME_SEC_TO_MSEC(sec)))
#define OSI_TIME_MIN_TO_SEC(min) ((min) * 60LL)
#define OSI_TIME_MIN_TO_MSEC(min) (OSI_TIME_SEC_TO_MSEC(OSI_TIME_MIN_TO_SEC(min)))
#define OSI_TIME_MIN_TO_USEC(min) (OSI_TIME_SEC_TO_USEC(OSI_TIME_MIN_TO_SEC(min)))
#define OSI_TIME_MIN_TO_NSEC(min) (OSI_TIME_SEC_TO_NSEC(OSI_TIME_MIN_TO_SEC(min)))
#define OSI_TIME_HOUR_TO_MIN(hour) ((hour) * 60LL)
#define OSI_TIME_HOUR_TO_SEC(hour) (OSI_TIME_MIN_TO_SEC(OSI_TIME_HOUR_TO_MIN(hour)))
#define OSI_TIME_HOUR_TO_MSEC(hour) (OSI_TIME_MIN_TO_MSEC(OSI_TIME_HOUR_TO_MIN(hour)))
#define OSI_TIME_HOUR_TO_USEC(hour) (OSI_TIME_MIN_TO_USEC(OSI_TIME_HOUR_TO_MIN(hour)))
#define OSI_TIME_HOUR_TO_NSEC(hour) (OSI_TIME_MIN_TO_NSEC(OSI_TIME_HOUR_TO_MIN(hour)))

#define OSI_TIME_NSEC_TO_USEC(nsec) ((nsec) / 1000LL)
#define OSI_TIME_USEC_TO_MSEC(usec) ((usec) / 1000LL)
#define OSI_TIME_NSEC_TO_MSEC(nsec) (OSI_TIME_USEC_TO_MSEC(OSI_TIME_NSEC_TO_USEC(nsec)))
#define OSI_TIME_MSEC_TO_SEC(msec) ((msec) / 1000LL)
#define OSI_TIME_USEC_TO_SEC(usec) (OSI_TIME_MSEC_TO_SEC(OSI_TIME_USEC_TO_MSEC(usec)))
#define OSI_TIME_NSEC_TO_SEC(nsec) (OSI_TIME_MSEC_TO_SEC(OSI_TIME_NSEC_TO_MSEC(nsec)))
#define OSI_TIME_SEC_TO_MIN(sec)   ((sec) / 60LL)
#define OSI_TIME_MSEC_TO_MIN(msec) (OSI_TIME_SEC_TO_MIN(OSI_TIME_MSEC_TO_SEC(msec)))
#define OSI_TIME_USEC_TO_MIN(usec) (OSI_TIME_SEC_TO_MIN(OSI_TIME_USEC_TO_SEC(usec)))
#define OSI_TIME_NSEC_TO_MIN(nsec) (OSI_TIME_SEC_TO_MIN(OSI_TIME_NSEC_TO_SEC(nsec)))
#define OSI_TIME_MIN_TO_HOUR(min)   ((min) / 60LL)
#define OSI_TIME_SEC_TO_HOUR(sec)   (OSI_TIME_MIN_TO_HOUR(OSI_TIME_SEC_TO_MIN(sec)))
#define OSI_TIME_MSEC_TO_HOUR(msec) (OSI_TIME_MIN_TO_HOUR(OSI_TIME_MSEC_TO_MIN(msec)))
#define OSI_TIME_USEC_TO_HOUR(usec) (OSI_TIME_MIN_TO_HOUR(OSI_TIME_USEC_TO_MIN(usec)))
#define OSI_TIME_NSEC_TO_HOUR(nsec) (OSI_TIME_MIN_TO_HOUR(OSI_TIME_NSEC_TO_MIN(nsec)))

#define OSI_TIME_CONVERT_FROM_NSEC(osi_time_var,nnsec) \
{ \
	osi_time_var.sec = OSI_TIME_NSEC_TO_SEC(nnsec); \
	osi_time_var.nsec = (nnsec) - OSI_TIME_SEC_TO_NSEC(osi_time_var.sec); \
}
#define OSI_TIME_CONVERT_FROM_USEC(osi_time_var,usec) OSI_TIME_CONVERT_FROM_NSEC(osi_time_var,OSI_TIME_USEC_TO_NSEC(usec))
#define OSI_TIME_CONVERT_FROM_MSEC(osi_time_var,msec) OSI_TIME_CONVERT_FROM_NSEC(osi_time_var,OSI_TIME_MSEC_TO_NSEC(msec))

#define OSI_TIME_CONVERT_TO_NSEC(osi_time_var,nnsec) \
{ \
	nnsec = OSI_TIME_SEC_TO_NSEC(osi_time_var.sec) + osi_time_var.nsec; \
}
#define OSI_TIME_CONVERT_TO_USEC(osi_time_var,usec) \
{ \
	usec = OSI_TIME_SEC_TO_USEC(osi_time_var.sec) + OSI_TIME_NSEC_TO_USEC(osi_time_var.nsec); \
}
#define OSI_TIME_CONVERT_TO_MSEC(osi_time_var,msec) \
{ \
	msec = OSI_TIME_SEC_TO_MSEC(osi_time_var.sec) + OSI_TIME_NSEC_TO_MSEC(osi_time_var.nsec); \
}


/**
 * Timestamp definition.
 */
typedef struct osi_time
{
	uint32_t sec;
	uint32_t nsec;
} osi_time_t;

eos_error_t osi_time_get_time(osi_time_t* time);
eos_error_t osi_time_get_timestamp(osi_time_t* timestamp);
eos_error_t osi_time_add(osi_time_t* first, osi_time_t* second, osi_time_t* sum);
eos_error_t osi_time_diff(osi_time_t* first, osi_time_t* second, osi_time_t* diff);
eos_error_t osi_time_usleep(uint32_t micros);

#endif /* OSI_TIME_H_ */
