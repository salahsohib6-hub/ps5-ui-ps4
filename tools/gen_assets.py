#!/usr/bin/env python3
"""Generate the app icon (sce_sys/icon0.png, 512x512) procedurally.

The artwork is original and abstract; it contains no third-party logos.
Requires Pillow.
"""
import math, os, sys
from PIL import Image, ImageChops, ImageDraw, ImageFilter

def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "pkg", "sce_sys")
    os.makedirs(out_dir, exist_ok=True)
    S = 512
    img = Image.new("RGB", (S, S))
    px = img.load()
    for y in range(S):
        for x in range(S):
            t = y / (S - 1)
            r = int(10 + 18 * t); g = int(18 + 22 * t); b = int(48 + 40 * t)
            px[x, y] = (r, g, b)
    glow = Image.new("RGB", (S, S), (0, 0, 0))
    d = ImageDraw.Draw(glow)
    d.ellipse((-80, 200, 300, 580), fill=(30, 90, 220))
    d.ellipse((220, -120, 620, 280), fill=(110, 60, 200))
    glow = glow.filter(ImageFilter.GaussianBlur(90))
    img = ImageChops.add(img, glow)
    d = ImageDraw.Draw(img, "RGBA")
    # Three rounded "tiles" echoing a carousel UI.
    d.rounded_rectangle((70, 190, 190, 330), 22, fill=(255, 255, 255, 60))
    d.rounded_rectangle((330, 190, 450, 330), 22, fill=(255, 255, 255, 60))
    d.rounded_rectangle((170, 150, 342, 362), 30, fill=(255, 255, 255, 235))
    d.rounded_rectangle((170, 150, 342, 362), 30, outline=(120, 190, 255, 255), width=5)
    # Simple "pulse" line (performance graph motif) on the focused tile.
    pts = []
    for i in range(0, 121):
        x = 192 + i * 1.2
        y = 290 - 36 * math.sin(i / 120 * math.pi * 3) * (1 - abs(i - 60) / 90)
        pts.append((x, y))
    d.line(pts, fill=(30, 90, 220, 255), width=8, joint="curve")
    d.rounded_rectangle((70, 392, 442, 410), 9, fill=(255, 255, 255, 70))
    d.rounded_rectangle((70, 392, 300, 410), 9, fill=(120, 190, 255, 255))
    path = os.path.join(out_dir, "icon0.png")
    img.save(path, optimize=True)
    print("wrote", os.path.normpath(path))

if __name__ == "__main__":
    main()
