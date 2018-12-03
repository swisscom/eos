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

#define MODULE_NAME "factory:util"

// *************************************
// *             Includes              *
// *************************************

#include "util_factory.h"
#include "util_slist.h"
#include "osi_memory.h"
#include "osi_mutex.h"
#include "eos_macro.h"
#include "util_log.h"

#include <string.h>

// *************************************
// *              Macros               *
// *************************************

// *************************************
// *              Types                *
// *************************************

typedef struct util_factory_spec
{
	uint64_t model_id;
	void *model;
	util_manufacture_cb_t manufacture;
	util_dismantle_cb_t dismantle;
} util_factory_spec_t;

typedef struct util_factory_product
{
	util_factory_spec_t *spec;
	void *instance;
	uint64_t id;
} util_factory_product_t;

struct util_factory
{
	util_slist_t specs;
	util_slist_t products;
	osi_mutex_t *products_mutex;
	util_probe_cb_t probe;
};

// *************************************
// *            Prototypes             *
// *************************************

static util_factory_spec_t* util_factory_id_and_remove (util_factory_t* factory, void* instance);
static int32_t util_factory_dismantle_all (util_factory_t* factory, util_factory_spec_t* spec);
static uint64_t util_factory_gen_model_id (void);

// *************************************
// *         Global variables          *
// *************************************

// *************************************
// *             Threads               *
// *************************************

// *************************************
// *         Local functions           *
// *************************************

static util_factory_spec_t* util_factory_id_and_remove (util_factory_t* factory, void* instance)
{
	util_factory_product_t *product = NULL;
	util_factory_spec_t *spec = NULL;
	eos_error_t error = EOS_ERROR_OK;

	if (factory == NULL || instance == NULL)
	{
		return NULL;
	}

	osi_mutex_lock(factory->products_mutex);

	error = factory->products.first(factory->products, (void**)&product);
	while ((error == EOS_ERROR_OK) || (error == EOS_ERROR_EOL))
	{
		if (product == NULL)
		{
			continue;
		}
		if (instance == product->instance)
		{
			spec = product->spec;
			factory->products.remove(factory->products, (void*)product);
                        osi_free((void**)&product);
		}
		if (error == EOS_ERROR_EOL)
		{
			break;
		}

		error = factory->products.next(factory->products, (void**)&product);
	}
	osi_mutex_unlock(factory->products_mutex);

	return spec;
}

static int32_t util_factory_dismantle_all (util_factory_t* factory, util_factory_spec_t *spec)
{
	util_factory_product_t *product = NULL;
	int32_t counter = 0;
	eos_error_t error = EOS_ERROR_OK;

	if (spec == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	osi_mutex_lock(factory->products_mutex);

	factory->products.count(factory->products, &counter);

	if (counter == 0)
	{
		osi_mutex_unlock(factory->products_mutex);
		return 0;
	}

	counter = 0;
	error = factory->products.first(factory->products, (void**)&product);
	while ((error == EOS_ERROR_OK) || (error == EOS_ERROR_EOL))
	{
		if (product == NULL)
		{
			continue;
		}
		if (product->spec == spec)
		{
			counter++;
			spec->dismantle(spec->model_id, &product->instance);
			factory->products.remove(factory->products, (void*)product);
			osi_free((void**)&product);
		}
		if (error == EOS_ERROR_EOL)
		{
			break;
		}
		error = factory->products.next(factory->products, (void**)&product);
	}
	if (product != NULL)
	{
		UTIL_GLOGWTF("Not empty");
		if (product->spec == spec)
		{
			counter++;
			spec->dismantle(spec->model_id, &product->instance);
			factory->products.remove(factory->products, (void*)product);
			osi_free((void**)&product);
		}
	}

	osi_mutex_unlock(factory->products_mutex);
	return counter;
}

static uint64_t util_factory_gen_model_id (void)
{
	osi_time_t t;
	uint32_t seed = 0;
	uint64_t ret = 0LL;

	if(osi_time_get_time(&t) == EOS_ERROR_OK)
	{
		seed = t.sec ^ t.nsec;
	}
	srand(seed);
	ret = rand();
	ret <<= 32;
	ret += rand();

	return ret;
}

// *************************************
// *         Global functions          *
// *************************************

eos_error_t util_factory_create(util_factory_t** factory, util_probe_cb_t probe)
{
	util_factory_t *tmp = NULL;

	if(factory == NULL || probe == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	tmp = osi_calloc(sizeof(util_factory_t));
	if(tmp == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_GLOGD("Creating util manufacturing specs register...");
	if (util_slist_create(&tmp->specs, NULL) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Unable to create util manufacturing specs register");
		return EOS_ERROR_GENERAL;
	}
	UTIL_GLOGI("Created util manufacturing specs register");
	UTIL_GLOGD("Creating manufactured utils register...");
	if (util_slist_create(&tmp->products, NULL) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Unable to create manufactured utils register");
		util_slist_destroy(&tmp->specs);
		osi_free((void**)&tmp);
		return EOS_ERROR_GENERAL;
	}
	if (osi_mutex_create(&tmp->products_mutex) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Unable to create manufactured utils register");
		util_slist_destroy(&tmp->products);
		util_slist_destroy(&tmp->specs);
		osi_free((void**)&tmp);
		return EOS_ERROR_GENERAL;
	}
	tmp->probe = probe;
	*factory = tmp;
	UTIL_GLOGI("Created manufactured utils register");

	return EOS_ERROR_OK;
}

eos_error_t util_factory_destroy(util_factory_t** factory)
{
	int32_t count = 0;
	eos_error_t error = EOS_ERROR_OK;
	util_factory_t *tmp = NULL;

	if(factory == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	tmp = *factory;
	if ((error = tmp->specs.count(tmp->specs, &count)) != EOS_ERROR_OK)
	{
		return error;
	}
	if (count != 0)
	{
		return EOS_ERROR_PERM;
	}
	UTIL_GLOGD("Destroying util manufacturing specs register...");
	if (util_slist_destroy(&tmp->specs) == EOS_ERROR_OK)
	{
		UTIL_GLOGI("Destroyed util manufacturing specs register");
	}
	else
	{
		UTIL_GLOGE("Unable to destroy util manufacturing specs register");
	}
	UTIL_GLOGD("Destroying manufactured utils register...");
	if (util_slist_destroy(&tmp->products) == EOS_ERROR_OK)
	{
		UTIL_GLOGI("Destroyed manufactured utils register");
	}
	else
	{
		UTIL_GLOGE("Unable to destroy manufactured utils register");
	}
	if (osi_mutex_destroy(&tmp->products_mutex) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Unable to destroy manufactured utils register mutex");
	}
	osi_free((void**)factory);

	return EOS_ERROR_OK;
}

eos_error_t util_factory_register (util_factory_t* factory, void* model, uint64_t* model_id,
		util_manufacture_cb_t manufacture, util_dismantle_cb_t dismantle)
{
	util_factory_spec_t *product_spec = NULL;

	if (factory == NULL || model == NULL || model_id == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	product_spec = (util_factory_spec_t*)osi_calloc(sizeof(util_factory_spec_t));
	product_spec->model = model;
	if(*model_id == 0LL)
	{
		*model_id = util_factory_gen_model_id();
	}
	product_spec->model_id = *model_id;
	product_spec->manufacture = manufacture;
	product_spec->dismantle = dismantle;

	if (factory->specs.add(factory->specs, (void*)product_spec) != EOS_ERROR_OK)
	{
		osi_free((void**)&product_spec);
		UTIL_GLOGE("Unable to register util model: %llu", *model_id);
		return EOS_ERROR_GENERAL;
	}
	UTIL_GLOGD("Registered model id: 0x%llX", *model_id);

	return EOS_ERROR_OK;
}

eos_error_t util_factory_unregister (util_factory_t* factory, void* model, uint64_t model_id)
{
	util_factory_spec_t *product_spec = NULL;
	uint8_t found = 0;
	int32_t count = 0;
	eos_error_t error = EOS_ERROR_OK;

	if (model == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (factory->specs.priv == NULL)
	{
		return EOS_ERROR_GENERAL;
	}

	error = factory->specs.first(factory->specs, (void**)&product_spec);
	while ((error == EOS_ERROR_OK) || (error == EOS_ERROR_EOL))
	{
		if (product_spec != NULL)
		{
			if ((product_spec->model == model) && (product_spec->model_id == model_id))
			{
				count = util_factory_dismantle_all(factory, product_spec);
				if (count > 0)
				{
					UTIL_GLOGW("%d manufactured %s forcefully dismantled", count, ((count % 10 == 1) && (count % 100 != 11))? "util was" : "utils were");
				}
				if (factory->specs.remove(factory->specs, (void*)product_spec) == EOS_ERROR_OK)
				{
					osi_free((void**)&product_spec);
					found = 1;
					UTIL_GLOGI("Unregistered util model: %llu", model_id);
				}
				break;
			}
		}
		if (error == EOS_ERROR_EOL)
		{
			break;
		}
		error = factory->specs.next(factory->specs, (void**)&product_spec);
	}

	if (found != 1)
	{
		UTIL_GLOGE("Unable to unregister util model: %llu", model_id);
		return EOS_ERROR_NFOUND;
	}

	return EOS_ERROR_OK;
}

eos_error_t util_factory_get_count (util_factory_t* factory, uint32_t* count)
{
	if(factory == NULL || count == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	return factory->specs.count(factory->specs, (int32_t*)count);
}

eos_error_t util_factory_manufacture (util_factory_t* factory, void** product, void* data)
{
	util_factory_spec_t *product_spec = NULL;
	util_factory_product_t *product_data = NULL;
	bool found = false;
	uint64_t product_id = UTIL_FACTORY_START_PRODUCT_ID;
	eos_error_t error = EOS_ERROR_OK;
	eos_error_t spec_error = EOS_ERROR_OK;

	if (product == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	spec_error = factory->specs.first(factory->specs, (void**)&product_spec);
	while ((spec_error == EOS_ERROR_OK) || (spec_error == EOS_ERROR_EOL))
	{
		if (product_spec == NULL)
		{
			continue;
		}
		if (factory->probe(product_spec->model, data) == EOS_ERROR_OK)
		{
			UTIL_GLOGD("Found model");
			error = factory->products.last(factory->products, (void**)&product_data);
			if (error == EOS_ERROR_OK)
			{
				product_id = product_data->id + 1;
			}
			else
			{
				if (error != EOS_ERROR_EMPTY)
				{
					UTIL_GLOGD("Product list not empty but failed");
					UTIL_GLOGD("Element manufacturing failed");
					break;
				}
			}
			if (product_id == UTIL_FACTORY_START_PRODUCT_ID)
			{
				// In theory possible but in practice quite impossible to overlap (ever)
			}
			if (product_spec->manufacture(data, product_spec->model, 
			    product_spec->model_id, product, product_id) == EOS_ERROR_OK)
			{
				product_data = (util_factory_product_t*)osi_calloc(sizeof(util_factory_product_t));
				if (product_data == NULL)
				{
					UTIL_GLOGD("No memory");
					product_spec->dismantle(product_spec->model_id, product);
					UTIL_GLOGD("Element manufacturing failed");
					break;
				}
				product_data->instance = *product;
				product_data->spec = product_spec;
				product_data->id = product_id;
				if (factory->products.add(factory->products, (void*)product_data) != EOS_ERROR_OK)
				{
					UTIL_GLOGD("Unable to add to products list");
					product_spec->dismantle(product_spec->model_id, product);
					osi_free((void**)&product_data);
					UTIL_GLOGD("Element manufacturing failed");
					break;
				}
				found = true;
				break;
			}
			else
			{
				UTIL_GLOGD("Element manufacturing failed");
				break;
			}
		}
		if (spec_error == EOS_ERROR_EOL)
		{
			break;
		}

		spec_error = factory->specs.next(factory->specs, (void**)&product_spec);
	}

	if (found == true)
	{
		return EOS_ERROR_OK;
	}
	else
	{
		return EOS_ERROR_NFOUND;
	}
}

eos_error_t util_factory_dismantle (util_factory_t* factory, void** product)
{
	util_factory_spec_t *product_spec = NULL;
	eos_error_t error = EOS_ERROR_OK;

	if (product == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	product_spec = util_factory_id_and_remove(factory, *product);

	if (product_spec != NULL)
	{
		error = product_spec->dismantle(product_spec->model_id, product);
		if (error != EOS_ERROR_OK)
		{
			UTIL_GLOGE("Unable to dismantle the product");
			return error;
		}
	}
	else
	{
		UTIL_GLOGE("Unable to id the product");
		return EOS_ERROR_NFOUND;
	}

	return EOS_ERROR_OK;
}

