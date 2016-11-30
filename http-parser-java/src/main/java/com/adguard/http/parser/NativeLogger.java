package com.adguard.http.parser;

import java.io.Closeable;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Created by s.fionov on 29.11.16.
 */
public class NativeLogger implements Closeable, AutoCloseable {

	public enum LogLevel {
		ERROR(0),
		WARN(1),
		INFO(2),
		DEBUG(3),
		TRACE(4);

		int code;
		static final Map<Integer, LogLevel> map;

		static {
			map = new ConcurrentHashMap<>();
			for (LogLevel level : values()) {
				map.put(level.code, level);
			}
		}

		LogLevel(int code) {
			this.code = code;
		}

		public int getCode() {
			return code;
		}

		public static LogLevel getByCode(int code) {
			LogLevel logLevel = map.get(code);
			if (logLevel == null) {
				throw new IllegalArgumentException("No log level with code " + code);
			}
			return logLevel;
		}
	}

	long nativePtr;

	private static native NativeLogger open0(int logLevel, NativeCallback callback);

	public static NativeLogger open(LogLevel logLevel, Callback callback) {
		return open0(logLevel.getCode(), new NativeCallback(callback));
	}

	private static native NativeLogger open1(int logLevel, String fileName);

	public static NativeLogger open(LogLevel logLevel, String fileName) {
		return open1(logLevel.getCode(), fileName);
	}

	public static NativeLogger open(LogLevel logLevel) {
		return open1(logLevel.getCode(), null);
	}

	private static native boolean isOpen(long nativePtr);

	public boolean isOpen() {
		return isOpen(nativePtr);
	}

	private static native void close(long nativePtr);

	public void close() {
		close(nativePtr);
	}

	private static native void log(long nativePtr, int value, String message);

	public void log(LogLevel level, String message) {
		log(nativePtr, level.getCode(), message);
	}

	public static class NativeCallback {
		private final Callback callback;

		public NativeCallback(Callback callback) {
			this.callback = callback;
		}

		private void log(int logLevel, String threadInfo, String message) {
			callback.log(LogLevel.getByCode(logLevel), threadInfo, message);
		}
	}

	public interface Callback {
		void log(LogLevel logLevel, String threadInfo, String message);
	}
}
