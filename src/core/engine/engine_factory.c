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

#define MODULE_NAME "factory:engine"

// *************************************
// *             Includes              *
// *************************************

#include "engine_factory.h"
#include "util_factory.h"
#include "osi_mutex.h"
#include "osi_memory.h"
#include "util_log.h"

#include <string.h>

// *************************************
// *              Macros               *
// *************************************

// *************************************
// *              Types                *
// *************************************

// *************************************
// *            Prototypes             *
// *************************************

static eos_error_t engine_probe(engine_t* model, engine_params_t* params);

// *************************************
// *         Global variables          *
// *************************************

static util_factory_t *factory = NULL;

// *************************************
// *             Threads               *
// *************************************

// *************************************
// *         Local functions           *
// *************************************

static eos_error_t engine_probe(engine_t* model, engine_params_t* params)
{
	if ((model == NULL) || (params == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (model->probe(params->codec) == EOS_ERROR_OK)
	{
		return EOS_ERROR_OK;
	}

	return EOS_ERROR_NFOUND;
}

// *************************************
// *         Global functions          *
// *************************************

eos_error_t engine_factory_register(engine_t* model, uint64_t* model_id, engine_manufacture_cb_t manufacture,
		engine_dismantle_cb_t dismantle)
{
	eos_error_t ret = EOS_ERROR_OK;

	if ((model == NULL) || (manufacture == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (factory == NULL)
	{
		ret = util_factory_create(&factory, (util_probe_cb_t)engine_probe);
		if (ret != EOS_ERROR_OK)
		{
			return ret;
		}
	}
	ret = util_factory_register(factory, (void*)model, model_id,
			(util_manufacture_cb_t)manufacture, (util_dismantle_cb_t)dismantle);
	if (ret != EOS_ERROR_OK)
	{
		return ret;
	}
	UTIL_GLOGI("Registered engine model: %s", model->name());

	return EOS_ERROR_OK;
}

eos_error_t engine_factory_unregister(engine_t* model, uint64_t model_id)
{
	eos_error_t ret = EOS_ERROR_OK;
	uint32_t count = 0;

	if (model == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (factory == NULL)
	{
		return EOS_ERROR_PERM;
	}

	ret = util_factory_unregister(factory, (void*)model, model_id);
	if (ret != EOS_ERROR_OK)
	{
		return ret;
	}
	UTIL_GLOGI("Unregistered engine model: %s", model->name());

	ret = util_factory_get_count(factory, &count);
	if (ret != EOS_ERROR_OK)
	{
		return ret;
	}
	if (count == 0)
	{
		util_factory_destroy(&factory);
	}

	return EOS_ERROR_OK;
}

eos_error_t engine_factory_manufacture(engine_params_t* params, engine_t** product)
{
	eos_error_t err = EOS_ERROR_OK;

	if ((product == NULL) || (params == NULL))
	{
		return EOS_ERROR_INVAL;
	}
	err = util_factory_manufacture(factory, (void**)product, params);
	if (err != EOS_ERROR_OK)
	{
		if (err == EOS_ERROR_NFOUND)
		{
			UTIL_GLOGW("Engine for \"%s\" was not found", 
			           media_codec_string(params->codec));
		}
		else
		{
			UTIL_GLOGE("Engine manufacture failed: %d", err);
		}
		return err;
	}

	return EOS_ERROR_OK;
}

eos_error_t engine_factory_dismantle(engine_t** product)
{
	eos_error_t err = EOS_ERROR_OK;

	if ((product == NULL) || (*product == NULL))
	{
		return EOS_ERROR_INVAL;
	}
	err = util_factory_dismantle(factory, (void**)product);
	if (err != EOS_ERROR_OK)
	{
		UTIL_GLOGW("Engine (%s) dismantle failed!", (*product)->name());
	}

	return EOS_ERROR_OK;
}

