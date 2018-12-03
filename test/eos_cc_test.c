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


#include "eos.h"
#include "eos_macro.h"
#include "eos_types.h"
#include "osi_thread.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

static void* async_sink_stop(void* arg)
{                       
	int i = 0;
	char* uri = (char*)arg;
	int rnd = 0;
	for (i = 0; i < 200; i++)
	{
		rnd = rand();
		printf("Thread2[%d/%d]\n", i + 1, 200);
		eos_player_play(uri, NULL, EOS_OUT_MAIN_AV);
		usleep(rnd%500000);
	}
        return arg;
} 

int main(int argc, char** argv)
{
	eos_error_t error = EOS_ERROR_OK;
	osi_thread_t *sink_stop_thread = NULL;
	int i = 0;
	int rnd = 0;

	if(argc != 2)
	{
		printf("Invalid parameters\n");
		printf("Usage: %s <URI>\n", argv[0]);
		return -1;
	}

	printf("ETIMEDOUT:%d\n", ETIMEDOUT);

	if((error = eos_init()) != EOS_ERROR_OK)
	{
		return error;
	}

	osi_thread_create(&sink_stop_thread, NULL, async_sink_stop, (void*)argv[1]);
	for (i = 0; i < 200; i++)
	{
		rnd = rand();
		printf("Thread1[%d/%d]\n", i + 1, 200);
		eos_player_play(argv[1], NULL, EOS_OUT_MAIN_AV);
		printf("Thread1 done[%d/%d]\n", i + 1, 200);
		usleep(rnd%500000);
		printf("Thread1 after sleep[%d/%d]\n", i + 1, 200);
	}

	osi_thread_join(sink_stop_thread, NULL);
	osi_thread_release(&sink_stop_thread);

	eos_player_play(argv[1], NULL, EOS_OUT_MAIN_AV);
	usleep(5000000);
	eos_player_stop(EOS_OUT_MAIN_AV);

	eos_deinit();

	return 0;
}

