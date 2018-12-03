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


#include "util_msgq.h"
#include "osi_error.h"
#include "osi_memory.h"
#include "osi_mutex.h"
#include "osi_bin_sem.h"

#include <stdlib.h>

typedef struct msg_box
{
	void* data;
	size_t size;
	struct msg_box *previous;
	struct msg_box *next;
} msg_box_t;

struct util_msgq_handle
{
	msg_box_t *first;
	msg_box_t *last;
	uint32_t count;
	uint32_t max;
	osi_mutex_t *lock;
	osi_bin_sem_t *get_sem;
	osi_bin_sem_t *put_sem;
	uint8_t running;
	util_msgq_free_cbk_t free_cbk;
};

static msg_box_t* msg_box_create(void* msg_data, size_t msg_size);
static void msg_box_destroy(msg_box_t *box);
static inline eos_error_t msg_wait(osi_bin_sem_t* sem, osi_time_t* t);

eos_error_t util_msgq_create(util_msgq_t** handle, uint32_t max, util_msgq_free_cbk_t free_cbk)
{
	util_msgq_t *head = (util_msgq_t*)osi_calloc(sizeof(util_msgq_t));

	if(head == NULL)
	{
		*handle = NULL;
		return EOS_ERROR_NOMEM;
	}
	head->free_cbk = free_cbk;
	head->first = NULL;
	head->last = NULL;
	head->count = 0;
	head->max = max;
	/* Init mutex */
	if(osi_mutex_create(&(head->lock)) != EOS_ERROR_OK)
	{

	}
	osi_bin_sem_create(&(head->get_sem), false);
	osi_bin_sem_create(&(head->put_sem), false);
	head->running = 1;
	*handle = head;

	return EOS_ERROR_OK;
}

eos_error_t util_msgq_destroy(util_msgq_t** handle)
{
	util_msgq_t* queue = NULL;

	if(handle == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	queue = (util_msgq_t*)*handle;
	if(queue == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	util_msgq_pause(queue);
	util_msgq_flush(queue);
	osi_mutex_destroy(&(queue->lock));
	osi_bin_sem_destroy(&(queue->get_sem));
	osi_bin_sem_destroy(&(queue->put_sem));
	osi_free((void**)handle);

	return EOS_ERROR_OK;
}

eos_error_t util_msgq_put(util_msgq_t* handle, void* msg_data, size_t msg_size, osi_time_t* timeout)
{
	msg_box_t *box = NULL;
	eos_error_t err = EOS_ERROR_OK;

	if(handle == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	osi_mutex_lock(handle->lock);
	if(handle->max > 0)
	{
		while((handle->count + 1) > handle->max && handle->running)
		{
			osi_mutex_unlock(handle->lock);
			if((err = msg_wait(handle->put_sem, timeout)) != EOS_ERROR_OK)
			{
				return err;
			}
			osi_mutex_lock(handle->lock);
		}
	}
	if(handle->running == 0)
	{
		osi_mutex_unlock(handle->lock);

		return EOS_ERROR_PERM;
	}
	box = msg_box_create(msg_data, msg_size);
	if(box == NULL)
	{
		osi_mutex_unlock(handle->lock);
		return EOS_ERROR_NOMEM;
	}
	box->previous = handle->last;
	if(handle->last)
	{
		handle->last->next = box;
	}
	handle->last = box;
	if(handle->count == 0)
	{
		handle->first = box;
	}

	handle->count++;
	osi_mutex_unlock(handle->lock);
	/* send signal that new message arrived, if someone is waiting for that */
	osi_bin_sem_give(handle->get_sem);

	return EOS_ERROR_OK;
}

eos_error_t util_msgq_put_urgent(util_msgq_t* handle, void* msg_data, size_t msg_size)
{
	msg_box_t *box = NULL;

	if(handle == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	osi_mutex_lock(handle->lock);
	if(handle->running == 0)
	{
		osi_mutex_unlock(handle->lock);
		return EOS_ERROR_PERM;
	}
	/* If we do not have space for this message, OWERWRITE first one! */
	if((handle->max != 0) && ((handle->count + 1) > handle->max))
	{
		if(handle->free_cbk != NULL)
		{
			handle->free_cbk(handle->first->data, handle->first->size);
		}
		box = handle->first;
		box->data = msg_data;
		box->size = msg_size;
		osi_mutex_unlock(handle->lock);
		return EOS_ERROR_OK;
	}
	box = msg_box_create(msg_data, msg_size);
	if(box == NULL)
	{
		osi_mutex_unlock(handle->lock);
		return EOS_ERROR_NOMEM;
	}
	box->next = handle->first;
	handle->first = box;
	handle->count++;
	osi_mutex_unlock(handle->lock);
	/* send signal that new message arrived, if someone is waiting for that */
	osi_bin_sem_give(handle->get_sem);

	return EOS_ERROR_OK;
}

eos_error_t util_msgq_get(util_msgq_t* handle, void** msg_data, size_t* msg_size, osi_time_t* timeout)
{
	msg_box_t *box = NULL;
	eos_error_t err = EOS_ERROR_OK;

	if(handle == NULL || msg_data == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	osi_mutex_lock(handle->lock);
	while(handle->count == 0 && handle->running)
	{
		osi_mutex_unlock(handle->lock);
		if((err = msg_wait(handle->get_sem, timeout)) != EOS_ERROR_OK)
		{
			return err;
		}
		osi_mutex_lock(handle->lock);
	}
	if(handle->running == 0)
	{
		osi_mutex_unlock(handle->lock);

		return EOS_ERROR_PERM;
	}
	box = handle->first;
	handle->first = box->next;
	*msg_data = box->data;
	if(msg_size != NULL)
	{
		*msg_size = box->size;
	}
	msg_box_destroy(box);
	handle->count--;
	if(handle->count == 0)
	{
		handle->last = NULL;
	}
	osi_mutex_unlock(handle->lock);
	if(handle->max > 0)
	{
		osi_bin_sem_give(handle->put_sem);
	}

	return EOS_ERROR_OK;
}

eos_error_t util_msgq_peek(util_msgq_t* queue, void** msg_data, size_t* msg_size, uint32_t idx)
{
	msg_box_t *box = NULL;
	uint32_t i = 0;

	if(queue == NULL || msg_data == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	osi_mutex_lock(queue->lock);
	if(queue->count == 0 || idx >= queue->count)
	{
		osi_mutex_unlock(queue->lock);
		return EOS_ERROR_NFOUND;
	}
	box = queue->first;
	i = 0;
	while(i < idx)
	{
		box = box->next;
		i++;
	}
	*msg_data = box->data;
	if(msg_size != NULL)
	{
		*msg_size = box->size;
	}
	osi_mutex_unlock(queue->lock);

	return EOS_ERROR_OK;
}


eos_error_t util_msgq_pause(util_msgq_t* handle)
{
	if(handle == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	osi_mutex_lock(handle->lock);
	handle->running = 0;
	osi_mutex_unlock(handle->lock);
	osi_bin_sem_give(handle->get_sem);
	osi_bin_sem_give(handle->put_sem);

	return EOS_ERROR_OK;
}

eos_error_t util_msgq_resume(util_msgq_t* queue)
{
	if(queue == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	osi_mutex_lock(queue->lock);
	queue->running = 1;
	osi_mutex_unlock(queue->lock);

	return EOS_ERROR_OK;
}

eos_error_t util_msgq_flush(util_msgq_t* queue)
{
	unsigned int i;
	msg_box_t *box, *next;

	if(queue == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	osi_mutex_lock(queue->lock);
	for(i=0, box=queue->first; i<queue->count; i++)
	{
		next = box->next;
		if(queue->free_cbk != NULL)
		{
			queue->free_cbk(box->data, box->size);
		}
		msg_box_destroy(box);
		box = next;
	}
	queue->count = 0;
	queue->first = NULL;
	queue->last = NULL;
	osi_bin_sem_give(queue->put_sem);
	osi_mutex_unlock(queue->lock);

	return EOS_ERROR_OK;
}

eos_error_t util_msgq_count(util_msgq_t* queue, uint32_t* count)
{
	if(queue == NULL || count == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	osi_mutex_lock(queue->lock);
	*count = queue->count;
	osi_mutex_unlock(queue->lock);

	return EOS_ERROR_OK;
}

static msg_box_t* msg_box_create(void* msg_data, size_t msg_size)
{
	msg_box_t *box = (msg_box_t*)osi_calloc(sizeof(msg_box_t));

	if(box == NULL)
	{
		return NULL;
	}
	box->data = msg_data;
	box->size = msg_size;
	box->next = NULL;
	box->previous = NULL;

	return box;
}

static void msg_box_destroy(msg_box_t* box)
{
	osi_free((void**)&box);
}

static inline eos_error_t msg_wait(osi_bin_sem_t* sem, osi_time_t* t)
{
	if(t != NULL)
	{
		return osi_bin_sem_timedtake(sem, t);
	}

	return osi_bin_sem_take(sem);
}
