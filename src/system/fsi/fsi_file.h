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


#ifndef FSI_FILE_H_
#define FSI_FILE_H_

#include "eos_error.h"
#include "osi_time.h"

#include <stdint.h>
#include <stddef.h>


#define FSI_FILE_SEEK_END (-1)

typedef enum
{
	F_F_RO = 1,
	F_F_WR,
	F_F_RW
} fsi_file_flag_t;

typedef enum
{
	F_M_CREATE = 1,
	F_M_APPEND = 2,
	F_M_SYNC = 4,
	F_M_TRUNC = 8
} fsi_file_mode_t;

typedef enum
{
	F_S_CUR = 1,
	F_S_BEG,
	F_S_END
} fsi_seek_from_t;

typedef struct fsi_file_handle fsi_file_t;
typedef struct fsi_fd_set fsi_fd_set_t;

eos_error_t fsi_file_open(fsi_file_t** file, char* path, fsi_file_flag_t flags, fsi_file_mode_t modes);
eos_error_t fsi_file_close(fsi_file_t** file);
eos_error_t fsi_file_read(fsi_file_t* file, uint8_t* buff, size_t* bytes);
eos_error_t fsi_file_select(fsi_file_t* file, fsi_fd_set_t* fsi_rfds, osi_time_t timeout);
void fsi_file_fd_zero(fsi_fd_set_t** fsi_rfds);
void fsi_file_fd_set(fsi_file_t* file, fsi_fd_set_t* fsi_rfds);
eos_error_t fsi_file_write(fsi_file_t* file, uint8_t* buff, size_t* bytes);
eos_error_t fsi_file_seek(fsi_file_t* file, int64_t offset, fsi_seek_from_t from);
eos_error_t fsi_file_size (fsi_file_t* file, uint64_t* size);

#endif /* FSI_FILE_H_ */
