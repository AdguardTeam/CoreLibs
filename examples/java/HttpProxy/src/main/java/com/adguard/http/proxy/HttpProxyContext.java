package com.adguard.http.proxy;

import com.adguard.http.parser.ContentEncoding;
import com.adguard.http.parser.Direction;
import com.adguard.http.parser.HttpMessage;
import com.adguard.http.parser.Parser;

import java.nio.channels.Selector;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Created by s.fionov on 15.11.16.
 */
public class HttpProxyContext {

	private Parser.Connection connection;

	private AsyncTcpConnectionEndpoint localEndpoint;
	private Map<String, AsyncTcpConnectionEndpoint> remoteEndpoints = new ConcurrentHashMap<>();
	private AsyncTcpConnectionEndpoint currentRemoteEndpoint;
	private final Selector selector;

	private HttpMessage request;
	private HttpMessage response;
	private volatile boolean isChunked;
	private ContentEncoding contentEncoding = ContentEncoding.IDENTITY;
	private volatile boolean httpConnectMode = false;
	private volatile Direction currentDirection = Direction.OUT;

	public HttpProxyContext(Parser.Connection connection, Selector selector, AsyncTcpConnectionEndpoint localEndpoint) {
		this.connection = connection;
		this.selector = selector;
		this.localEndpoint = localEndpoint;
		localEndpoint.attach(this);
	}

	public Parser.Connection getConnection() {
		return connection;
	}

	public AsyncTcpConnectionEndpoint getLocalEndpoint() {
		return localEndpoint;
	}

	public boolean hasRemoteEndpoint(String endpointName) {
		return remoteEndpoints.containsKey(endpointName);
	}

	public AsyncTcpConnectionEndpoint getRemoteEndpoint(String endpointName) {
		return remoteEndpoints.get(endpointName);
	}

	public void addRemoteEndpoint(String host, int port, AsyncTcpConnectionEndpoint remoteEndpoint) {
		remoteEndpoints.put(endpointName(host, port), remoteEndpoint);
		remoteEndpoint.attach(this);
	}

	public HttpMessage getRequest() {
		return request;
	}

	public void setRequest(HttpMessage request) {
		resetRequest();
		this.request = request;
	}

	public HttpMessage getResponse() {
		return response;
	}

	public void setResponse(HttpMessage response) {
		this.response = response;
	}

	public boolean isChunked() {
		return isChunked;
	}

	public void setChunked(boolean chunked) {
		isChunked = chunked;
	}

	public ContentEncoding getContentEncoding() {
		return contentEncoding;
	}

	public void setContentEncoding(ContentEncoding contentEncoding) {
		this.contentEncoding = contentEncoding;
	}

	public void removeRemoteEndpoint(String endpointName) {
		remoteEndpoints.remove(endpointName);
	}

	public void removeRemoteEndpoint(AsyncTcpConnectionEndpoint endpoint) {
		remoteEndpoints.values().remove(endpoint);
	}

	public AsyncTcpConnectionEndpoint getCurrentRemoteEndpoint() {
		return currentRemoteEndpoint;
	}

	public void setCurrentRemoteEndpoint(String endpointName) {
		this.currentRemoteEndpoint = remoteEndpoints.get(endpointName);
	}

	public void setCurrentRemoteEndpoint(AsyncTcpConnectionEndpoint endpoint) {
		if (remoteEndpoints.values().contains(endpoint)) {
			this.currentRemoteEndpoint = endpoint;
		} else {
			throw new IllegalArgumentException("No such endpoint " + endpoint);
		}
	}

	public AsyncTcpConnectionEndpoint getOpposingEndpoint(AsyncTcpConnectionEndpoint endpoint) {
		if (endpoint.equals(localEndpoint)) {
			return getCurrentRemoteEndpoint();
		} else if (remoteEndpoints.values().contains(endpoint)) {
			return localEndpoint;
		} else {
			throw new IllegalArgumentException("No such endpoint in this connection");
		}
	}

	public boolean isHttpConnectMode() {
		return httpConnectMode;
	}

	public void setHttpConnectMode(boolean active) {
		this.httpConnectMode = active;
		updateTimeouts();
	}

	private void updateTimeouts() {
		if (localEndpoint == null) {
			return;
		}

		if (httpConnectMode) {
			localEndpoint.setReadTimeout(Constants.TUNNEL_READ_TIMEOUT_SECONDS);
			if (currentRemoteEndpoint != null) {
				currentRemoteEndpoint.setReadTimeout(Constants.TUNNEL_READ_TIMEOUT_SECONDS);
			}
		} else if (currentDirection == Direction.OUT) {
			localEndpoint.setReadTimeout(Constants.KEEP_ALIVE_INTERVAL_SECONDS);
			if (currentRemoteEndpoint != null) {
				currentRemoteEndpoint.setReadTimeout(0L);
			}
		} else {
			localEndpoint.setReadTimeout(0L);
			if (currentRemoteEndpoint != null) {
				currentRemoteEndpoint.setReadTimeout(Constants.KEEP_ALIVE_INTERVAL_SECONDS);
			}
		}
	}

	public Selector getSelector() {
		return selector;
	}

	public AsyncTcpConnectionEndpoint getRemoteEndpoint(String host, int port) {
		String endpointName = host + ":" + port;
		return getRemoteEndpoint(endpointName);
	}

	public boolean hasRemoteEndpoint(String host, int port) {
		String endpointName = host + ":" + port;
		return hasRemoteEndpoint(endpointName);
	}

	private static String endpointName(String host, int port) {
		return host + ":" + port;
	}

	private void resetRequest() {
		if (this.request != null) {
			this.request.close();
			this.request = null;
		}
	}

	private void resetResponse() {
		if (this.response != null) {
			this.response.close();
			this.response = null;
		}
	}

	public Direction getCurrentDirection() {
		return currentDirection;
	}

	public void setCurrentDirection(Direction currentDirection) {
		this.currentDirection = currentDirection;
		updateTimeouts();
	}

	public Direction getDirection(AsyncTcpConnectionEndpoint endpoint) {
		return localEndpoint.equals(endpoint) ? Direction.OUT : Direction.IN;
	}

	public void close() {
		try {
			localEndpoint.close();
			for (AsyncTcpConnectionEndpoint endpoint : remoteEndpoints.values()) {
				endpoint.close();
			}
			resetRequest();
			resetResponse();
		} finally {
			localEndpoint = null;
			remoteEndpoints.clear();
		}
	}

	public boolean isClosed() {
		return localEndpoint == null;
	}
}
