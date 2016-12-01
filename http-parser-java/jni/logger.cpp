//
// Created by s.fionov on 30.11.16.
//

#include "com_adguard_http_parser_NativeLogger.h"
#include "../../http-parser/src/logger.h"

struct LoggerCtx {
    JavaVM *vm;
    jobject callbackObject;
    jmethodID callbackMethod;
};

extern "C" {
    void NativeLogger_callback(logger *ctx, logger_log_level_t log_level, const char *thread_info, const char *message);
}

void NativeLogger_callback(logger *ctx, logger_log_level_t log_level, const char *thread_info, const char *message) {
    LoggerCtx *loggerCtx = (LoggerCtx *) ctx->attachment;
    JNIEnv *env;
    if (loggerCtx->vm->AttachCurrentThread((void **) &env, NULL) != JNI_OK) {
        return;
    }

    jstring threadInfoString = thread_info ? env->NewStringUTF(thread_info) : NULL;
    jstring messageString = message ? env->NewStringUTF(message) : NULL;

    env->CallVoidMethod(loggerCtx->callbackObject, loggerCtx->callbackMethod, (int) log_level,
                                   threadInfoString, messageString);

    env->DeleteLocalRef(threadInfoString);
    env->DeleteLocalRef(messageString);
    loggerCtx->vm->DetachCurrentThread();
}

jobject Java_com_adguard_http_parser_NativeLogger_open0(JNIEnv *env, jclass cls, jint logLevel, jobject callback) {
    jobject loggerObject = env->AllocObject(cls);
    if (loggerObject == NULL) {
        return NULL;
    }

    LoggerCtx *loggerCtx = new LoggerCtx;
    loggerCtx->callbackObject = env->NewGlobalRef(callback);
    jclass callbackClass = env->FindClass("com/adguard/http/parser/NativeLogger$NativeCallback");
    if (callbackClass == NULL) {
        delete loggerCtx;
        return NULL;
    }
    loggerCtx->callbackMethod = env->GetMethodID(callbackClass, "log", "(ILjava/lang/String;Ljava/lang/String;)V");
    env->GetJavaVM(&loggerCtx->vm);

    logger *log = logger_open(NULL, (logger_log_level_t) logLevel, NativeLogger_callback, loggerCtx);

    jfieldID nativePtr = env->GetFieldID(cls, "nativePtr", "J");
    if (nativePtr == NULL) {
        delete loggerCtx;
        return NULL;
    }
    env->SetLongField(loggerObject, nativePtr, (jlong) log);

    return loggerObject;
}

jobject Java_com_adguard_http_parser_NativeLogger_open1(JNIEnv *env, jclass cls, jint logLevel, jstring fileName) {
    jobject loggerObject = env->AllocObject(cls);
    if (loggerObject == NULL) {
        return NULL;
    }

    logger *log;
    if (fileName != NULL) {
        jboolean isCopy;
        const char *fileNameChars = env->GetStringUTFChars(fileName, &isCopy);

        log = logger_open(fileNameChars, (logger_log_level_t) logLevel, NULL, NULL);
        fprintf(stderr, "logger_open returned %p\n", (void *) log);
        if (isCopy) {
            env->ReleaseStringUTFChars(fileName, fileNameChars);
        }
    } else {
        log = logger_open(NULL, (logger_log_level_t) logLevel, NULL, NULL);
    }

    jfieldID nativePtr = env->GetFieldID(cls, "nativePtr", "J");
    if (nativePtr == NULL) {
        return NULL;
    }
    env->SetLongField(loggerObject, nativePtr, (jlong) log);

    return loggerObject;
}

jboolean Java_com_adguard_http_parser_NativeLogger_isOpen(JNIEnv *env, jclass cls, jlong nativePtr) {
    logger *log = (logger *) nativePtr;
    return (jboolean) logger_is_open(log);
}

void Java_com_adguard_http_parser_NativeLogger_close(JNIEnv *env, jclass cls, jlong nativePtr) {
    logger *log = (logger *) nativePtr;
    logger_close(log);
}

void Java_com_adguard_http_parser_NativeLogger_log(JNIEnv *env, jclass cls, jlong nativePtr, jint log_level, jstring message) {
    logger *log = (logger *) nativePtr;

    jboolean isCopy;
    const char *messageChars = env->GetStringUTFChars(message, &isCopy);
    logger_log(log, (logger_log_level_t) log_level, "%s", messageChars);
    if (isCopy) {
        env->ReleaseStringUTFChars(message, messageChars);
    }
}
