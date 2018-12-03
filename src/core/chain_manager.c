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
// *            Module name            *
// *************************************

#define MODULE_NAME "core:manager:chain"

// *************************************
// *             Includes              *
// *************************************

#include "eos_macro.h"
#include "eos_types.h"
#include "source_factory.h"
#include "sink_factory.h"
#include "lynx.h"
#include "util_log.h"
#include "util_slist.h"
#include "osi_sem.h"
#include "osi_mutex.h"
#include "osi_memory.h"
#include "osi_thread.h"

#include "chain_manager.h"

#include <string.h>

// *************************************
// *              Macros               *
// *************************************

#define INTERRUPTABLE
//#define UNSYNC

// *************************************
// *              Types                *
// *************************************

typedef struct chain_protection
{
	osi_mutex_t *tune;
	osi_mutex_t *lynk;
	osi_mutex_t *interrupt;
} chain_protection_t;

typedef struct chain_element
{
	uint32_t sink_id;
	int32_t ref_counter;
	int32_t manipulation_counter;
	chain_t* chain;
	chain_protection_t protection;
} chain_element_t;

typedef struct chain_manager
{
	osi_mutex_t *sync;
	osi_mutex_t *list_lock;
	util_slist_t chain;
} chain_manager_t;

struct thread_data
{
	sink_t *sink;
	eos_error_t error;
};

// *************************************
// *            Prototypes             *
// *************************************

// *************************************
// *         Global variables          *
// *************************************

chain_manager_t chain_manager;

// *************************************
// *             Threads               *
// *************************************

#ifdef UNSYNC
static void* async_sink_stop(void* arg)
{
	UTIL_GLOGI("Async sink stop ...");
	sink_t *sink = ((struct thread_data*)arg)->sink;
//	UTIL_GLOGWTF("Stop");
	((struct thread_data*)arg)->error = sink->command.stop(sink);
//	UTIL_GLOGWTF("Flush");
	((struct thread_data*)arg)->error = sink->command.flush_buffs(sink);
	UTIL_GLOGI("Async sink stop [%s]", (((struct thread_data*)arg)->error == EOS_ERROR_OK)? "Success":"Failure");

	return arg;
}
#endif

// *************************************
// *         Local functions           *
// *************************************
eos_error_t chain_manager_assemble(chain_protection_t protection, chain_t* chain, 
				char* source_url, char* source_extras, uint32_t sink_id)
{
	source_t *source = NULL;
	sink_t *sink = NULL;
	eos_error_t error = EOS_ERROR_OK;
	osi_time_t lock_timeout = {0, 0};
	link_io_type_t io_type = 0;
	bool reuse_source = false;
	bool restart = false;
#ifdef UNSYNC
	osi_thread_t *sink_stop_thread = NULL;
#endif
	struct thread_data sink_stop_data;
	eos_media_desc_t *media = NULL;
	EOS_UNUSED(sink_id);

	UTIL_GLOGI("Assemble ...");
	if ((chain == NULL) || (source_url == NULL))
	{
		UTIL_GLOGE("Invalid parameter");
		return EOS_ERROR_INVAL;
	}
#ifdef INTERRUPTABLE
	// Lock interrupt
	osi_mutex_lock(protection.interrupt);
	// Lock lynk
	osi_mutex_lock(protection.lynk);

	chain_get_source(chain, &source);
	chain_get_sink(chain, &sink);
	if (source != NULL)
	{
		// Interupt tune
		chain_interrupt(chain);
	}

	// Unock lynk
	osi_mutex_unlock(protection.lynk);
	// Lock tune
	osi_mutex_lock(protection.tune);
	// Unlock interrupt
	osi_mutex_unlock(protection.interrupt);
#else
	EOS_UNUSED(protection);
#endif
	chain_get_source(chain, &source);
	chain_get_sink(chain, &sink);

	//Sanity check if only one of the elements is NULL
	if (((link_handle_t)sink != (link_handle_t)source) &&
			((sink == NULL) || (source == NULL)))
	{
		//TODO destroy and maybe recreate
	}

	if ((source != NULL) && (sink != NULL))
	{
		restart = true;
		if (source->probe(source_url) == EOS_ERROR_OK)
		{
			reuse_source = true;
		}
		// TODO Create sink stop thread
		sink_stop_data.sink = sink;
		sink_stop_data.error = EOS_ERROR_OK;
		chain_unload_data_mgr(chain);
#ifdef UNSYNC
		error = osi_thread_create(&sink_stop_thread, NULL, async_sink_stop,
				(void*)&sink_stop_data);
#else
		//unlock before stop
		chain_unlock(chain);

		UTIL_GLOGWTF("Stop");
		sink_stop_data.error = sink->command.stop(sink_stop_data.sink);
		UTIL_GLOGI("Flush sink  [%s]", (sink_stop_data.error == EOS_ERROR_OK)? "Success":"Failure");
		sink_stop_data.error = sink->command.flush_buffs(sink);
		UTIL_GLOGI("Sync sink stop [%s]", (sink_stop_data.error == EOS_ERROR_OK)? "Success":"Failure");
#endif

#ifdef INTERRUPTABLE
#ifdef UNSYNC
		chain_unlock(chain);
#endif
		// Lock lynk
		osi_mutex_lock(protection.lynk);
#endif
		chain_set_source(chain, NULL);
		chain_set_sink(chain, NULL);
		if (reuse_source == false)
		{
			if (source_factory_dismantle(&source) != EOS_ERROR_OK)
			{
				UTIL_GLOGW("Unable to dismantle source");
			}
			source = NULL;
		}
#ifdef INTERRUPTABLE
		// Unlock lynk
		osi_mutex_unlock(protection.lynk);
#endif
	}
	{
		if (source == NULL)
		{
			error = source_factory_manufacture(source_url, &source);
			if (error != EOS_ERROR_OK)
			{
				UTIL_GLOGE("Source manufacturing failed");
				goto done;
			}
		}
		chain_set_source(chain, source);
		lock_timeout.sec += 5;
		error = chain_lock(chain, source_url, source_extras, lock_timeout);
		if (error != EOS_ERROR_OK)
		{
			UTIL_GLOGE("Source locking failure (err: %d)", error);
			goto done;
		}

		source->get_output_type(source, &io_type);
		if (restart == true)
		{
			// TODO Wait for sink stop thread
			restart = false;
#ifdef UNSYNC
			osi_thread_join(sink_stop_thread, NULL);
			osi_thread_release(&sink_stop_thread);
#endif
			if (!EOS_MASK_SUBSET(sink->plug_type, io_type) || 
			!EOS_MASK_SUBSET(sink->caps,(LINK_CAP_STREAM_SEL | LINK_CAP_SINK)))
			{
				if (sink_factory_dismantle(&sink) != EOS_ERROR_OK)
				{
					UTIL_GLOGW("Unable to dismantle sink");
				}
				sink = NULL;
			}
			else
			{
				if((media = osi_malloc(sizeof(eos_media_desc_t))) == NULL)
				{
					error = EOS_ERROR_NOMEM;
					UTIL_GLOGE("Cannot allocate media desc");
					goto done;
				}
				error = chain_get_streams(chain, media);
				if (error != EOS_ERROR_OK)
				{
					UTIL_GLOGE("Get streams failed");
					goto done;
				}
				error = sink->command.setup(sink, sink_id, media);
				if (error != EOS_ERROR_OK)
				{
					UTIL_GLOGE("Sink setup failed");
					goto done;
				}
			}
		}

		if (sink == NULL)
		{
			if((media = osi_malloc(sizeof(eos_media_desc_t))) == NULL)
			{
				error = EOS_ERROR_NOMEM;
				UTIL_GLOGE("Cannot allocate media desc");
				goto done;
			}
			chain_get_streams(chain, media);
			error = sink_factory_manufacture(sink_id, media,
					LINK_CAP_STREAM_SEL | LINK_CAP_SINK, io_type, &sink);
			if (error != EOS_ERROR_OK)
			{
				UTIL_GLOGE("Sink manufacturing failed");
				goto done;
			}
		}

		error = chain_set_sink(chain, sink);
		if (error != EOS_ERROR_OK)
		{
			UTIL_GLOGE("Setting sink failed");
			goto done;
		}
	}
	// TODO Add error checking
	chain_load_playback_controler(chain);

#ifdef INTERRUPTABLE
	// Unlock tune
	osi_mutex_unlock(protection.tune);
#endif
	UTIL_GLOGI("Assemble [Success]");

done:
	if(media != NULL)
	{
		osi_free((void**)&media);
	}
	if (error != EOS_ERROR_OK)
	{
#ifdef INTERRUPTABLE
		// Lock lynk
		osi_mutex_lock(protection.lynk);
#endif
		if (source != NULL)
		{
			if (chain_unlock(chain) != EOS_ERROR_OK)
			{
				UTIL_GLOGW("Source unlock failed");
			}
			chain_set_source(chain, NULL);
			if (source_factory_dismantle(&source) != EOS_ERROR_OK)
			{
				UTIL_GLOGW("Unable to dismantle source");
			}
		}
		if (sink != NULL)
		{
			chain_set_sink(chain, NULL);
			if (restart == true)
			{
				// TODO Wait for sink stop thread
				restart = false;
#ifdef UNSYNC
				osi_thread_join(sink_stop_thread, NULL);
				osi_thread_release(&sink_stop_thread);
#endif
			}
			if (sink_factory_dismantle(&sink) != EOS_ERROR_OK)
			{
				UTIL_GLOGW("Unable to dismantle sink");
			}
		}
#ifdef INTERRUPTABLE
		// Unlock lynk
		osi_mutex_unlock(protection.lynk);
		// Unlock tune
		osi_mutex_unlock(protection.tune);
#endif
		UTIL_GLOGE("Assemble [Failure]");
	}

	return error;
}

eos_error_t chain_manager_disassemble(chain_t* chain)
{
	source_t *source = NULL;
	sink_t *sink = NULL;
	eos_error_t error = EOS_ERROR_OK;

	UTIL_GLOGI("Disassemble");
	if (chain == NULL)
	{
		UTIL_GLOGE("Invalid parameter");
		UTIL_GLOGE("Disassemble [Failure]");
		return EOS_ERROR_INVAL;
	}

	chain_get_source(chain, &source);
	if (source != NULL)
	{
		error = chain_unlock(chain);
		if (error != EOS_ERROR_OK)
		{
			UTIL_GLOGE("Source unlock failed");
			UTIL_GLOGE("Disassemble [Failure]");
			return error;
		}

		chain_set_source(chain, NULL);
		error = source_factory_dismantle(&source);
		if (error != EOS_ERROR_OK)
		{
			UTIL_GLOGE("Source dismantling failed");
			UTIL_GLOGE("Disassemble [Failure]");
			return error;
		}
	}

	chain_get_sink(chain, &sink);
	if (sink != NULL)
	{
		// TODO BEGIN
		//	sink->command.stop(sink);
		// TODO END
		chain_set_sink(chain, NULL);
		error = sink_factory_dismantle(&sink);
		if (error != EOS_ERROR_OK)
		{
			UTIL_GLOGE("Sink dismantling failed");
			UTIL_GLOGE("Disassemble [Failure]");
			return error;
		}
	}

	UTIL_GLOGI("Disassemble [Success]");
	return EOS_ERROR_OK;
}

bool chain_list_comparator(void* sink_id_address, void* chain_element_address)
{
	uint32_t sink_id = 0;
	chain_element_t *element = NULL;

	if ((chain_element_address == NULL) || (sink_id_address == NULL))
	{
		return false;
	}

	sink_id = *((uint32_t*)sink_id_address);
	element = (chain_element_t*)chain_element_address;

	if (sink_id == element->sink_id)
	{
		return true;
	}
	return false;
}

// *************************************
// *         Global functions          *
// *************************************

eos_error_t chain_manager_module_init()
{
	eos_error_t error = EOS_ERROR_OK;
	error = osi_mutex_create(&chain_manager.sync);
	if (error != EOS_ERROR_OK)
	{
		return error;
	}
	error = osi_mutex_create(&chain_manager.list_lock);
	if (error != EOS_ERROR_OK)
	{
		if (osi_mutex_destroy(&chain_manager.sync) != EOS_ERROR_OK)
		{
		}
		return error;
	}
	error = util_slist_create(&chain_manager.chain, chain_list_comparator);
	if (error != EOS_ERROR_OK)
	{
		if (osi_mutex_destroy(&chain_manager.list_lock) != EOS_ERROR_OK)
		{
		}
		if (osi_mutex_destroy(&chain_manager.sync) != EOS_ERROR_OK)
		{
		}
		return error;
	}
	return EOS_ERROR_OK;
}

eos_error_t chain_manager_get(uint32_t sink_id, chain_t** chain)
{
	chain_element_t *element = NULL;
	eos_error_t error = EOS_ERROR_OK;

	if (chain == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	*chain = NULL;
	error = osi_mutex_lock(chain_manager.list_lock);
	if (error != EOS_ERROR_OK)
	{
		return EOS_ERROR_GENERAL;
	}
	if (chain_manager.chain.get(chain_manager.chain, &sink_id, (void**)&element) == EOS_ERROR_OK)
	{
		if ((element->manipulation_counter > 0) || (element->ref_counter > 0))
		{
			element = NULL;
		}
		else
		{
			*chain = element->chain;
			element->ref_counter++;
		}
	}
	error = osi_mutex_unlock(chain_manager.list_lock);
	if (error != EOS_ERROR_OK)
	{
	}
	return (element == NULL)? EOS_ERROR_NFOUND:EOS_ERROR_OK;
}

eos_error_t chain_manager_release(uint32_t sink_id, chain_t** chain)
{
	chain_element_t *element = NULL;
	eos_error_t error = EOS_ERROR_OK;

	if (chain == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	error = osi_mutex_lock(chain_manager.list_lock);
	if (error != EOS_ERROR_OK)
	{
		return EOS_ERROR_GENERAL;
	}
	if (chain_manager.chain.get(chain_manager.chain, &sink_id,
			(void**)&element) == EOS_ERROR_OK)
	{
		if (*chain == element->chain)
		{
			element->ref_counter--;
			*chain = NULL;
		}
		else
		{
			// If it does not match assume that its not found
			element = NULL; 
		}
	}
	error = osi_mutex_unlock(chain_manager.list_lock);
	if (error != EOS_ERROR_OK)
	{
	}
	return (element == NULL)? EOS_ERROR_NFOUND:EOS_ERROR_OK;
}

eos_error_t chain_manager_create(char* source_url, char* source_extras,
		uint32_t sink_id, chain_handler_t* handler)
{
	eos_error_t error = EOS_ERROR_OK;
	chain_element_t *element = NULL;
	chain_t *local_chain = NULL;
	bool destroy = false;

	UTIL_GLOGI("Create ...");
	if ((source_url == NULL) || (handler == NULL))
	{
		UTIL_GLOGE("Create [Failure]");

		return EOS_ERROR_INVAL;
	}

	error = osi_mutex_lock(chain_manager.list_lock);
	if (error != EOS_ERROR_OK)
	{	
		UTIL_GLOGE("Create [Failure]");

		return EOS_ERROR_GENERAL;
	}

	if (chain_manager.chain.get(chain_manager.chain, &sink_id,
			(void**)&element) == EOS_ERROR_OK)
	{
		local_chain = element->chain;
		UTIL_GLOGI("Recycling chain");
		if (element->ref_counter > 0)
		{
			UTIL_GLOGW("Chain is being used (ref count %d) => Abort",
					element->ref_counter);
			if (osi_mutex_unlock(chain_manager.list_lock) != EOS_ERROR_OK)
			{
				UTIL_GLOGW("List mutex unlock failure");
			}
			UTIL_GLOGE("Create [Failure]");

			return EOS_ERROR_PERM;
		}
	}
	else
	{
		UTIL_GLOGI("Creating chain");
		error = chain_create(&local_chain, sink_id, handler);
		if (error != EOS_ERROR_OK)
		{
			
			if (osi_mutex_unlock(chain_manager.list_lock) != EOS_ERROR_OK)
			{
				UTIL_GLOGW("List mutex unlock failure");
			}
			UTIL_GLOGE("Create [Failure]");

			return error;
		}
		element = osi_calloc(sizeof(chain_element_t));
		if (element == NULL)
		{
			UTIL_GLOGE("List element allocation failed");
			if (osi_mutex_unlock(chain_manager.list_lock) != EOS_ERROR_OK)
			{
				UTIL_GLOGW("List mutex unlock failure");
			}
			if (chain_destroy(&local_chain) != EOS_ERROR_OK)
			{
				UTIL_GLOGW("Chain destroy failure");
			}
			UTIL_GLOGE("Create [Failure]");

			return error;
		}
		element->sink_id = sink_id;
		element->chain = local_chain;
		error = osi_mutex_create(&element->protection.interrupt);
		if (error != EOS_ERROR_OK)
		{
			UTIL_GLOGE("Interrupt mutex creation failed");
			osi_free((void**)&element);
			if (osi_mutex_unlock(chain_manager.list_lock) != EOS_ERROR_OK)
			{
				UTIL_GLOGW("List mutex unlock failure");
			}
			if (chain_destroy(&local_chain) != EOS_ERROR_OK)
			{
				UTIL_GLOGW("Chain destroy failure");
			}
			UTIL_GLOGE("Create [Failure]");

			return error;
		}
		error = osi_mutex_create(&element->protection.tune);
		if (error != EOS_ERROR_OK)
		{
			UTIL_GLOGE("Tune mutex creation failed");
			if (osi_mutex_destroy(&element->protection.interrupt)
					!= EOS_ERROR_OK)
			{
				UTIL_GLOGW("Interrupt mutex destruction failure");
			}
			osi_free((void**)&element);
			if (osi_mutex_unlock(chain_manager.list_lock) != EOS_ERROR_OK)
			{
				UTIL_GLOGW("List mutex unlock failure");
			}
			if (chain_destroy(&local_chain) != EOS_ERROR_OK)
			{
				UTIL_GLOGW("Chain destroy failure");
			}
			UTIL_GLOGE("Create [Failure]");

			return error;
		}
		error = osi_mutex_create(&element->protection.lynk);
		if (error != EOS_ERROR_OK)
		{
			UTIL_GLOGE("Lynk mutex creation failed");
			if (osi_mutex_destroy(&element->protection.tune) != EOS_ERROR_OK)
			{
				UTIL_GLOGW("Tune mutex destruction failure");
			}
			if (osi_mutex_destroy(&element->protection.interrupt)
					!= EOS_ERROR_OK)
			{
				UTIL_GLOGW("Interrupt mutex destruction failure");
			}
			osi_free((void**)&element);
			if (osi_mutex_unlock(chain_manager.list_lock) != EOS_ERROR_OK)
			{
				UTIL_GLOGW("List mutex unlock failure");
			}
			if (chain_destroy(&local_chain) != EOS_ERROR_OK)
			{
				UTIL_GLOGW("Chain destroy failure");
			}
			UTIL_GLOGE("Create [Failure]");

			return error;
		}
		error = chain_manager.chain.add(chain_manager.chain, element);
		if (error != EOS_ERROR_OK)
		{
			UTIL_GLOGE("Storing chain failed");
			if (osi_mutex_destroy(&element->protection.lynk) != EOS_ERROR_OK)
			{
				UTIL_GLOGW("Tune mutex destruction failure");
			}
			if (osi_mutex_destroy(&element->protection.tune) != EOS_ERROR_OK)
			{
				UTIL_GLOGW("Tune mutex destruction failure");
			}
			if (osi_mutex_destroy(&element->protection.interrupt)
					!= EOS_ERROR_OK)
			{
				UTIL_GLOGW("Interrupt mutex destruction failure");
			}
			osi_free((void**)&element);
			if (osi_mutex_unlock(chain_manager.list_lock) != EOS_ERROR_OK)
			{
				UTIL_GLOGW("List mutex unlock failure");
			}
			if (chain_destroy(&local_chain) != EOS_ERROR_OK)
			{
				UTIL_GLOGW("Chain destroy failure");
			}
			UTIL_GLOGE("Create [Failure]");

			return error;
		}
	}
	element->manipulation_counter++;
	error = osi_mutex_unlock(chain_manager.list_lock);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGW("List mutex unlock failure");
	}

	error = chain_manager_assemble(element->protection, local_chain,
			source_url, source_extras, sink_id);

	if (osi_mutex_lock(chain_manager.list_lock) != EOS_ERROR_OK)
	{	
		UTIL_GLOGE("List mutex lock failure");
		return EOS_ERROR_GENERAL;
	}
	element->manipulation_counter--;
	if (error != EOS_ERROR_OK)
	{
		if ((element->ref_counter > 0) || (element->manipulation_counter > 0))
		{
			destroy = false;
			UTIL_GLOGW("Chain is being used (usage cnt %d) => Skip destroying",
					element->ref_counter + element->manipulation_counter);
		}
		else
		{
			destroy = true;
		}
	}
	if (destroy == true)
	{
		if (chain_manager.chain.remove(chain_manager.chain, &sink_id)
				!= EOS_ERROR_OK)
		{
			UTIL_GLOGW("Chain removal from the list failure");
		}
		if (osi_mutex_destroy(&element->protection.lynk) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("Lynk mutex destruction failure");
		}
		if (osi_mutex_destroy(&element->protection.tune) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("Tune mutex destruction failure");
		}
		if (osi_mutex_destroy(&element->protection.interrupt) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("Interrupt mutex destruction failure");
		}
		osi_free((void**)&element);
		if (osi_mutex_unlock(chain_manager.list_lock) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("List mutex unlock failure");
		}
		if (chain_destroy(&local_chain) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("Chain destroy failure");
		}
		UTIL_GLOGE("Create [Failure]");

		return error;
	}
	if (osi_mutex_unlock(chain_manager.list_lock) != EOS_ERROR_OK)
	{
		UTIL_GLOGW("List mutex unlock failure");
	}

	UTIL_GLOGI("Create [Success]");
	return EOS_ERROR_OK;
}

eos_error_t chain_manager_destroy(uint32_t sink_id)
{
	chain_element_t *element = NULL;
	eos_error_t error = EOS_ERROR_OK;

	UTIL_GLOGI("Destroy ...");

	error = osi_mutex_lock(chain_manager.list_lock);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGW("List mutex lock failure");
		UTIL_GLOGE("Destroy [Failure]");
		return EOS_ERROR_GENERAL;
	}
	if (chain_manager.chain.get(chain_manager.chain, &sink_id, (void**)&element) != EOS_ERROR_OK)
	{
		error = osi_mutex_unlock(chain_manager.list_lock);
		if (error != EOS_ERROR_OK)
		{
			UTIL_GLOGW("List mutex unlock failure");
		}
		UTIL_GLOGE("Chain associated with sink id %d not found", sink_id);
		UTIL_GLOGE("Destroy [Failure]");
		return EOS_ERROR_NFOUND;
	}
	if ((element->ref_counter == 0) && (element->manipulation_counter == 0))
	{
		if (chain_manager.chain.remove(chain_manager.chain, &sink_id) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("Chain removal from the list failure");
		}
		if (osi_mutex_destroy(&element->protection.lynk) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("Lynk mutex destruction failure");
		}
		if (osi_mutex_destroy(&element->protection.tune) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("Tune mutex destruction failure");
		}
		if (osi_mutex_destroy(&element->protection.interrupt) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("Interrupt mutex destruction failure");
		}
		if (osi_mutex_unlock(chain_manager.list_lock) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("List mutex unlock failure");
		}
		if (chain_manager_disassemble(element->chain) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("Chain disassemble failure");
		}
		if (chain_destroy(&element->chain) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("Chain destroy failure");
		}
		osi_free((void**)&element);
	}
	else
	{
		if (osi_mutex_unlock(chain_manager.list_lock) != EOS_ERROR_OK)
		{
			UTIL_GLOGW("List mutex unlock failure");
		}
		UTIL_GLOGE("Chain in transition");
		error = EOS_ERROR_PERM;
	}

	if (error == EOS_ERROR_OK)
	{
		UTIL_GLOGI("Destroy [Success]");
	}
	else
	{
		UTIL_GLOGE("Destroy [Failure]");
	}
	return error;
}

