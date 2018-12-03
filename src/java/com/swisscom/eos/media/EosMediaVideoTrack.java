package com.swisscom.eos.media;

public class EosMediaVideoTrack extends EosMediaTrack {
	private int fps;
	private int width;
	private int height;
	
	EosMediaVideoTrack(int id, boolean selected, EosMediaCodec codec, int fps, 
			int width, int height) {
		super(id, selected, codec);
		this.fps = fps;
		this.width = width;
		this.height = height;
	}

	public int getFps() {
		return fps;
	}

	void setFps(int fps) {
		this.fps = fps;
	}

	public int getWidth() {
		return width;
	}

	void setWidth(int width) {
		this.width = width;
	}

	public int getHeight() {
		return height;
	}

	void setHeight(int height) {
		this.height = height;
	}
}
