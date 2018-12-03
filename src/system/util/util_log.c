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
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#ifdef ANDROID
#include <android/log.h>
#endif

#include "util_log.h"
#include "osi_memory.h"
#include "osi_mutex.h"
#include "eos_macro.h"

#define LOGGER_NAME_MAX (32)
#define LOGGER_BUF_LEN (1024)
#define LOGGER_LOCK_LOCK(logger) osi_mutex_lock(logger->lock)
#define LOGGER_LOCK_UNLOCK(logger) osi_mutex_unlock(logger->lock)
#define LOGGER_TXT_INF ("[INF]")
#define LOGGER_TXT_WRN ("[WRN]")
#define LOGGER_TXT_ERR ("[ERR]")
#define LOGGER_TXT_DBG ("[DBG]")
#define LOGGER_TXT_VER ("[VER]")
#define LOGGER_TXT_WTF ("[WTF]")
#define LOGGER_VT100_ESC (0x1B)
#define LOGGER_VT100_CLR_FORMAT ("%c[1;%dm")
#define LOGGER_VT100_CLR_INF (37)
#define LOGGER_VT100_CLR_WRN (33)
#define LOGGER_VT100_CLR_ERR (31)
#define LOGGER_VT100_CLR_DBG (32)
#define LOGGER_VT100_CLR_VER (36)
#define LOGGER_VT100_CLR_WTF (35)
#define LOGGER_VT100_RST_FORMAT ("%c[0m")

//#undef ANDROID

struct util_log_handle
{
	char name[LOGGER_NAME_MAX];
	uint8_t enable;
	uint8_t color;
	util_log_level_t level;
	util_log_level_t inline_level;
	uint8_t timestamp;
	FILE* file;
	osi_mutex_t *lock;
	char buffer[LOGGER_BUF_LEN];
};

static inline util_log_t* logger_get_handle(util_log_t* log);
static void logger_insert_timestamp(util_log_t* logger);
static void logger_insert_level(util_log_t* logger, util_log_level_t level);
static void logger_insert_module(util_log_t* logger, char* module);
static void logger_close_log(util_log_t* logger);
static void logger_log(util_log_t* logger);
static eos_error_t util_valog(util_log_t* log, char* module, util_log_level_t level,
		char* format, va_list args);


static util_log_t logger_global =
{	
	.name = {0}, 
#ifdef EOS_LOG_ALL
	.enable = 1,
	.color = 1,
	.level = UTIL_LOG_LEVEL_ALL,
	.inline_level = UTIL_LOG_LEVEL_ALL,
	.timestamp = 1,
#else
	.enable = 0,
	.color = 0,
	.level = UTIL_LOG_LEVEL_NONE,
	.inline_level = UTIL_LOG_LEVEL_NONE,
	.timestamp = 0,
#endif
	.file = NULL,
	.lock = NULL, 
	.buffer = {0}
};

CALL_ON_LOAD(util_log_init_global)
static void util_log_init_global(void)
{
	char *name = EOS_NAME;

	if(logger_global.lock != NULL)
	{
		return;
	}
	if(name != NULL)
	{
		strncpy(logger_global.name, name, LOGGER_NAME_MAX);
		logger_global.name[LOGGER_NAME_MAX - 1] = '\0';
	}

#ifdef EOS_LOG_ALL
	logger_global.enable = 1;
	logger_global.color = 1;
	logger_global.level = UTIL_LOG_LEVEL_ALL;
	logger_global.inline_level = UTIL_LOG_LEVEL_ALL;
	logger_global.timestamp = 1;
#else
	logger_global.enable = 0;
	logger_global.color = 0;
	logger_global.level = UTIL_LOG_LEVEL_NONE;
	logger_global.inline_level = UTIL_LOG_LEVEL_NONE;
	logger_global.timestamp = 0;
#endif

	logger_global.file = NULL;
	osi_mutex_create(&logger_global.lock);
}

CALL_ON_UNLOAD(util_log_deinit_global)
static void util_log_deinit_global(void)
{
	// TODO Memset logger_global.name to nothingness
	osi_mutex_destroy(&logger_global.lock);
}

eos_error_t util_log_create(util_log_t** log, char* name)
{
	util_log_t *tmp = NULL;
	eos_error_t err = EOS_ERROR_OK;

	if(log == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	tmp = (util_log_t *)osi_calloc(sizeof(util_log_t));
	if(tmp == NULL)
	{
		return EOS_ERROR_NOMEM;
	}
	if(name != NULL)
	{
		strncpy(tmp->name, name, LOGGER_NAME_MAX);
		tmp->name[LOGGER_NAME_MAX - 1] = '\0';
	}
	if((err = osi_mutex_create(&tmp->lock)) != EOS_ERROR_OK)
	{
		goto done;
	}
#ifdef EOS_LOG_ALL
	tmp->enable = 1;
	tmp->color = 1;
	tmp->level = UTIL_LOG_LEVEL_ALL;
	tmp->inline_level = UTIL_LOG_LEVEL_ALL;
	tmp->timestamp = 1;
#else
	tmp->enable = 1;
	tmp->color = 0;
	tmp->level = UTIL_LOG_LEVEL_NONE;
	tmp->inline_level = UTIL_LOG_LEVEL_NONE;
	tmp->timestamp = 0;
#endif
	tmp->file = NULL;
	*log = tmp;

done:
	if(err != EOS_ERROR_OK)
	{
		if(tmp->lock != NULL)
		{
			osi_mutex_destroy(&tmp->lock);
		}
		if(tmp != NULL)
		{
			osi_free((void**)&tmp);
		}
	}
	return err;
}

eos_error_t util_log_destroy(util_log_t** log)
{
	util_log_t *logger = NULL;

	if(log == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	logger = *log;
	LOGGER_LOCK_LOCK(logger);
	LOGGER_LOCK_UNLOCK(logger);
	osi_mutex_destroy(&logger->lock);
	osi_free((void**)log);

	return EOS_ERROR_OK;
}

eos_error_t util_log_enable(util_log_t* log, uint8_t enable)
{
	util_log_t *logger = logger_get_handle(log);

	LOGGER_LOCK_LOCK(logger);
	logger->enable = enable;
	LOGGER_LOCK_UNLOCK(logger);

	return EOS_ERROR_OK;
}

eos_error_t util_log_set_color(util_log_t* log, uint8_t enable)
{
	util_log_t *logger = logger_get_handle(log);

	LOGGER_LOCK_LOCK(logger);
	logger->color = enable;
	LOGGER_LOCK_UNLOCK(logger);

	return EOS_ERROR_OK;
}

eos_error_t util_log_set_level(util_log_t* log, util_log_level_t level)
{
	util_log_t *logger = logger_get_handle(log);

	LOGGER_LOCK_LOCK(logger);
	logger->level = level;
	LOGGER_LOCK_UNLOCK(logger);

	return EOS_ERROR_OK;
}

eos_error_t util_log(util_log_t* log, const char* module, util_log_level_t level, const char* format, ...)
{
	va_list args;
	eos_error_t err = EOS_ERROR_OK;

	if(format == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	va_start(args, format);
	err = util_valog(log, (char*)module, level, (char*)format, args);
	va_end(args);

	return err;
}

static eos_error_t util_valog(util_log_t* log, char* module, util_log_level_t level, char* format, va_list args)
{
	util_log_t *logger = NULL;
	int len;

	logger = logger_get_handle(log);
	LOGGER_LOCK_LOCK(logger);
	/* should we ignore it */
	if((logger->level & level) == 0 || logger->enable == 0)
	{
		LOGGER_LOCK_UNLOCK(logger);
		return EOS_ERROR_OK;
	}

	logger->buffer[0] = '\0';
	logger_insert_timestamp(logger);
	logger_insert_level(logger, level);
	logger_insert_module(logger, module);
	len = strlen(logger->buffer);
	vsnprintf(logger->buffer + len, LOGGER_BUF_LEN - len,
			format, args);
	logger_close_log(logger);
#ifdef ANDROID
	int prio = ANDROID_LOG_UNKNOWN;

	switch(level)
	{
	case UTIL_LOG_LEVEL_INFO:
		prio = ANDROID_LOG_INFO;
		break;
	case UTIL_LOG_LEVEL_WARN:
		prio = ANDROID_LOG_WARN;
		break;
	case UTIL_LOG_LEVEL_ERR:
		prio = ANDROID_LOG_ERROR;
		break;
	case UTIL_LOG_LEVEL_DBG:
		prio = ANDROID_LOG_DEBUG;
		break;
	case UTIL_LOG_LEVEL_VERB:
		prio = ANDROID_LOG_VERBOSE;
		break;
	case UTIL_LOG_LEVEL_WTF:
		prio = ANDROID_LOG_FATAL;
		break;
	default:
		break;
	}
	__android_log_write(prio, logger->name, logger->buffer);
#else
	logger_log(logger);
#endif

	LOGGER_LOCK_UNLOCK(logger);

	return EOS_ERROR_OK;
}


eos_error_t util_log_inline_start(util_log_t* log, char* module, util_log_level_t level);
eos_error_t util_log_inline(char* format, ...);
eos_error_t util_log_inline_stop(util_log_t* log);

static inline util_log_t* logger_get_handle(util_log_t* log)
{
	if(log == NULL)
	{
		if(logger_global.lock == NULL)
		{
			util_log_init_global();
		}
		return &logger_global;
	}
	return log;
}

static void logger_insert_timestamp(util_log_t* logger)
{
	osi_time_t ts = {0, 0};
	uint32_t h = 0, m = 0, s = 0;
	char formated[13] = "";

	if(logger->timestamp == 1)
	{
		if(osi_time_get_timestamp(&ts) != EOS_ERROR_OK)
		{
			return;
		}
		strcat(logger->buffer, "[");
		h = OSI_TIME_SEC_TO_HOUR(ts.sec);
		ts.sec -= OSI_TIME_HOUR_TO_SEC(h);
		m = OSI_TIME_SEC_TO_MIN(ts.sec);
		s = ts.sec - OSI_TIME_MIN_TO_SEC(m);
		sprintf(formated, "%02d:%02d:%02d:%03d", h % 24, m, s,
				(uint32_t)OSI_TIME_NSEC_TO_MSEC(ts.nsec));
		strcat(logger->buffer, formated);
		strcat(logger->buffer, "]");
	}
}

static void logger_insert_level(util_log_t* logger, util_log_level_t level)
{
	int color = 0;

	if(logger->color == 0 || logger->file != NULL)
	{
		switch(level)
		{
		case UTIL_LOG_LEVEL_INFO:
			strcat(logger->buffer, LOGGER_TXT_INF);
			break;
		case UTIL_LOG_LEVEL_WARN:
			strcat(logger->buffer, LOGGER_TXT_WRN);
			break;
		case UTIL_LOG_LEVEL_ERR:
			strcat(logger->buffer, LOGGER_TXT_ERR);
			break;
		case UTIL_LOG_LEVEL_DBG:
			strcat(logger->buffer, LOGGER_TXT_DBG);
			break;
		case UTIL_LOG_LEVEL_VERB:
			strcat(logger->buffer, LOGGER_TXT_VER);
			break;
		case UTIL_LOG_LEVEL_WTF:
			strcat(logger->buffer, LOGGER_TXT_WTF);
			break;
		default:
			break;
		}
	}
	else
	{
		switch(level)
		{
		case UTIL_LOG_LEVEL_INFO:
			color = LOGGER_VT100_CLR_INF;
			break;
		case UTIL_LOG_LEVEL_WARN:
			color = LOGGER_VT100_CLR_WRN;
			break;
		case UTIL_LOG_LEVEL_ERR:
			color = LOGGER_VT100_CLR_ERR;
			break;
		case UTIL_LOG_LEVEL_DBG:
			color = LOGGER_VT100_CLR_DBG;
			break;
		case UTIL_LOG_LEVEL_VERB:
			color = LOGGER_VT100_CLR_VER;
			break;
		case UTIL_LOG_LEVEL_WTF:
			color = LOGGER_VT100_CLR_WTF;
			break;
		default:
			return;
		}
		sprintf(logger->buffer + strlen(logger->buffer),
				LOGGER_VT100_CLR_FORMAT, LOGGER_VT100_ESC, color);
	}
}

static void logger_insert_module(util_log_t* logger, char* module)
{
	char tmp[LOGGER_NAME_MAX] = "";

	strcat(logger->buffer, "[");
	if(logger->name[0] != '\0')
	{
		strcat(logger->buffer, logger->name);
	}
	if(module != NULL)
	{
		strncpy(tmp, module, LOGGER_NAME_MAX);
		tmp[LOGGER_NAME_MAX - 1] = '\0';
		strcat(logger->buffer, ":");
		strcat(logger->buffer, tmp);
	}
	strcat(logger->buffer, "] ");
}

static void logger_close_log(util_log_t* logger)
{
	char rst[8];
	int len = 0;

	logger->buffer[LOGGER_BUF_LEN - 1] = '\0';
	if(logger->color)
	{
		len = strlen(logger->buffer);
		sprintf(rst, LOGGER_VT100_RST_FORMAT, LOGGER_VT100_ESC);
		if(len + strlen(rst) >= LOGGER_BUF_LEN)
		{
			logger->buffer[LOGGER_BUF_LEN - strlen(rst) - 1] = '\0';
		}
		strcat(logger->buffer, rst);
	}
}


static void logger_log(util_log_t* logger)
{
	if(logger->file != NULL)
	{
		fputs(logger->buffer, logger->file);
		fflush(logger->file);
	}
	else
	{
		printf("%s\r\n", logger->buffer);
		fflush(stdout);
	}
}

