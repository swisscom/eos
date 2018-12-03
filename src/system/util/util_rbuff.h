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


#ifndef UTIL_RING_BUFF_H_
#define UTIL_RING_BUFF_H_

#include "eos_error.h"

#include <stdint.h>

#define UTIL_RBUFF_FOREVER (-1)

/**
 * Watermark levels.
 */
typedef enum util_rbuff_wm_level
{
	/** Low watermark */
	util_rbuff_wm_low,
	/** High watermark */
	util_rbuff_wm_high
} util_rbuff_wm_level_t;

/** Ring buffer handle. */
typedef struct util_rbuff_handle util_rbuff_t;
/**
 * Notification function type. Ring buffer client has to implement one,
 * if notification mechanism is used.
 * @param rb Ring buffer handle.
 * @param buff Notified data buffer.
 * @param buff Notified data buffer size.
 * @return EOS_ERROR_OK if everything was OK, or error if ring buffer client detected some problem.
 */
typedef eos_error_t (*util_rbuff_notify_t) (util_rbuff_t* rb, void* buff, uint32_t size);
/**
 * Callback which is used to notify the upper layers that buffer fullness is on one of
 * watermark values. User has to set watermark levels AND callback to use watermark mechanism.
 * @param rb Ring buffer handle.
 * @param level Watermark which is hit.
 * @return EOS_ERROR_OK if everything was OK, or error if ring buffer client detected some problem.
 */
typedef eos_error_t (*util_rbuff_wm_cb_t) (util_rbuff_t* rb, util_rbuff_wm_level_t level);

/**
 * Ring buffer attribute structure. It is used when ring buffer is created.
 */
typedef struct util_rbuff_attr
{
	/** Memory used for ring buffer. */
	void* buff;
	/** Buffer size. */
	uint32_t size;
	/**
	 * Accumulate size. If set to 0, accumulation/notification mechanism will not be used.
	 */
	uint32_t accumulate;
	/**
	 * Notify function pointer. This function is called whenever end of buffer is reached,
	 * or there is up to "accumulate" number of bytes available. If "accumulate" attribute
	 * field is not set, this function will NOT be called.
	 * NOTE: This function is called from the same thread from which data commit is done.
	 */
	util_rbuff_notify_t notify_func;
	/**
	 * Low watermark value in bytes. It has to be between zero and wm_high.
	 */
	uint32_t wm_low;
	/**
	 * High watermark value in bytes. It has to be less than buffer size.
	 */
	uint32_t wm_high;
	/**
	 * Watermark callback. It is called whenever buffer fullness gets under wm_low,
	 * or whenever it gets over wm_high. It is called ONLY during the fullness transition.
	 */
	util_rbuff_wm_cb_t wm_cb;
} util_rbuff_attr_t;

/**
 * Creates ring buffer with attributes passed as argument. Handle returned must be saved
 * by ring buffer client, so that it can be used for operations on ring buffer.
 * @param attr Ring buffer attribute object.
 * @param rb Pointer to the handle. This argument must not be NULL. If function returns
 * without error, this pointer will point to ring buffer handle that is required for other
 * ring buffer operations.
 * @return EOS_ERROR_OK if everything was OK, or error if there was some problem.
 */
eos_error_t util_rbuff_create(util_rbuff_attr_t* attr, util_rbuff_t** rb);
/**
 * Ring buffer destructor function. This function must be called, so that all resources
 * allocated on ring buffer construction are freed.
 * @param rb Ring buffer handle.
 * @return EOS_ERROR_OK if everything was OK, or error if there was some problem.
 */
eos_error_t util_rbuff_destroy(util_rbuff_t** rb);
/**
 * Reserves chunk of memory from ring buffer. Chunk allocated is continuous memory that
 * is ready to be written with data.
 * @param rb Ring buffer handle.
 * @param buff Pointer to the reserved data. This is output value.
 * @param size Requested buffer size in bytes.
 * @param wait Reserve timeout in milliseconds. Pass -1 for blocking read.
 * @return EOS_ERROR_OK if everything was OK, or error if there was some problem.
 */
eos_error_t util_rbuff_reserve(util_rbuff_t* rb, void** buff, uint32_t size, int32_t wait);
/**
 * Commits written data. After this function is called, data is available for reading.
 * @param rb Ring buffer handle.
 * @param buff Pointer to data that should be committed. This pointer is retrieved with "util_rbuff_reserve".
 * @param size Committed data size in bytes.
 * @return EOS_ERROR_OK if everything was OK, or error if there was some problem.
 */
eos_error_t util_rbuff_commit(util_rbuff_t* rb, void *buff, uint32_t size);
/**
 * Frees ring buffer chunk, so that it can be used for writing.
 * @param rb Ring buffer handle.
 * @param buff Pointer to data that should be freed.
 * @param size Chunk length that should be freed.
 * @return EOS_ERROR_OK if everything was OK, or error if there was some problem.
 */
eos_error_t util_rbuff_free(util_rbuff_t* rb, void *buff, uint32_t size);
/**
 * Reads out requested size of data. Data should be additionally freed with "util_rbuff_free".
 * Read mechanism should NOT be used together with the notify mechanism.
 * @param rb Ring buffer handle.
 * @param buff Output argument that will contain pointer with read data.
 * @param size Data size that should be read.
 * @param wait Read timeout in milliseconds. Pass -1 for blocking read.
 * @return EOS_ERROR_OK if everything was OK, or error if there was some problem.
 */
eos_error_t util_rbuff_read(util_rbuff_t* rb, void **buff, uint32_t size, uint32_t *read, int32_t wait);
/**
 * Similar to <code>ring_buff_read</code>, but ensures that requested data is read AND read buffer
 * is freed. Internally this function will call memcpy, so it is expected that passed buffer is
 * allocated by the caller.
 * @param rb Ring buffer handle.
 * @param buff Data buffer (allocated by the caller).
 * @param size Requested size to read.
 * @param wait Read timeout in milliseconds. Pass -1 for blocking read.
 * @return EOS_ERROR_OK if everything was OK, or error if there was some problem.
 */
eos_error_t util_rbuff_read_all(util_rbuff_t* rb, void *buff, uint32_t size, int32_t wait);

/**
 * This function can be used when the notify mechanism is used. Calling this function will result
 * with forced call to the notify function.
 * @param rb Ring buffer handle.
 * @return EOS_ERROR_OK if everything was OK, or error if there was some problem.
 */
eos_error_t util_rbuff_flush(util_rbuff_t* rb);
/**
 * This function will stop further operations on ring buffer.
 * It will NOT free any resources.
 * @param rb Ring buffer handle.
 * @return EOS_ERROR_OK if everything was OK, or error if there was some problem.
 */
eos_error_t util_rbuff_cancel(util_rbuff_t* rb);
/**
 * Similar to <code>util_rbuff_cancel</code>, but reader can still read rest of the data provided.
 * It should be called after the last 'commit' is done.
 * @param rb Ring buffer handle.
 * @return EOS_ERROR_OK if everything was OK, or error if there was some problem.
 */
eos_error_t util_rbuff_stop(util_rbuff_t* rb);
/**
 * Resets the buffer to the initial state (offsets and accumulation will be reseted to zero).
 * All data is lost after this function is called.
 * If the ring buffer was stopped/canceled, it will be resumed.
 * @param rb Ring buffer handle.
 * @return EOS_ERROR_OK if everything was OK, or error if there was some problem.
 */
eos_error_t util_rbuff_rst(util_rbuff_t* rb);
/**
 * Polls for current buffer fullness. This does not mean that the read will return all
 * data right away (as read is giving consecutive data).
 * @param rb Ring buffer handle.
 * @param fullness Buffer fullness.
 * @return EOS_ERROR_OK if everything was OK, or error if there was some problem.
 */
eos_error_t util_rbuff_get_fullness(util_rbuff_t* rb, uint32_t *fullness);

#endif /* UTIL_RING_BUFF_H_ */
