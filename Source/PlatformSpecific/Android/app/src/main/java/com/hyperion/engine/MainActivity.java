package com.hyperion.engine;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.content.res.AssetManager;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.Gravity;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

public class MainActivity extends Activity implements SurfaceHolder.Callback {

    private static final String TAG = "HyperionMain";

    private SurfaceView m_surfaceView;
    private boolean m_engineReady = false;

    private void initializeEngine() throws Exception {
        if (m_engineReady) {
            throw new Exception("Engine already initialized! Double init detected");
        }

        int result = HyperionBridge.nativeInit();

        if (result == 0) {
            throw new Exception("Failed to initialize Hyperion Engine");
        }

        m_engineReady = true;
    }

    private void shutdownEngine() {
        if (!m_engineReady) {
            return; // not init, fine
        }

        HyperionBridge.nativeShutdown();

        m_engineReady = false;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        String statusText;
        int statusColor;

        // call before Hyp_Initialize
        HyperionBridge.nativeSetAssetManager(getAssets());

        FrameLayout root = new FrameLayout(this);

        // Rendering surface
        m_surfaceView = new SurfaceView(this);
        m_surfaceView.getHolder().addCallback(this);
        root.addView(m_surfaceView);

        // Status overlay
        LinearLayout overlay = new LinearLayout(this);
        overlay.setOrientation(LinearLayout.VERTICAL);
        overlay.setGravity(Gravity.CENTER);

        TextView title = new TextView(this);
        title.setText("Hyperion Engine \u2013 Android");
        title.setTextColor(Color.WHITE);
        title.setTextSize(22f);
        title.setGravity(Gravity.CENTER);
        title.setPadding(24, 24, 24, 16);

        TextView status = new TextView(this);
        status.setText("Hi");
        status.setTextSize(14f);
        status.setGravity(Gravity.CENTER);
        status.setPadding(24, 0, 24, 24);

        overlay.addView(title);
        overlay.addView(status);
        root.addView(overlay);

        setContentView(root);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        HyperionBridge.nativeSetAssetManager(null);

        shutdownEngine();
    }

    // SurfaceHolder.Callback

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        boolean needsLaunchThreads = false;

        if (!m_engineReady) {
            needsLaunchThreads = true;

            try {
                initializeEngine();
            } catch (Exception ex) {
                Log.e("HyperionEngine", "Failed to initialize engine upon surface creation: " + ex.getMessage());
                return;
            }
        }

        HyperionBridge.nativeSetSurface(holder.getSurface());

        if (needsLaunchThreads) {
            HyperionBridge.nativeLaunchThreads();
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
