#!/usr/bin/env python3
"""Plots the soak's memory samples ("SOAK t_min=.. pss_kb=.. native_kb=.. java_kb=.." logcat lines) as
an SVG line chart. usage: memory_graph.py <logcat file> <out.svg> <title>"""
import re
import sys


def main():
    log, out, title = sys.argv[1], sys.argv[2], sys.argv[3]
    rx = re.compile(r"SOAK t_min=([\d.]+) pss_kb=(\d+) native_kb=(\d+) java_kb=(\d+)")
    with open(log, errors="replace") as f:  # streamed: the logcat can be hundreds of MB
        pts = [tuple(float(v) for v in m.groups()) for m in map(rx.search, f) if m]
    W, H, L, B = 760, 360, 70, 40
    series = [("PSS", 1, "#1f77b4"), ("native heap", 2, "#d62728"), ("Java heap", 3, "#2ca02c")]
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" font-family="sans-serif" font-size="12">',
           f'<rect width="{W}" height="{H}" fill="white"/><text x="{L}" y="20" font-size="14">{title}</text>']
    if len(pts) >= 2:
        tmax = max(p[0] for p in pts) or 1
        vmax = max(max(p[1:]) for p in pts) / 1024.0 or 1
        sx = lambda t: L + (W - L - 20) * t / tmax
        sy = lambda mb: H - B - (H - B - 40) * mb / vmax
        svg.append(f'<line x1="{L}" y1="{H - B}" x2="{W - 20}" y2="{H - B}" stroke="#999"/><line x1="{L}" y1="40" x2="{L}" y2="{H - B}" stroke="#999"/>')
        for k in range(5):
            v = vmax * k / 4
            svg.append(f'<text x="{L - 8}" y="{sy(v) + 4:.0f}" text-anchor="end">{v:.0f} MB</text>')
            t = tmax * k / 4
            svg.append(f'<text x="{sx(t):.0f}" y="{H - B + 16}" text-anchor="middle">{t:.0f} min</text>')
        for i, (name, col, colour) in enumerate(series):
            path = " ".join(f"{sx(p[0]):.1f},{sy(p[col] / 1024.0):.1f}" for p in pts)
            svg.append(f'<polyline fill="none" stroke="{colour}" stroke-width="2" points="{path}"/>')
            svg.append(f'<text x="{W - 160}" y="{50 + 16 * i}" fill="{colour}">{name}</text>')
    else:
        svg.append(f'<text x="{L}" y="80">no soak samples</text>')
    svg.append("</svg>")
    open(out, "w").write("\n".join(svg))


if __name__ == "__main__":
    main()
