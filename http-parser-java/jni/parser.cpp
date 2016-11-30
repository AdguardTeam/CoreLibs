#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <string.h>
#include "com_adguard_http_parser_NativeParser.h"
#include "com_adguard_http_parser_HttpMessage.h"
#include "com_adguard_http_parser_HttpMessage_HttpHeaderField.h"
#include "../../http-parser/src/parser.h"

typedef struct {
    JNIEnv *env;
    jobject callbacks;
} ParserContext;

struct connection_context {
    connection_id_t id;
};

static int NativeParser_HttpRequestReceived(connection_context *context, void *header);
static int NativeParser_HttpRequestBodyStarted(connection_context *context);
static void NativeParser_HttpRequestBodyData(connection_context *context, const char *data, size_t length);
static void NativeParser_HttpRequestBodyFinished(connection_context *context);
static int NativeParser_HttpResponseReceived(connection_context *context, void *message);
static int NativeParser_HttpResponseBodyStarted(connection_context *context);
static void NativeParser_HttpResponseBodyData(connection_context *context, const char *data, size_t length);
static void NativeParser_HttpResponseBodyFinished(connection_context *context);
static void processError(JNIEnv *env, int return_code, char message[256]);

static parser_callbacks contextCallbacks = {
        .http_request_received = NativeParser_HttpRequestReceived,
        .http_request_body_started = NativeParser_HttpRequestBodyStarted,
        .http_request_body_data = NativeParser_HttpRequestBodyData,
        .http_request_body_finished = NativeParser_HttpRequestBodyFinished,
        .http_response_received = NativeParser_HttpResponseReceived,
        .http_response_body_started = NativeParser_HttpResponseBodyStarted,
        .http_response_body_data = NativeParser_HttpResponseBodyData,
        .http_response_body_finished = NativeParser_HttpResponseBodyFinished
};

class Callbacks {

public:
    jmethodID HttpRequestReceivedCallback;
    jmethodID HttpRequestBodyStartedCallback;
    jmethodID HttpRequestBodyDataCallback;
    jmethodID HttpRequestBodyFinishedCallback;
    jmethodID HttpResponseReceivedCallback;
    jmethodID HttpResponseBodyStartedCallback;
    jmethodID HttpResponseBodyDataCallback;
    jmethodID HttpResponseBodyFinishedCallback;

    Callbacks(JNIEnv *env);
};
static Callbacks *javaCallbacks = NULL;

static std::map<jlong, ParserContext *> context_map;

/**
 * Invoke parser_connect(), process errors and return the native pointer to connection context
 * @param env JNI env
 * @param cls NativeParser class
 * @param parserNativePtr Pointer to parser context
 * @param id Connection id
 * @param callbacks NativeParser$Callbacks object
 * @return Pointer to connection context
 */
jlong Java_com_adguard_http_parser_NativeParser_connect(JNIEnv *env, jclass cls, jlong parserNativePtr, jlong id,
                                                      jobject callbacks) {
    ParserContext *context = context_map[id];
    if (context == NULL) {
        context = context_map[id] = new ParserContext;
    }

    context->env = env;
    context->callbacks = env->NewGlobalRef(callbacks);

    connection_info *info = new connection_info;
    snprintf(info->endpoint_1, ENDPOINT_MAXLEN, "%s", "something1");
    snprintf(info->endpoint_2, ENDPOINT_MAXLEN, "%s", "something2");

    if (javaCallbacks == NULL) {
        javaCallbacks = new Callbacks(env);
    }

    connection_context *connection_ctx;
    parser_context *parser_ctx = (parser_context *) parserNativePtr;
    int r = parser_connect(parser_ctx, id, &contextCallbacks, &connection_ctx);
    if (r) {
        char message[256];
        snprintf(message, 256, "parser_connect() returned %d", r);
        processError(env, r, message);
    }
    return (jlong) connection_ctx;
}

static int NativeParser_HttpRequestReceived(connection_context *connection_ctx, void *message) {
    ParserContext *context = context_map[connection_ctx->id];
    if (context == NULL) {
        return -1;
    }

    http_message *clone = http_message_clone((const http_message *) message);
    return context->env->CallIntMethod(context->callbacks, javaCallbacks->HttpRequestReceivedCallback, connection_ctx->id, (jlong) clone);
}

static int NativeParser_HttpRequestBodyStarted(connection_context *connection_ctx) {
    ParserContext *context = context_map[connection_ctx->id];
    if (context == NULL) {
        return -1;
    }

    return context->env->CallIntMethod(context->callbacks, javaCallbacks->HttpRequestBodyStartedCallback, connection_ctx->id);
}

static void NativeParser_HttpRequestBodyData(connection_context *connection_ctx, const char *data, size_t length) {
    ParserContext *context = context_map[connection_ctx->id];
    if (context == NULL) {
        return;
    }

    jbyteArray arr = context->env->NewByteArray((jsize) length);
    context->env->SetByteArrayRegion(arr, 0, (jsize) length, (jbyte *) data);
    context->env->CallVoidMethod(context->callbacks, javaCallbacks->HttpRequestBodyDataCallback, connection_ctx->id, arr);

    // JNI will not auto clean local references since this method wasn't invoked from JVM
    context->env->DeleteLocalRef(arr);
}

static void NativeParser_HttpRequestBodyFinished(connection_context *connection_ctx) {
    ParserContext *context = context_map[connection_ctx->id];
    if (context == NULL) {
        return;
    }

    context->env->CallVoidMethod(context->callbacks, javaCallbacks->HttpRequestBodyFinishedCallback, connection_ctx->id);
}

static int NativeParser_HttpResponseReceived(connection_context *connection_ctx, void *message) {
    ParserContext *context = context_map[connection_ctx->id];
    if (context == NULL) {
        return -1;
    }

    http_message *clone = http_message_clone((const http_message *) message);
    return context->env->CallIntMethod(context->callbacks, javaCallbacks->HttpResponseReceivedCallback, connection_ctx->id, (jlong) clone);
}

static int NativeParser_HttpResponseBodyStarted(connection_context *connection_ctx) {
    ParserContext *context = context_map[connection_ctx->id];
    if (context == NULL) {
        return -1;
    }

    return context->env->CallIntMethod(context->callbacks, javaCallbacks->HttpResponseBodyStartedCallback, connection_ctx->id);
}

static void NativeParser_HttpResponseBodyData(connection_context *connection_ctx, const char *data, size_t length) {
    ParserContext *context = context_map[connection_ctx->id];
    if (context == NULL) {
        return;
    }

    jbyteArray arr = context->env->NewByteArray((jsize) length);
    context->env->SetByteArrayRegion(arr, 0, (jsize) length, (jbyte *) data);
    context->env->CallVoidMethod(context->callbacks, javaCallbacks->HttpResponseBodyDataCallback, connection_ctx->id, arr);

    // JNI will not auto clean local references since this method wasn't invoked from JVM
    context->env->DeleteLocalRef(arr);
}

static void NativeParser_HttpResponseBodyFinished(connection_context *connection_ctx) {
    ParserContext *context = context_map[connection_ctx->id];
    if (context == NULL) {
        return;
    }

    context->env->CallVoidMethod(context->callbacks, javaCallbacks->HttpResponseBodyFinishedCallback, connection_ctx->id);
}

Callbacks::Callbacks(JNIEnv *env) {
    jclass callbacksClass = env->FindClass("Lcom/adguard/http/parser/NativeParser$Callbacks;");
    if (callbacksClass == NULL) {
        throw "Can't find class NativeParser$Callbacks";
    }
    HttpRequestReceivedCallback = env->GetMethodID(callbacksClass, "onHttpRequestReceived", "(JJ)I");
    HttpRequestBodyStartedCallback = env->GetMethodID(callbacksClass, "onHttpRequestBodyStarted", "(J)I");
    HttpRequestBodyDataCallback = env->GetMethodID(callbacksClass, "onHttpRequestBodyData", "(J[B)V");
    HttpRequestBodyFinishedCallback = env->GetMethodID(callbacksClass, "onHttpRequestBodyFinished", "(J)V");
    HttpResponseReceivedCallback = env->GetMethodID(callbacksClass, "onHttpResponseReceived", "(JJ)I");
    HttpResponseBodyStartedCallback = env->GetMethodID(callbacksClass, "onHttpResponseBodyStarted", "(J)I");
    HttpResponseBodyDataCallback = env->GetMethodID(callbacksClass, "onHttpResponseBodyData", "(J[B)V");
    HttpResponseBodyFinishedCallback = env->GetMethodID(callbacksClass, "onHttpResponseBodyFinished", "(J)V");
}

jint Java_com_adguard_http_parser_NativeParser_disconnect0(JNIEnv *env, jclass cls, jlong connectionPtr,
                                                          jint direction) {
    connection_context *context = (connection_context *) connectionPtr;
    parser_disconnect(context, (transfer_direction_t) direction);
    if (direction == DIRECTION_OUT) {
        // delete context;
    }
    return 0;
}

jint Java_com_adguard_http_parser_NativeParser_input0(JNIEnv *env, jclass cls, jlong connectionPtr, jint direction,
                                                     jbyteArray bytes) {
    connection_context *context = (connection_context *) connectionPtr;
    jbyte *data = env->GetByteArrayElements(bytes, NULL);
    int len = env->GetArrayLength(bytes);
    int r = parser_input(context, (transfer_direction_t) direction, (const char *) data, len);
    env->ReleaseByteArrayElements(bytes, data, JNI_ABORT);
    return r;
}

jint Java_com_adguard_http_parser_NativeParser_closeConnection(JNIEnv *env, jclass cls, jlong connectionPtr) {
    connection_context *context = (connection_context *) connectionPtr;
    return parser_connection_close(context);
}

jlong Java_com_adguard_http_parser_NativeParser_getConnectionId(JNIEnv *env, jclass cls, jlong connectionPtr) {
    connection_context *context = (connection_context *) connectionPtr;
    return (jlong) context->id;
}

void Java_com_adguard_http_parser_NativeParser_init(JNIEnv *env, jclass cls, jobject parser, jlong loggerPtr) {
    fprintf(stderr, "NativeParser.init()\n");

    jfieldID parserCtxPtr = env->GetFieldID(cls, "parserCtxPtr", "J");
    if (parserCtxPtr == NULL) {
        return;
    }

    logger *log = (logger *) loggerPtr;
    parser_context *parser_ctx;
    int r = parser_create(log, &parser_ctx);
    if (r == 0) {
        env->SetLongField(parser, parserCtxPtr, (jlong) parser_ctx);
    } else {
        env->ThrowNew(env->FindClass("java/io/Exception"), "NativeParser()");
    }
}

void Java_com_adguard_http_parser_NativeParser_closeParser(JNIEnv *env, jclass cls, jlong parserPtr) {
    parser_context *parser_ctx = (parser_context *) parserPtr;
    int r = parser_destroy(parser_ctx);
    if (r != 0) {
        env->ThrowNew(env->FindClass("java/io/Exception"), "NativeParser.close()");
    }
}

jstring Java_com_adguard_http_parser_HttpMessage_getUrl(JNIEnv * env, jclass cls, jlong nativePtr) {
    http_message *header = (http_message *) nativePtr;
    return env->NewStringUTF(header->url);
}

void Java_com_adguard_http_parser_HttpMessage_setUrl(JNIEnv *env, jclass cls, jlong nativePtr, jstring value) {
    http_message *header = (http_message *) nativePtr;
    jboolean isCopy;
    const char *chars = env->GetStringUTFChars(value, &isCopy);
    size_t len = strlen(chars);
    http_message_set_url(header, chars, len);
    if (isCopy) {
        env->ReleaseStringUTFChars(value, chars);
    }
}

jstring Java_com_adguard_http_parser_HttpMessage_getStatus(JNIEnv *env, jclass cls, jlong nativePtr) {
    http_message *header = (http_message *) nativePtr;
    return env->NewStringUTF(header->status);
}

jint Java_com_adguard_http_parser_HttpMessage_getStatusCode(JNIEnv *env, jclass cls, jlong nativePtr) {
    http_message *header = (http_message *) nativePtr;
    return header->status_code;
}

jstring Java_com_adguard_http_parser_HttpMessage_getMethod(JNIEnv *env, jclass cls, jlong nativePtr) {
    http_message *header = (http_message *) nativePtr;
    return env->NewStringUTF(header->method);
}

jlongArray Java_com_adguard_http_parser_HttpMessage_getHeaders(JNIEnv *env, jclass cls, jlong nativePtr) {
    http_message *header = (http_message *) nativePtr;
    jlong nativePtrs[header->field_count];
    http_header_field *parameter = header->fields;
    for (size_t i = 0; i < header->field_count; i++) {
        nativePtrs[i] = (jlong) parameter;
        ++parameter;
    }
    jlongArray array = env->NewLongArray(header->field_count);
    env->SetLongArrayRegion(array, 0, header->field_count, nativePtrs);
    return array;
}

void Java_com_adguard_http_parser_HttpMessage_addHeader(JNIEnv *env, jclass cls, jlong nativePtr, jstring fieldName, jstring value) {
    jboolean fieldCharsIsCopy;
    const char *fieldChars = env->GetStringUTFChars(fieldName, &fieldCharsIsCopy);
    jboolean valueCharsIsCopy;
    const char *valueChars = env->GetStringUTFChars(value, &valueCharsIsCopy);
    http_message *header = (http_message *) nativePtr;
    http_message_add_header_field(header, fieldChars, strlen(fieldChars));
    http_message_set_header_field(header, fieldChars, strlen(fieldChars), valueChars, strlen(valueChars));
    if (fieldCharsIsCopy) {
        env->ReleaseStringUTFChars(fieldName, fieldChars);
    }
    if (valueCharsIsCopy) {
        env->ReleaseStringUTFChars(fieldName, valueChars);
    }
}

jint Java_com_adguard_http_parser_HttpMessage_sizeBytes(JNIEnv *env, jclass cls, jlong nativePtr) {
    http_message *header = (http_message *) nativePtr;
    size_t length = 0;
    char *message_raw = http_message_raw(header, &length);

    free(message_raw);
    return (jint) length;
}

jbyteArray Java_com_adguard_http_parser_HttpMessage_getBytes__J(JNIEnv *env, jclass cls, jlong nativePtr) {
    http_message *header = (http_message *) nativePtr;
    size_t length = 0;
    char *message_raw = http_message_raw(header, &length);

    jbyteArray arr = env->NewByteArray((jsize) length);
    env->SetByteArrayRegion(arr, 0, (jsize) length, (jbyte *) message_raw);
    free(message_raw);
    return arr;
}

void Java_com_adguard_http_parser_HttpMessage_getBytes__J_3B(JNIEnv *env, jclass cls, jlong nativePtr, jbyteArray arr) {
    http_message *header = (http_message *) nativePtr;
    size_t length = 0;
    char *message_raw = http_message_raw(header, &length);

    env->SetByteArrayRegion(arr, 0, (jsize) length, (jbyte *) message_raw);
    free(message_raw);
}

void Java_com_adguard_http_parser_HttpMessage_removeHeader(JNIEnv *env, jclass cls, jlong nativePtr, jstring fieldName) {
    http_message *header = (http_message *) nativePtr;
    jboolean fieldCharsIsCopy;
    const char *fieldChars = env->GetStringUTFChars(fieldName, &fieldCharsIsCopy);
    http_message_del_header_field(header, fieldChars, strlen(fieldChars));
    if (fieldCharsIsCopy) {
        env->ReleaseStringUTFChars(fieldName, fieldChars);
    }
}

jlong Java_com_adguard_http_parser_HttpMessage_clone(JNIEnv *env, jclass cls, jlong nativePtr) {
    if (nativePtr == 0)
        return 0;
    return (jlong) http_message_clone((http_message *) nativePtr);
}

jlong Java_com_adguard_http_parser_HttpMessage_createHttpMessage(JNIEnv *env, jclass cls) {
    http_message *message = (http_message *) malloc(sizeof(http_message));
    memset(message, 0, sizeof(http_message));
    return (jlong) message;
}

void Java_com_adguard_http_parser_HttpMessage_setStatusCode(JNIEnv *env, jclass cls, jlong nativePtr, jint code) {
    http_message *message = (http_message *) nativePtr;
    http_message_set_status_code(message, code);
}

void Java_com_adguard_http_parser_HttpMessage_setStatus(JNIEnv *env, jclass cls, jlong nativePtr, jstring status) {
    http_message *message = (http_message *) nativePtr;
    jboolean isCopy;
    const char *statusText = env->GetStringUTFChars(status, &isCopy);
    http_message_set_status(message, statusText, strlen(statusText));
    if (isCopy) {
        env->ReleaseStringUTFChars(status, statusText);
    }
}

void Java_com_adguard_http_parser_HttpMessage_free(JNIEnv *env, jclass cls, jlong nativePtr) {
    http_message *message = (http_message *) nativePtr;
    http_message_free(message);
}

jstring Java_com_adguard_http_parser_HttpMessage_00024HttpHeaderField_getKey(JNIEnv *env, jclass cls, jlong nativePtr) {
    http_header_field *parameter = (http_header_field *) nativePtr;
    return env->NewStringUTF(parameter->name);
}

jstring Java_com_adguard_http_parser_HttpMessage_00024HttpHeaderField_getValue(JNIEnv *env, jclass cls, jlong nativePtr) {
    http_header_field *parameter = (http_header_field *) nativePtr;
    return env->NewStringUTF(parameter->value);
}

/**
 * Throws exception corresponding to error code
 * @param env JNI env
 * @param return_code Error code
 * @param message Error message
 */
static void processError(JNIEnv *env, int return_code, char *message) {
    char final_message[256];
    switch ((error_type_t) return_code) {
        case PARSER_NULL_POINTER_ERROR:
            env->ThrowNew(env->FindClass("java/lang/NullPointerException"), message);
            break;
        case PARSER_HTTP_PARSE_ERROR:
            snprintf(final_message, 256, "HTTP parse error: %s", message);
            env->ThrowNew(env->FindClass("java/io/IOException"), message);
            break;
        case PARSER_ZLIB_ERROR:
            snprintf(final_message, 256, "Zlib error: %s", message);
            env->ThrowNew(env->FindClass("java/io/IOException"), message);
            break;
        case PARSER_INVALID_ARGUMENT_ERROR:
            env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), message);
            break;
        case PARSER_ALREADY_CONNECTED_ERROR:
            env->ThrowNew(env->FindClass("java/io/IOException"), message);
        default:
            env->ThrowNew(env->FindClass("java/lang/RuntimeException"), message);
            break;
    }
}
