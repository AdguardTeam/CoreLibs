package com.adguard.http.proxy;

import com.adguard.http.parser.*;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.SocketChannel;
import java.nio.charset.Charset;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.logging.Logger;

/**
 * Created by s.fionov on 10.11.16.
 */
public class TestHttpClient {

	private static final Logger log = Logger.getLogger("TestHttpClient");
	private static final String TEST_URL = "http://mirror.yandex.ru/";
	private static final String TEST_HOST = "mirror.yandex.ru";
	private static final AtomicInteger requestCounter = new AtomicInteger();

	public TestHttpClient() throws IOException {
	}

	public void get() throws IOException {
		long requestId = requestCounter.incrementAndGet();
		SocketChannel ch = SocketChannel.open(new InetSocketAddress(TEST_HOST, 80));
		ByteBuffer buf = ByteBuffer.allocateDirect(2048);//32768);
		buf.put(buildRequest().getBytes());
		buf.flip();
		ch.write(buf);
		buf = ByteBuffer.allocateDirect(65536);

		Parser parser = new NativeParser(NativeLogger.open(NativeLogger.LogLevel.DEBUG));
		Parser.Connection parserConnection = parser.connect(requestId, new HttpClientCallbacks());
		while (true) {
			buf.clear();
			int r = ch.read(buf);
			if (r < 0) {
				break;
			}
			buf.flip();
			byte[] bytes = bufToBytes(buf);
			log.info("received:\n" + new String(bytes, Charset.forName("ascii")));
			r = parser.input(parserConnection, Direction.IN, bytes);
			log.info("result: " + r);
		}
	}

	private byte[] bufToBytes(ByteBuffer buf) {
		byte[] bytes = new byte[buf.remaining()];
		buf.get(bytes);
		return bytes;
	}

	private String buildRequest() {
		return "GET / HTTP/1.1\r\n" +
				"Host: mirror.yandex.ru\r\n" +
				"\r\n";
	}

	private static class HttpClientCallbacks implements ParserCallbacks {
		public void onHttpRequestReceived(long id, HttpMessage header) {
			log.info("onHttpRequestReceived");
		}

		public ContentEncoding onHttpRequestBodyStarted(long id) {
			log.info("onHttpRequestBodyStarted");
			return ContentEncoding.IDENTITY;
		}

		public void onHttpRequestBodyData(long id, byte[] data) {
			log.info("onHttpRequestBodyData");
		}

		public void onHttpRequestBodyFinished(long id) {
			log.info("onHttpRequestBodyFinished");
		}

		public void onHttpResponseReceived(long id, HttpMessage header) {
			log.info("onHttpResponseReceived:\n" + header.toString());
		}

		public ContentEncoding onHttpResponseBodyStarted(long id) {
			log.info("onHttpResponseBodyStarted");
			return ContentEncoding.IDENTITY;
		}

		public void onHttpResponseBodyData(long id, byte[] data) {
			log.info("onHttpResponseBodyData");
		}

		public void onHttpResponseBodyFinished(long id) {
			log.info("onHttpResponseBodyFinished");
		}

		@Override
		public void onParseError(long id, Direction direction, int errorType, String message) {
			log.info("onParseError");
		}
	}

	private static void sleep() {
		try {
			Thread.sleep(1000);
		} catch (InterruptedException ignored) {

		}
	}
}
