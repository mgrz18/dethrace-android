package com.dethrace.android;

import android.content.pm.ActivityInfo;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.FrameLayout;

import org.libsdl.app.SDLActivity;

import java.io.File;

public class DethraceActivity extends SDLActivity {

    private static final String TAG = "DethRace";

    private TouchOverlayView mOverlay;
    private final Handler mUiHandler = new Handler(Looper.getMainLooper());

    private native boolean nativeIsInRace();

    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL2",
            "main"
        };
    }

    @Override
    protected String getMainSharedObject() {
        String library;
        String[] libraries = getLibraries();
        if (libraries.length > 0) {
            library = "lib" + libraries[libraries.length - 1] + ".so";
        } else {
            library = "libmain.so";
        }
        return getContext().getApplicationInfo().nativeLibraryDir + "/" + library;
    }

    @Override
    protected String getMainFunction() {
        return "SDL_main";
    }

    // SDL queries this; force landscape regardless of game window aspect.
    @Override
    public void setOrientationBis(int w, int h, boolean resizable, String hint) {
        super.setOrientationBis(0, 0, resizable, "LandscapeRight LandscapeLeft");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        File extDir = getExternalFilesDir(null);
        String rootDir = (extDir != null) ? extDir.getAbsolutePath() : "/sdcard/dethrace";

        File f = new File(rootDir);
        if (!f.exists()) {
            f.mkdirs();
        }

        Log.i(TAG, "DETHRACE_ROOT_DIR=" + rootDir);

        try {
            android.system.Os.setenv("DETHRACE_ROOT_DIR", rootDir, true);
            android.system.Os.setenv("HOME", rootDir, true);
        } catch (Exception e) {
            Log.e(TAG, "Failed to setenv", e);
        }

        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

        super.onCreate(savedInstanceState);

        // Draw under cutout (notch / camera hole)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }

        applyImmersiveMode();

        // Overlay the touch-control hints on top of the SDL surface
        if (mLayout != null) {
            mOverlay = new TouchOverlayView(this);
            mOverlay.setVisibility(View.GONE);
            FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT);
            mLayout.addView(mOverlay, lp);
            mUiHandler.postDelayed(mPollRaceState, 500);
        }
    }

    private final Runnable mPollRaceState = new Runnable() {
        @Override
        public void run() {
            try {
                boolean racing = nativeIsInRace();
                if (mOverlay != null) {
                    int target = racing ? View.VISIBLE : View.GONE;
                    if (mOverlay.getVisibility() != target) {
                        mOverlay.setVisibility(target);
                    }
                }
            } catch (UnsatisfiedLinkError ignore) {
                // libmain.so not yet ready or symbol not exported
            } catch (Throwable ignore) {
            }
            mUiHandler.postDelayed(this, 250);
        }
    };

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            applyImmersiveMode();
        }
    }

    private void applyImmersiveMode() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            int flags = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
            getWindow().getDecorView().setSystemUiVisibility(flags);
        }
    }
}
