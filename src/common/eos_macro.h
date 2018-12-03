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


#ifndef EOS_MACRO_H_
#define EOS_MACRO_H_

//#include "util_log.h"
#ifndef EOS_NAME
#define EOS_NAME "eos"
#endif

#ifdef EOS_ALLOW_UNUSED
#define EOS_UNUSED(x) (void)(x);
#else
#define EOS_UNUSED(x) 
#endif

#define CALL_ON_LOAD(func) static void func() __attribute__((constructor));
#define CALL_ON_UNLOAD(func) static void func() __attribute__((destructor));

#ifndef __LINE__
#define __LINE__ -1
#endif

#ifndef __FILE__
#define __FILE__ "unknown file"
#endif

#ifdef EOS_DEBUG
#define EOS_ASSERT(condition) \
if (!(condition)) \
{ \
	/*util_log(NULL, "ASSERT", UTIL_LOG_LEVEL_ERR, "%s:%d in function %s", __FILE__, __LINE__, __func__);*/ \
	exit(-1); \
}
#else
#define EOS_ASSERT(condition)
#endif

#define EOS_MASK_SUBSET(param,mask) (((param) & (mask)) == (mask))

#endif // EOS_MACRO_H_
