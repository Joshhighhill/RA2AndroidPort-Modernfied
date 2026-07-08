#!/usr/bin/env python3
"""Convert XCC-exported indexed images (BMP/PCX/PNG) to headerless .raw index dumps."""
import sys
from PIL import Image

for path in sys.argv[1:]:
    im = Image.open(path)
    if im.mode != "P":
        raise SystemExit(f"{path}: not an indexed image (mode={im.mode})")
    out = path.rsplit(".", 1)[0] + ".raw"
    with open(out, "wb") as f:
        f.write(im.tobytes())      # top-down, row-major w*h palette indices
    print(f"{out}: {im.width}x{im.height}")
