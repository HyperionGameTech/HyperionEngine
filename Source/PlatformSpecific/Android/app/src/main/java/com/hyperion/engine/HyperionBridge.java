package com.hyperion.engine;

import android.content.res.AssetManager;
import android.view.Surface;

public final class HyperionBridge {

    static {
        System.loadLibrary("hyperion");
        System.loadLibrary("hyperion-android");
    }

    public static native int nativeInit();
    public static native void nativeShutdown();
    public static native long nativeCreateGame(String gameClassName);
    public static native void nativeDestroyGame(long gameInstancePtr);
    public static native void nativeSetGame(long gameInstancePtr);
    public static native void nativeLaunchThreads();
    public static native void nativeSetAssetManager(AssetManager mgr);
    public static native void nativeSetCacheDirectory(String cacheDir);
    public static native void nativeSetSurface(Surface surface, int width, int height);
    public static native void nativeTouchEvent(int action, float x, float y, int pointerId);
    public static native void nativeKeyEvent(int action, int keyCode);
    public static native void nativeTextInputEvent(String text);
    
    public static void showSoftKeyboard() {
        MainActivity activity = MainActivity.getInstance();

        if (activity != null) {
            activity.showSoftKeyboardInternal();
        }
    }

    public static void hideSoftKeyboard() {
        MainActivity activity = MainActivity.getInstance();

        if (activity != null) {
            activity.hideSoftKeyboardInternal();
        }
    }
}
