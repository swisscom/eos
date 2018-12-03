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
// *       Module name definition      *
// *************************************

#define MODULE_NAME "sbuff"

// *************************************
// *             Includes              *
// *************************************

#include "osi_mutex.h"
#include "osi_memory.h"
#include "osi_bin_sem.h"
#include "util_seq_buff.h"

#include <stdlib.h>
//TODO Remove
//#include "util_log.h"

// *************************************
// *              Macros               *
// *************************************

#define IS_FULL(buff) (((buff)->written != 0) && ((buff)->write == (buff)->read))
#define INSUFFICIENT_SPACE(buff,space) (abs((buff)->write - (buff)->read) < space)
#define IS_WRAPPED(buff) ((buff)->write < (buff)->read)
#define IS_AT_THE_END(buff,space) ((buff)->write + space >= (buff)->start + (buff)->size)

// *************************************
// *              Types                *
// *************************************

typedef enum util_seq_buff_state
{
	SEQ_BUFF_STOPPED = 0,
	SEQ_BUFF_PAUSED,
	SEQ_BUFF_EXECUTING,
} util_seq_buff_state_t;

struct util_seq_buff
{
	uint8_t *start;
	uint32_t size;
	uint32_t segment_size;

	uint8_t *read;
	uint8_t *push;
	uint8_t *write;
	uint32_t written;
	uint32_t pushed;

	osi_mutex_t *lock;
	osi_bin_sem_t *sem_rd;
	osi_bin_sem_t *sem_ph;

	bool allocated;
	util_seq_buff_state_t state;

	util_seq_buff_cb_t cb;
};

// *************************************
// *            Prototypes             *
// *************************************

static eos_error_t util_seq_buff_local_reset(util_seq_buff_t* sbuff, osi_time_t msec);

// *************************************
// *         Global variables          *
// *************************************

// *************************************
// *             Threads               *
// *************************************

// *************************************
// *         Local functions           *
// *************************************

static eos_error_t util_seq_buff_local_reset(util_seq_buff_t* sbuff, osi_time_t msec)
{
	eos_error_t error = EOS_ERROR_OK;
	bool forever = (msec.sec == 0) && (msec.nsec == 0);

	while ((sbuff->allocated) && (sbuff->pushed != 0))
	{
		if (forever)
		{
			error = osi_bin_sem_timedtake(sbuff->sem_ph, &msec);
		}
		else
		{
			error = osi_bin_sem_take(sbuff->sem_ph);
		}
		if (error != EOS_ERROR_OK)
		{
			return error;
		}
	}

	error = osi_mutex_lock(sbuff->lock);
	if (error != EOS_ERROR_OK)
	{
		return error;
	}
	sbuff->read = sbuff->start;
	sbuff->write = sbuff->start;
	sbuff->push = sbuff->start;
	sbuff->written = 0;
	if (osi_mutex_unlock(sbuff->lock) != EOS_ERROR_OK)
	{
	}

	return EOS_ERROR_OK;
}

// *************************************
// *         Global functions          *
// *************************************

eos_error_t util_seq_buff_create(util_seq_buff_t** sbuff, uint8_t* data, uint32_t size,
                                 uint32_t segment_size, util_seq_buff_cb_t* cb)
{
	eos_error_t error = EOS_ERROR_OK;
	util_seq_buff_t *temp_sbuff = NULL;

	if ((sbuff == NULL) || (data == NULL) || (cb == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	temp_sbuff = (util_seq_buff_t*)osi_calloc(sizeof(util_seq_buff_t));
	if (temp_sbuff == NULL)
	{
		return EOS_ERROR_NOMEM;
	}

	error = osi_mutex_create(&temp_sbuff->lock);
	if (error != EOS_ERROR_OK)
	{
		osi_free((void**)&temp_sbuff);
		return error;
	}

	error = osi_bin_sem_create(&temp_sbuff->sem_rd, 0);
	if (error != EOS_ERROR_OK)
	{
		osi_mutex_destroy(&temp_sbuff->lock);
		osi_free((void**)&temp_sbuff);
		return error;
	}

	error = osi_bin_sem_create(&temp_sbuff->sem_ph, 0);
	if (error != EOS_ERROR_OK)
	{
		
		osi_bin_sem_destroy(&temp_sbuff->sem_rd);
		osi_mutex_destroy(&temp_sbuff->lock);
		osi_free((void**)&temp_sbuff);
		return error;
	}

	temp_sbuff->cb = *cb;
	temp_sbuff->start = data;
	temp_sbuff->read = data;
	temp_sbuff->push = data;
	temp_sbuff->write = data;
	temp_sbuff->size = size;
	temp_sbuff->segment_size = segment_size;

	temp_sbuff->written = 0;
	temp_sbuff->pushed = 0;

	temp_sbuff->allocated = false;
	temp_sbuff->state = SEQ_BUFF_EXECUTING;

	*sbuff = temp_sbuff;

	return EOS_ERROR_OK;
}

eos_error_t util_seq_buff_destroy(util_seq_buff_t** sbuff)
{
	eos_error_t error = EOS_ERROR_OK;
	util_seq_buff_t *temp_sbuff = NULL;
	osi_time_t timeout = {0, 0};

	if (sbuff == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	temp_sbuff = *sbuff;

	error = osi_mutex_lock(temp_sbuff->lock);
	if (error != EOS_ERROR_OK)
	{
		return error;
	}
	temp_sbuff->state = SEQ_BUFF_STOPPED;
	if (osi_mutex_unlock(temp_sbuff->lock) != EOS_ERROR_OK)
	{
	}
	error = util_seq_buff_local_reset(temp_sbuff, timeout);

	if (temp_sbuff->sem_ph != NULL)
	{
		error = osi_bin_sem_destroy(&temp_sbuff->sem_ph);
		if (error != EOS_ERROR_OK)
		{
			return error;
		}
	}

	if (temp_sbuff->sem_rd != NULL)
	{
		error = osi_bin_sem_destroy(&temp_sbuff->sem_rd);
		if (error != EOS_ERROR_OK)
		{
			return error;
		}
	}

	if (temp_sbuff->lock != NULL)
	{
		error = osi_mutex_destroy(&temp_sbuff->lock);
		if (error != EOS_ERROR_OK)
		{
			return error;
		}
	}

	osi_free((void**)sbuff);

	return EOS_ERROR_OK;
}

eos_error_t util_seq_buff_allocate(util_seq_buff_t* sbuff, uint32_t size, int32_t timeout,
                                   uint8_t** address, uint32_t* allocated)
{
	eos_error_t error = EOS_ERROR_OK;
	osi_time_t msec = {0, 0};
	div_t divide = {0, 0};

	if ((sbuff == NULL) || (address == NULL) || (allocated == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	error = osi_mutex_lock(sbuff->lock);
	if (error != EOS_ERROR_OK)
	{
		return error;
	}

	if (timeout != -1)
	{
		divide = div(timeout, 1000);
		msec.sec = divide.quot;
		msec.nsec = divide.rem * 1000000;
	}
	
	while ((sbuff->allocated) && (sbuff->state == SEQ_BUFF_EXECUTING))
	{
		if (osi_mutex_unlock(sbuff->lock) != EOS_ERROR_OK)
		{
		}
		if (timeout != -1)
		{
			error = osi_bin_sem_timedtake(sbuff->sem_rd, &msec);
		}
		else
		{
			error = osi_bin_sem_take(sbuff->sem_rd);
		}
		if (error != EOS_ERROR_OK)
		{
//			UTIL_GLOGD("Allocated timed out");
			return error;
		}
		if (sbuff->state != SEQ_BUFF_EXECUTING)
		{
			return EOS_ERROR_PERM;
		}
		error = osi_mutex_lock(sbuff->lock);
		if (error != EOS_ERROR_OK)
		{
			return error;
		}
	}

	if (sbuff->state != SEQ_BUFF_EXECUTING)
	{
		if (osi_mutex_unlock(sbuff->lock) != EOS_ERROR_OK)
		{
		}
		return EOS_ERROR_PERM;
	}

	if (sbuff->written != 0)
	{
		// TODO add while for insufficient buffer space in case of wrap
		// current one only covers when its totally full
		while ((IS_FULL(sbuff)) && (sbuff->state == SEQ_BUFF_EXECUTING))
		{
			if (osi_mutex_unlock(sbuff->lock) != EOS_ERROR_OK)
			{
			}
			if (timeout != -1)
			{
				error = osi_bin_sem_timedtake(sbuff->sem_rd, &msec);
			}
			else
			{
				error = osi_bin_sem_take(sbuff->sem_rd);
			}
			if (error != EOS_ERROR_OK)
			{
//				UTIL_GLOGD("Read wait timed out");
				return error;
			}
			if (sbuff->state != SEQ_BUFF_EXECUTING)
			{
				return EOS_ERROR_PERM;
			}
			error = osi_mutex_lock(sbuff->lock);
			if (error != EOS_ERROR_OK)
			{
				return error;
			}
		}
		if (sbuff->state != SEQ_BUFF_EXECUTING)
		{
			if (osi_mutex_unlock(sbuff->lock) != EOS_ERROR_OK)
			{
			}
			return EOS_ERROR_PERM;
		}
		
		if (IS_WRAPPED(sbuff))
		{
			if (sbuff->write + size >= sbuff->read)
			{
				*allocated = sbuff->read - sbuff->write;
			}
			else
			{
				*allocated = size;
			}
		}
		else
		{
			if (IS_AT_THE_END(sbuff, size))
			{
				*allocated = (sbuff->start + sbuff->size) - sbuff->write;
			}
			else
			{
				*allocated = size;
			}
		}
	}
	else
	{
		if (sbuff->write + size >= sbuff->start + sbuff->size)
		{
			*allocated = (sbuff->start + sbuff->size) - sbuff->write;
		}
		else
		{
			*allocated = size;
		}
	}

	sbuff->allocated = true;
	*address = sbuff->write;
//	UTIL_GLOGD("Allocated: %d %p", *allocated, *address);

	if (osi_mutex_unlock(sbuff->lock) != EOS_ERROR_OK)
	{
	}

	return EOS_ERROR_OK;
}

eos_error_t util_seq_buff_commit(util_seq_buff_t* sbuff, uint8_t* address, uint32_t size)
{
	eos_error_t error = EOS_ERROR_OK;
	uint32_t push_count = 0;
	uint32_t packet_size = 0;
	uint32_t i = 0;

	if ((sbuff == NULL) || (address == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	error = osi_mutex_lock(sbuff->lock);
	if (error != EOS_ERROR_OK)
	{
		return error;
	}

	if (sbuff->write + size >= sbuff->start + sbuff->size)
	{
		sbuff->write = sbuff->start;
	}
	else
	{
		sbuff->write += size;
	}

	sbuff->written += size;

	if (sbuff->segment_size > 0)
	{
		push_count = (sbuff->written - sbuff->pushed) / sbuff->segment_size;
		packet_size = sbuff->segment_size;
	}
	else
	{
		push_count = 1;
		packet_size = size;
	}

	if (sbuff->state != SEQ_BUFF_EXECUTING)
	{
		push_count = 0;
	}
		
	for (i = 0; i < push_count; i++)
	{
		if (osi_mutex_unlock(sbuff->lock) != EOS_ERROR_OK)
		{
		}
//		UTIL_GLOGD("Consumed: %d at %p", packet_size, sbuff->push);
		error = sbuff->cb.consume(sbuff->cb.handle, sbuff->push, packet_size);
		if (error != EOS_ERROR_OK)
		{
			return error;
		}
		error = osi_mutex_lock(sbuff->lock);
		if (error != EOS_ERROR_OK)
		{
			return error;
		}
		if (sbuff->push + packet_size >= sbuff->start + sbuff->size)
		{
			sbuff->push = sbuff->start;
		}
		else
		{
			sbuff->push += packet_size;
		}
		sbuff->pushed += packet_size;
	}

	sbuff->allocated = false;
	osi_bin_sem_give(sbuff->sem_rd);

	if (osi_mutex_unlock(sbuff->lock) != EOS_ERROR_OK)
	{
	}

	return EOS_ERROR_OK;
}

eos_error_t util_seq_buff_release(util_seq_buff_t* sbuff, uint8_t* address, uint32_t size)
{
	eos_error_t error = EOS_ERROR_OK;
	
	if ((sbuff == NULL) || (address == NULL))
	{
		return EOS_ERROR_INVAL;
	}
	
	error = osi_mutex_lock(sbuff->lock);
	if (error != EOS_ERROR_OK)
	{
		return error;
	}
	
	sbuff->pushed -= size;
	sbuff->written -= size;
	
	if (sbuff->read + size >= sbuff->start + sbuff->size)
	{
		sbuff->read = sbuff->start;
	}
	else
	{
		sbuff->read += size;
	}
	
	osi_bin_sem_give(sbuff->sem_ph);
	if (osi_mutex_unlock(sbuff->lock) != EOS_ERROR_OK)
	{
	}

	return EOS_ERROR_OK;
}

eos_error_t util_seq_buff_reset(util_seq_buff_t* sbuff, int32_t timeout)
{
	eos_error_t error = EOS_ERROR_OK;
	osi_time_t msec = {0, 0};
	div_t divide = {0, 0};
	util_seq_buff_state_t cached_state = SEQ_BUFF_EXECUTING;

	if (sbuff == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	
	if (timeout != -1)
	{
		divide = div(timeout, 1000);
		msec.sec = divide.quot;
		msec.nsec = divide.rem * 1000000;
	}
	
	error = osi_mutex_lock(sbuff->lock);
	if (error != EOS_ERROR_OK)
	{
		return error;
	}

	cached_state = sbuff->state;
	sbuff->state = SEQ_BUFF_PAUSED;
	if (osi_mutex_unlock(sbuff->lock) != EOS_ERROR_OK)
	{
	}

	error = util_seq_buff_local_reset(sbuff, msec);
	error = osi_mutex_lock(sbuff->lock);
	if (error != EOS_ERROR_OK)
	{
		return error;
	}

	sbuff->state = cached_state;

	if (osi_mutex_unlock(sbuff->lock) != EOS_ERROR_OK)
	{
	}

	return error;
}

