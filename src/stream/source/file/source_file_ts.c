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


// *************************************
// *       Module name definition      *
// *************************************

#define PARENT_MODULE_NAME SOURCE_MODULE_NAME
#define FILE_TS_MODULE_NAME "file:ts"
#define MODULE_NAME PARENT_MODULE_NAME":"FILE_TS_MODULE_NAME

// *************************************
// *             Includes              *
// *************************************

#include "source.h"
#include "source_factory.h"
#include "eos_types.h"
#include "eos_macro.h"
#include "osi_time.h"
#include "osi_thread.h"
#include "osi_memory.h"
#include "osi_mutex.h"
#include "osi_bin_sem.h"
#include "util_log.h"
#include "util_tsparser.h"
#include "fsi_file.h"

#include <string.h> // For strncpy,...
#include <strings.h> // For strncasecmp

// *************************************
// *              Macros               *
// *************************************

#define SOURCE_NAME "file:ts"
#define FILE_TS_URI_PREFIX "file://"
#define FILE_TS_URI_SUFFIX ".ts"

#define FAILED_ALLOCATIONS_COUNT 20
#define FAILED_ALLOCATIONS_TIMEOUT 100 // msec
#define FAILED_COMMITS_COUNT 20
#define FAILED_COMMITS_TIMEOUT 100 // msec
#define FAILED_READS_COUNT 20
#define FAILED_READS_TIMEOUT 100 // msec

#define START_WAIT_TIMEOUT 2000 // msec

#define READ_CHUNK_SIZE (7 * 188)

// *************************************
// *              Types                *
// *************************************

typedef struct source_file_ts_shared
{
	osi_mutex_t *lock_unlock;
	osi_mutex_t *cas; // check and set mutex
} source_file_ts_shared_t;

typedef struct source_file_ts_private
{
	util_log_t *log;
	link_io_t *output;
	link_ev_hnd_t event_cb;
	void *event_cookie;
	osi_mutex_t *sync;
	source_state_t state;
	bool fatal_error_occured;
	osi_thread_t *read_thread;
	fsi_file_t *fd;
	osi_bin_sem_t *thread_sem;
	uint64_t size;
} source_file_ts_private_t;

/*
typedef struct source_file_ts_product
{
	util_log_t *log;
	uint64_t product_id;
	osi_mutex_t *sync;
	source_state_t state;
} source_file_ts_product_t;
*/

typedef struct source_file_ts_handle
{
	bool original;
	uint64_t product_id;
	source_file_ts_shared_t shared;
	source_file_ts_private_t *private;
} source_file_ts_handle_t;

// *************************************
// *            Prototypes             *
// *************************************

static eos_error_t source_file_ts_init (source_t* source);
static eos_error_t source_file_ts_deinit (source_t* source);

static const char* source_file_ts_name (void);
static eos_error_t source_file_ts_probe (char* uri);
static eos_error_t source_file_ts_prelock (source_t* source, char* uri);
static eos_error_t source_file_ts_lock (source_t* source, char* uri, char* extras, link_ev_hnd_t event_cb, void* event_cookie);
static eos_error_t source_file_ts_resume (source_t* source);
static eos_error_t source_file_ts_unlock (source_t* source);
static eos_error_t source_file_ts_suspend (source_t* source);
static eos_error_t source_file_ts_flush_buffers (source_t* source);
static eos_error_t source_file_ts_get_output_type (source_t* source, link_io_type_t* type);
static eos_error_t source_file_ts_get_capabilities (source_t* source, uint64_t* capabilities);
static eos_error_t source_file_ts_get_ctrl_funcs (link_handle_t link, link_cap_t cap, void** ctrl_funcs);
static eos_error_t source_file_ts_assign_output (source_t* source, link_io_t* next_link_io);
static void source_file_ts_handle_event(source_t* source, link_ev_t event,
		link_ev_data_t* data);

static eos_error_t source_file_ts_manufacture (source_t* model, uint64_t model_id, source_t** product, uint64_t product_id);
static eos_error_t source_file_ts_dismantle (uint64_t model_id, source_t** product);


static void source_file_ts_dispatch_event(source_t* source, link_ev_t event, void* event_param);


// *************************************
// *         Global variables          *
// *************************************

static source_t source_file_ts_model =
{
	.handle = NULL,

	.name = source_file_ts_name,
	.probe = source_file_ts_probe,
	.prelock = source_file_ts_prelock,
	.lock = source_file_ts_lock,
	.resume = source_file_ts_resume,
	.unlock = source_file_ts_unlock,
	.suspend = source_file_ts_suspend,
	.get_output_type = source_file_ts_get_output_type,
	.get_capabilities = source_file_ts_get_capabilities,
	.flush_buffers = source_file_ts_flush_buffers,
	.get_ctrl_funcs = source_file_ts_get_ctrl_funcs,
	.assign_output = source_file_ts_assign_output,
	.handle_event = source_file_ts_handle_event
};

static uint64_t source_file_ts_model_id = 0LL;

// *************************************
// *             Threads               *
// *************************************

void* source_file_ts_read_thread (void* arg)
{
	source_t *source = (source_t*)arg;
	source_file_ts_handle_t *handle = NULL;
	size_t size = 0;
	eos_error_t error = EOS_ERROR_OK;
	uint8_t *buff = NULL;
	link_io_t *output = NULL;
	uint32_t failed_operations = 0;
	bool eof = false;
	bool result = true;
	uint64_t total_data_read = 0;
	eos_media_desc_t desc;
	uint32_t i = 0;
	uint8_t packet[188*7] = {0};
	util_tsparser_t *tsparser = NULL;
	link_ev_data_t ev_data;

	EOS_UNUSED(result)

	UTIL_GLOGI("Read thread ...");
	if (source == NULL)
	{
		UTIL_GLOGE("Invalid argument");
		UTIL_GLOGE("Read thread [Failure]");
		return NULL;
	}

	handle = (source_file_ts_handle_t*)source->handle;
	if (handle == NULL)
	{
		UTIL_GLOGE("Source is not initialized");
		UTIL_GLOGE("Read thread [Failure]");
		return NULL;
	}

	UTIL_GLOGI("<ID:0x%llX> Read thread ...", handle->product_id);
	osi_memset(&ev_data, 0, sizeof(link_ev_data_t));
	if (handle->private == NULL)
	{
		UTIL_GLOGE("<ID:0x%llX> Source is not locked", handle->product_id);
		UTIL_GLOGE("<ID:0x%llX> Read thread [Failure]", handle->product_id);
		return NULL;
	}

	if (handle->private->state != SOURCE_STATE_STARTING)
	{
		UTIL_GLOGE("<ID:0x%llX> Source is in invalid state", handle->product_id);
		UTIL_GLOGE("<ID:0x%llX> Read thread [Failure]", handle->product_id);
		ev_data.conn_info.reason = LINK_CONN_ERR_NONE;
		source_file_ts_dispatch_event(source, LINK_EV_NO_CONNECT, &ev_data);
		return NULL;
	}

	osi_memset(&desc, 0, sizeof(eos_media_desc_t));
	if (util_tsparser_create(&tsparser) != EOS_ERROR_OK)
	{
	}
	for (i = 0; i < 3000; i++)
	{
		size = sizeof(packet);
		error = fsi_file_read(handle->private->fd, packet, &size);
		if (error != EOS_ERROR_OK)
		{
			UTIL_GLOGE("<ID:0x%llX> Cannot read %d", handle->product_id);
			break;
		}
		error = util_tsparser_get_media_info(tsparser, packet, sizeof(packet), INFO_ID_FIRST_FOUND, &desc);
		if (error == EOS_ERROR_OK)
		{
			break;
		}
	}
	util_tsparser_destroy(&tsparser);

	if (desc.es_cnt == 0)
	{
		UTIL_GLOGE("<ID:0x%llX> Invalid TS (no PMT)", handle->product_id);
		CHECK_AND_SET(handle->shared.cas, result, handle->private->state, SOURCE_STATE_STOPPING, true);
	}

	desc.container = EOS_MEDIA_CONT_MPEGTS;

	if (handle->private->state == SOURCE_STATE_STOPPING)
	{
		UTIL_GLOGE("<ID:0x%llX> Read thread [Failure]", handle->product_id);
		ev_data.conn_info.reason = LINK_CONN_ERR_READ;
		source_file_ts_dispatch_event(source, LINK_EV_NO_CONNECT, &ev_data);
		return NULL;
	}
	ev_data.conn_info.media = desc;
	ev_data.conn_info.reason = LINK_CONN_ERR_NONE;
	source_file_ts_dispatch_event(source, LINK_EV_CONNECTED, &ev_data);

	fsi_file_seek(handle->private->fd, 0, F_S_BEG);
	CHECK_AND_SET(handle->shared.cas, result, handle->private->state, SOURCE_STATE_SUSPENDED, ((error == EOS_ERROR_OK) && (handle->private->state == SOURCE_STATE_STARTING)));
	error = osi_bin_sem_take(handle->private->thread_sem);
	if (error != EOS_ERROR_OK)
	{
		if (error == EOS_ERROR_TIMEDOUT)
		{
			UTIL_LOGE(handle->private->log, "<ID:0x%llX> Read thread was not started for %.2f seconds", handle->product_id, START_WAIT_TIMEOUT / 1000.0);
		}
		else
		{
			UTIL_LOGE(handle->private->log, "<ID:0x%llX> Read thread semaphore failed", handle->product_id);
		}
		handle->private->fatal_error_occured = true;
		CHECK_AND_SET(handle->shared.cas, result, handle->private->state, SOURCE_STATE_STOPPING, true);
		ev_data.conn_info.reason = LINK_CONN_ERR_READ;
		source_file_ts_dispatch_event(source, LINK_EV_CONN_LOST, &ev_data);
		UTIL_LOGE(handle->private->log, "<ID:0x%llX> Read thread [Failure]", handle->product_id);
		return arg;
	}

	
	CHECK_AND_SET(handle->shared.cas, result, handle->private->state, SOURCE_STATE_STARTED, ((error == EOS_ERROR_OK) && (handle->private->state == SOURCE_STATE_SUSPENDED)));

	output = handle->private->output;

	while (handle->private->state == SOURCE_STATE_STARTED)
	{
		for (failed_operations = 0; failed_operations <= FAILED_ALLOCATIONS_COUNT; failed_operations++)
		{
			if (handle->private->state != SOURCE_STATE_STARTED)
			{
				failed_operations = FAILED_ALLOCATIONS_COUNT;
				break;
			}
			EOS_ASSERT(handle->private->size >= total_data_read)
			size = handle->private->size - total_data_read;
			size = ((size > READ_CHUNK_SIZE) ? READ_CHUNK_SIZE : size);
			if (output->allocate(output->handle, &buff, &size, NULL, FAILED_ALLOCATIONS_TIMEOUT, 0) != EOS_ERROR_OK)
			{
				if (failed_operations < FAILED_ALLOCATIONS_COUNT)
				{
					osi_time_usleep(OSI_TIME_MSEC_TO_USEC(FAILED_ALLOCATIONS_TIMEOUT));
					UTIL_LOGD(handle->private->log, "<ID:0x%llX> Unable to allocate output buffer", handle->product_id);
					continue;
				}
				else
				{
					UTIL_LOGE(handle->private->log, "<ID:0x%llX> Unable to allocate output buffer for %.2f seconds => Abort", handle->product_id, (FAILED_ALLOCATIONS_TIMEOUT * FAILED_ALLOCATIONS_COUNT) / 1000.0);
					handle->private->fatal_error_occured = true;
					CHECK_AND_SET(handle->shared.cas, result, handle->private->state, SOURCE_STATE_STOPPING, true);
					break;
				}

			}
			else
			{
				failed_operations = 0;
				break;
			}
		}
		if (failed_operations != 0)
		{
			continue;
		}

		for (failed_operations = 0; failed_operations <= FAILED_READS_COUNT; failed_operations++)
		{
			if (handle->private->state != SOURCE_STATE_STARTED)
			{
				failed_operations = FAILED_READS_COUNT;
				break;
			}
			error = fsi_file_read(handle->private->fd, buff, &size);
			if (error != EOS_ERROR_OK)
			{
				if (error == EOS_ERROR_EOF)
				{
					UTIL_LOGI(handle->private->log, "<ID:0x%llX> End of file reached", handle->product_id);
					CHECK_AND_SET(handle->shared.cas, result, handle->private->state, SOURCE_STATE_STOPPING, true);
					eof = true;
					break;
				}

				if (failed_operations < FAILED_READS_COUNT)
				{
					osi_time_usleep(OSI_TIME_MSEC_TO_USEC(FAILED_READS_TIMEOUT));
					UTIL_LOGD(handle->private->log, "<ID:0x%llX> Unable to read data %p %p %p", handle->product_id, handle->private->fd, buff, &size);
					continue;
				}
				else
				{
					UTIL_LOGE(handle->private->log, "<ID:0x%llX> Unable to retrieve data for %.2f seconds => Abort", handle->product_id, (FAILED_READS_TIMEOUT * FAILED_READS_COUNT) / 1000.0);
					handle->private->fatal_error_occured = true;
					CHECK_AND_SET(handle->shared.cas, result, handle->private->state, SOURCE_STATE_STOPPING, true);
					break;
				}
			}
			else
			{
				failed_operations = 0;
				break;
			}
		}
		if (failed_operations != 0)
		{
			continue;
		}

		total_data_read += size;
		for (failed_operations = 0; failed_operations <= FAILED_COMMITS_COUNT; failed_operations++)
		{
			if (handle->private->state != SOURCE_STATE_STARTED)
			{
				failed_operations = FAILED_COMMITS_COUNT;
				break;
			}
			if (output->commit(output->handle, &buff, size, NULL, FAILED_COMMITS_TIMEOUT, 0) != EOS_ERROR_OK)
			{
				if (failed_operations < FAILED_COMMITS_COUNT)
				{
					osi_time_usleep(OSI_TIME_MSEC_TO_USEC(FAILED_COMMITS_TIMEOUT));
					UTIL_LOGD(handle->private->log, "<ID:0x%llX> Unable to commit data", handle->product_id);
					continue;
				}
				else
				{
					UTIL_LOGE(handle->private->log, "<ID:0x%llX> Unable to commit read data for %.2f seconds => Abort", handle->product_id, (FAILED_COMMITS_TIMEOUT * FAILED_COMMITS_COUNT) / 1000.0);
					handle->private->fatal_error_occured = true;
					CHECK_AND_SET(handle->shared.cas, result, handle->private->state, SOURCE_STATE_STOPPING, true);
					break;
				}
			}
			else
			{
				buff = NULL;
				failed_operations = 0;
				break;
			}
		}
		if (failed_operations != 0)
		{
			continue;
		}

		if (handle->private->size - total_data_read == 0)
		{
			UTIL_LOGI(handle->private->log, "<ID:0x%llX> End of file reached", handle->product_id);
			CHECK_AND_SET(handle->shared.cas, result, handle->private->state, SOURCE_STATE_STOPPING, true);
			eof = true;
			break;
		}
	}

	if (buff != NULL)
	{
		UTIL_LOGW(handle->private->log, "Uncommited buffer detected => Try to release it (commit zero data)", handle->product_id);
		if (output->commit(output->handle, &buff, 0, NULL, FAILED_COMMITS_TIMEOUT, 0) != EOS_ERROR_OK)
		{
			UTIL_LOGW(handle->private->log, "Unable to commit", handle->product_id);
		}
	}

	if (handle->private->fatal_error_occured == true)
	{
		ev_data.conn_info.reason = LINK_CONN_ERR_WRITE;
		source_file_ts_dispatch_event(source, LINK_EV_CONN_LOST, &ev_data);
		UTIL_LOGE(handle->private->log, "<ID:0x%llX> Read thread [Failure]", handle->product_id);
		return arg;
	}

	if (eof == true)
	{
		ev_data.conn_info.reason = LINK_CONN_ERR_EOF;
		source_file_ts_dispatch_event(source, LINK_EV_CONN_LOST, &ev_data);
	}
	else
	{
		ev_data.conn_info.reason = LINK_CONN_ERR_NONE;
		source_file_ts_dispatch_event(source, LINK_EV_DISCONN, &ev_data);
	}

	UTIL_LOGI(handle->private->log, "<ID:0x%llX> Read thread [Success]", handle->product_id);
	return arg;
}

// *************************************
// *         Local functions           *
// *************************************

CALL_ON_LOAD(source_file_ts_register)
static void source_file_ts_register(void)
{
	osi_time_t timestamp = {0, 0};

	source_file_ts_init(&source_file_ts_model);

	if (source_file_ts_model_id == 0LL)
	{
		osi_time_usleep(4000); // Add randomnes to model_id
		osi_time_get_timestamp(&timestamp);
		source_file_ts_model_id = (timestamp.sec) * 1000000000LL + timestamp.nsec / 1;
	}

	source_factory_register_model(&source_file_ts_model, &source_file_ts_model_id,
			source_file_ts_manufacture, source_file_ts_dismantle);
}

CALL_ON_UNLOAD(source_file_ts_unregister)
static void source_file_ts_unregister(void)
{
	source_factory_unregister_model(&source_file_ts_model, source_file_ts_model_id);
	source_file_ts_deinit(&source_file_ts_model);
}


static void source_file_ts_dispatch_event(source_t* source, link_ev_t event, void* event_param)
{
	source_file_ts_handle_t *handle = NULL;
	
	// Since this is a local function assume that it will be used properly
	EOS_ASSERT(source != NULL)
	EOS_ASSERT(source->handle != NULL)

	handle = (source_file_ts_handle_t*)source->handle;

	EOS_ASSERT(handle->private != NULL)
	EOS_ASSERT(handle->private->event_cb != NULL)

	handle->private->event_cb(event, event_param, handle->private->event_cookie, handle->product_id);
}


static const char* source_file_ts_name (void)
{
	return SOURCE_NAME;
}

static eos_error_t source_file_ts_probe (char* uri)
{
	if (uri == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if (strstr(uri, FILE_TS_URI_PREFIX) == uri)
	{
		if (strncasecmp(&uri[strlen(uri) - strlen(FILE_TS_URI_SUFFIX)], FILE_TS_URI_SUFFIX, strlen(FILE_TS_URI_SUFFIX)) == 0)
		{
			return EOS_ERROR_OK;
		}
	}
	return EOS_ERROR_GENERAL;
}

static eos_error_t source_file_ts_init (source_t* source)
{
	source_file_ts_handle_t *handle = NULL;
	eos_error_t error = EOS_ERROR_OK;

	UTIL_GLOGI("Init ...");

	if (source == NULL)
	{
		UTIL_GLOGE("Invalid argument");
		UTIL_GLOGE("Init [Failure]");
		return EOS_ERROR_INVAL;
	}

	handle = (source_file_ts_handle_t*)osi_calloc(sizeof(source_file_ts_handle_t));
	if (handle == NULL)
	{
		UTIL_GLOGE("Memory allocation failed");
		UTIL_GLOGE("Init [Failure]");
		return EOS_ERROR_NOMEM;
	}

	error = osi_mutex_create(&handle->shared.lock_unlock);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Lock/Unlock mutex creation failed");
		UTIL_GLOGE("Init [Failure]");
		return error;
	}

	error = osi_mutex_create(&handle->shared.cas);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Check and set mutex creation failed");
		if (osi_mutex_destroy(&handle->shared.lock_unlock) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("Lock/Unlock mutex destruction failed");
		}
		UTIL_GLOGE("Init [Failure]");
		return error;
	}

	handle->original = true;
	handle->product_id = SOURCE_FACTORY_INV_PRODUCT_ID;
	source->handle = (source_handle_t)handle;
	UTIL_GLOGI("Init [Success]");
	return EOS_ERROR_OK;
}

static eos_error_t source_file_ts_deinit (source_t* source)
{
	source_file_ts_handle_t *handle = NULL;
	UTIL_GLOGI("Deinit ...");

	if (source == NULL)
	{
		UTIL_GLOGE("Invalid argument");
		UTIL_GLOGE("Deinit [Failure]");
		return EOS_ERROR_INVAL;
	}

	if (source->handle == NULL)
	{
		UTIL_GLOGE("Source is not initialized");
		UTIL_GLOGE("Deinit [Failure]");
		return EOS_ERROR_INVAL;
	}

	handle = (source_file_ts_handle_t*)source->handle;

	if (handle->private != NULL)
	{
		UTIL_GLOGW("Deinitializing locked source => Attempting unlock");
		if (source->unlock(source) != EOS_ERROR_OK)
		{
			UTIL_GLOGE("Unable to unlock source");
			UTIL_GLOGE("Deinit [Failure]");
			return EOS_ERROR_GENERAL;
		}
	}

	if (osi_mutex_destroy(&handle->shared.cas) != EOS_ERROR_OK)
	{
		UTIL_GLOGW("Unable to destroy check and set mutex");
	}

	if (osi_mutex_destroy(&handle->shared.lock_unlock) != EOS_ERROR_OK)
	{
		UTIL_GLOGW("Unable to destroy lock/unlock mutex");
	}

	osi_free(&source->handle);
	handle = NULL;
	
	UTIL_GLOGI("Deinit [Success]");
	return EOS_ERROR_OK;
}

static eos_error_t source_file_ts_prelock (source_t* source, char* uri)
{
	EOS_UNUSED(source)
	EOS_UNUSED(uri)
	return EOS_ERROR_NIMPLEMENTED;
}

static eos_error_t source_file_ts_lock (source_t* source, char* uri, char* extras, link_ev_hnd_t event_cb, void* event_cookie)
{
	source_file_ts_handle_t *handle = NULL;
	eos_error_t error = EOS_ERROR_OK;
	bool result = true;

	EOS_UNUSED(result)
	EOS_UNUSED(extras)
	UTIL_GLOGI("Lock ...");

	if ((uri == NULL) || (source == NULL) || (event_cookie == NULL))
	{
		UTIL_GLOGE("Invalid argument");
		UTIL_GLOGE("Lock [Failure]");
		return EOS_ERROR_INVAL;
	}

	if (source->handle == NULL)
	{
		UTIL_GLOGE("Source is not initialized");
		UTIL_GLOGE("Lock [Failure]");
		return EOS_ERROR_INVAL;
	}

	handle = (source_file_ts_handle_t*)source->handle;

	UTIL_GLOGI("<ID:0x%llX> Lock ...", handle->product_id);
	error = osi_mutex_lock(handle->shared.lock_unlock);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("<ID:0x%llX> Unable to lock", handle->product_id);
		UTIL_GLOGE("<ID:0x%llX> Lock [Failure]", handle->product_id);
		return error;
	}

	EOS_ASSERT(handle->private == NULL)
	if (handle->private != NULL)
	{
		UTIL_GLOGE("<ID:0x%llX> Locking already locked source", handle->product_id);
		if (osi_mutex_unlock(handle->shared.lock_unlock) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> Unlock failed", handle->product_id);
		}
		UTIL_GLOGE("<ID:0x%llX> Lock [Failure]", handle->product_id);
		return EOS_ERROR_GENERAL;
	}

	handle->private = (source_file_ts_private_t*)osi_calloc(sizeof(source_file_ts_private_t));
	EOS_ASSERT(handle->private != NULL)
	if (handle->private == NULL)
	{
		UTIL_GLOGE("<ID:0x%llX> Memory allocation failed", handle->product_id);
		if (osi_mutex_unlock(handle->shared.lock_unlock) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> Unlock failed", handle->product_id);
		}
		UTIL_GLOGE("<ID:0x%llX> Lock [Failure]", handle->product_id);
		return EOS_ERROR_NOMEM;
	}

	handle->private->event_cb = event_cb;
	handle->private->event_cookie = event_cookie;
	error = fsi_file_open(&handle->private->fd, &uri[strlen(FILE_TS_URI_PREFIX)], F_F_RO, 0);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("<ID:0x%llX> File RO open operation failed on %s", handle->product_id, &uri[strlen(FILE_TS_URI_PREFIX)]);
		osi_free((void**)&handle->private);
		if (osi_mutex_unlock(handle->shared.lock_unlock) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> Unlock failed", handle->product_id);
		}
		UTIL_GLOGE("<ID:0x%llX> Lock [Failure]", handle->product_id);
		return error;
	}

	error = fsi_file_size(handle->private->fd, &handle->private->size);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("<ID:0x%llX> File size calculation failed", handle->product_id);
		if (fsi_file_close(&handle->private->fd) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> File closing failed", handle->product_id);
		}
		osi_free((void**)&handle->private);
		if (osi_mutex_unlock(handle->shared.lock_unlock) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> Unlock failed", handle->product_id);
		}
		UTIL_GLOGE("<ID:0x%llX> Lock [Failure]", handle->product_id);
		return error;
	}

	error = osi_mutex_create(&handle->private->sync);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("<ID:0x%llX> Mutex creation failed", handle->product_id);
		if (fsi_file_close(&handle->private->fd) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> File closing failed", handle->product_id);
		}
		osi_free((void**)&handle->private);
		if (osi_mutex_unlock(handle->shared.lock_unlock) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> Unlock failed", handle->product_id);
		}
		UTIL_GLOGE("<ID:0x%llX> Lock [Failure]", handle->product_id);
		return error;
	}

	error = osi_bin_sem_create(&handle->private->thread_sem, false);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("<ID:0x%llX> Logger creation failed", handle->product_id);
		if (osi_mutex_destroy(&handle->private->sync) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> Unable to destroy mutex", handle->product_id);
		}
		if (fsi_file_close(&handle->private->fd) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> File closing failed", handle->product_id);
		}
		osi_free((void**)&handle->private);
		if (osi_mutex_unlock(handle->shared.lock_unlock) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> Unlock failed", handle->product_id);
		}
		UTIL_GLOGE("<ID:0x%llX> Lock [Failure]", handle->product_id);
		return error;
	}

	error = util_log_create(&handle->private->log, EOS_NAME);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("<ID:0x%llX> Logger creation failed", handle->product_id);
		if (osi_bin_sem_destroy(&handle->private->thread_sem) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> Unable to destroy semaphore", handle->product_id);
		}
		if (osi_mutex_destroy(&handle->private->sync) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> Unable to destroy mutex", handle->product_id);
		}
		if (fsi_file_close(&handle->private->fd) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> File closing failed", handle->product_id);
		}
		osi_free((void**)&handle->private);
		if (osi_mutex_unlock(handle->shared.lock_unlock) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> Unlock failed", handle->product_id);
		}
		UTIL_GLOGE("<ID:0x%llX> Lock [Failure]", handle->product_id);
		return error;
	}

	CHECK_AND_SET(handle->shared.cas, result, handle->private->state, SOURCE_STATE_STARTING, true);

	error = osi_thread_create(&handle->private->read_thread, NULL, source_file_ts_read_thread, (void*)source);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("<ID:0x%llX> Reader thread creation failed", handle->product_id);
		if (util_log_destroy(&handle->private->log) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> Unable to destroy logger", handle->product_id);
		}
		if (osi_bin_sem_destroy(&handle->private->thread_sem) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> Unable to destroy semaphore", handle->product_id);
		}
		if (osi_mutex_destroy(&handle->private->sync) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> Unable to destroy mutex", handle->product_id);
		}
		if (fsi_file_close(&handle->private->fd) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> File closing failed", handle->product_id);
		}
		osi_free((void**)&handle->private);
		if (osi_mutex_unlock(handle->shared.lock_unlock) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> Unlock failed", handle->product_id);
		}
		UTIL_GLOGE("<ID:0x%llX> Lock [Failure]", handle->product_id);
		return error;
	}

	UTIL_LOGI(handle->private->log, "<ID:0x%llX> File size: %llu bytes",
			handle->product_id, handle->private->size);

	UTIL_LOGI(handle->private->log, "<ID:0x%llX> Lock [Success]", handle->product_id);
	if (osi_mutex_unlock(handle->shared.lock_unlock) != EOS_ERROR_OK)
	{
		UTIL_GLOGW("<ID:0x%llX> Unlock failed", handle->product_id);
	}
	return EOS_ERROR_OK;
}

static eos_error_t source_file_ts_resume (source_t* source)
{
	source_file_ts_handle_t *handle = NULL;
	eos_error_t error = EOS_ERROR_OK;

	UTIL_GLOGI("Start ...");
	if (source == NULL)
	{
		UTIL_GLOGE("Invalid argument");
		UTIL_GLOGE("Start [Failure]");
		return EOS_ERROR_INVAL;
	}

	handle = (source_file_ts_handle_t*)source->handle;
	EOS_ASSERT(handle != NULL)
	if (handle == NULL)
	{
		UTIL_GLOGE("Source is not initialized");
		UTIL_GLOGE("Start [Failure]");
		return EOS_ERROR_INVAL;
	}
	
	UTIL_GLOGI("<ID:0x%llX> Start ...", handle->product_id);
	if (handle->private == NULL)
	{
		UTIL_GLOGE("<ID:0x%llX> Source is not locked", handle->product_id);
		UTIL_GLOGE("<ID:0x%llX> Start [Failure]", handle->product_id);
		return EOS_ERROR_GENERAL;
	}

	error = osi_mutex_lock(handle->private->sync);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("<ID:0x%llX> Unable to lock", handle->product_id);
		UTIL_GLOGE("<ID:0x%llX> Lock [Failure]", handle->product_id);
		return error;
	}

	if ((handle->private->state != SOURCE_STATE_STARTING) && (handle->private->state != SOURCE_STATE_SUSPENDED))
	{
		UTIL_GLOGE("<ID:0x%llX> Invalid source state", handle->product_id);
		if (osi_mutex_unlock(handle->private->sync) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> Unlock failed", handle->product_id);
		}
		UTIL_GLOGE("<ID:0x%llX> Start [Failure]", handle->product_id);
		return EOS_ERROR_GENERAL;
	}
	
	if (handle->private->output == NULL)
	{
		UTIL_GLOGE("<ID:0x%llX> Not connected to a next link", handle->product_id);
		if (osi_mutex_unlock(handle->private->sync) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> Unlock failed", handle->product_id);
		}
		UTIL_GLOGE("<ID:0x%llX> Start [Failure]", handle->product_id);
		return EOS_ERROR_INVAL;
	}

	if ((handle->private->output->allocate == NULL) || (handle->private->output->commit == NULL))
	{
		UTIL_GLOGE("<ID:0x%llX> Not properly connected to a next link", handle->product_id);
		if (osi_mutex_unlock(handle->private->sync) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> Unlock failed", handle->product_id);
		}
		UTIL_GLOGE("<ID:0x%llX> Start [Failure]", handle->product_id);
		return EOS_ERROR_INVAL;
	}

	error = osi_bin_sem_give(handle->private->thread_sem);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("<ID:0x%llX> Unable to release semaphore", handle->product_id);
		if (osi_mutex_unlock(handle->private->sync) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> Unlock failed", handle->product_id);
		}
		UTIL_GLOGE("<ID:0x%llX> Start [Failure]", handle->product_id);
		return error;
	}

	if (osi_mutex_unlock(handle->private->sync) != EOS_ERROR_OK)
	{
		UTIL_GLOGW("<ID:0x%llX> Sync mutex unlock failed", handle->product_id);
	}
	UTIL_GLOGI("<ID:0x%llX> Start [Success]", handle->product_id);
	return EOS_ERROR_OK;
}

static eos_error_t source_file_ts_unlock (source_t* source)
{
	source_file_ts_handle_t *handle = NULL;
	eos_error_t error = EOS_ERROR_OK;
	bool result = true;

	EOS_UNUSED(result)

	UTIL_GLOGI("Unlock ...");

	if (source == NULL)
	{
		UTIL_GLOGE("Invalid argument");
		UTIL_GLOGE("Unlock [Failure]");
		return EOS_ERROR_INVAL;
	}

	if (source->handle == NULL)
	{
		UTIL_GLOGE("Source is not initialized");
		UTIL_GLOGE("Unlock [Failure]");
		return EOS_ERROR_INVAL;
	}

	handle = (source_file_ts_handle_t*)source->handle;

	UTIL_GLOGI("<ID:0x%llX> Unlock ...", handle->product_id);
	error = osi_mutex_lock(handle->shared.lock_unlock);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("<ID:0x%llX> Unable to lock", handle->product_id);
		UTIL_GLOGE("<ID:0x%llX> Unlock [Failure]", handle->product_id);
		return error;
	}

	if (handle->private == NULL)
	{
		UTIL_GLOGW("<ID:0x%llX> Source is not running => Assume success", handle->product_id);
		if (osi_mutex_unlock(handle->shared.lock_unlock) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> Unlock failed", handle->product_id);
		}
		UTIL_GLOGI("<ID:0x%llX> Unlock [Success]", handle->product_id);
		return EOS_ERROR_OK;
	}

	if (fsi_file_close(&handle->private->fd) != EOS_ERROR_OK)
	{
		UTIL_GLOGW("<ID:0x%llX> File closing failed", handle->product_id);
	}

	CHECK_AND_SET(handle->shared.cas, result, handle->private->state, SOURCE_STATE_STOPPING, true);

	
	if (osi_bin_sem_give(handle->private->thread_sem) != EOS_ERROR_OK)
	{
		UTIL_GLOGW("<ID:0x%llX> Unable to release semaphore", handle->product_id);
	}

	osi_thread_join(handle->private->read_thread, NULL);
	osi_thread_release(&handle->private->read_thread);

	if (util_log_destroy(&handle->private->log) != EOS_ERROR_OK)
	{
		UTIL_GLOGW("<ID:0x%llX> Unable to destroy logger", handle->product_id);
	}

	if (osi_bin_sem_destroy(&handle->private->thread_sem) != EOS_ERROR_OK)
	{
		UTIL_GLOGW("<ID:0x%llX> Unable to destroy semaphore", handle->product_id);
	}

	if (osi_mutex_destroy(&handle->private->sync) != EOS_ERROR_OK)
	{
		UTIL_GLOGW("<ID:0x%llX> Unable to destroy mutex", handle->product_id);
	}

	osi_free((void**)&handle->private);
	handle->private = NULL;

	if (osi_mutex_unlock(handle->shared.lock_unlock) != EOS_ERROR_OK)
	{
		UTIL_GLOGW("<ID:0x%llX> Unlock failed", handle->product_id);
	}
	UTIL_GLOGI("<ID:0x%llX> Unlock [Success]", handle->product_id);
	return EOS_ERROR_OK;
}

static eos_error_t source_file_ts_suspend (source_t* source)
{
	EOS_UNUSED(source)
	source_file_ts_handle_t *handle = NULL;
	bool result = true;
	EOS_UNUSED(result)

	UTIL_GLOGI("Suspend ...");
	if (source == NULL)
	{
		UTIL_GLOGE("Invalid argument");
		UTIL_GLOGE("Suspend [Failure]");
		return EOS_ERROR_INVAL;
	}

	handle = (source_file_ts_handle_t*)source->handle;
	EOS_ASSERT(handle != NULL)
	if (handle == NULL)
	{
		UTIL_GLOGE("Source is not initialized");
		UTIL_GLOGE("Suspend [Failure]");
		return EOS_ERROR_INVAL;
	}
	
	CHECK_AND_SET(handle->shared.cas, result, handle->private->state, SOURCE_STATE_STOPPING, (handle->private != NULL));

	UTIL_GLOGI("Suspend [Success]");
	return EOS_ERROR_OK;
}

static eos_error_t source_file_ts_flush_buffers (source_t* source)
{
	EOS_UNUSED(source)
	return EOS_ERROR_NIMPLEMENTED;
}

static eos_error_t source_file_ts_get_output_type (source_t* source, link_io_type_t* type)
{
	if ((source  == NULL) || (type == NULL))
	{
		return EOS_ERROR_INVAL;
	}
	// TODO Change later. For now hardcoded
	*type = LINK_IO_TYPE_TS | LINK_IO_TYPE_SPROG_TS;
	return EOS_ERROR_OK;
}

static eos_error_t source_file_ts_get_capabilities (source_t* source, uint64_t* capabilities)
{
	EOS_UNUSED(source)
	EOS_UNUSED(capabilities)
	return EOS_ERROR_NIMPLEMENTED;
}

static eos_error_t source_file_ts_get_ctrl_funcs (link_handle_t link, link_cap_t cap, void** ctrl_funcs)
{
	EOS_UNUSED(link)
	EOS_UNUSED(cap)
	EOS_UNUSED(ctrl_funcs)
	return EOS_ERROR_NIMPLEMENTED;
}

static eos_error_t source_file_ts_assign_output (source_t* source, link_io_t* next_link_io)
{
	source_file_ts_handle_t *handle = NULL;
	eos_error_t error = EOS_ERROR_OK;

	UTIL_GLOGI("Connecting to a next link ...");
	if ((source == NULL) || (next_link_io == NULL))
	{
		UTIL_GLOGE("Invalid argument");
		UTIL_GLOGE("Connecting to a next link [Failure]");
		return EOS_ERROR_INVAL;
	}

	if ((next_link_io->allocate == NULL) || (next_link_io->commit == NULL))
	{
		UTIL_GLOGE("Invalid argument");
		UTIL_GLOGE("Connecting to a next link [Failure]");
		return EOS_ERROR_INVAL;
	}

	handle = (source_file_ts_handle_t*)source->handle;
	EOS_ASSERT(handle != NULL)
	if (handle == NULL)
	{
		UTIL_GLOGE("Source is not initialized");
		UTIL_GLOGE("Connecting to a next link [Failure]");
		return EOS_ERROR_INVAL;
	}
	
	UTIL_GLOGI("<ID:0x%llX> Connecting to a next link ...", handle->product_id);
	if (handle->private == NULL)
	{
		UTIL_GLOGE("<ID:0x%llX> Source is not locked", handle->product_id);
		UTIL_GLOGE("<ID:0x%llX> Connecting to a next link [Failure]", handle->product_id);
		return EOS_ERROR_GENERAL;
	}

	error = osi_mutex_lock(handle->private->sync);
	if (error != EOS_ERROR_OK)
	{
		UTIL_LOGE(handle->private->log, "<ID:0x%llX> Unable to lock", handle->product_id);
		UTIL_LOGE(handle->private->log, "<ID:0x%llX> Lock [Failure]", handle->product_id);
		return error;
	}

	if ((handle->private->state != SOURCE_STATE_STARTING) && (handle->private->state != SOURCE_STATE_SUSPENDED))
	{
		UTIL_LOGE(handle->private->log, "<ID:0x%llX> Invalid source state", handle->product_id);
		if (osi_mutex_unlock(handle->private->sync) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("<ID:0x%llX> Unlock failed", handle->product_id);
		}
		UTIL_LOGE(handle->private->log, "<ID:0x%llX> Connecting to a next link [Failure]", handle->product_id);
		return EOS_ERROR_GENERAL;
	}
	
	handle->private->output = next_link_io;

	if (osi_mutex_unlock(handle->private->sync) != EOS_ERROR_OK)
	{
		UTIL_LOGW(handle->private->log, "<ID:0x%llX> Unlock failed", handle->product_id);
	}
	UTIL_LOGI(handle->private->log, "<ID:0x%llX> Connecting to a next link [Success]");
	return EOS_ERROR_OK;
}

static void source_file_ts_handle_event(source_t* source, link_ev_t event,
		link_ev_data_t* data)
{
	EOS_UNUSED(data);
	source_file_ts_handle_t *handle = NULL;

	if(source == NULL)
	{
		return;
	}
	handle = (source_file_ts_handle_t*)source->handle;
	if(handle == NULL)
	{
		return;
	}

	switch (event)
	{
		case LINK_EV_FRAME_DISP:
			return;
		case LINK_EV_BOS:
			return;
		case LINK_EV_EOS:
			return;
		default:
			return;
	}
}

static eos_error_t source_file_ts_manufacture (source_t* model, uint64_t model_id, source_t** product, uint64_t product_id)
{
	source_file_ts_handle_t *handle = NULL;

	UTIL_GLOGI("Manufacture ...");
	if ((product == NULL) || (model == NULL) || (source_file_ts_model_id != model_id) || (product_id == SOURCE_FACTORY_INV_PRODUCT_ID))
	{
		UTIL_GLOGE("Invalid argument");
		UTIL_GLOGE("Manufacture [Failure]");
		return EOS_ERROR_INVAL;
	}

	if (*product != NULL)
	{
		UTIL_GLOGW("Passing initialized argument");
	}

	if (model->handle == NULL)
	{
		UTIL_GLOGE("Model is not set up properly");
		UTIL_GLOGE("Manufacture [Failure]");
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("Manufacturing product (ID:0x%llX)", product_id);
	handle = (source_file_ts_handle_t*)osi_calloc(sizeof(source_file_ts_handle_t));
	if (handle == NULL)
	{	
		UTIL_GLOGE("Memory allocation failed");
		UTIL_GLOGE("Manufacture [Failure]");
		return EOS_ERROR_NOMEM;
	}

	*product = (source_t*)osi_calloc(sizeof(source_t));
	if (*product == NULL)
	{
		UTIL_GLOGE("Memory allocation failed");
		UTIL_GLOGE("Manufacture [Failure]");
		osi_free((void**)&handle);
		return EOS_ERROR_NOMEM;
	}

	osi_memcpy(*product, model, sizeof(source_t));
	osi_memcpy(handle, model->handle, sizeof(source_file_ts_handle_t));
	handle->original = false;
	handle->product_id = product_id;
	(*product)->handle = (source_handle_t)handle;

	UTIL_GLOGI("Manufacture [Success]");

	return EOS_ERROR_OK;
}

static eos_error_t source_file_ts_dismantle (uint64_t model_id, source_t** product)
{
	source_file_ts_handle_t *handle = NULL;

	UTIL_GLOGI("Dismantle ...");
	if ((product == NULL) || (source_file_ts_model_id != model_id))
	{
		UTIL_GLOGE("Invalid argument");
		UTIL_GLOGE("Dismantle [Failure]");
		return EOS_ERROR_INVAL;
	}

	if (*product == NULL)
	{
		UTIL_GLOGE("Invalid argument");
		UTIL_GLOGE("Dismantle [Failure]");
		return EOS_ERROR_INVAL;
	}

	handle = (source_file_ts_handle_t*)(*product)->handle;
	EOS_ASSERT(handle != NULL)
	if (handle == NULL)
	{
		UTIL_GLOGE("Source is not initialized");
		UTIL_GLOGE("Dismantle [Failure]");
		return EOS_ERROR_INVAL;
	}

	UTIL_GLOGD("Dismantling product (ID:0x%llX)", handle->product_id);
	if (handle->private != NULL)
	{
		UTIL_GLOGW("Dismantling locked source => Attempting unlock");
		if ((*product)->unlock(*product) != EOS_ERROR_OK)
		{
			UTIL_GLOGE("Unable to unlock source");
			UTIL_GLOGE("Dismantle [Failure]");
			return EOS_ERROR_GENERAL;
		}
	}

	osi_free((void**)&(*product)->handle);
	handle = NULL;
	osi_free((void**)product);

	UTIL_GLOGI("Dismantle [Success]");
	return EOS_ERROR_OK;
}

// *************************************
// *       Global functions            *
// *************************************


