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


#ifndef EOS_MEDIA_H_
#define EOS_MEDIA_H_

#include "eos_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EOS_MEDIA_DRM_SIZE_MAX (1024)
#define EOS_MEDIA_ES_MAX (30)
#define EOS_MEDIA_TTXT_PAGE_INFOS_MAX 10
/** defined by ISO-639 (Alpha 4) */
#define EOS_MEDIA_LANG_MAX (5)

#define EOS_MEDIA_IS(codec,codec_type) ((codec ^ codec_type) < codec_type)
#define EOS_MEDIA_IS_VID(codec) (EOS_MEDIA_IS(codec,EOS_MEDIA_CODEC_VID))
#define EOS_MEDIA_IS_AUD(codec) (EOS_MEDIA_IS(codec,EOS_MEDIA_CODEC_AUD))
#define EOS_MEDIA_IS_DAT(codec) (EOS_MEDIA_IS(codec,EOS_MEDIA_CODEC_DAT))
#define EOS_MEDIA_IS_DRM(codec) (EOS_MEDIA_IS(codec,EOS_MEDIA_CODEC_DRM))
#define EOS_MEDIA_IS_HBBTV(codec) (EOS_MEDIA_IS(codec,EOS_MEDIA_CODEC_HBBTV))

typedef enum eos_media_cont
{
	EOS_MEDIA_CONT_NONE = 0,
	EOS_MEDIA_CONT_MPEGTS,
	EOS_MEDIA_CONT_MP4
} eos_media_cont_t;

typedef enum eos_media_codec
{
	EOS_MEDIA_CODEC_VID = 0x20,
	EOS_MEDIA_CODEC_MPEG1,
	EOS_MEDIA_CODEC_MPEG2,
	EOS_MEDIA_CODEC_MPEG4,	//H.263
	EOS_MEDIA_CODEC_H264,	//AVC
	EOS_MEDIA_CODEC_H265,	//HEVC
	EOS_MEDIA_CODEC_WMV,
	EOS_MEDIA_CODEC_AUD = 0x40,
	EOS_MEDIA_CODEC_MP1,
	EOS_MEDIA_CODEC_MP2,
	EOS_MEDIA_CODEC_MP3,
	EOS_MEDIA_CODEC_AAC,
	EOS_MEDIA_CODEC_HEAAC,
	EOS_MEDIA_CODEC_AC3,
	EOS_MEDIA_CODEC_EAC3,
	EOS_MEDIA_CODEC_DTS,
	EOS_MEDIA_CODEC_LPCM,
	EOS_MEDIA_CODEC_WMA,
	EOS_MEDIA_CODEC_DAT = 0x80,
	EOS_MEDIA_CODEC_TTXT,
	EOS_MEDIA_CODEC_DVB_SUB,
	EOS_MEDIA_CODEC_CC,
	EOS_MEDIA_CODEC_CLK,
	EOS_MEDIA_CODEC_HBBTV,
	EOS_MEDIA_CODEC_DSMCC_A,
	EOS_MEDIA_CODEC_DSMCC_B,
	EOS_MEDIA_CODEC_DSMCC_C,
	EOS_MEDIA_CODEC_DSMCC_D,
	EOS_MEDIA_CODEC_DRM = 0x100,
	EOS_MEDIA_CODEC_VMX,
	EOS_MEDIA_CODEC_UNKNOWN = 0
} eos_media_codec_t;

typedef enum eos_media_drm_type
{
	EOS_MEDIA_DRM_NONE = 0,
	EOS_MEDIA_DRM_VMX,
	EOS_MEDIA_DRM_PLAYREADY,
	EOS_MEDIA_DRM_UNKNOWN
} eos_media_drm_type_t;

typedef enum eos_media_data_fmt
{
	EOS_MEDIA_FMT_JSON,
	EOS_MEDIA_FMT_NONE
} eos_media_data_fmt_t;

typedef enum eos_media_ttxt_type
{
	EOS_MEDIA_TTXT_INITIAL_PAGE = 1,
	EOS_MEDIA_TTXT_SUB,
	EOS_MEDIA_TTXT_INFO,
	EOS_MEDIA_TTXT_SCHEDULE,
	EOS_MEDIA_TTXT_CC
} eos_media_ttxt_type_t;

typedef union eos_media_es_attr
{
	struct
	{
		uint16_t fps;
		struct
		{
			uint16_t width;
			uint16_t height;
		} resolution;
	} video;
	struct
	{
		uint32_t rate;
		uint8_t ch;
	} audio;
	struct
	{
		uint8_t page_info_cnt;
		struct
		{
			char lang[EOS_MEDIA_LANG_MAX];
			eos_media_ttxt_type_t type;
			uint16_t page;
		} page_infos[EOS_MEDIA_TTXT_PAGE_INFOS_MAX];
	} ttxt;
} eos_media_es_attr_t;

typedef struct eos_media_drm
{
	uint32_t id;
	eos_media_drm_type_t type;
	uint16_t system;
	uint16_t size;
	uint8_t data[EOS_MEDIA_DRM_SIZE_MAX];
} eos_media_drm_t;

typedef struct eos_media_es
{
	uint32_t id;
	eos_media_codec_t codec;
	char lang[EOS_MEDIA_LANG_MAX];
	eos_media_es_attr_t atr;
	bool selected;
} eos_media_es_t;

typedef struct eos_media_desc
{
	eos_media_cont_t container;
	uint8_t es_cnt;
	eos_media_drm_t drm;
	eos_media_es_t es[EOS_MEDIA_ES_MAX];
} eos_media_desc_t;

typedef struct eos_media_data
{
	eos_media_codec_t codec;
	eos_media_data_fmt_t fmt;
	void *data;
	uint32_t size;
} eos_media_data_t;


static inline const char* media_codec_string(eos_media_codec_t codec)
{
	switch (codec)
	{
		case EOS_MEDIA_CODEC_MPEG1: return "MPEG1";
		case EOS_MEDIA_CODEC_MPEG2: return "MPEG2";
		case EOS_MEDIA_CODEC_MPEG4: return "MPEG4";
		case EOS_MEDIA_CODEC_H264: return "H264";
		case EOS_MEDIA_CODEC_H265: return "H265";
		case EOS_MEDIA_CODEC_MP1: return "MP1";
		case EOS_MEDIA_CODEC_MP2: return "MP2";
		case EOS_MEDIA_CODEC_MP3: return "MP3";
		case EOS_MEDIA_CODEC_AAC: return "AAC";
		case EOS_MEDIA_CODEC_HEAAC: return "HEAAC";
		case EOS_MEDIA_CODEC_AC3: return "AC3";
		case EOS_MEDIA_CODEC_EAC3: return "EAC3";
		case EOS_MEDIA_CODEC_DTS: return "DTS";
		case EOS_MEDIA_CODEC_LPCM: return "LPCM";
		case EOS_MEDIA_CODEC_TTXT: return "TTXT";
		case EOS_MEDIA_CODEC_DVB_SUB: return "DVB SUB";
		case EOS_MEDIA_CODEC_CC: return "CC";
		case EOS_MEDIA_CODEC_HBBTV: return "HBBTV";
		case EOS_MEDIA_CODEC_DSMCC_A: return "DSMCC_A";
		case EOS_MEDIA_CODEC_DSMCC_B: return "DSMCC_B";
		case EOS_MEDIA_CODEC_DSMCC_C: return "DSMCC_C";
		case EOS_MEDIA_CODEC_DSMCC_D: return "DSMCC_D";
		case EOS_MEDIA_CODEC_VMX: return "VMX";
		case EOS_MEDIA_CODEC_CLK: return "CLK";
		default: break;
	}
	return "/";
}

#ifdef __cplusplus
}
#endif

#endif /* EOS_MEDIA_H_ */

