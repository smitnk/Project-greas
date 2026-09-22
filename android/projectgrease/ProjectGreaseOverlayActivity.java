package com.smitnk.projectgrease;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;

/**
 * Transparent MotionCanvas-style control surface over Blender's NativeActivity.
 * The center remains transparent; drawing input is forwarded to Blender GHOST.
 */
public final class ProjectGreaseOverlayActivity extends Activity {
  static {
    System.loadLibrary("blender");
  }

  private static native void nativeProjectGreaseTouch(
      int action, float x, float y, float pressure, int toolType, int metaState);

  @Override
  protected void onCreate(Bundle state) {
    super.onCreate(state);
    Window w = getWindow();
    w.setBackgroundDrawableResource(android.R.color.transparent);
    w.setStatusBarColor(Color.TRANSPARENT);
    w.setNavigationBarColor(Color.TRANSPARENT);
    w.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
    w.getDecorView().setSystemUiVisibility(
        View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
            | View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);

    ProjectGreaseOverlayView view = new ProjectGreaseOverlayView(this);
    view.setBlenderTouchForwarder((action, x, y, pressure, toolType, meta) ->
        nativeProjectGreaseTouch(action, x, y, pressure, toolType, meta));
    setContentView(view);
  }

  @Override
  public boolean dispatchTouchEvent(MotionEvent event) {
    // ProjectGreaseOverlayView handles both UI regions and the transparent
    // drawing region. Do not let Activity dispatch create a second path.
    return super.dispatchTouchEvent(event);
  }

  @Override
  public void onBackPressed() {
    finishAffinity();
  }
}
