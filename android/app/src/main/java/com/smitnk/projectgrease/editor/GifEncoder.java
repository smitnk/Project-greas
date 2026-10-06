package com.smitnk.projectgrease.editor;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.HashMap;
import java.util.Map;

/**
 * Animated GIF89a writer for Project Grease animation export (Blender exports animation through
 * FFmpeg/image sequences; Android has no GIF encoder, so this is a small self-contained one).
 * Palette: an adaptive local color table per frame ({@link GifQuantizer}: median cut, up to 255
 * colors, exact when the frame has no more than 255 distinct opaque colors) plus one transparent
 * index (alpha < 128), optionally with Floyd-Steinberg dithering. Deterministic: the same frames give
 * the same bytes. Frame delay from fps, infinite loop (NETSCAPE2.0). Usage: begin(), addFrame() per
 * frame, finish().
 */
public final class GifEncoder {
    private final OutputStream out;
    private final int width, height;
    private final int delayCs;
    private final boolean dither;
    private boolean started;

    public static final int TRANSPARENT_INDEX = 255;

    public GifEncoder(OutputStream out, int width, int height, int fps) { this(out, width, height, fps, false); }

    public GifEncoder(OutputStream out, int width, int height, int fps, boolean dither) {
        if (width <= 0 || height <= 0 || width > 65535 || height > 65535 || fps <= 0) throw new IllegalArgumentException("bad size/fps");
        this.out = out; this.width = width; this.height = height;
        this.delayCs = Math.max(2, Math.round(100f / fps)); // browsers clamp < 2 cs
        this.dither = dither;
    }

    public void begin() throws IOException {
        write("GIF89a");
        short16(width); short16(height);
        out.write(0x70); // no global color table (each frame has its own), 8 bits color resolution
        out.write(0); out.write(0);
        // NETSCAPE2.0 loop forever
        out.write(0x21); out.write(0xFF); out.write(11); write("NETSCAPE2.0");
        out.write(3); out.write(1); short16(0); out.write(0);
        started = true;
    }

    /** The 256-entry local table: the palette, black padding, index 255 transparent. */
    static int[] colorTable(int[] palette) {
        int[] table = new int[256];
        System.arraycopy(palette, 0, table, 0, Math.min(palette.length, TRANSPARENT_INDEX));
        return table;
    }

    /** One frame, ARGB pixels row by row (width * height). */
    public void addFrame(int[] argb) throws IOException {
        if (!started) throw new IllegalStateException("begin() first");
        if (argb.length != width * height) throw new IllegalArgumentException("pixel count");
        // graphic control extension: dispose to background, transparency on
        out.write(0x21); out.write(0xF9); out.write(4); out.write(0x09);
        short16(delayCs); out.write(TRANSPARENT_INDEX); out.write(0);
        // image descriptor
        int[] palette = GifQuantizer.palette(argb, GifQuantizer.MAX_COLORS);
        out.write(0x2C); short16(0); short16(0); short16(width); short16(height);
        out.write(0x87); // local color table, 256 entries
        for (int c : colorTable(palette)) { out.write((c >> 16) & 0xFF); out.write((c >> 8) & 0xFF); out.write(c & 0xFF); }
        lzw(GifQuantizer.indexFrame(argb, width, height, palette, dither, TRANSPARENT_INDEX));
    }

    public void finish() throws IOException {
        out.write(0x3B);
        out.flush();
    }

    private void lzw(byte[] px) throws IOException {
        final int minCode = 8, clear = 1 << minCode, end = clear + 1;
        ByteArrayOutputStream data = new ByteArrayOutputStream();
        BitWriter bw = new BitWriter(data);
        Map<Integer, Integer> table = new HashMap<>();
        int codeSize = minCode + 1, next = end + 1;
        bw.write(clear, codeSize);
        int prefix = px[0] & 0xFF;
        for (int i = 1; i < px.length; i++) {
            int c = px[i] & 0xFF;
            int key = (prefix << 8) | c;
            Integer code = table.get(key);
            if (code != null) { prefix = code; continue; }
            bw.write(prefix, codeSize);
            if (next < 4096) {
                table.put(key, next++);
                if (next > (1 << codeSize) && codeSize < 12) codeSize++;
            } else {
                bw.write(clear, codeSize);
                table.clear(); codeSize = minCode + 1; next = end + 1;
            }
            prefix = c;
        }
        bw.write(prefix, codeSize);
        bw.write(end, codeSize);
        bw.flush();
        byte[] bytes = data.toByteArray();
        out.write(minCode);
        for (int off = 0; off < bytes.length; off += 255) {
            int len = Math.min(255, bytes.length - off);
            out.write(len); out.write(bytes, off, len);
        }
        out.write(0);
    }

    private static final class BitWriter {
        private final ByteArrayOutputStream o; private int acc, bits;
        BitWriter(ByteArrayOutputStream o) { this.o = o; }
        void write(int code, int size) { acc |= code << bits; bits += size; while (bits >= 8) { o.write(acc & 0xFF); acc >>>= 8; bits -= 8; } }
        void flush() { if (bits > 0) o.write(acc & 0xFF); acc = 0; bits = 0; }
    }

    private void short16(int v) throws IOException { out.write(v & 0xFF); out.write((v >> 8) & 0xFF); }
    private void write(String s) throws IOException { for (int i = 0; i < s.length(); i++) out.write(s.charAt(i)); }
}
