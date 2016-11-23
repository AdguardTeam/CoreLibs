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
 * Method:    connect
 * Signature: (JLcom/adguard/http/parser/NativeParser/Callbacks;)I
 */
JNIEXPORT jint JNICALL Java_com_adguard_http_parser_NativeParser_connect
  (JNIEnv *, jclass, jlong, jobject);

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
 * Method:    close
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_com_adguard_http_parser_NativeParser_close
  (JNIEnv *, jobject, jlong);

#ifdef __cplusplus
}
#endif
#endif