package com.hyperion.engine;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
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
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class MainActivity extends Activity implements SurfaceHolder.Callback {

    private static final String TAG = "HyperionMain";
    private static final long NULL = 0;

    private static final String BUNDLED_CACHE_ASSET_PATH = "Cache";
    private static final String BUNDLED_CONTENT_ASSET_PATH = "Content";

    private static final String INSTALLED_CACHE_STAMP_NAME = ".bundled_cache_stamp";

    private static final int COPY_BUFFER_SIZE = 64 * 1024;

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

    private  File getDataDirectory(String subpath) {
        File externalFilesDir = getExternalFilesDir(null);

        return new File((externalFilesDir != null ? externalFilesDir : getFilesDir()) + subpath);
    }

    private long getPackageInstallTime() {
        try {
            PackageInfo packageInfo = getPackageManager().getPackageInfo(getPackageName(), 0);

            return packageInfo.lastUpdateTime;
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Failed to query package info", e);

            return 0;
        }
    }

    private String readInstalledCacheStamp(File stampFile) {
        if (!stampFile.isFile()) {
            return null;
        }

        try (InputStream input = new FileInputStream(stampFile)) {
            byte[] contents = new byte[(int) stampFile.length()];
            int totalRead = 0;

            while (totalRead < contents.length) {
                int read = input.read(contents, totalRead, contents.length - totalRead);

                if (read < 0) {
                    break;
                }

                totalRead += read;
            }

            return new String(contents, 0, totalRead, "UTF-8").trim();
        } catch (IOException e) {
            Log.w(TAG, "Failed to read cache stamp " + stampFile, e);

            return null;
        }
    }

    private void writeInstalledCacheStamp(File stampFile, String stamp) {
        try (OutputStream output = new FileOutputStream(stampFile)) {
            output.write(stamp.getBytes("UTF-8"));
        } catch (IOException e) {
            Log.e(TAG, "Failed to write cache stamp " + stampFile, e);
        }
    }

    private boolean copyAssetFile(AssetManager assetManager, String assetPath, File destinationFile) {
        File temporaryFile = new File(destinationFile.getAbsolutePath() + ".tmp");

        try (InputStream input = assetManager.open(assetPath, AssetManager.ACCESS_STREAMING);
             OutputStream output = new FileOutputStream(temporaryFile)) {

            byte[] buffer = new byte[COPY_BUFFER_SIZE];
            int read;

            while ((read = input.read(buffer)) >= 0) {
                output.write(buffer, 0, read);
            }
        } catch (IOException e) {
            Log.e(TAG, "Failed to copy asset " + assetPath + " to " + destinationFile, e);

            temporaryFile.delete();

            return false;
        }

        if (destinationFile.exists() && !destinationFile.delete()) {
            Log.e(TAG, "Failed to replace existing file " + destinationFile);

            temporaryFile.delete();

            return false;
        }

        if (!temporaryFile.renameTo(destinationFile)) {
            Log.e(TAG, "Failed to move " + temporaryFile + " into place");

            temporaryFile.delete();

            return false;
        }

        return true;
    }

    private boolean copyAssetTree(AssetManager assetManager, String assetPath, File destination) {
        String[] children;

        try {
            children = assetManager.list(assetPath);
        } catch (IOException e) {
            Log.e(TAG, "Failed to list assets under " + assetPath, e);

            return false;
        }

        // An empty listing means the asset is a file rather than a directory.
        if (children == null || children.length == 0) {
            return copyAssetFile(assetManager, assetPath, destination);
        }

        if (!destination.isDirectory() && !destination.mkdirs()) {
            Log.e(TAG, "Failed to create directory " + destination);

            return false;
        }

        boolean success = true;

        for (String child : children) {
            success &= copyAssetTree(assetManager, assetPath + "/" + child, new File(destination, child));
        }

        return success;
    }

    // Copy minimal engine cache included in the apk to the dest cache directory.
    private void installBundledCache(File downloadedDir)  throws Exception {
        AssetManager assetManager = getAssets();

        File cacheDirectory = new File(downloadedDir.toString() + "/Cache");
        File contentDirectory = new File(downloadedDir.toString() + "/Content");

        String[] bundledCacheEntries = assetManager.list(BUNDLED_CACHE_ASSET_PATH);
        String[] bundledContentEntries = assetManager.list(BUNDLED_CONTENT_ASSET_PATH);

        if ((bundledCacheEntries == null || bundledCacheEntries.length == 0)
            && (bundledContentEntries == null || bundledContentEntries.length == 0)) {
            
            Log.w(TAG, "No assets to copy? Num cache entries: "
                    + (bundledCacheEntries != null ? bundledCacheEntries.length : 0) + ", num content entries: "
                    + (bundledContentEntries != null ? bundledContentEntries.length : 0));
            
            return;
        }

        File stampFile = new File(downloadedDir, INSTALLED_CACHE_STAMP_NAME);
        String stamp = String.valueOf(getPackageInstallTime());

        if (stamp.equals(readInstalledCacheStamp(stampFile))) {
            Log.i(TAG, "Bundled cache up to date");

            return;
        }

        long startTime = System.currentTimeMillis();

        if (!copyAssetTree(assetManager, BUNDLED_CONTENT_ASSET_PATH, contentDirectory)) {
            throw new Exception("Failed to install the engine assets! (content)");
        }

        if (!copyAssetTree(assetManager, BUNDLED_CACHE_ASSET_PATH, cacheDirectory)) {
            throw new Exception("Failed to install the engine assets! (cache)");
        }

        writeInstalledCacheStamp(stampFile, stamp);

        Log.i(TAG, "Installed bundled cache in " + (System.currentTimeMillis() - startTime) + " ms");
    }

    private void runEngineLoop() {
        Log.i(TAG, "Hyperion runEngineLoop()");

        File downloadedDir;
        File downloadedContentDir;
        File downloadedCacheDir;

        try {
            downloadedDir = getDataDirectory("/downloaded_content");
            if (!downloadedDir.isDirectory() && !downloadedDir.mkdir()) {
                throw new Exception("Failed to create downloaded content dir");
            }

            downloadedCacheDir = new File(downloadedDir + "/Cache");
            if (!downloadedCacheDir.isDirectory() && !downloadedCacheDir.mkdirs()) {
                throw new Exception("Failed to create cache dir");
            }

            downloadedContentDir = new File(downloadedDir + "/Content");
            if (!downloadedContentDir.isDirectory() && !downloadedContentDir.mkdirs()) {
                throw new Exception("Failed to create content dir");
            }

            Log.i(TAG, "Installing minimal cache to: " + downloadedDir);

            installBundledCache(downloadedDir);
        } catch (Exception e) {
            Log.e(TAG, "Failed to install bundled cache", e);

            runOnUiThread(() -> {
                new AlertDialog.Builder(this)
                    .setTitle("Hyperion")
                    .setMessage("Failed to install bundled cache. Please reinstall the app.\n\nError message was: " + e.getMessage())
                    .setPositiveButton(android.R.string.ok, (dialog, which) -> finish())
                    .setCancelable(false)
                    .show();
            });

            return;
        }

        HyperionBridge.nativeSetCacheDirectory(downloadedCacheDir.getAbsolutePath());
        HyperionBridge.nativeSetContentDirectory(downloadedContentDir.getAbsolutePath());

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
}
