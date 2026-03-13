#include <jni.h>

#include <android/log.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#define LOG_TAG "HyperionAndroid"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C"
{
    int Hyp_Initialize(int argc, char** argv);
    void Hyp_Shutdown();
    void Hyp_LaunchThreads();
    void Hyp_SetAssetManager(void* mgr);
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
    Hyp_Shutdown();
}


extern "C" JNIEXPORT void JNICALL
Java_com_hyperion_engine_HyperionBridge_nativeLaunchThreads(JNIEnv* env, jclass /*clazz*/)
{
    Hyp_LaunchThreads();
}

extern "C" JNIEXPORT void JNICALL
Java_com_hyperion_engine_HyperionBridge_nativeSetAssetManager(JNIEnv* env, jclass /*clazz*/,  jobject javaAssetManager)
{
    static jobject s_assetManagerRef {};

    if (s_assetManagerRef != nullptr)
    {
        env->DeleteGlobalRef(s_assetManagerRef);
        s_assetManagerRef = nullptr;
    }

    if (javaAssetManager != nullptr)
    {
        s_assetManagerRef = env->NewGlobalRef(javaAssetManager);

        Hyp_SetAssetManager(AAssetManager_fromJava(env, s_assetManagerRef));
    }
    else
    {
        Hyp_SetAssetManager(nullptr);
    }
}
