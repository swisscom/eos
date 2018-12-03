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


#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "eos_macro.h"
#define MODULE_NAME "test"
#include "util_log.h"

void err_exit(int err, int line)
{
	printf("err: %d [line: %d]\n", err, line);
	exit(err);
}

int main(int argc, char** argv)
{
	char a[1024];
	util_log_t *log = NULL;

	(void)argc;
	(void)argv;

	memset(a, 'a', 1024);
	a[1023] = '\0';
	util_log_set_level(NULL, UTIL_LOG_LEVEL_ALL);
	util_log_set_color(NULL, 1);
	util_log(NULL, NULL, UTIL_LOG_LEVEL_INFO, "INFO!!!!");
	util_log(NULL, NULL, UTIL_LOG_LEVEL_WARN, "WARNING!!!!");
	util_log(NULL, NULL, UTIL_LOG_LEVEL_ERR, "ERROR!!!!");
	util_log(NULL, NULL, UTIL_LOG_LEVEL_DBG, "DEBUG!!!!");
	util_log(NULL, NULL, UTIL_LOG_LEVEL_VERB, "VERBOSE!!!!");
	util_log(NULL, NULL, UTIL_LOG_LEVEL_WTF, "WTF!!!!");

	util_log(NULL, NULL, UTIL_LOG_LEVEL_INFO, "\n");
	util_log_set_level(NULL, UTIL_LOG_LEVEL_INFO | UTIL_LOG_LEVEL_WARN
			| UTIL_LOG_LEVEL_ERR);
	util_log(NULL, NULL, UTIL_LOG_LEVEL_INFO, "INFO!!!!");
	util_log(NULL, NULL, UTIL_LOG_LEVEL_WARN, "WARNING!!!!");
	util_log(NULL, NULL, UTIL_LOG_LEVEL_ERR, "ERROR!!!!");
	util_log(NULL, NULL, UTIL_LOG_LEVEL_DBG, "DEBUG!!!!");
	util_log(NULL, NULL, UTIL_LOG_LEVEL_VERB, "VERBOSE!!!!");
	util_log(NULL, NULL, UTIL_LOG_LEVEL_WTF, "WTF!!!!");

	UTIL_GLOGI("A: %s", a);
	UTIL_GLOGI("\n");

	if(util_log_create(&log, MODULE_NAME) != EOS_ERROR_OK)
	{
		err_exit(-1, __LINE__);
	}
	util_log_set_level(log, UTIL_LOG_LEVEL_INFO | UTIL_LOG_LEVEL_WARN
			| UTIL_LOG_LEVEL_ERR);
	UTIL_LOGI(log, "INFO!!!!", 1);
	UTIL_LOGW(log, "WARNING!!!!");
	UTIL_LOGE(log, "ERROR!!!!");
	UTIL_LOGD(log, "DEBUG!!!!");
	UTIL_LOGV(log, "VERBOSE!!!!");
	UTIL_LOGWTF(log, "WTF!!!!");
	util_log_destroy(&log);

	return 0;
}
