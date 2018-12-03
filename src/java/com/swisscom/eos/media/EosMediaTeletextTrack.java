package com.swisscom.eos.media;

import java.util.ArrayList;

public class EosMediaTeletextTrack extends EosMediaTrack {
	private ArrayList<EosMediaTeletextPage> pages;
	
	EosMediaTeletextTrack(int id, boolean selected) {
		super(id, selected, EosMediaCodec.TTXT);
		pages = new ArrayList<EosMediaTeletextPage>();
	}
	
	void addPage(EosMediaTeletextPage page) {
		pages.add(page);
	}
	
	public EosMediaTeletextPage[] getPages() {
		EosMediaTeletextPage[] array = new EosMediaTeletextPage[pages.size()];
		array = pages.toArray(array);
		
		return array;
	}
}
