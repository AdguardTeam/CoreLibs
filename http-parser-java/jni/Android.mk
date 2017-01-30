LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_CPPFLAGS := -std=c++11 -fexceptions

LOCAL_MODULE := httpparser-jni

LOCAL_SRC_FILES := callbacks.cpp parser.cpp logger.cpp

LOCAL_STATIC_LIBRARIES := httpparser-c

LOCAL_LDLIBS := -llog -lz

include $(BUILD_SHARED_LIBRARY)

include $(LOCAL_PATH)/../../http-parser/Android.mk
