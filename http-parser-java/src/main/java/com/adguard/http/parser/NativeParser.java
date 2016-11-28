package com.adguard.http.parser;

/**
 * Created by s.fionov on 08.11.16.
 */
public class NativeParser implements Parser {

	static {
		System.loadLibrary("httpparser");
	}

	// TODO: add ability to create multiple parsers (store native pointer of parser-wide context)

	public static native synchronized int connect(long id, Callbacks callbacks);

	@Override
	public int connect(long id, ParserCallbacks callbacks) {
		return connect(id, new Callbacks(callbacks));
	}

	public native static synchronized int disconnect0(long id, int direction);

	@Override
	public int disconnect(long id, Direction direction) {
		return disconnect0(id, direction.getCode());
	}

	public static native int input0(long id, int direction, byte[] data);

	@Override
	public int input(long id, Direction direction, byte[] data) {
		return input0(id, direction.getCode(), data);
	}

	@Override
	public native int close(long id);

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
}
