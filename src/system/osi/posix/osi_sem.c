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


#include <semaphore.h>
#include <pthread.h>
#include <errno.h>

#include "osi_sem.h"
#include "osi_memory.h"

struct osi_sem_handle
{
	sem_t sem;
	pthread_mutex_t lock;
	uint8_t max;
};

eos_error_t osi_sem_create(osi_sem_t** handle, uint8_t initial, uint8_t max)
{
	osi_sem_t *tmp = NULL;
	int err = 0;

	if(handle == NULL || initial > max)
	{
		return EOS_ERROR_INVAL;
	}
	*handle = NULL;
	tmp = (osi_sem_t*) osi_calloc(sizeof(osi_sem_t));
	if(tmp == NULL)
	{
		return EOS_ERROR_NOMEM;
	}
	if((err = sem_init(&tmp->sem, 0, initial)) != 0)
	{
		osi_free((void**) &tmp);
		return osi_error_conv(errno);
	}
	if((err = pthread_mutex_init(&tmp->lock, NULL)) != 0)
	{
		osi_free((void**) &tmp);
		return osi_error_conv(err);
	}
	tmp->max = max;
	*handle = tmp;

	return EOS_ERROR_OK;
}

eos_error_t osi_sem_destroy(osi_sem_t** handle)
{
	osi_sem_t *sem = NULL;

	if(handle == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	sem = (osi_sem_t*) *handle;
	sem_destroy(&sem->sem);
	pthread_mutex_destroy(&sem->lock);
	osi_free((void**)handle);

	return EOS_ERROR_OK;
}

eos_error_t osi_sem_wait(osi_sem_t* handle)
{
	osi_sem_t *sem = NULL;
	int err = 0;

	if(handle == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	sem = (osi_sem_t*) handle;
	if((err = sem_wait(&sem->sem)) != 0)
	{
		return osi_error_conv(errno);
	}

	return EOS_ERROR_OK;
}
eos_error_t osi_sem_timedwait(osi_sem_t* handle, osi_time_t* timeout)
{
	osi_sem_t *sem = NULL;
	int err = 0;
	struct timespec ts;

	if(handle == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	sem = (osi_sem_t*) handle;
	if(timeout == NULL)
	{
		return osi_sem_wait(handle);
	}
	ts.tv_sec = timeout->sec;
	ts.tv_nsec = timeout->nsec;
	if((err = sem_timedwait(&sem->sem, &ts)) != 0)
	{
		return osi_error_conv(errno);
	}

	return EOS_ERROR_OK;
}

eos_error_t osi_sem_post(osi_sem_t* handle)
{
	osi_sem_t *sem = NULL;
	int err = 0;
	int val = 0;

	if(handle == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	sem = (osi_sem_t*) handle;
	if((err = pthread_mutex_lock(&sem->lock)) != 0)
	{
		return osi_error_conv(err);
	}
	sem_getvalue(&sem->sem, &val);
	if(val == sem->max)
	{
		pthread_mutex_unlock(&sem->lock);
		return EOS_ERROR_INVAL;
	}
	if((err = pthread_mutex_unlock(&sem->lock)) != 0)
	{
		return osi_error_conv(err);
	}
	if((err = sem_post(&sem->sem)) != 0)
	{
		return osi_error_conv(errno);
	}

	return EOS_ERROR_OK;
}

