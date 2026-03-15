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
    public static native void nativeLaunchThreads();
    public static native void nativeStopThreads();
    public static native void nativeSetAssetManager(AssetManager mgr);
    public static native void nativeSetSurface(Surface surface);
    public static native void nativeTouchEvent(int action, float x, float y, int pointerId);
    public static native void nativeKeyEvent(int action, int keyCode);
}
