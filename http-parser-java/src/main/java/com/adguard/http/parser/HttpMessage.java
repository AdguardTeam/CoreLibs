package com.adguard.http.parser;

import java.util.Map;

/**
 * Created by s.fionov on 11.11.16.
 */
public class HttpMessage {

	private long nativePtr;

	public HttpMessage(long nativePtr) {
		this.nativePtr = nativePtr;
	}

	public HttpMessage() {
		this.nativePtr = createHttpMessage();
	}

	private static native long createHttpMessage();

	public static native void addHeader(long address, String key, String value);

	public void addHeader(String key, String value) {
		if (key == null || value == null) {
			throw new NullPointerException();
		}
		addHeader(nativePtr, key, value);
	}

	private static native long[] getHeaders(long address);

	private HttpHeaderField[] getHeaders() {
		long[] headerAddresses = getHeaders(nativePtr);
		HttpHeaderField[] headerFields = new HttpHeaderField[headerAddresses.length];
		for (int i = 0; i < headerAddresses.length; i++) {
			headerFields[i] = new HttpHeaderField(headerAddresses[i]);
		}
		return headerFields;
	}

	public String getHeader(String name) {
		// TODO: do that in native code
		for (HttpHeaderField field : getHeaders()) {
			if (field.getKey().toLowerCase().equals(name.toLowerCase())) {
				return field.getValue();
			}
		}
		return null;
	}

	private static native void removeHeader(long address, String name);

	public void removeHeader(String name) {
		removeHeader(nativePtr, name);
	}

	private static native String getMethod(long address);

	public String getMethod() {
		return getMethod(nativePtr);
	}

	private static native String getUrl(long address);

	public String getUrl() {
		return getUrl(nativePtr);
	}

	private static native void setUrl(long address, String url);

	public void setUrl(String url) {
			if (url == null) {
			throw new NullPointerException();
		}
		setUrl(nativePtr, url);
	}

	private static native String getStatus(long address);

	public String getStatus() {
		return getStatus(nativePtr);
	}

	private static native int getStatusCode(long address);

	public int getStatusCode() {
		return getStatusCode(nativePtr);
	}

	private static native int sizeBytes(long address);

	public int sizeBytes() {
		return sizeBytes(nativePtr);
	}

	private static native byte[] getBytes(long address);

	public byte[] getBytes() {
		return getBytes(nativePtr);
	}

	private static native void getBytes(long address, byte[] destination);

	public void getBytes(byte[] destination) {
		getBytes(nativePtr, destination);
	}

	@Override
	public String toString() {
		return "HttpMessage{" + new String(getBytes()) + "}";
	}

	private static native long clone(long address);

	public HttpMessage cloneHeader() {
		return new HttpMessage(clone(nativePtr));
	}

	private static native void free(long address);

	public void free() {
		free(nativePtr);
	}

	private static native void setStatusCode(long address, int code);

	public void setStatusCode(int code) {
		setStatusCode(nativePtr, code);
	}

	private static native void setStatus(long address, String status);

	public void setStatus(String status) {
		setStatus(nativePtr, status);
	}

	private static class HttpHeaderField implements Map.Entry<String, String> {
		long nativePtr;

		HttpHeaderField(long nativePtr) {
			this.nativePtr = nativePtr;
		}

		private static native String getKey(long address);

		@Override
		public String getKey() {
			return getKey(nativePtr);
		}

		private static native String getValue(long address);

		@Override
		public String getValue() {
			return getValue(nativePtr);
		}

		@Override
		public String setValue(String value) {
			throw new UnsupportedOperationException();
		}
	}
}
