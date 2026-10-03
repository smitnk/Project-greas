package com.smitnk.projectgrease.editor;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.HashMap;
import java.util.Map;

/**
 * Animated GIF89a writer for Project Grease animation export (Blender exports animation through
 * FFmpeg/image sequences; Android has no GIF encoder, so this is a small self-contained one).
 * Palette: fixed 6x7x6 color cube (252 colors) plus one transparent index (alpha < 128).
 * Frame delay from fps, infinite loop (NETSCAPE2.0). Usage: begin(), addFrame() per frame, finish().
 */
public final class GifEncoder {
    private final OutputStream out;
    private final int width, height;
    private final int delayCs;
    private boolean started;

    public static final int TRANSPARENT_INDEX = 252;

    public GifEncoder(OutputStream out, int width, int height, int fps) {
        if (width <= 0 || height <= 0 || width > 65535 || height > 65535 || fps <= 0) throw new IllegalArgumentException("bad size/fps");
        this.out = out; this.width = width; this.height = height;
        this.delayCs = Math.max(2, Math.round(100f / fps)); // browsers clamp < 2 cs
    }

    public void begin() throws IOException {
        write("GIF89a");
        short16(width); short16(height);
        out.write(0xF7); // global color table, 8 bits color resolution, 256 entries
        out.write(TRANSPARENT_INDEX); out.write(0);
        for (int i = 0; i < 256; i++) {
            int[] c = paletteColor(i);
            out.write(c[0]); out.write(c[1]); out.write(c[2]);
        }
        // NETSCAPE2.0 loop forever
        out.write(0x21); out.write(0xFF); out.write(11); write("NETSCAPE2.0");
        out.write(3); out.write(1); short16(0); out.write(0);
        started = true;
    }

    static int[] paletteColor(int i) {
        if (i >= TRANSPARENT_INDEX) return new int[]{0, 0, 0};
        int r = i / 42, g = (i / 6) % 7, b = i % 6;
        return new int[]{r * 255 / 5, g * 255 / 6, b * 255 / 5};
    }

    static int index(int argb) {
        if (((argb >>> 24) & 0xFF) < 128) return TRANSPARENT_INDEX;
        int r = (((argb >> 16) & 0xFF) * 5 + 127) / 255;
        int g = (((argb >> 8) & 0xFF) * 6 + 127) / 255;
        int b = ((argb & 0xFF) * 5 + 127) / 255;
        return r * 42 + g * 6 + b;
    }

    /** One frame, ARGB pixels row by row (width * height). */
    public void addFrame(int[] argb) throws IOException {
        if (!started) throw new IllegalStateException("begin() first");
        if (argb.length != width * height) throw new IllegalArgumentException("pixel count");
        // graphic control extension: dispose to background, transparency on
        out.write(0x21); out.write(0xF9); out.write(4); out.write(0x09);
        short16(delayCs); out.write(TRANSPARENT_INDEX); out.write(0);
        // image descriptor
        out.write(0x2C); short16(0); short16(0); short16(width); short16(height); out.write(0);
        byte[] px = new byte[argb.length];
        for (int i = 0; i < px.length; i++) px[i] = (byte) index(argb[i]);
        lzw(px);
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
