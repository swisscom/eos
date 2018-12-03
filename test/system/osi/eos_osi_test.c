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


#include <stdlib.h>
#include <stdio.h>

#include "../../../src/system/util/util_msgq.h"
#include "osi_memory.h"
#include "osi_thread.h"
#include "osi_mutex.h"
#include "osi_time.h"
#include "osi_sem.h"
#include "osi_bin_sem.h"

#define TEST_ONE_BUFF_SIZE (20 * 1024 * 1024)
#define TEST_ONE_PATTERN (0xFA)

#ifndef __LINE__
#define __LINE__ (-1)
#endif

void err_exit(int err, int line)
{
	printf("err: %d [line: %d]\n", err, line);
	exit(err);
}

void test_one(void)
{
	int fail = -1;
	uint8_t *buff = osi_malloc(TEST_ONE_BUFF_SIZE);
	int i;

	printf("Test 1: Test %d bytes of heap\n", TEST_ONE_BUFF_SIZE);
	if(buff == NULL)
	{
		err_exit(fail, __LINE__);
	}
	printf("Test 1: Writing 0x%2X\n", TEST_ONE_PATTERN);
	for(i=0; i<TEST_ONE_BUFF_SIZE; i++)
	{
		buff[i] = TEST_ONE_PATTERN;
	}
	printf("Test 1: Checking written values\n");
	for(i=0; i<TEST_ONE_BUFF_SIZE; i++)
	{
		if(buff[i] != TEST_ONE_PATTERN)
		{
			err_exit(fail, __LINE__);
		}
	}
	osi_free((void*)&buff);
	if(buff != NULL)
	{
		err_exit(fail, __LINE__);
	}
	printf("Test 1: Done!\n");
}

#define TEST_TWO_RET (2)

void* test_two_thread(void* arg)
{
	int *ret = (int *) arg;
	printf("Test 2: Thread\n");
	*ret = TEST_TWO_RET;
	osi_time_usleep(OSI_TIME_MICROS);

	return arg;
}

void test_two(void)
{
	osi_thread_t *thread;
	osi_thread_attr_t attr = {OSI_THREAD_JOINABLE};
	int arg = 1;
	void* ret;
	int fail = -2;

	printf("Test 2: Thread create\n");
	if(osi_thread_create(&thread, &attr, test_two_thread, &arg) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	printf("Test 2: Joining...\n");
	if(osi_thread_join(thread, &ret) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	if(osi_thread_release(&thread) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	if(*(int*) ret != TEST_TWO_RET)
	{
		err_exit(fail, __LINE__);
	}

	printf("Test 2: Done!\n");
}

#define TEST_THREE_COUNT (20000000)

int test_three_count = 0;

void* test_three_thread(void* arg)
{
	osi_mutex_t *mtx = (osi_mutex_t*) arg;
	int fail = -3, i;

	for(i=0; i<TEST_THREE_COUNT/2; i++)
	{
		if(osi_mutex_lock(mtx) != EOS_ERROR_OK)
		{
			err_exit(fail, __LINE__);
		}
		test_three_count++;
		if(osi_mutex_unlock(mtx) != EOS_ERROR_OK)
		{
			err_exit(fail, __LINE__);
		}
	}

	return NULL;
}

void test_three(void)
{
	int fail = -3;
	osi_thread_t *t1, *t2;
	osi_thread_attr_t attr = {OSI_THREAD_JOINABLE};
	osi_mutex_t *mtx;

	printf("Test 3: Stress test for mutex lock/unlock\n");
	if(osi_mutex_create(&mtx) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	if(osi_thread_create(&t1, &attr, test_three_thread, mtx) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	if(osi_thread_create(&t2, &attr, test_three_thread, mtx) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	printf("Test 3: Joining...\n");
	if(osi_thread_join(t1, NULL) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	if(osi_thread_release(&t1) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	if(osi_thread_join(t2, NULL) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	if(osi_thread_release(&t2) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	if(osi_mutex_destroy(&mtx) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	if(test_three_count != TEST_THREE_COUNT)
	{
		err_exit(fail, __LINE__);
	}
	printf("Test 3: Done...\n");
}

#define TEST_FOUR_SLEEP (3)

void test_four(void)
{
	osi_time_t one, two, diff;
	int fail = -4;

	printf("Test 4: Timestamp test\n");
	if(osi_time_get_timestamp(&one) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	/* add 1% error margin */
	if(osi_time_usleep(TEST_FOUR_SLEEP * OSI_TIME_MICROS +
			OSI_TIME_MICROS/100) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	if(osi_time_get_timestamp(&two) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	if(osi_time_diff(&one, &two, &diff) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	if(diff.sec != TEST_FOUR_SLEEP)
	{
		printf("Test 4: FAILED... %u %u\n", diff.sec, diff.nsec);
		err_exit(fail, __LINE__);
	}
	printf("Test 4: Done... %d[ms] %u[ns]\n", (int)(diff.nsec/1000000.0), diff.nsec);
}

#define TEST_FIVE_COUNT (4)
#define TEST_FIVE_SLEEP (3)

int test_five_count = 0;

void* test_five_post_thread(void* arg)
{
	osi_sem_t *sem = (osi_sem_t*) arg;
	int fail = -5;

	/* add 1% error margin */
	if(osi_time_usleep(TEST_FIVE_SLEEP * OSI_TIME_MICROS +
			OSI_TIME_MICROS/100) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	osi_sem_post(sem);

	return NULL;
}

void* test_five_wait_thread(void* arg)
{
	osi_sem_t *sem = (osi_sem_t*) arg;

	osi_sem_wait(sem);
	test_five_count++;

	return NULL;
}

void test_five(void)
{
	int fail = -5, i = 0;
	osi_sem_t *sem = NULL;
	osi_thread_t *t = NULL;
	osi_thread_attr_t attr = {OSI_THREAD_DETACHED};
	osi_time_t one, two, diff;

	printf("Test 5: Semaphores test\n");
	if(osi_sem_create(&sem, 0, TEST_FIVE_COUNT) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	printf("Test 5: Test counter...\n");
	for(i=0; i<TEST_FIVE_COUNT; i++)
	{
		if(osi_sem_post(sem) != EOS_ERROR_OK)
		{
			err_exit(fail, __LINE__);
		}
	}
	if(osi_sem_post(sem) == EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	for(i=0; i<TEST_FIVE_COUNT; i++)
	{
		if(osi_sem_wait(sem) != EOS_ERROR_OK)
		{
			err_exit(fail, __LINE__);
		}
	}
	printf("Test 5: Test threading...\n");
	if(osi_time_get_timestamp(&one) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	for(i=0; i<TEST_FIVE_COUNT; i++)
	{
		if(osi_thread_create(&t, &attr, test_five_post_thread, sem) != EOS_ERROR_OK)
		{
			err_exit(fail, __LINE__);
		}
		if(osi_thread_release(&t) != EOS_ERROR_OK)
		{
			err_exit(fail, __LINE__);
		}
	}
	attr.type = OSI_THREAD_JOINABLE;
	for(i=0; i<TEST_FIVE_COUNT; i++)
	{
		if(osi_thread_create(&t, &attr, test_five_wait_thread, sem) != EOS_ERROR_OK)
		{
			err_exit(fail, __LINE__);
		}
		if(osi_thread_join(t, NULL) != EOS_ERROR_OK)
		{
			err_exit(fail, __LINE__);
		}
		if(osi_thread_release(&t) != EOS_ERROR_OK)
		{
			err_exit(fail, __LINE__);
		}
	}
	if(osi_time_get_timestamp(&two) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	if(osi_time_diff(&one, &two, &diff) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	if(diff.sec != TEST_FIVE_SLEEP)
	{
		printf("Test 5: FAILED... %u %u\n", diff.sec, diff.nsec);
		err_exit(fail, __LINE__);
	}
	if(test_five_count != TEST_FIVE_COUNT)
	{
		err_exit(fail, __LINE__);
	}
	if(osi_sem_destroy(&sem) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	printf("Test 5: Done...%d[ms] %u[ns]\n", (int)(diff.nsec/1000000.0), diff.nsec);
}

#define TEST_SIX_CHECK (1)

int test_six_check = 0;

void* test_six_thread(void* arg)
{
	osi_bin_sem_t *sem = (osi_bin_sem_t*) arg;
	int fail = 6;

	printf("Test 6: Entered test thread\n");
	test_six_check = TEST_SIX_CHECK;
	if(osi_bin_sem_give(sem) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	printf("Test 6: Bin sem given\n");

	return NULL;
}

void test_six(void)
{
	osi_bin_sem_t *sem = NULL;
	int fail = 6;
	osi_thread_t *t = NULL;
	osi_thread_attr_t attr = {OSI_THREAD_DETACHED};

	printf("Test 6: Binary semaphores test\n");
	if(osi_bin_sem_create(&sem, true) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	printf("Test 6: Take sem...\n");
	if(osi_bin_sem_take(sem) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	if(osi_thread_create(&t, &attr, test_six_thread, sem) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	if(osi_thread_release(&t) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	printf("Test 6: Take sem again...\n");
	if(osi_bin_sem_take(sem) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	printf("Test 6: Check value...\n");
	if(test_six_check != TEST_SIX_CHECK)
	{
		err_exit(fail, __LINE__);
	}
	if(osi_bin_sem_destroy(&sem) != EOS_ERROR_OK)
	{
		err_exit(fail, __LINE__);
	}
	printf("Test 6: Done...\n");
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
	test_four();
	test_five();
	test_six();

	return 0;
}

