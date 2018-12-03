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

#include "util_ilist.h"
#include "osi_memory.h"
#include "osi_mutex.h"
#include "eos_macro.h"


static bool util_ilist_is_tainted (util_ilist_iter_t *iter);
static void util_ilist_taint (util_ilist_iter_t *iter);
static void util_ilist_validate (util_ilist_iter_t *iter);

static eos_error_t util_ilist_get (util_ilist_iter_t *thiz,
		void **element);

static eos_error_t util_ilist_previous (util_ilist_iter_t *thiz,
		void **element);
static eos_error_t util_ilist_next (util_ilist_iter_t *thiz, void **element);
static eos_error_t util_ilist_first (util_ilist_iter_t *thiz, void **element);
static eos_error_t util_ilist_last (util_ilist_iter_t *thiz, void **element);

static eos_error_t util_ilist_insert_before (util_ilist_iter_t *thiz,
		void *element);
static eos_error_t util_ilist_insert_after (util_ilist_iter_t *thiz,
		void *element);
static eos_error_t util_ilist_insert_first (util_ilist_iter_t *thiz,
		void *element);
static eos_error_t util_ilist_insert_last (util_ilist_iter_t *thiz,
		void *element);

static eos_error_t util_ilist_remove (util_ilist_iter_t *thiz, void **element);
static eos_error_t util_ilist_remove_first (util_ilist_iter_t *thiz, void **element);
static eos_error_t util_ilist_remove_last (util_ilist_iter_t *thiz, void **element);

static eos_error_t util_ilist_for_each (util_ilist_iter_t *thiz,
		util_slist_traverse_t trav,
		util_ilist_for_each_t for_each_cb, void *cookie);
static eos_error_t util_ilist_remove_if (util_ilist_iter_t *thiz,
		util_slist_traverse_t trav,
		util_ilist_cmpr_t cmpr_cb, void *search_param,
		void **element);
static eos_error_t util_ilist_position_to (util_ilist_iter_t *thiz,
		util_slist_traverse_t trav,
		util_ilist_cmpr_t cmpr_cb,
		void *search_param, void **element);

static eos_error_t util_ilist_length (util_ilist_iter_t *thiz, uint32_t *length);

typedef struct util_ilist_element
{
	void *data;
	struct util_ilist_element *next;
	struct util_ilist_element *prev;
} util_ilist_element_t;

struct util_ilist
{
	uint64_t state;
	uint32_t iter_count;
	osi_mutex_t *sync;
	uint32_t length;
	util_ilist_element_t *head;
	util_ilist_element_t *tail;
	util_ilist_free_t free;
};

struct util_ilist_iter_priv
{
	uint64_t state;
	util_ilist_t *list;

	util_ilist_element_t *current;
};

eos_error_t util_ilist_create (util_ilist_t **list, util_ilist_free_t free_cb)
{
	util_ilist_t* priv = NULL;

	if (list == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	priv = (util_ilist_t*)osi_calloc(sizeof(util_ilist_t));
	if (priv == NULL)
	{
		return EOS_ERROR_NOMEM;
	}

	if (osi_mutex_create(&priv->sync) != EOS_ERROR_OK)
	{
		osi_free((void**)&priv);
		return EOS_ERROR_GENERAL;
	}

	priv->iter_count = 0;
	priv->length = 0;
	priv->state = 0;
	priv->free = free_cb;

	*list = priv;

	return EOS_ERROR_OK;
}

eos_error_t util_ilist_clear(util_ilist_t *list)
{
	util_ilist_element_t *next = NULL;
	util_ilist_element_t *current = NULL;

	if (list == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (list->free == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (osi_mutex_lock(list->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	next = list->head;
	current = next;

	while (next != NULL)
	{
		list->state++;
		if (list->length > 0)
		{
			list->length--;
		}
		current = next;
		next = next->next;
		if (list->free)
		{
			list->free(current->data);
		}
		current->data = NULL;
		current->next = NULL;
		osi_free((void**)&current);
	}

	list->head = NULL;
	list->tail = NULL;

	osi_mutex_unlock(list->sync);

	return EOS_ERROR_OK;
}

eos_error_t util_ilist_destroy (util_ilist_t **list)
{
	util_ilist_element_t *next = NULL;
	util_ilist_element_t *current = NULL;

	if (list == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (*list == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (osi_mutex_lock((*list)->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	if ((*list)->iter_count > 0)
	{
		osi_mutex_unlock((*list)->sync);
		// TODO Rethink
		return EOS_ERROR_INVAL;
	}

	next = (*list)->head;
	current = next;

	while(next != NULL)
	{
		(*list)->state = 0;
		if ((*list)->length > 0)
		{
			(*list)->length--;
		}
		current = next;
		next = next->next;
		if ((*list)->free)
		{
			(*list)->free(current->data);
		}
		current->data = NULL;
		current->next = NULL;
		osi_free((void**)&current);
	}

	osi_mutex_unlock((*list)->sync);

	if (osi_mutex_destroy(&(*list)->sync) != EOS_ERROR_OK)
	{
	}
	
	osi_free((void**)list);

	return EOS_ERROR_OK;
}

eos_error_t util_ilist_iter_create (util_ilist_iter_t **iter,
		util_ilist_t *list)
{
	util_ilist_iter_t *tmp = NULL;

	if ((iter == NULL) || (list == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	tmp = (util_ilist_iter_t*)osi_calloc(sizeof(util_ilist_iter_t));
	if (tmp == NULL)
	{
		return EOS_ERROR_NOMEM;
	}

	tmp->priv = (util_ilist_iter_priv_t*)osi_calloc(
			sizeof(util_ilist_iter_priv_t));
	if (tmp->priv == NULL)
	{
		return EOS_ERROR_NOMEM;
	}

	osi_mutex_lock(list->sync);

	tmp->get = util_ilist_get;

	tmp->previous = util_ilist_previous;
	tmp->next = util_ilist_next;
	tmp->first = util_ilist_first;
	tmp->last = util_ilist_last;

	tmp->insert_before = util_ilist_insert_before;
	tmp->insert_after = util_ilist_insert_after;
	tmp->insert_first = util_ilist_insert_first;
	tmp->insert_last = util_ilist_insert_last;

	tmp->remove = util_ilist_remove;
	tmp->remove_first = util_ilist_remove_first;
	tmp->remove_last = util_ilist_remove_last;

	tmp->for_each = util_ilist_for_each;
	tmp->remove_if = util_ilist_remove_if;
	tmp->position_to = util_ilist_position_to;

	tmp->length = util_ilist_length;


	tmp->priv->list = list;
	tmp->priv->state = list->state;
	tmp->priv->current = list->head;
	list->iter_count++;
	osi_mutex_unlock(list->sync);

	*iter = tmp;
	return EOS_ERROR_OK;
}

eos_error_t util_ilist_iter_validate(util_ilist_iter_t *iter)
{
	if (iter == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (iter->priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (osi_mutex_lock(iter->priv->list->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	util_ilist_validate(iter);

	osi_mutex_unlock(iter->priv->list->sync);

	return EOS_ERROR_OK;
}

eos_error_t util_ilist_iter_set(util_ilist_iter_t *dst, util_ilist_iter_t *src)
{
	if ((dst == NULL) || (src == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if ((dst->priv == NULL) || (src->priv == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (dst->priv->list != src->priv->list)
	{
		return EOS_ERROR_INVAL;
	}

	if (osi_mutex_lock(src->priv->list->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	dst->priv->state = src->priv->state;
	dst->priv->current = src->priv->current;

	osi_mutex_unlock(src->priv->list->sync);

	return EOS_ERROR_OK;
}

eos_error_t util_ilist_iter_destroy (util_ilist_iter_t **iter)
{
	util_ilist_iter_t *tmp = NULL;

	if (iter == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (*iter == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	tmp = *iter;

	if (tmp->priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	osi_mutex_lock(tmp->priv->list->sync);
	tmp->priv->list->iter_count--;
	osi_mutex_unlock(tmp->priv->list->sync);

	osi_free((void**)&tmp->priv);
	osi_free((void**)iter);

	return EOS_ERROR_OK;
}

static bool util_ilist_is_tainted (util_ilist_iter_t *iter)
{
	if (iter->priv->list == NULL)
	{
		return true;
	}

	if (iter->priv->state != iter->priv->list->state)
	{
		return true;
	}

	return false;
}

static void util_ilist_taint (util_ilist_iter_t *iter)
{
	iter->priv->list->state++;
}

static void util_ilist_validate (util_ilist_iter_t *iter)
{
	iter->priv->state = iter->priv->list->state;
}

static eos_error_t util_ilist_get (util_ilist_iter_t *thiz, void **element)
{
	util_ilist_t *list = NULL;
	bool eol = false;

	if ((element == NULL) || (thiz == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz->priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (util_ilist_is_tainted(thiz))
	{
		return EOS_ERROR_PERM;
	}

	if (osi_mutex_lock(thiz->priv->list->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	list = thiz->priv->list;

	if (util_ilist_is_tainted(thiz))
	{
		osi_mutex_unlock(list->sync);
		return EOS_ERROR_PERM;
	}

	if (thiz->priv->current == NULL)
	{
		if (thiz->priv->list->head == NULL)
		{
			eol = true;
		}
		*element = NULL;
		osi_mutex_unlock(list->sync);
		return eol ? EOS_ERROR_EOL:EOS_ERROR_PERM;
	}
	*element = thiz->priv->current->data;

	osi_mutex_unlock(list->sync);

	return EOS_ERROR_OK;
}

static eos_error_t util_ilist_previous (util_ilist_iter_t *thiz, void **element)
{
	util_ilist_t *list = NULL;
	bool eol = false;

	if (thiz == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz->priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (util_ilist_is_tainted(thiz))
	{
		return EOS_ERROR_PERM;
	}

	if (osi_mutex_lock(thiz->priv->list->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	list = thiz->priv->list;

	if (util_ilist_is_tainted(thiz))
	{
		osi_mutex_unlock(list->sync);
		return EOS_ERROR_PERM;
	}

	if (thiz->priv->current == NULL)
	{
		if (thiz->priv->list->head == NULL)
		{
			eol = true;
		}
		if (element != NULL)
		{
			*element = NULL;
		}
		osi_mutex_unlock(list->sync);
		return eol ? EOS_ERROR_EOL:EOS_ERROR_PERM;
	}
	
	if (thiz->priv->current->prev == NULL)
	{
		if (element != NULL)
		{
			*element = NULL;
		}
		osi_mutex_unlock(list->sync);
		return EOS_ERROR_EOL;
	}
	thiz->priv->current = thiz->priv->current->prev;
	if (element != NULL)
	{
		*element = thiz->priv->current->data;
	}

	osi_mutex_unlock(list->sync);

	return EOS_ERROR_OK;
}

static eos_error_t util_ilist_next (util_ilist_iter_t *thiz, void **element)
{
	util_ilist_t *list = NULL;
	bool eol = false;

	if (thiz == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz->priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (util_ilist_is_tainted(thiz))
	{
		return EOS_ERROR_PERM;
	}

	if (osi_mutex_lock(thiz->priv->list->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	list = thiz->priv->list;

	if (util_ilist_is_tainted(thiz))
	{
		osi_mutex_unlock(list->sync);
		return EOS_ERROR_PERM;
	}

	if (thiz->priv->current == NULL)
	{
		if (thiz->priv->list->head == NULL)
		{
			eol = true;
		}
		if (element != NULL)
		{
			*element = NULL;
		}
		osi_mutex_unlock(list->sync);
		return eol ? EOS_ERROR_EOL:EOS_ERROR_PERM;
	}
	
	if (thiz->priv->current->next == NULL)
	{
		if (element != NULL)
		{
			*element = NULL;
		}
		osi_mutex_unlock(list->sync);
		return EOS_ERROR_EOL;
	}
	thiz->priv->current = thiz->priv->current->next;
	if (element != NULL)
	{
		*element = thiz->priv->current->data;
	}

	osi_mutex_unlock(list->sync);

	return EOS_ERROR_OK;
}

static eos_error_t util_ilist_first (util_ilist_iter_t *thiz, void **element)
{
	util_ilist_t *list = NULL;

	if (thiz == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz->priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz->priv->list == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (osi_mutex_lock(thiz->priv->list->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	list = thiz->priv->list;

	thiz->priv->state = thiz->priv->list->state;
	thiz->priv->current = list->head;
	if (thiz->priv->current == NULL)
	{
		if (element != NULL)
		{
			*element = NULL;
		}
		osi_mutex_unlock(list->sync);
		return EOS_ERROR_EOL;
	}
	if (element != NULL)
	{
		*element = thiz->priv->current->data;
	}

	osi_mutex_unlock(list->sync);

	return EOS_ERROR_OK;
}

static eos_error_t util_ilist_last (util_ilist_iter_t *thiz, void **element)
{
	util_ilist_t *list = NULL;

	if ((element == NULL) || (thiz == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz->priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz->priv->list == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (osi_mutex_lock(thiz->priv->list->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	list = thiz->priv->list;

	thiz->priv->state = thiz->priv->list->state;
	thiz->priv->current = list->tail;
	if (thiz->priv->current == NULL)
	{
		if (element != NULL)
		{
			*element = NULL;
		}
		osi_mutex_unlock(list->sync);
		return EOS_ERROR_EOL;
	}
	if (element != NULL)
	{
		*element = thiz->priv->current->data;
	}

	osi_mutex_unlock(list->sync);

	return EOS_ERROR_OK;
}

static eos_error_t util_ilist_insert_before (util_ilist_iter_t *thiz,
		void *element)
{
	util_ilist_element_t *prev = NULL;
	util_ilist_t *list = NULL;

	if ((element == NULL) || (thiz == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz->priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (util_ilist_is_tainted(thiz))
	{
		return EOS_ERROR_PERM;
	}

	if (osi_mutex_lock(thiz->priv->list->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	list = thiz->priv->list;

	if (util_ilist_is_tainted(thiz))
	{
		osi_mutex_unlock(list->sync);
		return EOS_ERROR_PERM;
	}

	if (thiz->priv->current != NULL)
	{
		prev = (util_ilist_element_t*)osi_calloc(
				sizeof(util_ilist_element_t));
		prev->data = element;
		prev->next = thiz->priv->current;
		prev->prev = thiz->priv->current->prev;
		thiz->priv->current->prev = prev;

		if (thiz->priv->current == list->head)
		{
			list->head = prev;
		}
		else
		{
			prev->prev->next = prev;
			util_ilist_taint(thiz);
			util_ilist_validate(thiz);
		}
	}

	// Empty list
	if (thiz->priv->current == NULL)
	{
		list->head = (util_ilist_element_t*)osi_calloc(
				sizeof(util_ilist_element_t));
		list->head->data = element;
		list->tail = list->head;
		thiz->priv->current = list->head;
		util_ilist_taint(thiz);
		util_ilist_validate(thiz);
	}
	list->length++;

	osi_mutex_unlock(list->sync);

	return EOS_ERROR_OK;
}

static eos_error_t util_ilist_insert_after (util_ilist_iter_t *thiz, void *element)
{
	util_ilist_element_t *next = NULL;
	util_ilist_t *list = NULL;

	if ((element == NULL) || (thiz == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz->priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (util_ilist_is_tainted(thiz))
	{
		return EOS_ERROR_PERM;
	}

	if (osi_mutex_lock(thiz->priv->list->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	list = thiz->priv->list;

	if (util_ilist_is_tainted(thiz))
	{
		osi_mutex_unlock(list->sync);
		return EOS_ERROR_PERM;
	}

	if (thiz->priv->current != NULL)
	{
		next = (util_ilist_element_t*)osi_calloc(
				sizeof(util_ilist_element_t));
		next->data = element;
		next->next = thiz->priv->current->next;
		next->prev = thiz->priv->current;
		thiz->priv->current->next = next;

		if (thiz->priv->current == list->tail)
		{
			list->tail = next;
		}
		else
		{
			next->next->prev = next;
			util_ilist_taint(thiz);
			util_ilist_validate(thiz);
		}
	}

	// Empty list
	if (thiz->priv->current == NULL)
	{
		list->head = (util_ilist_element_t*)osi_calloc(
				sizeof(util_ilist_element_t));
		list->head->data = element;
		list->tail = list->head;
		thiz->priv->current = list->head;
		util_ilist_taint(thiz);
		util_ilist_validate(thiz);
	}
	list->length++;

	osi_mutex_unlock(list->sync);

	return EOS_ERROR_OK;
}

static eos_error_t util_ilist_insert_first (util_ilist_iter_t *thiz,
		void *element)
{
	util_ilist_t *list = NULL;

	if ((element == NULL) || (thiz == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz->priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (osi_mutex_lock(thiz->priv->list->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	list = thiz->priv->list;

	if (list->head == NULL)
	{
		list->head = (util_ilist_element_t*)osi_calloc(
				sizeof(util_ilist_element_t));
		list->head->data = element;
		list->tail = list->head;
		thiz->priv->current = list->head;
	}
	else
	{
		list->head->prev = (util_ilist_element_t*)osi_calloc(
				sizeof(util_ilist_element_t));
		list->head->prev->data = element;
		list->head->prev->next = list->head;
		list->head = list->head->prev;
	}
	util_ilist_taint(thiz);
	util_ilist_validate(thiz);
	list->length++;

	osi_mutex_unlock(list->sync);

	return EOS_ERROR_OK;
}

static eos_error_t util_ilist_insert_last (util_ilist_iter_t *thiz,
		void *element)
{
	util_ilist_t *list = NULL;

	if ((element == NULL) || (thiz == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz->priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (osi_mutex_lock(thiz->priv->list->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	list = thiz->priv->list;

	if (list->head == NULL)
	{
		list->head = (util_ilist_element_t*)osi_calloc(
				sizeof(util_ilist_element_t));
		list->head->data = element;
		list->tail = list->head;
		thiz->priv->current = list->head;
		util_ilist_taint(thiz);
		util_ilist_validate(thiz);
	}
	else
	{
		list->tail->next = (util_ilist_element_t*)osi_calloc(
				sizeof(util_ilist_element_t));
		list->tail->next->data = element;
		list->tail->next->prev = list->tail;
		list->tail = list->tail->next;
	}
	list->length++;

	osi_mutex_unlock(list->sync);

	return EOS_ERROR_OK;
}

static eos_error_t util_ilist_remove (util_ilist_iter_t *thiz, void **element)
{
	util_ilist_t *list = NULL;
	util_ilist_element_t *tmp = NULL;

	if (thiz == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz->priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (util_ilist_is_tainted(thiz))
	{
		return EOS_ERROR_PERM;
	}

	if (osi_mutex_lock(thiz->priv->list->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	list = thiz->priv->list;

	if (util_ilist_is_tainted(thiz))
	{
		osi_mutex_unlock(list->sync);
		return EOS_ERROR_PERM;
	}

	if (thiz->priv->current == NULL)
	{
		if (element != NULL)
		{
			*element = NULL;
		}
		osi_mutex_unlock(list->sync);
		return EOS_ERROR_EOL;
	}
	
	tmp = thiz->priv->current;
	if (tmp->next == NULL)
	{
		if (tmp->prev == NULL)
		{
			// Empty list
			thiz->priv->list->head = NULL;
		}
		else
		{
			tmp->prev->next = NULL;
		}
		thiz->priv->current = tmp->prev;
		thiz->priv->list->tail = thiz->priv->current;
	}
	else
	{
		thiz->priv->current = tmp->next;
		thiz->priv->current->prev = tmp->prev;
		if (tmp->prev == NULL)
		{
			thiz->priv->list->head = thiz->priv->current;
		}
		else
		{
			tmp->prev->next = thiz->priv->current;
		}
	}

	if (element != NULL)
	{
		*element = tmp->data;
	}
	else
	{
		if (thiz->priv->list->free != NULL)
		{
			thiz->priv->list->free(tmp->data);
		}
	}

	list->length--;
	osi_free((void**)&tmp);

	util_ilist_taint(thiz);
	util_ilist_validate(thiz);

	osi_mutex_unlock(list->sync);

	return EOS_ERROR_OK;
}

static eos_error_t util_ilist_remove_first (util_ilist_iter_t *thiz, void **element)
{
	util_ilist_t *list = NULL;
	util_ilist_element_t *tmp = NULL;

	if (thiz == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz->priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (osi_mutex_lock(thiz->priv->list->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	list = thiz->priv->list;

	if (thiz->priv->list->head == NULL)
	{
		if (element != NULL)
		{
			*element = NULL;
		}
		osi_mutex_unlock(list->sync);
		return EOS_ERROR_EOL;
	}
	
	tmp = thiz->priv->list->head;
	if (tmp->next == NULL)
	{
		thiz->priv->list->tail = NULL;
	}
	else
	{
		tmp->next->prev = NULL;
	}
	thiz->priv->list->head = tmp->next;

	if (thiz->priv->current == tmp)
	{
		thiz->priv->current = thiz->priv->list->head;
	}

	if (element != NULL)
	{
		*element = tmp->data;
	}
	else
	{
		if (thiz->priv->list->free != NULL)
		{
			thiz->priv->list->free(tmp->data);
		}
	}

	list->length--;
	osi_free((void**)&tmp);

	util_ilist_taint(thiz);
	if (thiz->priv->current)
	{
		util_ilist_validate(thiz);
	}

	osi_mutex_unlock(list->sync);

	return EOS_ERROR_OK;
}

static eos_error_t util_ilist_remove_last (util_ilist_iter_t *thiz, void **element)
{
	util_ilist_t *list = NULL;
	util_ilist_element_t *tmp = NULL;

	if (thiz == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz->priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (osi_mutex_lock(thiz->priv->list->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	list = thiz->priv->list;

	if (thiz->priv->list->tail == NULL)
	{
		if (element != NULL)
		{
			*element = NULL;
		}
		osi_mutex_unlock(list->sync);
		return EOS_ERROR_EOL;
	}
	
	tmp = thiz->priv->list->tail;
	if (tmp->prev == NULL)
	{
		thiz->priv->list->head = NULL;
	}
	else
	{
		tmp->prev->next = NULL;
	}
	thiz->priv->list->tail = tmp->prev;

	if (thiz->priv->current == tmp)
	{
		thiz->priv->current = thiz->priv->list->tail;
	}

	if (element != NULL)
	{
		*element = tmp->data;
	}
	else
	{
		if (thiz->priv->list->free != NULL)
		{
			thiz->priv->list->free(tmp->data);
		}
	}

	list->length--;
	osi_free((void**)&tmp);

	util_ilist_taint(thiz);
	if (thiz->priv->current)
	{
		util_ilist_validate(thiz);
	}

	osi_mutex_unlock(list->sync);

	return EOS_ERROR_OK;
}

static eos_error_t util_ilist_for_each (util_ilist_iter_t *thiz,
		util_slist_traverse_t trav,
		util_ilist_for_each_t for_each_cb, void *cookie)
{
	util_ilist_t *list = NULL;
	util_ilist_element_t *current = NULL;
	bool reverse = false;

	if ((for_each_cb == NULL) || (thiz == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz->priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (util_ilist_is_tainted(thiz)
	&& (trav != UTIL_ILIST_BEG_END) && (trav != UTIL_ILIST_END_BEG))
	{
		return EOS_ERROR_PERM;
	}

	if (osi_mutex_lock(thiz->priv->list->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	list = thiz->priv->list;

	if (util_ilist_is_tainted(thiz)
	&& (trav != UTIL_ILIST_BEG_END) && (trav != UTIL_ILIST_END_BEG))
	{
		osi_mutex_unlock(list->sync);
		return EOS_ERROR_PERM;
	}

	switch (trav)
	{
		case UTIL_ILIST_BEG_END:
			current = list->head;
			break;
		case UTIL_ILIST_CURR_END:
			current = thiz->priv->current;
			break;
		case UTIL_ILIST_END_BEG:
			current = list->tail;
			reverse = true;
			break;
		case UTIL_ILIST_CURR_BEG:
			current = thiz->priv->current;
			reverse = true;
			break;
		default:
			osi_mutex_unlock(list->sync);
			return EOS_ERROR_INVAL;
	}

	while (current != NULL)
	{
		if (!for_each_cb(cookie, current->data))
		{
			break;
		}
		if (reverse)
		{
			current = current->prev;
		}
		else
		{
			current = current->next;
		}
	}
	osi_mutex_unlock(list->sync);

	return EOS_ERROR_OK;
}

static eos_error_t util_ilist_remove_if (util_ilist_iter_t *thiz, 
		util_slist_traverse_t trav, util_ilist_cmpr_t cmpr_cb, 
		void *search_param, void **element)
{
	util_ilist_element_t *next = NULL;
	util_ilist_element_t *current = NULL;
	util_ilist_t *list = NULL;
	bool found = false; 
	bool reverse = false;

	if ((search_param == NULL) || (thiz == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz->priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (util_ilist_is_tainted(thiz)
	&& (trav != UTIL_ILIST_BEG_END) && (trav != UTIL_ILIST_END_BEG))
	{
		return EOS_ERROR_PERM;
	}

	if (osi_mutex_lock(thiz->priv->list->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	list = thiz->priv->list;

	if (util_ilist_is_tainted(thiz)
	&& (trav != UTIL_ILIST_BEG_END) && (trav != UTIL_ILIST_END_BEG))
	{
		osi_mutex_unlock(list->sync);
		return EOS_ERROR_PERM;
	}

	switch (trav)
	{
		case UTIL_ILIST_BEG_END:
			next = list->head;
			break;
		case UTIL_ILIST_CURR_END:
			next = thiz->priv->current;
			break;
		case UTIL_ILIST_END_BEG:
			next = list->tail;
			reverse = true;
			break;
		case UTIL_ILIST_CURR_BEG:
			next = thiz->priv->current;
			reverse = true;
			break;
		default:
			osi_mutex_unlock(list->sync);
			return EOS_ERROR_INVAL;
	}
	current = next;

	// If we are removing the head element
	if (next != NULL)
	{
		if (reverse)
		{
			current = next->prev;
		}
		else
		{
			current = next->next;
		}
	}

	while (next != NULL)
	{
		if (cmpr_cb != NULL)
		{
			found = cmpr_cb(search_param, next->data);
		}
		else
		{
			found = (next->data == search_param)? true:false;
		}

		if (found == true)
		{
			// If not the first iteration
			if (current != NULL) 
			{
				if (reverse && (next != list->tail))
				{
					current->prev = next->prev;
				}
				if (!reverse && (next != list->head))
				{
					current->next = next->next;
				}
			}
			// If we are removng the current one, move current
			// pointer to one element before
			if (thiz->priv->current == next)
			{
				thiz->priv->current = current;
			}
			if (list->head == next)
			{
				list->head = current;
				if (list->head != NULL)
				{
					list->head->prev = NULL;
				}
			}
			if (list->tail == next)
			{
				list->tail = current;
				if (list->tail != NULL)
				{
					list->tail->next = NULL;
				}
			}
			if (element == NULL)
			{
				if (list->free)
				{
					list->free(next->data);
				}
			}
			else
			{
				*element = next->data;
			}
			osi_free((void**)&next);
			next = NULL;
			list->length--;
			util_ilist_taint(thiz);
			util_ilist_validate(thiz);
			break;
		}
		current = next;
		if (reverse)
		{
			next = next->prev;
		}
		else
		{
			next = next->next;
		}
	}

	osi_mutex_unlock(list->sync);

	if (found != true)
	{
		return EOS_ERROR_NFOUND;
	}

	return EOS_ERROR_OK;
}

static eos_error_t util_ilist_position_to (util_ilist_iter_t *thiz,
		util_slist_traverse_t trav, util_ilist_cmpr_t cmpr_cb,
	       	void *search_param, void **element)
{
	util_ilist_element_t *current = NULL;
	util_ilist_t *list = NULL;
	bool found = false; 
	bool reverse = false; 

	if ((search_param == NULL) || (thiz == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz->priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (util_ilist_is_tainted(thiz)
	&& (trav != UTIL_ILIST_BEG_END) && (trav != UTIL_ILIST_END_BEG))
	{
		return EOS_ERROR_PERM;
	}

	if (osi_mutex_lock(thiz->priv->list->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	list = thiz->priv->list;

	if (util_ilist_is_tainted(thiz)
	&& (trav != UTIL_ILIST_BEG_END) && (trav != UTIL_ILIST_END_BEG))
	{
		osi_mutex_unlock(list->sync);
		return EOS_ERROR_PERM;
	}

	switch (trav)
	{
		case UTIL_ILIST_BEG_END:
			current = list->head;
			break;
		case UTIL_ILIST_CURR_END:
			current = thiz->priv->current;
			break;
		case UTIL_ILIST_END_BEG:
			current = list->tail;
			reverse = true;
			break;
		case UTIL_ILIST_CURR_BEG:
			current = thiz->priv->current;
			reverse = true;
			break;
		default:
			osi_mutex_unlock(list->sync);
			return EOS_ERROR_INVAL;
	}

	while (current != NULL)
	{
		if (cmpr_cb != NULL)
		{
			found = cmpr_cb(search_param, current->data);
		}
		else
		{
			found = (current->data == search_param)? true:false;
		}
		if (found == true)
		{
			if (element != NULL)
			{
				*element = current->data;
			}
			thiz->priv->current = current;
			thiz->priv->state = thiz->priv->list->state;
			break;
		}
		if (reverse)
		{
			current = current->prev;
		}
		else
		{
			current = current->next;
		}
	}


	osi_mutex_unlock(list->sync);

	if (found != true)
	{
		if (element != NULL)
		{
			*element = NULL;
		}
		return EOS_ERROR_NFOUND;
	}

	return EOS_ERROR_OK;
}

eos_error_t util_ilist_length (util_ilist_iter_t *thiz, uint32_t *length)
{
	util_ilist_t *list = NULL;

	if ((thiz == NULL) || (length == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz->priv == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (thiz->priv->list == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (osi_mutex_lock(thiz->priv->list->sync) != EOS_ERROR_OK)
	{
		return EOS_ERROR_INVAL;
	}

	list = thiz->priv->list;

	*length = list->length;

	osi_mutex_unlock(list->sync);

	return EOS_ERROR_OK;
}

