package com.smitnk.projectgrease.editor;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Adaptive per-frame palette for {@link GifEncoder} (no android.* so it is JVM-testable).
 *
 * <p>Median cut (Heckbert): the opaque colors of the frame (alpha &gt;= 128) are counted; when there
 * are at most {@code maxColors} distinct colors they are the palette as they are (exact). Otherwise
 * the color box with the widest channel range is split at the population-weighted median of that
 * channel until there are {@code maxColors} boxes; each palette entry is the population-weighted mean
 * of its box. Every choice has a fixed tie-break (lowest box index, colors ordered by channel then
 * rgb), so the same pixels always give the same palette.
 *
 * <p>Pixels map to the nearest entry (squared RGB distance, lowest index on ties). Optional
 * Floyd-Steinberg error diffusion (7/16, 3/16, 5/16, 1/16, left-to-right rows, integer arithmetic) is
 * deterministic; transparent pixels neither receive nor spread error.
 */
public final class GifQuantizer {
    private GifQuantizer() {}

    public static final int MAX_COLORS = 255;

    static boolean opaque(int argb) { return ((argb >>> 24) & 0xFF) >= 128; }

    /** Palette as 0xRRGGBB values, at most {@code maxColors} (1..256) entries; may be empty. */
    public static int[] palette(int[] argb, int maxColors) {
        if (maxColors < 1 || maxColors > 256) throw new IllegalArgumentException("maxColors");
        HashMap<Integer, Integer> hist = new HashMap<>();
        for (int c : argb) if (opaque(c)) hist.merge(c & 0xFFFFFF, 1, Integer::sum);
        int n = hist.size();
        int[] colors = new int[n];
        int k = 0;
        for (Integer c : hist.keySet()) colors[k++] = c;
        Arrays.sort(colors);
        if (n <= maxColors) return colors;
        int[] counts = new int[n];
        for (int i = 0; i < n; i++) counts[i] = hist.get(colors[i]);

        List<int[]> boxes = new ArrayList<>(); // {start, end} (end exclusive)
        boxes.add(new int[]{0, n});
        while (boxes.size() < maxColors) {
            int best = -1, bestRange = -1, bestChannel = 0;
            for (int b = 0; b < boxes.size(); b++) {
                int[] box = boxes.get(b);
                if (box[1] - box[0] < 2) continue;
                int[] r = ranges(colors, box[0], box[1]);
                for (int ch = 0; ch < 3; ch++) {
                    if (r[ch] > bestRange) { bestRange = r[ch]; best = b; bestChannel = ch; }
                }
            }
            if (best < 0 || bestRange <= 0) break;
            int[] box = boxes.get(best);
            int split = sortAndSplit(colors, counts, box[0], box[1], bestChannel);
            boxes.set(best, new int[]{box[0], split});
            boxes.add(best + 1, new int[]{split, box[1]});
        }
        int[] out = new int[boxes.size()];
        for (int b = 0; b < out.length; b++) {
            int[] box = boxes.get(b);
            long sr = 0, sg = 0, sb = 0, total = 0;
            for (int i = box[0]; i < box[1]; i++) {
                long w = counts[i];
                sr += w * ((colors[i] >> 16) & 0xFF); sg += w * ((colors[i] >> 8) & 0xFF); sb += w * (colors[i] & 0xFF);
                total += w;
            }
            int r = (int) ((sr + total / 2) / total), g = (int) ((sg + total / 2) / total), bl = (int) ((sb + total / 2) / total);
            out[b] = (r << 16) | (g << 8) | bl;
        }
        return out;
    }

    private static int channel(int rgb, int ch) { return (rgb >> (16 - 8 * ch)) & 0xFF; }

    private static int[] ranges(int[] colors, int start, int end) {
        int[] mn = {255, 255, 255}, mx = {0, 0, 0};
        for (int i = start; i < end; i++) for (int ch = 0; ch < 3; ch++) {
            int v = channel(colors[i], ch);
            if (v < mn[ch]) mn[ch] = v;
            if (v > mx[ch]) mx[ch] = v;
        }
        return new int[]{mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2]};
    }

    /** Sorts [start, end) by the channel (then rgb) and returns the weighted-median split index. */
    private static int sortAndSplit(int[] colors, int[] counts, int start, int end, int ch) {
        long[] keys = new long[end - start];
        for (int i = start; i < end; i++) {
            keys[i - start] = ((long) channel(colors[i], ch) << 56) | ((long) colors[i] << 32) | (counts[i] & 0xFFFFFFFFL);
        }
        Arrays.sort(keys);
        long total = 0;
        for (int i = 0; i < keys.length; i++) {
            colors[start + i] = (int) ((keys[i] >>> 32) & 0xFFFFFF);
            counts[start + i] = (int) keys[i];
            total += counts[start + i];
        }
        long acc = 0;
        int split = start + 1;
        for (int i = start; i < end - 1; i++) {
            acc += counts[i];
            split = i + 1;
            if (acc * 2 >= total) break;
        }
        return Math.max(start + 1, Math.min(split, end - 1));
    }

    /** Index of the nearest palette entry (squared RGB distance, lowest index on ties). */
    public static int nearest(int[] palette, int r, int g, int b) {
        int best = 0, bestD = Integer.MAX_VALUE;
        for (int i = 0; i < palette.length; i++) {
            int dr = r - ((palette[i] >> 16) & 0xFF), dg = g - ((palette[i] >> 8) & 0xFF), db = b - (palette[i] & 0xFF);
            int d = dr * dr + dg * dg + db * db;
            if (d < bestD) { bestD = d; best = i; }
        }
        return best;
    }

    /**
     * Palette indices for one frame; pixels with alpha &lt; 128 get {@code transparentIndex}.
     * With an empty palette every opaque pixel maps to 0.
     */
    public static byte[] indexFrame(int[] argb, int width, int height, int[] palette, boolean dither, int transparentIndex) {
        if (argb.length != width * height) throw new IllegalArgumentException("pixel count");
        byte[] px = new byte[argb.length];
        Map<Integer, Integer> cache = new HashMap<>();
        if (!dither || palette.length == 0) {
            for (int i = 0; i < argb.length; i++) {
                int c = argb[i];
                if (!opaque(c)) { px[i] = (byte) transparentIndex; continue; }
                if (palette.length == 0) { px[i] = 0; continue; }
                Integer key = c & 0xFFFFFF;
                Integer idx = cache.get(key);
                if (idx == null) { idx = nearest(palette, (key >> 16) & 0xFF, (key >> 8) & 0xFF, key & 0xFF); cache.put(key, idx); }
                px[i] = (byte) (int) idx;
            }
            return px;
        }
        // Floyd-Steinberg: errors in 1/16 units, two rows of (r, g, b) accumulators
        int[] cur = new int[(width + 2) * 3], next = new int[(width + 2) * 3];
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int i = y * width + x;
                int c = argb[i];
                int e = (x + 1) * 3;
                if (!opaque(c)) { px[i] = (byte) transparentIndex; continue; }
                int r = clamp(((c >> 16) & 0xFF) + div16(cur[e]));
                int g = clamp(((c >> 8) & 0xFF) + div16(cur[e + 1]));
                int b = clamp((c & 0xFF) + div16(cur[e + 2]));
                Integer key = (r << 16) | (g << 8) | b;
                Integer idx = cache.get(key);
                if (idx == null) { idx = nearest(palette, r, g, b); cache.put(key, idx); }
                px[i] = (byte) (int) idx;
                int p = palette[idx];
                int[] err = {r - ((p >> 16) & 0xFF), g - ((p >> 8) & 0xFF), b - (p & 0xFF)};
                for (int ch = 0; ch < 3; ch++) {
                    if (x + 1 < width && opaque(argb[i + 1])) cur[e + 3 + ch] += err[ch] * 7;
                    if (y + 1 < height) {
                        if (x > 0 && opaque(argb[i + width - 1])) next[e - 3 + ch] += err[ch] * 3;
                        if (opaque(argb[i + width])) next[e + ch] += err[ch] * 5;
                        if (x + 1 < width && opaque(argb[i + width + 1])) next[e + 3 + ch] += err[ch];
                    }
                }
            }
            int[] t = cur; cur = next; next = t;
            Arrays.fill(next, 0);
        }
        return px;
    }

    /** Rounds v / 16 to nearest, half away from zero (symmetric, deterministic). */
    private static int div16(int v) { return v >= 0 ? (v + 8) >> 4 : -((-v + 8) >> 4); }

    private static int clamp(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }
}
