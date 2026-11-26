#!/usr/bin/env python3
"""Generate PNG test images in many colour types plus their expected raw RGBA."""
import os, random, sys
from PIL import Image

out = sys.argv[1]
os.makedirs(out, exist_ok=True)
random.seed(7)

def noise_rgba(w, h, alpha=True):
    img = Image.new("RGBA", (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            a = random.randrange(256) if alpha else 255
            px[x, y] = ((x * 5) & 255, (y * 3 + x) & 255, random.randrange(256), a)
    return img

cases = {}
cases["rgba"] = noise_rgba(37, 29)
cases["rgb"] = noise_rgba(64, 48, alpha=False).convert("RGB")
cases["gray"] = noise_rgba(33, 21, alpha=False).convert("L")
cases["graya"] = noise_rgba(20, 20).convert("LA")
for bits, ncol in ((1, 2), (2, 4), (4, 16), (8, 200)):
    base = noise_rgba(41, 23, alpha=False).convert("RGB").quantize(ncol, dither=Image.NONE)
    cases[f"pal{bits}"] = base
big = noise_rgba(300, 200, alpha=False)
for x in range(300):
    for y in range(200):
        if (x // 20 + y // 20) % 2 == 0:
            big.putpixel((x, y), (x % 256, 90, 200, 255))
cases["big_compressible"] = big.convert("RGB")

for name, img in cases.items():
    for level in (0, 1, 9):
        path = os.path.join(out, f"{name}_z{level}.png")
        img.save(path, compress_level=level, optimize=(level == 9))
        raw = img.convert("RGBA").tobytes()
        with open(path + ".rgba", "wb") as f:
            f.write(img.size[0].to_bytes(4, "little") + img.size[1].to_bytes(4, "little") + raw)
print("fixtures:", len(os.listdir(out)) // 2)
