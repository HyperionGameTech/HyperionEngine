#include <jni.h>

#include <android/log.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#define LOG_TAG "HyperionAndroid"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace Hyperion {
    class Game;
} // namespace Hyperion

using namespace Hyperion;

extern "C"
{
    int Hyp_Initialize(int argc, char** argv);
    void Hyp_Shutdown();
    Game* Hyp_CreateGame(const char* gameClassName);
    void Hyp_DestroyGame(Game* game);
    void Hyp_SetGame(Game* game);
    void Hyp_LaunchThreads();
    void Hyp_StopThreads();
    void Hyp_SetAssetManager(void* mgr);
    void Hyp_SetNativeWindow(void* nativeWindow);
    void Hyp_InputEvent(int type, int action, float x, float y, int iParam);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_hyperion_engine_HyperionBridge_nativeInit(JNIEnv* env, jclass /*clazz*/)
{
    const char* argv[] = {
        "hyperion",
        "-RenderOnMainThread=false",
        "-SimulateOnMainThread=true"
    };

    return Hyp_Initialize(int(sizeof(argv) / sizeof(argv[0])), const_cast<char**>(argv));
}

extern "C" JNIEXPORT void JNICALL
Java_com_hyperion_engine_HyperionBridge_nativeShutdown(JNIEnv* env, jclass /*clazz*/)
{
    Hyp_Shutdown();
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_hyperion_engine_HyperionBridge_nativeCreateGame(JNIEnv* env, jclass /*clazz*/, jstring gameClassName)
{
    const char* gameClassNameStr = env->GetStringUTFChars(gameClassName, nullptr);
    Hyperion::Game* game = Hyp_CreateGame(gameClassNameStr);
    env->ReleaseStringUTFChars(gameClassName, gameClassNameStr);

    return reinterpret_cast<jlong>(game);
}

extern "C" JNIEXPORT void JNICALL
Java_com_hyperion_engine_HyperionBridge_nativeDestroyGame(JNIEnv* env, jclass /*clazz*/, jlong gameInstancePtr)
{
    Hyp_DestroyGame(reinterpret_cast<Game*>(gameInstancePtr));
}

extern "C" JNIEXPORT void JNICALL
Java_com_hyperion_engine_HyperionBridge_nativeSetGame(JNIEnv* env, jclass /*clazz*/, jlong gameInstancePtr)
{
    Hyp_SetGame(reinterpret_cast<Game*>(gameInstancePtr));
}

extern "C" JNIEXPORT void JNICALL
Java_com_hyperion_engine_HyperionBridge_nativeLaunchThreads(JNIEnv* env, jclass /*clazz*/)
{
    Hyp_LaunchThreads();
}

extern "C" JNIEXPORT void JNICALL
Java_com_hyperion_engine_HyperionBridge_nativeStopThreads(JNIEnv* /*env*/, jclass /*clazz*/)
{
    Hyp_StopThreads();
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

extern "C" JNIEXPORT void JNICALL
Java_com_hyperion_engine_HyperionBridge_nativeSetSurface(JNIEnv* env, jclass /*clazz*/, jobject javaSurface)
{
    if (javaSurface != nullptr)
    {
        ANativeWindow* nativeWindow = ANativeWindow_fromSurface(env, javaSurface);
        Hyp_SetNativeWindow(nativeWindow);
        // the engine will acquire its own ref, release this
        ANativeWindow_release(nativeWindow);
    }
    else
    {
        Hyp_SetNativeWindow(nullptr);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_hyperion_engine_HyperionBridge_nativeTouchEvent(JNIEnv* /*env*/, jclass /*clazz*/,
    jint action, jfloat x, jfloat y, jint pointerId)
{
    Hyp_InputEvent(0, action, x, y, pointerId);
}

extern "C" JNIEXPORT void JNICALL
Java_com_hyperion_engine_HyperionBridge_nativeKeyEvent(JNIEnv* /*env*/, jclass /*clazz*/,
    jint action, jint keyCode)
{
    Hyp_InputEvent(1, action, 0.0f, 0.0f, keyCode);
}
