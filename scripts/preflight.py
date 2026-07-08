#!/usr/bin/env python3
"""Verify the game install and binary before RE/oracle work."""
import hashlib, os, sys

EXPECTED_SHA1 = "7be4e5508655f8af482ab3ff5965d196e1b6b6c7"
install = os.environ.get("RA2_INSTALL_DIR", r"D:\Red Alert 2")

def sha1(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()

def main():
    exe = os.path.join(install, "gamemd.exe")
    if not os.path.isfile(exe):
        print(f"FAIL: gamemd.exe not found under {install}", file=sys.stderr)
        return 1
    got = sha1(exe)
    if got != EXPECTED_SHA1:
        print(f"FAIL: gamemd.exe SHA1 {got} != expected {EXPECTED_SHA1} (need YR 1.001)", file=sys.stderr)
        return 1
    for mix in ("ra2.mix", "ra2md.mix", "language.mix", "ecache01.mix"):
        if not os.path.isfile(os.path.join(install, mix)):
            print(f"WARN: expected MIX {mix} missing", file=sys.stderr)
    print(f"OK: install={install}, gamemd.exe verified (YR 1.001)")
    return 0

if __name__ == "__main__":
    sys.exit(main())
