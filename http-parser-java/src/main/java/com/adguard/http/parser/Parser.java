package com.adguard.http.parser;

import java.io.Closeable;
import java.nio.ByteBuffer;

/**
 * Created by s.fionov on 08.11.16.
 */
public interface Parser extends Closeable {

	// TODO: don't return int, throw an Exception
	Connection connect(long id, ParserCallbacks callbacks);

	int disconnect(Connection connection, Direction direction);

	int input(Connection connection, Direction direction, byte[] data);

	int close(Connection connection);

	interface Connection {
		long getId();
	}
}
