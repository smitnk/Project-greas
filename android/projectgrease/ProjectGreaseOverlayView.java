package com.smitnk.projectgrease;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.View;

/**
 * MotionCanvas-style Project Grease UI.
 *
 * Important: this View never draws an artboard, grid, stroke, or placeholder
 * canvas. Blender owns the real drawing surface behind this transparent UI.
 */
public final class ProjectGreaseOverlayView extends View {
  public interface BlenderTouchForwarder {
    void send(int action, float x, float y, float pressure, int toolType, int metaState);
  }

  private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
  private BlenderTouchForwarder forwarder;

  private boolean home = true;
  private boolean showTools = true;
  private boolean showLayers = true;
  private boolean showTimeline = true;
  private boolean showTop = true;
  private boolean onion = false;
  private boolean viewport3d = false;
  private int selectedTool = 0;
  private int frame = 1;
  private int frameCount = 48;
  private float leftScroll = 0f;
  private float rightScroll = 0f;
  private float timelineScroll = 0f;
  private float lastX;
  private float lastY;
  private boolean scrolling;

  private final String[] tools = {
      "Brush", "Eraser", "Select", "Lasso", "Fill", "Text",
      "Picker", "Pan", "Arrow", "Shape", "Undo", "Redo", "Brushes", "Color"
  };

  public ProjectGreaseOverlayView(Context context) {
    super(context);
    setBackgroundColor(Color.TRANSPARENT);
    setClickable(true);
    setFocusable(false);
  }

  public void setBlenderTouchForwarder(BlenderTouchForwarder forwarder) {
    this.forwarder = forwarder;
  }

  private void fill(Canvas c, int color, float l, float t, float r, float b, float radius) {
    paint.setStyle(Paint.Style.FILL);
    paint.setColor(color);
    c.drawRoundRect(new RectF(l, t, r, b), radius, radius, paint);
  }

  private void outline(Canvas c, int color, float l, float t, float r, float b, float radius) {
    paint.setStyle(Paint.Style.STROKE);
    paint.setStrokeWidth(1f);
    paint.setColor(color);
    c.drawRoundRect(new RectF(l, t, r, b), radius, radius, paint);
    paint.setStyle(Paint.Style.FILL);
  }

  private void text(Canvas c, String value, float x, float y, float size, int color) {
    paint.setStyle(Paint.Style.FILL);
    paint.setTextSize(size);
    paint.setColor(color);
    c.drawText(value, x, y, paint);
  }

  private void button(Canvas c, float l, float t, float r, float b, String label, boolean active) {
    fill(c, active ? 0xFFFFE4ED : Color.WHITE, l, t, r, b, 10);
    outline(c, active ? 0xFFF64F83 : 0xFFE5E5E8, l, t, r, b, 10);
    text(c, label, l + 10, t + (b - t) * .64f, 12,
        active ? 0xFFF64F83 : 0xFF303238);
  }

  @Override
  protected void onDraw(Canvas c) {
    super.onDraw(c);
    final float w = getWidth();
    final float h = getHeight();

    if (home) {
      fill(c, 0xF9FFFFFF, 0, 0, w, h, 0);
      text(c, "Project Grease", 32, 72, 30, 0xFF303238);
      text(c, "Blender Grease Pencil animation", 32, 101, 14, 0xFF777B84);
      fill(c, 0xFFF64F83, 32, 135, w - 32, 188, 14);
      text(c, "+  New Project", 52, 168, 16, Color.WHITE);
      button(c, 32, 198, w - 32, 251, "Open Project", false);
      text(c, "RECENT PROJECTS", 32, 294, 11, 0xFF777B84);
      text(c, "My Grease Animation", 32, 327, 15, 0xFF303238);
      text(c, "Untitled Project", 32, 366, 15, 0xFF303238);
      return;
    }

    if (showTop) {
      fill(c, Color.WHITE, 0, 0, w, 58, 0);
      paint.setColor(0xFFE5E5E8);
      c.drawRect(0, 57, w, 58, paint);
      text(c, "×", 16, 38, 30, 0xFF303238);
      text(c, "Project Grease", 56, 35, 17, 0xFF303238);
      button(c, 190, 9, 228, 49, "−", false);
      text(c, "100%", 238, 35, 13, 0xFF303238);
      button(c, 282, 9, 320, 49, "+", false);
      button(c, 328, 9, 410, 49, "Onion", onion);
      button(c, 416, 9, 458, 49, "↶", false);
      button(c, 464, 9, 506, 49, "↷", false);
      button(c, 512, 9, 565, 49, "2D", !viewport3d);
      button(c, 571, 9, 624, 49, "3D", viewport3d);
    }

    if (showTools) {
      final float top = showTop ? 66 : 8;
      final float bottom = showTimeline ? h - 174 : h;
      fill(c, Color.WHITE, 0, top, 74, bottom, 0);
      float y = top + 8 - leftScroll;
      for (int i = 0; i < tools.length; i++) {
        if (y > top - 55 && y < bottom) {
          boolean active = i == selectedTool;
          fill(c, active ? 0xFFF64F83 : Color.WHITE, 7, y, 67, y + 50, 12);
          outline(c, active ? 0xFFF64F83 : 0xFFE5E5E8, 7, y, 67, y + 50, 12);
          text(c, tools[i], 13, y + 31, 10, active ? Color.WHITE : 0xFF303238);
        }
        y += 56;
      }
    }

    if (showLayers && w > 620) {
      final float left = w - 285;
      final float bottom = showTimeline ? h - 174 : h;
      fill(c, Color.WHITE, left, 58, w, bottom, 0);
      text(c, "BRUSH", left + 14, 86 - rightScroll, 12, 0xFF303238);
      button(c, left + 12, 96 - rightScroll, left + 86, 132 - rightScroll, "Pencil", false);
      button(c, left + 92, 96 - rightScroll, left + 162, 132 - rightScroll, "Pen", false);
      text(c, "Stroke Width", left + 14, 160 - rightScroll, 11, 0xFF777B84);
      text(c, "Opacity", left + 14, 202 - rightScroll, 11, 0xFF777B84);
      text(c, "LAYERS", left + 14, 252 - rightScroll, 12, 0xFF303238);
      layer(c, left + 12, 265 - rightScroll, "Layer 1", true);
      layer(c, left + 12, 310 - rightScroll, "Layer 2", false);
      button(c, left + 12, 358 - rightScroll, w - 12, 398 - rightScroll, "+ Add Layer", false);
      text(c, "MATERIALS", left + 14, 435 - rightScroll, 12, 0xFF303238);
      text(c, "Grease Pencil Material", left + 14, 462 - rightScroll, 12, 0xFF777B84);
      text(c, "REFERENCE", left + 14, 510 - rightScroll, 12, 0xFF303238);
      text(c, "ADVANCED", left + 14, 568 - rightScroll, 12, 0xFF303238);
      text(c, "AUDIO", left + 14, 626 - rightScroll, 12, 0xFF303238);
    }

    if (showTimeline) {
      final float top = h - 174;
      fill(c, Color.WHITE, 0, top, w, h, 0);
      paint.setColor(0xFFE5E5E8);
      c.drawRect(0, top, w, top + 1, paint);
      text(c, "Timeline", 12, top + 32, 16, 0xFF303238);
      button(c, 86, top + 8, 122, top + 45, "‹", false);
      text(c, "Frame " + frame, 130, top + 31, 13, 0xFF303238);
      button(c, 190, top + 8, 226, top + 45, "›", false);
      button(c, 234, top + 8, 278, top + 45, "▶", false);
      button(c, 284, top + 8, 338, top + 45, "Loop", false);
      button(c, 344, top + 8, 390, top + 45, "+", false);

      float x = 10 - timelineScroll;
      for (int i = 1; i <= frameCount; i++) {
        if (x + 58 >= 0 && x <= w) {
          fill(c, i == frame ? 0xFFFFE4ED : 0xFFFAFAFA,
              x, top + 57, x + 58, top + 111, 7);
          outline(c, i == frame ? 0xFFF64F83 : 0xFFE5E5E8,
              x, top + 57, x + 58, top + 111, 7);
          text(c, String.valueOf(i), x + 23, top + 90, 11,
              i == frame ? 0xFFF64F83 : 0xFF303238);
        }
        x += 63;
      }
      text(c, "FPS 24  •  Onion Skin  •  Motion Trails  •  Audio",
          12, h - 27, 11, 0xFF777B84);
    }
  }

  private void layer(Canvas c, float x, float y, String name, boolean active) {
    fill(c, active ? 0xFFFFF2F6 : Color.WHITE, x, y, x + 250, y + 38, 9);
    outline(c, active ? 0xFFF64F83 : 0xFFE5E5E8, x, y, x + 250, y + 38, 9);
    text(c, name, x + 52, y + 24, 12, 0xFF303238);
    text(c, "100%", x + 202, y + 24, 10, 0xFF777B84);
  }

  @Override
  public boolean onTouchEvent(MotionEvent e) {
    final float x = e.getX();
    final float y = e.getY();
    final int action = e.getActionMasked();
    final float h = getHeight();
    final float timelineTop = h - 174;

    if (home) {
      if (action == MotionEvent.ACTION_UP && y >= 125 && y <= 255) {
        home = false;
        invalidate();
        return true;
      }
      return true;
    }

    if (action == MotionEvent.ACTION_DOWN) {
      lastX = x;
      lastY = y;
      scrolling = false;
    }

    if (showTimeline && y >= timelineTop) {
      if (action == MotionEvent.ACTION_MOVE) {
        timelineScroll = Math.max(0, timelineScroll - (x - lastX));
        lastX = x;
        scrolling = true;
        invalidate();
        return true;
      }
      if (action == MotionEvent.ACTION_UP) {
        if (y < timelineTop + 55 && x >= 86 && x <= 122) {
          frame = Math.max(1, frame - 1);
        }
        else if (y < timelineTop + 55 && x >= 190 && x <= 226) {
          frame = Math.min(frameCount, frame + 1);
        }
        else if (y < timelineTop + 55 && x >= 344 && x <= 390) {
          frameCount++;
        }
        else if (!scrolling && y >= timelineTop + 55) {
          int n = 1 + (int)((x + timelineScroll - 10) / 63);
          frame = Math.max(1, Math.min(frameCount, n));
        }
        invalidate();
        return true;
      }
      return true;
    }

    if (showTools && x < 74 && y >= 58) {
      if (action == MotionEvent.ACTION_MOVE) {
        leftScroll = Math.max(0, leftScroll - (y - lastY));
        lastY = y;
        scrolling = true;
        invalidate();
        return true;
      }
      if (action == MotionEvent.ACTION_UP && !scrolling) {
        int index = (int)((y - 74 + leftScroll) / 56);
        if (index >= 0 && index < tools.length) selectedTool = index;
        invalidate();
        return true;
      }
      return true;
    }

    if (showLayers && getWidth() > 620 && x > getWidth() - 285) {
      if (action == MotionEvent.ACTION_MOVE) {
        rightScroll = Math.max(0, rightScroll - (y - lastY));
        lastY = y;
        scrolling = true;
        invalidate();
        return true;
      }
      return true;
    }

    if (showTop && y < 58) {
      if (action == MotionEvent.ACTION_UP) {
        if (x < 50) home = true;
        else if (x >= 328 && x <= 410) onion = !onion;
        else if (x >= 512 && x <= 565) viewport3d = false;
        else if (x >= 571 && x <= 624) viewport3d = true;
        else if (x >= 416 && x <= 458) showTools = !showTools;
        else if (x >= 464 && x <= 506) showLayers = !showLayers;
        else if (x >= 190 && x <= 228) showTimeline = !showTimeline;
        invalidate();
        return true;
      }
      return true;
    }

    // The center has no UI background and is never painted by this class.
    // Forward the original coordinates and pressure to Blender's native GHOST bridge.
    if (forwarder != null) {
      forwarder.send(action, x, y, e.getPressure(), e.getToolType(0), e.getMetaState());
    }
    return true;
  }
}
