package org.blender.blender;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.View;

/** MotionCanvas-style Project Grease UI over the real Blender surface. */
public final class ProjectGreaseOverlayView extends View {
  private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
  private boolean home = true, tools = true, layers = true, timeline = true, top = true;
  private boolean onion, view3d, scrolling;
  private int selectedTool, frame = 1, frameCount = 48;
  private float leftScroll, rightScroll, timelineScroll, lastX, lastY;

  private final String[] toolNames = {
      "Brush","Eraser","Select","Lasso","Fill","Text","Picker",
      "Pan","Arrow","Shape","Undo","Redo","Brushes","Color"
  };

  public ProjectGreaseOverlayView(Context context) {
    super(context);
    setBackgroundColor(Color.TRANSPARENT);
    setClickable(true);
  }

  private void rect(Canvas c, int color, float l, float t, float r, float b, float rad) {
    p.setStyle(Paint.Style.FILL); p.setColor(color);
    c.drawRoundRect(new RectF(l,t,r,b),rad,rad,p);
  }
  private void line(Canvas c, int color, float l, float t, float r, float b, float rad) {
    p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(1); p.setColor(color);
    c.drawRoundRect(new RectF(l,t,r,b),rad,rad,p); p.setStyle(Paint.Style.FILL);
  }
  private void tx(Canvas c,String s,float x,float y,float size,int color) {
    p.setStyle(Paint.Style.FILL); p.setTextSize(size); p.setColor(color); c.drawText(s,x,y,p);
  }
  private void btn(Canvas c,float l,float t,float r,float b,String s,boolean active) {
    rect(c,active?0xFFFFE4ED:Color.WHITE,l,t,r,b,10);
    line(c,active?0xFFF64F83:0xFFE5E5E8,l,t,r,b,10);
    tx(c,s,l+9,t+(b-t)*.64f,11,active?0xFFF64F83:0xFF303238);
  }

  @Override protected void onDraw(Canvas c) {
    super.onDraw(c);
    float w=getWidth(), h=getHeight();

    if (home) {
      rect(c,0xF9FFFFFF,0,0,w,h,0);
      tx(c,"Project Grease",32,72,30,0xFF303238);
      tx(c,"Blender Grease Pencil animation",32,101,14,0xFF777B84);
      rect(c,0xFFF64F83,32,135,w-32,188,14);
      tx(c,"+  New Project",52,168,16,Color.WHITE);
      btn(c,32,198,w-32,251,"Open Project",false);
      tx(c,"RECENT PROJECTS",32,294,11,0xFF777B84);
      tx(c,"My Grease Animation",32,327,15,0xFF303238);
      tx(c,"Untitled Project",32,366,15,0xFF303238);
      return;
    }

    if (top) {
      rect(c,Color.WHITE,0,0,w,58,0);
      p.setColor(0xFFE5E5E8); c.drawRect(0,57,w,58,p);
      tx(c,"×",16,38,30,0xFF303238);
      tx(c,"Project Grease",56,35,17,0xFF303238);
      btn(c,190,9,228,49,"−",false); tx(c,"100%",238,35,13,0xFF303238);
      btn(c,282,9,320,49,"+",false);
      btn(c,328,9,410,49,"Onion",onion);
      btn(c,416,9,458,49,"↶",false); btn(c,464,9,506,49,"↷",false);
      btn(c,512,9,565,49,"2D",!view3d); btn(c,571,9,624,49,"3D",view3d);
    }

    float bottom = timeline ? h-174 : h;
    if (tools) {
      float t=top?66:8, y=t+8-leftScroll;
      rect(c,Color.WHITE,0,t,74,bottom,0);
      for(int i=0;i<toolNames.length;i++,y+=56) if(y>t-55&&y<bottom) {
        boolean a=i==selectedTool;
        rect(c,a?0xFFF64F83:Color.WHITE,7,y,67,y+50,12);
        line(c,a?0xFFF64F83:0xFFE5E5E8,7,y,67,y+50,12);
        tx(c,toolNames[i],13,y+31,10,a?Color.WHITE:0xFF303238);
      }
    }

    if (layers && w>620) {
      float l=w-285;
      rect(c,Color.WHITE,l,58,w,bottom,0);
      tx(c,"BRUSH",l+14,86-rightScroll,12,0xFF303238);
      btn(c,l+12,96-rightScroll,l+86,132-rightScroll,"Pencil",false);
      btn(c,l+92,96-rightScroll,l+162,132-rightScroll,"Pen",false);
      tx(c,"Stroke Width",l+14,160-rightScroll,11,0xFF777B84);
      tx(c,"Opacity",l+14,202-rightScroll,11,0xFF777B84);
      tx(c,"LAYERS",l+14,252-rightScroll,12,0xFF303238);
      layer(c,l+12,265-rightScroll,"Layer 1",true);
      layer(c,l+12,310-rightScroll,"Layer 2",false);
      btn(c,l+12,358-rightScroll,w-12,398-rightScroll,"+ Add Layer",false);
      tx(c,"MATERIALS",l+14,435-rightScroll,12,0xFF303238);
      tx(c,"REFERENCE",l+14,510-rightScroll,12,0xFF303238);
      tx(c,"ADVANCED",l+14,568-rightScroll,12,0xFF303238);
      tx(c,"AUDIO",l+14,626-rightScroll,12,0xFF303238);
    }

    // Always-visible panel toggles, positioned outside the drawing center.
    if (!home) {
      btn(c,2,62,70,104,"T",tools);
      if(w>620) btn(c,w-70,62,w-2,104,"L",layers);
      if(timeline) btn(c,w-82,h-174,w-2,h-132,"TL",true);
      else btn(c,w-82,h-48,w-2,h-6,"TL",false);
    }

    if (timeline) {
      float t=h-174;
      rect(c,Color.WHITE,0,t,w,h,0);
      p.setColor(0xFFE5E5E8); c.drawRect(0,t,w,t+1,p);
      tx(c,"Timeline",12,t+32,16,0xFF303238);
      btn(c,86,t+8,122,t+45,"‹",false);
      tx(c,"Frame "+frame,130,t+31,13,0xFF303238);
      btn(c,190,t+8,226,t+45,"›",false);
      btn(c,234,t+8,278,t+45,"▶",false);
      btn(c,284,t+8,338,t+45,"Loop",false);
      btn(c,344,t+8,390,t+45,"+",false);
      float x=10-timelineScroll;
      for(int i=1;i<=frameCount;i++,x+=63) if(x+58>=0&&x<=w) {
        rect(c,i==frame?0xFFFFE4ED:0xFFFAFAFA,x,t+57,x+58,t+111,7);
        line(c,i==frame?0xFFF64F83:0xFFE5E5E8,x,t+57,x+58,t+111,7);
        tx(c,""+i,x+23,t+90,11,i==frame?0xFFF64F83:0xFF303238);
      }
      tx(c,"FPS 24  •  Onion Skin  •  Motion Trails  •  Audio",12,h-27,11,0xFF777B84);
    }
  }

  private void layer(Canvas c,float x,float y,String name,boolean active) {
    rect(c,active?0xFFFFF2F6:Color.WHITE,x,y,x+250,y+38,9);
    line(c,active?0xFFF64F83:0xFFE5E5E8,x,y,x+250,y+38,9);
    tx(c,name,x+52,y+24,12,0xFF303238); tx(c,"100%",x+202,y+24,10,0xFF777B84);
  }

  @Override public boolean onTouchEvent(MotionEvent e) {
    float x=e.getX(), y=e.getY(), h=getHeight(), w=getWidth();
    int a=e.getActionMasked();
    float timelineTop=h-174;

    if(home) {
      if(a==MotionEvent.ACTION_UP&&y>=125&&y<=255){home=false;invalidate();}
      return true;
    }

    if(a==MotionEvent.ACTION_DOWN){lastX=x;lastY=y;scrolling=false;}

    // Explicit hide/unhide buttons.
    if(a==MotionEvent.ACTION_UP&&y>=62&&y<=104&&x<=74){tools=!tools;invalidate();return true;}
    if(a==MotionEvent.ACTION_UP&&y>=62&&y<=104&&x>=w-74){layers=!layers;invalidate();return true;}
    if(a==MotionEvent.ACTION_UP&&x>=w-90&&(timeline&&y>=timelineTop&&y<timelineTop+48||!timeline&&y>h-55)){
      timeline=!timeline;invalidate();return true;
    }

    if(timeline&&y>=timelineTop){
      if(a==MotionEvent.ACTION_MOVE){timelineScroll=Math.max(0,timelineScroll-(x-lastX));lastX=x;invalidate();return true;}
      if(a==MotionEvent.ACTION_UP){
        if(y<timelineTop+55&&x>=86&&x<=122)frame=Math.max(1,frame-1);
        else if(y<timelineTop+55&&x>=190&&x<=226)frame=Math.min(frameCount,frame+1);
        else if(y<timelineTop+55&&x>=344&&x<=390)frameCount++;
        else if(!scrolling&&y>=timelineTop+55)frame=Math.max(1,Math.min(frameCount,1+(int)((x+timelineScroll-10)/63)));
        invalidate();return true;
      }
      return true;
    }

    if(tools&&x<74&&y>=108&&y<(timeline?timelineTop:h)){
      if(a==MotionEvent.ACTION_MOVE){leftScroll=Math.max(0,leftScroll-(y-lastY));lastY=y;scrolling=true;invalidate();return true;}
      if(a==MotionEvent.ACTION_UP&&!scrolling){int i=(int)((y-(top?66:8)-8+leftScroll)/56);if(i>=0&&i<toolNames.length)selectedTool=i;invalidate();return true;}
      return true;
    }

    if(layers&&w>620&&x>w-285){
      if(a==MotionEvent.ACTION_MOVE){rightScroll=Math.max(0,rightScroll-(y-lastY));lastY=y;scrolling=true;invalidate();return true;}
      return true;
    }

    if(top&&y<58&&a==MotionEvent.ACTION_UP){
      if(x<50)home=true;
      else if(x>=328&&x<=410)onion=!onion;
      else if(x>=512&&x<=565)view3d=false;
      else if(x>=571&&x<=624)view3d=true;
      invalidate();return true;
    }

    // Center: do not consume the event. The underlying Blender NativeActivity
    // InputView/GHOST path receives the original Android MotionEvent. This is
    // deliberate: Project Grease does not create a second input pipeline.
    return false;
  }
}
