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


#ifndef ALSA_AREN_H_
#define ALSA_AREN_H_

#include "eos_error.h"
#include "osi_mutex.h"

#include <alsa/asoundlib.h>
#include <libavformat/avformat.h>
#include <libavresample/avresample.h>

typedef struct alsa_aren
{
	snd_pcm_t *pcm_handle;
	AVAudioResampleContext *ar_ctx;
	uint8_t ar_opened;
	osi_mutex_t *lock;
} alsa_aren_t;

eos_error_t alsa_aren_init(alsa_aren_t* aren);
eos_error_t alsa_aren_setup(alsa_aren_t* aren);
eos_error_t alsa_aren_play(alsa_aren_t* aren, AVFrame* a_frame);
eos_error_t alsa_aren_pause(alsa_aren_t* aren);
eos_error_t alsa_aren_resume(alsa_aren_t* aren);
eos_error_t alsa_aren_flush(alsa_aren_t* aren);
eos_error_t alsa_aren_silence(alsa_aren_t* aren, uint32_t milis);
eos_error_t alsa_aren_get_buff_dur(alsa_aren_t* aren, double* buffered);
eos_error_t alsa_aren_deinit(alsa_aren_t* aren);

#endif /* ALSA_AREN_H_ */
