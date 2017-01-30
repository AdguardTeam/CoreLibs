package com.adguard.http.parser;

import java.util.HashMap;
import java.util.Map;

/**
 * Created by s.fionov on 11.11.16.
 */
public enum Direction {
	IN(0), OUT(1);

	private int code;
	private static final Map<Integer, Direction> map;

	static {
		map = new HashMap<>();
		for (Direction value : values()) {
			map.put(value.code, value);
		}
	}

	Direction(int code) {
		this.code = code;
	}

	public int getCode() {
		return code;
	}

	public static Direction getByCode(int code) {
		Direction value = map.get(code);
		if (value == null) {
			throw new IllegalArgumentException("Invalid " + Direction.class.getSimpleName() + " code " + code);
		}
		return value;
	}
}
