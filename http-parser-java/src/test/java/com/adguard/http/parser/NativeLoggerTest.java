package com.adguard.http.parser;

import org.junit.Test;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.PrintStream;

/**
 * Created by s.fionov on 30.11.16.
 */
public class NativeLoggerTest {
	static {
		System.setProperty("java.library.path", "/home/sw/HttpLibrary/http-parser-java/jni");
		System.loadLibrary("httpparser-jni");
	}

	@Test
	public void testNativeLogger() throws FileNotFoundException {
		String logFile = "/tmp/logfile" + System.currentTimeMillis();
		try (NativeLogger logger = NativeLogger.open(NativeLogger.LogLevel.INFO)) {
			logger.log(NativeLogger.LogLevel.ERROR, "test-error");
		}

		try (NativeLogger logger = NativeLogger.open(NativeLogger.LogLevel.INFO, logFile)) {
			logger.log(NativeLogger.LogLevel.TRACE, "test-trace");
			logger.log(NativeLogger.LogLevel.INFO, "test-info");
			logger.log(NativeLogger.LogLevel.WARN, "test-warn");
		}

		NativeLogger.Callback callback = new NativeLogger.Callback() {
			@Override
			public void log(NativeLogger.LogLevel logLevel, String threadInfo, String message) {
				System.out.println(logLevel + " " + threadInfo + " " + message);
			}
		};

		try (NativeLogger logger = NativeLogger.open(NativeLogger.LogLevel.INFO, callback)) {
			logger.log(NativeLogger.LogLevel.TRACE, "test-trace");
			logger.log(NativeLogger.LogLevel.INFO, "test-info");
			logger.log(NativeLogger.LogLevel.WARN, "test-warn");
		}
	}
}
