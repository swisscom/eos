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


#include <pthread.h>
#include <stdlib.h>

#include <osi_thread.h>
#include <osi_memory.h>

struct osi_thread_handle
{
	pthread_t pthread;
};

eos_error_t osi_thread_create(osi_thread_t** thread, osi_thread_attr_t* attr,
		osi_thread_func_t func, void* arg)
{
	osi_thread_t *handle = NULL;
	pthread_attr_t *p_attr = NULL;
	int err = 0;

	if(thread == NULL || func == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	*thread = NULL;
	if((handle = (osi_thread_t *)osi_malloc(sizeof (osi_thread_t))) == NULL)
	{
		return EOS_ERROR_NOMEM;
	}
	if((p_attr = (pthread_attr_t *)osi_malloc(sizeof (pthread_attr_t))) == NULL)
	{
		osi_free((void**) &handle);
		return EOS_ERROR_NOMEM;
	}
	pthread_attr_init(p_attr);
	if(attr == NULL || attr->type == OSI_THREAD_JOINABLE)
	{
		pthread_attr_setdetachstate(p_attr, PTHREAD_CREATE_JOINABLE);
	}
	else
	{
		pthread_attr_setdetachstate(p_attr, PTHREAD_CREATE_DETACHED);
	}
	if((err = pthread_create(&handle->pthread, p_attr, func, arg)) != 0)
	{
		osi_free((void**) &handle);
		pthread_attr_destroy(p_attr);
		osi_free((void**) &p_attr);

		return osi_error_conv(err);
	}
	pthread_attr_destroy(p_attr);
	osi_free((void**) &p_attr);

	*thread = handle;

	return EOS_ERROR_OK;
}

eos_error_t osi_thread_release(osi_thread_t** thread)
{
	if(thread == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	osi_free((void**)thread);

	return EOS_ERROR_OK;
}

eos_error_t osi_thread_join(osi_thread_t* thread, void** retval)
{
	int err = 0;

	if(thread == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if((err = pthread_join(thread->pthread, retval)) != 0)
	{
		return osi_error_conv(err);
	}

	return EOS_ERROR_OK;
}
