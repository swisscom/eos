package com.swisscom.eos.log;

public interface IEosLogger {
	public abstract void log(EosLogLevel level, String module, String message);
}
