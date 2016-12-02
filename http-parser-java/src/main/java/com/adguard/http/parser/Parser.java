package com.adguard.http.parser;

import java.io.Closeable;
import java.io.IOException;
import java.nio.ByteBuffer;

/**
 * Created by s.fionov on 08.11.16.
 */
public interface Parser extends Closeable {

	Connection connect(long id, ParserCallbacks callbacks) throws IOException;

	void disconnect(Connection connection, Direction direction) throws IOException;

	void input(Connection connection, Direction direction, byte[] data) throws IOException;

	void close(Connection connection) throws IOException;

	interface Connection {
		long getId();
	}
}
