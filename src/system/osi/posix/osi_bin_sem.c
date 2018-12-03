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



/*******************************************************************************
 *
 * Copyright (c) 2012 Vladimir Maksovic
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * * Neither Vladimir Maksovic nor the names of this software contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL VLADIMIR MAKSOVIC
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

#include "osi_bin_sem.h"
#include "osi_memory.h"

#include <stdlib.h>
#include <pthread.h>
#include <time.h>

/* ############### Binary semaphore implementation ################ */
/* It is based on great tutorial by Shun Yan Cheung on pthread and locking:
 * http://www.mathcs.emory.edu/~cheung/Courses/455/Syllabus/5c-pthreads/sync.html
 */

#define NSEC_IN_SEC 1000000000

struct osi_bin_sem_handle
{
	/**
	 * Cond. variable - used to block threads
	 */
	pthread_cond_t cv;
	/**
	 * Mutex variable - used to prevents concurrent access to the variable "flag"
	 */
	pthread_mutex_t mutex;
	/**
	 * Semaphore state: 0 = down, 1 = up
	 */
	int flag;
};

eos_error_t osi_bin_sem_create(osi_bin_sem_t** handle, bool given)
{
	osi_bin_sem_t *s = NULL;
	int err = 0;

	if(handle == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	*handle = NULL;
	/* Allocate space for bin_sema */
	s = (osi_bin_sem_t *)osi_malloc(sizeof(osi_bin_sem_t));
	if(s == NULL)
	{
		return EOS_ERROR_NOMEM;
	}
	/* Init mutex */
	if((err = pthread_mutex_init(&(s->mutex), NULL)) != 0)
	{
		osi_free((void**)s);
		return osi_error_conv(err);
	}
	/* Init cond. variable */
	if((err = pthread_cond_init(&(s->cv), NULL)) != 0)
	{
		pthread_mutex_destroy(&(s->mutex));
		osi_free((void**)&s);
		return osi_error_conv(err);
	}
	/* Set flag value */
	s->flag = given;
	*handle = s;

	return EOS_ERROR_OK;
}

eos_error_t osi_bin_sem_destroy(osi_bin_sem_t** handle)
{
	osi_bin_sem_t *s = NULL;

	if(handle == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	s = *handle;
	pthread_mutex_destroy(&(s->mutex));
	pthread_cond_destroy(&(s->cv));
	osi_free((void**)handle);;

	return EOS_ERROR_OK;
}

eos_error_t osi_bin_sem_take(osi_bin_sem_t* handle)
{
	int err = 0;

	/* Try to get exclusive access to s->flag */
	if((err = pthread_mutex_lock(&(handle->mutex))) != 0)
	{
		return osi_error_conv(err);
	}
	/* Examine the flag and wait until flag == 1 */
	while (handle->flag == 0)
	{
		if((err = pthread_cond_wait(&(handle->cv), &(handle->mutex))) != 0)
		{
			return osi_error_conv(err);
		}
	}
	/* Semaphore is successfully taken */
	handle->flag = 0;
	/* Release exclusive access to s->flag */
	if((err = pthread_mutex_unlock(&(handle->mutex))) != 0)
	{
		return osi_error_conv(err);
	}

	return EOS_ERROR_OK;
}

eos_error_t osi_bin_sem_timedtake(osi_bin_sem_t* handle, osi_time_t* timeout)
{
	struct timespec end = {0, 0};
	struct timespec start = {0, 0};
	div_t divide = {0, 0};
	eos_error_t error = EOS_ERROR_OK;
	struct timespec initial = {timeout->sec, timeout->nsec};
	int err = 0;

	clock_gettime(CLOCK_REALTIME, &start);

	end = start;

	divide = div(timeout->nsec + end.tv_nsec, NSEC_IN_SEC);

	// TODO Check for overflow
	end.tv_sec += divide.quot + timeout->sec;
	end.tv_nsec = divide.rem;

	/* Try to get exclusive access to handle->flag */
	pthread_mutex_lock(&(handle->mutex));
	/* Examine the flag and wait until flag == 1 */
	if(handle->flag == 0)
	{
		if((err = pthread_cond_timedwait(&(handle->cv), &(handle->mutex), &end)) != 0)
		{
			error = osi_error_conv(err);
			timeout->sec = 0;
			timeout->nsec = 0;
		}
		else
		{
			clock_gettime(CLOCK_REALTIME, &end);
			if (end.tv_nsec < start.tv_nsec)
			{
				end.tv_nsec += NSEC_IN_SEC;
				if (end.tv_sec <= start.tv_sec)
				{
					end.tv_nsec = start.tv_nsec;
					end.tv_sec = start.tv_sec;
				}
				else
				{
					end.tv_sec--;
				}
			}

			if ((long)timeout->nsec < (end.tv_nsec - start.tv_nsec))
			{
				timeout->nsec += NSEC_IN_SEC;
				if ((long)timeout->sec <= (end.tv_sec - start.tv_sec))
				{
					timeout->sec = 0;
					timeout->nsec = 0;
				}
				else
				{
					timeout->sec--;
				}
			}

			if ((timeout->sec != 0) && (timeout->nsec != 0))
			{
				timeout->nsec = timeout->nsec - (end.tv_nsec - start.tv_nsec);
				timeout->sec = timeout->sec - (end.tv_sec - start.tv_sec);
			}
		}
		if (((long)timeout->sec > initial.tv_sec) || (timeout->nsec >= NSEC_IN_SEC))
		{
			timeout->sec = 0;
			timeout->nsec = 0;
		}
	}

	/* Semaphore is successfully taken */
	handle->flag = 0;
	/* Release exclusive access to handle->flag */
	pthread_mutex_unlock(&(handle->mutex));

	return error;
}

eos_error_t osi_bin_sem_give(osi_bin_sem_t* handle)
{
	int err = 0;

	/* Try to get exclusive access to s->flag */
	if((err = pthread_mutex_lock(&(handle->mutex))) != 0)
	{
		return osi_error_conv(err);
	}
	/* Signal those that are waiting */
	if((err = pthread_cond_signal(&(handle->cv))) != 0)
	{
		return osi_error_conv(err);
	}
	/* Update semaphore state to Up */
	handle->flag = 1;
	/* Release exclusive access to s->flag */
	if((err = pthread_mutex_unlock(&(handle->mutex))) != 0)
	{
		return osi_error_conv(err);
	}

	return EOS_ERROR_OK;
}
