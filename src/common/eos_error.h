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


#ifndef EOS_ERROR_H_
#define EOS_ERROR_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	EOS_ERROR_OK,
	EOS_ERROR_GENERAL,
	EOS_ERROR_INVAL,
	EOS_ERROR_NFOUND,
	EOS_ERROR_NOMEM,
	EOS_ERROR_BUSY,
	EOS_ERROR_PERM,
	EOS_ERROR_TIMEDOUT,
	EOS_ERROR_EMPTY,
	EOS_ERROR_EOF,
	EOS_ERROR_EOL,
	EOS_ERROR_BOL,
	EOS_ERROR_OVERFLOW,
	EOS_ERROR_NIMPLEMENTED,
	EOS_ERROR_FATAL,
	EOS_ERROR_AGAIN,
	EOS_ERR_UNKNOWN
} eos_error_t;

#ifdef __cplusplus
}
#endif

#endif /* EOS_ERROR_H_ */
