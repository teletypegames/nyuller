#!/usr/bin/env python3
"""Apply scanline effect: darken every odd row for CRT aesthetic."""
import sys
from PIL import Image

src, dst = sys.argv[1], sys.argv[2]
img = Image.open(src).resize((320, 200), Image.LANCZOS).convert('RGB')
px = img.load()
for y in range(200):
    if y % 2 == 1:
        for x in range(320):
            r, g, b = px[x, y]
            px[x, y] = (r // 4, g // 4, b // 4)
img.save(dst)
