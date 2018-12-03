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


#include "util_rbuff.h"
#include "osi_mutex.h"
#include "osi_bin_sem.h"
#include "osi_memory.h"
#include "osi_time.h"

#include "eos_macro.h"
#define MODULE_NAME ((char*)__func__)
#include "util_log.h"

#include <stdlib.h>

#define UTIL_RBUFF_ENTER_CTX(handle) (osi_mutex_lock(handle->lock))
#define UTIL_RBUFF_LEAVE_CTX(handle) (osi_mutex_unlock(handle->lock))

//#define RING_BUFF_DBG_MSG (1)

/**
 * Buffer states. Buffer can be ONLY in ONE of possible states, but states are
 * defined so that bitwise or is possible.
 */
typedef enum util_rbuff_state
{
	UTIL_RING_BUFF_STATE_ACTIVE = 1,  /**< Active (normal) state. */
	UTIL_RING_BUFF_STATE_CANCELED = 2,/**< Buffering was canceled. */
	UTIL_RING_BUFF_STATE_STOPPED = 4  /**< Buffer is stopped (e.g. end of stream). */
} util_rbuff_state_t;

struct util_rbuff_handle
{
	/** Buffer */
	uint8_t *buff;
	/** Buffer size */
	uint32_t size;
	/** Accumulate window size */
	uint32_t accumulate;
	/** Notify callback, called whenever window is filled with data and available for consuming */
	util_rbuff_notify_t notify_func;
	/** Low watermark value in bytes. */
	uint32_t wm_low;
	/** High watermark value in bytes. */
	uint32_t wm_high;
	/** Watermark callback. */
	util_rbuff_wm_cb_t wm_cb;
	/** The last watermark level notified */
	util_rbuff_wm_level_t last_level;
	/** Read pointer (available data start) */
	uint8_t *read;
	/** Write pointer (free memory start) */
	uint8_t *write;
	/** Accumulation window pointer (it does not have info about the wrapped data) */
	uint8_t *acc;
	/**
	 * End of Data pointer. Used in reading mode. It marks last byte available for
	 * reading before write pointer wrapped to the buffer start.
	 */
	uint8_t *eod;
	/** Continuous data available */
	uint32_t acc_size;
	/** State */
	util_rbuff_state_t state;
	/** Buffer lock */
	osi_mutex_t *lock;
	/** Read semaphore */
	osi_bin_sem_t *read_sem;
	/** Write semaphore */
	osi_bin_sem_t *write_sem;
	util_log_t *log;
};

/**
 * Internal function which checks weather the buffer is in expected state.
 * It is internal function, and expects that buffer context is already acquired by the caller.
 * @param rb Valid buffer rbect.
 * @param states States to check against (use bitwise or to pass more than one).
 * @return EOS_ERROR_OK if buffer is in expected state, EOS_ERROR_PERM otherwise.
 */
static eos_error_t util_rbuff_check_state(util_rbuff_t* rb, util_rbuff_state_t states);
/**
 * Internal function which handles accumulation. It is used only if accumulation mechanism is
 * used. Internally, it may call notify callback (if enough data is accumulated).
 * @param rb Valid buffer rbect.
 * @param added_size How much of data (in bytes) is added to the buffer.
 * @return EOS_ERROR_OK or error code returned by the callback.
 */
static eos_error_t util_rbuff_handle_acc(util_rbuff_t* rb, uint32_t added_size);
/**
 * Internal function which handles watermark. It is used only if watermark notification
 * callback is set.
 * @param rb Valid buffer rbect.
 * @return EOS_ERROR_OK or error code returned by the watermark callback.
 */
static eos_error_t util_rbuff_handle_wm(util_rbuff_t* rb);
/**
 * Internal function which waits for enough data for read.
 * Call this function only if you are already in the "ring buffer context"!!!
 * @param rb Valid buffer rbect.
 * @param size Size to wait for.
 * @return EOS_ERROR_OK if enough data is available.
 */
static eos_error_t util_rbuff_data_wait(util_rbuff_t* rb, uint32_t size, int32_t wait);
/**
 * Internal zero-copy read function, which assumes that the caller is already in the
 * ring buffer context. It gives "best effort" read (reads <= of requested data size of continual memory)
 * @param rb Valid buffer rbect.
 * @param buff Pointer to buffer where data is available.
 * @param size Requested size.
 * @param read Actual size read.
 * @return EOS_ERROR_OK if read was successful.
 */
static eos_error_t util_rbuff_read_cont(util_rbuff_t* rb, void** buff, uint32_t size, uint32_t *read);

eos_error_t util_rbuff_create(util_rbuff_attr_t* attr, util_rbuff_t** rb)
{
	util_rbuff_t *obj = NULL;
	eos_error_t err_code = EOS_ERROR_INVAL;

	if(attr == NULL || rb == NULL)
	{
		goto done;
	}
	if(attr->size == 0 || attr->buff == NULL)
	{
		goto done;
	}
	obj = osi_calloc(sizeof(util_rbuff_t));
	if(obj == NULL)
	{
		err_code = EOS_ERROR_NOMEM;
		goto done;
	}
	if(osi_mutex_create(&(obj->lock)) != EOS_ERROR_OK)
	{
		osi_free((void**)&obj);
		rb = NULL;
		goto done;
	}
	if(attr->accumulate > (attr->size / 2))
	{
		UTIL_GLOGW("Accumulation set too high. It will be turned OFF!");
		obj->accumulate = 0;
	}
	else
	{
		obj->accumulate = attr->accumulate;
	}
	if(attr->wm_cb != NULL)
	{
		if(attr->wm_high > attr->size || attr->wm_low > attr->wm_high)
		{
			UTIL_GLOGW("Watermark is not set properly. It will be turned OFF!");
		}
		else
		{
			obj->wm_cb = attr->wm_cb;
			obj->wm_low = attr->wm_low;
			obj->wm_high = attr->wm_high;
			obj->last_level = util_rbuff_wm_low;
		}
	}
	obj->buff = attr->buff;
	obj->size = attr->size;
	obj->notify_func = attr->notify_func;
	obj->read = attr->buff;
	obj->write = attr->buff;
	obj->acc = attr->buff;
	obj->eod = NULL;
	obj->acc_size = 0;
	obj->state = UTIL_RING_BUFF_STATE_ACTIVE;
	osi_bin_sem_create(&(obj->read_sem), true);
	osi_bin_sem_create(&(obj->write_sem), true);
	util_log_create(&(obj->log), "ring buff");
	util_log_set_level(obj->log, UTIL_LOG_LEVEL_INFO | UTIL_LOG_LEVEL_WARN |
#ifdef RING_BUFF_DBG_MSG
			UTIL_LOG_LEVEL_VERB |
#endif
			UTIL_LOG_LEVEL_ERR);
	util_log_set_color(obj->log, 1);

	err_code = EOS_ERROR_OK;

done:
	if(rb != NULL)
	{
		*rb = obj;
	}
	return err_code;
}

eos_error_t util_rbuff_destroy(util_rbuff_t** rb)
{
	if(rb == NULL || (*rb) == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	osi_mutex_destroy(&(*rb)->lock);
	osi_bin_sem_destroy(&(*rb)->read_sem);
	osi_bin_sem_destroy(&(*rb)->write_sem);
	util_log_destroy(&(*rb)->log);
	osi_free((void**)rb);

	return EOS_ERROR_OK;
}

eos_error_t util_rbuff_reserve(util_rbuff_t* rb, void** buff, uint32_t size, int32_t wait)
{
	osi_time_t timeout = {0, 0};
	div_t divide = {0, 0};

	if (wait != -1)
	{
		divide = div(wait, 1000);
		timeout.sec = divide.quot;
		timeout.nsec = divide.rem * 1000000;
	}

	if(rb == NULL || buff == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if(size > rb->size)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_RBUFF_ENTER_CTX(rb);
	if(util_rbuff_check_state(rb, UTIL_RING_BUFF_STATE_ACTIVE))
	{
		UTIL_RBUFF_LEAVE_CTX(rb);
		return EOS_ERROR_PERM;
	}
	/* simple situation, there is enough space left till the end of buffer */
	if(rb->write + size <= rb->buff + rb->size)
	{
		/* don't want to overwrite read buffer partition, wait for free chunk if read is too close up-front */
		while(rb->write <= rb->read && rb->write + size > rb->read)
		{
			/* check whether the buffer is completely empty */
			if((rb->write == rb->read) && (rb->acc_size == 0))
			{
				break;
			}
			/* unlock context */
			UTIL_RBUFF_LEAVE_CTX(rb);
			/* wait for some free chunk */
			if(wait != -1)
			{
				if (osi_bin_sem_timedtake(rb->write_sem, &timeout) == EOS_ERROR_TIMEDOUT)
				{
					return EOS_ERROR_TIMEDOUT;
				}
			}
			else
			{
				osi_bin_sem_take(rb->write_sem);
			}
			UTIL_RBUFF_ENTER_CTX(rb);
			if(util_rbuff_check_state(rb, UTIL_RING_BUFF_STATE_ACTIVE))
			{
				UTIL_RBUFF_LEAVE_CTX(rb);
				return EOS_ERROR_PERM;
			}
		}
		*buff = rb->write;
		rb->write += size;
	}
	/* wrap around */
	else
	{
#ifdef RING_BUFF_DBG_MSG
		UTIL_LOGV(rb->log, "RESERVE: Wrap around %d (%p) RD %p ACC %p WR %p", size, rb->buff, rb->read, rb->acc, rb->write);
#endif
		/* try to get buffer from the beginning, and be sure that read is not overwritten */
		while((rb->read - size) < rb->buff || rb->read >= rb->write)
		{
#ifdef RING_BUFF_DBG_MSG
			UTIL_LOGV(rb->log, "RESERVE: Waiting start free buffer (%d) (%p) RD %p ACC %p WR %p", size, rb->buff, rb->read, rb->acc, rb->write);
#endif
			/* check whether the buffer is completely empty */
			if((rb->write == rb->read) && (rb->acc_size == 0))
			{
				break;
			}
			UTIL_RBUFF_LEAVE_CTX(rb);
			if(wait != -1)
			{
				if (osi_bin_sem_timedtake(rb->write_sem, &timeout) == EOS_ERROR_TIMEDOUT)
				{
					return EOS_ERROR_TIMEDOUT;
				}
			}
			else
			{
				osi_bin_sem_take(rb->write_sem);
			}
			UTIL_RBUFF_ENTER_CTX(rb);
			if(util_rbuff_check_state(rb, UTIL_RING_BUFF_STATE_ACTIVE))
			{
				UTIL_RBUFF_LEAVE_CTX(rb);
				return EOS_ERROR_PERM;
			}
		}
		/* reader must not exceed data available (current write) */
		rb->eod = rb->write;
		*buff = rb->buff;
		rb->write = rb->buff + size;
	}
	UTIL_RBUFF_LEAVE_CTX(rb);
#ifdef RING_BUFF_DBG_MSG
	UTIL_LOGV(rb->log, "RESERVE: Done %d (%p) RD %p ACC %p WR %p", size, rb->buff, rb->read, rb->acc, rb->write);
#endif

	return EOS_ERROR_OK;
}

eos_error_t util_rbuff_commit(util_rbuff_t* rb, void *buff, uint32_t size)
{
	if(rb == NULL || buff == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	UTIL_RBUFF_ENTER_CTX(rb);
	rb->acc_size += size;
#ifdef RING_BUFF_DBG_MSG
	UTIL_LOGV(rb->log, "COMMIT: Done %d (%d) RD %p ACC %p WR %p", size, rb->acc_size, rb->read, rb->acc, rb->write);
#endif
	/* Sanity check. This may be removed. */
	if(rb->acc_size > rb->size)
	{
		UTIL_RBUFF_LEAVE_CTX(rb);
		return EOS_ERROR_INVAL;
	}
	UTIL_RBUFF_LEAVE_CTX(rb);
	if(rb->wm_cb != NULL)
	{
		util_rbuff_handle_wm(rb);
	}
	/* in case of accumulation, leave buffer context and notify listener */
	if(rb->accumulate && rb->notify_func)
	{
		return util_rbuff_handle_acc(rb, size);
	}
	/* Read functionality may be used only if we don't accumulate data */
	else
	{
		osi_bin_sem_give(rb->read_sem);
#ifdef RING_BUFF_DBG_MSG
		UTIL_LOGV(rb->log, "COMMIT: GIVE!!! %d (%d) RD %p ACC %p WR %p", size, rb->acc_size, rb->read, rb->acc, rb->write);
#endif
	}

	return EOS_ERROR_OK;
}

eos_error_t util_rbuff_free(util_rbuff_t* rb, void* buff, uint32_t size)
{
	if(rb == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_RBUFF_ENTER_CTX(rb);
	/* Free will just update read pointer. It is up to the user to call it in proper order. */
	rb->read = (uint8_t*)buff + size;
	osi_bin_sem_give(rb->write_sem);
	UTIL_RBUFF_LEAVE_CTX(rb);
	if(rb->wm_cb != NULL)
	{
		return util_rbuff_handle_wm(rb);
	}

	return EOS_ERROR_OK;
}

eos_error_t util_rbuff_read(util_rbuff_t* rb, void** buff, uint32_t size, uint32_t *read, int32_t wait)
{
	eos_error_t err = EOS_ERROR_OK;

	if(rb == NULL || buff == NULL || size > rb->size || read == NULL || wait < -1)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_RBUFF_ENTER_CTX(rb);
	if((err = util_rbuff_data_wait(rb, size, wait)) != EOS_ERROR_OK)
	{
		UTIL_RBUFF_LEAVE_CTX(rb);
		return err;
	}
	err = util_rbuff_read_cont(rb, buff, size, read);
	UTIL_RBUFF_LEAVE_CTX(rb);

	return err;
}

eos_error_t util_rbuff_read_all(util_rbuff_t* rb, void *buff, uint32_t size, int32_t wait)
{
	uint32_t available = 0, to_read = size;
	void *data = NULL;
	uint8_t *dest = (uint8_t *)buff;
	eos_error_t err = EOS_ERROR_OK;

	if(rb == NULL || buff == NULL || size > rb->size || wait < -1)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_RBUFF_ENTER_CTX(rb);
#ifdef RING_BUFF_DBG_MSG
	UTIL_LOGV(rb->log, "READ ALL wait %d (%d) RD %p ACC %p WR %p", size, rb->acc_size, rb->read, rb->acc, rb->write);
#endif
	if((err = util_rbuff_data_wait(rb, size, wait)) != EOS_ERROR_OK)
	{
		UTIL_RBUFF_LEAVE_CTX(rb);
		return err;
	}
	do
	{
#ifdef RING_BUFF_DBG_MSG
		UTIL_LOGV(rb->log, "READ ALL %d (%p) RD %p ACC %p WR %p", size, rb->buff, rb->read, rb->acc, rb->write);
#endif
		if((err = util_rbuff_read_cont(rb, &data, to_read, &available)) != EOS_ERROR_OK)
		{
			UTIL_RBUFF_LEAVE_CTX(rb);
			return err;
		}
#ifdef RING_BUFF_DBG_MSG
		UTIL_LOGV(rb->log, "READ ALL %d (%p) RD %p ACC %p WR %p", size, rb->buff, rb->read, rb->acc, rb->write);
#endif
		osi_memcpy(dest, data, available);
		rb->read = (uint8_t*)data + available;
		to_read -= available;
		dest += available;
	} while(to_read);
	osi_bin_sem_give(rb->write_sem);
	UTIL_RBUFF_LEAVE_CTX(rb);

	return err;
}

eos_error_t util_rbuff_flush(util_rbuff_t* rb)
{
	if(rb == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	/* cannot be used if accumulation is not set */
	if(!rb->accumulate && !rb->notify_func)
	{
		return EOS_ERROR_GENERAL;
	}
	/* on commit, wrap around is handled, so just send what is left */
	if(rb->acc_size != 0 && rb->accumulate && rb->notify_func)
	{
		return rb->notify_func(rb, rb->acc, rb->acc_size);
	}

	return EOS_ERROR_OK;
}

eos_error_t util_rbuff_cancel(util_rbuff_t* rb)
{
	if(rb == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_RBUFF_ENTER_CTX(rb);
	rb->state = UTIL_RING_BUFF_STATE_CANCELED;
	rb->acc_size = 0;
	osi_bin_sem_give(rb->read_sem);
	osi_bin_sem_give(rb->write_sem);
	UTIL_RBUFF_LEAVE_CTX(rb);
	return EOS_ERROR_OK;
}

eos_error_t util_rbuff_stop(util_rbuff_t* rb)
{
	if(rb == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_RBUFF_ENTER_CTX(rb);
	rb->state = UTIL_RING_BUFF_STATE_STOPPED;
	osi_bin_sem_give(rb->read_sem);
	osi_bin_sem_give(rb->write_sem);
	UTIL_RBUFF_LEAVE_CTX(rb);
	return EOS_ERROR_OK;
}

eos_error_t util_rbuff_rst(util_rbuff_t* rb)
{
	if(rb == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	UTIL_RBUFF_ENTER_CTX(rb);
	rb->read = rb->buff;
	rb->write = rb->buff;
	rb->acc = rb->buff;
	rb->eod = NULL;
	rb->acc_size = 0;
	rb->last_level = util_rbuff_wm_low;
	rb->state = UTIL_RING_BUFF_STATE_ACTIVE;
	/* If someone is waiting, we have write space now */
	osi_bin_sem_give(rb->write_sem);
	UTIL_RBUFF_LEAVE_CTX(rb);

	return EOS_ERROR_OK;
}

eos_error_t util_rbuff_get_fullness(util_rbuff_t* rb, uint32_t *fullness)
{
	if(rb == NULL || fullness == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	UTIL_RBUFF_ENTER_CTX(rb);
	*fullness = rb->acc_size;
	UTIL_RBUFF_LEAVE_CTX(rb);
	return EOS_ERROR_OK;
}

static eos_error_t util_rbuff_check_state(util_rbuff_t* rb, util_rbuff_state_t states)
{
	if((rb->state & states) == 0)
	{
		return EOS_ERROR_PERM;
	}

	return EOS_ERROR_OK;
}


static eos_error_t util_rbuff_handle_acc(util_rbuff_t* rb, uint32_t added_size)
{
	void* buff = NULL;
	uint32_t size;

	UTIL_RBUFF_ENTER_CTX(rb);
	/* send notification if there is enough data accumulated,
	 * or we got to the end of the buffer */
	if(rb->acc + rb->acc_size > rb->buff + rb->size)
	{
		size = rb->acc_size - added_size;
		buff = rb->acc;
		rb->acc_size = added_size;

		rb->acc = rb->buff;
	}
	else if(rb->acc_size >= rb->accumulate)
	{
		size = rb->acc_size - added_size;
		buff = rb->acc;
		rb->acc_size = added_size;
		rb->acc += size;
	}
	/* callback is executed out of ring buffer context */
	UTIL_RBUFF_LEAVE_CTX(rb);
	if(buff)
	{
		return rb->notify_func(rb, buff, size);
	}

	return EOS_ERROR_OK;
}

static eos_error_t util_rbuff_handle_wm(util_rbuff_t* rb)
{
	uint8_t notify = 0;

	UTIL_RBUFF_ENTER_CTX(rb);
	if((rb->acc_size > rb->wm_high) && rb->last_level == util_rbuff_wm_low)
	{
		rb->last_level = util_rbuff_wm_high;
		notify = 1;
	}
	else if((rb->acc_size < rb->wm_low) && rb->last_level == util_rbuff_wm_high)
	{
		rb->last_level = util_rbuff_wm_low;
		notify = 1;
	}
	UTIL_RBUFF_LEAVE_CTX(rb);
	if(notify != 0)
	{
		return rb->wm_cb(rb, rb->last_level);
	}

	return EOS_ERROR_OK;
}


static eos_error_t util_rbuff_data_wait(util_rbuff_t* rb, uint32_t size, int32_t wait)
{
	osi_time_t timeout = {0, 0};
	div_t divide = {0, 0};

	if (wait != -1)
	{
		divide = div(wait, 1000);
		timeout.sec = divide.quot;
		timeout.nsec = divide.rem * 1000000;
	}
	while(size > rb->acc_size && rb->state != UTIL_RING_BUFF_STATE_STOPPED)
	{
#ifdef RING_BUFF_DBG_MSG
		UTIL_LOGV(rb->log, "READ: Waiting read buffer for %u ACC: %d", size, rb->acc_size);
#endif
		/* we are already in the context, so leave it... */
		UTIL_RBUFF_LEAVE_CTX(rb);
		if(wait != -1)
		{
			if(osi_bin_sem_timedtake(rb->read_sem, &timeout) == EOS_ERROR_TIMEDOUT)
			{
				return EOS_ERROR_TIMEDOUT;
			}
		}
		else
		{
			osi_bin_sem_take(rb->read_sem);
		}
		UTIL_RBUFF_ENTER_CTX(rb);
		/* We can read, even if buffer has been stopped */
		if(util_rbuff_check_state(rb, UTIL_RING_BUFF_STATE_ACTIVE | UTIL_RING_BUFF_STATE_STOPPED))
		{
			return EOS_ERROR_PERM;
		}
	}
	return EOS_ERROR_OK;
}

static eos_error_t util_rbuff_read_cont(util_rbuff_t* rb, void** buff, uint32_t size, uint32_t *read)
{
	eos_error_t err = EOS_ERROR_OK;

	*buff = rb->acc;
	/* If writer wrapped, and we don't have enough data at the end, give as much as we can */
	if(rb->eod != NULL && (rb->acc + size > rb->eod))
	{
		*read = rb->eod - rb->acc;
		/* just a wrap (no data) available -> wrap right away and give requested size */
		if(*read == 0)
		{
			*read = size;
			*buff = rb->buff;
			rb->acc = rb->buff + *read;
		}
		else
		{
			rb->acc = rb->buff;
		}
		rb->acc_size -= *read;
		/* reset EOD */
		rb->eod = NULL;
#ifdef RING_BUFF_DBG_MSG
		UTIL_LOGV(rb->log, "READ: Wrap around %u (%u) %p %p", *read, rb->acc_size, rb->read, rb->acc);
#endif
	}
	else
	{
		if(rb->state == UTIL_RING_BUFF_STATE_STOPPED && size > rb->acc_size)
		{
#ifdef RING_BUFF_DBG_MSG
			UTIL_LOGV(rb->log, "READ: Handle stopped state %u (%u)", size, rb->acc_size);
#endif
			size = rb->acc_size;
			err = EOS_ERROR_PERM;
		}
		rb->acc_size -= size;
		rb->acc += size;
		*read = size;
#ifdef RING_BUFF_DBG_MSG
		UTIL_LOGV(rb->log, "READ: %u (%u) %p %p", *read, rb->acc_size, rb->read, rb->acc);
#endif
	}

	return err;
}

