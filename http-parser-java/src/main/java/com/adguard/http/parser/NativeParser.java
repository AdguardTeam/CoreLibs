package com.adguard.http.parser;

import java.io.IOException;

/**
 * Created by s.fionov on 08.11.16.
 */
public class NativeParser implements Parser {

	static {
		System.loadLibrary("httpparser-jni");
	}

	private long nativePtr;

	private static native void init(NativeParser parser);

	public NativeParser() {
		init(this);
	}

	public static native synchronized long connect(long parserNativePtr, long id, Callbacks callbacks);

	@Override
	public NativeConnection connect(long id, ParserCallbacks callbacks) {
		return new NativeConnection(connect(nativePtr, id, new Callbacks(callbacks)));
	}

	public native static synchronized int disconnect0(long connectionNativePtr, int direction);

	@Override
	public int disconnect(Connection connection, Direction direction) {
		return disconnect0(((NativeConnection) connection).nativePtr, direction.getCode());
	}

	public static native int input0(long connectionNativePtr, int direction, byte[] data);

	@Override
	public int input(Connection connection, Direction direction, byte[] data) {
		return input0(((NativeConnection) connection).nativePtr, direction.getCode(), data);
	}

	public static native int closeConnection(long connectionNativePtr);

	@Override
	public int close(Connection connection) {
		return closeConnection(((NativeConnection) connection).nativePtr);
	}

	public static native long getConnectionId(long nativePtr);

	public static native void closeParser(long parserNativePtr);

	@Override
	public void close() throws IOException {
		closeParser(nativePtr);
	}

	private static class Callbacks {
		private final ParserCallbacks callbacks;

		private Callbacks(ParserCallbacks callbacks) {
			this.callbacks = callbacks;
		}

		int onHttpRequestReceived(long id, long nativePtr) {
			callbacks.onHttpRequestReceived(id, new HttpMessage(nativePtr));
			return 0;
		}

		int onHttpRequestBodyStarted(long id) {
			return callbacks.onHttpRequestBodyStarted(id).getCode();
		}

		// TODO: pass pointer to be able to provide byte array from the byte array pool
		void onHttpRequestBodyData(long id, byte[] data) {
			callbacks.onHttpRequestBodyData(id, data);
		}

		void onHttpRequestBodyFinished(long id) {
			callbacks.onHttpRequestBodyFinished(id);
		}

		int onHttpResponseReceived(long id, long nativePtr) {
			callbacks.onHttpResponseReceived(id, new HttpMessage(nativePtr));
			return 0;
		}

		int onHttpResponseBodyStarted(long id) {
			return callbacks.onHttpResponseBodyStarted(id).getCode();
		}

		// TODO: pass pointer to be able to provide byte array from the byte array pool
		void onHttpResponseBodyData(long id, byte[] data) {
			callbacks.onHttpResponseBodyData(id, data);
		}

		void onHttpResponseBodyFinished(long id) {
			callbacks.onHttpResponseBodyFinished(id);
		}

		void onParseError(long id, int direction, int errorType, String message) {
			callbacks.onParseError(id, Direction.getByCode(direction), errorType, message);
		}
	}

	/**
	 * Created by s.fionov on 29.11.16.
	 */
	public static class NativeConnection implements Parser.Connection {
		private long nativePtr;

		public NativeConnection(long nativePtr) {
			this.nativePtr = nativePtr;
		}

		@Override
		public long getId() {
			return getConnectionId(nativePtr);
		}
	}
}
