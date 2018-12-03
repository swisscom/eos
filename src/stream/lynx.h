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


#ifndef STREAMING_CHAIN_LINK_H_
#define STREAMING_CHAIN_LINK_H_

#include <stdint.h>
#include <stdlib.h>

#include "eos_error.h"
#include "osi_time.h"
#include "eos_media.h"

typedef void* link_handle_t;

typedef enum link_es_type
{
	LINK_ES_TYPE_VIDEO,
	LINK_ES_TYPE_AUDIO
} link_es_type_t;

typedef enum link_es_eos
{
	LINK_ES_EOS_NONE = 0,
	LINK_ES_EOS_BEGIN,
	LINK_ES_EOS_END
} link_es_eos_t;

typedef enum link_io_type
{
	LINK_IO_TYPE_TS         = 1,      // Transport Stream (TS) but with unknown number of programs
	LINK_IO_TYPE_SPROG_TS   = 1 << 1, // TS containing single program
	LINK_IO_TYPE_MPROG_TS   = 1 << 2, // TS containing multiple programs
	LINK_IO_TYPE_ES         = 1 << 3  // Elementary stream(s)
} link_io_type_t;

typedef struct link_data_ext_info
{
	link_es_type_t es_type;
	int64_t pts;
	int64_t dts;
	link_es_eos_t eos;

	/* crypto info will be added here */

} link_data_ext_info_t;

/**
 * \brief Every streaming chain link I/O definition.
 * Data connection definition between each link.
 * One link defines stream chain I/O functions, while the other one expects
 * that these functions are set during stream chain linking process.
 */
typedef struct link_io
{
	/**
	 * Allocate function.
	 * Allocation is "best effort" and does not guaranty that requested size
	 * will allocated (thus actual size is returned).
	 * @param[out] buff Pointer to return buffer.
	 * @param[in,out] size Preferred buffer size in bytes.
	 * Actually allocated size will be set to this variable on return.
	 * @return EOS_ERROR_OK if everything is fine.
	 */
	eos_error_t (*allocate) (link_handle_t handle, uint8_t** buff,
			size_t* size, link_data_ext_info_t* extended_info,
			int32_t msec, uint16_t id);
	/**
	 * Commit (signal "data available") function.
	 * @param buff Data buffer to use and free.
	 * @param size Ammount of useful data.
	 * @return EOS_ERROR_OK if everything is fine.
	 */
	eos_error_t (*commit) (link_handle_t handle, uint8_t** buff, size_t size,
			link_data_ext_info_t* extended_info, int32_t msec, uint16_t id);


	link_handle_t handle;
} link_io_t;

typedef enum link_connection_error
{
	LINK_CONN_ERR_NONE = 0,
	LINK_CONN_ERR_EOF,
	LINK_CONN_ERR_GENERAL,
	LINK_CONN_ERR_READ,
	LINK_CONN_ERR_WRITE
} link_conn_err_t;

typedef enum link_playback_error
{
	LINK_PBK_ERR_DMX,
	LINK_PBK_ERR_DEC,
	LINK_PBK_ERR_SYS
} link_pbk_err_t;

typedef enum link_ev
{
	LINK_EV_CONNECTED = 1,
	LINK_EV_NO_CONNECT,
	LINK_EV_CONN_LOST,
	LINK_EV_DISCONN,
	LINK_EV_FRAME_DISP,
	LINK_EV_LOW_WM,
	LINK_EV_NORMAL_WM,
	LINK_EV_HIGH_WM,
	LINK_EV_BOS,
	LINK_EV_EOS,
	LINK_EV_PBK_ERR,
	LINK_EV_PLAY_INFO,
	LINK_EV_LAST
} link_ev_t;

typedef union link_ev_data
{
	struct
	{
		uint64_t pts;
		bool key_frm;
	} frame;
	struct
	{
		link_pbk_err_t desc;
	} err;
	struct
	{
		uint64_t begin;
		uint64_t end;
		uint64_t position;
		uint16_t speed;
	} play_info;
	struct
	{
		eos_media_desc_t media;
		link_conn_err_t reason;
	} conn_info;
} link_ev_data_t;

#define LINK_CAP_SOURCE         (0x1LL)
#define LINK_CAP_TRICKPLAY      (0x1LL << 1)
#define LINK_CAP_DECRYPT        (0x1LL << 2)
#define LINK_CAP_STREAM_SEL     (0x1LL << 3)
#define LINK_CAP_STREAM_PROV    (0x1LL << 4)
#define LINK_CAP_DATA_PROV      (0x1LL << 5)
#define LINK_CAP_AV_OUT_SET     (0x1LL << 6)
#define LINK_CAP_SINK           (0x1LL << 7)

typedef uint64_t link_cap_t;

typedef void (*link_ev_hnd_t) (link_ev_t event, link_ev_data_t* data,
		void* cookie, uint64_t link_id);
typedef eos_error_t (*link_reg_ev_hnd_t) (link_handle_t link,
		link_ev_hnd_t handler, void* cookie);
typedef eos_error_t (*link_get_ctrl_t) (link_handle_t link, link_cap_t cap,
		void** ctrl_funcs);

typedef struct link_cap_stream_sel_ctrl
{
	eos_error_t (*select) (link_handle_t link, uint8_t es_idx);
	eos_error_t (*deselect) (link_handle_t link, uint8_t es_idx);
	eos_error_t (*enable) (link_handle_t link, uint8_t es_idx);
	eos_error_t (*disable) (link_handle_t link, uint8_t es_idx);
} link_cap_stream_sel_ctrl_t;


typedef eos_error_t (*link_stream_cb_t) (void* cookie, uint8_t* data,
		uint32_t size);
typedef struct link_cap_stream_prov_ctrl
{
	eos_error_t (*attach) (link_handle_t link, uint8_t es_idx,
			link_stream_cb_t cb, void* cookie);
	eos_error_t (*detach) (link_handle_t link, uint8_t es_idx);
} link_cap_stream_prov_ctrl_t;

typedef struct link_cap_av_out_set_ctrl
{
	eos_error_t (*vid_move) (link_handle_t link, uint16_t x, uint16_t y);
	eos_error_t (*vid_scale) (link_handle_t link, uint16_t w, uint16_t h);
	eos_error_t (*aud_pass_trgh) (link_handle_t link, bool enable);
	eos_error_t (*vol_leveling) (link_handle_t link, bool enable,
			eos_out_vol_lvl_t lvl);
} link_cap_av_out_set_ctrl_t;

#define TRICKPLAY_NO_CHANGE (0xFFFFFFFF)
typedef struct link_cap_trickplay
{
	eos_error_t (*trickplay) (link_handle_t link, int64_t position,
			int16_t speed);
	eos_error_t (*get_speed) (link_handle_t link, int16_t* speed);
} link_cap_trickplay_t;

typedef eos_error_t (*link_data_prov_cb_t) (void* cookie,
		eos_media_codec_t codec, eos_media_data_t *data);

typedef struct link_cap_data_prov
{
	eos_error_t (*poll) (link_handle_t link, uint32_t id,
			eos_media_codec_t codec, eos_media_data_t **data);
	eos_error_t (*set_cbk) (link_handle_t link, link_data_prov_cb_t cbk,
			void* cookie);
	eos_error_t (*ttxt_page_set) (link_handle_t link, uint16_t page,
			uint16_t subpage);
	eos_error_t (*ttxt_page_get) (link_handle_t link, uint16_t* page,
			uint16_t* subpage);
	eos_error_t (*ttxt_next_page_get)(link_handle_t link, uint16_t* next);
	eos_error_t (*ttxt_prev_page_get)(link_handle_t link, uint16_t* previous);
	eos_error_t (*ttxt_next_subpage_get)(link_handle_t link, uint16_t* next);
	eos_error_t (*ttxt_prev_subpage_get)(link_handle_t link, uint16_t*
			previous);
	eos_error_t (*ttxt_next_block_get)(link_handle_t link, uint16_t* block);
	eos_error_t (*ttxt_next_group_get)(link_handle_t link, uint16_t* group);
	eos_error_t (*ttxt_red_page_get)(link_handle_t link, uint16_t* red);
	eos_error_t (*ttxt_green_page_get)(link_handle_t link, uint16_t* green);
	eos_error_t (*dvbsub_enable)(link_handle_t link, bool enable);
} link_cap_data_prov_t;

#endif // STREAMING_CHAIN_LINK_H_

