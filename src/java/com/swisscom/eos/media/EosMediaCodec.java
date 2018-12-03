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

package com.swisscom.eos.media;

public enum EosMediaCodec {
	MPEG1,
	MPEG2,
	MPEG4,	//H.263
	H264,	//AVC
	H265,	//HEVC
	MP1,
	MP2,
	MP3,
	AAC,
	HEAAC,
	AC3,
	EAC3,
	DTS,
	LPCM,
	TTXT,
	SUB,
	CC,
	CLK,
	HBBTV,
	DSMCC_A,
	DSMCC_B,
	DSMCC_C,
	DSMCC_D,
	UNKNOWN;
	
	public EosMediaCodecType getType() {
		switch(this) {
		case MPEG1:
		case MPEG2:
		case MPEG4:
		case H264:
		case H265:
			return EosMediaCodecType.VIDEO;
		case MP1:
		case MP2:
		case MP3:
		case AAC:
		case HEAAC:
		case AC3:
		case EAC3:
		case DTS:
		case LPCM:
			return EosMediaCodecType.AUDIO;
		case TTXT:
		case SUB:
		case CC:
		case CLK:
		case HBBTV:
		case DSMCC_A:
		case DSMCC_B:
		case DSMCC_C:
		case DSMCC_D:
			return EosMediaCodecType.DATA;
		case UNKNOWN:
		default:
			return null;
		}
	}
}
