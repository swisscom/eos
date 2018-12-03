package com.swisscom.eos.log;

public class EosLog {
	private static IEosLogger logger;
	private static final String DEFAULT_MODULE = "eos";
	
	public static void setLogger(IEosLogger toSet) {
		logger = toSet;
	}
	
	public static void log(EosLogLevel level, String module, String message) {
		if(logger != null) {
			logger.log(level, module == null ? DEFAULT_MODULE : module, message);
		}
	}
	
	public static void v(String module, String message) {
		log(EosLogLevel.VERBOSE, module, message);
	}
	
	public static void d(String module, String message) {
		log(EosLogLevel.DEBUG, module, message);
	}
	
	public static void i(String module, String message) {
		log(EosLogLevel.INFO, module, message);
	}
	
	public static void w(String module, String message) {
		log(EosLogLevel.WARNING, module, message);
	}
	
	public static void e(String module, String message) {
		log(EosLogLevel.ERROR, module, message);
	}
}
