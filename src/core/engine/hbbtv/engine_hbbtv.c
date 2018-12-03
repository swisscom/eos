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

#define HBBTV_MODULE_NAME "hbbtv"
#define MODULE_NAME "core:engine:"HBBTV_MODULE_NAME

// *************************************
// *             Includes              *
// *************************************

#include "engine_factory.h"
#include "osi_time.h"
#include "osi_memory.h"
#include "eos_macro.h"
#include "util_tsparser.h"
#include "util_slist.h"
#include "util_log.h"
#include <string.h>
#include <stdio.h>

// *************************************
// *              Macros               *
// *************************************

#define ENGINE_TYPE ENGINE_TYPE_HBBTV
#define ENGINE_CODEC EOS_MEDIA_CODEC_HBBTV

// *************************************
// *              Types                *
// *************************************

typedef struct engine_hbbt_handle
{
	uint64_t product_id;
	uint8_t red_button_url[513];
	uint16_t url_len;
	psi_table_arrival_info_t ait_arrival_info;
	engine_cb_data_t data_cb;
	util_slist_t app_descriptors;
} engine_hbbt_handle_t;

// *************************************
// *            Prototypes             *
// *************************************

static void engine_hbbtv_register (void);
static void engine_hbbtv_unregister (void);

static eos_error_t engine_hbbtv_manufacture (engine_params_t* params, engine_t* model,
                uint64_t model_id, engine_t** product, uint64_t product_id);
static eos_error_t engine_hbbtv_dismantle (uint64_t model_id,
                engine_t** product);

static const char* engine_hbbtv_name (void);
static eos_error_t engine_hbbtv_probe (eos_media_codec_t codec);
static eos_error_t engine_hbbtv_get_api (engine_t* engine, engine_api_t* api);
static eos_error_t engine_hbbtv_get_hook (engine_t* engine, engine_in_hook_t* hook);
static eos_error_t engine_hbbtv_flush (engine_t* engine);
static eos_error_t engine_hbbtv_enable (engine_t* engine);
static eos_error_t engine_hbbtv_disable (engine_t* engine);

static eos_error_t engine_hbbtv_hook (engine_t* engine, uint8_t* data, uint32_t size);

static eos_error_t engine_hbbtv_get_red_btn_url_len (engine_t* engine, uint16_t* url_len);
static eos_error_t engine_hbbtv_get_red_btn_url (engine_t* engine, uint8_t** url);


// *************************************
// *         Global variables          *
// *************************************

static engine_t engine_hbbtv_model =
{
	.handle = NULL,
	.name = engine_hbbtv_name,
	.probe = engine_hbbtv_probe,
	.get_api = engine_hbbtv_get_api,
	.get_hook = engine_hbbtv_get_hook,
	.flush = engine_hbbtv_flush,
	.enable = engine_hbbtv_enable,
	.disable = engine_hbbtv_disable
};

static engine_api_t hbbtv_api =
{
	.type = ENGINE_TYPE_HBBTV,
	.func.hbbtv.get_red_btn_url_len = engine_hbbtv_get_red_btn_url_len,
	.func.hbbtv.get_red_btn_url = engine_hbbtv_get_red_btn_url,
};

uint64_t engine_hbbtv_model_id = 0;

static char json_str1[] = "{\"application_descriptors\":[";
static char json_str2[] = "{\"app_control\":\"";
static char json_str3[] = "\", \"app_id\":\"";
static char json_str4[] = "\", \"org_id\":\"";
static char json_str5[] = "\", \"base_url\":\"";
static char json_str6[] = "\", \"initial_path\":\"";
static char json_str7[] = "\"},";
static char json_str8[] = "]}";

// *************************************
// *             Threads               *
// *************************************

// *************************************
// *         Local functions           *
// *************************************

static bool app_desc_compare(void* search_param, void* data_address)
{
	ait_desc_app_info_t *search = (ait_desc_app_info_t*)search_param;
	ait_desc_app_info_t *element = (ait_desc_app_info_t*)data_address;

	if (search->application_id != element->application_id)
	{
		return false;
	}
	if (search->organisation_id != element->organisation_id)
	{
		return false;
	}
	if (search->application_control_code != element->application_control_code)
	{
		return false;
	}
	if (strcmp(search->url_base, element->url_base) != 0)
	{
		return false;
	}
	if (strcmp(search->initial_path, element->initial_path) != 0)
	{
		return false;
	}
	return true;
}

CALL_ON_LOAD(engine_hbbtv_register)
static void engine_hbbtv_register (void)
{
	osi_time_t timestamp = {0, 0};

	if (engine_hbbtv_model_id == 0LL)
	{
		osi_time_usleep(4000); // Add randomnes to model_id
		osi_time_get_timestamp(&timestamp);
		engine_hbbtv_model_id = (timestamp.sec) * 1000000000LL + timestamp.nsec / 1;
	}

	engine_factory_register(&engine_hbbtv_model, &engine_hbbtv_model_id,
	                        engine_hbbtv_manufacture, engine_hbbtv_dismantle);
}

CALL_ON_UNLOAD(engine_hbbtv_unregister)
static void engine_hbbtv_unregister (void)
{
	engine_factory_unregister(&engine_hbbtv_model, engine_hbbtv_model_id);
}

static eos_error_t engine_hbbtv_manufacture (engine_params_t* params, engine_t* model,
                uint64_t model_id, engine_t** product, uint64_t product_id)
{
	eos_error_t eos_error = EOS_ERROR_OK;
	engine_t *temp_product = NULL;
	engine_hbbt_handle_t *handle = NULL;

	if ((params == NULL) || (model == NULL) || (product == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (model_id != engine_hbbtv_model_id)
	{
		return EOS_ERROR_INVAL;
	}

	if ((params->cb.func == NULL) || (!(params->codec & ENGINE_CODEC)))
	{
		return EOS_ERROR_INVAL;
	}

	temp_product = (engine_t*)osi_calloc(sizeof(engine_t));
	if (temp_product == NULL)
	{
		return EOS_ERROR_NOMEM;
	}

	osi_memcpy(temp_product, model, sizeof(engine_t));

	handle = (engine_hbbt_handle_t*)osi_calloc(sizeof(engine_hbbt_handle_t));
	if (handle == NULL)
	{
		osi_free((void**)temp_product);
		return EOS_ERROR_NOMEM;
	}
	handle->product_id = product_id;
	handle->data_cb = params->cb;
	eos_error = util_slist_create(&handle->app_descriptors, app_desc_compare);
	if (eos_error != EOS_ERROR_OK)
	{
		osi_free((void**)temp_product);
		return EOS_ERROR_GENERAL;
	}

	temp_product->handle = (engine_handle_t*)handle;
	*product = temp_product;

	return EOS_ERROR_OK;
}

static eos_error_t engine_hbbtv_dismantle (uint64_t model_id,
                engine_t** product)
{
	eos_error_t eos_error = EOS_ERROR_OK;
	engine_hbbt_handle_t *handle = NULL;
	int32_t app_desc_count = 0;
	ait_desc_app_info_t* app_desc = NULL;

	if (product == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (model_id != engine_hbbtv_model_id)
	{
		return EOS_ERROR_INVAL;
	}

	if (*product == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	handle = (engine_hbbt_handle_t*)(*product)->handle;
	if (handle == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (handle->app_descriptors.count(handle->app_descriptors, &app_desc_count) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	if(app_desc_count)
	{
		while ((eos_error = handle->app_descriptors.first(handle->app_descriptors, (void**)&app_desc)) == EOS_ERROR_OK)
		{
			eos_error = handle->app_descriptors.remove(handle->app_descriptors, (void*)app_desc);
			if (eos_error != EOS_ERROR_OK)
			{
				return eos_error;
			}
			if (app_desc->url_base)
			{
				osi_free((void**)&app_desc->url_base);
			}
			if (app_desc->initial_path)
			{
				osi_free((void**)&app_desc->initial_path);
			}
			osi_free((void**)&app_desc);
		}
	}
	eos_error = util_slist_destroy(&handle->app_descriptors);
	if (eos_error != EOS_ERROR_OK)
	{
		return eos_error;
	}

	osi_free((void**)&handle);
	osi_free((void**)product);

	return EOS_ERROR_OK;
}


static const char* engine_hbbtv_name (void)
{
	return HBBTV_MODULE_NAME;
}

static eos_error_t engine_hbbtv_probe (eos_media_codec_t codec)
{
	if (codec == ENGINE_CODEC)
	{
		return EOS_ERROR_OK;
	}

	return EOS_ERROR_NFOUND;
}

static eos_error_t engine_hbbtv_get_api (engine_t* engine, engine_api_t* api)
{
	if ((engine == NULL) || (api == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	*api = hbbtv_api;

	return EOS_ERROR_OK;
}

static eos_error_t engine_hbbtv_get_hook (engine_t* engine, engine_in_hook_t* hook)
{
	if ((engine == NULL) || (hook == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	*hook = engine_hbbtv_hook;

	return EOS_ERROR_OK;
}

static eos_error_t engine_hbbtv_flush (engine_t* engine)
{
	engine_hbbt_handle_t *handle = NULL;

	if (engine == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	handle = (engine_hbbt_handle_t*)engine->handle;
	if (handle == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	handle->url_len = 0;
	osi_memset(&handle->ait_arrival_info, 0, sizeof(psi_table_arrival_info_t));

	return EOS_ERROR_OK;
}

static eos_error_t engine_hbbtv_enable (engine_t* engine)
{
	if (engine == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	return EOS_ERROR_NIMPLEMENTED;
}

static eos_error_t engine_hbbtv_disable (engine_t* engine)
{
	if (engine == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	return EOS_ERROR_NIMPLEMENTED;
}

static uint32_t dvb_sub_num_of_digits(uint32_t num)
{
	uint8_t num_of_digits = 0;
	if(num == 0)
	{
		return 1;
	}
	while(num)
	{
		num_of_digits++;
		num /=10;
	}
	return num_of_digits;
}

static eos_error_t format_app_desc_json_string (util_slist_t app_descriptors, uint8_t** buff, uint32_t* len)
{
	ait_desc_app_info_t* app_desc = NULL;
	eos_error_t error = EOS_ERROR_OK;
	int32_t app_desc_count = 0;
	uint8_t tmp_buff[128 * 1024] = { 0 };
	uint32_t str_pos = 0;

	if (buff == NULL || len == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	app_descriptors.count(app_descriptors, &app_desc_count);
	if (app_desc_count == 0)
	{
		return EOS_ERROR_GENERAL;
	}

	error = app_descriptors.first(app_descriptors, (void**)&app_desc);
	if (error != EOS_ERROR_OK)
	{
		return EOS_ERROR_GENERAL;
	}

	sprintf((char*)tmp_buff, "%s", json_str1);
	str_pos += strlen(json_str1);

	do
	{
		sprintf((char*)tmp_buff + str_pos, "%s", json_str2);
		str_pos += strlen(json_str2);

		sprintf((char*)tmp_buff + str_pos, "%u", app_desc->application_control_code);
		str_pos += dvb_sub_num_of_digits(app_desc->application_control_code);

		sprintf((char*)tmp_buff + str_pos, "%s", json_str3);
		str_pos += strlen(json_str3);

		sprintf((char*)tmp_buff + str_pos, "%u", app_desc->application_id);
		str_pos += dvb_sub_num_of_digits(app_desc->application_id);

		sprintf((char*)tmp_buff + str_pos, "%s", json_str4);
		str_pos += strlen(json_str4);

		sprintf((char*)tmp_buff + str_pos, "%u", app_desc->organisation_id);
		str_pos += dvb_sub_num_of_digits(app_desc->organisation_id);

		sprintf((char*)tmp_buff + str_pos, "%s", json_str5);
		str_pos += strlen(json_str5);

		sprintf((char*)tmp_buff + str_pos, "%s", app_desc->url_base);
		str_pos += strlen(app_desc->url_base);

		sprintf((char*)tmp_buff + str_pos, "%s", json_str6);
		str_pos += strlen(json_str6);

		sprintf((char*)tmp_buff + str_pos, "%s", app_desc->initial_path);
		str_pos += strlen(app_desc->initial_path);

		sprintf((char*)tmp_buff + str_pos, "%s", json_str7);
		str_pos += strlen(json_str7);

		error = app_descriptors.next(app_descriptors, (void**)&app_desc);
	}
	while (error == EOS_ERROR_OK);

	str_pos--;

	sprintf((char*)tmp_buff + str_pos, "%s", json_str8);
	str_pos += strlen(json_str8);

	*buff = (uint8_t*)osi_calloc(str_pos + 1);
	memcpy(*buff, tmp_buff, str_pos);
	*len = str_pos + 1;

	return EOS_ERROR_OK;
}

static eos_error_t engine_hbbtv_hook (engine_t* engine, uint8_t* data, uint32_t size)
{
	engine_hbbt_handle_t *handle = NULL;
	eos_error_t error = EOS_ERROR_OK;
	int32_t app_desc_count = 0;
	uint8_t* json_buff = NULL;
	uint32_t json_buff_len = 0;


	if ((engine == NULL) || (data == NULL) || (size == 0))
	{
		return EOS_ERROR_INVAL;
	}

	handle = (engine_hbbt_handle_t*)engine->handle;
	if (handle == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (!handle->ait_arrival_info.all_sec_arrived)
	{
		error = util_tsparser_parse_ait(data, &handle->ait_arrival_info, &handle->app_descriptors, size);
		if (error != EOS_ERROR_OK)
		{
			UTIL_GLOGE("Parse AIT error.");
			return error;
		}

		/*TODO: Just as a temporary solution*/
		if (handle->ait_arrival_info.all_sec_arrived)
		{
			handle->app_descriptors.count(handle->app_descriptors, &app_desc_count);
			if (app_desc_count == 0)
			{
				UTIL_GLOGE("Application descriptor missing in AIT sections");
				return EOS_ERROR_GENERAL;
			}

			error = format_app_desc_json_string(handle->app_descriptors, &json_buff, &json_buff_len);
			if (error != EOS_ERROR_OK)
			{
				UTIL_GLOGE("Error creating json string");
				return error;
			}

			handle->data_cb.func(handle->data_cb.cookie, ENGINE_TYPE_HBBTV, ENGINE_DATA_JSON, json_buff, json_buff_len);
			if (json_buff)
			{
				osi_free((void**)&json_buff);
			}
		}
	}

	return EOS_ERROR_OK;
}

static eos_error_t engine_hbbtv_get_red_btn_url (engine_t* engine, uint8_t** url)
{
	engine_hbbt_handle_t *handle = NULL;

	if ((engine == NULL) || (url == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	handle = (engine_hbbt_handle_t*)engine->handle;
	if (handle == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	osi_memcpy(*url, handle->red_button_url, handle->url_len);

	return EOS_ERROR_OK;
}

static eos_error_t engine_hbbtv_get_red_btn_url_len (engine_t* engine, uint16_t* url_len)
{
	engine_hbbt_handle_t *handle = NULL;

	if ((engine == NULL) || (url_len == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	handle = (engine_hbbt_handle_t*)engine->handle;
	if (handle == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	*url_len = handle->url_len;

	return EOS_ERROR_OK;
}

// *************************************
// *         Global functions          *
// *************************************

