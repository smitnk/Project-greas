package com.smitnk.projectgrease.editor;

import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Per-project settings (canvas size, fps, frame range, background), saved inside the project file. */
public final class ProjectSettings {
    public int width = 1920, height = 1080, fps = 24, frameStart = 1, frameEnd = 250;
    public int background = 0xFFFFFFFF;
    public boolean transparentBackground = false;

    public String validate() {
        if (width < 16 || height < 16 || width > 8192 || height > 8192) return "Canvas must be 16..8192 px";
        if (fps < 1 || fps > 120) return "FPS must be 1..120";
        if (frameStart < 0 || frameEnd < frameStart) return "Frame range is invalid";
        return null;
    }

    public String toJson() {
        return String.format(Locale.ROOT,
            "{\"width\":%d,\"height\":%d,\"fps\":%d,\"frameStart\":%d,\"frameEnd\":%d,\"background\":\"#%08X\",\"transparentBackground\":%b}",
            width, height, fps, frameStart, frameEnd, background, transparentBackground);
    }

    /** Lenient parse; missing keys keep defaults (older project files). */
    public static ProjectSettings fromJson(String json) {
        ProjectSettings s = new ProjectSettings();
        if (json == null) return s;
        s.width = intOr(json, "width", s.width);
        s.height = intOr(json, "height", s.height);
        s.fps = intOr(json, "fps", s.fps);
        s.frameStart = intOr(json, "frameStart", s.frameStart);
        s.frameEnd = intOr(json, "frameEnd", s.frameEnd);
        Matcher bg = Pattern.compile("\"background\"\\s*:\\s*\"#([0-9A-Fa-f]{8})\"").matcher(json);
        if (bg.find()) s.background = (int) Long.parseLong(bg.group(1), 16);
        Matcher t = Pattern.compile("\"transparentBackground\"\\s*:\\s*(true|false)").matcher(json);
        if (t.find()) s.transparentBackground = Boolean.parseBoolean(t.group(1));
        return s;
    }

    private static int intOr(String json, String key, int fallback) {
        Matcher m = Pattern.compile("\"" + key + "\"\\s*:\\s*(-?\\d+)").matcher(json);
        return m.find() ? Integer.parseInt(m.group(1)) : fallback;
    }
}
