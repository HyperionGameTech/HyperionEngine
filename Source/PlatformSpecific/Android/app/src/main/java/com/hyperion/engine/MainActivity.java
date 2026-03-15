package com.hyperion.engine;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.Window;
import android.view.WindowManager;

public class MainActivity extends Activity implements SurfaceHolder.Callback {

    private static final String TAG = "HyperionMain";
    private static final long NULL = 0;

    private SurfaceView m_surfaceView;
    private Thread m_engineThread;

    private volatile boolean m_engineReady = false;

    private long m_gameInstance = 0;

    private final Object m_surfaceLock = new Object();
    private volatile Surface m_pendingSurface = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().addFlags(
            WindowManager.LayoutParams.FLAG_FULLSCREEN |
            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().setBackgroundDrawable(null);

        HyperionBridge.nativeSetAssetManager(getAssets());

        m_surfaceView = new SurfaceView(this);
        m_surfaceView.getHolder().addCallback(this);
        setContentView(m_surfaceView);

        m_engineThread = new Thread(this::runEngineLoop, "HyperionEngineMain");
        m_engineThread.start();
    }

    private void runEngineLoop() {
        int result = HyperionBridge.nativeInit();
        if (result == 0) {
            Log.e(TAG, "nativeInit failed, engine will not start");
            return;
        }

        m_gameInstance = HyperionBridge.nativeCreateGame("DefaultGame"); // @TODO: don't hardcode game class name, pull it from somewhere
        assert m_gameInstance != NULL;

        HyperionBridge.nativeSetGame(m_gameInstance);

        Surface surface;
        synchronized (m_surfaceLock) {
            while (m_pendingSurface == null) {
                try {
                    m_surfaceLock.wait();
                } catch (InterruptedException e) {
                    return;
                }
            }
            surface = m_pendingSurface;
        }

        HyperionBridge.nativeSetSurface(surface);

        m_engineReady = true;

        // blocks until main thread loop exits
        HyperionBridge.nativeLaunchThreads();

        m_engineReady = false;

        HyperionBridge.nativeShutdown();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        if (m_gameInstance != NULL) {
            HyperionBridge.nativeSetGame(NULL);
            HyperionBridge.nativeDestroyGame(m_gameInstance);
            m_gameInstance = NULL;
        }

        HyperionBridge.nativeSetAssetManager(null);

        HyperionBridge.nativeStopThreads();

        synchronized (m_surfaceLock) {
            m_surfaceLock.notifyAll();
        }

        if (m_engineThread != null) {
            try {
                m_engineThread.join(5000);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }

    // SurfaceHolder.Callback

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        synchronized (m_surfaceLock) {
            m_pendingSurface = holder.getSurface();
            m_surfaceLock.notifyAll();
        }

        if (m_engineReady) {
            HyperionBridge.nativeSetSurface(holder.getSurface());
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        if (m_engineReady) {
            HyperionBridge.nativeSetSurface(holder.getSurface());
        }
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        synchronized (m_surfaceLock) {
            m_pendingSurface = null;
        }

        if (m_engineReady) {
            HyperionBridge.nativeSetSurface(null);
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (m_engineReady) {
            int action = event.getActionMasked();
            int pointerIndex = event.getActionIndex();
            int pointerId = event.getPointerId(pointerIndex);
            float x = event.getX(pointerIndex);
            float y = event.getY(pointerIndex);

            HyperionBridge.nativeTouchEvent(action, x, y, pointerId);
            return true;
        }
        return super.onTouchEvent(event);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (m_engineReady) {
            HyperionBridge.nativeKeyEvent(event.getAction(), event.getKeyCode());
        }
        return super.dispatchKeyEvent(event);
    }
}
