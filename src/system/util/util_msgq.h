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


#ifndef OSI_MSGQ_H_
#define OSI_MSGQ_H_

#include "eos_error.h"
#include "eos_types.h"
#include "osi_time.h"

#include <stddef.h>

/**
 * Message queue handle.
 */
typedef struct util_msgq_handle util_msgq_t;
/**
 * Callback invoked when the queue is flushed or in similar situation whenever messages are forcefully dismissed.
 * This is required as queue is not doing any data memory management.
 * @param msg_data Message data to be freed.
 * @param msg_size Message size.
 */
typedef void (*util_msgq_free_cbk_t)(void* msg_data, size_t msg_size);

/**
 * Message queue constructor.
 * @param queue pointer to handle which will be updated if construction was successful (output param)
 * @param max Maximum queue members count. Pass 0 to make a queue without count constraints.
 * @param free_cbk Free callback, used if messages are dropped.
 * Can be NULL but with potential memory leaks (set it to NULL if you are sure what you are doing).
 * @return no error, or error descriptor.
 */
eos_error_t util_msgq_create(util_msgq_t** queue, uint32_t max, util_msgq_free_cbk_t free_cbk);
/**
 * Message queue destructor.
 * @param queue handle which will be freed.
 * @return no error, or error descriptor.
 */
eos_error_t util_msgq_destroy(util_msgq_t** queue);
/**
 * Put message in the queue.
 * @param queue message queue handle
 * @param msg_data message data. Data is preallocated by user (queue is not doing any data memory management).
 * @param msg_size message data size in bytes.
 * @param timeout Timeot for the action. Pass <code>NULL<\code> for infinite timeout.
 * @return no error, or error descriptor.
 */
eos_error_t util_msgq_put(util_msgq_t* queue, void* msg_data, size_t msg_size, osi_time_t* timeout);
/**
 * Put message as first one in the queue.
 * @param queue message queue handle.
 * @param msg_data message data. Data is preallocated by user (queue is not doing any data memory management).
 * @param msg_size message data size in bytes.
 * @return no error, or error descriptor.
 */
eos_error_t util_msgq_put_urgent(util_msgq_t* queue, void* msg_data, size_t msg_size);
/**
 * Get message from queue. Message related memory is freed, but message data needs to be freed by user.
 * This function will block execution until message is available in the queue.
 * @param queue message queue handle.
 * @param msg_data message data. This is output parameter which will contain data pointer if there were no errors.
 * @param msg_size message size. This is output parameter which will contain data size if there were no errors.
 * @param timeout Timeout for the action. Pass <code>NULL<\code> for infinite timeout.
 * @return no error, or error descriptor.
 */
eos_error_t util_msgq_get(util_msgq_t* queue, void** msg_data, size_t* msg_size, osi_time_t* timeout);
/**
 * Gets message from the queue without removing the message from it.
 * For the message for the given index is NOT waited.
 * Caller must be sure that the index is in range at the time of the call.
 * @param queue message queue handle.
 * @param msg_data message data. This is output parameter which will contain data pointer if there were no errors.
 * @param msg_size message size. This is output parameter which will contain data size if there were no errors.
 * @param idx Index in the queue (range is from 0 to count - 1)
 * @return no error, or error descriptor.
 */
eos_error_t util_msgq_peek(util_msgq_t* queue, void** msg_data, size_t* msg_size, uint32_t idx);
/**
 * Pauses queue. After this call, all subsequent PUTs and GETs will fail with EOS_ERROR_PERM.
 * Queue is NOT flushed after this call.
 * @param queue message queue handle.
 * @return no error, or error descriptor.
 */
eos_error_t util_msgq_pause(util_msgq_t* queue);
/**
 * Starts again paused queue. If the queue is not paused, this call has no effect.
 * @param queue message queue handle.
 * @return no error, or error descriptor.
 */
eos_error_t util_msgq_resume(util_msgq_t* queue);
/**
 * Drops all messages from the queue.
 * @param queue message queue handle.
 * @return no error, or error descriptor.
 */
eos_error_t util_msgq_flush(util_msgq_t* queue);
/**
 * Returns number of messages in the queue.
 * This is useful for polling or message peeking.
 * @param queue message queue handle.
 * @param count message count.
 * @return no error, or error descriptor.
 */
eos_error_t util_msgq_count(util_msgq_t* queue, uint32_t* count);

#endif /* OSI_MSGQ_H_ */
