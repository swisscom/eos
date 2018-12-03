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


#include "util_msgq.h"
#include "osi_memory.h"
#include "osi_thread.h"
#include "osi_time.h"

#define MODULE_NAME "msgq test"
#include "util_log.h"

#include <stdlib.h>

#ifndef __LINE__
#define __LINE__ (-1)
#endif

void err_exit(int err, int line)
{
	UTIL_GLOGE("err: %d [line: %d]", err, line);
	exit(err);
}


#define TEST_ONE_LOOPS     (3000)

typedef struct test_one_arg
{
	util_msgq_t *queue;
	unsigned int loops;
	unsigned int failed;
} test_one_arg_t;

void* test_one_provider(void* arg)
{
	test_one_arg_t *tc_arg = (test_one_arg_t *) arg;
	int i, *to_put;
	int fail = 7;

	for(i=0; i<TEST_ONE_LOOPS; i++)
	{
		to_put = (int*)osi_malloc(sizeof(int));
		*to_put = i;
		if(util_msgq_put(tc_arg->queue, to_put, sizeof(int), NULL) != EOS_ERROR_OK)
		{
			UTIL_GLOGE("Test 1: ERROR sending message");
			err_exit(fail, __LINE__);
		}
		if(osi_time_usleep(rand() % 10000 + 100) != EOS_ERROR_OK)
		{
			err_exit(fail, __LINE__);
		}
	}

	return NULL;
}

void* test_one_consumer(void* arg)
{
	test_one_arg_t *tc_arg = (test_one_arg_t *) arg;
	int *msg, i;
	size_t size;
	int fail = 7;

	for(i=0; i<TEST_ONE_LOOPS; i++)
	{
		if(util_msgq_get(tc_arg->queue, (void*)&msg, &size, NULL) != EOS_ERROR_OK)
		{
			UTIL_GLOGE("Test 1: ERROR sending message	");
			return NULL;
		}
		if(i != *msg)
		{
			tc_arg->failed++;
		}
		osi_free((void**)&msg);
		tc_arg->loops++;
		if(osi_time_usleep(rand() % 10000 + 100) != EOS_ERROR_OK)
		{
			err_exit(fail, __LINE__);
		}
	}

	return NULL;
}

static void test_one(void)
{
	osi_thread_t *provider = NULL;
	osi_thread_t *consumer = NULL;
	osi_thread_attr_t attr = {OSI_THREAD_JOINABLE};
	test_one_arg_t tc_arg = {NULL, 0, 0};
	int fail = 1;

	UTIL_GLOGI("Test 1: Message queue basic get/put test (%d messages)", TEST_ONE_LOOPS);
	if(util_msgq_create(&tc_arg.queue, 3, NULL) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 1: ERROR creating message queue");
		err_exit(fail, __LINE__);
	}
	tc_arg.loops = 0;
	tc_arg.failed = 0;
	if(osi_thread_create(&provider, &attr, test_one_provider, &tc_arg) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 1: ERROR creating provider thread");
		err_exit(fail, __LINE__);
	}
	if(osi_thread_create(&consumer, &attr, test_one_consumer, &tc_arg) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 1: ERROR creating consumer thread");
		err_exit(fail, __LINE__);
	}
	if(osi_thread_join(provider, NULL) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	if(osi_thread_release(&provider) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	if(osi_thread_join(consumer, NULL) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	if(osi_thread_release(&consumer) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	util_msgq_destroy(&tc_arg.queue);

	UTIL_GLOGI("Test 1: Done...");
}

#define TEST_TWO_VAL (0xBEEF)

static void test_two(void)
{
	int fail = 2, to_put = 0, *to_get = NULL, to_put_urg = 0;
	size_t sz = 0;
	util_msgq_t *queue;
	osi_time_t timeout = {0, OSI_TIME_MSEC_TO_NSEC(500)};
	uint32_t len;

	UTIL_GLOGI("Test 2: Timeout test", TEST_ONE_LOOPS);
	if(util_msgq_create(&queue, 1, NULL) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 2: ERROR creating message queue");
		err_exit(fail, __LINE__);
	}
	to_put = TEST_TWO_VAL;
	if(util_msgq_get(queue, (void**)&to_get, &sz, &timeout) == EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 2: message should not be received!");
		err_exit(fail, __LINE__);
	}
	if(util_msgq_peek(queue, (void**)&to_get, &sz, 0) == EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 2: there is no message to peek!");
		err_exit(fail, __LINE__);
	}
	if(util_msgq_put(queue, &to_put, sizeof(int), &timeout) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 2: ERROR sending message");
		err_exit(fail, __LINE__);
	}
	if(util_msgq_peek(queue, (void**)&to_get, &sz, 1) == EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 2: there is no message to peek on index 1!");
		err_exit(fail, __LINE__);
	}
	if(util_msgq_put(queue, &to_put, sizeof(int), &timeout) == EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 2: message should not be sent");
		err_exit(fail, __LINE__);
	}
	to_put_urg = to_put << 1;
	if(util_msgq_put_urgent(queue, &to_put_urg, sizeof(int)) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 2: put urgent failed");
		err_exit(fail, __LINE__);
	}
	if(util_msgq_count(queue, &len) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 2: get count failed");
		err_exit(fail, __LINE__);
	}
	if(len != 1)
	{
		UTIL_GLOGE("Test 2: wrong count");
		err_exit(fail, __LINE__);
	}
	if(util_msgq_peek(queue, (void**)&to_get, &sz, 0) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 2: there is no message to peek on index %d!", 0);
		err_exit(fail, __LINE__);
	}
	if(*to_get != to_put_urg || sz != sizeof(int))
	{
		UTIL_GLOGE("Test 2: Wrong message peeked");
		err_exit(fail, __LINE__);
	}
	if(util_msgq_get(queue, (void**)&to_get, &sz, &timeout) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 2: ERROR receiving message");
		err_exit(fail, __LINE__);
	}
	if(*to_get == to_put || sz != sizeof(int))
	{
		UTIL_GLOGE("Test 2: Wrong message received");
		err_exit(fail, __LINE__);
	}
	if(util_msgq_destroy(&queue) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 2: destroy failed");
		err_exit(fail, __LINE__);
	}

	UTIL_GLOGI("Test 2: Done...");
}

static int test_three_free_count = 0;

void test_three_free_cbk(void* msg_data, size_t msg_size)
{
	(void)msg_data;
	(void)(msg_size);
	test_three_free_count++;
}

static void test_three(void)
{
	int fail = 3, to_put = 0/*, *to_get = NULL, to_put_urg = 0*/;
	//size_t sz = 0;
	util_msgq_t *queue;
	osi_time_t timeout = {0, OSI_TIME_MSEC_TO_NSEC(500)};

	UTIL_GLOGI("Test 3: FLUSH test", TEST_ONE_LOOPS);
	if(util_msgq_create(&queue, 3, test_three_free_cbk) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 3: ERROR creating message queue");
		err_exit(fail, __LINE__);
	}
	to_put = TEST_TWO_VAL;
	if(util_msgq_put(queue, &to_put, sizeof(int), NULL) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 3: sending message failed");
		err_exit(fail, __LINE__);
	}
	if(util_msgq_pause(queue) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 3: flush failed");
		err_exit(fail, __LINE__);
	}
	if(util_msgq_flush(queue) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 3: flush failed");
		err_exit(fail, __LINE__);
	}
	if(test_three_free_count != 1)
	{
		UTIL_GLOGE("Test 3: message free callback not called");
		err_exit(fail, __LINE__);
	}
	if(util_msgq_put(queue, &to_put, sizeof(int), &timeout) == EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 3: should not send message");
		err_exit(fail, __LINE__);
	}
	if(util_msgq_resume(queue) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 3: flush failed");
		err_exit(fail, __LINE__);
	}
	if(util_msgq_put(queue, &to_put, sizeof(int), NULL) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 3: sending message failed");
		err_exit(fail, __LINE__);
	}
	test_three_free_count = 0;
	if(util_msgq_destroy(&queue) != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test 3: destroy failed");
		err_exit(fail, __LINE__);
	}
	if(test_three_free_count != 1)
	{
		UTIL_GLOGE("Test 3: message free callback not called");
		err_exit(fail, __LINE__);
	}

	UTIL_GLOGI("Test 3: Done...");
}

int main(int argc, char** argv)
{
	/* kill warning */
	if(argc == 0 && argv == NULL)
	{

	}
	test_one();
	test_two();
	test_three();

	return 0;
}
