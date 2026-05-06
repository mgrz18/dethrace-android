#ifdef __ANDROID__
#include <jni.h>

#include "common/globvars.h"

JNIEXPORT jboolean JNICALL
Java_com_dethrace_android_DethraceActivity_nativeIsInRace(JNIEnv *env, jobject thiz) {
    (void)env;
    (void)thiz;
    return gProgram_state.racing ? JNI_TRUE : JNI_FALSE;
}
#endif
