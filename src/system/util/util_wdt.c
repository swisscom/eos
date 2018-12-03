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


// *************************************
// *             Includes              *
// *************************************

#include "util_wdt.h"
#include "eos_macro.h"
#include "osi_memory.h"
#include "util_log.h"
#include "osi_thread.h"
#include "osi_time.h"
#include "osi_bin_sem.h"
#include "osi_mutex.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// *************************************
// *              Macros               *
// *************************************

#define MODULE_NAME "wdt"
#define WDT_RELEASE_PERIOD_MSEC 5
#define MSEC_IN_SEC 1000
#define NSEC_IN_MSEC 1000000

// *************************************
// *              Types                *
// *************************************

typedef enum util_wdt_state
{
	EOS_WDT_STATE_STOPPED = 0,
	EOS_WDT_STATE_RUNNING,
	EOS_WDT_STATE_SUSPENDED,
	EOS_WDT_STATE_DESTROYING
} util_wdt_state_t;

struct util_wdt
{
	osi_thread_t *thread;
	osi_bin_sem_t *trigger;
	osi_mutex_t *sync;

	util_wdt_state_t state;

	uint32_t timer_msec;
	util_wdt_timeout_t callback;
	void *callback_cookie;
};

// *************************************
// *            Prototypes             *
// *************************************

static void* watchdog_thread (void* arg);

// *************************************
// *         Global variables          *
// *************************************

// *************************************
// *             Threads               *
// *************************************

static void* watchdog_thread (void* arg)
{
	util_wdt_t *wdt = (util_wdt_t*)arg;
	osi_time_t wait_timeout = {0, 0};
	osi_time_t default_wait_timeout = {0, 0};
	eos_error_t error = EOS_ERROR_OK;

	default_wait_timeout.sec = wdt->timer_msec / MSEC_IN_SEC;
	default_wait_timeout.nsec = (wdt->timer_msec - (default_wait_timeout.sec * MSEC_IN_SEC)) * NSEC_IN_MSEC;


//	UTIL_GLOGE("WDT %p Start", wdt->trigger);
	while ((wdt->state == EOS_WDT_STATE_RUNNING) || (wdt->state == EOS_WDT_STATE_SUSPENDED))
	{
		if (wdt->state == EOS_WDT_STATE_SUSPENDED)
		{
			wait_timeout.sec = 10000;
			wait_timeout.nsec = 0;
//			UTIL_GLOGE("WDT %p Suspend", wdt->trigger);
		}
		else
		{
			wait_timeout.sec = default_wait_timeout.sec;
			wait_timeout.nsec = default_wait_timeout.nsec;
		}

		error = osi_bin_sem_timedtake(wdt->trigger, &wait_timeout);
		if (error != EOS_ERROR_OK)
		{
			if (error == EOS_ERROR_TIMEDOUT)
			{
				if (wdt->state == EOS_WDT_STATE_SUSPENDED)
				{
					continue;
				}

				wdt->callback(wdt->callback_cookie);
			}
			else
			{
				// Unexpected
				EOS_ASSERT(error == EOS_ERROR_OK)
			}
		}
	}
//	UTIL_GLOGE("WDT %p Finish", wdt->trigger);
	return arg;
}

// *************************************
// *         Local functions           *
// *************************************

// *************************************
// *       Global functions            *
// *************************************

eos_error_t util_wdt_create(util_wdt_t** wdt)
{
	eos_error_t error = EOS_ERROR_OK;

	if (wdt == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	
	*wdt = (util_wdt_t*)osi_calloc(sizeof(util_wdt_t));
	if (*wdt == NULL)
	{
		return EOS_ERROR_NOMEM;
	}

	error = osi_bin_sem_create(&(*wdt)->trigger, false);
	if (error != EOS_ERROR_OK)
	{
		osi_free((void**)wdt);
		return error;
	}
	
	error = osi_mutex_create(&(*wdt)->sync);
	if (error != EOS_ERROR_OK)
	{
		osi_bin_sem_destroy(&(*wdt)->trigger);
		osi_free((void**)wdt);
		return error;
	}

	return EOS_ERROR_OK;
}

eos_error_t util_wdt_destroy(util_wdt_t** wdt)
{
	eos_error_t error = EOS_ERROR_OK;
	util_wdt_state_t curerent_state = EOS_WDT_STATE_STOPPED;

	if (wdt == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	if (*wdt == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	error = osi_mutex_lock((*wdt)->sync);
	EOS_ASSERT(error == EOS_ERROR_OK)

	curerent_state = (*wdt)->state;
	(*wdt)->state = EOS_WDT_STATE_DESTROYING;
	
	error = osi_mutex_unlock((*wdt)->sync);
	EOS_ASSERT(error == EOS_ERROR_OK)

	if  (curerent_state != EOS_WDT_STATE_STOPPED)
	{
		// Unexpected (assume that it will end well :)
		if (curerent_state == EOS_WDT_STATE_DESTROYING)
		{
			return EOS_ERROR_OK;
		}

		// Somebody is careless
		if ((curerent_state == EOS_WDT_STATE_RUNNING) || (curerent_state == EOS_WDT_STATE_SUSPENDED))
		{
			error = util_wdt_stop(*wdt);
			if (error != EOS_ERROR_OK)
			{
				return error;
			}
		}
	}

	error = osi_mutex_destroy(&(*wdt)->sync);
	EOS_ASSERT(error == EOS_ERROR_OK)
	if (error != EOS_ERROR_OK)
	{
		// Unexpected
	}

	error = osi_bin_sem_destroy(&(*wdt)->trigger);
	EOS_ASSERT(error == EOS_ERROR_OK)
	if (error != EOS_ERROR_OK)
	{
		// Unexpected
	}

	osi_free((void**)wdt);

	return EOS_ERROR_OK;
}

eos_error_t util_wdt_start (util_wdt_t* wdt, uint32_t timer_msec, util_wdt_timeout_t callback, void* callback_cookie)
{
	eos_error_t error = EOS_ERROR_OK;

	if ((wdt == NULL) || (callback == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	if (callback_cookie == NULL)
	{
		// Weird but not impossible
	}

	error = osi_mutex_lock(wdt->sync);
	EOS_ASSERT(error == EOS_ERROR_OK)
	if (error != EOS_ERROR_OK)
	{
		return error;
	}

	if (wdt->state != EOS_WDT_STATE_STOPPED)
	{
		osi_mutex_unlock(wdt->sync);
		return EOS_ERROR_INVAL;
	}

	wdt->timer_msec = timer_msec;
	wdt->callback = callback;
	wdt->callback_cookie = callback_cookie;

	wdt->state = EOS_WDT_STATE_RUNNING;
	error = osi_thread_create(&wdt->thread, NULL, watchdog_thread, (void*)wdt);
	EOS_ASSERT(error == EOS_ERROR_OK)
	if (error != EOS_ERROR_OK)
	{
		osi_mutex_unlock(wdt->sync);
		return error;
	}

	error = osi_mutex_unlock(wdt->sync);
	EOS_ASSERT(error == EOS_ERROR_OK)
	if (error != EOS_ERROR_OK)
	{
		// Unexpected
	}

	return EOS_ERROR_OK;
}
eos_error_t util_wdt_stop (util_wdt_t* wdt)
{
	eos_error_t error = EOS_ERROR_OK;

	if (wdt == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	error = osi_mutex_lock(wdt->sync);
	EOS_ASSERT(error == EOS_ERROR_OK)
	if (error != EOS_ERROR_OK)
	{
		return error;
	}

	// Unexpected but assume that its OK
	if (wdt->state == EOS_WDT_STATE_STOPPED)
	{
		osi_mutex_unlock(wdt->sync);
		return EOS_ERROR_OK;
	}

	if (wdt->state == EOS_WDT_STATE_DESTROYING)
	{
		osi_mutex_unlock(wdt->sync);
		return EOS_ERROR_GENERAL;
	}
	
	wdt->state = EOS_WDT_STATE_STOPPED;	

	error = osi_bin_sem_give(wdt->trigger);
	EOS_ASSERT(error == EOS_ERROR_OK)
	if (error != EOS_ERROR_OK)
	{
		osi_mutex_unlock(wdt->sync);
		return error;
	}

	wdt->timer_msec = 0;
	wdt->callback = NULL;
	wdt->callback_cookie = NULL;

	error = osi_thread_join(wdt->thread, NULL);
	EOS_ASSERT(error == EOS_ERROR_OK)
	if (error != EOS_ERROR_OK)
	{
		osi_mutex_unlock(wdt->sync);
		return error;
	}

	error = osi_thread_release(&wdt->thread);
	EOS_ASSERT(error == EOS_ERROR_OK)
	if (error != EOS_ERROR_OK)
	{
		// Unexpected
	}

	error = osi_mutex_unlock(wdt->sync);
	EOS_ASSERT(error == EOS_ERROR_OK)
	if (error != EOS_ERROR_OK)
	{
		// Unexpected
	}

	return EOS_ERROR_OK;
}

eos_error_t util_wdt_keep_alive (util_wdt_t* wdt)
{
	eos_error_t error = EOS_ERROR_OK;

	if (wdt == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	error = osi_mutex_lock(wdt->sync);
	EOS_ASSERT(error == EOS_ERROR_OK)
	if (error != EOS_ERROR_OK)
	{
		return error;
	}

	if (wdt->state != EOS_WDT_STATE_RUNNING)
	{
		osi_mutex_unlock(wdt->sync);
		return EOS_ERROR_INVAL;
	}
	else
	{	
		error = osi_bin_sem_give(wdt->trigger);
		EOS_ASSERT(error == EOS_ERROR_OK)
		if (error != EOS_ERROR_OK)
		{
			osi_mutex_unlock(wdt->sync);
			return error;
		}
	}

	error = osi_mutex_unlock(wdt->sync);
	EOS_ASSERT(error == EOS_ERROR_OK)
	if (error != EOS_ERROR_OK)
	{
		// Unexpected
	}

	return EOS_ERROR_OK;
}

eos_error_t util_wdt_suspend (util_wdt_t* wdt, bool suspend)
{
	EOS_UNUSED(wdt)
	
	eos_error_t error = EOS_ERROR_OK;

	if (wdt == NULL)
	{
		return EOS_ERROR_INVAL;
	}

	//UTIL_GLOGE("WDT %p: %s", wdt->trigger, suspend?"suspend":"resume");

	error = osi_mutex_lock(wdt->sync);
	EOS_ASSERT(error == EOS_ERROR_OK)
	if (error != EOS_ERROR_OK)
	{
		return error;
	}

	switch (wdt->state)
	{
		case EOS_WDT_STATE_RUNNING:
			if (suspend == true)
			{
				wdt->state = EOS_WDT_STATE_SUSPENDED;
				error = osi_bin_sem_give(wdt->trigger);
				EOS_ASSERT(error == EOS_ERROR_OK)
			}
			break;
		case EOS_WDT_STATE_SUSPENDED:
			if (suspend == false)
			{
				wdt->state = EOS_WDT_STATE_RUNNING;
				error = osi_bin_sem_give(wdt->trigger);
				EOS_ASSERT(error == EOS_ERROR_OK)
			}
			break;
		default:
			osi_mutex_unlock(wdt->sync);
			return EOS_ERROR_INVAL;
	}

	error = osi_mutex_unlock(wdt->sync);
	EOS_ASSERT(error == EOS_ERROR_OK)
	if (error != EOS_ERROR_OK)
	{
		// Unexpected
	}

	return EOS_ERROR_OK;
}

