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


#include "alsa_aren.h"
#include "osi_memory.h"
#define MODULE_NAME "alsa_aren"
#include "util_log.h"
#include "osi_time.h"

#include <libavutil/opt.h>

#define ALSA_AREN_SAMPLE_RATE (44100)
#define ALSA_AREN_CHANNEL_NUM (2)
#define ALSA_AREN_AVFORMAT (AV_SAMPLE_FMT_S16)
#define ALSA_AREN_PCMFORMAT (SND_PCM_FORMAT_S16)
#define ALSA_AREN_CH_LAYOUT (AV_CH_LAYOUT_STEREO)

static eos_error_t alsa_aren_open_ar(alsa_aren_t* aren, AVFrame *a_frame);

eos_error_t alsa_aren_init(alsa_aren_t* aren)
{
	snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;
	const char *pcm_name = "default";
	snd_pcm_sw_params_t *swparams = NULL;
	snd_pcm_hw_params_t *hwparams = NULL;
	unsigned int periods = 16;
	snd_pcm_uframes_t boundary, chunksz;
	unsigned int supported_sample_rate = ALSA_AREN_SAMPLE_RATE;
	unsigned int channels = ALSA_AREN_CHANNEL_NUM;
	int err = 0;
	snd_pcm_format_t format;
	uint64_t msk;

	osi_memset(aren, 0, sizeof(alsa_aren_t));
	snd_pcm_hw_params_alloca(&hwparams);
	snd_pcm_sw_params_alloca(&swparams);
	if(snd_pcm_open(&aren->pcm_handle, pcm_name, stream, 0) < 0)
	{
		return EOS_ERROR_GENERAL;
	}
	if(snd_pcm_hw_params_any(aren->pcm_handle, hwparams) < 0)
	{
		return EOS_ERROR_GENERAL;
	}
	if(snd_pcm_hw_params_set_access(aren->pcm_handle, hwparams,
			SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
	{
		return EOS_ERROR_GENERAL;
	}
	if(snd_pcm_hw_params_set_periods_near(aren->pcm_handle, hwparams, &periods, 0) < 0)
	{
		return EOS_ERROR_GENERAL;
	}
	if(snd_pcm_hw_params_set_rate_near(aren->pcm_handle, hwparams,
			&supported_sample_rate, 0) < 0)
	{
		return EOS_ERROR_GENERAL;
	}
	if(snd_pcm_hw_params_set_channels_near(aren->pcm_handle, hwparams, &channels) < 0)
	{
		return EOS_ERROR_GENERAL;
	}
	if((err = snd_pcm_hw_params_set_format(aren->pcm_handle, hwparams,
			ALSA_AREN_PCMFORMAT)) < 0)
	{
		UTIL_GLOGE("Could not set sample format:%s",snd_strerror(err));
		snd_pcm_hw_params_get_format_mask(hwparams, (snd_pcm_format_mask_t*)&msk);
		UTIL_GLOGD("Supported formats:");
		for(format = 0; format <= SND_PCM_FORMAT_LAST; format++)
		{
			if(snd_pcm_format_mask_test((snd_pcm_format_mask_t*)&msk, format))
			{
				UTIL_GLOGD("%s", snd_pcm_format_name(format));
			}
		}
		return EOS_ERROR_GENERAL;
	}
	if(snd_pcm_hw_params(aren->pcm_handle, hwparams) < 0)
	{
		return EOS_ERROR_GENERAL;
	}
	if(snd_pcm_hw_params_get_period_size_min(hwparams, &chunksz, NULL) < 0)
	{
		return EOS_ERROR_GENERAL;
	}
	if(snd_pcm_sw_params_current(aren->pcm_handle, swparams) < 0)
	{
		return EOS_ERROR_GENERAL;
	}
	if(snd_pcm_sw_params_get_boundary(swparams, &boundary) < 0)
	{
		return EOS_ERROR_GENERAL;
	}
	if(snd_pcm_sw_params_set_start_threshold(aren->pcm_handle, swparams,
			chunksz / 4) < 0)
	{
		return EOS_ERROR_GENERAL;
	}
	if(snd_pcm_sw_params_set_stop_threshold(aren->pcm_handle, swparams, boundary) < 0)
	{
		return EOS_ERROR_GENERAL;
	}
	if(snd_pcm_sw_params_set_silence_size(aren->pcm_handle, swparams, boundary) < 0)
	{
		return EOS_ERROR_GENERAL;
	}
	if(snd_pcm_sw_params(aren->pcm_handle, swparams) < 0)
	{
		return EOS_ERROR_GENERAL;
	}

	return osi_mutex_create(&aren->lock);
}

eos_error_t alsa_aren_setup(alsa_aren_t* aren)
{
	if(aren == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	aren->ar_ctx = avresample_alloc_context();
	aren->ar_opened = 0;

	return EOS_ERROR_OK;
}

eos_error_t alsa_aren_open_ar(alsa_aren_t* aren, AVFrame *a_frame)
{
	AVAudioResampleContext *avr = aren->ar_ctx;
	int err;

	av_opt_set_int(avr, "in_channel_layout",  a_frame->channel_layout,  0);
	av_opt_set_int(avr, "out_channel_layout", ALSA_AREN_CH_LAYOUT,      0);
	av_opt_set_int(avr, "in_sample_rate",     a_frame->sample_rate,     0);
	av_opt_set_int(avr, "out_sample_rate",    ALSA_AREN_SAMPLE_RATE,    0);
	av_opt_set_int(avr, "in_sample_fmt",      a_frame->format,          0);
	av_opt_set_int(avr, "out_sample_fmt",     ALSA_AREN_AVFORMAT,       0);
	if((err = avresample_open(avr)) < 0)
	{
		UTIL_GLOGE("Could not open AVResample context %d", err);
		return EOS_ERROR_GENERAL;
	}

	return EOS_ERROR_OK;
}

void alsa_aren_recover(alsa_aren_t* aren, void* output, unsigned int out_linesize)
{
	int err = 0;

	if((err = snd_pcm_prepare(aren->pcm_handle)) < 0)
	{
		UTIL_GLOGE("ALSA prepare failed (%s)", snd_strerror(err));
	}
	if((err = snd_pcm_writei(aren->pcm_handle, output, out_linesize)) < 0)
	{
		if(err == -EPIPE)
		{
			alsa_aren_recover(aren, output, out_linesize);
		}
		UTIL_GLOGE("Cannot recover ALSA (%s)", snd_strerror(err));
	}
}

eos_error_t alsa_aren_play(alsa_aren_t* aren, AVFrame *a_frame)
{
	uint8_t *output = NULL;
	int out_size;
	int out_size_1;
	int err = 0;

	if(!aren->ar_opened)
	{
		alsa_aren_open_ar(aren, a_frame);
		aren->ar_opened = 1;
		if((err = snd_pcm_prepare(aren->pcm_handle)) < 0)
		{
			UTIL_GLOGE("ALSA prepare failed (%s)", snd_strerror(err));
			return EOS_ERROR_GENERAL;
		}
	}
	if(av_samples_alloc(&output, &out_size_1, ALSA_AREN_CHANNEL_NUM, a_frame->nb_samples,
			ALSA_AREN_AVFORMAT, 0) < 0)
	{
		return EOS_ERROR_GENERAL;
	}
	out_size = avresample_convert(aren->ar_ctx, &output, out_size_1, a_frame->nb_samples,
			a_frame->extended_data, a_frame->linesize[0], a_frame->nb_samples);
	if(out_size < 0)
	{
		UTIL_GLOGE("Skipping wrong AVR data");
		avresample_close(aren->ar_ctx);
		alsa_aren_open_ar(aren, a_frame);
		av_freep(&output);

		return EOS_ERROR_OK;
	}
	if((err = snd_pcm_writei(aren->pcm_handle, output, out_size)) < 0)
	{
		if(err == -EPIPE)
		{
			alsa_aren_recover(aren, output, out_size);
		}
		else
		{
			UTIL_GLOGE("Cannot write to ALSA (%s)", snd_strerror(err));
			av_freep(&output);
			return EOS_ERROR_GENERAL;
		}
	}
	av_freep(&output);

	return EOS_ERROR_OK;
}

eos_error_t alsa_aren_pause(alsa_aren_t* aren)
{
	snd_pcm_pause(aren->pcm_handle, 1);

	return EOS_ERROR_OK;
}

eos_error_t alsa_aren_resume(alsa_aren_t* aren)
{
	snd_pcm_pause(aren->pcm_handle, 0);

	return EOS_ERROR_OK;
}

eos_error_t alsa_aren_flush(alsa_aren_t* aren)
{
	if(aren->pcm_handle != NULL)
	{
		snd_pcm_drop(aren->pcm_handle);
		snd_pcm_prepare(aren->pcm_handle);
	}
	return EOS_ERROR_OK;
}

eos_error_t alsa_aren_silence(alsa_aren_t* aren, uint32_t milis)
{
	uint32_t len = milis * 176; /*44100 * 2 * 2 / 1000 ~= 176 */
	uint8_t *buff = osi_calloc(len);

	snd_pcm_writei(aren->pcm_handle, buff, len/2);
	osi_free((void**)&buff);

	return EOS_ERROR_OK;
}

eos_error_t alsa_aren_get_buff_dur(alsa_aren_t* aren, double* buffered)
{
	snd_pcm_sframes_t frames = 0;

	if(aren == NULL || buffered == NULL || aren->pcm_handle == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	snd_pcm_avail_update(aren->pcm_handle);
	if(snd_pcm_delay(aren->pcm_handle, &frames) < 0)
	{
		frames = 0;
	}
	*buffered = frames / (double)ALSA_AREN_SAMPLE_RATE;

	return EOS_ERROR_OK;
}

eos_error_t alsa_aren_deinit(alsa_aren_t* aren)
{
	if(aren == NULL || aren->pcm_handle == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	osi_mutex_lock(aren->lock);
	avresample_free(&aren->ar_ctx);
	snd_pcm_drop(aren->pcm_handle);
	snd_pcm_close(aren->pcm_handle);
	aren->pcm_handle = NULL;
	aren->ar_opened = 0;
	osi_mutex_unlock(aren->lock);
	osi_mutex_destroy(&aren->lock);

	return EOS_ERROR_OK;
}

