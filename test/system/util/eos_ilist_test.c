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
#include "util_ilist.h"

#define MODULE_NAME "test:ilist"
#include "util_log.h"


#define ELEMENT_CNT 10000
#define TGT_NUM 7

typedef struct tfe_el
{
	uint8_t num;
} tfe_el_t;

void tfe_free(void* element)
{
	osi_free(&element);
}

bool tfe_operation(void *cookie, void *element)
{
	int32_t *tgt_cnt = cookie;
	tfe_el_t *el = element;

	if (el->num == TGT_NUM)
	{
		(*tgt_cnt)--;
	}

	return true;
}

#define tdp_free tfe_free
#define tdp_operation tfe_operation
#define tdp_el_t tfe_el_t

bool tdp_cmpr(void *search_param, void* element)
{
	uint8_t *tgt_num = (uint8_t*)search_param;
	tdp_el_t *el = element;

	if (el->num == *tgt_num)
	{
		return true;
	}

	return false;
}

typedef struct tid_data
{
	util_ilist_t *list;
	bool last;
	bool result;
} tid_data_t;

void *test_thread_insert(void *arg)
{
	tid_data_t *data = (tid_data_t*)arg;
	uint32_t i = 0;
	eos_error_t error = EOS_ERROR_OK;
	util_ilist_iter_t *iter = NULL;

	error = util_ilist_iter_create(&iter, data->list);
	if (error != EOS_ERROR_OK)
	{
		data->result &= false;
		return arg;
	}

	for (i = 0; i < ELEMENT_CNT && data->result; i++)
	{
		if (data->last)
		{
			data->result &= 
				(iter->insert_last(iter, &i) == EOS_ERROR_OK);
		}
		else
		{
			data->result &= 
				(iter->insert_first(iter, &i) == EOS_ERROR_OK);
		}
	}
	util_ilist_iter_destroy(&iter);

	return arg;
}

void *test_thread_delete(void *arg)
{
	tid_data_t *data = (tid_data_t*)arg;
	uint32_t i = 0;
	eos_error_t error = EOS_ERROR_OK;
	util_ilist_iter_t *iter = NULL;

	error = util_ilist_iter_create(&iter, data->list);
	if (error != EOS_ERROR_OK)
	{
		data->result &= false;
		return arg;
	}

	while (i < ELEMENT_CNT && data->result)
	{
		if (data->last)
		{
			error = iter->remove_last(iter, NULL);
			data->result &= (error == EOS_ERROR_OK)
				|| (error == EOS_ERROR_EOL);
		}
		else
		{
			error = iter->remove_first(iter, NULL);
			data->result &= (error == EOS_ERROR_OK)
				|| (error == EOS_ERROR_EOL);
		}
		if (error == EOS_ERROR_OK)
		{
			i++;
		}
	}

	util_ilist_iter_destroy(&iter);

	return arg;
}

bool test_insert_delete_edge()
{
	osi_thread_t *ins = NULL;
	osi_thread_t *del = NULL;
	eos_error_t error = EOS_ERROR_OK;
	util_ilist_t *list = NULL;
	tid_data_t ins_data = {NULL, true, true};
	tid_data_t del_data = {NULL, true, true};
	
	UTIL_GLOGI("Test: Insert Delete Edge ...");

	error = util_ilist_create(&list, NULL);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("List creation failed");
		UTIL_GLOGE("Test: Insert Delete Edge [Failure]");
		return false;
	}

	ins_data.list = list;
	del_data.list = list;

	osi_thread_create(&ins, NULL, test_thread_insert, &ins_data);
	osi_thread_create(&del, NULL, test_thread_delete, &del_data);

	osi_thread_join(ins, NULL);
	osi_thread_join(del, NULL);

	osi_thread_release(&ins);
	osi_thread_release(&del);

	if (!ins_data.result || !del_data.result)
	{
		util_ilist_destroy(&list);
		UTIL_GLOGE("Insert thread [%s]", 
				ins_data.result ? "Success":"Failre");
		UTIL_GLOGE("Delete thread [%s]", 
				del_data.result ? "Success":"Failre");
		UTIL_GLOGE("Test: Insert Delete Edge [0] [Failure]");
		return false;
	}

	ins_data.last = false;
	del_data.last = false;
	osi_thread_create(&ins, NULL, test_thread_insert, &ins_data);
	osi_thread_create(&del, NULL, test_thread_delete, &del_data);

	osi_thread_join(ins, NULL);
	osi_thread_join(del, NULL);

	osi_thread_release(&ins);
	osi_thread_release(&del);

	if (!ins_data.result || !del_data.result)
	{
		util_ilist_destroy(&list);
		UTIL_GLOGE("Insert thread [%s]", 
				ins_data.result ? "Success":"Failre");
		UTIL_GLOGE("Delete thread [%s]", 
				del_data.result ? "Success":"Failre");
		UTIL_GLOGE("Test: Insert Delete Edge [1] [Failure]");
		return false;
	}

	ins_data.last = false;
	del_data.last = true;
	osi_thread_create(&ins, NULL, test_thread_insert, &ins_data);
	osi_thread_create(&del, NULL, test_thread_delete, &del_data);

	osi_thread_join(ins, NULL);
	osi_thread_join(del, NULL);

	osi_thread_release(&ins);
	osi_thread_release(&del);

	if (!ins_data.result || !del_data.result)
	{
		util_ilist_destroy(&list);
		UTIL_GLOGE("Insert thread [%s]", 
				ins_data.result ? "Success":"Failre");
		UTIL_GLOGE("Delete thread [%s]", 
				del_data.result ? "Success":"Failre");
		UTIL_GLOGE("Test: Insert Delete Edge [2] [Failure]");
		return false;
	}

	ins_data.last = true;
	del_data.last = false;
	osi_thread_create(&ins, NULL, test_thread_insert, &ins_data);
	osi_thread_create(&del, NULL, test_thread_delete, &del_data);

	osi_thread_join(ins, NULL);
	osi_thread_join(del, NULL);

	osi_thread_release(&ins);
	osi_thread_release(&del);

	if (!ins_data.result || !del_data.result)
	{
		util_ilist_destroy(&list);
		UTIL_GLOGE("Insert thread [%s]", 
				ins_data.result ? "Success":"Failre");
		UTIL_GLOGE("Delete thread [%s]", 
				del_data.result ? "Success":"Failre");
		UTIL_GLOGE("Test: Insert Delete Edge [3] [Failure]");
		return false;
	}

	UTIL_GLOGI("Test: Insert Delete Edge [Success]");

	return true;
}

bool test_del_pos()
{
	uint32_t i = 0;
	uint32_t j = 0;
	tdp_el_t *el = NULL;
	int32_t tgt_cnt = 0;
	util_ilist_t *list = NULL;
	util_ilist_iter_t *iter1 = NULL;
	util_ilist_iter_t *iter2 = NULL;
	eos_error_t error = EOS_ERROR_OK;
	uint8_t tgt_num = 0;
	
	UTIL_GLOGI("Test: Delete Position ...");

	error = util_ilist_create(&list, tdp_free);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("List creation failed");
		goto done;
	}

	error = util_ilist_iter_create(&iter1, list);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Iterator creation failed");
		goto done;
	}

	error = util_ilist_iter_create(&iter2, list);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Iterator creation failed");
		goto done;
	}

	for (i = 0; i < ELEMENT_CNT; i++)
	{
		el = osi_calloc(sizeof(tdp_el_t));
		el->num = rand() % 10;
		if ((i == 0) || (i == ELEMENT_CNT - 1))
		{
			el->num = TGT_NUM;
		}
		if (el->num == TGT_NUM)
		{
			tgt_cnt++;
		}
		if (el->num % 2 == 0)
		{
			error = iter1->insert_last(iter1, (void*)el);
			if (error != EOS_ERROR_OK)
			{
				osi_free((void**)&el);
				UTIL_GLOGE("Add tail failed");
				goto done;
			}
		}
		else
		{
			error = iter1->insert_after(iter1, (void*)el);
			if (error != EOS_ERROR_OK)
			{
				osi_free((void**)&el);
				UTIL_GLOGE("Add insert_after failed");
				goto done;
			}
		}
	}

	UTIL_GLOGD("Added %d targeted numbers", tgt_cnt);
	error = iter2->first(iter2, UTIL_ILIST_OUT(&el));
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Positioning to first failed");
		goto done;
	}


	tgt_num = TGT_NUM;
	while (iter2->position_to(iter2, UTIL_ILIST_CURR_END, tdp_cmpr,
				&tgt_num, UTIL_ILIST_OUT(&el)) == EOS_ERROR_OK)
	{
		tgt_cnt--;
		error = iter2->next(iter2, UTIL_ILIST_OUT(&el));
		if (error == EOS_ERROR_EOL)
		{
			break;
		}
	}

	while (iter2->position_to(iter2, UTIL_ILIST_CURR_BEG, tdp_cmpr,
				&tgt_num, UTIL_ILIST_OUT(&el)) == EOS_ERROR_OK)
	{
		tgt_cnt++;
		error = iter2->previous(iter2, UTIL_ILIST_OUT(&el));
		if (error == EOS_ERROR_EOL)
		{
			break;
		}
	}

	while (iter2->position_to(iter2, UTIL_ILIST_CURR_END, tdp_cmpr,
				&tgt_num, UTIL_ILIST_OUT(&el)) == EOS_ERROR_OK)
	{
		tgt_cnt--;
		error = iter2->next(iter2, UTIL_ILIST_OUT(&el));
		if (error == EOS_ERROR_EOL)
		{
			break;
		}
	}

	error = iter2->first(iter2, UTIL_ILIST_OUT(&el));
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Positioning to first (second time) failed");
		goto done;
	}

	i = 0;
	error = iter2->length(iter2, &i);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Length retrieval failed");
		goto done;
	}

	if (i != ELEMENT_CNT)
	{
		error = EOS_ERROR_GENERAL;
		UTIL_GLOGE("Expected length %u, got %u", ELEMENT_CNT, i);
		goto done;
	}

	while (iter2->remove_if(iter2, UTIL_ILIST_BEG_END, tdp_cmpr, &tgt_num,
				UTIL_ILIST_OUT(&el)) == EOS_ERROR_OK)
	{
		i--;
	}

	j = 0;
	error = iter2->length(iter2, &j);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Length (second) retrieval failed");
		goto done;
	}

	if (i != j)
	{
		error = EOS_ERROR_GENERAL;
		UTIL_GLOGE("Expected length %u, got %u", i, j);
		goto done;
	}

	if (tgt_cnt != 0)
	{
		UTIL_GLOGE("Pop failed");
		goto done;
	}

	error = iter1->for_each(iter1, UTIL_ILIST_BEG_END, tdp_operation, &tgt_cnt);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("For each call failed");
		goto done;
	}

done:

	if (iter2 != NULL)
	{
		util_ilist_iter_destroy(&iter2);
	}

	if (iter1 != NULL)
	{
		util_ilist_iter_destroy(&iter1);
	}

	if (list != NULL)
	{
		util_ilist_destroy(&list);
	}

	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test: Delete Position [Failure]");
		return false;
	}

	if (tgt_cnt == 0)
	{
		UTIL_GLOGI("Test: Delete Position [Success]");
	}
	else
	{
		UTIL_GLOGE("Expected 0, got %d", tgt_cnt);
		UTIL_GLOGE("Test: Delete Position [Failure]");
	}
	return tgt_cnt == 0;
}

bool test_for_each()
{
	uint32_t i = 0;
	tfe_el_t *el = NULL;
	int32_t tgt_cnt = 0;
	util_ilist_t *list = NULL;
	util_ilist_iter_t *iter = NULL;
	eos_error_t error = EOS_ERROR_OK;
	
	UTIL_GLOGI("Test: For Each ...");

	error = util_ilist_create(&list, tfe_free);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("List creation failed");
		goto done;
	}

	error = util_ilist_iter_create(&iter, list);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Iterator creation failed");
		goto done;
	}

	for (i = 0; i < ELEMENT_CNT; i++)
	{
		el = osi_calloc(sizeof(tfe_el_t));
		el->num = rand() % 10;
		if (el->num == TGT_NUM)
		{
			tgt_cnt++;
		}
		if (el->num % 2 == 0)
		{
			error = iter->insert_last(iter, (void*)el);
			if (error != EOS_ERROR_OK)
			{
				osi_free((void**)&el);
				UTIL_GLOGE("Add tail failed");
				goto done;
			}
		}
		else
		{
			error = iter->insert_after(iter, (void*)el);
			if (error != EOS_ERROR_OK)
			{
				osi_free((void**)&el);
				UTIL_GLOGE("Add insert_after failed");
				goto done;
			}
		}
	}

	UTIL_GLOGD("Added %d targeted numbers", tgt_cnt);
	error = iter->for_each(iter, UTIL_ILIST_BEG_END, tfe_operation, &tgt_cnt);
	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("For each call failed");
		goto done;
	}

done:

	if (iter != NULL)
	{
		util_ilist_iter_destroy(&iter);
	}

	if (list != NULL)
	{
		util_ilist_destroy(&list);
	}

	if (error != EOS_ERROR_OK)
	{
		UTIL_GLOGE("Test: For Each [Failure]");
		return false;
	}

	if (tgt_cnt == 0)
	{
		UTIL_GLOGI("Test: For Each [Success]");
	}
	else
	{
		UTIL_GLOGE("Expected 0, got %d", tgt_cnt);
		UTIL_GLOGE("Test: For Each [Failure]");
	}
	return tgt_cnt == 0;
}

int32_t main (void)
{
	bool result = true;

	result &= test_for_each();
	result &= test_del_pos();
	result &= test_insert_delete_edge();

	if (result)
	{
		return 0;
	}

	return -1;
}

