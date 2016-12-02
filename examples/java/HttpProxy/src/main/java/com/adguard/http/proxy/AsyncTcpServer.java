package com.adguard.http.proxy;

import org.apache.commons.io.IOUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.nio.channels.*;
import java.util.*;

/**
 * Asynchronous TCP server with ability to accept connections and connecting to remote hosts.
 * Supports timeouts.
 *
 * Created by s.fionov on 10.11.16.
 */
public abstract class AsyncTcpServer implements AsyncTcpConnectionEndpoint.EndpointHandler {

	private static final Logger log = LoggerFactory.getLogger(AsyncTcpServer.class);
	public static final int BUFFER_SIZE = 262144;

	private InetSocketAddress bindAddress;
	private ServerSocketChannel serverSocketChannel;
	protected Selector selector;
	private Thread worker;

	AsyncTcpServer(InetSocketAddress bindAddress) throws IOException {
		this.bindAddress = bindAddress;
	}

	public synchronized void start() throws IOException {
		if (serverSocketChannel != null) {
			throw new IOException("Proxy is already started");
		}

		serverSocketChannel = ServerSocketChannel.open();
		serverSocketChannel.bind(bindAddress);
		serverSocketChannel.configureBlocking(false);

		selector = Selector.open();
		serverSocketChannel.register(selector, SelectionKey.OP_ACCEPT);
		worker = new Thread(new Runnable(){

			@Override
			public void run() {
				try {
					Thread.currentThread().setName("http-proxy-" + bindAddress.getHostString() + ":" + bindAddress.getPort());
					AsyncTcpServer.this.selectorLoop();
				} catch (IOException e) {
					log.error("Selector loop died due to exception, this should never be happened", e);
				}
			}
		});
		worker.start();
		log.info("Started native http proxy server on {}", bindAddress);
	}

	private synchronized void stop() throws IOException {
		onServerStopped();
		worker.interrupt();
		IOUtils.closeQuietly(serverSocketChannel);
		serverSocketChannel = null;
	}

	protected abstract void onServerStopped();

	private void selectorLoop() throws IOException {
		while (true) {
			long nextTimeout = selectorCheckTimeouts();
			selector.select(nextTimeout);

			log.trace("Selected keys is {}", selector.selectedKeys());

			Iterator<SelectionKey> it = selector.selectedKeys().iterator();
			while (it.hasNext()) {
				SelectionKey key = it.next();
				it.remove();
				try {
					if (!key.isValid()) {
						continue;
					}
					if (key.isAcceptable()) {
						selectorAcceptConnection(key);
					} else if (key.isConnectable()) {
						selectorProcessConnect(key);
					} else if (key.isReadable()) {
						selectorProcessRead(key);
					} else if (key.isWritable()) {
						selectorDoPendingWrite(key);
					}
				} catch (Throwable t) {
					log.error("Error while processing selection key {}", key.channel(), t);
				}
			}
		}
	}

	private long selectorCheckTimeouts() {
		long timeout = Long.MAX_VALUE;
		for (SelectionKey key : selector.keys()) {
			if (key.attachment() instanceof AsyncTcpConnectionEndpoint) {
				long time = ((AsyncTcpConnectionEndpoint) key.attachment()).timeToTimeout();
				if (time > 0) {
					if (time < timeout) {
						timeout = time;
					}
				} else {
					try {
						((AsyncTcpConnectionEndpoint) key.attachment()).doTimeout();
					} catch (Exception e) {
						log.error("Error timing out endpoint {}", key.attachment());
						IOUtils.closeQuietly((AsyncTcpConnectionEndpoint) key.attachment());
					}
				}
			}
		}
		if (timeout == Long.MAX_VALUE) {
			return 0;
		}
		return timeout;
	}

	private void selectorAcceptConnection(SelectionKey key) throws IOException {
		SocketChannel ch = ((ServerSocketChannel) key.channel()).accept();
		createClientEndpoint(ch);
	}

	private void selectorProcessConnect(SelectionKey key) {
		AsyncTcpConnectionEndpoint endpoint = (AsyncTcpConnectionEndpoint) key.attachment();
		endpoint.finishConnect();
	}

	private void selectorDoPendingWrite(SelectionKey key) {
		log.trace("Writing bytes to {}", key.channel());
		AsyncTcpConnectionEndpoint endpoint = (AsyncTcpConnectionEndpoint) key.attachment();
		endpoint.doWrite();
	}

	private void selectorProcessRead(SelectionKey key) {
		AsyncTcpConnectionEndpoint endpoint = (AsyncTcpConnectionEndpoint) key.attachment();
		endpoint.doRead();
	}

	private AsyncTcpConnectionEndpoint createClientEndpoint(SocketChannel clientChannel) {
		try {
			InetSocketAddress address = (InetSocketAddress) clientChannel.getRemoteAddress();
			AsyncTcpConnectionEndpoint endpoint = AsyncTcpConnectionEndpoint.localEndpoint(clientChannel,
					selector, address.getHostString(), address.getPort(), this, Constants.KEEP_ALIVE_INTERVAL_SECONDS);
			onClientConnect(endpoint);
			return endpoint;
		} catch (IOException e) {
			throw new RuntimeException(e);
		}
	}

	protected abstract void onClientConnect(AsyncTcpConnectionEndpoint endpoint);

	public AsyncTcpConnectionEndpoint newRemoteConnection(String host, int port, AsyncTcpConnectionEndpoint.EndpointHandler handler) {
		try {
			return AsyncTcpConnectionEndpoint.remoteEndpoint(selector, host, port, handler, Constants.KEEP_ALIVE_INTERVAL_SECONDS);
		} catch (IOException e) {
			// Async operation, io exceptions will be caught in selector
			throw new RuntimeException(e);
		}
	}
}
