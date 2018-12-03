package com.swisscom.eos.media;

public class EosMediaTeletextPage {
	private EosMediaTeletextPageType type;
	private String lang;
	private int page;
	
	EosMediaTeletextPage(EosMediaTeletextPageType type, String lang, int page) {
		this.setType(type);
		this.setLang(lang);
		this.setPage(page);
	}

	public EosMediaTeletextPageType getType() {
		return type;
	}

	void setType(EosMediaTeletextPageType type) {
		this.type = type;
	}

	public String getLang() {
		return lang;
	}

	void setLang(String lang) {
		this.lang = lang;
	}

	public int getPage() {
		return page;
	}

	void setPage(int page) {
		this.page = page;
	}
}
