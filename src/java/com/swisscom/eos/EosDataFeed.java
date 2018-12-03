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

import com.swisscom.eos.data.EosDataFormat;
import com.swisscom.eos.data.EosDataKind;
import com.swisscom.eos.event.EosError;
import com.swisscom.eos.event.EosException;
import com.swisscom.eos.out.EosOut;

public class EosDataFeed implements IEosNativeDataListener {
	private EosOut out;
	private EosNativeBridge bridge;
	private IEosDataListener ttxtListener;
	private IEosDataListener hbbtvListener;
	private IEosDataListener dsmccListener;
	private IEosDataListener subListener;
	
	public EosDataFeed(EosOut out) {
		this.out = out;
		bridge = EosNativeBridge.getInstance();
		bridge.addDataListener(this);
	}
	
	public void setTeletextListener(IEosDataListener ttxtListener) {
		if(ttxtListener == null || this.ttxtListener != null) {
			throw new IllegalArgumentException("Null TTXT listener or "
					+ "listener already set!");
		}
		this.ttxtListener = ttxtListener;
	}

	public void setSubtitlesListener(IEosDataListener subListener) {
		if(subListener == null || this.subListener != null) {
			throw new IllegalArgumentException("Null subtitles listener "
					+ "or listener already set!");
		}
		this.subListener = subListener;
	}

	public void setHbbTVListener(IEosDataListener hbbtvListener) {
		if(hbbtvListener == null || this.hbbtvListener != null) {
			throw new IllegalArgumentException("Null HbbTV listener "
					+ "or listener already set!");
		}
		this.hbbtvListener = hbbtvListener;
	}
	
	public void setDsmccListener(IEosDataListener dsmccListener) {
		if(dsmccListener == null || this.dsmccListener != null) {
			throw new IllegalArgumentException("Null DSMCC listener "
					+ "or listener already set!");
		}
		this.dsmccListener = dsmccListener;
	}
	
	public void setTeletextEnabled(boolean enabled) throws EosException {
		bridge.setTtxtEnabled(out, enabled);
	}
	
	public void setTeletextPage(int page, int subPage) throws EosException {
		bridge.setTtxtPage(out, page, subPage);
	}

	public int getTeletextPage() throws EosException {
		return bridge.getTtxtPageNumber(out);
	}

	public int getTeletextSubPage() throws EosException {
		return bridge.getTtxtSubPageNumber(out);
	}
	
	public int getTeletextNextPage() throws EosException {
		return bridge.getTtxtNextPage(out);
	}
	
	public int getTeletextPreviousPage() throws EosException {
		return bridge.getTtxtPreviousPage(out);
	}
	
	public int getTeletextRedPage() throws EosException {
		return bridge.getTtxtRedPage(out);
	}
	
	public int getTeletextGreenPage() throws EosException {
		return bridge.getTtxtGreenPage(out);
	}

	public int getTeletextNextSubPage() throws EosException {
		return bridge.getTtxtNextSubPageNumber(out);
	}
	
	public int getTeletextPreviousSubPage() throws EosException {
		return bridge.getTtxtPeviousSubPageNumber(out);
	}
	
	public int getTeletextBluePage() throws EosException {
		return bridge.getTtxtBluePage(out);
	}

	public int getTeletextYellowPage() throws EosException {
		return bridge.getTtxtYellowPage(out);
	}
	
	public void setTeletextTransparency(int alpha) throws EosException {
	bridge.setTtxtTransparency(out, alpha);
	}

	public void setDvbSubtitlesEnabled(boolean enabled) throws EosException {
		bridge.setDvbSubtitlesEnabled(out, enabled);
	}
	
	@Override
	public EosError handleData(EosDataKind type, EosDataFormat format, String data) {
		switch(type) {
		case SUB:
			if(subListener != null) {
				return subListener.handleData(format, data);
			}
			break;
		case HBBTV:
			if(hbbtvListener != null) {
				return hbbtvListener.handleData(format, data);
			}
			break;
		case DSMCC:
			if(dsmccListener != null) {
				return dsmccListener.handleData(format, data);
			}
			break;
		case TTXT:
			if(ttxtListener != null) {
				return ttxtListener.handleData(format, data);
			}
			break;
		default:
			break;
		}
		return EosError.OK;
	}
	
	@Override
	public EosOut getOut() {
		return out;
	}
}
