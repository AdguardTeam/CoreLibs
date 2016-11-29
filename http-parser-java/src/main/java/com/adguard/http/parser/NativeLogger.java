package com.adguard.http.parser;

/**
 * Created by s.fionov on 29.11.16.
 */
public class NativeLogger {

	public static native void open(String fileName, int logLevel, LoggerCallback callback);

	public interface LoggerCallback {
		void log(int logLevel, String message);
	}
}
