#include <jni.h>

#include <android/log.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include <cstdio>
#include <alloca.h>

#define LOG_TAG "HyperionAndroid"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace Hyperion {
    class Game;
} // namespace Hyperion

using namespace Hyperion;

static char g_cacheDirPath[1024] = { 0 };
static char g_contentDirPath[1024] = { 0 };
static char g_cacheServerAddress[256] = { 0 };

extern "C"
{
    int Hyp_Initialize(int argc, char** argv);
    void Hyp_Shutdown();
    Game* Hyp_CreateGame(const char* gameClassName);
    void Hyp_DestroyGame(Game* game);
    void Hyp_SetGame(Game* game);
    void Hyp_LaunchThreads();
    void Hyp_Shutdown();
    void Hyp_SetAssetManager(void* mgr);
    void Hyp_SetNativeWindow(void* nativeWindow, int width, int height);
    void Hyp_InputEvent(int type, int action, float x, float y, int iParam);
    void Hyp_TextInputEvent(const char* text);

    void Hyp_Android_InitJNI(JNIEnv* env, jclass hyperionBridgeClass);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_hyperion_engine_HyperionBridge_nativeInit(JNIEnv* env, jclass clazz)
{
    Hyp_Android_InitJNI(env, clazz);

    const char* baseArgs[] = {
        "hyperion",
        "-SimulateOnMainThread=true",
        "-RenderOnMainThread=false"
    };

    int argc = int(sizeof(baseArgs) / sizeof(baseArgs[0]));

    const char* cacheArg = nullptr;
    static char cacheArgBuf[1100];

    if (g_cacheDirPath[0] != '\0')
    {
        snprintf(cacheArgBuf, sizeof(cacheArgBuf), "--cachedir=%s", g_cacheDirPath);
        cacheArg = cacheArgBuf;
        argc += 1;
    }

    const char* contentArg = nullptr;
    static char contentArgBuf[1100];

    if (g_contentDirPath[0] != '\0')
    {
        snprintf(contentArgBuf, sizeof(contentArgBuf), "--contentdir=%s", g_contentDirPath);
        contentArg = contentArgBuf;
        argc += 1;
    }

    const char* serverArg = nullptr;
    static char serverArgBuf[300];

    if (g_cacheServerAddress[0] != '\0')
    {
        snprintf(serverArgBuf, sizeof(serverArgBuf), "--cacheserver=%s", g_cacheServerAddress);
        serverArg = serverArgBuf;
        argc += 1;
    }

    const char** combinedArgv = (const char**)alloca((argc + 1) * sizeof(const char*));
    int idx = 0;

    for (int i = 0; i < int(sizeof(baseArgs) / sizeof(baseArgs[0])); i++)
    {
        combinedArgv[idx++] = baseArgs[i];
    }
    if (cacheArg != nullptr)
    {
        combinedArgv[idx++] = cacheArg;
    }
    if (contentArg != nullptr)
    {
        combinedArgv[idx++] = contentArg;
    }
    if (serverArg != nullptr)
    {
        combinedArgv[idx++] = serverArg;
    }

    combinedArgv[idx] = nullptr;

    return Hyp_Initialize(argc, const_cast<char**>(combinedArgv));
}

extern "C" JNIEXPORT void JNICALL
Java_com_hyperion_engine_HyperionBridge_nativeSetCacheDirectory(JNIEnv* env, jclass /*clazz*/, jstring cacheDir)
{
    const char* path = env->GetStringUTFChars(cacheDir, nullptr);
    snprintf(g_cacheDirPath, sizeof(g_cacheDirPath), "%s", path);
    env->ReleaseStringUTFChars(cacheDir, path);
}

extern "C" JNIEXPORT void JNICALL
Java_com_hyperion_engine_HyperionBridge_nativeSetContentDirectory(JNIEnv* env, jclass /*clazz*/, jstring contentDir)
{
    const char* path = env->GetStringUTFChars(contentDir, nullptr);
    snprintf(g_contentDirPath, sizeof(g_contentDirPath), "%s", path);
    env->ReleaseStringUTFChars(contentDir, path);
}

extern "C" JNIEXPORT void JNICALL
Java_com_hyperion_engine_HyperionBridge_nativeSetCacheServer(JNIEnv* env, jclass /*clazz*/, jstring host, jint port)
{
    const char* hostStr = env->GetStringUTFChars(host, nullptr);
    snprintf(g_cacheServerAddress, sizeof(g_cacheServerAddress), "%s:%u", hostStr, unsigned(port));
    env->ReleaseStringUTFChars(host, hostStr);
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
Java_com_hyperion_engine_HyperionBridge_nativeSetSurface(JNIEnv* env, jclass /*clazz*/, jobject javaSurface, jint width, jint height)
{
    if (javaSurface != nullptr)
    {
        ANativeWindow* nativeWindow = ANativeWindow_fromSurface(env, javaSurface);
        Hyp_SetNativeWindow(nativeWindow, int(width), int(height));
        ANativeWindow_release(nativeWindow);
    }
    else
    {
        Hyp_SetNativeWindow(nullptr, 0, 0);
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

extern "C" JNIEXPORT void JNICALL
Java_com_hyperion_engine_HyperionBridge_nativeTextInputEvent(JNIEnv* env, jclass /*clazz*/, jstring text)
{
    if (text == nullptr)
    {
        return;
    }

    const char* textChars = env->GetStringUTFChars(text, nullptr);
    Hyp_TextInputEvent(textChars);
    env->ReleaseStringUTFChars(text, textChars);
}