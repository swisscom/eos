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


#define MODULE_NAME "core:chain"

#include "chain.h"
#include "osi_sem.h"
#include "osi_mutex.h"
#include "osi_thread.h"
#include "util_msgq.h"
#include "source.h"
#include "sink.h"
#include "osi_memory.h"
#include "playback_ctrl_factory.h"
#include "data_mgr.h"
#include "eos_macro.h"
#include "util_log.h"

#include <stdbool.h>

#define CHAIN_EVENT_QUEUE_LEN (20)

typedef struct chain_msg
{
	link_ev_t event;
	link_ev_data_t data;
} chain_msg_t;

struct chain
{
	uint32_t id;
	osi_sem_t *sem;
	osi_mutex_t *lock;
	source_t *source;
	sink_t *sink;
	playback_ctrl_t playback;
	data_mgr_t *data_mgr;
	int64_t pts;
	uint32_t position;
	chain_event_cbk_t cbk;
	void* cookie;
	chain_data_cbk_t data_cbk;
	void* data_cookie;
	eos_state_t state;
	util_msgq_t *event_queue;
	osi_thread_t *event_thread;
	util_log_t *log;
	eos_media_desc_t streams;
	bool connected;
	bool playing;
};

static void chain_event_hnd(link_ev_t event, link_ev_data_t* data,
		void* cookie, uint64_t link_id);
static void* chain_event_thread(void* arg);
static eos_error_t chain_process_data (void* cookie, engine_type_t engine_type,
                           engine_data_t data_type, uint8_t* data,
						   uint32_t size);

eos_error_t chain_create(chain_t** chain, uint32_t id,
		chain_handler_t* handler)
{
	eos_error_t error = EOS_ERROR_OK;
	osi_thread_attr_t attr = {OSI_THREAD_JOINABLE};

	if (chain == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	*chain = (chain_t*)osi_calloc(sizeof(chain_t));
	if (*chain == NULL)
	{
		return EOS_ERROR_NOMEM;
	}
	error = util_log_create(&(*chain)->log, "eos");
	if (error != EOS_ERROR_OK)
	{
		goto done;
	}
	error = osi_sem_create(&(*chain)->sem, 0, 1);
	if (error != EOS_ERROR_OK)
	{
		goto done;
	}
	error = osi_mutex_create(&(*chain)->lock);
	if (error != EOS_ERROR_OK)
	{
		goto done;
	}
	/* TODO: add free message callback... */
	error = util_msgq_create(&(*chain)->event_queue, CHAIN_EVENT_QUEUE_LEN,
			NULL);
	if (error != EOS_ERROR_OK)
	{
		goto done;
	}
	error = osi_thread_create(&(*chain)->event_thread, &attr,
			chain_event_thread, *chain);
	if (error != EOS_ERROR_OK)
	{
		goto done;
	}
	(*chain)->id = id;
	(*chain)->cbk = handler->event_cbk;
	(*chain)->cookie = handler->event_cookie;
	(*chain)->data_cbk = handler->data_cbk;
	(*chain)->data_cookie = handler->data_cookie;
	(*chain)->state = EOS_STATE_STOPPED;
done:
	if(error != EOS_ERROR_OK)
	{
		if((*chain)->log)
		{
			util_log_destroy(&(*chain)->log);
		}
		if((*chain)->event_queue)
		{
			util_msgq_destroy(&(*chain)->event_queue);
		}
		if((*chain)->sem != NULL)
		{
			osi_sem_destroy(&(*chain)->sem);
		}
		osi_free((void**)chain);
	}
	return error;
}

eos_error_t chain_destroy(chain_t** chain)
{
	UTIL_GLOGI("Chain destroy ...");
	if (chain == NULL)
	{
		UTIL_GLOGE("Chain destroy [Failure]");
		return EOS_ERROR_INVAL;
	}

	if (*chain == NULL)
	{
		UTIL_GLOGE("Chain destroy [Failure]");
		return EOS_ERROR_INVAL;
	}


	if ((*chain)->event_queue)
	{
		util_msgq_pause((*chain)->event_queue);

		if (osi_thread_join((*chain)->event_thread, NULL) != EOS_ERROR_OK)
		{

		}
		if (osi_thread_release(&(*chain)->event_thread) != EOS_ERROR_OK)
		{

		}
		if (util_msgq_destroy(&(*chain)->event_queue) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("Unable to destroy queue");
		}
		else
		{
			UTIL_GLOGI("Destroyed queue");
		}
	}

	if (osi_mutex_destroy(&(*chain)->lock) != EOS_ERROR_OK)
	{

	}

	if (osi_sem_destroy(&(*chain)->sem) != EOS_ERROR_OK)
	{

	}
	if ((*chain)->log)
	{
		util_log_destroy(&(*chain)->log);
	}
	osi_free((void**)chain);

	UTIL_GLOGI("Chain destroy [Success]");
	return EOS_ERROR_OK;
}

eos_error_t chain_set_event_cbk(chain_t* chain, chain_event_cbk_t cbk, void* cookie)
{
	eos_error_t err = EOS_ERROR_OK;

	if (chain == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	err = osi_mutex_lock(chain->lock);
	if (err != EOS_ERROR_OK)
	{
		return err;
	}
	chain->cbk = cbk;
	chain->cookie = cookie;

	return osi_mutex_unlock(chain->lock);
}

eos_error_t chain_set_data_cbk(chain_t* chain, chain_data_cbk_t cbk,
		void* cookie)
{
	eos_error_t err = EOS_ERROR_OK;

	if (chain == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	err = osi_mutex_lock(chain->lock);
	if (err != EOS_ERROR_OK)
	{
		return err;
	}
	chain->data_cbk = cbk;
	chain->data_cookie = cookie;

	return osi_mutex_unlock(chain->lock);
}

eos_error_t chain_get_id(chain_t* chain, uint32_t* id)
{
	eos_error_t err = EOS_ERROR_OK;

	if (chain == NULL || id == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	err = osi_mutex_lock(chain->lock);
	if (err != EOS_ERROR_OK)
	{
		return err;
	}
	*id = chain->id;

	return osi_mutex_unlock(chain->lock);
}

eos_error_t chain_set_source(chain_t* chain, source_t* source)
{
	eos_error_t error = EOS_ERROR_OK;
	if (chain == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	error = osi_mutex_lock(chain->lock);
	if (error != EOS_ERROR_OK)
	{
		return error;
	}
	if ((source == NULL) && (chain->connected == true))
	{
		osi_mutex_unlock(chain->lock);
		return EOS_ERROR_GENERAL;
	}
	chain->source = source;

	return osi_mutex_unlock(chain->lock);
}

eos_error_t chain_interrupt(chain_t* chain)
{
	eos_error_t error = EOS_ERROR_OK;
	source_t *source = NULL;

	if (chain == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	error = osi_mutex_lock(chain->lock);
	if (error != EOS_ERROR_OK)
	{
		return error;
	}
	if (chain->source == NULL)
	{
		osi_mutex_unlock(chain->lock);
		return EOS_ERROR_INVAL;
	}

	source = chain->source;

	osi_mutex_unlock(chain->lock);
	return source->suspend(source);
}

eos_error_t chain_lock(chain_t* chain, char* url, char* extras,
		osi_time_t timeout)
{
	eos_error_t error = EOS_ERROR_OK;
	osi_time_t current_time = {0, 0};
	osi_time_t lock_timeout = {0, 0};
	source_t *source = NULL;

	if ((chain == NULL) || (url == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	error = osi_mutex_lock(chain->lock);
	if (error != EOS_ERROR_OK)
	{
		return error;
	}
	if (chain->source == NULL)
	{
		osi_mutex_unlock(chain->lock);
		return EOS_ERROR_INVAL;
	}

	if (chain->connected)
	{
		UTIL_LOGE(chain->log, "Source already connected!");
		osi_mutex_unlock(chain->lock);
		return EOS_ERROR_INVAL;
	}

	source = chain->source;

	osi_mutex_unlock(chain->lock);
	error = source->lock(source, url, extras, chain_event_hnd, (void*)chain);

	if (error != EOS_ERROR_OK)
	{
		return error;
	}
	osi_time_get_time(&current_time);
	osi_time_add(&current_time, &timeout, &lock_timeout);
	error = osi_sem_timedwait(chain->sem, &lock_timeout);
	if (osi_mutex_lock(chain->lock) != EOS_ERROR_OK)
	{
		return EOS_ERROR_GENERAL;
	}

	if (error == EOS_ERROR_OK)
	{
		if (chain->connected)
		{
			osi_mutex_unlock(chain->lock);
			return EOS_ERROR_OK;
		}
		else
		{
			osi_mutex_unlock(chain->lock);
			source->unlock(source);
			return EOS_ERROR_GENERAL;
		}
	}
	osi_mutex_unlock(chain->lock);
	source->unlock(source);
	return EOS_ERROR_TIMEDOUT;
}

eos_error_t chain_unlock(chain_t* chain)
{
	eos_error_t error = EOS_ERROR_OK;
	source_t *source = NULL;

	if (chain == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	error = osi_mutex_lock(chain->lock);
	if (error != EOS_ERROR_OK)
	{
		return error;
	}
	if (chain->source == NULL)
	{
		osi_mutex_unlock(chain->lock);
		return EOS_ERROR_INVAL;
	}
	source = chain->source;
	chain->connected = false;
	chain->playing = false;

	osi_mutex_unlock(chain->lock);
	return source->unlock(source);
}

eos_error_t chain_set_track(chain_t* chain, uint32_t id, bool on)
{
	eos_error_t err = EOS_ERROR_OK;

	if (chain == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	err = osi_mutex_lock(chain->lock);
	if (err != EOS_ERROR_OK)
	{
		return err;
	}

	if (chain->sink == NULL)
	{
		osi_mutex_unlock(chain->lock);
		return EOS_ERROR_GENERAL;
	}

	if (chain->playback.select == NULL)
	{
		return EOS_ERROR_GENERAL;
	}
	err = chain->playback.select(&chain->playback, chain, &chain->streams, id, on);
	if (err == EOS_ERROR_OK)
	{
		osi_mutex_unlock(chain->lock);
		return err; 
	}
	err = data_mgr_set(chain->data_mgr, chain, &chain->streams, id, on);
	if (err == EOS_ERROR_OK)
	{
		osi_mutex_unlock(chain->lock);
		return err; 
	}
	osi_mutex_unlock(chain->lock);

	return err;
}

eos_error_t chain_set_sink(chain_t* chain, sink_t* sink)
{
	eos_error_t error = EOS_ERROR_OK;

	UTIL_GLOGI("Set sink");
	if (chain == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	error = osi_mutex_lock(chain->lock);
	if (error != EOS_ERROR_OK)
	{
		return error;
	}
	
	if ((chain->source != NULL) && (sink == NULL))
	{
		UTIL_GLOGE("Disconnecting sink from soruce still not supported");
		osi_mutex_unlock(chain->lock);
		return EOS_ERROR_GENERAL;
	}

	if (sink == NULL)
	{
		if (chain->sink != NULL)
		{
			UTIL_GLOGD("Unregister ev hnd");
			chain->sink->command.reg_ev_hnd(chain->sink, NULL, NULL);
		}
		chain->sink = NULL;
		osi_mutex_unlock(chain->lock);
		UTIL_GLOGD("Sink unset");
		return EOS_ERROR_OK;
	}

	if (chain->source == NULL)
	{
		UTIL_GLOGE("Assigning sink to chain without source "
				"still not supported");
		osi_mutex_unlock(chain->lock);
		return EOS_ERROR_GENERAL;
	}

	//TODO Add check if current sink is in running state

	error = chain->source->assign_output(chain->source, &sink->plug);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Source output assignment failed");
		osi_mutex_unlock(chain->lock);
		return error;
	}
	chain->sink = sink;
	error = chain->sink->command.reg_ev_hnd(chain->sink,
				chain_event_hnd, chain);
	if (error != EOS_ERROR_OK)
	{
		UTIL_LOGE(chain->log, "Failed to set sink event callback!");
		osi_mutex_unlock(chain->lock);
		return error;
	}

	osi_mutex_unlock(chain->lock);
	return EOS_ERROR_OK;
}

eos_error_t chain_load_playback_controler(chain_t* chain)
{
	UTIL_GLOGW("Load playback controler");

	if (playback_ctrl_factory_assign(chain, &chain->playback) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Loading playback controler failed");
	}
	UTIL_GLOGI("Loading playback controler succeded");

	return EOS_ERROR_OK;
}

eos_error_t chain_start(chain_t* chain)
{
	// TODO PRINT
	eos_error_t error = EOS_ERROR_OK;

	if (chain == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if (chain->playback.start == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	chain_load_data_mgr(chain);
	error = chain->playback.start(&chain->playback, chain, &chain->streams);
	if (error != EOS_ERROR_OK)
	{
		//TODO PRINT
	}
	else
	{
		//TODO PRINT
	}

	return error;
}

eos_error_t chain_trickplay(chain_t* chain, int32_t position, int16_t speed)
{
	eos_error_t error = EOS_ERROR_OK;

	UTIL_GLOGI("Trickplay");
	if (chain == NULL)
	{
		UTIL_GLOGE("Trickplay: Chain is NULL");

		return EOS_ERROR_INVAL;
	}

	if (chain->playback.trickplay == NULL)
	{
		UTIL_LOGE(chain->log, "Trickplay: Not supported");

		return EOS_ERROR_INVAL;
	}

	error = chain->playback.trickplay(&chain->playback, chain,
			position, speed);
	UTIL_LOGI(chain->log, "Trickplay exited");

	return error;
}

eos_error_t chain_start_buff(chain_t* chain)
{
	eos_error_t error = EOS_ERROR_OK;

	if (chain == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	error = chain->playback.start_buffering(&chain->playback, chain);

	return error;
}

eos_error_t chain_stop_buff(chain_t* chain)
{
	eos_error_t error = EOS_ERROR_OK;

	if (chain == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	error = chain->playback.stop_buffering(&chain->playback, chain);

	return error;
}

eos_error_t chain_stop(chain_t* chain)
{
	eos_error_t error = EOS_ERROR_OK;

	if (chain == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_LOGI(chain->log, "Stopping chain...");
	if (chain->playback.stop == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	// TODO Error handling
	osi_mutex_lock(chain->lock);
	chain->connected = false;
	chain->playing = false;
	osi_mutex_unlock(chain->lock);

	chain_unload_data_mgr(chain);
	error = chain->playback.stop(&chain->playback, chain);
	if (error != EOS_ERROR_OK)
	{
		UTIL_LOGE(chain->log, "Stopping chain: %d [Failure]", error);
	}
	else
	{
		UTIL_LOGI(chain->log, "Stopping chain [Success]");
	}
	util_msgq_flush(chain->event_queue);

	return error;
}

eos_error_t chain_get_source(chain_t* chain, source_t** source)
{
	if (chain == NULL || source == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	*source = chain->source;

	return EOS_ERROR_OK;
}

eos_error_t chain_get_streams(chain_t* chain, eos_media_desc_t* streams)
{
	eos_error_t err = EOS_ERROR_OK;

	if ((chain == NULL) || (streams == NULL))
	{
		return EOS_ERROR_INVAL;
	}
	err = osi_mutex_lock(chain->lock);
	if (err != EOS_ERROR_OK)
	{
		return err;
	}
	osi_memcpy(streams, &chain->streams, sizeof(eos_media_desc_t));

	return osi_mutex_unlock(chain->lock);
}

eos_error_t chain_get_stream_data(chain_t* chain, eos_media_codec_t codec,
		uint32_t id, eos_media_data_t **data)
{
	if (chain == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if (chain->data_mgr != NULL)
	{
		return data_mgr_poll(chain->data_mgr, codec, id, data);
	}
	else
	{
		UTIL_LOGW(chain->log, "Data manager is NULL?!?");
	}

	return EOS_ERROR_OK;
}

eos_error_t chain_load_data_mgr(chain_t* chain)
{
	eos_error_t error = EOS_ERROR_OK;
	engine_cb_data_t engine_cb;

	if (chain == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if (chain->data_mgr != NULL)
	{
		UTIL_LOGW(chain->log, "Data manager is not NULL");
		data_mgr_stop(chain->data_mgr);
		data_mgr_destroy(&chain->data_mgr);
	}
	engine_cb.cookie = (void*)chain;
	engine_cb.func = chain_process_data;
	error = data_mgr_create(&chain->data_mgr, engine_cb);
	if (error != EOS_ERROR_OK)
	{
		UTIL_LOGE(chain->log, "Cannot create data manager");
		return error;
	}
	return data_mgr_start(chain->data_mgr, chain, &chain->streams);
}

eos_error_t chain_unload_data_mgr(chain_t* chain)
{
	if (chain == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if (chain->data_mgr != NULL)
	{
		osi_mutex_lock(chain->lock);
		data_mgr_stop(chain->data_mgr);
		data_mgr_destroy(&chain->data_mgr);
		osi_mutex_unlock(chain->lock);
	}
	else
	{
		UTIL_LOGW(chain->log, "Data manager is NULL?!?");
	}

	return EOS_ERROR_OK;
}

eos_error_t chain_get_data_mgr(chain_t* chain, data_mgr_t** data_mgr)
{
	if (chain == NULL || data_mgr == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	*data_mgr = chain->data_mgr;

	return EOS_ERROR_OK;
}

eos_error_t chain_get_sink(chain_t* chain, sink_t** sink)
{
	if (chain == NULL || sink == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	*sink = chain->sink;

	return EOS_ERROR_OK;
}

static void chain_event_hnd(link_ev_t event, link_ev_data_t* data,
		void* cookie, uint64_t link_id)
{
	chain_t *chain = NULL;
	chain_msg_t *msg = NULL;
	size_t sz = sizeof(chain_msg_t);

	EOS_UNUSED(link_id);
	if(cookie == NULL)
	{
		UTIL_LOGW(chain->log, "Ignoring event: chain is NULL");
		return;
	}
	chain = (chain_t*) cookie;
	msg = osi_calloc(sz);
	if(msg == NULL)
	{
		UTIL_LOGE(chain->log, "Ignoring event: OOM on message alloc");
		return;
	}
	msg->event = event;
	if (data != NULL)
	{
		osi_memcpy(&msg->data, data, sizeof(link_ev_data_t));
	}
	if(util_msgq_put(chain->event_queue, (void*)msg, sz, NULL)
			!= EOS_ERROR_OK)
	{
		UTIL_LOGE(chain->log, "Ignoring event: failed to put message!");
		osi_free((void**)&msg);
	}
}

static void* chain_event_thread(void* arg)
{
	chain_t *chain = (chain_t*) arg;
	chain_msg_t *msg;
	eos_error_t err = EOS_ERROR_OK;
	size_t sz = 0;
	eos_event_t event = EOS_EVENT_LAST;
	eos_event_data_t event_data;

	if(chain == NULL)
	{
		return NULL;
	}
	UTIL_LOGI(chain->log, "Chain event thread started...");
	while(util_msgq_get(chain->event_queue, (void**)&msg, &sz, NULL)
			== EOS_ERROR_OK)
	{
//		if(msg->event != LINK_EV_FRAME_DISP)
//			UTIL_LOGI(chain->log, "Event thread %d", msg->event);
		/* Invalidate event */
		event = EOS_EVENT_LAST;
		osi_memset(&event_data, 0, sizeof(eos_event_data_t));
		/* Handle internal state... */
		switch(msg->event)
		{
		case LINK_EV_CONNECTED:
			UTIL_LOGI(chain->log, "CONNECTED");
			eos_media_desc_t *streams = &msg->data.conn_info.media;
			osi_mutex_lock(chain->lock);
			if (streams != NULL)
			{
				osi_memcpy(&chain->streams, streams, sizeof(eos_media_desc_t));
				chain->connected = true;
			}
			else
			{
				osi_memset(&chain->streams, 0, sizeof(eos_media_desc_t));
				chain->connected = false;
			}
			osi_mutex_unlock(chain->lock);
			osi_sem_post(chain->sem);
			event = EOS_EVENT_CONN_STATE;
			event_data.conn.state = EOS_CONNECTED;
			event_data.conn.reason = EOS_CONN_USER;
			break;
		case LINK_EV_CONN_LOST:
			UTIL_LOGW(chain->log, "CONN LOST");
			osi_mutex_lock(chain->lock);
			osi_memset(&chain->streams, 0, sizeof(eos_media_desc_t));
			chain->connected = false;
			chain->playing = false;
			osi_mutex_unlock(chain->lock);
			event = EOS_EVENT_CONN_STATE;
			event_data.conn.state = EOS_DISCONNECTED;
			/* TODO check msg->data.conn_info.reason */
			event_data.conn.reason = EOS_CONN_RD_ERR;
			break;
		case LINK_EV_DISCONN:
			UTIL_LOGW(chain->log, "DISCONNECTED");
			osi_mutex_lock(chain->lock);
			osi_memset(&chain->streams, 0, sizeof(eos_media_desc_t));
			chain->connected = false;
			chain->playing = false;
			osi_mutex_unlock(chain->lock);
			event = EOS_EVENT_CONN_STATE;
			event_data.conn.state = EOS_DISCONNECTED;
			/* TODO check msg->data.conn_info.reason */
			event_data.conn.reason = EOS_CONN_USER;
			break;
		case LINK_EV_NO_CONNECT:
			UTIL_LOGW(chain->log, "NO CONNECTION");
			osi_mutex_lock(chain->lock);
			osi_memset(&chain->streams, 0, sizeof(eos_media_desc_t));
			chain->connected = false;
			osi_mutex_unlock(chain->lock);
			osi_sem_post(chain->sem);
			break;
		case LINK_EV_FRAME_DISP:
			osi_mutex_lock(chain->lock);
			if ((chain->playing != true) && (chain->connected == true))
			{
				chain->playing = true;
				event = EOS_EVENT_STATE;
				event_data.state.state = EOS_STATE_PLAYING;
				UTIL_LOGI(chain->log, "Playback started");
			}
			osi_mutex_unlock(chain->lock);
			break;
		default:
			break;
		}

		osi_mutex_lock(chain->lock);
		/* Inform other core modules about this event */
		if ((chain->playback.handle_event != NULL) && chain->connected && chain->playing)
		{
			err = chain->playback.handle_event(&chain->playback, chain,
					msg->event, &msg->data);
			if(err != EOS_ERROR_OK)
			{
				UTIL_LOGW(chain->log, "Playback controller event handler "
						"returned error %d", err);
			}
		}
		err = data_mgr_hande_event(chain->data_mgr, msg->event, &msg->data);
		if(err != EOS_ERROR_OK)
		{
			UTIL_LOGW(chain->log, "Data manager event handler "
					"returned error %d", err);
		}
		osi_mutex_unlock(chain->lock);

		/* Inform upper layer */
		if(chain->cbk != NULL)
		{
			/* Translate event, if is not done before */
			if(event == EOS_EVENT_LAST)
			{
				switch(msg->event)
				{
				case LINK_EV_LOW_WM:
					event = EOS_EVENT_PBK_STATUS;
					event_data.pbk_status = EOS_PBK_LOW_WM;
					break;
				case LINK_EV_HIGH_WM:
#if 0
					event = EOS_EVENT_PBK_STATUS;
					event_data.pbk_status = EOS_PBK_HIGH_WM;
#else
					UTIL_LOGD(chain->log, "Ignoring HIGH WM");
#endif
					break;
				case LINK_EV_BOS:
					UTIL_GLOGD("EOS BEGIN received!");
					event = EOS_EVENT_PBK_STATUS;
					event_data.pbk_status = EOS_PBK_BOS;
					break;
				case LINK_EV_EOS:
					UTIL_GLOGD("EOS END received!");
					event = EOS_EVENT_PBK_STATUS;
					event_data.pbk_status = EOS_PBK_EOS;
					break;
				case LINK_EV_PLAY_INFO:
					//TODO: do we send paused state
#if 0
					if(msg->data.play_info.speed)
					{
						UTIL_LOGW(chain->log, "Detected PAUSE in play info");
						event = EOS_EVENT_STATE;
						event_data.state.state = EOS_STATE_PAUSED;
						break;
					}
#endif
					event = EOS_EVENT_PLAY_INFO;
					event_data.play_info.position =
							msg->data.play_info.position;
					event_data.play_info.begin = msg->data.play_info.begin;
					event_data.play_info.speed = msg->data.play_info.speed;
					event_data.play_info.end = msg->data.play_info.end;
					break;
				case LINK_EV_FRAME_DISP:
					/* This one is handled internally */
					break;
				default:
					UTIL_LOGW(chain->log, "Ignoring UNKNOWN event #%d",
							msg->event);
					break;
				}
			}
			/* Check again weather we have correct translation */
			if(event == EOS_EVENT_LAST)
			{
				osi_free((void**)&msg);

				continue;
			}
//			UTIL_LOGW(chain->log, "Before callback %d", msg->event);
			err = chain->cbk(chain, event, &event_data, chain->cookie);
			if(err != EOS_ERROR_OK)
			{
				UTIL_LOGW(chain->log, "Chain message handling failed "
						"with error code %d", err);
			}
//			UTIL_LOGW(chain->log, "After callback %d", msg->event);
			osi_free((void**)&msg);
		}
	}
	UTIL_LOGI(chain->log, "Chain event thread exited...");

	return NULL;
}

static eos_error_t chain_process_data (void* cookie, engine_type_t engine_type,
                           engine_data_t data_type, uint8_t* data,
						   uint32_t size)
{
	chain_t *chain = (chain_t*)cookie;

	UTIL_LOGD(chain->log, "Chain data process callback...");
	if(chain->data_cbk != NULL)
	{
		return chain->data_cbk(chain->id, engine_type, data_type,
				data, size, chain->data_cookie);
	}
	UTIL_LOGD(chain->log, "Ignoring chain callback (NULL)");

	return EOS_ERROR_OK;
}
