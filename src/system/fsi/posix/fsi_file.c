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


#include "fsi_file.h"
#include "osi_memory.h"
#include "osi_error.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>


struct fsi_file_handle
{
	int fd;
};

struct fsi_fd_set
{
	fd_set rfds;
};
eos_error_t fsi_file_open(fsi_file_t** file, char* path, fsi_file_flag_t flags, fsi_file_mode_t modes)
{
	fsi_file_t *tmp = NULL;
	int pflag = 0;
	int err = 0;
	mode_t pmode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;

	if(file == NULL || path == NULL || flags == 0)
	{
		return EOS_ERROR_INVAL;
	}
	if((tmp = (fsi_file_t*)osi_malloc(sizeof(fsi_file_t))) == NULL)
	{
		return EOS_ERROR_NOMEM;
	}
	switch(flags)
	{
	case F_F_RO:
		pflag = O_RDONLY;
		break;
	case F_F_WR:
		pflag = O_WRONLY;
		break;
	case F_F_RW:
		pflag = O_RDWR;
		break;
	default:
		osi_free((void**)&tmp);
		return EOS_ERROR_INVAL;
	}
	if(modes & F_M_APPEND)
	{
		pflag |= O_APPEND;
	}
	if(modes & F_M_CREATE)
	{
		pflag |= O_CREAT;
	}
	if(modes & F_M_TRUNC)
	{
		pflag |= O_TRUNC;
	}
	if(modes & F_M_SYNC)
	{
		pflag |= O_DIRECT;
		pflag |= O_SYNC;
		pflag |= O_DSYNC;
	}
	if((tmp->fd = open(path, pflag, pmode)) < 0)
	{
		err = errno;
		osi_free((void**)&tmp);
		return osi_error_conv(err);
	}
	*file = tmp;

	return EOS_ERROR_OK;
}

eos_error_t fsi_file_close(fsi_file_t** file)
{
	eos_error_t err = EOS_ERROR_OK;

	if(file == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	if(close((*file)->fd) == -1)
	{
		err = osi_error_conv(errno);
	}
	osi_free((void**)file);

	return err;
}

eos_error_t fsi_file_read (fsi_file_t* file, uint8_t* buff, size_t *bytes)
{
	eos_error_t err = EOS_ERROR_OK;
	ssize_t bytes_read = 0;

	if ((file == NULL) || (buff == NULL) || (bytes == NULL))
	{
		return EOS_ERROR_INVAL;
	}
	bytes_read = read(file->fd, buff, *bytes);
	if (bytes_read == -1)
	{
		err = osi_error_conv(errno);
		bytes_read = 0;
	}
	else
	{
		if (bytes_read == 0)
		{
			err = EOS_ERROR_EOF;
		}
	}
	*bytes = (size_t)bytes_read;

	return err;
}

eos_error_t fsi_file_write (fsi_file_t* file, uint8_t* buff, size_t* bytes)
{
	eos_error_t err = EOS_ERROR_OK;
	ssize_t bytes_written = 0;

	if ((file == NULL) || (buff == NULL) || (bytes == NULL))
	{
		return EOS_ERROR_INVAL;
	}
	bytes_written = write(file->fd, buff, *bytes);
	if (bytes_written == -1)
	{
		err = osi_error_conv(errno);
		bytes_written = 0;
	}
	*bytes = (size_t)bytes_written;

	return err;
}

eos_error_t fsi_file_seek (fsi_file_t* file, int64_t offset, fsi_seek_from_t from)
{
	eos_error_t err = EOS_ERROR_OK;
	int whence = 0;

	if (file == NULL)
	{
		return EOS_ERROR_INVAL;
	}
	switch(from)
	{
	case F_S_CUR:
		whence = SEEK_CUR;
		break;
	case F_S_BEG:
		whence = SEEK_SET;
		break;
	case F_S_END:
		whence = SEEK_END;
		break;
	default:
		return EOS_ERROR_INVAL;
	}
	if (lseek(file->fd, offset, whence) == -1)
	{
		err = osi_error_conv(errno);
	}

	return err;
}

eos_error_t fsi_file_size (fsi_file_t* file, uint64_t* size)
{
	eos_error_t err = EOS_ERROR_OK;
	struct stat fstats;

	if ((file == NULL) || (size == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	*size = 0;
	if (fstat(file->fd, &fstats) == -1)
	{
		err = osi_error_conv(errno);
	}
	else
	{
		*size = (uint64_t)fstats.st_size;
	}

	return err;
}

void fsi_file_fd_zero(fsi_fd_set_t** fsi_rfds)
{
	fsi_fd_set_t *tmp = NULL;

	if((tmp = (fsi_fd_set_t*)osi_malloc(sizeof(fsi_fd_set_t))) == NULL)
	{
		return;
	}
	FD_ZERO(&tmp->rfds);

	*fsi_rfds = tmp;

	return;
}

void fsi_file_fd_set(fsi_file_t* file, fsi_fd_set_t* fsi_rfds)
{
	FD_SET(file->fd, &fsi_rfds->rfds);

	return;
}

eos_error_t fsi_file_select(fsi_file_t* file, fsi_fd_set_t* fsi_rfds, osi_time_t timeout)
{
	eos_error_t err = EOS_ERROR_OK;
	struct timeval tv;
	int ret = 0;

	if ((file == NULL) || (fsi_rfds == NULL))
	{
		return EOS_ERROR_INVAL;
	}

	 tv.tv_sec = timeout.sec;
	 tv.tv_usec = timeout.nsec/1000;

	 ret = select(file->fd + 1, &fsi_rfds->rfds, NULL, NULL, &tv);

	if(ret == -1)
	{
		err = osi_error_conv(errno);
	}
	else if(ret==0)
	{
		err = osi_error_conv(errno);
	}

	return err;
}
