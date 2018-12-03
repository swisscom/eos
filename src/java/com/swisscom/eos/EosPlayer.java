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

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.MalformedURLException;
import java.net.URI;
import java.net.URL;
import java.net.URLConnection;
import java.util.ArrayList;

import com.swisscom.eos.data.EosHbbTVFeed;
import com.swisscom.eos.data.EosDsmccFeed;
import com.swisscom.eos.data.EosSubtitlesFeed;
import com.swisscom.eos.data.EosTeletextFeed;
import com.swisscom.eos.event.EosConnectionStateChange;
import com.swisscom.eos.event.EosError;
import com.swisscom.eos.event.EosException;
import com.swisscom.eos.event.EosPlayInfo;
import com.swisscom.eos.event.EosPlaybackStatus;
import com.swisscom.eos.event.EosPlayerStatus;
import com.swisscom.eos.event.EosPlayerState;
import com.swisscom.eos.event.IEosPlayerListener;
import com.swisscom.eos.log.EosLog;
import com.swisscom.eos.media.EosMediaDesc;
import com.swisscom.eos.media.EosMediaTrack;
import com.swisscom.eos.out.EosOut;

public class EosPlayer implements INativeBridgeListener {
	private EosNativeBridge bridge;
	private EosOut out;
	private EosDataProvider dataProvider;
	private ArrayList<IEosPlayerListener> listeners;
	private EosPlayerStatus status;
	private EosDataFeed dataFeed;
	private EosTeletextFeed ttxtFeed;
	private EosSubtitlesFeed subFeed;
	private EosHbbTVFeed hbbtvFeed;
	private EosDsmccFeed dsmccFeed;
	private EosMediaDesc mediaDesc;
	
	private static final long EOS_PLAYER_CURRENT_TIME = -1;
	private static final short EOS_PLAYER_PAUSE_SPEED = 0;
	private static final short EOS_PLAYER_NORMAL_SPEED = 1;
	private static final String EOS_PLAYER_POS = "pos=";
	private static final String EOS_PLAYER_SPEED = "speed=";
	private static final String EOS_PLAYER_DELIMITER = "&";
	private static final String LOG_TAG = "EosPlayer";
	
	public EosPlayer(EosOut out) {
		if(out == null) {
			throw new IllegalArgumentException("OUT cannot be null");
		}
		EosLog.d(null, "Creating EosPlayer for" + out.getID());
		bridge = EosNativeBridge.getInstance();
		bridge.addListener(this);
		this.out = out;
		listeners = new ArrayList<IEosPlayerListener>();
		dataProvider = new EosDataProvider(out);
		
		dataFeed = new EosDataFeed(out);
		ttxtFeed = new EosTeletextFeed(dataFeed);
		dataFeed.setTeletextListener(ttxtFeed);
		subFeed = new EosSubtitlesFeed(dataFeed);
		dataFeed.setSubtitlesListener(subFeed);
		hbbtvFeed = new EosHbbTVFeed();
		dataFeed.setHbbTVListener(hbbtvFeed);
		dsmccFeed = new EosDsmccFeed();
		dataFeed.setDsmccListener(dsmccFeed);
		
		status = new EosPlayerStatus();
		status.setState(EosPlayerState.STOPPED);
		status.setError(EosError.OK);
		mediaDesc = null;
	}
	
	public EosOut getOut() {
		return out;
	}
	
	public void addListener(IEosPlayerListener listener) {
		synchronized(listeners) {
			listeners.add(listener);
		}
	}

	public void removeListener(IEosPlayerListener listener) {
		synchronized(listeners) {
			listeners.remove(listener);
		}
	}
	
	public void play(URI inUri, String inExtras) {
		String uri;
		
		if(inUri == null) {
			return;
		}
		EosLog.d("eos", "PLAY: " + inUri.toString());
		uri = inUri.toString();
		if(uri.startsWith("file:/") && !uri.startsWith("file:///")) {
			uri = uri.replace("file:/", "file:///");
		}
		if(uri.startsWith("lalala://")) {
			uri = uri.replace("lalala://", "https://");
			uri = uri.replace("&redirect=true", "");
			uri = resolveLalala(uri);
			if(uri == null) {
				fireError(EosError.NFOUND);
				return;
			}
		}
		try {
			if(status.getPlayInfo() != null) {
				status.setPlayInfo(null);
			}
			if(!checkTransitioning()) {
				return;
			}
			synchronized (status) {
				mediaDesc = null;
			}
			fireStateChanged(EosPlayerState.TRANSITIONING);
			bridge.start(out, uri, inExtras);
			// when called with extras, caller must update the status
			if(inExtras == null) {
				synchronized (status) {
					status.setUri(inUri);
					status.setError(EosError.OK);
				}
			}
		} catch (EosException e) {
			fireError(e.getError());
		}
	}

	public void play(URI inUri, EosTime offset, short speed) {
		String extras = null;
		
		if(offset != null || speed != EOS_PLAYER_NORMAL_SPEED) {
			extras = EOS_PLAYER_POS + (offset == null ? Integer.toString(0) : 
				Integer.toString(offset.toSeconds()));
			extras += EOS_PLAYER_DELIMITER;
			extras += EOS_PLAYER_SPEED + Integer.toString(speed);
		}
		play(inUri, extras);
		synchronized (status) {
			status.setUri(inUri);
			status.setError(EosError.OK);
			/*
			if(speed == EOS_PLAYER_PAUSE_SPEED) {
				fireStateChanged(EosPlayerState.PAUSED);
			} else {
				fireStateChanged(EosPlayerState.PLAYING);
			}
			*/
		}
	}
	
	public void trickPlay(EosTime offset, short speed) {
		long nativeOff = EOS_PLAYER_CURRENT_TIME;
		
		if(offset != null) {
			nativeOff = offset.toSeconds();
		}
		try {
			if(!checkTransitioning()) {
				return;
			}
			fireStateChanged(EosPlayerState.TRANSITIONING);
			bridge.trickPlay(out, nativeOff, speed);
			synchronized (status) {
				status.setError(EosError.OK);
			}
			if(speed == EOS_PLAYER_PAUSE_SPEED) {
				fireStateChanged(EosPlayerState.PAUSED);
			} else {
				fireStateChanged(EosPlayerState.PLAYING);
			}
		} catch (EosException e) {
			fireError(e.getError());
		}
	}

	public void pause(boolean enable) {
		short speed = enable ? EOS_PLAYER_PAUSE_SPEED : 
			EOS_PLAYER_NORMAL_SPEED;
		
		trickPlay(null, speed);
	}
	
	public void setSpeed(short speed) {
		trickPlay(null, speed);
	}
	
	public void startBuffering() {
		try {
			if(!checkTransitioning()) {
				return;
			}
			fireStateChanged(EosPlayerState.TRANSITIONING);
			bridge.startBuffering(out);
		} catch (EosException e) {
			fireError(e.getError());
		}
		fireStateChanged(EosPlayerState.BUFFERING);
	}
	
	public void stopBuffering() {
		try {
			if(!checkTransitioning()) {
				return;
			}
			fireStateChanged(EosPlayerState.TRANSITIONING);
			bridge.stopBuffering(out);
		} catch (EosException e) {
			fireError(e.getError());
		}
		fireStateChanged(EosPlayerState.PLAYING);
	}
	
	public void jump(EosTime offset) {
		trickPlay(offset, EOS_PLAYER_NORMAL_SPEED);
	}
	
	public void stop() {
		try {
			if(!checkTransitioning()) {
				return;
			}
			fireStateChanged(EosPlayerState.TRANSITIONING);
			bridge.stop(out);
			synchronized (status) {
				status.setError(EosError.OK);
				status.setUri(null);
			}
			fireStateChanged(EosPlayerState.STOPPED);
		} catch (EosException e) {
			fireError(e.getError());
		}
	}
	
	public EosMediaDesc getMediaDesc() {	
		try {
			synchronized (status) {
				if(mediaDesc == null) {
					mediaDesc = bridge.getMediaDesc(out);
				}
				status.setError(EosError.OK);
			}
		} catch (EosException e) {
			fireError(e.getError());
		}
		
		return mediaDesc;
	}
	
	public void selectTrack(EosMediaTrack track) {
		if(track == null) {
			fireError(EosError.INVAL);
		}
		try {
			bridge.selectTrack(out, track.getID());
			synchronized (status) {
				status.setError(EosError.OK);
				mediaDesc = null;
			}
		} catch (EosException e) {
			fireError(e.getError());
		}
	}
	
	public void deselectTrack(EosMediaTrack track) {
		if(track == null) {
			fireError(EosError.INVAL);
		}
		try {
			bridge.deselectTrack(out, track.getID());
			synchronized (status) {
				status.setError(EosError.OK);
				mediaDesc = null;
			}
		} catch (EosException e) {
			fireError(e.getError());
		}
	}
	
	public EosDataProvider getDataProvider() {
		return dataProvider;
	}

	public EosTeletextFeed getTeletextFeed() {
		return ttxtFeed;
	}
	
	public EosSubtitlesFeed getSubtitlesFeed() {
		return subFeed;
	}
	
	public EosHbbTVFeed getHbbTVFeed() {
		return hbbtvFeed;
	}

	public EosDsmccFeed getDsmccFeed() {
		return dsmccFeed;
	}
	
	public static EosError setupVideo(EosOut out, int x, int y, 
			int width, int height) {
		try {
			// just in case... we must init EOS
			EosNativeBridge.getInstance();
			EosNativeBridge.setupVideo(out, x, y, width, height);
		} catch (EosException e) {
			return e.getError();
		}
		return EosError.OK;
	}
	
	public static EosError setAudioPassThrough(EosOut out, 
			boolean enable)  {
		try {
			// just in case... we must init EOS
			EosNativeBridge.getInstance();
			EosNativeBridge.setAudioPassThrough(out, enable);
		} catch (EosException e) {
			return e.getError();
		}
		return EosError.OK;
	}
	
	public static EosError setAudioVolumeLeveling(EosOut out,  boolean enable,
			EosVolumeLevel level)  {
		try {
			// just in case... we must init EOS
			EosNativeBridge.getInstance();
			EosNativeBridge.setVolumeLeveling(out, enable, level.ordinal());
		} catch (EosException e) {
			return e.getError();
		}
		return EosError.OK;
	}
	
	public static String getVersion() {
		// just in case... we must init EOS
		EosNativeBridge.getInstance();
		
		return EosNativeBridge.getVersion();
	}
	
	private void fireStateChanged(EosPlayerState state) {
		EosLog.d(LOG_TAG, "fireStateChanged ENTER");
		synchronized (status) {
			status.setState(state);
		}
		for (IEosPlayerListener lnr : listeners) {
			EosLog.d(LOG_TAG, "fireStateChanged FIRE");
			lnr.onStatusChange(status);
		}
		EosLog.d(LOG_TAG, "fireStateChanged EXIT");
	}

	private void firePlayInfo(EosPlayInfo playInfo) {
		synchronized (status) {
			status.setPlayInfo(playInfo);
		}
		for (IEosPlayerListener lnr : listeners) {
			lnr.onStatusChange(status);
		}
	}
	
	private void fireError(EosError err) {
		synchronized (status) {
			status.setError(err);	
		}
		for (IEosPlayerListener lnr : listeners) {
			lnr.onStatusChange(status);
		}
		// clear error after listener is informed
		synchronized (status) {
			status.setError(EosError.OK);
		}
	}
	
	public EosPlayerStatus getStatus() {
		return status;
	}

	@Override
	public void onStateChange(EosPlayerState state) {
		EosLog.d(LOG_TAG, "onStateChange ENTER");
		fireStateChanged(state);
		EosLog.d(LOG_TAG, "onStateChange EXIT");
	}

	@Override
	public void onPlayInfo(EosPlayInfo playInfo) {
		if(ignoreEvent()) {
			return;
		}
		firePlayInfo(playInfo);
	}

	@Override
	public void onError(EosError err) {
		fireError(err);
	}

	@Override
	public void onPlaybackStatus(EosPlaybackStatus playbackStatus) {
		if(ignoreEvent()) {
			return;
		}
		synchronized (status) {
			status.setPlaybackStatus(playbackStatus);	
		}
		for (IEosPlayerListener lnr : listeners) {
			lnr.onStatusChange(status);
		}
		// clear playback status after listener is informed
		synchronized (status) {
			status.setPlaybackStatus(null);	
		}
	}

	@Override
	public void onConnectionStateChange(EosConnectionStateChange connectionState) {
		synchronized (status) {
			status.setConnectionState(connectionState);	
		}
		for (IEosPlayerListener lnr : listeners) {
			lnr.onStatusChange(status);
		}
		// clear connection state after listener is informed
		synchronized (status) {
			status.setConnectionState(null);	
		}
	}

	private String resolveLalala(String httpUri) {
		URLConnection urlConn = null;
		InputStreamReader in = null;
		StringBuilder sb = new StringBuilder();
		
		System.out.println("Resolving lalala " + httpUri);
		try {
			URL url = new URL(httpUri);
			urlConn = url.openConnection();
			if (urlConn != null)
				urlConn.setReadTimeout(5 * 1000);
			if (urlConn != null && urlConn.getInputStream() != null) {
				in = new InputStreamReader(urlConn.getInputStream(),
						/*Charset.defaultCharset()*/"ISO-8859-1");
				BufferedReader bufferedReader = new BufferedReader(in);
				if (bufferedReader != null) {
					int cp;
					while ((cp = bufferedReader.read()) != -1) {
						sb.append((char) cp);
					}
					int begin = sb.indexOf("Address\":\"");
					if(begin == -1) {
						return null;
					}
					begin += "Address\":\"".length();
					int end = sb.lastIndexOf("\"}");
					if(end == -1) {
						end = sb.lastIndexOf("\",");
					}
					bufferedReader.close();
					return sb.substring(begin, end);
				}
			}
		} catch (MalformedURLException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
		return null;
	}
	
	private boolean checkTransitioning() {
		boolean check = false;
		
		synchronized (status) {
			if(status.getState() == EosPlayerState.TRANSITIONING) {
				check = false;
			}
			status.setError(EosError.OK);
			check = true;
		}
		
		if(!check) {
			fireError(EosError.BUSY);
		}
		
		return check;
	}
	
	private boolean ignoreEvent() {
		boolean ignore = false;
		
		synchronized (status) {
			if(status.getState() == EosPlayerState.TRANSITIONING) {
				ignore = true;
			}
		}
		
		return ignore;
	}
}
