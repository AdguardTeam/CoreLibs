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

static int NativeParser_HttpRequestReceived(connection_id id, void *header);
static int NativeParser_HttpRequestBodyStarted(connection_id id);
static void NativeParser_HttpRequestBodyData(connection_id id, const char *data, size_t length);
static void NativeParser_HttpRequestBodyFinished(connection_id id);
static int NativeParser_HttpResponseReceived(connection_id id, void *header);
static int NativeParser_HttpResponseBodyStarted(connection_id id);
static void NativeParser_HttpResponseBodyData(connection_id id, const char *data, size_t length);
static void NativeParser_HttpResponseBodyFinished(connection_id id);
static void NativeParser_ParseError(connection_id id, transfer_direction direction, error_type type, const char *message);

static parser_callbacks contextCallbacks = {
        .http_request_received = NativeParser_HttpRequestReceived,
        .http_request_body_started = NativeParser_HttpRequestBodyStarted,
        .http_request_body_data = NativeParser_HttpRequestBodyData,
        .http_request_body_finished = NativeParser_HttpRequestBodyFinished,
        .http_response_received = NativeParser_HttpResponseReceived,
        .http_response_body_started = NativeParser_HttpResponseBodyStarted,
        .http_response_body_data = NativeParser_HttpResponseBodyData,
        .http_response_body_finished = NativeParser_HttpResponseBodyFinished,
        .parse_error = NativeParser_ParseError,
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
    jmethodID ParseErrorCallback;

    Callbacks(JNIEnv *env);
};
static Callbacks *javaCallbacks = NULL;

static std::map<jlong, ParserContext *> context_map;

int Java_com_adguard_http_parser_NativeParser_connect(JNIEnv *env, jclass cls, jlong id,
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

    int r = parser_connect(id, info, &contextCallbacks);
    return r;
}

static int NativeParser_HttpRequestReceived(connection_id id, void *header) {
    ParserContext *context = context_map[id];
    if (context == NULL) {
        return -1;
    }

    http_message *clone = http_message_clone((const http_message *) header);
    return context->env->CallIntMethod(context->callbacks, javaCallbacks->HttpRequestReceivedCallback, id, (jlong) clone);
}

static int NativeParser_HttpRequestBodyStarted(connection_id id) {
    ParserContext *context = context_map[id];
    if (context == NULL) {
        return -1;
    }

    return context->env->CallIntMethod(context->callbacks, javaCallbacks->HttpRequestBodyStartedCallback, id);
}

static void NativeParser_HttpRequestBodyData(connection_id id, const char *data, size_t length) {
    ParserContext *context = context_map[id];
    if (context == NULL) {
        return;
    }

    jbyteArray arr = context->env->NewByteArray((jsize) length);
    context->env->SetByteArrayRegion(arr, 0, (jsize) length, (jbyte *) data);
    context->env->CallVoidMethod(context->callbacks, javaCallbacks->HttpRequestBodyDataCallback, id, arr);

    // JNI will not auto clean local references since this method wasn't invoked from JVM
    context->env->DeleteLocalRef(arr);
}

static void NativeParser_HttpRequestBodyFinished(connection_id id) {
    ParserContext *context = context_map[id];
    if (context == NULL) {
        return;
    }

    context->env->CallVoidMethod(context->callbacks, javaCallbacks->HttpRequestBodyFinishedCallback, id);
}

static int NativeParser_HttpResponseReceived(connection_id id, void *header) {
    ParserContext *context = context_map[id];
    if (context == NULL) {
        return -1;
    }

    http_message *clone = http_message_clone((const http_message *) header);
    return context->env->CallIntMethod(context->callbacks, javaCallbacks->HttpResponseReceivedCallback, id, (jlong) clone);
}

static int NativeParser_HttpResponseBodyStarted(connection_id id) {
    ParserContext *context = context_map[id];
    if (context == NULL) {
        return -1;
    }

    return context->env->CallIntMethod(context->callbacks, javaCallbacks->HttpResponseBodyStartedCallback, id);
}

static void NativeParser_HttpResponseBodyData(connection_id id, const char *data, size_t length) {
    ParserContext *context = context_map[id];
    if (context == NULL) {
        return;
    }

    jbyteArray arr = context->env->NewByteArray((jsize) length);
    context->env->SetByteArrayRegion(arr, 0, (jsize) length, (jbyte *) data);
    context->env->CallVoidMethod(context->callbacks, javaCallbacks->HttpResponseBodyDataCallback, id, arr);

    // JNI will not auto clean local references since this method wasn't invoked from JVM
    context->env->DeleteLocalRef(arr);
}

static void NativeParser_HttpResponseBodyFinished(connection_id id) {
    ParserContext *context = context_map[id];
    if (context == NULL) {
        return;
    }

    context->env->CallVoidMethod(context->callbacks, javaCallbacks->HttpResponseBodyFinishedCallback, id);
}

static void NativeParser_ParseError(connection_id id, transfer_direction direction, error_type type, const char *message) {
    ParserContext *context = context_map[id];
    if (context == NULL) {
        return;
    }

    jstring messageString = context->env->NewStringUTF(message);
    context->env->CallVoidMethod(context->callbacks, javaCallbacks->ParseErrorCallback, id, (int) direction, (int) type, messageString);
    context->env->DeleteLocalRef(messageString);
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
    ParseErrorCallback = env->GetMethodID(callbacksClass, "onParseError", "(JIILjava/lang/String;)V");
}

jint Java_com_adguard_http_parser_NativeParser_disconnect0(JNIEnv *env, jclass cls, jlong id,
                                                          jint direction) {
    ParserContext *context = context_map[id];
    parser_disconnect(id, (transfer_direction) direction);
    if (direction == DIRECTION_IN) {
        // delete context;
    }
    return 0;
}

jint Java_com_adguard_http_parser_NativeParser_input0(JNIEnv *env, jclass cls, jlong id, jint direction,
                                                     jbyteArray bytes) {
    jbyte *data = env->GetByteArrayElements(bytes, NULL);
    int len = env->GetArrayLength(bytes);
    int r = parser_input(id, (transfer_direction) direction, (const char *) data, len);
    env->ReleaseByteArrayElements(bytes, data, JNI_ABORT);
    return r;
}

jint Java_com_adguard_http_parser_NativeParser_close(JNIEnv *env, jobject obj, jlong id) {
    return parser_connection_close(id);
}

jstring Java_com_adguard_http_parser_HttpMessage_getUrl(JNIEnv * env, jclass cls, jlong address) {
    http_message *header = (http_message *) address;
    return env->NewStringUTF(header->url);
}

void Java_com_adguard_http_parser_HttpMessage_setUrl(JNIEnv *env, jclass cls, jlong address, jstring value) {
    http_message *header = (http_message *) address;
    jboolean isCopy;
    const char *chars = env->GetStringUTFChars(value, &isCopy);
    size_t len = strlen(chars);
    http_message_set_url(header, chars, len);
    if (isCopy) {
        env->ReleaseStringUTFChars(value, chars);
    }
}

jstring Java_com_adguard_http_parser_HttpMessage_getStatus(JNIEnv *env, jclass cls, jlong address) {
    http_message *header = (http_message *) address;
    return env->NewStringUTF(header->status);
}

jint Java_com_adguard_http_parser_HttpMessage_getStatusCode(JNIEnv *env, jclass cls, jlong address) {
    http_message *header = (http_message *) address;
    return header->status_code;
}

jstring Java_com_adguard_http_parser_HttpMessage_getMethod(JNIEnv *env, jclass cls, jlong address) {
    http_message *header = (http_message *) address;
    return env->NewStringUTF(header->method);
}

jlongArray Java_com_adguard_http_parser_HttpMessage_getHeaders(JNIEnv *env, jclass cls, jlong address) {
    http_message *header = (http_message *) address;
    jlong addresses[header->field_count];
    http_header_field *parameter = header->fields;
    for (size_t i = 0; i < header->field_count; i++) {
        addresses[i] = (jlong) parameter;
        ++parameter;
    }
    jlongArray array = env->NewLongArray(header->field_count);
    env->SetLongArrayRegion(array, 0, header->field_count, addresses);
    return array;
}

void Java_com_adguard_http_parser_HttpMessage_addHeader(JNIEnv *env, jclass cls, jlong address, jstring fieldName, jstring value) {
    jboolean fieldCharsIsCopy;
    const char *fieldChars = env->GetStringUTFChars(fieldName, &fieldCharsIsCopy);
    jboolean valueCharsIsCopy;
    const char *valueChars = env->GetStringUTFChars(value, &valueCharsIsCopy);
    http_message *header = (http_message *) address;
    http_message_add_header_field(header, fieldChars, strlen(fieldChars));
    http_message_set_header_field(header, fieldChars, strlen(fieldChars), valueChars, strlen(valueChars));
    if (fieldCharsIsCopy) {
        env->ReleaseStringUTFChars(fieldName, fieldChars);
    }
    if (valueCharsIsCopy) {
        env->ReleaseStringUTFChars(fieldName, valueChars);
    }
}

jint Java_com_adguard_http_parser_HttpMessage_sizeBytes(JNIEnv *env, jclass cls, jlong address) {
    http_message *header = (http_message *) address;
    size_t length = 0;
    char *message_raw = http_message_raw(header, &length);

    free(message_raw);
    return (jint) length;
}

jbyteArray Java_com_adguard_http_parser_HttpMessage_getBytes__J(JNIEnv *env, jclass cls, jlong address) {
    http_message *header = (http_message *) address;
    size_t length = 0;
    char *message_raw = http_message_raw(header, &length);

    jbyteArray arr = env->NewByteArray((jsize) length);
    env->SetByteArrayRegion(arr, 0, (jsize) length, (jbyte *) message_raw);
    free(message_raw);
    return arr;
}

void Java_com_adguard_http_parser_HttpMessage_getBytes__J_3B(JNIEnv *env, jclass cls, jlong address, jbyteArray arr) {
    http_message *header = (http_message *) address;
    size_t length = 0;
    char *message_raw = http_message_raw(header, &length);

    env->SetByteArrayRegion(arr, 0, (jsize) length, (jbyte *) message_raw);
    free(message_raw);
}

void Java_com_adguard_http_parser_HttpMessage_removeHeader(JNIEnv *env, jclass cls, jlong address, jstring fieldName) {
    http_message *header = (http_message *) address;
    jboolean fieldCharsIsCopy;
    const char *fieldChars = env->GetStringUTFChars(fieldName, &fieldCharsIsCopy);
    http_message_del_header_field(header, fieldChars, strlen(fieldChars));
    if (fieldCharsIsCopy) {
        env->ReleaseStringUTFChars(fieldName, fieldChars);
    }
}

jlong Java_com_adguard_http_parser_HttpMessage_clone(JNIEnv *env, jclass cls, jlong address) {
    if (address == 0)
        return 0;
    return (jlong) http_message_clone((http_message *) address);
}

jlong Java_com_adguard_http_parser_HttpMessage_createHttpMessage(JNIEnv *env, jclass cls) {
    http_message *message = (http_message *) malloc(sizeof(http_message));
    memset(message, 0, sizeof(http_message));
    return (jlong) message;
}

void Java_com_adguard_http_parser_HttpMessage_setStatusCode(JNIEnv *env, jclass cls, jlong address, jint code) {
    http_message *message = (http_message *) address;
    http_message_set_status_code(message, code);
}

void Java_com_adguard_http_parser_HttpMessage_setStatus(JNIEnv *env, jclass cls, jlong address, jstring status) {
    http_message *message = (http_message *) address;
    jboolean isCopy;
    const char *statusText = env->GetStringUTFChars(status, &isCopy);
    http_message_set_status(message, statusText, strlen(statusText));
    if (isCopy) {
        env->ReleaseStringUTFChars(status, statusText);
    }
}

void Java_com_adguard_http_parser_HttpMessage_free(JNIEnv *env, jclass cls, jlong address) {
    http_message *message = (http_message *) address;
    http_message_free(message);
}

jstring Java_com_adguard_http_parser_HttpMessage_00024HttpHeaderField_getKey(JNIEnv *env, jclass cls, jlong address) {
    http_header_field *parameter = (http_header_field *) address;
    return env->NewStringUTF(parameter->name);
}

jstring Java_com_adguard_http_parser_HttpMessage_00024HttpHeaderField_getValue(JNIEnv *env, jclass cls, jlong address) {
    http_header_field *parameter = (http_header_field *) address;
    return env->NewStringUTF(parameter->value);
}
