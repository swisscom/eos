package com.swisscom.eos.media;

public class EosMediaAudioTrack extends EosMediaTrack {
	private String lang;
	private int sampleRate;
	private int channelCount;
	
	EosMediaAudioTrack(int id, boolean selected, EosMediaCodec codec, String lang,
			int sampleRate, int channelCount) {
		super(id, selected, codec);
		this.lang = lang;
		this.sampleRate = sampleRate;
		this.channelCount = channelCount;
	}
	
	public String getLang() {
		return lang;
	}

	void setLang(String lang) {
		this.lang = lang;
	}

	public int getSampleRate() {
		return sampleRate;
	}

	void setSampleRate(int sampleRate) {
		this.sampleRate = sampleRate;
	}

	public int getChannelCount() {
		return channelCount;
	}

	void setChannelCount(int channelCount) {
		this.channelCount = channelCount;
	}
}
