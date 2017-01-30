LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_CFLAGS := -std=c99

LOCAL_MODULE := httpparser-c

LOCAL_SRC_FILES := src/parser.c src/logger.c src/nodejs_http_parser/http_parser.c

include $(BUILD_STATIC_LIBRARY)
