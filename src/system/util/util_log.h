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


#ifndef UTIL_LOG_H_
#define UTIL_LOG_H_

#include <stdint.h>
#include <stdlib.h>

#include "osi_error.h"

#ifndef MODULE_NAME
//#define MODULE_NAME (NULL)
#endif

#define UTIL_GLOGI(...) \
	util_log(NULL, MODULE_NAME, UTIL_LOG_LEVEL_INFO, __VA_ARGS__)
#define UTIL_GLOGE(...) \
	util_log(NULL, MODULE_NAME, UTIL_LOG_LEVEL_ERR, __VA_ARGS__)
#define UTIL_GLOGW(...) \
	util_log(NULL, MODULE_NAME, UTIL_LOG_LEVEL_WARN, __VA_ARGS__)
#define UTIL_GLOGD(...) \
	util_log(NULL, MODULE_NAME, UTIL_LOG_LEVEL_DBG, __VA_ARGS__)
#define UTIL_GLOGV(...) \
	util_log(NULL, MODULE_NAME, UTIL_LOG_LEVEL_VERB, __VA_ARGS__)
#define UTIL_GLOGWTF(...) \
	util_log(NULL, MODULE_NAME, UTIL_LOG_LEVEL_WTF, __VA_ARGS__)

#define UTIL_LOGI(log, ...) \
	util_log(log, MODULE_NAME, UTIL_LOG_LEVEL_INFO, __VA_ARGS__)
#define UTIL_LOGE(log, ...) \
	util_log(log, MODULE_NAME, UTIL_LOG_LEVEL_ERR, __VA_ARGS__)
#define UTIL_LOGW(log, ...) \
	util_log(log, MODULE_NAME, UTIL_LOG_LEVEL_WARN, __VA_ARGS__)
#define UTIL_LOGD(log, ...) \
	util_log(log, MODULE_NAME, UTIL_LOG_LEVEL_DBG, __VA_ARGS__)
#define UTIL_LOGV(log, ...) \
	util_log(log, MODULE_NAME, UTIL_LOG_LEVEL_VERB, __VA_ARGS__)
#define UTIL_LOGWTF(log, ...) \
	util_log(log, MODULE_NAME, UTIL_LOG_LEVEL_WTF, __VA_ARGS__)

typedef enum util_log_level
{
	UTIL_LOG_LEVEL_NONE = 0,
	UTIL_LOG_LEVEL_INFO = 1,
	UTIL_LOG_LEVEL_WARN = 2,
	UTIL_LOG_LEVEL_ERR = 4,
	UTIL_LOG_LEVEL_DBG = 8,
	UTIL_LOG_LEVEL_VERB = 16,
	UTIL_LOG_LEVEL_WTF = 32,
	UTIL_LOG_LEVEL_ALL = UTIL_LOG_LEVEL_INFO | UTIL_LOG_LEVEL_WARN |
	UTIL_LOG_LEVEL_ERR | UTIL_LOG_LEVEL_DBG | UTIL_LOG_LEVEL_VERB |
	UTIL_LOG_LEVEL_WTF
} util_log_level_t;

typedef struct util_log_handle util_log_t;

eos_error_t util_log_create(util_log_t** log, char* name);
eos_error_t util_log_destroy(util_log_t** log);
eos_error_t util_log_enable(util_log_t* log, uint8_t enable);
eos_error_t util_log_set_level(util_log_t* log, util_log_level_t level);
eos_error_t util_log_set_color(util_log_t* log, uint8_t enable);
eos_error_t util_log(util_log_t* log, const char* module, util_log_level_t level, const char* format, ...);
eos_error_t util_log_inline_start(util_log_t* log, char* module, util_log_level_t level);
eos_error_t util_log_inline(char* format, ...);
eos_error_t util_log_inline_stop(util_log_t* log);


#endif /* UTIL_LOG_H_ */
