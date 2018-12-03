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

#define MODULE_NAME "core:factory:controler:playback"

// *************************************
// *             Includes              *
// *************************************

#include "playback_ctrl_factory.h"
#include "eos_macro.h"
#include "eos_types.h"
#include "util_slist.h"
#include "util_log.h"
#include "osi_mutex.h"

// *************************************
// *              Macros               *
// *************************************

// *************************************
// *              Types                *
// *************************************

typedef struct playback_ctrl_factory
{
	util_slist_t providers;
	bool initialized;
	osi_mutex_t *sync;
} playback_ctrl_factory_t;

typedef enum compare_type
{
	COMPARE_TYPE_PROVIDER,
	COMPARE_TYPE_PLAYBACK_DATA
} compare_type_t;

typedef struct compare_data
{
	compare_type_t type;
	union
	{
		playback_ctrl_provider_t *provider;
		struct
		{
			chain_t *chain;
			playback_ctrl_func_type_t func;
		}playback;
	}data;
} compare_data_t;

// *************************************
// *            Prototypes             *
// *************************************

static bool provider_comparator(void* search_param, void* element);
static eos_error_t playback_ctrl_factory_init();
static eos_error_t assign_functionality(chain_t* chain, playback_ctrl_t* ctrl, playback_ctrl_func_type_t func);

// *************************************
// *         Global variables          *
// *************************************

static playback_ctrl_factory_t factory =
{
	.providers =
	{
	        .add = NULL,
	        .remove = NULL,
	        .next = NULL,
	        .first = NULL,
	        .count = NULL,
        	.priv = NULL
	},
	.initialized = false,
	.sync = NULL
};

// *************************************
// *             Threads               *
// *************************************

// *************************************
// *         Local functions           *
// *************************************

static eos_error_t playback_ctrl_factory_init()
{
	eos_error_t error = EOS_ERROR_OK;

	UTIL_GLOGI("Init ...");
	if (factory.initialized == false)
	{
		error = osi_mutex_create(&factory.sync);
		if (error != EOS_ERROR_OK)
		{
			UTIL_GLOGE("Init [Failure]");
			return error;
		}
		error = util_slist_create(&factory.providers, provider_comparator);
		if (error != EOS_ERROR_OK)
		{
			if (osi_mutex_destroy(&factory.sync) != EOS_ERROR_OK)
			{
			}
			UTIL_GLOGE("Init [Failure]");
			return error;
		}
		factory.initialized = true;
	}
	UTIL_GLOGI("Init [Success]");
	return EOS_ERROR_OK;
}

// TODO Think about deinit

static bool provider_comparator(void* search_param, void* element)
{
	EOS_UNUSED(search_param)
	EOS_UNUSED(element)
	compare_data_t *compare = NULL;

	if ((element == NULL) || (search_param == NULL))
	{
		return false;
	}

	compare = (compare_data_t*)search_param;
	switch (compare->type)
	{
		case COMPARE_TYPE_PROVIDER:
			if (compare->data.provider == element)
			{
				return true;
			}
			else
			{
				return false;
			}
		case COMPARE_TYPE_PLAYBACK_DATA:
			if (((playback_ctrl_provider_t*)element)->probe(compare->data.playback.chain, compare->data.playback.func) == EOS_ERROR_OK)
			{
				return true;
			}
			else
			{
				return false;
			}
			break;
		default:
			return false;
	}

	return false;
}

static eos_error_t assign_functionality(chain_t* chain, playback_ctrl_t* ctrl, playback_ctrl_func_type_t func)
{
	compare_data_t compare;
	playback_ctrl_provider_t *provider = NULL;
	// TODO Add default handler
	if (factory.initialized == false)
	{
		return EOS_ERROR_NFOUND;
	}
	compare.type = COMPARE_TYPE_PLAYBACK_DATA;
	compare.data.playback.chain = chain;
	compare.data.playback.func = func;
	if (factory.providers.get(factory.providers, &compare, (void**)&provider) != EOS_ERROR_OK)
	{
		return EOS_ERROR_NFOUND;
	}

	if (provider->assign(chain, func, ctrl) != EOS_ERROR_OK)
	{
		return EOS_ERROR_NFOUND;
	}

	return EOS_ERROR_OK;
}

// *************************************
// *         Global functions          *
// *************************************

eos_error_t playback_ctrl_factory_register_provider(playback_ctrl_provider_t* provider)
{
	eos_error_t error = EOS_ERROR_OK;

	UTIL_GLOGI("Register provider ...");
	if (provider == NULL)
	{
		UTIL_GLOGE("Register provider [Failure]");
		return EOS_ERROR_INVAL;
	}

	if ((provider->assign == NULL) || (provider->probe == NULL))
	{
		UTIL_GLOGE("Register provider [Failure]");
		return EOS_ERROR_INVAL;
	}

	if (factory.initialized == false)
	{
		error = playback_ctrl_factory_init();
		if (error != EOS_ERROR_OK)
		{
			UTIL_GLOGE("Register provider [Failure]");
			return error;
		}
	}

	return factory.providers.add(factory.providers, provider);
}

eos_error_t playback_ctrl_factory_unregister_provider(playback_ctrl_provider_t* provider)
{
	compare_data_t compare;
	if (provider == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if (factory.initialized == false)
	{
		return EOS_ERROR_GENERAL;
	}
	compare.type = COMPARE_TYPE_PROVIDER;
	compare.data.provider = provider;
	return factory.providers.remove(factory.providers, &compare);
}

eos_error_t playback_ctrl_factory_assign(chain_t* chain, playback_ctrl_t* ctrl)
{
	if ((chain == NULL) || (ctrl == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	assign_functionality(chain, ctrl, PLAYBACK_CTRL_START);	
	assign_functionality(chain, ctrl, PLAYBACK_CTRL_SELECT);	
	assign_functionality(chain, ctrl, PLAYBACK_CTRL_TRICKPLAY);	
	assign_functionality(chain, ctrl, PLAYBACK_CTRL_STOP);	
	assign_functionality(chain, ctrl, PLAYBACK_CTRL_START_BUFF);
	assign_functionality(chain, ctrl, PLAYBACK_CTRL_STOP_BUFF);
	assign_functionality(chain, ctrl, PLAYBACK_CTRL_HANDLE_EVENT);

	return EOS_ERROR_OK;
}


