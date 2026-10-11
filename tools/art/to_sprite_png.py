"""Draw Things PNG on a solid key background -> small RGBA PNG ready for sprite_bake.

usage: python3 -I tools/art/to_sprite_png.py in.png green|magenta WIDTH HEIGHT out.png [colors]
"""
import sys

import numpy as np
from PIL import Image

src, key, w, h, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), sys.argv[5]
colors = int(sys.argv[6]) if len(sys.argv) > 6 else 32

img = np.asarray(Image.open(src).convert("RGB")).astype(int)
r, g, b = img[..., 0], img[..., 1], img[..., 2]
# How strongly a pixel leans toward the key color; anything leaning clearly is background or
# anti-aliased fringe.
if key == "green":
    lean = g - np.maximum(r, b)
else:
    lean = np.minimum(r, b) - g
alpha = lean < 40
ys, xs = np.nonzero(alpha)
crop = np.dstack([img, alpha * 255]).astype(np.uint8)[ys.min() : ys.max() + 1, xs.min() : xs.max() + 1]
cut = Image.fromarray(crop, "RGBA")

# Fit inside w x h, bottom-centered (feet on the floor line).
s = min(w / cut.width, h / cut.height)
nw, nh = max(1, round(cut.width * s)), max(1, round(cut.height * s))
small = cut.resize((nw, nh), Image.Resampling.BOX)
canvas = Image.new("RGBA", (w, h), (0, 0, 0, 0))
canvas.paste(small, ((w - nw) // 2, h - nh))

a = np.asarray(canvas)[..., 3]
q = canvas.convert("RGB").quantize(colors, method=Image.Quantize.MEDIANCUT).convert("RGB")
rgba = np.dstack([np.asarray(q), np.where(a >= 128, 255, 0)]).astype(np.uint8)
Image.fromarray(rgba, "RGBA").save(out)
