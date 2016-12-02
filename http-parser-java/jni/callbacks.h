//
// Created by sw on 02.12.16.
//

#ifndef JNI_CALLBACKS_H
#define JNI_CALLBACKS_H

#include <map>

/**
 * This class stores all callbacks to java and provides c-style callbacks for C HTTP library
 */
class Callbacks {

    static std::map<connection_context *, Callbacks *> callbacksMap;
    connection_context *connection;

public:
    JavaVM *vm;
    jobject obj;
    jmethodID HttpRequestReceivedCallback;
    jmethodID HttpRequestBodyStartedCallback;
    jmethodID HttpRequestBodyDataCallback;
    jmethodID HttpRequestBodyFinishedCallback;
    jmethodID HttpResponseReceivedCallback;
    jmethodID HttpResponseBodyStartedCallback;
    jmethodID HttpResponseBodyDataCallback;
    jmethodID HttpResponseBodyFinishedCallback;

    Callbacks(JNIEnv *env, jobject callbackObject, connection_context *context);
    ~Callbacks();

    static Callbacks *get(connection_context *context);
};

/**
 * C callbacks
 */
extern "C" {
    extern int NativeParser_HttpRequestReceived(connection_context *context, void *message);
    extern int NativeParser_HttpRequestBodyStarted(connection_context *context);
    extern void NativeParser_HttpRequestBodyData(connection_context *context, const char *data, size_t length);
    extern void NativeParser_HttpRequestBodyFinished(connection_context *context);
    extern int NativeParser_HttpResponseReceived(connection_context *context, void *message);
    extern int NativeParser_HttpResponseBodyStarted(connection_context *context);
    extern void NativeParser_HttpResponseBodyData(connection_context *context, const char *data, size_t length);
    extern void NativeParser_HttpResponseBodyFinished(connection_context *context);
};

extern "C" {
    extern parser_callbacks parserCallbacks;
}

#endif //JNI_CALLBACKS_H
