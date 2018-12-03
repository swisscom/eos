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

#define MODULE_NAME "core:controler:playback:default"

// *************************************
// *             Includes              *
// *************************************

#include "eos_macro.h"
#include "eos_types.h"
#include "util_log.h"
#include "playback_ctrl_factory.h"
#include "playback_ctrl_provider.h"
#include "playback_ctrl.h"
#include "chain.h"

// *************************************
// *              Macros               *
// *************************************

// *************************************
// *              Types                *
// *************************************

// *************************************
// *            Prototypes             *
// *************************************

static eos_error_t def_playback_ctrl_provider_probe(chain_t* chain,
		playback_ctrl_func_type_t func_type);
static eos_error_t def_playback_ctrl_provider_assign(chain_t* chain,
		playback_ctrl_func_type_t func_type, playback_ctrl_t* ctrl);
static eos_error_t simple_start(playback_ctrl_t* ctrl, chain_t* chain,
		eos_media_desc_t* streams);
static eos_error_t simple_select(playback_ctrl_t* ctrl, chain_t* chain,
		eos_media_desc_t* streams, uint32_t id, bool on);
static eos_error_t simple_stop(playback_ctrl_t* ctrl, chain_t* chain);
static eos_error_t simple_start_buff(playback_ctrl_t* ctrl, chain_t* chain);
static eos_error_t simple_stop_buff(playback_ctrl_t* ctrl, chain_t* chain);
static eos_error_t simple_handle_event(playback_ctrl_t* ctrl, chain_t* chain,
		link_ev_t event, link_ev_data_t* data);
static eos_error_t simple_trickplay(playback_ctrl_t* ctrl, chain_t* chain,
		int64_t position, int16_t speed);

static eos_error_t check_av_set(eos_media_desc_t* streams);
static eos_error_t set_av(eos_media_desc_t* streams);

static eos_error_t get_video_index(eos_media_desc_t* desc, uint8_t* idx);
static eos_error_t get_audio_index(eos_media_desc_t* desc, uint8_t* idx);
static eos_error_t get_id_index(eos_media_desc_t* desc, uint32_t id, uint8_t* idx);

// *************************************
// *         Global variables          *
// *************************************

playback_ctrl_provider_t default_provider =
{
	.probe = NULL, 
	.assign = NULL
};

// *************************************
// *             Threads               *
// *************************************

// *************************************
// *         Local functions           *
// *************************************

CALL_ON_LOAD(def_playback_ctrl_provider_register)
static void def_playback_ctrl_provider_register(void)
{
	default_provider.probe = def_playback_ctrl_provider_probe;
	default_provider.assign = def_playback_ctrl_provider_assign;

	if (playback_ctrl_factory_register_provider(&default_provider) == EOS_ERROR_OK)
	{
		UTIL_GLOGI("Default provider successfully registered");
	}
	else
	{
		UTIL_GLOGE("Unable to register default provider");
	}
}

CALL_ON_UNLOAD(def_playback_ctrl_provider_unregister)
static void def_playback_ctrl_provider_unregister(void)
{
	if (playback_ctrl_factory_unregister_provider(&default_provider) == EOS_ERROR_OK)
	{
		UTIL_GLOGI("Default provider successfully unregistered");
	}
	else
	{
		UTIL_GLOGE("Unable to unregister default provider");
	}
}

static eos_error_t check_av_set(eos_media_desc_t* streams)
{
	uint8_t i = 0;
	bool audio_set = false;
	bool video_set = false;

	for (i = 0; i < streams->es_cnt; i++)
	{
		if (EOS_MEDIA_IS_AUD(streams->es[i].codec))
		{
			if (streams->es[i].selected)
			{
				if (audio_set == false)
				{
					audio_set = true;
				}
				else
				{
					// Sort of sanity check, although this
					// func should not edit streams
					streams->es[i].selected = false;
				}
			}
		}
		if (EOS_MEDIA_IS_VID(streams->es[i].codec))
		{
			if (streams->es[i].selected)
			{
				if (video_set == false)
				{
					video_set = true;
				}
				else
				{
					// Sort of sanity check, although this
					// func should not edit streams
					streams->es[i].selected = false;
				}
			}
		}
	}
	
	return (audio_set || video_set)? EOS_ERROR_OK:EOS_ERROR_NFOUND;
}

static eos_error_t set_av(eos_media_desc_t* streams)
{
	uint8_t i = 0;
	bool audio_set = false;
	bool video_set = false;

	for (i = 0; i < streams->es_cnt; i++)
	{
		if (EOS_MEDIA_IS_AUD(streams->es[i].codec) && (audio_set == false))
		{
			audio_set = true;
			streams->es[i].selected = true;
		}
		
		if (EOS_MEDIA_IS_VID(streams->es[i].codec) && (video_set == false))
		{
			video_set = true;
			streams->es[i].selected = true;
		}
	}
	return (audio_set || video_set)? EOS_ERROR_OK:EOS_ERROR_NFOUND;
}

static eos_error_t def_playback_ctrl_provider_probe(chain_t* chain, playback_ctrl_func_type_t func_type)
{
	EOS_UNUSED(chain)
	EOS_UNUSED(func_type)
	switch (func_type)
	{
		case PLAYBACK_CTRL_START:
			return EOS_ERROR_OK;
		case PLAYBACK_CTRL_SELECT:
			return EOS_ERROR_OK;
		case PLAYBACK_CTRL_STOP:
			return EOS_ERROR_OK;
		case PLAYBACK_CTRL_START_BUFF:
			return EOS_ERROR_OK;
		case PLAYBACK_CTRL_STOP_BUFF:
			return EOS_ERROR_OK;
		case PLAYBACK_CTRL_TRICKPLAY:
			return EOS_ERROR_OK;
		case PLAYBACK_CTRL_HANDLE_EVENT:
			return EOS_ERROR_OK;
		default:
			return EOS_ERROR_NFOUND;
	}
}

static eos_error_t def_playback_ctrl_provider_assign(chain_t* chain, playback_ctrl_func_type_t func_type, playback_ctrl_t* ctrl)
{
	EOS_UNUSED(chain)
	EOS_UNUSED(func_type)
	EOS_UNUSED(ctrl)
	switch (func_type)
	{
		case PLAYBACK_CTRL_START:
			ctrl->start = simple_start;
			return EOS_ERROR_OK;
		case PLAYBACK_CTRL_SELECT:
			ctrl->select = simple_select;
			return EOS_ERROR_OK;
		case PLAYBACK_CTRL_STOP:
			ctrl->stop = simple_stop;
			return EOS_ERROR_OK;
		case PLAYBACK_CTRL_START_BUFF:
			ctrl->start_buffering = simple_start_buff;
			return EOS_ERROR_OK;
		case PLAYBACK_CTRL_STOP_BUFF:
			ctrl->stop_buffering = simple_stop_buff;
			return EOS_ERROR_OK;
		case PLAYBACK_CTRL_TRICKPLAY:
			ctrl->trickplay = simple_trickplay;
			return EOS_ERROR_OK;
		case PLAYBACK_CTRL_HANDLE_EVENT:
			ctrl->handle_event = simple_handle_event;
			return EOS_ERROR_OK;
		default:
			UTIL_GLOGWTF("Unknown assign: #%d\n", func_type);
			return EOS_ERROR_NFOUND;
	}
}

static eos_error_t simple_start(playback_ctrl_t* ctrl, chain_t* chain, eos_media_desc_t* streams)
{
	source_t *source = NULL;
	sink_t *sink = NULL;
	eos_error_t error = EOS_ERROR_OK;
	uint8_t i = 0;
	link_cap_stream_sel_ctrl_t *sink_sel = NULL;
	link_cap_stream_sel_ctrl_t *source_sel = NULL;
	bool audio_selected = false;
	bool video_selected = false;
	EOS_UNUSED(ctrl)

	chain_get_source(chain, &source);
	chain_get_sink(chain, &sink);

	if ((sink == NULL) || (source == NULL))
	{
		return EOS_ERROR_GENERAL;
	}

	if (check_av_set(streams) != EOS_ERROR_OK)
	{
		if (set_av(streams) != EOS_ERROR_OK)
		{
			source->unlock(source);
			return EOS_ERROR_GENERAL;
		}
	}

	if (sink->command.get_ctrl_funcs(sink, LINK_CAP_STREAM_SEL, (void**)&sink_sel) != EOS_ERROR_OK)
	{
			source->unlock(source);
			return EOS_ERROR_GENERAL;
	}
	if (source->get_ctrl_funcs(source, LINK_CAP_STREAM_SEL, (void**)&source_sel) != EOS_ERROR_OK)
	{
		source_sel = NULL;
	}

	for (i = 0; i < streams->es_cnt; i++)
	{
		if (EOS_MEDIA_IS_AUD(streams->es[i].codec) && (streams->es[i].selected == true))
		{
			if (sink_sel->select(sink, i) == EOS_ERROR_OK)
			{
				audio_selected = true;
				if (source_sel != NULL)
				{
					if (source_sel->select(source, i) == EOS_ERROR_OK)
					{
						streams->es[i].selected = true;
					}
				}
			}
			else
			{
				streams->es[i].selected = false;
			}
		}
		if (EOS_MEDIA_IS_VID(streams->es[i].codec) && (streams->es[i].selected == true))
		{
			if (sink_sel->select(sink, i) == EOS_ERROR_OK)
			{
				video_selected = true;
				if (source_sel != NULL)
				{
					if (source_sel->select(source, i) == EOS_ERROR_OK)
					{
						streams->es[i].selected = true;
					}
				}
			}
			else
			{
				streams->es[i].selected = false;
			}
		}
	}

	if (!video_selected && !audio_selected)
	{
		source->unlock(source);
		return EOS_ERROR_GENERAL;
	}

	error = sink->command.start(sink);
	if (error != EOS_ERROR_OK)
	{
		source->unlock(source);
		return error;
	}

	error = source->resume(source);
	if (error != EOS_ERROR_OK)
	{
		return error;
	}

	return EOS_ERROR_OK;
}

static eos_error_t simple_select(playback_ctrl_t* ctrl, chain_t* chain, 
		eos_media_desc_t* streams, uint32_t id, bool on)
{
	source_t *source = NULL;
	sink_t *sink = NULL;
	link_cap_stream_sel_ctrl_t *sink_sel = NULL;
	link_cap_stream_sel_ctrl_t *source_sel = NULL;
	uint8_t audio_idx = 0;
	uint8_t video_idx = 0;
	uint8_t selected_idx = 0;
	uint8_t id_idx = 0;
	bool already_selected_type = false;
	EOS_UNUSED(ctrl)

	chain_get_source(chain, &source);
	chain_get_sink(chain, &sink);

	if ((sink == NULL) || (source == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (get_id_index(streams, id, &id_idx) != EOS_ERROR_OK)
	{
		return EOS_ERROR_NFOUND;
	}

	if (!EOS_MEDIA_IS_VID(streams->es[id_idx].codec)
	&& !EOS_MEDIA_IS_AUD(streams->es[id_idx].codec))
	{
		return EOS_ERROR_INVAL;
	}

	if (streams->es[id_idx].selected == on)
	{
		// Already in requested state
		return EOS_ERROR_OK;
	}

	if (get_video_index(streams, &video_idx) == EOS_ERROR_OK)
	{
		if ((video_idx == id_idx) && on)
		{
			// Already in requested state
			return EOS_ERROR_OK;
		}
		if (EOS_MEDIA_IS_VID(streams->es[id_idx].codec) && on)
		{
			already_selected_type = true;
			selected_idx = video_idx;
		}
	}
	if (get_audio_index(streams, &audio_idx) == EOS_ERROR_OK)
	{
		if ((audio_idx == id_idx) && on)
		{
			// Already in requested state
			return EOS_ERROR_OK;
		}
		if (EOS_MEDIA_IS_AUD(streams->es[id_idx].codec) && on)
		{
			already_selected_type = true;
			selected_idx = audio_idx;
		}
	}

	if (sink->command.get_ctrl_funcs(sink, LINK_CAP_STREAM_SEL, (void**)&sink_sel) != EOS_ERROR_OK)
	{
			return EOS_ERROR_GENERAL;
	}

	if (source->get_ctrl_funcs(source, LINK_CAP_STREAM_SEL, (void**)&source_sel) != EOS_ERROR_OK)
	{
		source_sel = NULL;
	}

	if (already_selected_type || !on)
	{
		// Deselect
		if (sink_sel->deselect(sink, selected_idx) == EOS_ERROR_OK)
		{
			streams->es[selected_idx].selected = false;
		}
		if (source_sel != NULL)
		{
			if (source_sel->deselect(source, selected_idx) == EOS_ERROR_OK)
			{
				streams->es[selected_idx].selected = false;
			}
		}
	}

	if (on)
	{
		if (sink_sel->select(sink, id_idx) == EOS_ERROR_OK)
		{
			streams->es[id_idx].selected = true;
		}
		if (source_sel != NULL)
		{
			if (source_sel->select(source, id_idx) == EOS_ERROR_OK)
			{
				streams->es[id_idx].selected = true;
			}
		}
	}

	return EOS_ERROR_OK;
}

static eos_error_t simple_stop(playback_ctrl_t* ctrl, chain_t* chain)
{
	EOS_UNUSED(ctrl)
	source_t *source = NULL;
	sink_t *sink = NULL;
	EOS_UNUSED(ctrl)

	chain_get_source(chain, &source);
	chain_get_sink(chain, &sink);

	// TODO Do not unlock, make it go to suspend
	if (source != NULL)
	{
		source->unlock(source);
	}
	if (sink != NULL)
	{
		sink->command.stop(sink);
		sink->command.flush_buffs(sink);
	}

	return EOS_ERROR_OK;
}

static eos_error_t get_id_index(eos_media_desc_t* desc, uint32_t id, uint8_t* idx)
{
	*idx = 0;
	for (*idx = 0; *idx < desc->es_cnt; (*idx)++)
	{
		if (desc->es[*idx].id == id)
		{
			return EOS_ERROR_OK;
		}
	}
	return EOS_ERROR_NFOUND;
}

static eos_error_t get_video_index(eos_media_desc_t* desc, uint8_t* idx)
{
	*idx = 0;
	for (*idx = 0; *idx < desc->es_cnt; (*idx)++)
	{
		if (EOS_MEDIA_IS_VID(desc->es[*idx].codec) && (desc->es[*idx].selected == true))
		{
			return EOS_ERROR_OK;
		}
	}
	return EOS_ERROR_NFOUND;
}

static eos_error_t get_audio_index(eos_media_desc_t* desc, uint8_t* idx)
{
	*idx = 0;
	for (*idx = 0; *idx < desc->es_cnt; (*idx)++)
	{
		if (EOS_MEDIA_IS_AUD(desc->es[*idx].codec) && (desc->es[*idx].selected == true))
		{
			return EOS_ERROR_OK;
		}
	}
	return EOS_ERROR_NFOUND;
}

static eos_error_t simple_trickplay(playback_ctrl_t* ctrl, chain_t* chain, int64_t position, int16_t speed)
{
	EOS_UNUSED(ctrl)
	source_t *source = NULL;
	sink_t *sink = NULL;
	link_cap_trickplay_t *caps = NULL;
	int16_t current_speed = 1;
	eos_media_desc_t desc;

	EOS_UNUSED(position)
	EOS_UNUSED(speed)

	chain_get_source(chain, &source);
	chain_get_sink(chain, &sink);
	chain_get_streams(chain, &desc);

	if ((sink != NULL) && (source != NULL))
	{
		if (source->get_ctrl_funcs(source, LINK_CAP_TRICKPLAY, (void**)&caps) != EOS_ERROR_OK)
		{
			return EOS_ERROR_GENERAL;
		}
		if (caps->get_speed(source, &current_speed) != EOS_ERROR_OK)
		{
		}
		if (current_speed == speed)
		{
			if (((position > 0) && (speed == 0)) || (position < 0))
			{
				return EOS_ERROR_OK;
			}
		}

		if (current_speed != 0)
		{
			sink->command.pause(sink, true);
		}

		caps->trickplay(source, -2, 0);
//		else
		if(position < 0)
		{
			if (caps->trickplay(source, position, speed) != EOS_ERROR_OK)
			{
				if (current_speed != 0)
				{
					sink->command.resume(sink);
				}

				return EOS_ERROR_GENERAL;
			}
		}

		if (!(((current_speed == 1) && (speed == 0)) || 
				((current_speed == 0) && (speed == 1) && (position < 0))))
		{
			UTIL_GLOGI("Flush buffers");
			sink->command.flush_buffs(sink);
		}

		if (speed != 0)
		{
			sink->command.resume(sink);
		}
		if(position >= 0)
		{
			if (caps->trickplay(source, position, speed) != EOS_ERROR_OK)
			{
				if (current_speed != 0)
				{
					sink->command.resume(sink);
				}

				return EOS_ERROR_GENERAL;
			}
		}
		if (speed != 0)
		{
			sink->command.resume(sink);
		}
	}

	return EOS_ERROR_OK;
}

static eos_error_t simple_start_buff(playback_ctrl_t* ctrl, chain_t* chain)
{
	EOS_UNUSED(ctrl)
	source_t *source = NULL;
	sink_t *sink = NULL;
	EOS_UNUSED(ctrl)

	chain_get_source(chain, &source);
	chain_get_sink(chain, &sink);

	if (sink != NULL)
	{
		sink->command.pause(sink, true);
	}

	return EOS_ERROR_OK;
}

static eos_error_t simple_stop_buff(playback_ctrl_t* ctrl, chain_t* chain)
{
	EOS_UNUSED(ctrl)
	source_t *source = NULL;
	sink_t *sink = NULL;
	EOS_UNUSED(ctrl)

	chain_get_source(chain, &source);
	chain_get_sink(chain, &sink);

	if (sink != NULL)
	{
		sink->command.resume(sink);
	}

	return EOS_ERROR_OK;
}

static eos_error_t simple_handle_event(playback_ctrl_t* ctrl, chain_t* chain,
		link_ev_t event, link_ev_data_t* data)
{
	source_t *source = NULL;
	//link_cap_trickplay_t *caps = NULL;
	//int16_t current_speed = 1;
	sink_t *sink = NULL;

	if(ctrl == NULL || chain == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	chain_get_source(chain, &source);
	chain_get_sink(chain, &sink);
	switch (event)
	{
		case LINK_EV_LOW_WM:
			UTIL_GLOGI("Handle LWM");
			//sink->command.pause(sink, true);
			UTIL_GLOGI("Handled LWM");
			break;
		case LINK_EV_NORMAL_WM:
			UTIL_GLOGI("Handle NWM");
			/*sink->command.resume(sink);
			if (source->get_ctrl_funcs(source, LINK_CAP_TRICKPLAY, (void**)&caps) != EOS_ERROR_OK)
			{
			}
			if (caps != NULL)
			{
				if (caps->get_speed(source, &current_speed) != EOS_ERROR_OK)
				{
				}
				caps->trickplay(source, -1, 1);
			}*/
			UTIL_GLOGI("Handled NWM");
			break;
		case LINK_EV_HIGH_WM:
			UTIL_GLOGI("Handle HWM");
			/*if (source->get_ctrl_funcs(source, LINK_CAP_TRICKPLAY, (void**)&caps) != EOS_ERROR_OK)
			{
			}
			if (caps != NULL)
			{
				caps->trickplay(source, -1, 0);
			}*/
			UTIL_GLOGI("Handled HWM");
			break;
		default:
			if (source != NULL && source->handle_event != NULL)
			{
				source->handle_event(source, event, data);
			}
			break;
	}

	return EOS_ERROR_OK;
}

// *************************************
// *         Global functions          *
// *************************************


