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


#include "source_factory.h"
#include "link_factory.h"

#define MODULE_NAME SOURCE_MODULE_NAME
#include "util_log.h"


static link_factory_t *factory = NULL;

static eos_error_t source_factory_identify_cbk (source_t* model, char* url);

eos_error_t source_factory_register_model (source_t* model, uint64_t* model_id, source_manufacture_func_t manufacture, source_dismantle_func_t dismantle)
{
	eos_error_t ret = EOS_ERROR_OK;

	if (model == NULL || manufacture == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (factory == NULL)
	{
		ret = link_factory_create(&factory,
				(link_identify_cbk_t)source_factory_identify_cbk);
		if(ret != EOS_ERROR_OK)
		{
			return ret;
		}
	}
	ret = link_factory_register(factory, (link_handle_t*)model, model_id,
			(link_manufacture_func_t)manufacture, (link_dismantle_func_t)dismantle);
	if(ret != EOS_ERROR_OK)
	{
		return ret;
	}
	UTIL_GLOGD("Registered model id: 0x%llX", model_id);
	
	return EOS_ERROR_OK;
}

eos_error_t source_factory_unregister_model (source_t* model, uint64_t model_id)
{
	eos_error_t ret = EOS_ERROR_OK;
	uint32_t count;

	if (model == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (factory == NULL)
	{
		return EOS_ERROR_PERM;
	}
	ret = link_factory_unregister(factory, (link_handle_t*)model, model_id);
	if(ret != EOS_ERROR_OK)
	{
		return ret;
	}
	ret = link_factory_get_count(factory, &count);
	if(ret != EOS_ERROR_OK)
	{
		return ret;
	}
	if(count == 0)
	{
		link_factory_destroy(&factory);
	}

	return EOS_ERROR_OK;
}

eos_error_t source_factory_manufacture(char* uri, source_t** product)
{
	if (product == NULL || uri == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	return link_factory_manufacture(factory, (link_handle_t**)product, uri);
}

eos_error_t source_factory_dismantle(source_t** product)
{
	if (product == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	return link_factory_dismantle(factory, (link_handle_t**)product);
}

eos_error_t source_factory_identify_cbk(source_t* model, char* url)
{
	if(model == NULL || url == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	return model->probe(url);
}
