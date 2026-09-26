#include <jni.h>

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativePing(
    JNIEnv *, jobject)
{
    return JNI_TRUE;
}
