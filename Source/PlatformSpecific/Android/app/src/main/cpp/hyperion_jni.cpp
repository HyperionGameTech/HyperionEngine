#include <jni.h>
#include <android/log.h>

#define LOG_TAG "HyperionAndroid"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Declarations from libhyperion.so (extern "C" symbols)
extern "C"
{
    int Hyp_Initialize(int argc, char** argv);
    void Hyp_Shutdown();
}

extern "C" JNIEXPORT jint JNICALL
Java_com_hyperion_engine_HyperionBridge_nativeInit(JNIEnv* env, jclass /*clazz*/)
{
    const char* argv0 = "hyperion";
    const char* argv1 = "-Headless";
    char* argv[] = { const_cast<char*>(argv0), const_cast<char*>(argv1) };

    return Hyp_Initialize(2, argv);
}

extern "C" JNIEXPORT void JNICALL
Java_com_hyperion_engine_HyperionBridge_nativeShutdown(JNIEnv* env, jclass /*clazz*/)
{
    LOGI("Shutting down Hyperion engine...");
    Hyp_Shutdown();
}
