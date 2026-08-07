package com.hyperion.engine;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.text.InputType;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.IOException;

public class MainActivity extends Activity implements SurfaceHolder.Callback {

    private static final String TAG = "HyperionMain";
    private static final long NULL = 0;

    private static MainActivity s_instance;

    static MainActivity getInstance() {
        return s_instance;
    }

    private SurfaceView m_surfaceView;
    private Thread m_engineThread;

    private volatile boolean m_engineReady = false;

    private long m_gameInstance = 0;

    private final Object m_surfaceLock = new Object();
    private volatile Surface m_pendingSurface = null;

    private static final float TOUCH_DEAD_ZONE = 8.0f;
    private float m_touchDownX = 0.0f;
    private float m_touchDownY = 0.0f;
    private boolean m_touchMoved = false;

    private class HyperionSurfaceView extends SurfaceView {
        HyperionSurfaceView(Context context) {
            super(context);

            setFocusable(true);
            setFocusableInTouchMode(true);
        }

        @Override
        public boolean onCheckIsTextEditor() {
            return true;
        }

        @Override
        public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
            outAttrs.inputType = InputType.TYPE_CLASS_TEXT;
            outAttrs.imeOptions = EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_EXTRACT_UI;

            return new BaseInputConnection(this, false) {
                @Override
                public boolean commitText(CharSequence text, int newCursorPosition) {
                    if (m_engineReady && text != null && text.length() > 0) {
                        HyperionBridge.nativeTextInputEvent(text.toString());
                    }

                    return true;
                }

                @Override
                public boolean deleteSurroundingText(int beforeLength, int afterLength) {
                    if (m_engineReady) {
                        for (int i = 0; i < beforeLength; i++) {
                            HyperionBridge.nativeKeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL);
                            HyperionBridge.nativeKeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL);
                        }
                    }

                    return true;
                }

                @Override
                public boolean sendKeyEvent(KeyEvent event) {
                    if (m_engineReady) {
                        HyperionBridge.nativeKeyEvent(event.getAction(), event.getKeyCode());
                    }

                    return true;
                }
            };
        }
    }

    void showSoftKeyboardInternal() {
        runOnUiThread(() -> {
            m_surfaceView.requestFocus();

            InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);

            if (imm != null) {
                imm.showSoftInput(m_surfaceView, InputMethodManager.SHOW_FORCED);
            }
        });
    }

    void hideSoftKeyboardInternal() {
        runOnUiThread(() -> {
            InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);

            if (imm != null) {
                imm.hideSoftInputFromWindow(m_surfaceView.getWindowToken(), 0);
            }
        });
    }

    private void runEngineLoop() {
        Log.i(TAG, "Hyperion runEngineLoop()");

        // Extract Cache/ assets to internal storage so the engine's blob storage
        // can memory-map them (APK assets can't be mmap'd).
        File internalCacheDir = new File(getFilesDir(), "EngineCache");
        if (!internalCacheDir.exists()) {
            internalCacheDir.mkdirs();
            try {
                copyAssetFolder("Cache", internalCacheDir);
            } catch (IOException e) {
                Log.e(TAG, "Failed to extract cache assets: " + e.getMessage());
            }
        }
        HyperionBridge.nativeSetCacheDirectory(internalCacheDir.getAbsolutePath());

        int result = HyperionBridge.nativeInit();
        if (result == 0) {
            Log.e(TAG, "nativeInit failed, engine will not start");
            return;
        }

        m_gameInstance = HyperionBridge.nativeCreateGame("DefaultGame"); // @TODO: don't hardcode game class name, pull it from somewhere
        assert m_gameInstance != NULL;

        Log.i(TAG, "Hyperion setGame : " + m_gameInstance);

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

        HyperionBridge.nativeSetSurface(surface, 0, 0);

        m_engineReady = true;


        Log.i(TAG, "Hyperion launch threads");
        // blocks until main thread loop exits
        HyperionBridge.nativeLaunchThreads();

        m_engineReady = false;

        HyperionBridge.nativeShutdown();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Log.i(TAG, "Surface onCreate()");

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().addFlags(
            WindowManager.LayoutParams.FLAG_FULLSCREEN |
            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().setBackgroundDrawable(null);

        HyperionBridge.nativeSetAssetManager(getAssets());

        s_instance = this;

        m_surfaceView = new HyperionSurfaceView(this);
        m_surfaceView.getHolder().addCallback(this);
        setContentView(m_surfaceView);

        Log.i(TAG, "Hyperion: starting up engine thread");

        m_engineThread = new Thread(this::runEngineLoop, "HyperionEngineMain");
        m_engineThread.start();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        Log.i(TAG, "Surface onDestroy()");

        if (s_instance == this) {
            s_instance = null;
        }

        if (!isFinishing()) {
            // treat as a config change (recreates swapchain)
            HyperionBridge.nativeSetSurface(null, 0, 0);
            return;
        }

        // shut down!
        if (m_gameInstance != NULL) {
            HyperionBridge.nativeSetGame(NULL);
            HyperionBridge.nativeDestroyGame(m_gameInstance);
            m_gameInstance = NULL;
        }

        HyperionBridge.nativeSetAssetManager(null);

        HyperionBridge.nativeShutdown();

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
            HyperionBridge.nativeSetSurface(holder.getSurface(), 0, 0);
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        if (m_engineReady) {
            HyperionBridge.nativeSetSurface(holder.getSurface(), width, height);
        }
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        synchronized (m_surfaceLock) {
            m_pendingSurface = null;
        }

        if (m_engineReady) {
            HyperionBridge.nativeSetSurface(null, 0, 0);
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

            switch (action) {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_POINTER_DOWN:
                    m_touchDownX = x;
                    m_touchDownY = y;
                    m_touchMoved = false;
                    HyperionBridge.nativeTouchEvent(action, x, y, pointerId);
                    break;

                case MotionEvent.ACTION_MOVE:
                    // Send move events for ALL active pointers
                    int pointerCount = event.getPointerCount();
                    for (int i = 0; i < pointerCount; i++) {
                        int pid = event.getPointerId(i);
                        float px = event.getX(i);
                        float py = event.getY(i);
                        HyperionBridge.nativeTouchEvent(action, px, py, pid);
                    }
                    m_touchMoved = true;
                    break;

                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_POINTER_UP:
                case MotionEvent.ACTION_CANCEL:
                    HyperionBridge.nativeTouchEvent(action, x, y, pointerId);
                    m_touchMoved = false;
                    break;

                default:
                    HyperionBridge.nativeTouchEvent(action, x, y, pointerId);
                    break;
            }
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

    private void copyAssetFolder(String assetPath, File destDir) throws IOException {
        String[] files = getAssets().list(assetPath);
        if (files == null || files.length == 0) {
            return;
        }
        for (String file : files) {
            String assetFilePath = assetPath + "/" + file;
            String[] subFiles = getAssets().list(assetFilePath);
            if (subFiles != null && subFiles.length > 0) {
                copyAssetFolder(assetFilePath, new File(destDir, file));
            } else {
                File outFile = new File(destDir, file);
                outFile.getParentFile().mkdirs();
                try (InputStream in = getAssets().open(assetFilePath);
                     FileOutputStream out = new FileOutputStream(outFile)) {
                    byte[] buffer = new byte[65536];
                    int len;
                    while ((len = in.read(buffer)) > 0) {
                        out.write(buffer, 0, len);
                    }
                }
            }
        }
    }
}
