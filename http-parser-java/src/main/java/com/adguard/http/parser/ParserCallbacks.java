package com.adguard.http.parser;

/**
 * Created by s.fionov on 08.11.16.
 */
public interface ParserCallbacks {

	void onHttpRequestReceived(long id, HttpMessage message);

	boolean onHttpRequestBodyStarted(long id);

	void onHttpRequestBodyData(long id, byte[] data);

	void onHttpRequestBodyFinished(long id);

	void onHttpResponseReceived(long id, HttpMessage message);

	boolean onHttpResponseBodyStarted(long id);

	void onHttpResponseBodyData(long id, byte[] data);

	void onHttpResponseBodyFinished(long id);
}
