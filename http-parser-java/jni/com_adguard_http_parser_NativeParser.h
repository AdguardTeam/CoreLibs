/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class com_adguard_http_parser_NativeParser */

#ifndef _Included_com_adguard_http_parser_NativeParser
#define _Included_com_adguard_http_parser_NativeParser
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_adguard_http_parser_NativeParser
 * Method:    init
 * Signature: (Lcom/adguard/http/parser/NativeParser;J)V
 */
JNIEXPORT void JNICALL Java_com_adguard_http_parser_NativeParser_init
  (JNIEnv *, jclass, jobject, jlong);

/*
 * Class:     com_adguard_http_parser_NativeParser
 * Method:    connect
 * Signature: (JJLcom/adguard/http/parser/NativeParser/Callbacks;)J
 */
JNIEXPORT jlong JNICALL Java_com_adguard_http_parser_NativeParser_connect
  (JNIEnv *, jclass, jlong, jlong, jobject);

/*
 * Class:     com_adguard_http_parser_NativeParser
 * Method:    disconnect0
 * Signature: (JI)I
 */
JNIEXPORT jint JNICALL Java_com_adguard_http_parser_NativeParser_disconnect0
  (JNIEnv *, jclass, jlong, jint);

/*
 * Class:     com_adguard_http_parser_NativeParser
 * Method:    input0
 * Signature: (JI[B)I
 */
JNIEXPORT jint JNICALL Java_com_adguard_http_parser_NativeParser_input0
  (JNIEnv *, jclass, jlong, jint, jbyteArray);

/*
 * Class:     com_adguard_http_parser_NativeParser
 * Method:    closeConnection
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_com_adguard_http_parser_NativeParser_closeConnection
  (JNIEnv *, jclass, jlong);

/*
 * Class:     com_adguard_http_parser_NativeParser
 * Method:    getConnectionId
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_com_adguard_http_parser_NativeParser_getConnectionId
  (JNIEnv *, jclass, jlong);

/*
 * Class:     com_adguard_http_parser_NativeParser
 * Method:    closeParser
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_adguard_http_parser_NativeParser_closeParser
  (JNIEnv *, jclass, jlong);

#ifdef __cplusplus
}
#endif
#endif
