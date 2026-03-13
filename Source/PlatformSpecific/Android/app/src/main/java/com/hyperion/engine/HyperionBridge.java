package com.hyperion.engine;
public final class HyperionBridge {

    static {
        System.loadLibrary("hyperion");
        System.loadLibrary("hyperion-android");
    }

    /**
     * Calls Hyp_Initialize on the native side.
     * @return non-zero on success, 0 on failure.
     */
    public static native int nativeInit();

    /**
     * Calls Hyp_Shutdown on the native side.
     */
    public static native void nativeShutdown();
}
