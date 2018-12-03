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


#ifndef SET_REG_H_
#define SET_REG_H_

#include "eos_error.h"
#include "eos_types.h"

typedef enum set_reg_opt
{
	SET_REG_OPT_VID_POS,
	SET_REG_OPT_VID_SIZE,
	SET_REG_OPT_AUD_MODE,
	SET_REG_OPT_VOL_LVL,
	SET_REG_OPT_LAST
} set_reg_opt_t;

typedef union set_reg_val
{
	struct
	{
		uint16_t x;
		uint16_t y;
	} vid_pos;
	struct
	{
		uint16_t w;
		uint16_t h;
	} vid_size;
	bool aud_pt;
	struct
	{
		bool enable;
		eos_out_vol_lvl_t lvl;
	} vol_lvl;
} set_reg_val_t;

typedef eos_error_t (*set_reg_cbk_t) (set_reg_opt_t opt, set_reg_val_t* val,
		void* opaque);

eos_error_t set_reg_set_sys_cbk(uint32_t id, set_reg_opt_t opt,
		set_reg_cbk_t cbk, void* opaque);
eos_error_t set_reg_fetch(uint32_t id, set_reg_opt_t opt, set_reg_val_t* val);
eos_error_t set_reg_apply(uint32_t id, set_reg_opt_t opt, set_reg_val_t* val);
eos_error_t set_reg_add_cbk(uint32_t id, set_reg_cbk_t cbk, void* opaque);
eos_error_t set_reg_remove_cbk(uint32_t id, set_reg_cbk_t cbk, void* opaque);

#endif /* SET_REG_H_ */
