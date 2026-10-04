package com.smitnk.projectgrease.editor;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * SVG import for Project Grease (behaviour of Blender's io "Import SVG" for Grease Pencil, which
 * turns every SVG shape into a stroke, flattening curves).
 * Supported: path (M L H V C S Q T Z, relative and absolute), polyline, polygon, line, rect,
 * circle, ellipse; stroke / fill colors (#rgb, #rrggbb, rgb()), stroke-width. Not supported:
 * transforms, arcs (A is drawn as a straight line to its end point), gradients, CSS stylesheets.
 */
public final class SvgImport {
    public static final class Stroke {
        public final List<float[]> points = new ArrayList<>();
        public boolean closed;
        public int strokeArgb = 0xFF000000;
        public Integer fillArgb; // null = no fill
        public float width = 1f;
    }

    public static final int CURVE_SEGMENTS = 12;
    public static final int CIRCLE_SEGMENTS = 32;

    private static final Pattern ELEMENT = Pattern.compile("<(path|polyline|polygon|line|rect|circle|ellipse)\\b([^>]*)>", Pattern.CASE_INSENSITIVE);
    private static final Pattern ATTR = Pattern.compile("([a-zA-Z_:][a-zA-Z0-9_:.-]*)\\s*=\\s*\"([^\"]*)\"");
    private static final Pattern NUMBER = Pattern.compile("[-+]?(?:\\d+\\.?\\d*|\\.\\d+)(?:[eE][-+]?\\d+)?");

    private SvgImport() {}

    public static List<Stroke> parse(String svg) {
        List<Stroke> out = new ArrayList<>();
        Matcher m = ELEMENT.matcher(svg);
        while (m.find()) {
            String tag = m.group(1).toLowerCase(Locale.ROOT);
            java.util.Map<String, String> a = attrs(m.group(2));
            List<Stroke> made = new ArrayList<>();
            switch (tag) {
                case "path": made.addAll(path(a.getOrDefault("d", ""))); break;
                case "polyline": case "polygon": {
                    Stroke s = new Stroke();
                    float[] n = numbers(a.getOrDefault("points", ""));
                    for (int i = 0; i + 1 < n.length; i += 2) s.points.add(new float[]{n[i], n[i + 1]});
                    s.closed = tag.equals("polygon");
                    made.add(s);
                    break;
                }
                case "line": {
                    Stroke s = new Stroke();
                    s.points.add(new float[]{f(a, "x1"), f(a, "y1")});
                    s.points.add(new float[]{f(a, "x2"), f(a, "y2")});
                    made.add(s);
                    break;
                }
                case "rect": {
                    float x = f(a, "x"), y = f(a, "y"), w = f(a, "width"), h = f(a, "height");
                    Stroke s = new Stroke();
                    s.points.add(new float[]{x, y}); s.points.add(new float[]{x + w, y});
                    s.points.add(new float[]{x + w, y + h}); s.points.add(new float[]{x, y + h});
                    s.closed = true;
                    made.add(s);
                    break;
                }
                case "circle": case "ellipse": {
                    float cx = f(a, "cx"), cy = f(a, "cy");
                    float rx = tag.equals("circle") ? f(a, "r") : f(a, "rx");
                    float ry = tag.equals("circle") ? rx : f(a, "ry");
                    Stroke s = new Stroke();
                    for (int i = 0; i < CIRCLE_SEGMENTS; i++) {
                        double t = 2 * Math.PI * i / CIRCLE_SEGMENTS;
                        s.points.add(new float[]{cx + rx * (float) Math.cos(t), cy + ry * (float) Math.sin(t)});
                    }
                    s.closed = true;
                    made.add(s);
                    break;
                }
                default: break;
            }
            String style = a.getOrDefault("style", "");
            String stroke = styleOr(style, "stroke", a.get("stroke"));
            String fill = styleOr(style, "fill", a.get("fill"));
            String width = styleOr(style, "stroke-width", a.get("stroke-width"));
            for (Stroke s : made) {
                if (s.points.size() < 2) continue;
                Integer sc = color(stroke);
                if (sc != null) s.strokeArgb = sc;
                // SVG's default fill is black for closed shapes; only an explicit fill becomes a GP fill
                s.fillArgb = fill == null ? null : color(fill);
                if (width != null) {
                    float[] w = numbers(width);
                    if (w.length > 0 && w[0] > 0) s.width = w[0];
                }
                out.add(s);
            }
        }
        return out;
    }

    private static java.util.Map<String, String> attrs(String text) {
        java.util.Map<String, String> map = new java.util.HashMap<>();
        Matcher m = ATTR.matcher(text);
        while (m.find()) map.put(m.group(1).toLowerCase(Locale.ROOT), m.group(2));
        return map;
    }

    private static float f(java.util.Map<String, String> a, String key) {
        float[] n = numbers(a.getOrDefault(key, "0"));
        return n.length > 0 ? n[0] : 0f;
    }

    static float[] numbers(String text) {
        Matcher m = NUMBER.matcher(text);
        List<Float> list = new ArrayList<>();
        // A value outside the float range is an error: the list ends there (SVG 1.1 F.2), so a lone
        // bad attribute reads as its default (0) and a point list keeps its valid head.
        while (m.find()) {
            float v = Float.parseFloat(m.group());
            if (Float.isNaN(v) || Float.isInfinite(v)) break;
            list.add(v);
        }
        float[] r = new float[list.size()];
        for (int i = 0; i < r.length; i++) r[i] = list.get(i);
        return r;
    }

    private static String styleOr(String style, String key, String fallback) {
        for (String part : style.split(";")) {
            int c = part.indexOf(':');
            if (c > 0 && part.substring(0, c).trim().equalsIgnoreCase(key)) return part.substring(c + 1).trim();
        }
        return fallback;
    }

    /** #rgb, #rrggbb, rgb(r,g,b); "none" or unknown -> null. */
    static Integer color(String text) {
        if (text == null) return null;
        String t = text.trim().toLowerCase(Locale.ROOT);
        if (t.startsWith("#")) {
            String h = t.substring(1);
            if (h.length() == 3) h = "" + h.charAt(0) + h.charAt(0) + h.charAt(1) + h.charAt(1) + h.charAt(2) + h.charAt(2);
            if (h.length() != 6) return null;
            try { return 0xFF000000 | Integer.parseInt(h, 16); } catch (NumberFormatException e) { return null; }
        }
        if (t.startsWith("rgb(")) {
            float[] n = numbers(t);
            if (n.length < 3) return null;
            return 0xFF000000 | (clamp255(n[0]) << 16) | (clamp255(n[1]) << 8) | clamp255(n[2]);
        }
        switch (t) {
            case "black": return 0xFF000000;
            case "white": return 0xFFFFFFFF;
            case "red": return 0xFFFF0000;
            case "green": return 0xFF008000;
            case "blue": return 0xFF0000FF;
            default: return null;
        }
    }

    private static int clamp255(float v) { return Math.max(0, Math.min(255, Math.round(v))); }

    /** Path data to strokes; every M starts a new stroke, Z closes the current one. */
    static List<Stroke> path(String d) {
        List<Stroke> out = new ArrayList<>();
        Matcher tok = Pattern.compile("[MmLlHhVvCcSsQqTtAaZz]|" + NUMBER.pattern()).matcher(d);
        List<String> tokens = new ArrayList<>();
        while (tok.find()) tokens.add(tok.group());
        Stroke cur = null;
        float x = 0, y = 0, sx = 0, sy = 0, lcx = 0, lcy = 0;
        char cmd = 0, prev = 0;
        int i = 0;
        while (i < tokens.size()) {
            String t = tokens.get(i);
            if (Character.isLetter(t.charAt(0))) { cmd = t.charAt(0); i++; if (cmd == 'Z' || cmd == 'z') {
                if (cur != null) cur.closed = true;
                x = sx; y = sy; cur = null; prev = cmd; continue; } }
            boolean rel = Character.isLowerCase(cmd);
            char c = Character.toUpperCase(cmd);
            int need = c == 'H' || c == 'V' ? 1 : c == 'C' ? 6 : c == 'S' || c == 'Q' ? 4 : c == 'A' ? 7 : 2;
            if (i + need > tokens.size()) break;
            // SVG path error handling (SVG 1.1 F.2): the path is drawn up to the last complete segment
            // and the rest is ignored. A command letter where a number belongs or a value outside the
            // float range is such an error (a malformed file used to throw and close the app).
            float[] v = new float[need];
            boolean bad = false;
            for (int k = 0; k < need && !bad; k++) {
                String n = tokens.get(i + k);
                if (Character.isLetter(n.charAt(0))) { bad = true; break; }
                v[k] = Float.parseFloat(n);
                if (Float.isNaN(v[k]) || Float.isInfinite(v[k])) bad = true;
            }
            if (bad) break;
            i += need;
            float ox = rel ? x : 0, oy = rel ? y : 0;
            if (c != 'M' && cur == null) { // drawing after Z (or without M) starts at the current point
                cur = new Stroke(); out.add(cur); cur.points.add(new float[]{x, y});
            }
            switch (c) {
                case 'M':
                    cur = new Stroke(); out.add(cur);
                    x = ox + v[0]; y = oy + v[1]; sx = x; sy = y;
                    cur.points.add(new float[]{x, y});
                    cmd = rel ? 'l' : 'L'; // following pairs are line-tos
                    break;
                case 'L': x = ox + v[0]; y = oy + v[1]; pt(cur, x, y); break;
                case 'H': x = (rel ? x : 0) + v[0]; pt(cur, x, y); break;
                case 'V': y = (rel ? y : 0) + v[0]; pt(cur, x, y); break;
                case 'A': x = ox + v[5]; y = oy + v[6]; pt(cur, x, y); break;
                case 'C': case 'S': {
                    float c1x, c1y;
                    if (c == 'C') { c1x = ox + v[0]; c1y = oy + v[1]; }
                    else {
                        boolean smooth = "CcSs".indexOf(prev) >= 0;
                        c1x = smooth ? 2 * x - lcx : x; c1y = smooth ? 2 * y - lcy : y;
                    }
                    int o = c == 'C' ? 2 : 0;
                    float c2x = ox + v[o], c2y = oy + v[o + 1], ex = ox + v[o + 2], ey = oy + v[o + 3];
                    for (int s = 1; s <= CURVE_SEGMENTS; s++) {
                        float tt = (float) s / CURVE_SEGMENTS, u = 1 - tt;
                        pt(cur, u*u*u*x + 3*u*u*tt*c1x + 3*u*tt*tt*c2x + tt*tt*tt*ex,
                                      u*u*u*y + 3*u*u*tt*c1y + 3*u*tt*tt*c2y + tt*tt*tt*ey);
                    }
                    lcx = c2x; lcy = c2y; x = ex; y = ey;
                    break;
                }
                case 'Q': case 'T': {
                    float qx, qy;
                    if (c == 'Q') { qx = ox + v[0]; qy = oy + v[1]; }
                    else {
                        boolean smooth = "QqTt".indexOf(prev) >= 0;
                        qx = smooth ? 2 * x - lcx : x; qy = smooth ? 2 * y - lcy : y;
                    }
                    int o = c == 'Q' ? 2 : 0;
                    float ex = ox + v[o], ey = oy + v[o + 1];
                    for (int s = 1; s <= CURVE_SEGMENTS; s++) {
                        float tt = (float) s / CURVE_SEGMENTS, u = 1 - tt;
                        pt(cur, u*u*x + 2*u*tt*qx + tt*tt*ex, u*u*y + 2*u*tt*qy + tt*tt*ey);
                    }
                    lcx = qx; lcy = qy; x = ex; y = ey;
                    break;
                }
                default: break;
            }
            prev = cmd;
        }
        return out;
    }

    private static void pt(Stroke cur, float x, float y) {
        cur.points.add(new float[]{x, y});
    }
}
