package com.adguard.http.parser;

import java.nio.ByteBuffer;

/**
 * Created by s.fionov on 08.11.16.
 */
public interface Parser {

	// TODO: don't return int, throw an Exception
	int connect(long id, ParserCallbacks callbacks);

	int disconnect(long id, Direction direction);

	int input(long id, Direction direction, byte[] data);

	int close(long id);
}
