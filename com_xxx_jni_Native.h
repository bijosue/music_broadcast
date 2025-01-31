/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class com_xxx_jni_Native */

#ifndef _Included_com_xxx_jni_Native
#define _Included_com_xxx_jni_Native
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_xxx_jni_Native
 * Method:    musicSenderInit
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_xxx_jni_Native_musicSenderInit
  (JNIEnv *, jclass);

/*
 * Class:     com_xxx_jni_Native
 * Method:    musicRecvStart
 * Signature: (IIIII)I
 */
JNIEXPORT jint JNICALL Java_com_xxx_jni_Native_musicRecvStart
  (JNIEnv *, jclass, jint, jint, jint, jint, jint);

/*
 * Class:     com_xxx_jni_Native
 * Method:    musicRecvStop
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_com_xxx_jni_Native_musicRecvStop
  (JNIEnv *, jclass, jint);

/*
 * Class:     com_xxx_jni_Native
 * Method:    musicSenderStart
 * Signature: (Ljava/lang/String;III)I
 */
JNIEXPORT jint JNICALL Java_com_xxx_jni_Native_musicSenderStart
  (JNIEnv *, jclass, jstring, jint, jint, jint);

/*
 * Class:     com_xxx_jni_Native
 * Method:    musicSenderPause
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_com_xxx_jni_Native_musicSenderPause
  (JNIEnv *, jclass, jint);

/*
 * Class:     com_xxx_jni_Native
 * Method:    musicSenderStop
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_com_xxx_jni_Native_musicSenderStop
  (JNIEnv *, jclass, jint, jint);

#ifdef __cplusplus
}
#endif
#endif
