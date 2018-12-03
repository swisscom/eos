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

#include "util_slist.h"
#include "osi_memory.h"
#include "osi_mutex.h"
#include "eos_macro.h"

static eos_error_t util_slist_add (util_slist_t thiz, void* data_address);
static eos_error_t util_slist_remove (util_slist_t thiz, void* search_param);
static eos_error_t util_slist_get (util_slist_t thiz, void* search_param, void** data_address);
static eos_error_t util_slist_next (util_slist_t thiz, void** data_address);
static eos_error_t util_slist_first (util_slist_t thiz, void** data_addres);
static eos_error_t util_slist_last (util_slist_t thiz, void** data_address);
static eos_error_t util_slist_count (util_slist_t thiz, int32_t* count);

typedef struct util_slist_element
{
	void *data;
	struct util_slist_element *next;
} util_slist_element_t;

struct util_slist_priv
{
	osi_mutex_t *sync;
	int32_t count;
	util_slist_cmpr_t comparator;
	util_slist_element_t *head;
	util_slist_element_t *current;
	util_slist_element_t *tail;
};

eos_error_t util_slist_create(util_slist_t* list, util_slist_cmpr_t comparator)
{
	util_slist_priv_t* priv = NULL;

	if (list == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	priv = (util_slist_priv_t*)osi_calloc(sizeof(util_slist_priv_t));
	if (priv == NULL)
	{
		return EOS_ERROR_NOMEM;
	}

	if (osi_mutex_create(&priv->sync) != EOS_ERROR_OK)
	{
		osi_free((void**)&priv);
		return EOS_ERROR_GENERAL;
	}

	list->add = util_slist_add;
	list->remove = util_slist_remove;
	list->get = util_slist_get;
	list->next = util_slist_next;
	list->first = util_slist_first;
	list->last = util_slist_last;
	list->count = util_slist_count;
	priv->comparator = comparator;
	list->priv = priv;

	return EOS_ERROR_OK;
}

eos_error_t util_slist_destroy(util_slist_t* list)
{
	util_slist_element_t *next = NULL;
	util_slist_element_t *current = NULL;

	if (list == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (list->priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (osi_mutex_lock(list->priv->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	list->add = NULL;
	list->remove = NULL;
	list->next = NULL;
	list->first = NULL;
	list->last = NULL;
	list->count = NULL;
	
	next = list->priv->head;
	current = next;

	while(next != NULL)
	{
		list->priv->count--;
		current = next;
		next = next->next;
		current->data = NULL;
		current->next = NULL;
		osi_free((void**)&current);
	}

	list->priv->tail = NULL;
	list->priv->current = NULL;

	if (osi_mutex_unlock(list->priv->sync) != EOS_ERROR_OK)
	{
	}

	if (osi_mutex_destroy(&list->priv->sync) != EOS_ERROR_OK)
	{
	}
	
	osi_free((void**)&list->priv);
	list->priv = NULL;


	return EOS_ERROR_OK;
}

eos_error_t util_slist_add (util_slist_t thiz, void* data_address)
{
	util_slist_element_t **next = NULL;
	uint8_t found = 0;

	if ((data_address == NULL) || (thiz.priv == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (osi_mutex_lock(thiz.priv->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz.priv->head == NULL)
	{
		thiz.priv->head = (util_slist_element_t*)osi_calloc(sizeof(util_slist_element_t));
		thiz.priv->head->data = data_address;
		thiz.priv->tail = thiz.priv->head;
		thiz.priv->current = thiz.priv->head;
		thiz.priv->count++;
	}
	else
	{
		next = &thiz.priv->head;
	
		while(*next != NULL)
		{
			if ((*next)->data == data_address)
			{
				found = 1;
				break;
			}
			next = &(*next)->next;
		}

		if (found != 1)
		{
			thiz.priv->tail->next = (util_slist_element_t*)osi_calloc(sizeof(util_slist_element_t));
			thiz.priv->tail->next->data = data_address;
			thiz.priv->tail = thiz.priv->tail->next;
			thiz.priv->count++;
		}
	}

	if (osi_mutex_unlock(thiz.priv->sync) != EOS_ERROR_OK)
	{
	}

	if (found != 1)
	{
		return EOS_ERROR_OK;
	}
	else
	{
		return EOS_ERROR_GENERAL;
	}
}

eos_error_t util_slist_remove (util_slist_t thiz, void* search_param)
{
	util_slist_element_t *next = NULL;
	util_slist_element_t *current = NULL;
	bool found = false; 

	if ((search_param == NULL) || (thiz.priv == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (osi_mutex_lock(thiz.priv->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	next = thiz.priv->head;
	current = next;

	// If we are removing the head element
	if (thiz.priv->head != NULL)
	{
		current = thiz.priv->head->next;
	}

	while(next != NULL)
	{
		if (thiz.priv->comparator != NULL)
		{
			found = thiz.priv->comparator(search_param, next->data);
		}
		else
		{
			found = (next->data == search_param)? true:false;
		}
		if (found == true)
		{
			// Empty list
			if ((current != NULL) && (next != thiz.priv->head))
			{
				current->next = next->next;
			}
			// If we are removng the current one, move current pointer to one element before
			if (thiz.priv->current == next)
			{
				thiz.priv->current = current;
			}
			if (thiz.priv->head == next)
			{
				thiz.priv->head = current;
			}
			if (thiz.priv->tail == next)
			{
				thiz.priv->tail = current;
			}
			osi_free((void**)&next);
			next = NULL;
			thiz.priv->count--;
			break;
		}
		current = next;
		next = next->next;
	}

	if (osi_mutex_unlock(thiz.priv->sync) != EOS_ERROR_OK)
	{
	}

	if (found != true)
	{
		return EOS_ERROR_NFOUND;
	}

	return EOS_ERROR_OK;
}

static eos_error_t util_slist_get (util_slist_t thiz, void* search_param, void** data_address)
{
	util_slist_element_t *current = NULL;
	bool found = false; 

	if ((search_param == NULL) || (thiz.priv == NULL) || (data_address == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (osi_mutex_lock(thiz.priv->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	current = thiz.priv->head;

	while(current != NULL)
	{
		if (thiz.priv->comparator != NULL)
		{
			found = thiz.priv->comparator(search_param, current->data);
		}
		else
		{
			found = (current->data == search_param)? true:false;
		}
		if (found == true)
		{
			*data_address = current->data;
			break;
		}
		current = current->next;
	}

	if (osi_mutex_unlock(thiz.priv->sync) != EOS_ERROR_OK)
	{
	}

	if (found != true)
	{
		return EOS_ERROR_NFOUND;
	}

	return EOS_ERROR_OK;
}

eos_error_t util_slist_next (util_slist_t thiz, void** data_address)
{
	uint8_t eol = 0;
	uint8_t empty = 1;

	if ((data_address == NULL) || (thiz.priv == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (*data_address != NULL)
	{
	}

	if (osi_mutex_lock(thiz.priv->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz.priv->current != NULL)
	{
		empty = 0;
		if (thiz.priv->current->next != NULL)
		{
			thiz.priv->current = thiz.priv->current->next;
		}
		else
		{
			eol = 1;
		}
		*data_address = thiz.priv->current->data;
	}
	else
	{
		*data_address = NULL;
	}

	if (osi_mutex_unlock(thiz.priv->sync) != EOS_ERROR_OK)
	{
	}

	if (empty == 1)
	{
		return EOS_ERROR_EMPTY;
	}
	
	if (eol == 1)
	{
		return EOS_ERROR_EOL;
	}

	return EOS_ERROR_OK;
}

eos_error_t util_slist_first (util_slist_t thiz, void** data_address)
{
	uint8_t empty = 1;
	if ((data_address == NULL) || (thiz.priv == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (*data_address != NULL)
	{
	}

	if (osi_mutex_lock(thiz.priv->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}
	
	thiz.priv->current = thiz.priv->head;
	if (thiz.priv->current != NULL)
	{
		empty = 0;
		*data_address = thiz.priv->current->data;
	}
	else
	{
		*data_address = NULL;
	}

	if (osi_mutex_unlock(thiz.priv->sync) != EOS_ERROR_OK)
	{
	}

	if (empty == 1)
	{
		return EOS_ERROR_EMPTY;
	}

	return EOS_ERROR_OK;
}

eos_error_t util_slist_last (util_slist_t thiz, void** data_address)
{
	uint8_t empty = 1;
	if ((data_address == NULL) || (thiz.priv == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (*data_address != NULL)
	{
	}

	if (osi_mutex_lock(thiz.priv->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}
	
	thiz.priv->current = thiz.priv->tail;
	if (thiz.priv->current != NULL)
	{
		empty = 0;
		*data_address = thiz.priv->current->data;
	}
	else
	{
		*data_address = NULL;
	}

	if (osi_mutex_unlock(thiz.priv->sync) != EOS_ERROR_OK)
	{
	}

	if (empty == 1)
	{
		return EOS_ERROR_EMPTY;
	}

	return EOS_ERROR_OK;
}


eos_error_t util_slist_count (util_slist_t thiz, int32_t* count)
{
	if ((thiz.priv == NULL) || (count == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (osi_mutex_lock(thiz.priv->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	*count = thiz.priv->count;

	if (osi_mutex_unlock(thiz.priv->sync) != EOS_ERROR_OK)
	{
	}

	return EOS_ERROR_OK;
}

