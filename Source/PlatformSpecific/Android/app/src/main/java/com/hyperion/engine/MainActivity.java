package com.hyperion.engine;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.widget.LinearLayout;
import android.widget.TextView;

public class MainActivity extends Activity {

    private static final String TAG = "HyperionMain";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        String statusText;
        int statusColor;

        try {
            int result = HyperionBridge.nativeInit();

            if (result != 0) {
                statusText = "Hyp_Initialize OK (returned " + result + ")";
                statusColor = Color.rgb(100, 220, 100);
                Log.i(TAG, statusText);
            } else {
                statusText = "Hyp_Initialize FAILED (returned 0)";
                statusColor = Color.rgb(220, 100, 100);
                Log.e(TAG, statusText);
            }
        } catch (UnsatisfiedLinkError e) {
            statusText = "Native library load failed: " + e.getMessage();
            statusColor = Color.rgb(220, 100, 100);
            Log.e(TAG, statusText, e);
        }

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setBackgroundColor(Color.rgb(18, 22, 34));

        TextView title = new TextView(this);
        title.setText("Hyperion Engine – Android");
        title.setTextColor(Color.WHITE);
        title.setTextSize(22f);
        title.setGravity(Gravity.CENTER);
        title.setPadding(24, 24, 24, 16);

        TextView status = new TextView(this);
        status.setText(statusText);
        status.setTextColor(statusColor);
        status.setTextSize(14f);
        status.setGravity(Gravity.CENTER);
        status.setPadding(24, 0, 24, 24);

        root.addView(title);
        root.addView(status);

        setContentView(root);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        try {
            HyperionBridge.nativeShutdown();
        } catch (Exception e) {
            Log.e(TAG, "Shutdown failed", e);
        }
    }
}
