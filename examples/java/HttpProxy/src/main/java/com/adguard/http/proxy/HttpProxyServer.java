package com.adguard.http.proxy;

import com.adguard.http.parser.*;
import org.apache.commons.io.IOUtils;
import org.apache.commons.lang.StringUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.URI;
import java.net.URISyntaxException;
import java.nio.channels.ClosedChannelException;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.TimeoutException;
import java.util.regex.MatchResult;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Created by s.fionov on 23.11.16.
 */
public class HttpProxyServer extends AsyncTcpServer implements ParserCallbacks {

	private static final Logger log = LoggerFactory.getLogger(HttpProxyServer.class);
	private static final Logger parserLog = LoggerFactory.getLogger("NativeParser");

	private static final byte[] EMPTY_CHUNK_DATA = new byte[0];
	private static final String HTTP_METHOD_CONNECT = "CONNECT";

	private Map<Long, HttpProxyContext> contexts = new ConcurrentHashMap<>();
	private Parser parser;

	private final Object nextConnectionIdSync = new Object();
	private long nextConnectionId = 0;

	HttpProxyServer(InetSocketAddress bindAddress) throws IOException {
		super(bindAddress);
		this.parser = new NativeParser(NativeLogger.open(NativeLogger.LogLevel.TRACE, new LoggerCallback()));
	}

	@Override
	public void onInput(AsyncTcpConnectionEndpoint endpoint, byte[] bytes) {
		HttpProxyContext context = getContext(endpoint);
		if (!context.isHttpConnectMode()) {
			try {
				parser.input(context.getConnection(), context.getDirection(endpoint), bytes);
			} catch (Exception e) {
				onError(endpoint, e);
				processError(context, context.getDirection(endpoint), e.getMessage());
			}
		} else {
			AsyncTcpConnectionEndpoint anotherEndpoint = context.getOpposingEndpoint(endpoint);
			anotherEndpoint.write(bytes);
		}
	}

	@Override
	protected void onClientConnect(AsyncTcpConnectionEndpoint endpoint) {
		long connectionId;
		synchronized (nextConnectionIdSync) {
			connectionId = nextConnectionId++;
		}

		Parser.Connection connection = null;
		try {
			connection = parser.connect(connectionId, this);
		} catch (IOException e) {
			endpoint.close();
			return;
		}

		HttpProxyContext context = new HttpProxyContext(connection, selector, endpoint);
		contexts.put(connectionId, context);
	}

	@Override
	public void onHttpRequestReceived(long id, HttpMessage message) {
		if (message.getMethod().equals(HTTP_METHOD_CONNECT)) {
			onHttpConnectReceived(id, message);
			return;
		}

		// Start remote connection
		String host = stripHostFromRequestUrl(message);
		if (host == null) {
			host = message.getHeader("Host");
		}

		HttpProxyContext context = contexts.get(id);
		if (host == null) {
			sendBadRequestResponseAndClose(context.getLocalEndpoint());
			return;
		}

		context.setRequest(message);
		String transferEncoding = message.getHeader("Transfer-Encoding");
		context.setChunked(Objects.equals(transferEncoding, "chunked"));
		removeUnsupportedFeatures(message);

		AsyncTcpConnectionEndpoint remoteEndpoint = setRemoteEndpoint(id, host);
		sendRequest(remoteEndpoint, message);
		if (!context.isChunked()) {
			context.setCurrentDirection(Direction.IN);
		}
	}

	private void removeUnsupportedFeatures(HttpMessage header) {
		// HTTP 2.0 upgrade is unsupported
		header.removeHeader("Upgrade");
		// sdch Content-Encoding is unsupported
		String acceptEncoding = header.getHeader("Accept-Encoding");
		if (acceptEncoding != null) {
			String[] encodings = acceptEncoding.split(",\\s+");
			Collection<String> allowedEncodings = ContentEncoding.names();
			Pattern pattern = Pattern.compile("([a-zA-Z]+|\\*)\\s*(;\\s*q=[0-9.]+)?");
			List<String> filteredEncodings = new ArrayList<>();
			for (String encoding : encodings) {
				Matcher matcher = pattern.matcher(encoding);
				if (matcher.matches()) {
					MatchResult match = matcher.toMatchResult();
					String encodingName = match.group(1);
					if (encodingName != null && (encodingName.equals("*") || allowedEncodings.contains(encodingName.toLowerCase()))) {
						filteredEncodings.add(match.group(0));
					}
				}
			}

			header.addHeader("Accept-Encoding", StringUtils.join(filteredEncodings, ","));
		}
	}

	private void onHttpConnectReceived(long id, HttpMessage header) {
		HttpProxyContext context = contexts.get(id);
		setRemoteEndpoint(id, header.getUrl());
		context.setCurrentDirection(Direction.IN);
		context.setHttpConnectMode(true);
	}

	private void sendResponse(AsyncTcpConnectionEndpoint endpoint, HttpMessage message, byte[] responseBody) {
		assert getContext(endpoint).getLocalEndpoint().equals(endpoint);
		if (responseBody != null) {
			message.addHeader("Content-Length", Integer.toString(responseBody.length));
		}
		endpoint.write(message.getBytes());
		if (responseBody != null) {
			endpoint.write(responseBody);
		}
		getContext(endpoint).setCurrentDirection(Direction.OUT);
	}

	private HttpMessage createErrorResponse(int statusCode, String status, Throwable t) {
		HttpMessage message = HttpMessage.create();
		String content = "Error occurred while connecting to the remote host" + (t == null ? "\n" : ": " + t.getMessage() + "\n");
		byte[] contentBytes = content.getBytes();
		message.setStatusCode(statusCode);
		message.setStatus(status);
		message.addHeader("Content-Type", "text/plain");
		message.addHeader("Content-Length", Integer.toString(contentBytes.length));
		message.addHeader("Keep-Alive", "timeout=" + Constants.KEEP_ALIVE_INTERVAL_SECONDS);
		return message;
	}

	/**
	 * Response to HTTP CONNECT request
	 */
	private HttpMessage createHttpConnectionEstablishedResponse() {
		HttpMessage message = HttpMessage.create();
		message.setStatusCode(200);
		message.setStatus("Connection established");
		message.addHeader("Connection", "close");
		return message;
	}

	private HttpMessage createGatewayTimeoutResponse() {
		return createErrorResponse(504, "Gateway timeout", null);
	}

	/**
	 * Response to client with 503 status (Remote host has sent invalid response)
	 * @param endpoint Client endpoint
	 */
	private HttpMessage createRemoteHostInvalidResponseResponse(AsyncTcpConnectionEndpoint endpoint) {
		return createErrorResponse(503, "Invalid response", null);
	}

	/**
	 * Response to client with 400 status (Bad request)
	 * @param endpoint Client endpoint
	 */
	private void sendBadRequestResponseAndClose(AsyncTcpConnectionEndpoint endpoint) {
		try (HttpMessage response = HttpMessage.create()) {
			response.setStatusCode(400);
			response.setStatus("Bad request");
			response.addHeader("Content-Length", "0");
			response.addHeader("Connection", "close");
			endpoint.write(response.getBytes());
		} finally {
			endpoint.close();
		}
	}

	@Override
	public boolean onHttpRequestBodyStarted(long id) {
		return false;
	}

	@Override
	public void onHttpRequestBodyData(long id, byte[] data) {
		HttpProxyContext context = contexts.get(id);
		redirectData(context, context.getCurrentRemoteEndpoint(), data);
	}

	@Override
	public void onHttpRequestBodyFinished(long id) {
		HttpProxyContext context = contexts.get(id);
		writeLastChunk(context, context.getCurrentRemoteEndpoint());
		context.setCurrentDirection(Direction.IN);
	}

	@Override
	public void onHttpResponseReceived(long id, HttpMessage message) {
		log.trace(">>> {}", message);
		HttpProxyContext context = contexts.get(id);
		String transferEncoding = message.getHeader("Transfer-Encoding");
		context.setChunked(Objects.equals(transferEncoding, "chunked"));
		String contentEncodingName = message.getHeader("Content-Encoding");
		if (contentEncodingName != null) {
			context.setContentEncoding(ContentEncoding.getByName(contentEncodingName));
		} else {
			context.setContentEncoding(ContentEncoding.IDENTITY);
		}

		HttpMessage responseHeader = message.clone();
		responseHeader.removeHeader("Content-Encoding");
		if (contentEncodingName != null) {
			responseHeader.addHeader("Orig-Content-Encoding", contentEncodingName);
			responseHeader.removeHeader("Content-Length");
			responseHeader.addHeader("Transfer-Encoding", "chunked");
			context.setChunked(true);
		}
		context.setResponse(responseHeader);
		log.trace("<<< {}", responseHeader.toString());
		byte[] responseBytes = responseHeader.getBytes();
		contexts.get(id).getLocalEndpoint().write(responseBytes);
	}

	@Override
	public boolean onHttpResponseBodyStarted(long id) {
		HttpProxyContext context = contexts.get(id);
		return context.getContentEncoding() != ContentEncoding.IDENTITY;
	}

	@Override
	public void onHttpResponseBodyData(long id, byte[] data) {
		HttpProxyContext context = contexts.get(id);
		redirectData(context, context.getLocalEndpoint(), data);
	}

	@Override
	public void onHttpResponseBodyFinished(long id) {
		HttpProxyContext context = contexts.get(id);
		writeLastChunk(context, context.getLocalEndpoint());
	}

	private void processError(HttpProxyContext context, Direction direction, String message) {
		if (direction == Direction.OUT) {
			sendBadRequestResponseAndClose(context.getLocalEndpoint());
		} else {
			context.getCurrentRemoteEndpoint().close();
			try (HttpMessage response = createRemoteHostInvalidResponseResponse(context.getLocalEndpoint())) {
				sendResponse(context.getLocalEndpoint(), response, null);
			}
		}
	}

	private void redirectData(HttpProxyContext context, AsyncTcpConnectionEndpoint destination, byte[] data) {
		try {
			if (context.isChunked()) {
				writeChunk(destination, data);
			} else {
				destination.write(data);
			}
		} catch (ClosedChannelException ignored) {

		}
	}

	private void writeLastChunk(HttpProxyContext context, AsyncTcpConnectionEndpoint destination) {
		try {
			if (context.isChunked()) {
				writeChunk(destination, EMPTY_CHUNK_DATA);
			}
		} catch (ClosedChannelException ignored) {

		}
	}

	private void sendRequest(AsyncTcpConnectionEndpoint endpoint, HttpMessage request) {
		HttpProxyContext context = getContext(endpoint);
		assert context.getCurrentRemoteEndpoint().equals(endpoint);
		byte[] requestBytes = request.getBytes();
		endpoint.write(requestBytes);
	}

	private void writeChunk(AsyncTcpConnectionEndpoint endpoint, byte[] data) throws ClosedChannelException {
		String chunkHeader = (Integer.toHexString(data.length).toUpperCase() + "\r\n");
		String chunkFooter = "\r\n";
		endpoint.write(chunkHeader.getBytes());
		if (data.length > 0) {
			endpoint.write(data);
		}
		endpoint.write(chunkFooter.getBytes());
	}

	static String stripHostFromRequestUrl(HttpMessage header) {
		// TODO: do it in native code
		try {
			String s = header.getUrl();
			URI uri = new URI(s);
			if (uri.getPath() != null) {
				header.setUrl(uri.getRawPath() + (uri.getRawQuery() != null ? "?" + uri.getRawQuery() : ""));
			}
			return uri.getHost();
		} catch (URISyntaxException e) {
			return null;
		}
	}

	@Override
	public void onConnect(AsyncTcpConnectionEndpoint endpoint) {
		if (getContext(endpoint).isHttpConnectMode()) {
			endpoint.setReadTimeout(Constants.TUNNEL_READ_TIMEOUT_SECONDS);
			try (HttpMessage message = createHttpConnectionEstablishedResponse()) {
				sendResponse(getContext(endpoint).getOpposingEndpoint(endpoint), message, null);
			}
		}
	}

	@Override
	public void onDisconnect(AsyncTcpConnectionEndpoint endpoint) {
		log.debug("Endpoint {} disconnected", endpoint);
		HttpProxyContext context = getContext(endpoint);
		if (!context.isClosed()) {
			Direction direction = context.getDirection(endpoint);
			try {
				parser.disconnect(context.getConnection(), direction);
			} catch (IOException e) {
				log.warn("Error disconnecting connection {} in HTTP parser");
			}

			if (direction == Direction.OUT || context.isHttpConnectMode()) {
				getContext(endpoint).close();
			} else {
				getContext(endpoint).removeRemoteEndpoint(endpoint);
			}
		} else {
			try {
				parser.close(context.getConnection());
			} catch (IOException e) {
				log.warn("Error closing connection {} in HTTP parser");
			}
		}
	}


	@Override
	public void onError(AsyncTcpConnectionEndpoint endpoint, Throwable t) {
		if (log.isDebugEnabled()) {
			log.info("Error on endpoint {}", endpoint, t);
		} else {
			log.info("Error on endpoint {}: {}", endpoint, t);
		}

		HttpProxyContext context = getContext(endpoint);
		if (context.getCurrentDirection() == Direction.IN) {
			AsyncTcpConnectionEndpoint anotherEndpoint = context.getOpposingEndpoint(endpoint);
			if (anotherEndpoint != null && anotherEndpoint.equals(context.getLocalEndpoint())) {
				if (t instanceof TimeoutException) {
					try (HttpMessage response = createGatewayTimeoutResponse()) {
						sendResponse(anotherEndpoint, response, null);
					}
				} else if (context.isHttpConnectMode()) {
					try (HttpMessage response = createErrorResponse(503, "Connection failed", t)) {
						sendResponse(anotherEndpoint, response, null);
					}
				}
			}
		}

		if (getContext(endpoint).isHttpConnectMode()) {
			getContext(endpoint).setHttpConnectMode(false);
		}

		context.setCurrentDirection(Direction.OUT);
	}

	private AsyncTcpConnectionEndpoint setRemoteEndpoint(long connectionId, String host) {
		HttpProxyContext context = contexts.get(connectionId);
		String[] parts = host.split(":");
		host = parts[0];
		int port = 80;
		if (parts.length == 2) {
			port = Integer.valueOf(parts[1]);
		}

		AsyncTcpConnectionEndpoint endpoint;
		if (!context.hasRemoteEndpoint(host, port)) {
			endpoint = newRemoteConnection(host, port, this);
			context.addRemoteEndpoint(host, port, endpoint);
		} else {
			endpoint = context.getRemoteEndpoint(host, port);
		}

		context.setCurrentRemoteEndpoint(endpoint);
		return endpoint;
	}

	private static HttpProxyContext getContext(AsyncTcpConnectionEndpoint endpoint) {
		return (HttpProxyContext) endpoint.attachment();
	}

	@Override
	protected void onServerStopped() {
		for (HttpProxyContext context : contexts.values()) {
			// TODO: wait for queue flushing
			context.close();
		}
	}

	static class LoggerCallback implements NativeLogger.Callback {
		@Override
		public void log(NativeLogger.LogLevel logLevel, String threadInfo, String message) {
			message = "" + threadInfo + " " + message;
			switch (logLevel) {
				case ERROR:
					parserLog.error(message);
					break;
				case WARN:
					parserLog.warn(message);
					break;
				case INFO:
					parserLog.info(message);
					break;
				case DEBUG:
					parserLog.debug(message);
					break;
				case TRACE:
					parserLog.trace(message);
					break;
			}
		}
	}
}
