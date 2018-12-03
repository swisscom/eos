package com.swisscom.eos.media;

public class EosMediaSubtitleTrack extends EosMediaTrack {
	private String lang;
	
	EosMediaSubtitleTrack(int id, boolean selected, String lang) {
		super(id, selected, EosMediaCodec.SUB);
		this.lang = lang;
	}
	
	public String getLang() {
		return lang;
	}

	void setLang(String lang) {
		this.lang = lang;
	}
}

