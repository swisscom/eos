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
#include "osi_time.h"
#include "osi_thread.h"
#include "eos_macro.h"

#define MODULE_NAME "rbuff_test"
#include "util_log.h"

#include <stdlib.h>


#if 0
#include "message_queue.h"
#endif

#define FIRST_TC_BUFF_SIZE (50*1024)
#define FIRST_TC_LOOPS     (3000)

#define SECOND_TC_BUFF_SIZE (64*1024)
#define SECOND_TC_ACC_SIZE  (24*1024)
#define SECOND_TC_LOOPS     (5000)

#define DEMUX_CRC_ADDER_MASK    0x04C11DB7  /* As defined in MPEG-2 CRC     */
                                            /* Decoder Model:               */
                                            /*   1 - adder is enabled       */
                                            /*   0 - adder is disabled      */

static unsigned int uCRCTable[256];

static void Init_CRC(void)
{
	unsigned int crc;
	int i, j;

	/* Initialize CRC table */
	for(i=0; i < 256; i++)
	{
		crc= 0;
		for(j=7; j >= 0; j--)
		{
			if(((i >> j) ^ (crc >> 31)) & 1)
			{
				crc = (crc << 1) ^ DEMUX_CRC_ADDER_MASK;
			}
			else
			{
				crc <<= 1;
			}
		}
		uCRCTable[i] = crc;
	}
}

static unsigned int CalculateCRC(const unsigned char *data, unsigned int len)
{
	unsigned int crc = 0xFFFFFFFF;
	unsigned int i;

	for (i = 0; i < len; ++i)
	{
		crc = (crc << 8) ^ uCRCTable[(crc >> 24) ^ (*data++)];
	}

	return (crc);
}

/* ####### First test case: This is general "provider/consumer" test case. ####### */

typedef struct first_tc_msg
{
	unsigned int size;
	unsigned int crc;
} first_tc_msg_t;

typedef struct _tc_arg
{
	util_rbuff_t *ring_buff;
	unsigned int loops;
	unsigned int failed;
} tc_arg_t;


void* first_tc_provider(void* arg)
{
	util_rbuff_t *ring_buff = ((tc_arg_t*) arg)->ring_buff;
	int tc_run = ((tc_arg_t*) arg)->loops;
	unsigned int size = 0;
	first_tc_msg_t* msg;
	unsigned char* data;
	unsigned int i = 0;
	eos_error_t err;

	while(tc_run)
	{
		size = rand() % 16384 + 1024;
		err = util_rbuff_reserve(ring_buff, (void**)&msg,
				(unsigned int)sizeof(first_tc_msg_t), UTIL_RBUFF_FOREVER);
		if(err != EOS_ERROR_OK)
		{
			UTIL_GLOGE("************* ERROR reserving message **************");
			return NULL;
		}
		msg->size = size;
		err = util_rbuff_reserve(ring_buff, (void**)&data, size, UTIL_RBUFF_FOREVER);
		if(err != EOS_ERROR_OK)
		{
			UTIL_GLOGE("*************** ERROR reserving data ***************");
			return NULL;
		}
		for(i=0; i<size; i++)
		{
			data[i] = (unsigned char) (rand() % 0xFF);
		}
		msg->crc = CalculateCRC(data, size);
		err = util_rbuff_commit(ring_buff, msg, sizeof(first_tc_msg_t));
		if(err != EOS_ERROR_OK)
		{
			UTIL_GLOGE("************* ERROR committing message *************");
			return NULL;
		}
		err = util_rbuff_commit(ring_buff, data, size);
		if(err != EOS_ERROR_OK)
		{
			UTIL_GLOGE("************** ERROR committing data ***************");
			return NULL;
		}
		/* usleep(rand() % 100000 + 100); */
		tc_run--;
	}
	err = util_rbuff_reserve(ring_buff, (void**)&msg,
			(unsigned int)sizeof(first_tc_msg_t), UTIL_RBUFF_FOREVER);
	if(err != EOS_ERROR_OK)
	{
		UTIL_GLOGE("************* ERROR reserving message **************");
		return NULL;
	}
	/* send "End Of Test" message */
	msg->size = 0;
	msg->crc = 0;
	err = util_rbuff_commit(ring_buff, msg, sizeof(first_tc_msg_t));
	if(err != EOS_ERROR_OK)
	{
		UTIL_GLOGE("************* ERROR committing message *************");
		return NULL;
	}

	return NULL;
}

void* first_tc_consumer(void* arg)
{
	tc_arg_t* tc_arg = (tc_arg_t*) arg;
	util_rbuff_t *ring_buff = tc_arg->ring_buff;
	first_tc_msg_t* msg;
	unsigned char* data;
	unsigned int crc = 0;
	unsigned int count = 1;
	unsigned int read;
	eos_error_t err;

	while(1)
	{
		err = util_rbuff_read(ring_buff, (void**)&msg, sizeof(first_tc_msg_t), &read, UTIL_RBUFF_FOREVER);
		if(err != EOS_ERROR_OK)
		{
			UTIL_GLOGE("************** ERROR reading message ***************");
			return NULL;
		}
		if(msg->size == 0 && msg->crc == 0)
		{
			UTIL_GLOGI("************** End Of Test received ****************");
			break;
		}
		err = util_rbuff_read(ring_buff, (void**)&data, msg->size, &read, UTIL_RBUFF_FOREVER);
		if(err != EOS_ERROR_OK)
		{
			UTIL_GLOGE("*************** ERROR reading data *****************");
			return NULL;
		}
		crc = CalculateCRC(data, msg->size);
		if(crc != msg->crc)
		{
			UTIL_GLOGE("** %04u: FAILED (CRC exp/rd: 0x%08x/0x%08x) **", count, msg->crc, crc);
			tc_arg->failed++;
		}
		else
		{
			UTIL_GLOGI("********* %04u: PASSED (CRC: 0x%08x) ***********", count, crc);
		}
		err = util_rbuff_free(ring_buff, msg, sizeof(first_tc_msg_t));
		if(err != EOS_ERROR_OK)
		{
			UTIL_GLOGE("************** ERROR freeing message ***************");
			return NULL;
		}
		err = util_rbuff_free(ring_buff, (void*)data, msg->size);
		if(err != EOS_ERROR_OK)
		{
			UTIL_GLOGE("*************** ERROR freeing data *****************");
			return NULL;
		}
		count++;
		osi_time_usleep(rand() % 100000 + 100);
	}
	tc_arg->loops = count - 1;

	return NULL;
}

static void execute_first_tc(void)
{
	osi_thread_t *provider;
	osi_thread_t *consumer;
	util_rbuff_t *ring_buff = NULL;
	util_rbuff_attr_t  ring_buff_attr = {NULL, FIRST_TC_BUFF_SIZE, 0, NULL, 0, 0, NULL};
	tc_arg_t tc_arg = {NULL, FIRST_TC_LOOPS, 0};
	eos_error_t err;
	void *buff;
	osi_time_t time;

	if((buff = malloc(FIRST_TC_BUFF_SIZE)) == NULL)
	{
		UTIL_GLOGE("****************** ERROR no memory *****************");
		goto done;
	}
	ring_buff_attr.buff = buff;
	UTIL_GLOGI("********** Executing blocking read/write test **********");
	err = util_rbuff_create(&ring_buff_attr, &ring_buff);
	if(err != EOS_ERROR_OK)
	{
		UTIL_GLOGE("************** ERROR creating ring buffer **************");
		goto done;
	}
	osi_time_get_timestamp(&time);
	srand(time.nsec);
	Init_CRC();
	tc_arg.ring_buff = ring_buff;
	if(osi_thread_create(&provider, NULL, first_tc_provider, &tc_arg) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("************ ERROR creating provider thread *************");
		goto done;
	}
	if(osi_thread_create(&consumer, NULL, first_tc_consumer, &tc_arg) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("************ ERROR creating consumer thread *************");
		goto done;
	}
	if(osi_thread_join(provider, NULL) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("************ ERROR joining provider thread **************");
		goto done;
	}
	if(osi_thread_release(&provider) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("*********** ERROR releasing provider thread *************");
		goto done;
	}
	if(osi_thread_join(consumer, NULL) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("************ ERROR joining consumer thread **************");
		goto done;
	}
	if(osi_thread_release(&consumer) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("*********** ERROR releasing consumer thread *************");
		goto done;
	}

done:
	if(ring_buff != NULL)
	{
		util_rbuff_destroy(&ring_buff);
	}
	if(buff != NULL)
	{
		free(buff);
	}
	UTIL_GLOGI(" LOOPS:  %u", tc_arg.loops);
	UTIL_GLOGI(" FAILED: %u", tc_arg.failed);
	UTIL_GLOGI("************************* DONE *************************");
}

/* lets save message if */
static first_tc_msg_t second_tc_save_msg = {0, 0};
static unsigned int second_tc_count = 0;
static unsigned int second_tc_failed = 0;

void* second_tc_provider(void* arg)
{
	util_rbuff_t *ring_buff = ((tc_arg_t*) arg)->ring_buff;
	eos_error_t err;

	/* use same provider as for regular read/write test */
	first_tc_provider(arg);
	/* we have to be sure that all data is sent */
	err = util_rbuff_flush(ring_buff);
	if(err != EOS_ERROR_OK)
	{
		//print?
	}

	return NULL;
}

eos_error_t second_tc_notify(util_rbuff_t *handle, void* buff, unsigned int size)
{
	static first_tc_msg_t* msg;
	unsigned int crc = 0, tmp_size;
	unsigned char save_msg = 0;
	eos_error_t err;
	int len = size;

	/* check for leftovers from previous notify */
	if(second_tc_save_msg.crc != 0 && second_tc_save_msg.size != 0)
	{
		crc = CalculateCRC(buff, second_tc_save_msg.size);
		second_tc_count++;
		if(crc != second_tc_save_msg.crc)
		{
			UTIL_GLOGE("** %04u: FAILED (CRC exp/rd: 0x%08x/0x%08x) **", second_tc_count, second_tc_save_msg.crc, crc);
			second_tc_failed++;
		}
		else
		{
			UTIL_GLOGI("********* %04u: PASSED (CRC: 0x%08x) ***********", second_tc_count, crc);
		}
		err = util_rbuff_free(handle, buff, second_tc_save_msg.size);
		if(err != EOS_ERROR_OK)
		{
			UTIL_GLOGE("**************** ERROR freeing data ****************");
			return EOS_ERROR_GENERAL;
		}
		len -= second_tc_save_msg.size;
		buff = (uint8_t*)buff + second_tc_save_msg.size;
		second_tc_save_msg.size = 0;
		second_tc_save_msg.crc = 0;
	}
	while(len > 0)
	{
		msg = (first_tc_msg_t*) buff;
		buff = (uint8_t*)buff + sizeof(first_tc_msg_t);
		len -= sizeof(first_tc_msg_t);
		if(msg->crc == 0 && msg->size == 0)
		{
			UTIL_GLOGI("************** End Of Test received ****************");
			/* force exit */
			len = 0;
		}
		if(len == 0)
		{
			save_msg = 1;
			break;
		}
		crc = CalculateCRC(buff, msg->size);
		if(crc != msg->crc)
		{
			UTIL_GLOGE("** %04u: FAILED (CRC exp/rd: 0x%08x/0x%08x) **", second_tc_count, msg->crc, crc);
			second_tc_failed++;
		}
		else
		{
			UTIL_GLOGI("********* %04u: PASSED (CRC: 0x%08x) ***********", second_tc_count, crc);
		}
		tmp_size = msg->size;
		err = util_rbuff_free(handle, msg, sizeof(first_tc_msg_t));
		if(err != EOS_ERROR_OK)
		{
			UTIL_GLOGE("************** ERROR freeing message ***************");
			return EOS_ERROR_GENERAL;
		}
		err = util_rbuff_free(handle, buff, tmp_size);
		if(err != EOS_ERROR_OK)
		{
			UTIL_GLOGE("**************** ERROR freeing data ****************");
			return EOS_ERROR_GENERAL;
		}
		len -= msg->size;
		buff = (uint8_t*)buff + msg->size;
		second_tc_count++;
	}
	if(save_msg)
	{
		second_tc_save_msg.crc = msg->crc;
		second_tc_save_msg.size = msg->size;
		err = util_rbuff_free(handle, msg, sizeof(first_tc_msg_t));
		if(err != EOS_ERROR_OK)
		{
			UTIL_GLOGE("************** ERROR freeing message ***************");
			return EOS_ERROR_GENERAL;
		}
	}
	if(len < 0)
	{
		UTIL_GLOGE("************ Notify: protocol ERROR!!! *************");
	}

	return EOS_ERROR_OK;
}


static void execute_second_tc(void)
{
	osi_thread_t *provider;
	util_rbuff_t *ring_buff;
	util_rbuff_attr_t   ring_buff_attr = {NULL, SECOND_TC_BUFF_SIZE, SECOND_TC_ACC_SIZE, second_tc_notify, 0, 0, NULL};
	tc_arg_t tc_arg = {NULL, SECOND_TC_LOOPS, 0};
	eos_error_t err;
	void *buff;
	osi_time_t time;

	if((buff = malloc(SECOND_TC_BUFF_SIZE)) == NULL)
	{
		UTIL_GLOGE("****************** ERROR no memory *****************");
		goto done;
	}
	ring_buff_attr.buff = buff;
	UTIL_GLOGI("************* Executing reader notify test *************");
	err = util_rbuff_create(&ring_buff_attr, &ring_buff);
	if(err != EOS_ERROR_OK)
	{
		UTIL_GLOGE("************** ERROR creating ring buffer **************");
		goto done;
	}
	osi_time_get_timestamp(&time);
	srand(time.nsec);
	Init_CRC();
	tc_arg.ring_buff = ring_buff;
	/* reuse same provider as for regular read/write */
	if(osi_thread_create(&provider, NULL, second_tc_provider, &tc_arg) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("************ ERROR creating provider thread *************");
		util_rbuff_destroy(&ring_buff);
		goto done;
	}
	if(osi_thread_join(provider, NULL) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("************ ERROR joining provider thread **************");
		util_rbuff_destroy(&ring_buff);
		goto done;
	}
	if(osi_thread_release(&provider) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("*********** ERROR releasing provider thread *************");
		util_rbuff_destroy(&ring_buff);
		goto done;
	}
	util_rbuff_destroy(&ring_buff);

done:
	if(buff != NULL)
	{
		free(buff);
	}
	UTIL_GLOGI(" LOOPS:  %u", second_tc_count);
	UTIL_GLOGI(" FAILED: %u", second_tc_failed);
	UTIL_GLOGI("************************* DONE *************************");
}

#if 0
#define FOURTH_TC_LOOPS     (3000)

typedef struct fourth_tc_msg
{
	unsigned int size;
	unsigned int crc;
} fourth_tc_msg_t;

typedef struct _fourth_tc_arg
{
	msg_queue_hndl queue;
	unsigned int loops;
	unsigned int failed;
} fourth_tc_arg_t;

void* fourth_tc_provider(void* arg)
{
	fourth_tc_arg_t *tc_arg = (fourth_tc_arg_t *) arg;
	int i, *to_put;

	for(i=0; i<FOURTH_TC_LOOPS; i++)
	{
		to_put = (int*)malloc(sizeof(int));
		*to_put = i;
		if(msg_queue_put(tc_arg->queue, to_put, sizeof(int)) != MSG_QUEUE_ERR_OK)
		{
			UTIL_GLOGI("**************** ERROR sending message *****************");
			return NULL;
		}
		usleep(rand() % 10000 + 100);
	}
	return NULL;
}

void* fourth_tc_consumer(void* arg)
{
	fourth_tc_arg_t *tc_arg = (fourth_tc_arg_t *) arg;
	int *msg, i;
	size_t size;

	for(i=0; i<FOURTH_TC_LOOPS; i++)
	{
		if(msg_queue_get(tc_arg->queue, (void*)&msg, &size) != MSG_QUEUE_ERR_OK)
		{
			UTIL_GLOGI("**************** ERROR sending message *****************");
			return NULL;
		}
		if(i != *msg)
		{
			tc_arg->failed++;
		}
		free(msg);
		tc_arg->loops++;
		usleep(rand() % 10000 + 100);
	}
	return NULL;
}

static void execute_fourth_tc(void)
{
	pthread_t provider;
	pthread_t consumer;
	pthread_attr_t attr;
	fourth_tc_arg_t tc_arg = {NULL, 0, 0};

	if(msg_queue_create(&tc_arg.queue) != MSG_QUEUE_ERR_OK)
	{
		UTIL_GLOGI("************* ERROR creating message queue **************");
		goto done;
	}
	tc_arg.loops = 0;
	tc_arg.failed = 0;
	pthread_attr_init(&attr);
	if (pthread_create(&provider, &attr, fourth_tc_provider, &tc_arg) != 0)
	{
		UTIL_GLOGI("************ ERROR creating provider thread *************");
		pthread_attr_destroy(&attr);
		goto done;
	}
	if (pthread_create(&consumer, &attr, fourth_tc_consumer, &tc_arg) != 0)
	{
		UTIL_GLOGI("************ ERROR creating consumer thread *************");
		pthread_attr_destroy(&attr);
		goto done;
	}
	pthread_attr_destroy(&attr);


	pthread_join(provider, NULL);
	pthread_join(consumer, NULL);
done:
	if(tc_arg.queue)
	{
		msg_queue_destroy(tc_arg.queue);
	}
	UTIL_GLOGI(" LOOPS:  %u", tc_arg.loops);
	UTIL_GLOGI(" FAILED: %u", tc_arg.failed);
	UTIL_GLOGI("************************* DONE *************************");
}
#endif

int main(int argc, char** argv)
{
	EOS_UNUSED(argc);
	EOS_UNUSED(argv);

	execute_first_tc();
	execute_second_tc();

	return 0;
}

