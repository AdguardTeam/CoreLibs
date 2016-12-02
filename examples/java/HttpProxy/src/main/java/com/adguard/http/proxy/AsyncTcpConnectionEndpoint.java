package com.adguard.http.proxy;

import org.apache.commons.io.IOUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.Closeable;
import java.io.IOException;
import java.lang.ref.WeakReference;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.ClosedChannelException;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.SocketChannel;
import java.util.Collection;
import java.util.LinkedList;
import java.util.Queue;
import java.util.concurrent.TimeoutException;

/**
 * Created by s.fionov on 14.11.16.
 */
class AsyncTcpConnectionEndpoint implements Closeable {

	private static final Logger log = LoggerFactory.getLogger(AsyncTcpConnectionEndpoint.class);
	private static final int MAX_OUTBOUND_CHUNKS = 3000;

	private static final Object nextIdSync = new Object();
	private static long nextId = 100000;

	private long id;
	private SelectionKey key;
	private ByteBuffer inBuffer;
	private ByteBuffer outBuffer;
	private Queue<byte[]> outboundQueue;
	private Collection<EndpointHandler> endpointHandlers = new LinkedList<>();
	private boolean closed;
	private WeakReference<Object> attachment;
	private String host;
	private int port;
	private volatile long readTimeoutMs;
	private volatile long lastRead;

	static AsyncTcpConnectionEndpoint localEndpoint(SocketChannel ch, Selector selector, String host, int port, EndpointHandler handler, long readTimeout) throws IOException {
		return new AsyncTcpConnectionEndpoint(ch, selector, host, port, handler, readTimeout);
	}

	static AsyncTcpConnectionEndpoint remoteEndpoint(Selector selector, String host, int port, EndpointHandler handler, long readTimeout) throws IOException {
		return new AsyncTcpConnectionEndpoint(null, selector, host, port, handler, readTimeout);
	}

	private AsyncTcpConnectionEndpoint(SocketChannel ch, Selector selector, String host, int port, EndpointHandler handler, long readTimeout) throws IOException {
		synchronized (nextIdSync) {
			this.id = nextId++;
		}

		this.inBuffer = ByteBuffer.allocate(AsyncTcpServer.BUFFER_SIZE);
		this.outBuffer = ByteBuffer.allocate(AsyncTcpServer.BUFFER_SIZE);
		outBuffer.clear().limit(0);
		this.outboundQueue = new LinkedList<>();

		this.closed = false;

		this.host = host;
		this.port = port;
		if (ch != null) {
			registerToSelector(ch, selector);
		} else {
			connect(selector);
		}

		this.readTimeoutMs = readTimeout * 1000;
		this.lastRead = System.currentTimeMillis();

		addHandler(handler);
	}

	@Override
	public String toString() {
		return "AsyncTcpConnectionEndpoint{" +
				"id=" + id +
				", host='" + host + '\'' +
				", port=" + port +
				'}';
	}

	private void onInput(byte[] data) {
		for (EndpointHandler handler : endpointHandlers) {
			handler.onInput(this, data);
		}
	}

	private void onDisconnect() {
		for (EndpointHandler handler : endpointHandlers) {
			handler.onDisconnect(this);
		}
	}

	private void onError(Throwable t) {
		for (EndpointHandler handler : endpointHandlers) {
			handler.onError(this, t);
		}
	}

	private void onConnect() {
		for (EndpointHandler handler : endpointHandlers) {
			handler.onConnect(this);
		}
	}

	void doRead() {
		lastRead = System.currentTimeMillis();
		try {
			ByteBuffer buf = inBuffer;
			SocketChannel ch = (SocketChannel) key.channel();
			buf.clear();
			int r = ch.read(buf);
			if (r < 0) {
				// handle EOF
				close();
				return;
			} else if (r == 0) {
				// Seems that epool() selector doesn't unmark read ops when triggered
				return;
			}
			buf.flip();
			int length = buf.remaining();
			byte[] bytes = new byte[length];
			buf.get(bytes);

			// TODO: add explicit length parameter for byte-array pools support
			log.trace("[id={}] Read {} bytes from channel {}", id, bytes.length, ch);
			onInput(bytes);
		} catch (IOException e) {
			log.error("[id={}] IO exception occurred on connection {}:{}: ", id, host, port, e.getMessage());
			close();
			onError(e);
		}
	}

	public void write(byte[] bytes) {
		synchronized (this) {
			while (!closed && outboundQueue.size() >= MAX_OUTBOUND_CHUNKS) {
				try {
					this.wait();
				} catch (InterruptedException e) {
					throw new RuntimeException(e);
				}
			}
			if (closed) {
				onError(new ClosedChannelException());
				return;
			}
			outboundQueue.add(bytes);
			log.trace("[id={}] Added {} bytes to outbound queue of endpoint {}", id, bytes.length, this);
			nextBuffer();
			selectWrite();
		}
	}

	private void nextBuffer() {
		synchronized (this) {
			if (!outBuffer.hasRemaining()) {
				byte[] data = outboundQueue.poll();
				if (data != null) {
					outBuffer.clear();
					outBuffer.put(data);
					outBuffer.flip();
				} else if (key.isValid() && closed) {
					closeSync();
				}
			}
			this.notifyAll();
		}
	}

	private synchronized void selectWrite() {
		if (key == null) {
			if (outBuffer.hasRemaining()) {
				log.warn("[id={}] Pending writes on unconnected endpoint {}", id, this);
				// onError(new ClosedChannelException());
			}
			return;
		}
		if (key.isValid()) {
			if (!outBuffer.hasRemaining()) {
				log.trace("[id={}] Unset write selection flag on {}", id, key.channel());
				key.interestOps(key.interestOps() & ~SelectionKey.OP_WRITE);
			} else {
				log.trace("[id={}] Set write selection flag on {}", id, key.channel());
				key.interestOps(key.interestOps() | SelectionKey.OP_WRITE);
			}
		}
		key.selector().wakeup();
	}

	public void doWrite() {
		synchronized (this) {
			try {
				SocketChannel ch = (SocketChannel) key.channel();
				int w = ch.write(outBuffer);
				log.trace("[id={}] Written {} bytes to channel {}", id, w, key.channel());
			} catch (IOException e) {
				log.trace("[id={}] Write error occurred on connection {}", id, this, e);
				close();
				onError(e);
				return;
			}

			nextBuffer();
			selectWrite();
		}
	}

	private void closeSync() {
		closed = true;
		IOUtils.closeQuietly(key.channel());
		key.cancel();
		onDisconnect();
	}

	public void close() {
		if (!closed && !outBuffer.hasRemaining()) {
			closeSync();
		} else {
			closed = true;
		}
	}

	public void addHandler(EndpointHandler handler) {
		endpointHandlers.add(handler);
	}

	public Object attachment() {
		return attachment.get();
	}

	public void attach(Object attachment) {
		this.attachment = new WeakReference<>(attachment);
	}

	private void registerToSelector(SocketChannel ch, Selector selector) throws IOException {
		ch.configureBlocking(false);
		this.key = ch.register(selector, SelectionKey.OP_READ);
		key.attach(this);
		log.info("[id={}] New client connection {}:{}", id, host, port);
	}

	private void connect(Selector selector) throws IOException {
		SocketChannel ch = SocketChannel.open();
		ch.configureBlocking(false);
		ch.connect(new InetSocketAddress(host, port));
		key = ch.register(selector, 0);
		key.interestOps(SelectionKey.OP_CONNECT);
		key.attach(this);
		key.selector().wakeup();
	}

	public void finishConnect() {
		// If connect failed return an error
		SocketChannel ch = (SocketChannel) key.channel();
		if (key.isValid()) {
			try {
				ch.finishConnect();
				log.info("[id={}] Connected to remote host {}:{}", id, host, port);
				key.interestOps(key.interestOps() & ~SelectionKey.OP_CONNECT);
				key.interestOps(key.interestOps() | SelectionKey.OP_READ);
				onConnect();
				// Start sending queued packets
				nextBuffer();
			} catch (IOException e) {
				onError(e);
				close();
				key.cancel();
			}
		}
	}

	public void setReadTimeout(long seconds) {
		this.readTimeoutMs = seconds * 1000;
	}

	public long timeToTimeout() {
		if (readTimeoutMs == 0) {
			return Long.MAX_VALUE;
		}
		return lastRead + readTimeoutMs - System.currentTimeMillis();
	}

	public void doTimeout() {
		onError(new TimeoutException());
		close();
	}

	public interface EndpointHandler {

		void onInput(AsyncTcpConnectionEndpoint endpoint, byte[] data);

		void onConnect(AsyncTcpConnectionEndpoint endpoint);

		void onDisconnect(AsyncTcpConnectionEndpoint endpoint);

		void onError(AsyncTcpConnectionEndpoint endpoint, Throwable t);
	}
}
