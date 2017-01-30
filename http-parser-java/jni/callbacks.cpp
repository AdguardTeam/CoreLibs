#include <jni.h>
#include "../../http-parser/src/parser.h"
#include "callbacks.h"
#include <string>
#include <stdexcept>

/**
 * Callbacks definitions
 */
parser_callbacks parserCallbacks = {
        .http_request_received = NativeParser_HttpRequestReceived,
        .http_request_body_started = NativeParser_HttpRequestBodyStarted,
        .http_request_body_data = NativeParser_HttpRequestBodyData,
        .http_request_body_finished = NativeParser_HttpRequestBodyFinished,
        .http_response_received = NativeParser_HttpResponseReceived,
        .http_response_body_started = NativeParser_HttpResponseBodyStarted,
        .http_response_body_data = NativeParser_HttpResponseBodyData,
        .http_response_body_finished = NativeParser_HttpResponseBodyFinished
};

std::map<connection_context *, Callbacks *> Callbacks::callbacksMap;

/**
 * Attaches VM to native thread and get JNIEnv
 * @param vm Java virtual machine
 * @return JNIEnv object
 */
static JNIEnv *getEnv(JavaVM *vm) {
    JNIEnv *env;
#ifdef ANDROID
    if (vm->AttachCurrentThread(&env, NULL) != JNI_OK) {
#else
    if (vm->AttachCurrentThread((void **) &env, NULL) != JNI_OK) {
#endif /* defined(ANDROID) */
        throw std::runtime_error("Can't attach to Java VM");
    }
    return env;
}

/*
 * Java callbacks. Attach VM to native thread add call Java method, then detach
 */

int NativeParser_HttpRequestReceived(connection_context *connection_ctx, void *message) {
    Callbacks* callbacks = Callbacks::get(connection_ctx);
    if (callbacks == NULL) {
        return -1;
    }

    http_message *clone = http_message_clone((const http_message *) message);
    JNIEnv *env = getEnv(callbacks->vm);
    int r = env->CallIntMethod(callbacks->obj, callbacks->HttpRequestReceivedCallback,
                               connection_get_id(connection_ctx), (jlong) clone);
    callbacks->vm->DetachCurrentThread();
    return r;
}

int NativeParser_HttpRequestBodyStarted(connection_context *connection_ctx) {
    Callbacks *callbacks = Callbacks::get(connection_ctx);
    if (callbacks == NULL) {
        return -1;
    }

    JNIEnv *env = getEnv(callbacks->vm);
    int r = env->CallIntMethod(callbacks->obj, callbacks->HttpRequestBodyStartedCallback,
                               connection_get_id(connection_ctx));
    callbacks->vm->DetachCurrentThread();
    return r;
}

void NativeParser_HttpRequestBodyData(connection_context *connection_ctx, const char *data, size_t length) {
    Callbacks *callbacks = Callbacks::get(connection_ctx);
    if (callbacks == NULL) {
        return;
    }

    JNIEnv *env = getEnv(callbacks->vm);
    jbyteArray arr = env->NewByteArray((jsize) length);
    env->SetByteArrayRegion(arr, 0, (jsize) length, (jbyte *) data);
    env->CallVoidMethod(callbacks->obj, callbacks->HttpRequestBodyDataCallback,
                        connection_get_id(connection_ctx), arr);

    // JNI will not auto clean local references since this method wasn't invoked from JVM
    env->DeleteLocalRef(arr);
    callbacks->vm->DetachCurrentThread();
}

void NativeParser_HttpRequestBodyFinished(connection_context *connection_ctx) {
    Callbacks *callbacks = Callbacks::get(connection_ctx);
    if (callbacks == NULL) {
        return;
    }

    JNIEnv *env = getEnv(callbacks->vm);
    env->CallVoidMethod(callbacks->obj, callbacks->HttpRequestBodyFinishedCallback,
                        connection_get_id(connection_ctx));
    callbacks->vm->DetachCurrentThread();
}

int NativeParser_HttpResponseReceived(connection_context *connection_ctx, void *message) {
    Callbacks *callbacks = Callbacks::get(connection_ctx);
    if (callbacks == NULL) {
        return -1;
    }

    http_message *clone = http_message_clone((const http_message *) message);

    JNIEnv *env = getEnv(callbacks->vm);
    int r = env->CallIntMethod(callbacks->obj, callbacks->HttpResponseReceivedCallback,
                               connection_get_id(connection_ctx), (jlong) clone);
    callbacks->vm->DetachCurrentThread();
    return r;
}

int NativeParser_HttpResponseBodyStarted(connection_context *connection_ctx) {
    Callbacks *callbacks = Callbacks::get(connection_ctx);
    if (callbacks == NULL) {
        return -1;
    }

    JNIEnv *env = getEnv(callbacks->vm);
    int r = env->CallIntMethod(callbacks->obj, callbacks->HttpResponseBodyStartedCallback,
                               connection_get_id(connection_ctx));
    callbacks->vm->DetachCurrentThread();
    return r;
}

void NativeParser_HttpResponseBodyData(connection_context *connection_ctx, const char *data, size_t length) {
    Callbacks *callbacks = Callbacks::get(connection_ctx);
    if (callbacks == NULL) {
        return;
    }

    JNIEnv *env = getEnv(callbacks->vm);
    jbyteArray arr = env->NewByteArray((jsize) length);
    env->SetByteArrayRegion(arr, 0, (jsize) length, (jbyte *) data);
    env->CallVoidMethod(callbacks->obj, callbacks->HttpResponseBodyDataCallback,
                        connection_get_id(connection_ctx), arr);

    // JNI will not auto clean local references since this method wasn't invoked from JVM
    env->DeleteLocalRef(arr);
    callbacks->vm->DetachCurrentThread();
}

void NativeParser_HttpResponseBodyFinished(connection_context *connection_ctx) {
    Callbacks *callbacks = Callbacks::get(connection_ctx);
    if (callbacks == NULL) {
        return;
    }

    JNIEnv *env = getEnv(callbacks->vm);
    env->CallVoidMethod(callbacks->obj, callbacks->HttpResponseBodyFinishedCallback,
                        connection_get_id(connection_ctx));
    callbacks->vm->DetachCurrentThread();
}

Callbacks::Callbacks(JNIEnv *env, jobject callbacksObject, connection_context *context) {
    jclass callbacksClass = env->FindClass("Lcom/adguard/http/parser/NativeParser$Callbacks;");
    if (callbacksClass == NULL) {
        throw std::runtime_error("Can't find class NativeParser$Callbacks");
    }

    if (env->GetJavaVM(&this->vm) != JNI_OK) {
        throw std::runtime_error("Can't get Java VM");
    }

    this->obj = env->NewGlobalRef(callbacksObject);
    this->HttpRequestReceivedCallback = env->GetMethodID(callbacksClass, "onHttpRequestReceived", "(JJ)I");
    this->HttpRequestBodyStartedCallback = env->GetMethodID(callbacksClass, "onHttpRequestBodyStarted", "(J)Z");
    this->HttpRequestBodyDataCallback = env->GetMethodID(callbacksClass, "onHttpRequestBodyData", "(J[B)V");
    this->HttpRequestBodyFinishedCallback = env->GetMethodID(callbacksClass, "onHttpRequestBodyFinished", "(J)V");
    this->HttpResponseReceivedCallback = env->GetMethodID(callbacksClass, "onHttpResponseReceived", "(JJ)I");
    this->HttpResponseBodyStartedCallback = env->GetMethodID(callbacksClass, "onHttpResponseBodyStarted", "(J)Z");
    this->HttpResponseBodyDataCallback = env->GetMethodID(callbacksClass, "onHttpResponseBodyData", "(J[B)V");
    this->HttpResponseBodyFinishedCallback = env->GetMethodID(callbacksClass, "onHttpResponseBodyFinished", "(J)V");

    this->connection = context;
    callbacksMap[context] = this;
}

Callbacks::~Callbacks() {
    getEnv(vm)->DeleteGlobalRef(obj);
    callbacksMap.erase(this->connection);
}

Callbacks *Callbacks::get(connection_context *context) {
    auto it = callbacksMap.find(context);
    if (it == callbacksMap.end()) {
        return NULL;
    }
    return it->second;
}
