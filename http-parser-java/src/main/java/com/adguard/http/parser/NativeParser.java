package com.adguard.http.parser;

import java.io.IOException;

/**
 * Created by s.fionov on 08.11.16.
 */
public class NativeParser implements Parser {

	static {
		System.loadLibrary("httpparser-jni");
	}

	private long parserCtxPtr;

	private static native void init(NativeParser parser, long logger);

	public NativeParser(NativeLogger logger) {
		init(this, logger.nativePtr);
	}

	public static native synchronized long connect(long parserNativePtr, long id, Callbacks callbacks);

	@Override
	public NativeConnection connect(long id, ParserCallbacks callbacks) {
		return new NativeConnection(connect(parserCtxPtr, id, new Callbacks(callbacks)));
	}

	public native static synchronized void disconnect0(long connectionNativePtr, int direction) throws IOException;

	@Override
	public void disconnect(Connection connection, Direction direction) throws IOException {
		disconnect0(((NativeConnection) connection).nativePtr, direction.getCode());
	}

	public static native void input0(long connectionNativePtr, int direction, byte[] data) throws IOException;

	@Override
	public void input(Connection connection, Direction direction, byte[] data) throws IOException {
		input0(((NativeConnection) connection).nativePtr, direction.getCode(), data);
	}

	public static native void closeConnection(long connectionNativePtr) throws IOException;

	@Override
	public void close(Connection connection) throws IOException {
		closeConnection(((NativeConnection) connection).nativePtr);
	}

	public static native long getConnectionId(long nativePtr);

	public static native void closeParser(long parserNativePtr) throws IOException;

	@Override
	public void close() throws IOException {
		closeParser(parserCtxPtr);
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

		boolean onHttpRequestBodyStarted(long id) {
			return callbacks.onHttpRequestBodyStarted(id);
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

		boolean onHttpResponseBodyStarted(long id) {
			return callbacks.onHttpResponseBodyStarted(id);
		}

		// TODO: pass pointer to be able to provide byte array from the byte array pool
		void onHttpResponseBodyData(long id, byte[] data) {
			callbacks.onHttpResponseBodyData(id, data);
		}

		void onHttpResponseBodyFinished(long id) {
			callbacks.onHttpResponseBodyFinished(id);
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
