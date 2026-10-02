package com.smitnk.projectgrease.editor;

import java.util.ArrayList;
import java.util.List;

/**
 * Image trace for Project Grease. Blender's GPENCIL_OT_trace_image vectorizes with potrace; potrace is
 * not in the focused closure, so this traces the outlines of the dark (or bright) regions with Moore
 * neighbour boundary following and simplifies them with Ramer-Douglas-Peucker. Output: closed outlines
 * in image pixel coordinates (one per region boundary, holes included), largest first.
 */
public final class ImageTrace {
    private ImageTrace() {}

    /** Luminance (Rec. 601) below `threshold` (0..1) counts as ink; `invert` traces bright regions. */
    public static boolean[] mask(int[] argb, int width, int height, float threshold, boolean invert) {
        boolean[] m = new boolean[width * height];
        for (int i = 0; i < m.length; i++) {
            int c = argb[i];
            float a = ((c >>> 24) & 0xFF) / 255f;
            float lum = (0.299f * ((c >> 16) & 0xFF) + 0.587f * ((c >> 8) & 0xFF) + 0.114f * (c & 0xFF)) / 255f;
            lum = lum * a + (1 - a); // transparent pixels count as white
            m[i] = invert ? lum >= threshold : lum < threshold;
        }
        return m;
    }

    private static final int[] DX = {1, 1, 0, -1, -1, -1, 0, 1};
    private static final int[] DY = {0, 1, 1, 1, 0, -1, -1, -1};

    public static List<List<float[]>> trace(boolean[] m, int width, int height, float tolerance, int minPoints) {
        List<List<float[]>> out = new ArrayList<>();
        boolean[] done = new boolean[width * height];
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int i = y * width + x;
                if (!m[i] || done[i]) continue;
                boolean leftOutside = x == 0 || !m[i - 1];
                if (!leftOutside) continue; // start only on a left edge pixel
                List<int[]> contour = follow(m, width, height, x, y, done);
                if (contour.size() < minPoints) continue;
                List<float[]> pts = new ArrayList<>();
                for (int[] p : contour) pts.add(new float[]{p[0] + 0.5f, p[1] + 0.5f});
                List<float[]> simple = simplify(pts, tolerance);
                if (simple.size() >= 3) out.add(simple);
            }
        }
        out.sort((a, b) -> Integer.compare(b.size(), a.size()));
        return out;
    }

    private static boolean ink(boolean[] m, int w, int h, int x, int y) {
        return x >= 0 && y >= 0 && x < w && y < h && m[y * w + x];
    }

    /** Moore neighbour tracing (clockwise, y down) with Jacob's stopping criterion. */
    private static List<int[]> follow(boolean[] m, int w, int h, int sx, int sy, boolean[] done) {
        List<int[]> c = new ArrayList<>();
        int x = sx, y = sy;
        int dir = 0;            // we entered the start pixel moving east (its left neighbour is empty)
        int firstDir = -1;
        int guard = 4 * w * h + 8;
        while (guard-- > 0) {
            c.add(new int[]{x, y});
            done[y * w + x] = true;
            int start = (dir + 5) % 8; // one step clockwise past the backtrack pixel
            int next = -1;
            for (int k = 0; k < 8; k++) {
                int d = (start + k) % 8;
                if (ink(m, w, h, x + DX[d], y + DY[d])) { next = d; break; }
            }
            if (next < 0) break; // isolated pixel
            if (x == sx && y == sy) {
                if (firstDir < 0) firstDir = next;
                else if (next == firstDir) { c.remove(c.size() - 1); break; } // back at start, same way
            }
            x += DX[next]; y += DY[next]; dir = next;
        }
        return c;
    }

    /** Ramer-Douglas-Peucker on a closed outline. */
    public static List<float[]> simplify(List<float[]> pts, float tol) {
        if (pts.size() < 4 || tol <= 0) return pts;
        // split the ring at the point farthest from the first point
        int far = 0; float best = -1;
        for (int i = 1; i < pts.size(); i++) {
            float d = dist2(pts.get(0), pts.get(i));
            if (d > best) { best = d; far = i; }
        }
        List<float[]> a = rdp(pts.subList(0, far + 1), tol);
        List<float[]> ring = new ArrayList<>(pts.subList(far, pts.size()));
        ring.add(pts.get(0));
        List<float[]> b = rdp(ring, tol);
        List<float[]> out = new ArrayList<>(a);
        out.addAll(b.subList(1, b.size() - 1));
        return out;
    }

    private static List<float[]> rdp(List<float[]> p, float tol) {
        if (p.size() < 3) return new ArrayList<>(p);
        float[] s = p.get(0), e = p.get(p.size() - 1);
        int idx = -1; float best = tol * tol;
        for (int i = 1; i < p.size() - 1; i++) {
            float d = segDist2(p.get(i), s, e);
            if (d > best) { best = d; idx = i; }
        }
        List<float[]> out = new ArrayList<>();
        if (idx < 0) { out.add(s); out.add(e); return out; }
        List<float[]> l = rdp(p.subList(0, idx + 1), tol), r = rdp(p.subList(idx, p.size()), tol);
        out.addAll(l.subList(0, l.size() - 1));
        out.addAll(r);
        return out;
    }

    private static float dist2(float[] a, float[] b) { float dx = a[0] - b[0], dy = a[1] - b[1]; return dx * dx + dy * dy; }

    private static float segDist2(float[] p, float[] a, float[] b) {
        float vx = b[0] - a[0], vy = b[1] - a[1];
        float len2 = vx * vx + vy * vy;
        float t = len2 > 0 ? ((p[0] - a[0]) * vx + (p[1] - a[1]) * vy) / len2 : 0;
        t = Math.max(0, Math.min(1, t));
        float cx = a[0] + t * vx - p[0], cy = a[1] + t * vy - p[1];
        return cx * cx + cy * cy;
    }
}
