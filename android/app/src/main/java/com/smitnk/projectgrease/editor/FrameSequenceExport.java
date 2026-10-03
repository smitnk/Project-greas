package com.smitnk.projectgrease.editor;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Animation export planning (Blender renders frame_start..frame_end, honouring holds: a frame
 * without its own keyframe shows the previous keyframe). Produces the list of scene frames to render
 * and their file names (PNG sequence: name_0001.png ...), and which keyframe each frame shows.
 */
public final class FrameSequenceExport {
    public static final class Item {
        public final int frame, shownKeyframe; public final String fileName;
        Item(int frame, int shown, String name) { this.frame = frame; this.shownKeyframe = shown; this.fileName = name; }
    }
    private FrameSequenceExport() {}

    /** keys: sorted keyframe numbers of the drawing; frames before the first key show nothing (-1). */
    public static List<Item> plan(int[] keys, int start, int end, String baseName, String ext) {
        if (start > end || start < 0) throw new IllegalArgumentException("range");
        List<Item> out = new ArrayList<>();
        int digits = Math.max(4, String.valueOf(end).length());
        for (int f = start; f <= end; f++) {
            int shown = -1;
            for (int k : keys) if (k <= f) shown = k;
            out.add(new Item(f, shown, String.format(Locale.ROOT, "%s_%0" + digits + "d.%s", baseName, f, ext)));
        }
        return out;
    }
}
