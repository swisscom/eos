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


#include "data_mgr.h"
#include "eos_types.h"
#include "osi_mutex.h"
#include "osi_memory.h"
#include "util_log.h"
#include "util_slist.h"
#include "sink.h"
#include "engine_factory.h"
#define MODULE_NAME ("core:manager:data")
#include "util_log.h"

#include <stdlib.h>

struct data_mgr
{
	chain_t *chain;
	bool running;
	osi_mutex_t *lock;
	util_log_t *log;
	util_slist_t engines;
	engine_cb_data_t engine_cb;
};

typedef struct list_item
{
	uint32_t id;
	engine_t *engine;
} list_item_t;

static eos_error_t data_prov_start(data_mgr_t* data_mgr, chain_t* chain,
		eos_media_desc_t* media);
static eos_error_t data_prov_stop(data_mgr_t* data_mgr);
static eos_error_t data_prov_select(data_mgr_t* data_mgr,
		eos_media_desc_t *streams, uint32_t id, bool on);
static engine_api_t* find_api(data_mgr_t* data_mgr, engine_type_t type,
		engine_t **engine);

static bool engine_comparator(void* search_param, void* data_address)
{
	list_item_t *item = (list_item_t*)data_address;
	engine_type_t *type = (engine_type_t*) search_param;
	engine_api_t *api = NULL;
	bool found = false;
	eos_error_t error = EOS_ERROR_OK;

	if(search_param == NULL || data_address == NULL)
	{
		return false;
	}

	api = osi_calloc(sizeof(engine_api_t));
	error = item->engine->get_api(item->engine, api);
	if (error != EOS_ERROR_OK)
	{
		osi_free((void**)&api);
		return false;
	}
	if(api->type == *type)
	{
		found = true;
	}
	osi_free((void**)&api);

	return found;
}

eos_error_t data_mgr_create(data_mgr_t** data_mgr, engine_cb_data_t cb)
{
	data_mgr_t *tmp = NULL;
	eos_error_t err = EOS_ERROR_OK;

	if(data_mgr == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	tmp = osi_calloc(sizeof(struct data_mgr));
	if(tmp == NULL)
	{
		return EOS_ERROR_NOMEM;
	}
	if((err = osi_mutex_create(&tmp->lock)) != EOS_ERROR_OK)
	{
		osi_free((void**)&tmp);
		return err;
	}
	if((err = util_log_create(&tmp->log, "eos")) != EOS_ERROR_OK)
	{
		osi_mutex_destroy(&tmp->lock);
		osi_free((void**)&tmp);
		return err;
	}
	if ((err = util_slist_create(&tmp->engines, engine_comparator))
			!= EOS_ERROR_OK)
	{
		util_log_destroy(&tmp->log);
		osi_mutex_destroy(&tmp->lock);
		osi_free((void**)&tmp);
		return err;
	}

	tmp->engine_cb = cb;
	*data_mgr = tmp;

	return EOS_ERROR_OK;
}

eos_error_t data_mgr_destroy(data_mgr_t** data_mgr)
{
	if(data_mgr == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	osi_mutex_destroy(&(*data_mgr)->lock);
	util_slist_destroy(&(*data_mgr)->engines);
	util_log_destroy(&(*data_mgr)->log);
	osi_free((void**)data_mgr);

	return EOS_ERROR_OK;
}

static void set_streams(eos_media_desc_t *streams)
{
	uint8_t i = 0;

	for (i = 0; i < streams->es_cnt; i++)
	{
		if (EOS_MEDIA_IS_DAT(streams->es[i].codec))
		{
			switch (streams->es[i].codec)
			{
				case EOS_MEDIA_CODEC_TTXT:
#ifndef EOS_TTXT_MANUAL
					streams->es[i].selected = true;
#endif
					break;
				case EOS_MEDIA_CODEC_HBBTV:
#ifndef EOS_HBBTV_MANUAL
					streams->es[i].selected = true;
#endif
					break;
				case EOS_MEDIA_CODEC_DSMCC_C:
#ifndef EOS_DSMCC_MANUAL
					streams->es[i].selected = true;
#endif
					break;
				default:
					break;
			}
		}
	}
}


eos_error_t data_mgr_start(data_mgr_t* data_mgr, chain_t* chain, eos_media_desc_t *streams)
{
	eos_error_t err = EOS_ERROR_OK;
	uint8_t i = 0;
	sink_t *sink = NULL;
	engine_params_t params;
	engine_t *engine = NULL;
	list_item_t *item = NULL;
	link_cap_stream_prov_ctrl_t *stream_prov = NULL;
	engine_in_hook_t hook = NULL;

	if(data_mgr == NULL || chain == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_LOGI(data_mgr->log, "Starting...");
	err = chain_get_sink(chain, &sink);
	if(err != EOS_ERROR_OK)
	{
		return err;
	}

	set_streams(streams);
	err = data_prov_start(data_mgr, chain, streams);
	if(err != EOS_ERROR_OK)
	{
		UTIL_LOGE(data_mgr->log, "Data provider handling returned error!");
	}
	else
	{
		data_mgr->chain = chain;
		data_mgr->running = true;
	}

	if (!(sink->caps & LINK_CAP_STREAM_PROV))
	{
		UTIL_LOGI(data_mgr->log, "Skipping: no stream provider caps");
		return EOS_ERROR_OK;
	}
	data_mgr->running = false;
	err = sink->command.get_ctrl_funcs(sink, LINK_CAP_STREAM_PROV,
			(void**)&stream_prov);
	if (err != EOS_ERROR_OK)
	{
		UTIL_LOGE(data_mgr->log, "Failed to get control functions!");
		return err;
	}

	osi_memset(&params, 0, sizeof(engine_params_t));
	params.cb = data_mgr->engine_cb;
	osi_mutex_lock(data_mgr->lock);
	for (i = 0; i < streams->es_cnt; i++)
	{
		if (EOS_MEDIA_IS_DAT(streams->es[i].codec))
		{
			if (streams->es[i].selected == true)
			{
				params.codec = streams->es[i].codec;
				err = engine_factory_manufacture(&params, &engine);
				if (err == EOS_ERROR_OK)
				{
					item = (list_item_t*)osi_calloc(sizeof(list_item_t));
					if (item == NULL)
					{
						UTIL_LOGE(data_mgr->log,
								"Engine list item mem alloc failed");
						engine_factory_dismantle(&engine);
						break;
					}
					item->id = i;
					item->engine = engine;
					err = engine->get_hook(engine, &hook);
					if (err != EOS_ERROR_OK)
					{
						UTIL_LOGW(data_mgr->log, "Unable to get engine hook");
						osi_free((void*)&item);
						continue;
					}
					err = stream_prov->attach((link_handle_t)sink, i,
							    (link_stream_cb_t)hook,
							    (void*)engine);
					if (err != EOS_ERROR_OK)
					{
						UTIL_LOGW(data_mgr->log, "Unable to attach engine");
						osi_free((void*)&item);
						continue;
					}
					err = data_mgr->engines.add(data_mgr->engines, item);
					if (err != EOS_ERROR_OK)
					{
						UTIL_LOGW(data_mgr->log, "Unable to store engine");
						stream_prov->detach((link_handle_t)sink, i);
						osi_free((void*)&item);
						continue;
					}
					UTIL_LOGI(data_mgr->log, "Attached \"%s\" engine",
							engine->name());
				}
				engine = NULL;
			}
		}
	}

	data_mgr->chain = chain;
	data_mgr->running = true;
	osi_mutex_unlock(data_mgr->lock);

	return EOS_ERROR_OK;
}

static engine_type_t codec2engine(eos_media_codec_t codec)
{
	switch (codec)
	{
		case EOS_MEDIA_CODEC_TTXT:
			return ENGINE_TYPE_TTXT;
		case EOS_MEDIA_CODEC_HBBTV:
			return ENGINE_TYPE_HBBTV;
		case EOS_MEDIA_CODEC_DVB_SUB:
			return ENGINE_TYPE_DVB_SUB;
		case EOS_MEDIA_CODEC_DSMCC_C:
			return ENGINE_TYPE_DSMCC;
		default:
			return ENGINE_TYPE_INVALID;
	}
}

eos_error_t data_mgr_set(data_mgr_t* data_mgr, chain_t* chain,  
		eos_media_desc_t *streams, uint32_t id, bool on)
{
	sink_t *sink = NULL;
	link_cap_stream_prov_ctrl_t *stream_prov = NULL;
	uint8_t i = 0;
	eos_error_t error = EOS_ERROR_OK;
	engine_params_t params;
	engine_type_t engine_type = ENGINE_TYPE_INVALID;
	engine_t *engine = NULL;
	list_item_t *item = NULL;
	engine_in_hook_t hook = NULL;

	if ((data_mgr == NULL) || (chain == NULL) || (streams == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	UTIL_LOGI(data_mgr->log, "Setting track id #%u %s ...", id, on ? "on" : "off");
	if(data_prov_select(data_mgr, streams, id, on) == EOS_ERROR_OK)
	{
		return EOS_ERROR_OK;
	}

	error = chain_get_sink(chain, &sink);
	if (error != EOS_ERROR_OK)
	{
		return error;
	}

	error = sink->command.get_ctrl_funcs(sink, LINK_CAP_STREAM_PROV,
			(void**)&stream_prov);
	if (error != EOS_ERROR_OK)
	{
		UTIL_LOGE(data_mgr->log, "Failed to get control functions!");
		return error;
	}

	for (i = 0; i < streams->es_cnt; i++)
	{
		if (streams->es[i].id == id)
		{
			if (!EOS_MEDIA_IS_DAT(streams->es[i].codec))
			{
				return EOS_ERROR_INVAL;
			}
			if (streams->es[i].selected == on)
			{
				return EOS_ERROR_OK;
			}

			osi_mutex_lock(data_mgr->lock);
			engine_type = codec2engine(streams->es[i].codec);
			if (engine_type == ENGINE_TYPE_INVALID)
			{
				osi_mutex_unlock(data_mgr->lock);
				return EOS_ERROR_INVAL;
			}
			error = data_mgr->engines.get(data_mgr->engines, 
					(void*)&engine_type, (void**)&item);
			if (!on)
			{
					if (error != EOS_ERROR_OK)
					{
						osi_mutex_unlock(data_mgr->lock);
						return EOS_ERROR_INVAL;
					}
					UTIL_LOGI(data_mgr->log, "Detach \"%s\" engine", 
							item->engine->name());
					error = stream_prov->detach((link_handle_t)sink, i);
					if (error != EOS_ERROR_OK)
					{
						osi_mutex_unlock(data_mgr->lock);
						return EOS_ERROR_INVAL;
					}
					error = data_mgr->engines.remove(data_mgr->engines, 
							(void*)&engine_type);
					if (error != EOS_ERROR_OK)
					{
						osi_mutex_unlock(data_mgr->lock);
						return EOS_ERROR_INVAL;
					}
					
					error = engine_factory_dismantle(&item->engine);
					if (error != EOS_ERROR_OK)
					{
						UTIL_LOGW(data_mgr->log, "Unable to dismantle engine");
					}
					osi_free((void*)&item);
					streams->es[i].selected = on;
					osi_mutex_unlock(data_mgr->lock);
					return EOS_ERROR_OK;
			}
			if (error == EOS_ERROR_OK)
			{
				// Engine of the same type already attached
				UTIL_LOGE(data_mgr->log,
						"Disconnect currently connected engine");
				error = stream_prov->detach((link_handle_t)sink, item->id);
				if (error != EOS_ERROR_OK)
				{
					osi_mutex_unlock(data_mgr->lock);
					return EOS_ERROR_INVAL;
				}
				//TODO: dismantle?
				error = data_mgr->engines.remove(data_mgr->engines, 
						(void*)&engine_type);
				if (error != EOS_ERROR_OK)
				{
					osi_mutex_unlock(data_mgr->lock);
					return EOS_ERROR_INVAL;
				}
				streams->es[item->id].selected = false;
				UTIL_LOGE(data_mgr->log,
						"Engine for stream #%hhu is disconnected",
						item->id);
				osi_free((void*)&item);
			}
			osi_memset(&params, 0, sizeof(engine_params_t));
			params.cb = data_mgr->engine_cb;
			params.codec = streams->es[i].codec;

			error = engine_factory_manufacture(&params, &engine);
			if (error != EOS_ERROR_OK)
			{
				osi_mutex_unlock(data_mgr->lock);

				return EOS_ERROR_NFOUND;
			}
			item = (list_item_t*)osi_calloc(sizeof(list_item_t));
			if (item == NULL)
			{
				UTIL_LOGE(data_mgr->log,
						"Engine list item mem alloc failed");
				engine_factory_dismantle(&engine);
				osi_mutex_unlock(data_mgr->lock);
				return EOS_ERROR_NOMEM;
			}
			item->id = i;
			item->engine = engine;
			error = engine->get_hook(engine, &hook);
			if (error != EOS_ERROR_OK)
			{
				UTIL_LOGW(data_mgr->log, "Unable to get engine hook");
				osi_free((void*)&item);
				engine_factory_dismantle(&engine);
				osi_mutex_unlock(data_mgr->lock);
				return error;
			}
			error = stream_prov->attach((link_handle_t)sink, i,
						(link_stream_cb_t)hook,
						(void*)engine);
			if (error != EOS_ERROR_OK)
			{
				UTIL_LOGW(data_mgr->log, "Unable to attach engine");
				osi_free((void*)&item);
				engine_factory_dismantle(&engine);
				osi_mutex_unlock(data_mgr->lock);
				return error;
			}
			error = data_mgr->engines.add(data_mgr->engines, item);
			if (error != EOS_ERROR_OK)
			{
				UTIL_LOGW(data_mgr->log, "Unable to store engine");
				stream_prov->detach((link_handle_t)sink, i);
				osi_free((void*)&item);
				engine_factory_dismantle(&engine);
				osi_mutex_unlock(data_mgr->lock);
				return error;
			}
			UTIL_LOGI(data_mgr->log, "Attached \"%s\" engine",
					engine->name());
			streams->es[i].selected = on;

			osi_mutex_unlock(data_mgr->lock);
			UTIL_LOGI(data_mgr->log, "Setting track id #%u %s [Success]", id, on ? "on" : "off");
			return EOS_ERROR_OK;
		}
	}

	return EOS_ERROR_NFOUND;
}

eos_error_t data_mgr_stop(data_mgr_t* data_mgr)
{
	eos_error_t error = EOS_ERROR_OK;
	list_item_t *item = NULL;
	sink_t *sink = NULL;
	link_cap_stream_prov_ctrl_t *stream_prov = NULL;
	engine_api_t api;

	UTIL_GLOGI("Data manager stop ...");
	/* TODO: Anything smart to do here?*/
	if(data_mgr == NULL)
	{
		UTIL_GLOGE("Data manager stop [Failure]");
		return EOS_ERROR_INVAL;
	}
	data_prov_stop(data_mgr);
	//TODO Add error checks
	error = chain_get_sink(data_mgr->chain, &sink);
	if (sink == NULL)
	{
		UTIL_LOGE(data_mgr->log, "Unable to acquire sink handle");
		UTIL_LOGE(data_mgr->log, "Data manager stop [Failure]");
		return EOS_ERROR_GENERAL;
	}
	if (!(sink->caps & LINK_CAP_STREAM_PROV))
	{
		UTIL_LOGI(data_mgr->log, "Skipping");
		return EOS_ERROR_OK;
	}
	error = sink->command.get_ctrl_funcs(sink, LINK_CAP_STREAM_PROV,
			(void**)&stream_prov);
	if (error != EOS_ERROR_OK)
	{
		UTIL_LOGE(data_mgr->log, "Unable to acquire stream provider API");
		UTIL_LOGE(data_mgr->log, "Data manager stop [Failure]");
		return EOS_ERROR_GENERAL;
	}
	osi_mutex_lock(data_mgr->lock);
	data_mgr->running = false;
	data_mgr->engines.first(data_mgr->engines, (void**)&item);
	while (item != NULL)
	{
		error = stream_prov->detach((link_handle_t)sink, item->id);
		if (error != EOS_ERROR_OK)
		{
			UTIL_LOGW(data_mgr->log, "Unable to detach engine");
		}

		osi_memset(&api, 0, sizeof(engine_api_t));
		error = item->engine->get_api(item->engine, &api);
		if (error != EOS_ERROR_OK)
		{
			UTIL_LOGW(data_mgr->log, "Unable to get engine API");
		}
		else
		{
			error = data_mgr->engines.remove(data_mgr->engines, (void*)&api.type);
			if (error != EOS_ERROR_OK)
			{
				UTIL_LOGW(data_mgr->log, "Unable to remove engine");
			}
		}
		error = engine_factory_dismantle(&item->engine);
		if (error != EOS_ERROR_OK)
		{
			UTIL_LOGW(data_mgr->log, "Unable to dismantle engine");
		}
		osi_free((void**)&item);
		data_mgr->engines.first(data_mgr->engines, (void**)&item);
	}
	osi_mutex_unlock(data_mgr->lock);

	UTIL_LOGI(data_mgr->log, "Data manager stop [Success]");
	return error;
}

eos_error_t data_mgr_poll(data_mgr_t* data_mgr, eos_media_codec_t codec,
		uint32_t id, eos_media_data_t **data)
{
	sink_t *sink = NULL;
	eos_error_t err = EOS_ERROR_OK;
	link_cap_data_prov_t *data_prov = NULL;

	/* For now only teletext can be polled */
	if(data_mgr == NULL || data == NULL || codec != EOS_MEDIA_CODEC_TTXT)
	{
		return EOS_ERROR_INVAL;
	}
	osi_mutex_lock(data_mgr->lock);
	if(data_mgr->running == false)
	{
		osi_mutex_unlock(data_mgr->lock);
		return EOS_ERROR_PERM;
	}
	osi_mutex_unlock(data_mgr->lock);
	err = chain_get_sink(data_mgr->chain, &sink);
	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	err = sink->command.get_ctrl_funcs(sink, LINK_CAP_DATA_PROV,
			(void**)&data_prov);
	if(err != EOS_ERROR_OK || data_prov == NULL)
	{
		return err;
	}

	return data_prov->poll(sink, id, codec, data);
}

eos_error_t data_mgr_hande_event(data_mgr_t* data_mgr, link_ev_t event,
			link_ev_data_t* data)
{
	list_item_t *item = NULL;

	if(data_mgr == NULL)
	{
		UTIL_GLOGE("Data manager handle event NULL [Failure]");

		return EOS_ERROR_INVAL;
	}
	osi_mutex_lock(data_mgr->lock);
	data_mgr->engines.first(data_mgr->engines, (void**)&item);
	while (item != NULL)
	{
		if(item->engine->event_handler != NULL)
		{
			item->engine->event_handler(item->engine, event, data);
		}
		if(data_mgr->engines.next(data_mgr->engines, (void**)&item)
				!= EOS_ERROR_OK)
		{
			break;
		}
	}
	osi_mutex_unlock(data_mgr->lock);

	return EOS_ERROR_OK;
}

eos_error_t data_mgr_ttxt_enable(data_mgr_t* data_mgr, bool enable)
{
	engine_api_t *api = NULL;
	eos_error_t err = EOS_ERROR_OK;
	engine_t *engine = NULL;

	if(data_mgr == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	api = find_api(data_mgr, ENGINE_TYPE_DATA_PROV, &engine);
	if(api == NULL)
	{
		err = EOS_ERROR_NFOUND;
	}
	else
	{
		err = api->func.data_prov.ttxt_enable(engine, enable);
		osi_free((void**)&api);
	}

	return err;
}

eos_error_t data_mgr_ttxt_page_set(data_mgr_t* data_mgr, uint16_t page,
		uint16_t subpage)
{
	engine_api_t *api = NULL;
	eos_error_t err = EOS_ERROR_OK;
	engine_t *engine = NULL;

	if(data_mgr == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	api = find_api(data_mgr, ENGINE_TYPE_DATA_PROV, &engine);
	if(api == NULL)
	{
		err = EOS_ERROR_NFOUND;
	}
	else
	{
		err = api->func.data_prov.ttxt_page_set(engine, page, subpage);
		osi_free((void**)&api);
	}

	return err;
}

eos_error_t data_mgr_ttxt_page_get(data_mgr_t* data_mgr, uint16_t* page,
		uint16_t* subpage)
{
	engine_api_t *api = NULL;
	eos_error_t err = EOS_ERROR_OK;
	engine_t *engine = NULL;

	if(data_mgr == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	api = find_api(data_mgr, ENGINE_TYPE_DATA_PROV, &engine);
	if(api == NULL)
	{
		err = EOS_ERROR_NFOUND;
	}
	else
	{
		err = api->func.data_prov.ttxt_page_get(engine, page, subpage);
		osi_free((void**)&api);
	}

	return err;
}

eos_error_t data_mgr_ttxt_next_page_get(data_mgr_t* data_mgr, uint16_t* next)
{
	engine_api_t *api = NULL;
	eos_error_t err = EOS_ERROR_OK;
	engine_t *engine = NULL;

	if(data_mgr == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	api = find_api(data_mgr, ENGINE_TYPE_DATA_PROV, &engine);
	if(api == NULL)
	{
		err = EOS_ERROR_NFOUND;
	}
	else
	{
		err = api->func.data_prov.ttxt_next_page_get(engine, next);
		osi_free((void**)&api);
	}

	return err;
}

eos_error_t data_mgr_ttxt_prev_page_get(data_mgr_t* data_mgr,
		uint16_t* previous)
{
	engine_api_t *api = NULL;
	eos_error_t err = EOS_ERROR_OK;
	engine_t *engine = NULL;

	if(data_mgr == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	api = find_api(data_mgr, ENGINE_TYPE_DATA_PROV, &engine);
	if(api == NULL)
	{
		err = EOS_ERROR_NFOUND;
	}
	else
	{
		err = api->func.data_prov.ttxt_prev_page_get(engine, previous);
		osi_free((void**)&api);
	}

	return err;
}

eos_error_t data_mgr_ttxt_red_page_get(data_mgr_t* data_mgr,
		uint16_t* red)
{
	engine_api_t *api = NULL;
	eos_error_t err = EOS_ERROR_OK;
	engine_t *engine = NULL;

	if(data_mgr == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	api = find_api(data_mgr, ENGINE_TYPE_DATA_PROV, &engine);
	if(api == NULL)
	{
		err = EOS_ERROR_NFOUND;
	}
	else
	{
		err = api->func.data_prov.ttxt_red_page_get(engine, red);
		osi_free((void**)&api);
	}

	return err;
}

eos_error_t data_mgr_ttxt_green_page_get(data_mgr_t* data_mgr,
		uint16_t* green)
{
	engine_api_t *api = NULL;
	eos_error_t err = EOS_ERROR_OK;
	engine_t *engine = NULL;

	if(data_mgr == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	api = find_api(data_mgr, ENGINE_TYPE_DATA_PROV, &engine);
	if(api == NULL)
	{
		err = EOS_ERROR_NFOUND;
	}
	else
	{
		err = api->func.data_prov.ttxt_green_page_get(engine, green);
		osi_free((void**)&api);
	}

	return err;
}


eos_error_t data_mgr_ttxt_next_subpage_get(data_mgr_t* data_mgr, uint16_t* next)
{
	engine_api_t *api = NULL;
	eos_error_t err = EOS_ERROR_OK;
	engine_t *engine = NULL;

	if(data_mgr == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	api = find_api(data_mgr, ENGINE_TYPE_DATA_PROV, &engine);
	if(api == NULL)
	{
		err = EOS_ERROR_NFOUND;
	}
	else
	{
		err = api->func.data_prov.ttxt_next_subpage_get(engine, next);
		osi_free((void**)&api);
	}

	return err;
}

eos_error_t data_mgr_ttxt_prev_subpage_get(data_mgr_t* data_mgr,
		uint16_t* previous)
{
	engine_api_t *api = NULL;
	eos_error_t err = EOS_ERROR_OK;
	engine_t *engine = NULL;

	if(data_mgr == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	api = find_api(data_mgr, ENGINE_TYPE_DATA_PROV, &engine);
	if(api == NULL)
	{
		err = EOS_ERROR_NFOUND;
	}
	else
	{
		err = api->func.data_prov.ttxt_prev_subpage_get(engine, previous);
		osi_free((void**)&api);
	}

	return err;
}

eos_error_t data_mgr_ttxt_blue_page_get(data_mgr_t* data_mgr,
		uint16_t* blue)
{
	engine_api_t *api = NULL;
	eos_error_t err = EOS_ERROR_OK;
	engine_t *engine = NULL;

	if(data_mgr == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	api = find_api(data_mgr, ENGINE_TYPE_DATA_PROV, &engine);
	if(api == NULL)
	{
		err = EOS_ERROR_NFOUND;
	}
	else
	{
		err = api->func.data_prov.ttxt_blue_page_get(engine, blue);
		osi_free((void**)&api);
	}

	return err;
}

eos_error_t data_mgr_ttxt_yellow_page_get(data_mgr_t* data_mgr,
		uint16_t* yellow)
{
	engine_api_t *api = NULL;
	eos_error_t err = EOS_ERROR_OK;
	engine_t *engine = NULL;

	if(data_mgr == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	api = find_api(data_mgr, ENGINE_TYPE_DATA_PROV, &engine);
	if(api == NULL)
	{
		err = EOS_ERROR_NFOUND;
	}
	else
	{
		err = api->func.data_prov.ttxt_yellow_page_get(engine, yellow);
		osi_free((void**)&api);
	}

	return err;
}

eos_error_t data_mgr_ttxt_transparency_set(data_mgr_t* data_mgr,
		uint8_t alpha)
{
	engine_api_t *api = NULL;
	eos_error_t err = EOS_ERROR_OK;
	engine_t *engine = NULL;

	if(data_mgr == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	api = find_api(data_mgr, ENGINE_TYPE_DATA_PROV, &engine);
	if(api == NULL)
	{
		err = EOS_ERROR_NFOUND;
	}
	else
	{
		err = api->func.data_prov.ttxt_transparency_set(engine, alpha);
		osi_free((void**)&api);
	}

	return err;
}

eos_error_t data_mgr_hbbtv_uri_get(data_mgr_t* data_mgr, char* uri)
{
	engine_api_t *api = NULL;
	eos_error_t err = EOS_ERROR_OK;
	engine_t *engine = NULL;

	if(data_mgr == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	api = find_api(data_mgr, ENGINE_TYPE_HBBTV, &engine);
	if(api == NULL)
	{
		err = EOS_ERROR_NFOUND;
	}
	else
	{
		err = api->func.hbbtv.get_red_btn_url(engine, (uint8_t**)&uri);
		osi_free((void**)&api);
	}

	return err;
}

eos_error_t data_mgr_dvbsub_enable(data_mgr_t* data_mgr, bool enable)
{
	engine_api_t *api = NULL;
	eos_error_t err = EOS_ERROR_OK;
	engine_t *engine = NULL;

	if(data_mgr == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	api = find_api(data_mgr, ENGINE_TYPE_DATA_PROV, &engine);
	if(api == NULL)
	{
		err = EOS_ERROR_NFOUND;
	}
	else
	{
		err = api->func.data_prov.dvbsub_enable(engine, enable);
		osi_free((void**)&api);
	}

	return err;
}

static eos_error_t data_prov_start(data_mgr_t* data_mgr, chain_t* chain,
		eos_media_desc_t* media)
{
	uint8_t i = 0;
	sink_t *sink = NULL;
	eos_error_t err = EOS_ERROR_OK;
	link_cap_data_prov_t *data_prov = NULL;
	engine_params_t params;
	engine_t *engine = NULL;
	list_item_t *item = NULL;
	engine_api_t *api = NULL;
	link_cap_stream_sel_ctrl_t *sel = NULL;

	err = chain_get_sink(chain, &sink);
	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	if(!(sink->caps & LINK_CAP_DATA_PROV))
	{
		UTIL_LOGI(data_mgr->log, "Skipping: Sink does "
				"not have data provider");
		return EOS_ERROR_OK;
	}

	if (sink->command.get_ctrl_funcs(sink, LINK_CAP_STREAM_SEL,
			(void**)&sel) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Get control callbacks failed");
		return EOS_ERROR_GENERAL;
	}

	osi_mutex_lock(data_mgr->lock);
	for(i=0; i<media->es_cnt; i++)
	{
		if(media->es[i].codec == EOS_MEDIA_CODEC_TTXT)
		{
			UTIL_LOGI(data_mgr->log, "Selecting teletext ID [%d]",
					media->es[i].id);
			if (sel->select(sink, i) != EOS_ERROR_OK)
			{
				media->es[i].selected = false;
				UTIL_LOGW(data_mgr->log,
					"Selecting teletext ID [%d] [Failure]",
					media->es[i].id);
			}
			else
			{
				media->es[i].selected = true;
				UTIL_LOGI(data_mgr->log,
					"Selecting teletext ID [%d] [Success]",
					media->es[i].id);
			}
		}
	}
	err = sink->command.get_ctrl_funcs(sink, LINK_CAP_DATA_PROV,
			(void**)&data_prov);
	if (err != EOS_ERROR_OK)
	{
		UTIL_LOGE(data_mgr->log, "Failed to get control functions!");
		osi_mutex_unlock(data_mgr->lock);
		return err;
	}

	params.cb = data_mgr->engine_cb;
	params.codec = EOS_MEDIA_CODEC_DAT;
	err = engine_factory_manufacture(&params, &engine);
	if (err == EOS_ERROR_OK)
	{
		item = (list_item_t*)osi_calloc(sizeof(list_item_t));
		if (item == NULL)
		{
			UTIL_LOGE(data_mgr->log,
					"Engine list item mem alloc failed");
			engine_factory_dismantle(&engine);
			osi_mutex_unlock(data_mgr->lock);
			return EOS_ERROR_NOMEM;
		}
		item->id = media->es_cnt;
		item->engine = engine;
		data_mgr->engines.add(data_mgr->engines, item);

		api = osi_calloc(sizeof(engine_api_t));
		item->engine->get_api(item->engine, api);
		UTIL_LOGI(data_mgr->log, "Setting data provider");
		api->func.data_prov.set_data_prov(engine, sink, data_prov);
		osi_free((void**)&api);
	}
	osi_mutex_unlock(data_mgr->lock);

	return err;
}

static eos_error_t data_prov_stop(data_mgr_t* data_mgr)
{
	sink_t *sink = NULL;
	eos_error_t err = EOS_ERROR_OK;
	list_item_t *item = NULL;
	engine_type_t type = ENGINE_TYPE_DATA_PROV;

	err = chain_get_sink(data_mgr->chain, &sink);
	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	if(!(sink->caps & LINK_CAP_DATA_PROV))
	{
		UTIL_LOGI(data_mgr->log, "Skipping data provider stop");
		return EOS_ERROR_OK;
	}
	osi_mutex_lock(data_mgr->lock);
	err = data_mgr->engines.get(data_mgr->engines, &type, (void**)&item);
	if(err != EOS_ERROR_OK || item == NULL)
	{
		osi_mutex_unlock(data_mgr->lock);
		return err;
	}
	err = data_mgr->engines.remove(data_mgr->engines, (void*)&type);
	if(err != EOS_ERROR_OK)
	{
		UTIL_LOGW(data_mgr->log, "Remove from list failed?!?");
	}
	osi_free((void**)&item);
	osi_mutex_unlock(data_mgr->lock);

	return EOS_ERROR_OK;
}

static eos_error_t data_prov_select(data_mgr_t* data_mgr,
		eos_media_desc_t *streams, uint32_t id, bool on)
{
	sink_t *sink = NULL;
	eos_error_t err = EOS_ERROR_OK;
	int i = 0, idx = 0;
	link_cap_stream_sel_ctrl_t *sel = NULL;

	err = chain_get_sink(data_mgr->chain, &sink);
	if(err != EOS_ERROR_OK)
	{
		return err;
	}
	if(!(sink->caps & LINK_CAP_DATA_PROV) ||
			!(sink->caps & LINK_CAP_STREAM_SEL))
	{
		UTIL_LOGI(data_mgr->log, "Skipping data provider select %d", id);

		return EOS_ERROR_NFOUND;
	}
	if (sink->command.get_ctrl_funcs(sink, LINK_CAP_STREAM_SEL,
			(void**)&sel) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Get control callbacks failed");

		return EOS_ERROR_GENERAL;
	}
	osi_mutex_lock(data_mgr->lock);

	for (i = 0; i < streams->es_cnt; i++)
	{
		if (streams->es[i].id == id)
		{
			if(streams->es[i].codec != EOS_MEDIA_CODEC_DVB_SUB)
			{
				osi_mutex_unlock(data_mgr->lock);
				UTIL_GLOGE("We don't support %s",
						media_codec_string(streams->es[i].codec));

				return EOS_ERROR_INVAL;
			}
			idx = i;
			if (streams->es[i].selected == on)
			{
				osi_mutex_unlock(data_mgr->lock);

				return EOS_ERROR_OK;
			}
			break;
		}
	}
	if(i == streams->es_cnt)
	{
		osi_mutex_unlock(data_mgr->lock);
		UTIL_GLOGE("Stream not found %d", id);

		return EOS_ERROR_NFOUND;
	}
	for (i = 0; i < streams->es_cnt; i++)
	{
		if (streams->es[i].codec == streams->es[idx].codec && on &&
				streams->es[i].selected == true)
		{
			sel->deselect((link_handle_t)sink, i);
			streams->es[i].selected = false;
		}
	}
	if(on)
	{
		err = sel->select((link_handle_t)sink, idx);
		if (err != EOS_ERROR_OK)
		{
			osi_mutex_unlock(data_mgr->lock);

			return EOS_ERROR_INVAL;
		}
	}
	else
	{
		err = sel->deselect((link_handle_t)sink, idx);
		if (err != EOS_ERROR_OK)
		{
			osi_mutex_unlock(data_mgr->lock);

			return EOS_ERROR_INVAL;
		}
	}
	streams->es[idx].selected = on;
	osi_mutex_unlock(data_mgr->lock);

	return EOS_ERROR_OK;
}

static engine_api_t* find_api(data_mgr_t* data_mgr, engine_type_t type,
		engine_t **engine)
{
	engine_api_t *api = NULL;
	list_item_t *item = NULL;
	eos_error_t err = EOS_ERROR_OK;

	err = data_mgr->engines.get(data_mgr->engines, &type, (void**)&item);
	if(err != EOS_ERROR_OK || item == NULL)
	{
		return NULL;
	}
	*engine = item->engine;
	api = osi_calloc(sizeof(engine_api_t));
	item->engine->get_api(item->engine, api);

	return api;
}
