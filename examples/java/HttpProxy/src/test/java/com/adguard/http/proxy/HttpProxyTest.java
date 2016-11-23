package com.adguard.http.proxy;

import org.junit.Test;

import java.io.IOException;
import java.net.InetSocketAddress;

/**
 * Created by s.fionov on 14.11.16.
 */
public class HttpProxyTest {
	@Test
	public void testHttpProxy() throws IOException, InterruptedException {
		AsyncTcpServer proxy = new HttpProxyServer(new InetSocketAddress(3129));
		proxy.start();
		while (true) {
			Thread.sleep(10000);
		}
	}
}
