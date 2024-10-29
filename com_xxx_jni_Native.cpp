#include <pthread.h>
#include "com_xxx_jni_Native.h"
#include "AudioS.h"
#include "AudioR.h"


static JavaVM *jvm = NULL;
static pthread_key_t jnienv_key;

void _android_key_cleanup(void *data)
{
    JNIEnv* env = (JNIEnv*)pthread_getspecific(jnienv_key);
    if (env != NULL) {
        jvm->DetachCurrentThread();
        pthread_setspecific(jnienv_key, NULL);
    }
}

void setJvm(JavaVM *vm)
{
    jvm = vm;
    pthread_key_create(&jnienv_key, _android_key_cleanup);
}

JNIEnv *getJniEnv(void)
{
    JNIEnv *env = NULL;
    if (jvm == NULL) {

    } else {
        env = (JNIEnv*)pthread_getspecific(jnienv_key);
        if (env == NULL) {
            if (jvm->AttachCurrentThread(&env, NULL) != 0) {
                return NULL;
            }
            pthread_setspecific(jnienv_key, env);
        }
    }
    return env;
}

static const char* GetStringUTFChars(JNIEnv* env, jstring string)
{
    const char *cstring = string ? env->GetStringUTFChars(string, NULL) : NULL;
    return cstring;
}

static void ReleaseStringUTFChars(JNIEnv* env, jstring string, const char *cstring)
{
    if (string) env->ReleaseStringUTFChars(string, cstring);
}

static jclass jxxx = NULL;
static jmethodID eventID = NULL;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv *env = NULL;
    jint result = -1;

    setJvm(vm);

    if ((*vm).GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK)
        return -1;
    if (env == NULL)
    {
        return result;
    }

    result = JNI_VERSION_1_6;
    
    return result;
}

int jni_event_request(const char *url, const char *xml)
{
    JNIEnv *env = getJniEnv();

    if (env == NULL) {
        return -1;
    }

    jstring _url = env->NewStringUTF(url);
    jstring _xml = env->NewStringUTF(xml);

    env->CallStaticVoidMethod(jxxx, eventID, _url, _xml);

    if (_url)
        env->DeleteLocalRef(_url);
    if (_xml)
        env->DeleteLocalRef(_xml);

    return 0;
}

extern "C" JNIEXPORT jint JNICALL Java_com_xxx_jni_Native_musicSenderInit
        (JNIEnv *env, jclass clazz)
{
    jxxx = (jclass)env->NewGlobalRef(env->FindClass("com/xxx/jni/Native"));
    eventID = env->GetStaticMethodID(jxxx, "event", "(Ljava/lang/String;Ljava/lang/String;)V");


   
	av_register_all();
	for (int i=0; i<MAX_MULTICAST_CHANNEL; i++)
	{
		audio_s[i].init(i);
	}
    


    return 0;
}

extern "C" JNIEXPORT jint JNICALL Java_com_xxx_jni_Native_musicSenderStart
        (JNIEnv *env, jclass, jstring jurl, jint chn, jint port,  jint contn)
{

    int ret = -1;
    const char *url = GetStringUTFChars(env, jurl);
    if (NULL == url)
    {
        return ret;
    }
    if (MAX_MULTICAST_CHANNEL <= chn)
    {
        return ret;
    }

    ret = audio_s[chn].playMusic(port, contn, url);
    if (!contn)
    {
        audio_s[chn].play_ack(ret);
    }

    ReleaseStringUTFChars(env, jurl, url);

    return ret;
}

extern "C" JNIEXPORT jint JNICALL Java_com_xxx_jni_Native_musicSenderPause
        (JNIEnv *, jclass, jint chn)
{
    if (MAX_MULTICAST_CHANNEL <= chn)
    {
        return -1;
    }
    KLOGI("multicast pause chn(%d)\n", chn);
    audio_s[chn].pausePlay();

    return 0;
}

extern "C" JNIEXPORT jint JNICALL Java_com_xxx_jni_Native_musicSenderStop
        (JNIEnv *, jclass, jint port, jint chn)
{
    if (0 <= chn && MAX_MULTICAST_CHANNEL < chn)
    {
        audio_s[chn].stop();
    }else
    {
        for (int i = 0; i < MAX_MULTICAST_CHANNEL; i++)
        {
            audio_s[chn].stop();
        }
    }
    KLOGI("audio_sendr_stop (%d).\n", chn);
    return 0;
}


extern "C"
JNIEXPORT jint JNICALL
Java_com_xxx_jni_Native_musicRecvStart(JNIEnv *env, jclass clazz, jint chn, jint port,
                                                   jint channels, jint sample_rate, jint vol) {
    // TODO: implement multicast_receive()

    if (0 == audio_r.setParam(port, channels, sample_rate, chn,
                                      vol))
    {
        audio_r.start();
    }

    return 0;
}
extern "C"
JNIEXPORT jint JNICALL
Java_com_xxx_jni_Native_musicRecvStop(JNIEnv *env, jclass clazz, jint chn) {

    audio_r.stopReceive(chn);

    return 0;
}


