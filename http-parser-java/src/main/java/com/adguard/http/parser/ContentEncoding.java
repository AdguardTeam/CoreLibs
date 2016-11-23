package com.adguard.http.parser;

import java.util.Collection;
import java.util.HashMap;
import java.util.Map;

/**
 * Created by s.fionov on 11.11.16.
 */
public enum ContentEncoding {
	IDENTITY(0, "identity"),
	DEFLATE(1, "deflate"),
	GZIP(2, "gzip", "x-gzip");

	private int code;
	private String[] names;
	private static final Map<Integer, ContentEncoding> map;
	private static final Map<String, ContentEncoding> namesMap;

	static {
		map = new HashMap<>();
		namesMap = new HashMap<>();
		for (ContentEncoding value : values()) {
			map.put(value.code, value);
		}
		for (ContentEncoding value : values()) {
			for (String name : value.names) {
				namesMap.put(name, value);
			}
		}
	}

	ContentEncoding(int code, String... names) {
		this.code = code;
		this.names = names;
	}

	public int getCode() {
		return code;
	}

	public static ContentEncoding getByCode(int code) {
		ContentEncoding value = map.get(code);
		if (value == null) {
			throw new IllegalArgumentException("Invalid " + ContentEncoding.class.getSimpleName() + " code " + code);
		}
		return value;
	}

	public static ContentEncoding getByName(String name) {
		ContentEncoding value = namesMap.get(name);
		if (value == null) {
			throw new IllegalArgumentException("Invalid " + ContentEncoding.class.getSimpleName() + " code " + name);
		}
		return value;
	}

	public static Collection<String> names() {
		return namesMap.keySet();
	}
}
