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
// *             Includes              *
// *************************************

#include "util_slist.h"
#include "osi_memory.h"
#include "util_tsparser.h"
#define MODULE_NAME "ts_parser"
#include "util_log.h"

#include "bitstream/mpeg/ts.h"
#include "bitstream/mpeg/pes.h"
#include "bitstream/mpeg/psi.h"
#include "bitstream/dvb/si.h"
#include "bitstream/hbbtv/descs_list.h"
#include "bitstream/hbbtv/ait.h"

// *************************************
// *              Macros               *
// *************************************

#define IS_ECM(table_id) ((table_id == 0x80) || (table_id == 0x81))

#define MODULE_NAME "ts_parser"

#define AIT_URL_MAX_LENGTH (256)
// *************************************
// *              Types                *
// *************************************

struct util_tsparser
{
	util_slist_t pid_list;
	uint16_t pmt_pid;
};

typedef struct ts_data_t {
	int32_t psi_refcount;
	int8_t last_cc;
	uint16_t pid;
	// biTStream PSI section gathering
	uint8_t *psi_buffer;
	uint16_t psi_buffer_used;
	// ECM data
	uint8_t *ecm_buffer;
	uint16_t ecm_buffer_size;
} ts_data_t;

// *************************************
// *            Prototypes             *
// *************************************

static bool util_tsparser_pid_comparator (void* search_param, void* data_address);
static eos_media_codec_t util_tsparser_CAsysid_to_codec(uint16_t CA_sysid);
static eos_media_drm_type_t util_tsparser_CAsysid_to_type(uint16_t CA_sysid);
static eos_error_t util_tsparser_drm_from_desc(uint8_t* descs, eos_media_drm_t* drm);
static eos_error_t util_tsparser_payload_extract(util_tsparser_t *parser, ts_data_t *ts_data, const uint8_t** payload, uint8_t *length, eos_media_desc_t* desc, uint16_t pid);
static eos_error_t util_tsparser_probe_pat(util_tsparser_t* tsparser,
		uint8_t *buff, uint32_t size, uint8_t **pos);
static eos_error_t util_tsparser_parse_descriptor(uint8_t* buff, uint16_t len, uint8_t* url_base_byte,
						uint8_t* initial_path_byte, uint8_t application_control_code);

// *************************************
// *         Global variables          *
// *************************************

// *************************************
// *             Threads               *
// *************************************

// *************************************
// *         Local functions           *
// *************************************

static eos_media_codec_t util_tsparser_CAsysid_to_codec(uint16_t CA_sysid)
{
	switch (CA_sysid)
	{
		case 0x5601: return EOS_MEDIA_CODEC_VMX;
		case 0x5602: return EOS_MEDIA_CODEC_VMX;
		case 0x5603: return EOS_MEDIA_CODEC_VMX;
		case 0x5604: return EOS_MEDIA_CODEC_VMX;
		default:
			     break;
	}
	return EOS_MEDIA_CODEC_UNKNOWN;
}

static eos_media_drm_type_t util_tsparser_CAsysid_to_type(uint16_t CA_sysid)
{
	switch (CA_sysid)
	{
		case 0x5601: return EOS_MEDIA_DRM_VMX;
		case 0x5602: return EOS_MEDIA_DRM_VMX;
		case 0x5603: return EOS_MEDIA_DRM_VMX;
		case 0x5604: return EOS_MEDIA_DRM_VMX;
		default:
			     break;
	}
	return EOS_MEDIA_DRM_UNKNOWN;
}

static eos_error_t util_tsparser_drm_from_desc(uint8_t* descs, eos_media_drm_t* drm)
{
	uint16_t length = descs_get_length(descs);
	uint8_t *desc = NULL;
	uint16_t i = 0;
	uint8_t tag = 0;

	while ((desc = descl_get_desc(descs + DESCS_HEADER_SIZE, length, i++)) != NULL)
	{
		tag = desc_get_tag(desc);
		switch (tag)
		{
			case 0x09: 
				if (desc09_validate(desc))
				{
					drm->id = desc09_get_pid(desc);
					drm->system = desc09_get_sysid(desc);
					drm->type = util_tsparser_CAsysid_to_type(drm->system);
					return EOS_ERROR_OK;
				}
				return EOS_ERROR_NFOUND;
			default: 
				break;
		}
	}
	return EOS_ERROR_NFOUND;
}

static bool util_tsparser_pid_comparator (void* search_param, void* data_address)
{
	uint16_t pid = 0;
	ts_data_t *ts_data = NULL;
	if ((search_param == NULL) || (data_address == NULL))
	{
		return false;
	}
	pid = *(uint16_t*)search_param;
	ts_data = (ts_data_t*)data_address;

	if (ts_data->pid == pid)
	{
		return true;
	}
	return false;
}

static void util_tsparser_fill_ttxt_data(uint8_t* descs, eos_media_es_attr_t* attr)
{
	uint16_t length = descs_get_length(descs);
	uint8_t *desc = NULL;
	uint8_t *desc_n = NULL;
	uint16_t i = 0;
	uint8_t tag = 0;
	char *lang = NULL;
	uint8_t n = 0;

	while ((desc = descl_get_desc(descs + DESCS_HEADER_SIZE, length, i++)) != NULL)
	{
		tag = desc_get_tag(desc);
		switch (tag)
		{
			case 0x46: 
				if (!desc46_validate(desc))
				{
					break;
				}
				desc_n = desc46_get_language(desc, n);
				while ((desc_n != NULL) && (n < EOS_MEDIA_TTXT_PAGE_INFOS_MAX))
				{
					lang = attr->ttxt.page_infos[n].lang;
					osi_memcpy(lang, (void*)desc46n_get_code(desc_n), 3);
					attr->ttxt.page_infos[n].page =
						(desc46n_get_teletextmagazine(desc_n) << 8) |
						desc46n_get_teletextpage(desc_n);
					attr->ttxt.page_infos[n].type =
						desc46n_get_teletexttype(desc_n);

					n++;
					attr->ttxt.page_info_cnt = n;
					desc_n = desc46_get_language(desc, n);
				}
				break;
			case 0x56: 
				if (!desc56_validate(desc))
				{
					break;
				}
				desc_n = desc56_get_language(desc, n);
				while ((desc_n != NULL) && (n < EOS_MEDIA_TTXT_PAGE_INFOS_MAX))
				{
					lang = attr->ttxt.page_infos[n].lang;
					osi_memcpy(lang, (void*)desc56n_get_code(desc_n), 3);
					attr->ttxt.page_infos[n].page =
						(desc56n_get_teletextmagazine(desc_n) << 8) |
						desc56n_get_teletextpage(desc_n);
					attr->ttxt.page_infos[n].type =
						desc56n_get_teletexttype(desc_n);

					n++;
					attr->ttxt.page_info_cnt = n;
					desc_n = desc56_get_language(desc, n);
				}
				break;
			default: 
				break;
		}
	}

}

static eos_error_t util_tsparser_lang_from_desc(uint8_t* descs, char lang[5])
{
	uint16_t length = descs_get_length(descs);
	uint8_t *desc = NULL;
	uint16_t i = 0;
	uint8_t tag = 0;

	while ((desc = descl_get_desc(descs + DESCS_HEADER_SIZE, length, i++)) != NULL)
	{
		tag = desc_get_tag(desc);
		switch (tag)
		{
			case 0x46: 
				if (desc46_validate(desc))
				{
					memcpy(lang, desc46_get_language(desc, 0), 3);
					return EOS_ERROR_OK; // VBI TTXT
				}
				return EOS_ERROR_NFOUND;
			case 0x56: 
				if (desc56_validate(desc))
				{
					memcpy(lang, desc56_get_language(desc, 0), 3); return EOS_ERROR_OK;
				}
				return EOS_ERROR_NFOUND;
			case 0x59:
				if (desc59_validate(desc))
				{
					memcpy(lang, desc59_get_language(desc, 0), 3); return EOS_ERROR_OK;
				}
				return EOS_ERROR_NFOUND;
			case 0x0a:
				if (desc0a_validate(desc))
				{
					memcpy(lang, desc0a_get_language(desc, 0), 3); return EOS_ERROR_OK;
				}
				return EOS_ERROR_NFOUND;
			default: 
				break;
		}
	}

	return EOS_ERROR_NFOUND;
}

static eos_media_codec_t util_tsparser_codec_from_desc(uint8_t* descs)
{
	uint16_t length = descs_get_length(descs);
	uint8_t *desc = NULL;
	uint16_t i = 0;
	uint8_t tag = 0;

	while ((desc = descl_get_desc(descs + DESCS_HEADER_SIZE, length, i++)) != NULL)
	{
		tag = desc_get_tag(desc);
		switch (tag)
		{
			case 0x46:
				if (desc46_validate(desc))
				{
					return EOS_MEDIA_CODEC_TTXT; // VBI TTXT
				}
				return EOS_MEDIA_CODEC_UNKNOWN;
			case 0x56:
				if (desc56_validate(desc))
				{
					return EOS_MEDIA_CODEC_TTXT;
				}
				return EOS_MEDIA_CODEC_UNKNOWN;
			case 0x59:
				if (desc59_validate(desc))
				{
					return EOS_MEDIA_CODEC_DVB_SUB;
				}
				return EOS_MEDIA_CODEC_UNKNOWN;
			case 0x6a:
				if (desc6a_validate(desc))
				{
					return EOS_MEDIA_CODEC_AC3;
				}
				return EOS_MEDIA_CODEC_UNKNOWN;
			case 0x6f:
				/*TODO validate app signaling descriptor*/
//				if (desc6f_validate(desc))
//				{
//					return EOS_MEDIA_CODEC_HBBTV;
//				}
//				return EOS_MEDIA_CODEC_UNKNOWN;
				return EOS_MEDIA_CODEC_HBBTV;
			case 0x28:
				if (desc28_validate(desc))
				{
					return EOS_MEDIA_CODEC_H264;
				}
				return EOS_MEDIA_CODEC_UNKNOWN;
			case 0x2b:
				if (desc2b_validate(desc))
				{
					return EOS_MEDIA_CODEC_AAC;
				}
				return EOS_MEDIA_CODEC_UNKNOWN;
			case 0x7a:
				if (desc7a_validate(desc))
				{
					return EOS_MEDIA_CODEC_EAC3;
				}
				return EOS_MEDIA_CODEC_UNKNOWN;
			case 0x09:
				if (desc09_validate(desc))
				{
					return util_tsparser_CAsysid_to_codec(desc09_get_sysid(desc));
				}
				return EOS_MEDIA_CODEC_UNKNOWN;
			case 0x7c: // Need to check
				if (desc7c_validate(desc))
				{
					return EOS_MEDIA_CODEC_HEAAC;
				}
				return EOS_MEDIA_CODEC_UNKNOWN;
			default: 
				break;
		}
	}

	return EOS_MEDIA_CODEC_UNKNOWN;
}

static eos_media_codec_t util_tsparser_streamtype2codec(uint8_t stream_type)
{
	switch (stream_type)
	{
		case 0x01: return EOS_MEDIA_CODEC_MPEG1;
		case 0x02: return EOS_MEDIA_CODEC_MPEG2;
		case 0x10: return EOS_MEDIA_CODEC_MPEG4;
		case 0x1b: return EOS_MEDIA_CODEC_H264;
		case 0x24: return EOS_MEDIA_CODEC_H265;
		case 0x03: return EOS_MEDIA_CODEC_MP1;
		case 0x04: return EOS_MEDIA_CODEC_MP2;
		case 0x05: return EOS_MEDIA_CODEC_HBBTV;
		case 0x0C: return EOS_MEDIA_CODEC_DSMCC_C;
		case 0x0F: return EOS_MEDIA_CODEC_AAC;
		case 0x11: return EOS_MEDIA_CODEC_HEAAC;
		case 0x81: return EOS_MEDIA_CODEC_AC3;
		case 0x87: return EOS_MEDIA_CODEC_EAC3;
		default: return EOS_MEDIA_CODEC_UNKNOWN;
	}
}
static eos_error_t util_tsparser_payload_extract(util_tsparser_t *parser, ts_data_t *ts_data, const uint8_t** payload, uint8_t *length, eos_media_desc_t* desc, uint16_t pid)
{
	uint8_t *section = psi_assemble_payload(&ts_data->psi_buffer, &ts_data->psi_buffer_used, payload, length);
	uint16_t tid = 0;
	eos_error_t error = EOS_ERROR_NFOUND;

	if (section != NULL)
	{
		if (!psi_validate(section))
		{
			osi_free((void**)&section);
			return EOS_ERROR_GENERAL;
		}
		tid = psi_get_tableid(section);
		if (tid == PMT_TABLE_ID && parser->pmt_pid == pid)
		{
			error = util_tsparser_extract_pmt_media_desc(section, desc);
			if (error == EOS_ERROR_OK)
			{
				if ((desc->drm.type != EOS_MEDIA_DRM_NONE) && (desc->drm.size == 0))
				{
					// Stream is encrypted but ECM packet is still not found
					error = EOS_ERROR_NFOUND;
				}
			}
			osi_free((void**)&section);
			return error;
		}
		else
		{
			if (IS_ECM(tid))
			{
				ts_data->ecm_buffer_size = psi_get_length(section) + PSI_HEADER_SIZE;
				if (ts_data->ecm_buffer != NULL)
				{
					osi_free((void**)&ts_data->ecm_buffer);
					ts_data->ecm_buffer = NULL;
				}
				ts_data->ecm_buffer = osi_calloc(ts_data->ecm_buffer_size);
				if (ts_data->ecm_buffer != NULL)
				{
					memcpy(ts_data->ecm_buffer, section, ts_data->ecm_buffer_size);
				}
				//TODO Change this logic to something smarter (its not consistent to not 
				//     overwrite just the ECM part)
				if (desc->drm.size == 0)
				{
					desc->drm.size = psi_get_length(section) + PSI_HEADER_SIZE;
					memcpy(&desc->drm.data[0], section, desc->drm.size);
					if (desc->drm.type != EOS_MEDIA_DRM_NONE)
					{
						osi_free((void**)&section);
						return EOS_ERROR_OK;
					}
				}
			}
		}
		osi_free((void**)&section);
	}
	return EOS_ERROR_NFOUND;
}

static eos_error_t util_tsparser_probe_pat(util_tsparser_t* tsparser,
		uint8_t *buff, uint32_t size, uint8_t **pos)
{
	uint32_t i = 0;
	uint8_t *pkt = NULL;
	uint8_t *payload = NULL;
	uint8_t length = 0;
	uint8_t *psi = NULL;
	uint8_t *program = NULL;
	uint16_t psi_buffer_used = 0;


	if(tsparser == NULL || buff == NULL || size == 0 || pos == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	*pos = NULL;
	if((size % TS_SIZE) != 0)
	{
		return EOS_ERROR_INVAL;
	}
	for(i=0; i<size; i+=TS_SIZE)
	{
		pkt = buff + i;
		if(ts_get_pid(pkt) == PAT_PID)
		{
			payload = ts_section(pkt);
			length = pkt + TS_SIZE - payload;
			uint8_t *section = psi_assemble_payload(&psi, &psi_buffer_used,
					(const uint8_t **)&payload, &length);
			if(section != NULL)
			{
				if(pat_validate(section) == true)
				{
					int j = 0;
					while(pat_get_program(section, j) != NULL)
					{
						j++;
					}
					if(j != 1)
					{
						UTIL_GLOGW("Found more than one PAT Program %d", j);
					}
					j = 0;
					while(pat_get_program(section, j) != NULL)
					{
						program = pat_get_program(section, j);
						if(patn_get_program(program) == 0)
						{
							UTIL_GLOGD("Skipping NIT packet ID");
							j++;
							continue;
						}
						tsparser->pmt_pid = patn_get_pid(program);
						UTIL_GLOGD("PAT found (PMT PID: %d)", tsparser->pmt_pid);
						break;
					}

					free(section);
				}
			}
			psi_assemble_reset(&psi, &psi_buffer_used);
			*pos = pkt;
			return EOS_ERROR_OK;
		}
		else
		{
			//UTIL_LOGD(probe->log, "Skipping %d", ts_get_pid(pkt));
		}
	}

	return EOS_ERROR_NFOUND;
}

// *************************************
// *       Global functions            *
// *************************************

eos_error_t util_tsparser_create(util_tsparser_t** tsparser)
{
	util_tsparser_t *local_tsparser = NULL;
	eos_error_t error = EOS_ERROR_OK;

	if (tsparser == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	local_tsparser = osi_calloc(sizeof(util_tsparser_t));
	if (local_tsparser == NULL)
	{
		return EOS_ERROR_NOMEM;
	}

	error = util_slist_create(&local_tsparser->pid_list, util_tsparser_pid_comparator);
	if (error != EOS_ERROR_OK)
	{
		osi_free((void**)&local_tsparser);
		return error;
	}

	*tsparser = local_tsparser;
	return EOS_ERROR_OK;
}


eos_error_t util_tsparser_destroy (util_tsparser_t** tsparser)
{
	ts_data_t *ts_data = NULL;
	if (tsparser == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (*tsparser == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	while((*tsparser)->pid_list.first((*tsparser)->pid_list, (void**)&ts_data) == EOS_ERROR_OK)
	{
		(*tsparser)->pid_list.remove((*tsparser)->pid_list, (void*)&ts_data->pid);
		psi_assemble_reset(&ts_data->psi_buffer, &ts_data->psi_buffer_used);
		if (ts_data->ecm_buffer != NULL)
		{
			osi_free((void**)&ts_data->ecm_buffer);
			ts_data->ecm_buffer = NULL;
		}
		osi_free((void**)&ts_data);
	}
	util_slist_destroy(&(*tsparser)->pid_list);

	osi_free((void**)tsparser);

	return EOS_ERROR_OK;
}

eos_error_t util_tsparser_extract_pmt_media_desc(uint8_t* pmt, eos_media_desc_t* desc)
{
	uint8_t *es = NULL;
	eos_media_codec_t local_codec = EOS_MEDIA_CODEC_UNKNOWN;
	bool encrypted = false;

	if ((pmt == NULL) || (desc == NULL))
	{
		return EOS_ERROR_NFOUND;
	}

	if (!pmt_validate(pmt))
	{
		return EOS_ERROR_NFOUND;
	}
	local_codec = util_tsparser_codec_from_desc(pmt_get_descs(pmt));
	if (EOS_MEDIA_IS_DRM(local_codec))
	{
		util_tsparser_drm_from_desc(pmt_get_descs(pmt), &desc->drm);
	}
	while ((es = pmt_get_es(pmt, desc->es_cnt - ((encrypted == true)? 1:0))) != NULL)
	{
		desc->es[desc->es_cnt].id = pmtn_get_pid(es);
		desc->es[desc->es_cnt].selected = false;
		desc->es[desc->es_cnt].codec = util_tsparser_streamtype2codec(pmtn_get_streamtype(es));
		local_codec = util_tsparser_codec_from_desc(pmtn_get_descs(es));
		if (desc->es[desc->es_cnt].codec == EOS_MEDIA_CODEC_UNKNOWN)
		{
			desc->es[desc->es_cnt].codec = local_codec;
		}
		else
		{
			if (local_codec != EOS_MEDIA_CODEC_UNKNOWN)
			{
				if (desc->es[desc->es_cnt].codec != local_codec)
				{
					// This was unexpected, maybe we should do something here
				}
			}
		}
		util_tsparser_lang_from_desc(pmtn_get_descs(es), desc->es[desc->es_cnt].lang);
		if (desc->es[desc->es_cnt].codec == EOS_MEDIA_CODEC_TTXT)
		{
			util_tsparser_fill_ttxt_data(pmtn_get_descs(es), &desc->es[desc->es_cnt].atr);
		}
		desc->es_cnt++;
		if (desc->es_cnt == sizeof(desc->es)/sizeof(desc->es[0]))
		{
			// We are full
			break;
		}
	}

	if (desc->es_cnt == 0)
	{
		return EOS_ERROR_NFOUND;
	}
	
	if (desc->es_cnt < sizeof(desc->es)/sizeof(desc->es[0]))
	{
		desc->es[desc->es_cnt].id = pmt_get_pcrpid(pmt);
		desc->es[desc->es_cnt].selected = true;
		desc->es[desc->es_cnt].codec = EOS_MEDIA_CODEC_CLK;
		desc->es_cnt++;
	}

	return EOS_ERROR_OK;
}

#include <stdio.h>
eos_error_t util_tsparser_get_media_info (util_tsparser_t* tsparser, uint8_t* ts, uint32_t size, int16_t info_id, eos_media_desc_t* desc)
{
	uint32_t i = 0;
	uint16_t pid = 0;
	ts_data_t *ts_data = NULL;
	eos_error_t error = EOS_ERROR_OK;
	uint8_t cc = 0;
	const uint8_t *payload = NULL;
	uint8_t length = 0;
	uint8_t *ts_iterator = NULL;

	if ((ts == NULL) || (tsparser == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (info_id != INFO_ID_FIRST_FOUND)
	{
		// Need to add PAT table parsing
		return EOS_ERROR_NIMPLEMENTED;
	}

	if (tsparser->pmt_pid == 0)
	{
		error = util_tsparser_probe_pat(tsparser, ts, size, (uint8_t **)&payload);
		if (error != EOS_ERROR_OK)
		{
			return error;
		}
	}
	for (i = 0; i < size; i += TS_SIZE)
	{
		if (i + TS_SIZE > size)
		{
			// Incomplete ts packet
			return EOS_ERROR_INVAL;
		}
		ts_iterator = &ts[i];
		if (!ts_validate(ts_iterator))
		{
			return EOS_ERROR_INVAL;
		}

		pid = ts_get_pid(ts_iterator);
		error = tsparser->pid_list.get(tsparser->pid_list, (void*)&pid, (void**)&ts_data);
		switch (error)
		{
			case EOS_ERROR_OK:
				break;
			case EOS_ERROR_NFOUND:
				ts_data = osi_calloc(sizeof(ts_data_t));
				if (ts_data == NULL)
				{
					return EOS_ERROR_NOMEM;
				}
				ts_data->pid = pid;
				if (tsparser->pid_list.add(tsparser->pid_list, (void*)ts_data) != EOS_ERROR_OK)
				{
					osi_free((void**)&ts_data);
					return EOS_ERROR_GENERAL;
				}
				ts_data->last_cc = -1;
				psi_assemble_init(&ts_data->psi_buffer, &ts_data->psi_buffer_used);
				break;
			default:
				break;
		}

		cc = ts_get_cc(ts_iterator);
		if (ts_check_duplicate(cc, ts_data->last_cc) || !ts_has_payload(ts_iterator))
		{
			ts_data->last_cc = cc;	
			continue;
		}

		if (ts_data->last_cc != -1 && ts_check_discontinuity(cc, ts_data->last_cc))
		{
			psi_assemble_reset(&ts_data->psi_buffer, &ts_data->psi_buffer_used);
		}
		payload = ts_section(ts_iterator);
		length = ts_iterator + TS_SIZE - payload;
		if (!psi_assemble_empty(&ts_data->psi_buffer, &ts_data->psi_buffer_used))
		{
			if (util_tsparser_payload_extract(tsparser, ts_data, &payload, &length, desc, pid) == EOS_ERROR_OK)
			{
				return EOS_ERROR_OK;
			}
		}
		payload = ts_next_section(ts_iterator);
		length = ts_iterator + TS_SIZE - payload;
		while (length != 0)
		{
			if (util_tsparser_payload_extract(tsparser, ts_data, &payload, &length, desc, pid) == EOS_ERROR_OK)
			{
				return EOS_ERROR_OK;
			}
		}

		ts_data->last_cc = cc;	
	}
	return EOS_ERROR_NFOUND;
}

eos_error_t util_tsparser_contains_packet (uint8_t* ts, uint32_t size, int16_t pid)
{
	uint32_t i = 0;
	if (ts == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	for (i = 0; i < size; i += TS_SIZE)
	{
		if (util_tsparser_check_pid(&ts[i], pid) == EOS_ERROR_OK)
		{
			return EOS_ERROR_OK;
		}
	}
	return EOS_ERROR_NFOUND;
}

eos_error_t util_tsparser_check_pid (uint8_t* ts_packet, int16_t pid)
{
	if (ts_packet == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (!(ts_validate(ts_packet)))
	{
		return EOS_ERROR_INVAL;
	}

	return (pid == ts_get_pid(ts_packet))? EOS_ERROR_OK:EOS_ERROR_NFOUND;
}

eos_error_t util_tsparser_get_ts_payload_by_pid (uint8_t* ts, uint32_t size, uint8_t** payload, uint8_t* payload_len, int16_t pid)
{
	uint32_t i = 0;

	if ((ts == NULL) || (payload == NULL)  || (payload_len == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	for (i = 0; i < size; i += TS_SIZE)
	{
		if (i + TS_SIZE > size)
		{
			// Incomplete ts packet
			return EOS_ERROR_INVAL;
		}
		if (util_tsparser_check_pid(&ts[i], pid) == EOS_ERROR_OK)
		{
			if (!ts_has_payload(&ts[i]))
			{
				*payload = NULL;
				*payload_len = 0;
			}
			else
			{
				*payload = ts_payload(&ts[i]);
				*payload_len = *payload - &ts[i];
			}
			return EOS_ERROR_OK;
		}
	}

	return EOS_ERROR_NFOUND;
}

eos_error_t util_tsparser_parse_ait(uint8_t* section, psi_table_arrival_info_t* psi_table_arrival_info,
		util_slist_t* app_descriptors, uint32_t size)
{
	eos_error_t error = EOS_ERROR_OK;
	uint8_t* tmp_section = section;
	uint16_t common_desc_len = 0;
	uint16_t application_descriptors_loop_length = 0;
	uint16_t app_loop_len = 0;
	uint16_t application_id = 0;
	uint8_t section_number = 0;
	uint8_t application_control_code = 0;
	uint32_t index = 0;
	uint32_t organisation_id = 0;
	uint8_t url_base_byte[AIT_URL_MAX_LENGTH] = { 0 };
	uint8_t initial_path_byte[AIT_URL_MAX_LENGTH] = { 0 };

	if(!ait_validate(section, size))
	{
		UTIL_GLOGE("AIT : validation failure.");
		return EOS_ERROR_INVAL;
	}

	common_desc_len = ait_get_common_descriptors_length(tmp_section);
	psi_table_arrival_info->version = psi_get_version(tmp_section);
	UTIL_GLOGI("AIT : version = %d ", psi_table_arrival_info->version);

//	if(common_desc_len)
//	{
//		error = util_tsparser_parse_descriptor(tmp_section, common_desc_len, url_base_byte,
//				initial_path_byte, application_control_code);
//		if(error != EOS_ERROR_OK)
//		{
//			return error;
//		}
//	}

	app_loop_len = ait_get_application_loop_length(tmp_section);
	tmp_section += (common_desc_len + AIT_HEADER_SIZE + 2);

	UTIL_GLOGI("AIT : app_loop_len = %d, size = %u ", app_loop_len, size);
	while(index < app_loop_len)
	{
		organisation_id = ait_get_organisation_id(tmp_section + index);
		UTIL_GLOGI("AIT : organisation_id = %d ", organisation_id);
		index +=4 ;
		application_id = ait_get_application_id(tmp_section + index);
		UTIL_GLOGI("AIT : application_id = %d", application_id);
		index +=2;
		application_control_code = ait_get_application_control_code(tmp_section + index);
		UTIL_GLOGI("AIT : application_control_code = (0X%x) ==> %s", application_control_code, 
					ait_app_contr_codes_string(application_control_code));
		index++;
		application_descriptors_loop_length = ait_get_application_descriptors_loop_length(tmp_section + index);
		UTIL_GLOGI("AIT : application_descriptors_loop_length = %d", application_descriptors_loop_length);
		index +=2;

		if(application_descriptors_loop_length)
		{
			osi_memset(url_base_byte, 0, AIT_URL_MAX_LENGTH);
			osi_memset(initial_path_byte, 0, AIT_URL_MAX_LENGTH);

			error = util_tsparser_parse_descriptor(tmp_section + index, 
									application_descriptors_loop_length, 
									url_base_byte, initial_path_byte,
									application_control_code);
			if(error != EOS_ERROR_OK)
			{
				return error;
			}
		}

		index+= application_descriptors_loop_length;

		if (strlen((char*)url_base_byte))
		{
			ait_desc_app_info_t* app_desc = (ait_desc_app_info_t*)osi_calloc(sizeof(ait_desc_app_info_t));
			if (app_desc == NULL)
			{
				return EOS_ERROR_NOMEM;
			}

			app_desc->application_control_code = application_control_code;
			app_desc->organisation_id = organisation_id;
			app_desc->application_id = application_id;
			app_desc->url_base = (char*)osi_calloc(strlen((char*)url_base_byte) + 1);
			memcpy(app_desc->url_base, url_base_byte, strlen((char*)url_base_byte));
			app_desc->initial_path = (char*)osi_calloc(strlen((char*)initial_path_byte) + 1);
			memcpy(app_desc->initial_path, initial_path_byte, strlen((char*)initial_path_byte));
			app_descriptors->add(*app_descriptors, (void*)app_desc);

		}
	}

	if(psi_table_arrival_info->first_section_arrived)
	{
		section_number = psi_get_section(section);

		if(((psi_table_arrival_info->first_arrived_sec_num == 0) && (psi_table_arrival_info->last_section_num == section_number))
				|| ((psi_table_arrival_info->first_arrived_sec_num != 0)&&(section_number +1) == psi_table_arrival_info->last_section_num))
		{
			psi_table_arrival_info->all_sec_arrived = true;
		}
		else
		{
			psi_table_arrival_info->all_sec_arrived = false;
		}
	}
	else
	{
		psi_table_arrival_info->first_arrived_sec_num = psi_get_section(section);
		psi_table_arrival_info->last_section_num = psi_get_lastsection(section);
		psi_table_arrival_info->first_section_arrived = true;

		if((psi_table_arrival_info->first_arrived_sec_num == 0) && (psi_table_arrival_info->last_section_num == 0))
		{
			psi_table_arrival_info->all_sec_arrived = true;
		}
		else
		{
			psi_table_arrival_info->all_sec_arrived = false;
		}
	}

	return EOS_ERROR_OK;
}

static eos_error_t util_tsparser_parse_descriptor(uint8_t* buff, uint16_t len, uint8_t* url_base_byte,
						uint8_t* initial_path_byte, uint8_t application_control_code)
{
	uint16_t app_desc_index = 0;
	uint8_t tag = 0;
	uint8_t length = 0;

	while(app_desc_index < len)
	{
		uint8_t desc_len = 0;
		tag = desc_get_tag(buff + app_desc_index);

		switch (tag)
		{
			case TRANSPORT_PROTOCOL_DESCRIPTOR_TAG:
				UTIL_GLOGI("AIT : TRANSPORT_PROTOCOL_DESCRIPTOR_TAG");
				if (transport_protocol_desc_validate(buff + app_desc_index))
				{
					uint16_t protocol_id = 0;

					desc_len = desc_get_length(buff + app_desc_index);
					protocol_id = transport_protocol_get_protocol_id(buff + app_desc_index);

					switch (protocol_id)
					{
						case 0x01: /* Object Carousel */
							UTIL_GLOGI("\tAIT : TPD carousel");
							app_desc_index += (desc_len + 2);
							break;
						case 0x03: /*  Transport via HTTP over the interaction channel */
						{
							UTIL_GLOGI("\tAIT : TPD HTTP");
							app_desc_index += 5;
							//uint8_t url_extension_count = 0;

							length = transport_protocol_get_url_base_length(buff + app_desc_index);
							if(application_control_code == AIT_AUTOSTART ||
									application_control_code == AIT_PRESENT)
							{
								app_desc_index++;
								transport_protocol_get_url_base_byte(buff + app_desc_index, url_base_byte, length);
								UTIL_GLOGI("AIT_base url = %s", url_base_byte);
								app_desc_index += length;
								//url_extension_count = transport_protocol_get_url_extension_count(buff + app_desc_index);
								app_desc_index += (desc_len - 4 - length);
								/*TODO parse url extension
								 * URL_extension_count shall be zero
								 * */
							}
							else
							{
								app_desc_index += (desc_len - 3);
							}
						}
							break;
						default:
							UTIL_GLOGI("AIT : protocol_id = %x", protocol_id);
							app_desc_index += (desc_len + 2);
							break;
					}
				}
				else
				{
					return EOS_ERROR_NFOUND;
				}
				break;
			case APPLICATION_DESCRIPTOR_TAG:
				UTIL_GLOGI("AIT : APPLICATION_DESCRIPTOR_TAG");
				desc_len = desc_get_length(buff + app_desc_index);
				app_desc_index += (desc_len + 2);
				break;
			case APPLICATION_NAME_DESCRIPTOR_TAG:
				UTIL_GLOGI("AIT : APPLICATION_NAME_DESCRIPTOR_TAG");
				desc_len = desc_get_length(buff + app_desc_index);
				app_desc_index += (desc_len + 2);
				break;
			case SIMPLE_APPLICATION_LOCATION_DESCRIPTOR_TAG:
				UTIL_GLOGI("AIT : SIMPLE_APPLICATION_LOCATION_DESCRIPTOR_TAG");
				desc_len = desc_get_length(buff + app_desc_index);
				app_desc_index += 2;
				if(application_control_code == AIT_AUTOSTART ||
						application_control_code == AIT_PRESENT)
				{
					transport_protocol_get_initial_path__bytes(buff + app_desc_index, initial_path_byte, desc_len);
					UTIL_GLOGI("\tAIT : HbbTV initial path = %s", initial_path_byte);
				}
				app_desc_index += desc_len;
				break;
			case APPLICATION_USAGE_DESCRIPTOR_TAG:
				UTIL_GLOGI("AIT : APPLICATION_USAGE_DESCRIPTOR_TAG");
				desc_len = desc_get_length(buff + app_desc_index);
				app_desc_index += 3;
				break;
			default:
				UTIL_GLOGI("AIT : Descriptor tag value = %x", tag);
				desc_len = desc_get_length(buff + app_desc_index);
				app_desc_index += (desc_len + 2);
				break;
		}
	}

	return EOS_ERROR_OK;
}

eos_error_t util_tsparser_get_ifrm_pts (uint8_t* ts, uint32_t size, uint16_t pid, uint64_t* pts)
{
	uint8_t *ts_iterator = NULL;
	uint8_t *pes = NULL;
	uint32_t i = 0;

	for (i = 0; i < size; i += TS_SIZE)
	{
		if (i + TS_SIZE > size)
		{
			// Incomplete ts packet
//			UTIL_GLOGD("OUT OF SCOPE");
			return EOS_ERROR_INVAL;
		}
		ts_iterator = &ts[i];
		if (!ts_validate(ts_iterator))
		{
//			UTIL_GLOGD("NOT VALIDATED");
			continue;
		}

		if (pid != ts_get_pid(ts_iterator))
		{
			continue;
		}
		if (!ts_has_payload(ts_iterator))
		{
			//UTIL_GLOGD("NOPLD");
			continue;
		}
		if (ts_get_unitstart(ts_iterator))
		{
//			UTIL_GLOGD("RAI");
			pes = ts_payload(ts_iterator);
			if (!pes_validate(pes))
			{
//				UTIL_GLOGD("BAD PES");
				continue;
			}
			if (!pes_has_pts(pes))
			{
//				UTIL_GLOGD("NOPTS");
				continue;
			}
//			UTIL_GLOGD("PTS");
			*pts = pes_get_pts(pes);
			return EOS_ERROR_OK;
		}
	}

	return EOS_ERROR_NFOUND;
}


