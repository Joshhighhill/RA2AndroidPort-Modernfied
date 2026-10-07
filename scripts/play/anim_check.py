"""W3-B anim acceptance helper for the anim-*.play scripts.

anim_check.py <dir> [--box W H]
    Shots are named <prefix>_<tick>.png. For every prefix with >= 2 shots,
    prints the changed pixels (|delta| > 24 on any channel) between its first
    and each later shot inside a W x H box centred on the playfield centre
    (where `view` puts the object; default 360 x 300). A playing anim -> > 0.
anim_check.py --cross <dirA> <dirB>
    For every shot name present in both dirs (same prefix + same anim tick),
    prints the changed pixels over the playfield (0..1110 x 0..720, the
    parked-cursor box masked). Frame-rate independence -> 0 everywhere.
Needs Pillow.
"""
import sys, os, re, glob
from PIL import Image, ImageChops, ImageDraw

def changed(a, b, box):
    ia = Image.open(a).convert("RGB").crop(box)
    ib = Image.open(b).convert("RGB").crop(box)
    for im in (ia, ib):   # the parked mouse cursor animates per render frame
        x0, y0 = 540 - box[0], 340 - box[1]   # (crop-relative; outside -> no-op)
        ImageDraw.Draw(im).rectangle((x0, y0, x0 + 50, y0 + 50), fill=(0, 0, 0))
    df = ImageChops.difference(ia, ib).convert("L").point(lambda v: 255 if v > 24 else 0)
    return sum(1 for v in df.getdata() if v)

def main(argv):
    if len(argv) >= 3 and argv[0] == "--cross":
        A, B = argv[1], argv[2]
        bad = 0
        for a in sorted(glob.glob(os.path.join(A, "*.png"))):
            n = os.path.basename(a)
            b = os.path.join(B, n)
            if not os.path.exists(b) or not re.search(r"_\d+\.png$", n) or n.startswith("live_"):
                continue   # live_*: real-input steps (applied after the frame's ticks)
            c = changed(a, b, (0, 0, 1110, 720))
            bad += c > 0
            print(f"{n:34s} cross_rate_changed_px={c}")
        print(f"cross_rate_mismatches={bad}")
        return 0
    d = argv[0]
    w, h = 280, 240
    if "--box" in argv:
        i = argv.index("--box"); w, h = int(argv[i + 1]), int(argv[i + 2])
    centre = {}   # shots.txt (the driver): name sx sy of the last `view` target
    try:
        for line in open(os.path.join(d, "shots.txt")):
            f = line.split()
            if len(f) == 3 and int(f[1]) >= 0:
                centre[f[0]] = (int(f[1]), int(f[2]))
    except OSError:
        pass
    def box_for(name):
        cx, cy = centre.get(name, (555, 360))
        return (max(0, cx - w // 2), max(0, cy - h // 2), min(1110, cx + w // 2), min(720, cy + h // 2))
    groups = {}
    for p in glob.glob(os.path.join(d, "*.png")):
        m = re.match(r"(.+)_(\d+)\.png$", os.path.basename(p))
        if m:
            groups.setdefault(m.group(1), []).append((int(m.group(2)), p))
    for k in sorted(groups):
        g = sorted(groups[k])
        for t, p in g[1:]:
            box = box_for(os.path.basename(g[0][1])[:-4])
            print(f"{k:16s} tick {g[0][0]:5d} -> {t:5d}: changed_px={changed(g[0][1], p, box)} box={box}")
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
