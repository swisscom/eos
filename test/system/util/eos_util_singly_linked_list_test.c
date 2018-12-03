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
#include <stdint.h>

#include "osi_thread.h"
#include "osi_time.h"
#include "osi_memory.h"
#include "util_slist.h"


#ifndef __LINE__
#define __LINE__ (-1)
#endif


#define EOS_ASSERT(condition) \
if (!(condition)) \
{ \
	err_exit(__LINE__); \
}

void err_exit(int line)
{
	printf("err: [line: %d]\n", line);
	exit(-1);
}

typedef struct randomize_arg
{
	util_slist_t list;
	int32_t write_count;
	int32_t read_count;
	uint8_t walk_stop;
} randomize_arg_t;

void* randomizing_write_thread (void* arg)
{
	randomize_arg_t *thread_arg = (randomize_arg_t*)arg;
	int32_t i = 0;
	int32_t *element = NULL;
	util_slist_t list = thread_arg->list;

	printf("Randomize: Write thread started\n");
	for (i = 0; i < thread_arg->write_count; i++)
	{
		element = (int32_t*)osi_calloc(sizeof(int32_t));
		list.add(list, (void*)element);
		osi_time_usleep(rand() % 20);
	}

	printf("Randomize: Write thread finished\n");
	return arg;
}

void* randomizing_read_thread (void* arg)
{
	randomize_arg_t *thread_arg = (randomize_arg_t*)arg;
	int32_t i = 0;
	int32_t *element = NULL;
	util_slist_t list = thread_arg->list;

	printf("Randomize: Read thread started\n");
	for (i = 0; i < thread_arg->read_count; i++)
	{
		if (list.next(list, (void**)&element) != EOS_ERROR_OK)
		{
		}
		list.remove(list, (void*)element);
		osi_free((void**)&element);
		osi_time_usleep(rand() % 30);
	}
	printf("Randomize: Read thread finished\n");

	return arg;
}

void* randomizing_walk_thread (void* arg)
{
	randomize_arg_t *thread_arg = (randomize_arg_t*)arg;
	int32_t *element = NULL;
	eos_error_t err = EOS_ERROR_OK;
	util_slist_t list = thread_arg->list;

	printf("Randomize: Walk thread started\n");
	while(thread_arg->walk_stop == 0)
	{
		err = list.first(list, (void**)&element);
		while(err == EOS_ERROR_OK)
		{
			if (thread_arg->walk_stop == 0)
			{
				break;
			}
			osi_time_usleep(rand() % 30);
		}
		if ((err != EOS_ERROR_OK) && (err != EOS_ERROR_EOL) && (err != EOS_ERROR_EMPTY))
		{
			break;
		}
		if (err != EOS_ERROR_EMPTY)
		{
			osi_time_usleep(3000);
		}
	}
	printf("Randomize: Walk thread finished\n");
	return arg;
}

void randomize (util_slist_t list, int32_t write_count, int32_t read_count)
{
	int32_t count = 0;
	int32_t i = 0;
	int32_t *element = NULL;
	randomize_arg_t thread_arg =
	{
		.list = list,
		.write_count = write_count,
		.read_count = read_count,
		.walk_stop = 0
	};

	osi_thread_t *write_thread;
	osi_thread_t *read_thread;
	osi_thread_t *walk_thread;

	osi_thread_create(&write_thread, NULL, randomizing_write_thread, &thread_arg);
	osi_time_usleep(300000);
	osi_thread_create(&read_thread, NULL, randomizing_read_thread, &thread_arg);
	osi_thread_create(&walk_thread, NULL, randomizing_walk_thread, &thread_arg);

	osi_thread_join(write_thread, NULL);
	osi_thread_join(read_thread, NULL);
	thread_arg.walk_stop = 1;
	osi_thread_join(walk_thread, NULL);

	osi_thread_release(&write_thread);
	osi_thread_release(&read_thread);
	osi_thread_release(&walk_thread);

	list.count(list, &count);
	if (count != (write_count - read_count))
	{
		printf("Randomize: Diff %d (+%d) elements\n", write_count - read_count, count - (write_count - read_count));
		for (i = 1; i < count - (write_count - read_count); i++)
		{
			if (list.next(list, (void**)&element) == EOS_ERROR_OK)
			{
				list.remove(list, element);
				osi_free((void**)&element);
			}
			else
			{
				printf("Can't access element\n");
			}
		}

		if (list.first(list, (void**)&element) == EOS_ERROR_OK)
		{
			list.remove(list, element);
			osi_free((void**)&element);
		}
		else
		{
			printf("Can't access element\n");
		}
	}
}

#define TEST_FOUR_WRITE_COUNT 100000
#define TEST_FOUR_REMAIN_COUNT 300
void test_four (util_slist_t list)
{
	//randomize(list, TEST_FOUR_WRITE_COUNT, TEST_FOUR_WRITE_COUNT - TEST_FOUR_REMAIN_COUNT);
	randomize(list, TEST_FOUR_WRITE_COUNT, TEST_FOUR_WRITE_COUNT);
//	randomize(list, 50, 40);
}

#define TEST_THREE_ELEMENTS_COUNT 1000
void test_three (util_slist_t list)
{
	int32_t data[TEST_THREE_ELEMENTS_COUNT];
	uint32_t i = 0;
	int32_t count = 0;
	int32_t j = 0;
	int32_t *element = 0;
	
	printf("Test three: Populate list\n");
	for (i = 0; i < TEST_THREE_ELEMENTS_COUNT; i++)
	{
		data[i] = i;
		list.count(list, &count); 
		list.add(list, &data[i]);
		EOS_ASSERT(count == data[i])
	}
	printf("Test three: Remove odd elements\n");
	list.first(list, (void**)&element);
	for (i = 1, j = 0; i < TEST_THREE_ELEMENTS_COUNT; i+=2, j++)
	{
		list.count(list, &count); 
		EOS_ASSERT(count == TEST_THREE_ELEMENTS_COUNT - j)
		list.next(list, (void**)&element);
		EOS_ASSERT(*element == data[i])
		list.remove(list, element);
		list.next(list, (void**)&element);
	}
	EOS_ASSERT((count == TEST_THREE_ELEMENTS_COUNT/2) || (count == TEST_THREE_ELEMENTS_COUNT/2 + 1) || (count == TEST_THREE_ELEMENTS_COUNT/2 - 1))
	printf("Test three: Remove remaining elements\n");
	list.first(list, (void**)&element);
	while (list.next(list, (void**)&element) != EOS_ERROR_EOL)
	{
		list.remove(list, element);
	}
	
	list.count(list, &count); 
	EOS_ASSERT(count == 1)

	list.first(list, (void**)&element);
	list.remove(list, element);
	list.count(list, &count);
	EOS_ASSERT(count == 0)
	EOS_ASSERT(list.first(list, (void**)&element) == EOS_ERROR_EMPTY)
	EOS_ASSERT(element == NULL)
	EOS_ASSERT(list.next(list, (void**)&element) == EOS_ERROR_EMPTY)
	EOS_ASSERT(element == NULL)

	printf("Test three: Done\n");

}

#define TEST_TWO_ELEMENTS_COUNT 500
void test_two (util_slist_t list)
{
	int32_t data[TEST_TWO_ELEMENTS_COUNT];
	uint32_t i = 0;
	int32_t count = 0;
	int32_t *element = 0;
	
	printf("Test two: Populate list\n");
	for (i = 0; i < TEST_TWO_ELEMENTS_COUNT; i++)
	{
		data[i] = i;
		list.count(list, &count); 
		list.add(list, &data[i]);
		EOS_ASSERT(count == data[i])
	}
	printf("Test two: Move to the end of the list\n");
	while (list.next(list, (void**)&element) != EOS_ERROR_EOL)
	{
	}

	printf("Test two: Backward empty list\n");
	for (i = 0; i < TEST_TWO_ELEMENTS_COUNT; i++)
	{
		list.count(list, &count); 
		EOS_ASSERT(count == TEST_TWO_ELEMENTS_COUNT - data[i])
		EOS_ASSERT(list.next(list, (void**)&element) == EOS_ERROR_EOL)
		EOS_ASSERT(*element == data[TEST_TWO_ELEMENTS_COUNT - 1 - i])
		list.remove(list, element);
	}
	printf("Test two: Done\n");

}

#define TEST_ONE_ELEMENTS_COUNT 500
void test_one (util_slist_t list)
{
	int32_t data[TEST_ONE_ELEMENTS_COUNT];
	uint32_t i = 0;
	int32_t count = 0;
	int32_t *element = 0;
	
	printf("Test one: Populate list\n");
	for (i = 0; i < TEST_ONE_ELEMENTS_COUNT; i++)
	{
		data[i] = i;
		list.count(list, &count); 
		list.add(list, &data[i]);
		EOS_ASSERT(count == data[i])
	}
	printf("Test one: Forward empty list\n");
	for (i = 0; i < TEST_ONE_ELEMENTS_COUNT; i++)
	{
		list.count(list, &count); 
		EOS_ASSERT(count == TEST_ONE_ELEMENTS_COUNT - data[i])
		list.first(list, (void**)&element);
		EOS_ASSERT(*element == data[i])
		list.remove(list, element);
	}
	printf("Test one: Done\n");

}

int main(void)
{
	util_slist_t sl_list =
	{
	        .add = NULL,
	        .remove = NULL,
	        .next = NULL,
	        .first = NULL,
	        .priv = NULL
	};

	if (sl_list.priv == NULL)
	{
		if (util_slist_create(&sl_list, NULL) != EOS_ERROR_OK)
 		{
			return -1;
		}
	}

	test_one(sl_list);
	test_two(sl_list);
	test_three(sl_list);
	test_four(sl_list);
	util_slist_destroy(&sl_list);
	return -1;
}

