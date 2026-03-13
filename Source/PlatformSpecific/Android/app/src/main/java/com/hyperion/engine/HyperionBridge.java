package com.hyperion.engine;

import android.content.res.AssetManager;

public final class HyperionBridge {

    static {
        System.loadLibrary("hyperion");
        System.loadLibrary("hyperion-android");
    }

    public static native int nativeInit();
    public static native void nativeShutdown();
    public static native void nativeLaunchThreads();
    public static native void nativeSetAssetManager(AssetManager mgr);
}
