#!/usr/bin/env python3
"""Proof-of-spec Bink1 'i' video decoder (pure Python port of the documented
algorithm). Verifies bit-exactness against ffmpeg's yuv420p raw output.
Tables are parsed straight out of ffmpeg binkdata.h to avoid transcription errors.
"""
import re, struct, sys

SCRATCH = r"C:\Users\Omar\AppData\Local\Temp\claude\d--Red-Alert-2\d44bece6-5e7c-465a-9fd5-820d808b4426\scratchpad"

# ---------------- tables from binkdata.h ----------------
src = open(SCRATCH + r"\ffbink\binkdata.h").read()

def carray(name):
    m = re.search(re.escape(name) + r"\s*\[[^=]*=\s*\{(.*?)\n\};", src, re.S)
    body = m.group(1)
    # strip nested braces, parse all integer literals in order
    vals = [int(x, 0) for x in re.findall(r"0[xX][0-9a-fA-F]+|\d+", body)]
    return vals

bink_scan      = carray("bink_scan")                      # 64
tree_bits_flat = carray("bink_tree_bits")                 # 16*16
tree_lens_flat = carray("bink_tree_lens")                 # 16*16
patterns_flat  = carray("bink_patterns")                  # 16*64
intra_q_flat   = carray("bink_intra_quant")               # 16*64
inter_q_flat   = carray("bink_inter_quant")               # 16*64
assert len(bink_scan)==64 and len(tree_bits_flat)==256 and len(tree_lens_flat)==256
assert len(patterns_flat)==1024 and len(intra_q_flat)==1024 and len(inter_q_flat)==1024

TREE_BITS = [tree_bits_flat[i*16:(i+1)*16] for i in range(16)]
TREE_LENS = [tree_lens_flat[i*16:(i+1)*16] for i in range(16)]
PATTERNS  = [patterns_flat[i*64:(i+1)*64] for i in range(16)]
INTRA_Q   = [intra_q_flat[i*64:(i+1)*64] for i in range(16)]
INTER_Q   = [inter_q_flat[i*64:(i+1)*64] for i in range(16)]

# LSB-first VLC decode maps: per tree, dict[(nbits, code_lsbfirst)] -> leaf index
TREE_MAP = []
for t in range(16):
    d = {}
    for leaf in range(16):
        d[(TREE_LENS[t][leaf], TREE_BITS[t][leaf])] = leaf
    TREE_MAP.append(d)
MAXLEN = [max(TREE_LENS[t]) for t in range(16)]

# ---------------- bit reader (LSB-first) ----------------
class BR:
    __slots__=("data","pos","size")
    def __init__(self, data):
        self.data = data + b"\0"*8
        self.pos = 0
        self.size = len(data)*8
    def get(self, n):
        if n == 0: return 0
        p = self.pos
        v = (int.from_bytes(self.data[p>>3:(p>>3)+((n+ (p&7) +7)>>3)+1], "little") >> (p&7)) & ((1<<n)-1)
        self.pos = p + n
        return v
    def get1(self):
        p = self.pos
        v = (self.data[p>>3] >> (p&7)) & 1
        self.pos = p+1
        return v
    def align32(self):
        if self.pos & 31:
            self.pos += 32 - (self.pos & 31)

def get_huff(br, vlc_num, syms):
    d = TREE_MAP[vlc_num]; acc = 0
    for depth in range(1, MAXLEN[vlc_num]+1):
        acc |= br.get1() << (depth-1)
        leaf = d.get((depth, acc))
        if leaf is not None:
            return syms[leaf]
    raise ValueError("bad code")

# ---------------- tree descriptor reading ----------------
def merge(br, dst, doff, src_, soff, size):
    p1, p2 = soff, soff+size
    n1, n2 = size, size
    o = doff
    while n1 and n2:
        if not br.get1():
            dst[o] = src_[p1]; p1 += 1; n1 -= 1
        else:
            dst[o] = src_[p2]; p2 += 1; n2 -= 1
        o += 1
    while n1: dst[o]=src_[p1]; p1+=1; n1-=1; o+=1
    while n2: dst[o]=src_[p2]; p2+=1; n2-=1; o+=1

def read_tree(br):
    vlc_num = br.get(4)
    if vlc_num == 0:
        return (0, list(range(16)))
    syms = [0]*16
    if br.get1():
        used = [0]*16
        ln = br.get(3)
        for i in range(ln+1):
            syms[i] = br.get(4)
            used[syms[i]] = 1
        i = 0
        while ln < 15 and i < 16:
            if not used[i]:
                ln += 1
                syms[ln] = i
            i += 1
    else:
        ln = br.get(2)
        a = list(range(16)); b = [0]*16
        for i in range(ln+1):
            size = 1 << i
            for t in range(0, 16, size*2):
                merge(br, b, t, a, t, size)
            a, b = b, a
        syms = a[:]
    return (vlc_num, syms)

# ---------------- bundles ----------------
(BT, SBT, COL, PAT, XO, YO, IDC, PDC, RUN) = range(9)
RLELENS = [4, 8, 12, 32]

class Bundle:
    __slots__=("lenbits","tree","data","dec","ptr","dead")
    def __init__(self):
        self.data=[]; self.dec=0; self.ptr=0; self.dead=False
        self.tree=(0,list(range(16))); self.lenbits=0
    def reset(self):
        self.data=[]; self.dec=0; self.ptr=0; self.dead=False
    def empty(self):
        return not self.dead and self.dec <= self.ptr

def av_log2(v): return v.bit_length()-1

class Plane:
    pass

class BinkDec:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.b = [Bundle() for _ in range(9)]
        self.col_high = [(0,list(range(16)))]*16
        self.col_lastval = 0
        # planes: index 0=Y,1=U,2=V (storage), decode order Y,V,U (swap_planes for >= 'h')
        self.cur = []; self.last = []
        self.geom = []
        for i in range(3):
            cw = w if i==0 else (w+1)>>1
            ch = h if i==0 else (h+1)>>1
            bw = (w+7)>>3 if i==0 else (w+15)>>4
            bh = (h+7)>>3 if i==0 else (h+15)>>4
            stride, rows = bw*8, bh*8
            self.geom.append((cw,ch,bw,bh,stride,rows))
            self.cur.append(bytearray(stride*rows))
            self.last.append(bytearray(stride*rows))
        self.first = True

    # ---- bundle refills ----
    def check_count(self, br, b):
        if b.dead or b.dec > b.ptr: return 0
        t = br.get(b.lenbits)
        if t == 0:
            b.dead = True
        return t

    def read_runs(self, br, b):
        t = self.check_count(br, b)
        if not t: return
        if br.get1():
            v = br.get(4)
            b.data.extend([v]*t); b.dec += t
        else:
            for _ in range(t):
                b.data.append(get_huff(br, *b.tree))
            b.dec += t

    def read_motion(self, br, b):
        t = self.check_count(br, b)
        if not t: return
        if br.get1():
            v = br.get(4)
            if v: v = -v if br.get1() else v
            b.data.extend([v]*t); b.dec += t
        else:
            for _ in range(t):
                v = get_huff(br, *b.tree)
                if v: v = -v if br.get1() else v
                b.data.append(v)
            b.dec += t

    def read_block_types(self, br, b):
        t = self.check_count(br, b)
        if not t: return
        if br.get1():
            v = br.get(4)
            b.data.extend([v]*t); b.dec += t
        else:
            last = 0; end = b.dec + t
            while b.dec < end:
                v = get_huff(br, *b.tree)
                if v < 12:
                    last = v; b.data.append(v); b.dec += 1
                else:
                    run = RLELENS[v-12]
                    b.data.extend([last]*run); b.dec += run

    def read_patterns(self, br, b):
        t = self.check_count(br, b)
        if not t: return
        for _ in range(t):
            v  = get_huff(br, *b.tree)
            v |= get_huff(br, *b.tree) << 4
            b.data.append(v)
        b.dec += t

    def read_colors(self, br, b):
        t = self.check_count(br, b)
        if not t: return
        if br.get1():
            self.col_lastval = get_huff(br, *self.col_high[self.col_lastval])
            v = get_huff(br, *b.tree)
            v = (self.col_lastval << 4) | v      # version 'i': no sign-fold
            b.data.extend([v]*t); b.dec += t
        else:
            for _ in range(t):
                self.col_lastval = get_huff(br, *self.col_high[self.col_lastval])
                v = get_huff(br, *b.tree)
                v = (self.col_lastval << 4) | v
                b.data.append(v)
            b.dec += t

    def read_dcs(self, br, b, has_sign):
        ln = self.check_count(br, b)
        if not ln: return
        v = br.get(11 - has_sign)
        if v and has_sign:
            if br.get1(): v = -v
        b.data.append(v); b.dec += 1
        ln -= 1
        i = 0
        while i < ln:
            len2 = min(ln - i, 8)
            bsize = br.get(4)
            if bsize:
                for _ in range(len2):
                    v2 = br.get(bsize)
                    if v2:
                        if br.get1(): v2 = -v2
                    v += v2
                    b.data.append(v)
            else:
                b.data.extend([v]*len2)
            b.dec += len2
            i += 8

    def get_value(self, idx):
        b = self.b[idx]
        v = b.data[b.ptr]; b.ptr += 1
        return v

    # ---- DCT/residue ----
    def read_dct_coeffs(self, br, block, quant_tab):
        coef_list = [0]*128; mode_list = [0]*128
        ls, le = 64, 64
        coef_list[le]=4;  mode_list[le]=0; le+=1
        coef_list[le]=24; mode_list[le]=0; le+=1
        coef_list[le]=44; mode_list[le]=0; le+=1
        coef_list[le]=1;  mode_list[le]=3; le+=1
        coef_list[le]=2;  mode_list[le]=3; le+=1
        coef_list[le]=3;  mode_list[le]=3; le+=1
        coef_idx = []
        bits = br.get(4) - 1
        while bits >= 0:
            pos = ls
            while pos < le:
                if not (mode_list[pos] | coef_list[pos]) or not br.get1():
                    pos += 1
                    continue
                ccoef = coef_list[pos]; mode = mode_list[pos]
                if mode == 0 or mode == 2:
                    if mode == 0:
                        coef_list[pos] = ccoef + 4
                        mode_list[pos] = 1
                    else:
                        coef_list[pos] = 0
                        mode_list[pos] = 0
                        pos += 1
                    for _ in range(4):
                        if br.get1():
                            ls -= 1
                            coef_list[ls] = ccoef
                            mode_list[ls] = 3
                        else:
                            if not bits:
                                t = 1 - (br.get1() << 1)
                            else:
                                t = br.get(bits) | (1 << bits)
                                if br.get1(): t = -t
                            block[bink_scan[ccoef]] = t
                            coef_idx.append(ccoef)
                        ccoef += 1
                elif mode == 1:
                    mode_list[pos] = 2
                    for _ in range(3):
                        ccoef += 4
                        coef_list[le] = ccoef
                        mode_list[le] = 2
                        le += 1
                else: # mode 3
                    if not bits:
                        t = 1 - (br.get1() << 1)
                    else:
                        t = br.get(bits) | (1 << bits)
                        if br.get1(): t = -t
                    block[bink_scan[ccoef]] = t
                    coef_idx.append(ccoef)
                    coef_list[pos] = 0
                    mode_list[pos] = 0
                    pos += 1
            bits -= 1
        quant_idx = br.get(4)
        quant = quant_tab[quant_idx]
        # dequant (32-bit wrapping multiply, arithmetic >>11)
        def mulq(a, q):
            p = (a * q) & 0xFFFFFFFF
            if p >= 0x80000000: p -= 0x100000000
            return p >> 11
        block[0] = mulq(block[0], quant[0])
        for idx in coef_idx:
            block[bink_scan[idx]] = mulq(block[bink_scan[idx]], quant[idx])

    def read_residue(self, br, block, masks_count):
        coef_list = [0]*128; mode_list = [0]*128
        ls, le = 64, 64
        coef_list[le]=4;  mode_list[le]=0; le+=1
        coef_list[le]=24; mode_list[le]=0; le+=1
        coef_list[le]=44; mode_list[le]=0; le+=1
        coef_list[le]=0;  mode_list[le]=2; le+=1
        nz = []
        mask = 1 << br.get(3)
        while mask:
            for pos_c in nz:
                if not br.get1(): continue
                if block[pos_c] < 0: block[pos_c] -= mask
                else:                block[pos_c] += mask
                masks_count -= 1
                if masks_count < 0: return
            pos = ls
            while pos < le:
                if not (coef_list[pos] | mode_list[pos]) or not br.get1():
                    pos += 1
                    continue
                ccoef = coef_list[pos]; mode = mode_list[pos]
                if mode == 0 or mode == 2:
                    if mode == 0:
                        coef_list[pos] = ccoef + 4
                        mode_list[pos] = 1
                    else:
                        coef_list[pos] = 0
                        mode_list[pos] = 0
                        pos += 1
                    for _ in range(4):
                        if br.get1():
                            ls -= 1
                            coef_list[ls] = ccoef
                            mode_list[ls] = 3
                        else:
                            p = bink_scan[ccoef]
                            nz.append(p)
                            block[p] = -mask if br.get1() else mask
                            masks_count -= 1
                            if masks_count < 0: return
                        ccoef += 1
                elif mode == 1:
                    mode_list[pos] = 2
                    for _ in range(3):
                        ccoef += 4
                        coef_list[le] = ccoef
                        mode_list[le] = 2
                        le += 1
                else:
                    p = bink_scan[ccoef]
                    nz.append(p)
                    block[p] = -mask if br.get1() else mask
                    coef_list[pos] = 0
                    mode_list[pos] = 0
                    pos += 1
                    masks_count -= 1
                    if masks_count < 0: return
            mask >>= 1

    # ---- IDCT ----
    A1, A2, A3, A4 = 2896, 2217, 3784, -5352
    @staticmethod
    def _mul(x, y):
        p = (x * y) & 0xFFFFFFFF
        if p >= 0x80000000: p -= 0x100000000
        return p >> 11

    def idct_core(self, blk):
        """columns then rows; returns 64 ints after row munge (+0x7F)>>8"""
        M = self._mul
        tmp = [0]*64
        for i in range(8):
            s = blk[i::8]
            if s[1]|s[2]|s[3]|s[4]|s[5]|s[6]|s[7] == 0:
                for r in range(8): tmp[i+8*r] = s[0]
                continue
            a0 = s[0]+s[4]; a1 = s[0]-s[4]
            a2 = s[2]+s[6]; a3 = M(self.A1, s[2]-s[6])
            a4 = s[5]+s[3]; a5 = s[5]-s[3]
            a6 = s[1]+s[7]; a7 = s[1]-s[7]
            b0 = a4+a6
            b1 = M(self.A3, a5+a7)
            b2 = M(self.A4, a5) - b0 + b1
            b3 = M(self.A1, a6-a4) - b2
            b4 = M(self.A2, a7) + b3 - b1
            col = (a0+a2+b0, a1+a3-a2+b2, a1-a3+a2+b3, a0-a2-b4,
                   a0-a2+b4, a1-a3+a2-b3, a1+a3-a2-b2, a0+a2-b0)
            for r in range(8): tmp[i+8*r] = col[r]
        out = [0]*64
        for r in range(8):
            s = tmp[8*r:8*r+8]
            a0 = s[0]+s[4]; a1 = s[0]-s[4]
            a2 = s[2]+s[6]; a3 = M(self.A1, s[2]-s[6])
            a4 = s[5]+s[3]; a5 = s[5]-s[3]
            a6 = s[1]+s[7]; a7 = s[1]-s[7]
            b0 = a4+a6
            b1 = M(self.A3, a5+a7)
            b2 = M(self.A4, a5) - b0 + b1
            b3 = M(self.A1, a6-a4) - b2
            b4 = M(self.A2, a7) + b3 - b1
            row = (a0+a2+b0, a1+a3-a2+b2, a1-a3+a2+b3, a0-a2-b4,
                   a0-a2+b4, a1-a3+a2-b3, a1+a3-a2-b2, a0+a2-b0)
            for cidx in range(8):
                out[8*r+cidx] = (row[cidx] + 0x7F) >> 8
        return out

    # ---- plane decode ----
    def decode_plane(self, br, pidx, is_chroma):
        cw, ch, bw, bh, stride, rows = self.geom[pidx]
        width = max(cw, 8)
        aw = (width + 7) & ~7
        b = self.b
        b[BT].lenbits  = av_log2((aw>>3)+511)+1
        b[SBT].lenbits = av_log2((aw>>4)+511)+1
        b[COL].lenbits = av_log2(bw*64+511)+1
        for k in (IDC, PDC, XO, YO):
            b[k].lenbits = av_log2((aw>>3)+511)+1
        b[PAT].lenbits = av_log2((bw<<3)+511)+1
        b[RUN].lenbits = av_log2(bw*48+511)+1

        for i in range(9):
            if i == COL:
                self.col_high = [read_tree(br) for _ in range(16)]
                self.col_lastval = 0
            if i not in (IDC, PDC):
                b[i].tree = read_tree(br)
            b[i].reset()

        dst = self.cur[pidx]
        prev = self.last[pidx] if not self.first else self.cur[pidx]
        coordmap = [(i & 7) + (i >> 3)*stride for i in range(64)]

        for by in range(bh):
            self.read_block_types(br, b[BT])
            self.read_block_types(br, b[SBT])
            self.read_colors(br, b[COL])
            self.read_patterns(br, b[PAT])
            self.read_motion(br, b[XO])
            self.read_motion(br, b[YO])
            self.read_dcs(br, b[IDC], 0)
            self.read_dcs(br, b[PDC], 1)
            self.read_runs(br, b[RUN])

            base = 8*by*stride
            bx = 0
            while bx < bw:
                doff = base + bx*8
                blk = self.get_value(BT)
                if ((by & 1) or (bx & 1)) and blk == 1:  # SCALED on odd pos: skip
                    bx += 2
                    continue
                if blk == 0:      # SKIP
                    for r in range(8):
                        dst[doff+r*stride:doff+r*stride+8] = prev[doff+r*stride:doff+r*stride+8]
                elif blk == 1:    # SCALED
                    sub = self.get_value(SBT)
                    ublock = bytearray(64)
                    if sub == 3:      # RUN
                        scan = PATTERNS[br.get(4)]
                        i = 0; si = 0
                        while True:
                            run = self.get_value(RUN) + 1
                            i += run
                            if i > 64: raise ValueError("run oob")
                            if br.get1():
                                v = self.get_value(COL)
                                for _ in range(run):
                                    ublock[scan[si]] = v; si += 1
                            else:
                                for _ in range(run):
                                    ublock[scan[si]] = self.get_value(COL); si += 1
                            if i >= 63: break
                        if i == 63:
                            ublock[scan[si]] = self.get_value(COL); si += 1
                    elif sub == 5:    # INTRA
                        blkc = [0]*64
                        blkc[0] = self.get_value(IDC)
                        self.read_dct_coeffs(br, blkc, INTRA_Q)
                        out = self.idct_core(blkc)
                        for k in range(64): ublock[k] = out[k] & 0xFF
                    elif sub == 6:    # FILL (writes 16x16 directly)
                        v = self.get_value(COL)
                        for r in range(16):
                            dst[doff+r*stride:doff+r*stride+16] = bytes([v])*16
                    elif sub == 8:    # PATTERN
                        c0 = self.get_value(COL); c1 = self.get_value(COL)
                        cols = (c0, c1)
                        for j in range(8):
                            v = self.get_value(PAT)
                            for i2 in range(8):
                                ublock[i2 + j*8] = cols[v & 1]; v >>= 1
                    elif sub == 9:    # RAW
                        for j in range(8):
                            for i2 in range(8):
                                ublock[i2 + j*8] = self.get_value(COL)
                    else:
                        raise ValueError("bad 16x16 sub type %d" % sub)
                    if sub != 6:
                        # scale 2x
                        for j in range(8):
                            row = bytearray()
                            for i2 in range(8):
                                row += bytes((ublock[j*8+i2], ublock[j*8+i2]))
                            dst[doff+2*j*stride:doff+2*j*stride+16] = row
                            dst[doff+(2*j+1)*stride:doff+(2*j+1)*stride+16] = row
                    bx += 2
                    continue
                elif blk == 2:    # MOTION
                    xoff = self.get_value(XO); yoff = self.get_value(YO)
                    roff = doff + xoff + yoff*stride
                    for r in range(8):
                        dst[doff+r*stride:doff+r*stride+8] = prev[roff+r*stride:roff+r*stride+8]
                elif blk == 3:    # RUN
                    scan = PATTERNS[br.get(4)]
                    i = 0; si = 0
                    while True:
                        run = self.get_value(RUN) + 1
                        i += run
                        if i > 64: raise ValueError("run oob")
                        if br.get1():
                            v = self.get_value(COL)
                            for _ in range(run):
                                dst[doff+coordmap[scan[si]]] = v; si += 1
                        else:
                            for _ in range(run):
                                dst[doff+coordmap[scan[si]]] = self.get_value(COL); si += 1
                        if i >= 63: break
                    if i == 63:
                        dst[doff+coordmap[scan[si]]] = self.get_value(COL)
                elif blk == 4:    # RESIDUE
                    xoff = self.get_value(XO); yoff = self.get_value(YO)
                    roff = doff + xoff + yoff*stride
                    for r in range(8):
                        dst[doff+r*stride:doff+r*stride+8] = prev[roff+r*stride:roff+r*stride+8]
                    block = [0]*64
                    v = br.get(7)
                    self.read_residue(br, block, v)
                    for r in range(8):
                        for i2 in range(8):
                            o = doff+r*stride+i2
                            dst[o] = (dst[o] + block[r*8+i2]) & 0xFF
                elif blk == 5:    # INTRA
                    blkc = [0]*64
                    blkc[0] = self.get_value(IDC)
                    self.read_dct_coeffs(br, blkc, INTRA_Q)
                    out = self.idct_core(blkc)
                    for r in range(8):
                        for i2 in range(8):
                            dst[doff+r*stride+i2] = out[r*8+i2] & 0xFF
                elif blk == 6:    # FILL
                    v = self.get_value(COL)
                    for r in range(8):
                        dst[doff+r*stride:doff+r*stride+8] = bytes([v])*8
                elif blk == 7:    # INTER
                    xoff = self.get_value(XO); yoff = self.get_value(YO)
                    roff = doff + xoff + yoff*stride
                    for r in range(8):
                        dst[doff+r*stride:doff+r*stride+8] = prev[roff+r*stride:roff+r*stride+8]
                    blkc = [0]*64
                    blkc[0] = self.get_value(PDC)
                    self.read_dct_coeffs(br, blkc, INTER_Q)
                    out = self.idct_core(blkc)
                    for r in range(8):
                        for i2 in range(8):
                            o = doff+r*stride+i2
                            dst[o] = (dst[o] + out[r*8+i2]) & 0xFF
                elif blk == 8:    # PATTERN
                    c0 = self.get_value(COL); c1 = self.get_value(COL)
                    cols = (c0, c1)
                    for r in range(8):
                        v = self.get_value(PAT)
                        for i2 in range(8):
                            dst[doff+r*stride+i2] = cols[v & 1]; v >>= 1
                elif blk == 9:    # RAW
                    bcol = self.b[COL]
                    for r in range(8):
                        for i2 in range(8):
                            dst[doff+r*stride+i2] = bcol.data[bcol.ptr + r*8 + i2]
                    bcol.ptr += 64
                else:
                    raise ValueError("bad block type %d" % blk)
                bx += 1
        br.align32()

    def decode_frame(self, pkt):
        br = BR(pkt)
        br.get(32)   # version >= 'i': luma plane size prefix (no alpha here)
        # decode order Y, V, U  (swap_planes for version >= 'h')
        for storage_idx, is_chroma in ((0,0),(2,1),(1,1)):
            self.decode_plane(br, storage_idx, is_chroma)
            if br.pos >= br.size:
                break
        # swap cur/last
        for i in range(3):
            self.last[i][:] = self.cur[i]
        self.first = False
        # return cropped planes
        outs = []
        for i in range(3):
            cw, ch, bw, bh, stride, rows = self.geom[i]
            p = bytearray(cw*ch)
            for r in range(ch):
                p[r*cw:(r+1)*cw] = self.cur[i][r*stride:r*stride+cw]
            outs.append(bytes(p))
        return outs

def main():
    nframes_test = int(sys.argv[1]) if len(sys.argv) > 1 else 12
    data = open(SCRATCH + r"\ra2ts_s.bik", "rb").read()
    u32 = lambda o: struct.unpack_from("<I", data, o)[0]
    n = u32(8); w = u32(20); h = u32(24)
    idx = [u32(44+4*i) & ~1 for i in range(n+1)]
    dec = BinkDec(w, h)
    oracle = open(SCRATCH + r"\oracle12.yuv", "rb")
    ysz = w*h; csz = ((w+1)>>1)*((h+1)>>1)
    ok = True
    for f in range(min(nframes_test, n)):
        pkt = data[idx[f]:idx[f+1]]
        y, u, v = dec.decode_frame(pkt)
        oy = oracle.read(ysz); ou = oracle.read(csz); ov = oracle.read(csz)
        my = "OK" if y == oy else "MISMATCH"
        mu = "OK" if u == ou else "MISMATCH"
        mv = "OK" if v == ov else "MISMATCH"
        if "MISMATCH" in (my, mu, mv):
            ok = False
            # diagnose first differing byte
            for nm, a, bb in (("Y",y,oy),("U",u,ou),("V",v,ov)):
                if a != bb:
                    d = next(i for i in range(len(a)) if a[i]!=bb[i])
                    cw = w if nm=="Y" else (w+1)>>1
                    print(f"  {nm} first diff at pixel ({d%cw},{d//cw}): mine={a[d]} oracle={bb[d]}")
        print(f"frame {f}: Y {my}  U {mu}  V {mv}")
        if not ok:
            break
    print("ALL MATCH" if ok else "FAILED")

if __name__ == "__main__":
    main()
