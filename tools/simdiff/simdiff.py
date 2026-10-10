#!/usr/bin/env python3
"""simdiff -- compare two per-frame simulation dumps (original gamemd vs clone).

Dump format v1 (written by tools/simdiff/hook/simhook.dll and by the
engine's --diff-dump switch); see docs/re/simdiff-harness.md section 3:

  # simdiff v1 src=<who> seed=<hex> ...
  F <frame> crc <hex> rng <i1> <i2> <hash> rng2 <i1> <i2> <hash>
          vec <inf> <unit> <air> <bld> logic <n> layers <l0..l4> cen <rtti>:<n> ...
  O <rtti> <uid> <type> <owner> <x> <y> <z> <facing16> <turret16|-1> <hp> <mission>

Fields the producer cannot provide are written as '-' and are skipped.

Usage:
  simdiff.py diff A B [--ignore uid,crc,...] [--frames N] [--json out]
      first divergent frame + field (strict, positional), plus the first
      divergence of each field class.
  simdiff.py l0 A.l0 B.l0          compare RNG draw streams (ladder rung L0)
  simdiff.py ladder MANIFEST       run every rung of a ladder manifest
  simdiff.py summary A             one-line-per-N-frames overview of a dump
"""
import argparse
import json
import sys
from collections import Counter, OrderedDict

OBJ_FIELDS = ["rtti", "uid", "type", "owner", "x", "y", "z", "facing", "turret", "hp", "mission"]
RTTI = {1: "Unit", 2: "Aircraft", 4: "Anim", 6: "Building", 8: "Bullet", 15: "Infantry",
        24: "Particle?", 36: "Terrain"}


class Frame:
    __slots__ = ("n", "hdr", "objs")

    def __init__(self, n):
        self.n = n
        self.hdr = OrderedDict()
        self.objs = []


def parse(path, max_frames=None):
    meta = {}
    frames = OrderedDict()
    cur = None
    with open(path, "r", errors="replace") as f:
        for line in f:
            if line.startswith("#"):
                if line.startswith("# simdiff"):
                    for tok in line.split()[2:]:
                        if "=" in tok:
                            k, v = tok.split("=", 1)
                            meta[k] = v
                continue
            t = line.split()
            if not t:
                continue
            if t[0] == "F":
                n = int(t[1])
                if max_frames is not None and n > max_frames:
                    break
                cur = Frame(n)
                frames[n] = cur
                i = 2
                while i < len(t):
                    k = t[i]
                    if k == "crc":
                        cur.hdr["crc"] = t[i + 1]; i += 2
                    elif k in ("rng", "rng2"):
                        cur.hdr[k] = " ".join(t[i + 1:i + 4]); i += 4
                    elif k == "vec":
                        cur.hdr["vec"] = " ".join(t[i + 1:i + 5]); i += 5
                    elif k == "logic":
                        cur.hdr["logic"] = t[i + 1]; i += 2
                    elif k == "layers":
                        cur.hdr["layers"] = " ".join(t[i + 1:i + 6]); i += 6
                    elif k == "cen":
                        cur.hdr["cen"] = " ".join(t[i + 1:]); break
                    else:
                        i += 1
            elif t[0] == "O" and cur is not None:
                cur.objs.append(t[1:12])
    return meta, frames


def techno_census(fr):
    c = Counter()
    for o in fr.objs:
        c[(o[0], o[2], o[3])] += 1
    return c


def matched(A, B):
    """Pair objects by (rtti, type, owner), greedily nearest in position, and
    measure how close the paired ones are. Order- and id-free, so it says
    something useful long before a strict positional compare can."""
    from collections import defaultdict
    ga, gb = defaultdict(list), defaultdict(list)
    for o in A.objs:
        ga[(o[0], o[2], o[3])].append(o)
    for o in B.objs:
        gb[(o[0], o[2], o[3])].append(o)
    pairs = []
    for k, la in ga.items():
        lb = list(gb.get(k, []))
        for oa in la:
            if not lb:
                break
            ax, ay = int(oa[4]), int(oa[5])
            j = min(range(len(lb)), key=lambda i: abs(int(lb[i][4]) - ax) + abs(int(lb[i][5]) - ay))
            pairs.append((oa, lb.pop(j)))
    st = {"a": len(A.objs), "b": len(B.objs), "paired": len(pairs)}
    if pairs:
        d = [abs(int(a[4]) - int(b[4])) + abs(int(a[5]) - int(b[5])) for a, b in pairs]
        st["pos_exact"] = sum(1 for x in d if x == 0)
        st["pos_within_cell"] = sum(1 for x in d if x < 256)
        st["pos_mean_l1"] = round(sum(d) / len(d), 1)
        fe = [(a, b) for a, b in pairs if a[7] != "-" and b[7] != "-"]
        st["facing_exact"] = sum(1 for a, b in fe if a[7] == b[7])
        he = [(a, b) for a, b in pairs if a[9] != "-" and b[9] != "-"]
        st["hp_exact"] = sum(1 for a, b in he if a[9] == b[9])
    return st


def cmp_val(a, b):
    return a == "-" or b == "-" or a == b


def diff(a_path, b_path, ignore=(), max_frames=None):
    ma, fa = parse(a_path, max_frames)
    mb, fb = parse(b_path, max_frames)
    common = [n for n in fa if n in fb]
    res = OrderedDict()
    res["a"] = {"path": a_path, **ma, "frames": len(fa)}
    res["b"] = {"path": b_path, **mb, "frames": len(fb)}
    res["common_frames"] = len(common)
    first = None
    # first divergence per field class
    classes = OrderedDict((k, None) for k in [
        "crc", "rng", "rng2", "vec", "logic", "layers", "cen",
        "object_count", "object_set", "order", "uid", "type", "owner",
        "position", "facing", "turret", "hp", "mission"])
    for k in list(classes):
        if k in ignore:
            del classes[k]
    hdr_keys = [k for k in ("crc", "rng", "rng2", "vec", "logic", "layers", "cen") if k not in ignore]
    absent, compared = set(), set()

    def is_absent(v):
        return all(x == "-" for x in v.split())
    for n in common:
        A, B = fa[n], fb[n]
        for k in hdr_keys:
            va, vb = A.hdr.get(k, "-"), B.hdr.get(k, "-")
            if is_absent(va) or is_absent(vb):
                absent.add(k)
                continue
            compared.add(k)
            if not all(cmp_val(x, y) for x, y in zip(va.split(), vb.split())) or (va != "-" and vb != "-" and len(va.split()) != len(vb.split())):
                if classes.get(k) is None:
                    classes[k] = {"frame": n, "a": va, "b": vb}
                if first is None:
                    first = {"frame": n, "field": k, "a": va, "b": vb}
        if len(A.objs) != len(B.objs) and classes.get("object_count") is None:
            classes["object_count"] = {"frame": n, "a": len(A.objs), "b": len(B.objs)}
            if first is None:
                first = {"frame": n, "field": "object_count", "a": len(A.objs), "b": len(B.objs)}
        if "object_set" in classes and classes["object_set"] is None and techno_census(A) != techno_census(B):
            ca, cb = techno_census(A), techno_census(B)
            only_a = sorted((ca - cb).items())[:6]
            only_b = sorted((cb - ca).items())[:6]
            classes["object_set"] = {"frame": n, "only_a": [" ".join(k) + " x%d" % v for k, v in only_a],
                                     "only_b": [" ".join(k) + " x%d" % v for k, v in only_b]}
        # positional compare
        for idx, (oa, ob) in enumerate(zip(A.objs, B.objs)):
            same_id = (oa[0], oa[2], oa[3]) == (ob[0], ob[2], ob[3])
            if not same_id:
                if classes.get("order") is None and "order" in classes:
                    classes["order"] = {"frame": n, "index": idx, "a": " ".join(oa[:4]), "b": " ".join(ob[:4])}
                if first is None and "order" not in ignore:
                    first = {"frame": n, "field": "order", "index": idx, "a": " ".join(oa), "b": " ".join(ob)}
                continue
            for fi, name in enumerate(OBJ_FIELDS):
                cls = {"x": "position", "y": "position", "z": "position"}.get(name, name)
                if cls in ignore or name in ("rtti",):
                    continue
                if oa[fi] == "-" or ob[fi] == "-":
                    absent.add(cls)
                    continue
                compared.add(cls)
                if not cmp_val(oa[fi], ob[fi]):
                    if classes.get(cls) is None:
                        classes[cls] = {"frame": n, "index": idx, "object": " ".join(oa[:4]),
                                        "field": name, "a": oa[fi], "b": ob[fi]}
                    if first is None:
                        first = {"frame": n, "field": name, "index": idx, "a": " ".join(oa), "b": " ".join(ob)}
    res["absent"] = sorted(absent - compared)
    res["not_compared"] = sorted(k for k in classes if classes[k] is None and k not in compared
                                 and k not in absent and k in ("uid", "type", "owner", "position",
                                                               "facing", "turret", "hp", "mission"))
    probe = sorted(set([common[0], common[-1]] + [n for n in (10, 100, 300, 1000, 3000) if n in fa and n in fb])) if common else []
    res["matched"] = OrderedDict((n, matched(fa[n], fb[n])) for n in probe)
    res["first_divergence"] = first
    res["by_class"] = classes
    res["matched_frames"] = (first["frame"] if first else (common[-1] + 1 if common else 0))
    return res


def l0(a_path, b_path):
    def load(p):
        raw, rng = [], []
        with open(p) as f:
            for line in f:
                t = line.split()
                if not t or t[0] == "L0":
                    continue
                if t[0] == "R":
                    rng.append((int(t[1]), int(t[2]), int(t[3])))
                else:
                    raw.append(t[0].upper())
        return raw, rng
    ra, ga = load(a_path)
    rb, gb = load(b_path)
    out = {"raw_compared": min(len(ra), len(rb)), "ranged_compared": min(len(ga), len(gb))}
    out["raw_first_mismatch"] = next((i for i, (x, y) in enumerate(zip(ra, rb)) if x != y), None)
    out["ranged_first_mismatch"] = next((i for i, (x, y) in enumerate(zip(ga, gb)) if x != y), None)
    out["pass"] = out["raw_first_mismatch"] is None and out["ranged_first_mismatch"] is None \
        and out["raw_compared"] > 0
    return out


def fmt_diff(r):
    lines = []
    lines.append("A: %s (%s frames, src=%s seed=%s scenario=%s)" % (r["a"]["path"], r["a"]["frames"], r["a"].get("src"), r["a"].get("seed"), r["a"].get("scenario")))
    lines.append("B: %s (%s frames, src=%s seed=%s scenario=%s)" % (r["b"]["path"], r["b"]["frames"], r["b"].get("src"), r["b"].get("seed"), r["b"].get("scenario")))
    fd = r["first_divergence"]
    if fd is None:
        lines.append("IDENTICAL over %d common frames" % r["common_frames"])
    else:
        lines.append("FIRST DIVERGENCE: frame %s field %s" % (fd["frame"], fd["field"]))
        for k in ("index", "a", "b"):
            if k in fd:
                lines.append("   %s: %s" % (k, fd[k]))
    lines.append("first divergence by field class:")
    for k, v in r["by_class"].items():
        if v is None and k in r.get("absent", []):
            lines.append("   %-12s n/a (not produced by one side)" % k)
        elif v is None and k in r.get("not_compared", []):
            lines.append("   %-12s not compared (no positionally aligned objects)" % k)
        elif v is None:
            lines.append("   %-12s match (all %d frames)" % (k, r["common_frames"]))
        else:
            extra = {kk: vv for kk, vv in v.items() if kk != "frame"}
            lines.append("   %-12s frame %-6s %s" % (k, v["frame"], json.dumps(extra)[:220]))
    if r.get("matched"):
        lines.append("order-free pairing by (rtti,type,owner), nearest position:")
        for n, st in r["matched"].items():
            lines.append("   frame %-6s %s" % (n, json.dumps(st)))
    return "\n".join(lines)


def summary(path, step=100):
    meta, fr = parse(path)
    print(meta)
    for n, f in fr.items():
        if n % step:
            continue
        owners = Counter(o[3] for o in f.objs)
        print("F%-6d crc %s techno %d owners %s cen %s" % (n, f.hdr.get("crc"), len(f.objs), dict(owners), f.hdr.get("cen")))


def markers(path):
    """Event markers of a dump, used to place ladder windows: first frame a
    techno moves, first new techno (production), first HP loss (combat)."""
    meta, fr = parse(path)
    prev = None
    out = OrderedDict([("first_move", None), ("first_new_object", None), ("first_damage", None)])

    def keyed(objs):
        # UniqueID when the producer has one; else (rtti,type,owner,k-th) --
        # stable until the first death (the clone has no ids).
        seen, d = Counter(), {}
        for o in objs:
            if o[1] != "-":
                d[o[1]] = o
            else:
                k = (o[0], o[2], o[3]); seen[k] += 1
                d[k + (seen[k],)] = o
        return d
    for n, f in fr.items():
        cur = keyed(f.objs)
        if prev is not None:
            pa = prev
            for key, o in cur.items():
                q = pa.get(key)
                if q is None:
                    if out["first_new_object"] is None:
                        out["first_new_object"] = (n, " ".join(o[:4]))
                    continue
                if out["first_move"] is None and q[4:7] != o[4:7]:
                    out["first_move"] = (n, " ".join(o[:4]))
                if out["first_damage"] is None and q[9] != "-" and int(o[9]) < int(q[9]):
                    out["first_damage"] = (n, " ".join(o[:4]))
        prev = cur
    return out


def build_order(a_path, b_path, owner="1"):
    """First frame each type appears for one house in A and B (AI economy
    rung): same sequence + timing deltas, even while every frame diverges."""
    def firsts(p):
        _, fr = parse(p)
        d = OrderedDict()
        for n, f in fr.items():
            for o in f.objs:
                if o[3] == owner and o[2] not in d:
                    d[o[2]] = n
        return d
    fa, fb = firsts(a_path), firsts(b_path)
    rows = []
    for t, n in fa.items():
        rows.append((t, n, fb.get(t)))
    for t, n in fb.items():
        if t not in fa:
            rows.append((t, None, n))
    return rows


def ladder(manifest):
    """Manifest: JSON list of rungs {name, kind: l0|diff, a, b, ignore?, frames?}."""
    with open(manifest) as f:
        rungs = json.load(f)
    rows = []
    for r in rungs:
        if r["kind"] == "l0":
            o = l0(r["a"], r["b"])
            rows.append((r["name"], "PASS" if o["pass"] else "FAIL",
                         "%d raw + %d ranged draws; first mismatch raw=%s ranged=%s" % (
                             o["raw_compared"], o["ranged_compared"], o["raw_first_mismatch"], o["ranged_first_mismatch"])))
        elif r["kind"] == "buildorder":
            bo = build_order(r["a"], r["b"], str(r.get("owner", 1)))
            both = [x for x in bo if x[1] is not None and x[2] is not None]
            txt = " ".join("%s:%s/%s" % (t, a, b) for t, a, b in bo)
            rows.append((r["name"], "%d/%d types" % (len(both), len(bo)), "type:orig/clone first frame  " + txt))
        elif r["kind"] == "markers":
            rows.append((r["name"], "info", json.dumps(markers(r["a"]))))
        else:
            o = diff(r["a"], r["b"], set(r.get("ignore", [])), r.get("frames"))
            fd = o["first_divergence"]
            if fd is None:
                rows.append((r["name"], "PASS", "identical over %d frames" % o["common_frames"]))
            else:
                rows.append((r["name"], "frame %d" % fd["frame"], "first divergent field: %s" % fd["field"]))
    w = max(len(x[0]) for x in rows)
    for name, score, what in rows:
        print("%-*s  %-12s %s" % (w, name, score, what))
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    d = sub.add_parser("diff"); d.add_argument("a"); d.add_argument("b")
    d.add_argument("--ignore", default=""); d.add_argument("--frames", type=int); d.add_argument("--json")
    l = sub.add_parser("l0"); l.add_argument("a"); l.add_argument("b")
    s = sub.add_parser("summary"); s.add_argument("a"); s.add_argument("--step", type=int, default=100)
    g = sub.add_parser("ladder"); g.add_argument("manifest")
    mk = sub.add_parser("markers"); mk.add_argument("a")
    bo = sub.add_parser("buildorder"); bo.add_argument("a"); bo.add_argument("b"); bo.add_argument("--owner", default="1")
    args = ap.parse_args()
    if args.cmd == "diff":
        r = diff(args.a, args.b, set(x for x in args.ignore.split(",") if x), args.frames)
        print(fmt_diff(r))
        if args.json:
            with open(args.json, "w") as f:
                json.dump(r, f, indent=1)
        return 0 if r["first_divergence"] is None else 1
    if args.cmd == "l0":
        o = l0(args.a, args.b)
        print(json.dumps(o, indent=1))
        return 0 if o["pass"] else 1
    if args.cmd == "summary":
        summary(args.a, args.step)
        return 0
    if args.cmd == "buildorder":
        for t, a, b in build_order(args.a, args.b, args.owner):
            print("%-12s orig %-8s clone %-8s %s" % (t, a, b, "" if a is None or b is None else "delta %+d" % (b - a)))
        return 0
    if args.cmd == "markers":
        print(json.dumps(markers(args.a), indent=1))
        return 0
    if args.cmd == "ladder":
        ladder(args.manifest)
        return 0


if __name__ == "__main__":
    sys.exit(main())
