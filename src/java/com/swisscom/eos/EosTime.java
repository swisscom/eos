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

package com.swisscom.eos;

public class EosTime {
	private int hour;
	private int minute;
	private int second;
	
	private static final String EXC_MSG = "Time MUST be positive";
	
	public EosTime(int hour, int minute, int second) {
		if(hour < 0 || minute < 0 || second < 0) {
			throw new IllegalArgumentException(EXC_MSG);
		}
		this.hour = hour;
		this.minute = minute;
		this.second = second;
	}

	public int getHour() {
		return hour;
	}

	public void setHour(int hour) {
		if(hour < 0) {
			throw new IllegalArgumentException(EXC_MSG);
		}
		this.hour = hour;
	}

	public int getMinute() {
		return minute;
	}

	public void setMinute(int minute) {
		if(minute < 0) {
			throw new IllegalArgumentException(EXC_MSG);
		}
		this.minute = minute;
	}

	public int getSecond() {
		return second;
	}

	public void setSecond(int second) {
		if(second < 0) {
			throw new IllegalArgumentException(EXC_MSG);
		}
		this.second = second;
	}
	
	public int toSeconds() {
		int convert = this.second;
		convert += this.minute * 60;
		convert += this.hour * 3600;
		
		return convert;
	}
	
	@Override
	public String toString() {
		return hour + ":" + minute + ":" + second; 
	}
	
	public static EosTime fromSeconds(int seconds) {
		int h, m, s;
		
		if(seconds < 0) {
			throw new IllegalArgumentException(EXC_MSG);
		}
		h = seconds / 3600;
		seconds -= h * 3600;
		m = seconds / 60;
		s = seconds - m * 60;
		
		return new EosTime(h, m, s);
	}
}
