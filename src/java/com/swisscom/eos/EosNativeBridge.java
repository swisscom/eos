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

import java.util.ArrayList;

import com.swisscom.eos.data.EosDataFormat;
import com.swisscom.eos.data.EosDataKind;
import com.swisscom.eos.event.EosConnectionStateChange;
import com.swisscom.eos.event.EosError;
import com.swisscom.eos.event.EosException;
import com.swisscom.eos.event.EosPlayInfo;
import com.swisscom.eos.event.EosPlaybackStatus;
import com.swisscom.eos.event.EosPlayerState;
import com.swisscom.eos.log.EosLog;
import com.swisscom.eos.media.EosMediaDesc;
import com.swisscom.eos.out.EosOut;


class EosNativeBridge {
	private ArrayList<INativeBridgeListener> listeners;
	private ArrayList<IEosNativeDataListener> dataListeners;
	private static EosNativeBridge singleton;
	private static final String LOG_TAG = "EosNativeBridge";
	/**
	 * Helper inner class that helps building the singleton instance once when this class is 
	 * loaded by the ClassLoader.
	 * No need for synchronisation as the ClassLoader loads the class only once.
	 * (c) Bill Pugh
	 */
	private static class SingletonHelper {
		final static EosNativeBridge INSTANCE = buildBridge();
		
		private static EosNativeBridge buildBridge() {
			final EosNativeBridge bridge = new EosNativeBridge();
			EosLog.d(LOG_TAG, "EosNativeBridge: Constructed new EosNativeBridge [" + 
						Integer.toHexString(bridge.hashCode()) + "]");
			bridge.init();
			
			return bridge;
		}
	}

	public static EosNativeBridge getInstance() {
		return SingletonHelper.INSTANCE;
	}
	
	private EosNativeBridge() {
		System.loadLibrary("eos_jni");
		listeners = new ArrayList<INativeBridgeListener>();
		dataListeners = new ArrayList<IEosNativeDataListener>();
	}
	
	public native void init();
	public native void deinit();
	
	public native void start(EosOut out, String inUrl, 
			String inExtras) throws EosException;
	
	public native void startBuffering(EosOut out) throws EosException;
	
	public native void stopBuffering(EosOut out) throws EosException;
	
	public native void trickPlay(EosOut out, long offset, 
			short speed) throws EosException;
	
	public native void stop(EosOut out) throws EosException;
	
	public native EosMediaDesc getMediaDesc(EosOut out) throws EosException;
	
	public native void selectTrack(EosOut out, int id) throws EosException;
	
	public native void deselectTrack(EosOut out, int id) throws EosException;
	
	public native void setTtxtEnabled(EosOut out, boolean enable) 
			throws EosException;
	
	public native String getTtxtPage(EosOut out, int page) 
			throws EosException;
	
	public native void setTtxtPage(EosOut out, int page, int subPage) 
			throws EosException;
	
	public native int getTtxtPageNumber(EosOut out) throws EosException;
	
	public native int getTtxtSubPageNumber(EosOut out) throws EosException;
	
	public native int getTtxtNextPage(EosOut out) throws EosException;
	
	public native int getTtxtPreviousPage(EosOut out) throws EosException;
	
	public native int getTtxtRedPage(EosOut out) throws EosException;
	
	public native int getTtxtGreenPage(EosOut out) throws EosException;
	
	public native int getTtxtBluePage(EosOut out) throws EosException;
	
	public native int getTtxtYellowPage(EosOut out) throws EosException;
	
	public native int getTtxtNextSubPageNumber(EosOut out) throws EosException;
	
	public native int getTtxtPeviousSubPageNumber(EosOut out) 
			throws EosException;
			
	public native void setTtxtTransparency(EosOut out, int alpha) 
		throws EosException;
	
	public native void setDvbSubtitlesEnabled(EosOut out, boolean enable) 
			throws EosException;
	
	public static native void setupVideo(EosOut out, int x, int y, 
			int width, int height) throws EosException;

	public static native void setAudioPassThrough(EosOut out, 
			boolean enable) throws EosException;
	
	public static native void setVolumeLeveling(EosOut out, boolean enable, 
			int level) throws EosException;
	
	public static native String getVersion();
	
	void addListener(INativeBridgeListener listener) {
		listeners.add(listener);
	}
	
	void addDataListener(IEosNativeDataListener dataListener) {
		if(dataListener == null) {
			return;
		}
		this.dataListeners.add(dataListener);
	}
	
	private void fireStateChange(EosOut out, EosPlayerState state) {
		EosLog.d(LOG_TAG, "fireStateChange:: ID " + out.getID() + " " + state.name());
		for (INativeBridgeListener lsnr : listeners) {
			if(lsnr.getOut().getID() == out.getID()) {
				lsnr.onStateChange(state);
			}
		}
	}
	
	private void firePlayInfo(EosOut out, EosPlayInfo info) {
		EosLog.d(LOG_TAG, "firePlayInfo: ID " + out.getID());
		for (INativeBridgeListener lsnr : listeners) {
			if(lsnr.getOut().getID() == out.getID()) {
				lsnr.onPlayInfo(info);
			}
		}
	}

	private void fireError(EosOut out, EosError err) {
		EosLog.d(LOG_TAG, "fireError: ID " + out.getID() + " " + err.name());
		for (INativeBridgeListener lsnr : listeners) {
			if(lsnr.getOut().getID() == out.getID()) {
				lsnr.onError(err);
			}
		}
	}

	private void firePlaybackStatus(EosOut out, 
			EosPlaybackStatus playbackStatus) {
		EosLog.d(LOG_TAG, "firePlaybackStatus: ID " + out.getID() + " " +
			playbackStatus.name());
		for (INativeBridgeListener lsnr : listeners) {
			if(lsnr.getOut().getID() == out.getID()) {
				lsnr.onPlaybackStatus(playbackStatus);
			}
		}
	}
	
	private void fireConnectionChange(EosOut out, 
			EosConnectionStateChange state) {
		for (INativeBridgeListener lsnr : listeners) {
			if(lsnr.getOut().getID() == out.getID()) {
				lsnr.onConnectionStateChange(state);
			}
		}
	}
	
	private void fireData(EosOut out, EosDataKind kind, EosDataFormat format,
			String data) {
		EosLog.d(LOG_TAG, "fireData: ID " + out.getID() + " " + kind.name());
		for (IEosNativeDataListener lsnr : dataListeners) {
			if(lsnr.getOut().getID() == out.getID()) {
				lsnr.handleData(kind, format, data);
			}
		}
	}
}
