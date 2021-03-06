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

package com.swisscom.eos.data;

import com.swisscom.eos.EosDataFeed;
import com.swisscom.eos.IEosDataListener;
import com.swisscom.eos.event.EosError;
import com.swisscom.eos.event.EosException;

public class EosSubtitlesFeed implements IEosDataListener, IEosDataFeed {
	private IEosSubtitlesListener listener;
	private EosDataFeed feed;
	
	public EosSubtitlesFeed(EosDataFeed feed) {
		this.feed = feed;
	}
	
	public void setListener(IEosSubtitlesListener listener) {
		synchronized (this) {
			this.listener = listener;
		}
	}
	
	@Override
	public EosError handleData(EosDataFormat format, String data) {
		synchronized (this) {
			if(this.listener != null) {
				listener.handleSubtitle(format, data);
			}
		}
		return EosError.OK;
	}

	@Override
	public EosError enable() {
		try {
			feed.setDvbSubtitlesEnabled(true);
		} catch (EosException e) {
			return e.getError();
		}
		return EosError.OK;
	}

	@Override
	public EosError disable() {
		try {
			feed.setDvbSubtitlesEnabled(false);
		} catch (EosException e) {
			return e.getError();
		}
		return EosError.OK;
	}
}
