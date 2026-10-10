#!/usr/bin/env python3
"""Run the ORIGINAL gamemd.exe FastMath / _ftol machine code in an x86 emulator
and record its results as the oracle for tests/test_orig_math.cpp.

Net L0. This is how the om:: port (engine/orig_math.cpp) is checked against
"values dumped from the binary": the original functions themselves are executed
(Unicorn / QEMU x87, under the Win32 default control word 0x027F) on a fixed,
deterministic input corpus, and their ST0 / EAX results are written as exact bit
patterns to tests/orig_math_oracle.inc. The engine and the tests never need
gamemd.exe; only a developer re-running this script does.

  pip install unicorn
  python tools/orig_math_oracle.py "D:/Red Alert 2/gamemd.exe" tests/orig_math_oracle.inc

Functions (RE build SHA-1 7BE4E550...):
  0x7C4D00 _ftol   0x4CAB60 sqrt   0x4CABD0 sin   0x4CAC20 cos   0x4CAC70 tan
  0x4CACA0 asin    0x4CACD0 acos   0x4CAD00 atan  0x4CAD50 atan2(y, x)
"""
import math
import random
import struct
import sys

from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX

sys.path.insert(0, __import__("os").path.dirname(__file__))
from extract_orig_math_tables import Pe, EXPECTED_SHA1  # noqa: E402

STUB = 0x00100000
SCRATCH = 0x00200000
STACK = 0x00300000
RET = STUB + 0x100


class Orig:
    def __init__(self, exe):
        self.pe = Pe(exe)
        assert self.pe.sha1 == EXPECTED_SHA1, self.pe.sha1
        mu = Uc(UC_ARCH_X86, UC_MODE_32)
        for sva, vsz, rptr, rsz in self.pe.secs:
            lo = sva & ~0xFFF
            hi = (sva + max(vsz, rsz) + 0xFFF) & ~0xFFF
            mu.mem_map(lo, hi - lo)
            mu.mem_write(sva, self.pe.data[rptr:rptr + min(rsz, max(vsz, rsz))])
        mu.mem_map(STUB, 0x1000)
        mu.mem_map(SCRATCH, 0x1000)
        mu.mem_map(STACK, 0x10000)
        mu.mem_write(SCRATCH, struct.pack("<H", 0x027F))   # Win32 default x87 CW
        code = bytes([0xDB, 0xE3])                           # FNINIT
        code += bytes([0xD9, 0x2D]) + struct.pack("<I", SCRATCH)        # FLDCW [SCRATCH]
        mu.mem_write(STUB, code)
        self.init_end = STUB + len(code)
        # at RET: FSTP qword [SCRATCH+8]
        fstp = bytes([0xDD, 0x1D]) + struct.pack("<I", SCRATCH + 8)
        mu.mem_write(RET, fstp)
        self.ret_end = RET + len(fstp)
        # FLD qword [SCRATCH+16] (ftol input)
        fld = bytes([0xDD, 0x05]) + struct.pack("<I", SCRATCH + 16)
        self.fld_at = STUB + 0x200
        mu.mem_write(self.fld_at, fld)
        self.fld_end = self.fld_at + len(fld)
        self.mu = mu

    def _reset(self):
        self.mu.emu_start(STUB, self.init_end)

    def call_d(self, fn, *args):
        """cdecl fn(double...), returns ST0 as double (bit pattern)."""
        self._reset()
        sp = STACK + 0x8000
        frame = struct.pack("<I", RET) + b"".join(struct.pack("<d", a) for a in args)
        self.mu.mem_write(sp, frame)
        self.mu.reg_write(UC_X86_REG_ESP, sp)
        self.mu.emu_start(fn, RET)
        self.mu.emu_start(RET, self.ret_end)
        return struct.unpack("<Q", bytes(self.mu.mem_read(SCRATCH + 8, 8)))[0]

    def ftol(self, x):
        self._reset()
        self.mu.mem_write(SCRATCH + 16, struct.pack("<d", x))
        self.mu.emu_start(self.fld_at, self.fld_end)
        sp = STACK + 0x8000
        self.mu.mem_write(sp, struct.pack("<I", RET + 0x40))
        self.mu.reg_write(UC_X86_REG_ESP, sp)
        self.mu.emu_start(0x7C4D00, RET + 0x40)
        return self.mu.reg_read(UC_X86_REG_EAX) & 0xFFFFFFFF


def corpus():
    rnd = random.Random(0x4C30)   # "L0"
    ftol_in = [0.0, -0.0, 0.5, -0.5, 1.999, -1.999, 2147483647.0, 2147483648.0, -2147483648.0,
               -2147483649.0, 4294967296.0 + 7, -4294967296.0 - 7, 1e18, -1e18, 9.3e18, -9.3e18,
               1e300, -1e300, float("inf"), float("-inf"), float("nan")]
    ftol_in += [rnd.uniform(-1e6, 1e6) for _ in range(200)] + [rnd.uniform(-1e12, 1e12) for _ in range(100)]
    sqrt_in = [0.0, -0.0, 1.0, 2.0, 3.0, 4.0, -4.0, 0.25, 65536.0, 1e-30, 1e30, 3.4e38, 1e39, float("nan")]
    sqrt_in += [float(rnd.randint(-7680, 7680) ** 2 + rnd.randint(-7680, 7680) ** 2) for _ in range(600)]
    sqrt_in += [rnd.uniform(0, 1e7) for _ in range(300)] + [rnd.uniform(-100, 0) for _ in range(50)]
    ang = [0.0, math.pi / 2, math.pi, -math.pi, 2 * math.pi, 1e-9, -1e-9, 100.0, -100.0, 1e5]
    ang += [k * 2 * math.pi / 256 for k in range(-300, 300, 7)]
    ang += [rnd.uniform(-20, 20) for _ in range(600)]
    unit = [-1.0, 1.0, 0.0, -0.9999, 0.9999, 1.0004, -1.0004] + [rnd.uniform(-1, 1) for _ in range(500)]
    at = [0.0, -0.0, 1.0, -1.0, 1e-6, 50.0, 99.9, 100.1, -100.1, 1e4, 5.3e7, 6e7, -6e7]
    at += [rnd.uniform(-120, 120) for _ in range(500)] + [rnd.uniform(-2, 2) for _ in range(300)]
    pairs = [(0.0, 0.0), (1.0, 0.0), (-1.0, 0.0), (0.0, 1.0), (0.0, -1.0), (1.0, 1.0), (-1.0, -1.0),
             (1e-3, -5.0), (-0.0, -3.0), (3.0, -0.0), (1e-46, 1.0), (1.0, 1e-46)]
    pairs += [(float(rnd.randint(-7680, 7680)), float(rnd.randint(-7680, 7680))) for _ in range(600)]
    pairs += [(rnd.uniform(-3, 3), rnd.uniform(-3, 3)) for _ in range(300)]
    return ftol_in, sqrt_in, ang, unit, at, pairs


def dbits(x):
    return struct.unpack("<Q", struct.pack("<d", x))[0]


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    o = Orig(argv[1])
    ftol_in, sqrt_in, ang, unit, at, pairs = corpus()
    out = ["// GENERATED by tools/orig_math_oracle.py -- DO NOT EDIT.",
           "// Results of the ORIGINAL gamemd.exe routines (executed in an x86 emulator)",
           "// on a fixed input corpus: {input bits..., result bits}. Included by tests/test_orig_math.cpp.",
           ""]

    def table(name, rows, width):
        out.append("const std::uint64_t %s[][%d] = {" % (name, width))
        for r in rows:
            out.append("    {" + ", ".join("0x%016xull" % v for v in r) + "},")
        out.append("};")
        out.append("")

    table("kOracleFtol", [(dbits(x), o.ftol(x)) for x in ftol_in], 2)
    table("kOracleSqrt", [(dbits(x), o.call_d(0x4CAB60, x)) for x in sqrt_in], 2)
    table("kOracleSin", [(dbits(x), o.call_d(0x4CABD0, x)) for x in ang], 2)
    table("kOracleCos", [(dbits(x), o.call_d(0x4CAC20, x)) for x in ang], 2)
    table("kOracleTan", [(dbits(x), o.call_d(0x4CAC70, x)) for x in ang], 2)
    inr = [x for x in unit if -1.0 <= x <= 1.0]
    table("kOracleAsin", [(dbits(x), o.call_d(0x4CACA0, x)) for x in inr], 2)
    table("kOracleAcos", [(dbits(x), o.call_d(0x4CACD0, x)) for x in inr], 2)
    table("kOracleAtan", [(dbits(x), o.call_d(0x4CAD00, x)) for x in at], 2)
    table("kOracleAtan2", [(dbits(y), dbits(x), o.call_d(0x4CAD50, y, x)) for y, x in pairs], 3)
    with open(argv[2], "w", newline="\n") as f:
        f.write("\n".join(out))
    print("wrote", argv[2])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
