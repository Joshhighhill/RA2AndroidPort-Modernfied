import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen import P
out = sys.argv[1] if len(sys.argv) > 1 else "../m-all11-real.play"
stage = tuple(int(v) for v in (sys.argv[2] if len(sys.argv) > 2 else "119 106").split())
dest = tuple(int(v) for v in (sys.argv[3] if len(sys.argv) > 3 else "190 195").split())
p = P("""# P1: ALL11 (Cuba) won with real orders only (no cheats).
# The map's three Chrono Miners harvest (P1 fix: Mission=Harvest). The player
# builds a war factory, Battle Lab and the Chronosphere ('Chronosphere is
# Built' reveals Cuba and hands the nuke silos to BadGuy2). IFVs, each
# carrying a Navy SEAL, gather in a block and are chronoshifted into the
# Tesla-free pocket of the silo compound; the SEALs get out and C4 the silos.
# All three silos fall -> 'Win (Nukes KIA)' -> VICTORY.
# W6 re-tune (full-strength teams, Patrol attack-move, guard-range
# acquisition, per-house difficulty handicap): Easy (the loading screen's
# Normal loses the radar / SEAL prerequisite to the landings). Two
# chronoshifts: the first takes two silos, the second the last one.
# W7 (original build times / speeds): group trained from the Battle Lab on,
# Chronosphere + a Prism pair inland, first shift ~f9950, WON f17780.
set render_every 3000
set on_quit continue
set campaign all11t.MAP GDI 0
until 60 scene == campaign-load
click start
until 300 scene == game
wait 30
until 3000 input_locked == 0
""")
sx, sy = stage
tx, ty = dest
G = "outcome == RUNNING and buildings(2,NAMISL) > 0"
# Holding ground south of the base, behind the defences: IFVs waiting on the
# open field north of it chase the landing invaders and die.
hx, hy = 112, 106
bx, by = 112, 92   # open ground north of the base (spare SEALs wait here)
csx, csy = 118, 90  # W7: Chronosphere anchor, inland (Clear, no Beach/Water cell near)

DESEL = "click xy 900 650 right nopark"   # right-click = deselect (original scheme)

def dodge(n=1, far=(112, 92)):
    p.add(f"until 6000 evareq(EVA_NuclearMissileLaunched) >= {n} or outcome != RUNNING", DESEL)
    p.select_types(["FV", "GHOST", "MTNK"])
    p.add(f"if outcome == RUNNING then click cell {far[0]} {far[1]} alt", "wait 330", DESEL)
    p.select_types(["FV", "GHOST", "MTNK"])
    p.add(f"if outcome == RUNNING then click cell {hx} {hy} alt", "wait 60", DESEL)

def spread_ifvs(n):
    # W6 (full-strength/Size wave): IFVs fresh from the factory park in a
    # packed 2x6 block at its exit and the SEALs cannot reach the inner ones
    # (one boarded out of ten). Like a player, park each empty IFV on its own
    # cell of the holding ground first (2-cell spacing).
    p.add(DESEL)   # a selected SEAL would take the first IFV click as "board"
    cells = [(hx - 4 + 2 * (k % 5), hy - 1 + 2 * (k // 5)) for k in range(n)]
    for k in reversed(range(n)):
        c = f"outcome == RUNNING and units(0,FV) > $cz+{k}"
        p.add(f"if {c} then click unit 0:FV~:{k}", f"if {c} then click cell {cells[k][0]} {cells[k][1]} alt")
    p.add("wait 200", DESEL)

def load_burst(n, w=400):
    # free SEAL j boards the j-th EMPTY IFV; all orders in one burst (stable indexes)
    p.add("let cz = carried(0)")
    spread_ifvs(n)
    # descending: a pair that boards mid-sweep only shifts HIGHER indexes
    # (already clicked), so every later (lower) index still names a free pair
    for k in reversed(range(n)):
        g = f"outcome == RUNNING and units(0,GHOST) > $cz+{k} and units(0,FV) > $cz+{k}"
        p.add(f"if {g} then click unit 0:GHOST:{k}", f"if {g} then click unit 0:FV~:{k}")
    p.add(f"wait {w}", DESEL)

def strike(tag):
    # W6: the group now waits on the holding ground, so a plain group move
    # to the stage leaves only ~5 IFVs inside the 3x3 the Chronosphere lifts.
    # Like a player, park the 9 IFVs one by one on the 9 cells of the 3x3
    # (the stage is flat level-2 ground: on the slope at the old (116,96)
    # several cells could not be reached).
    # SEALs left over from the boarding sweeps (every IFV full) stand on the
    # stage and block its cells: walk them back into the base first.
    for k in range(8):
        f_ = f"{G} and free(0,GHOST) > {k} and ucx(0,GHOST,{k}) < 150"   # home side only
        p.add(f"if {f_} then click unit 0:GHOST:{k}{'' if k == 0 else ' shift'}")
    p.add(f"if {G} and free(0,GHOST) > 0 then click cell {bx} {by} alt", f"if {G} then wait 60")
    p.add(DESEL)
    # Alt = force-move: a plain click on a cell an IFV crosses or stands on
    # would select it / unload it instead.
    # The second pass re-sends only an IFV that is not on its cell (a cell
    # still crossed by a neighbour on the first pass).
    for w, again in ((350, False), (200, True)):
        for k in range(12):   # 3x3 + a spare column
            cx, cy = (sx - 1 + k % 3, sy - 1 + k // 3) if k < 9 else (sx - 3, sy - 10 + k)
            c = f"{G} and units(0,FV) > {k} and ucx(0,FV,{k}) < 150" + (f" and ucell(0,FV,{k}) != {cx * 1000 + cy}" if again else "")
            p.add(f"if {c} then click unit 0:FV:{k}", f"if {c} then click cell {cx} {cy} alt")
        p.add(f"if {G} then wait {w}")
    # Box-select the parked group (clicking IFVs that overlap on screen can
    # toggle the wrong one); sel_pack = the 3x3 holding most of the selection.
    p.add(DESEL, f"if {G} then drag cell {sx - 3} {sy} {sx + 3} {sy}", "wait 2")
    p.add("dump", f"view cell {sx} {sy}", f"shot staged{tag}")
    p.add(f"if {G} then click sw:ChronoSphere", f"if {G} then click point sel_pack",
          f"if {G} then click cell {tx} {ty}", "wait 2", "dump", f"shot chrono{tag}")
    p.add("until 300 chrono_units(0) == 0", "wait 2", f"if {G} then key d", "wait 20", "dump")
    for r in range(16):
        # 16 indexes: the spare SEALs left at home (and the ones trained
        # meanwhile) take low indexes, the shifted ones sit above them;
        # only the SEALs on Cuba (x > 150) are sent.
        for k in range(16):
            want = [1, 1, 2, 2, 0, 0, 1, 2, 0][k % 9]
            f_ = f"{G} and free(0,GHOST) > {k} and ucx(0,GHOST,{k}) > 150"   # the SEALs on Cuba
            p.add(f"if {f_} then click unit 0:GHOST:{k}",
                  f"if {f_} and buildings(2,NAMISL) > {want} then click building 2:NAMISL:{want}",
                  f"if {f_} and buildings(2,NAMISL) <= {want} then click building 2:NAMISL:0")
        p.add("if outcome == RUNNING then wait 30", "if outcome == RUNNING then dump")
        if r == 1: p.add(f"shot fight{tag}")

p.build("GAPOWR").build("GAWEAP")
# War factory rally on the holding ground: IFVs parked in a block at the
# factory exit could not be ordered out (W6 re-tune).
p.add("click building 0:GAWEAP", "wait 2", f"click cell {hx} {hy}", "wait 2", DESEL)
p.build("GAPOWR").add("dump")
# W6 re-tune: the invaders now land at full strength, the BadGuy2 Dreadnought
# patrol attack-moves along the south shore, and idle units pick targets up at
# guard range -- an army that waits in the base for minutes is whittled down.
# Grizzlies hold the base (W7: the SEALs and IFVs now start with the
# Battle Lab, see below).
p.train("MTNK", 3, 6)
p.build("GATECH").add("dump")
# W7 re-tune (original build times: BuildTimeMultiplier, one SEAL ~400
# frames from one barracks): the strike group is the long pole now. Start
# the SEALs and IFVs the moment the Battle Lab stands, and add a second
# barracks and war factory (MultipleFactory) while they train.
p.train("GHOST", 2, 12)
p.train("FV", 3, 12)
# W7: the Chronosphere goes inland, on the clear plateau east of the yard.
# 'place auto' put it on the west shore, partly on Beach cells -- a naval
# target (GetFireError: the target cell's LandType Water/Beach), and the
# BadGuy2 Typhoon patrol torpedoed it to death while it waited charged.
p.add("until 900 can_build(GACSPH) == 1", "click tab:1", "click cameo:GACSPH", "until 3000 queue_ready(defense) == 1",
      "cycle cameo:GACSPH placing == 1", f"place near {csx} {csy}")
for _ in range(4):
    p.add("wait 1", f"if placing == 1 then place near {csx} {csy}")
p.add("until 900 buildings(0,GACSPH) >= 1", "dump", "view building 0:GACSPH", "shot csph")
p.build("GAPILE")
# fv8b: since w8fix-place (FUN_0047C530: no building on overlay / slope /
# bridge / unbuildable land) 'place auto' put this war factory on the
# holding ground (111,107), where the IFVs park for boarding -- packed
# between the factories, only 5 of 10 SEALs got in, and its old spot
# (101,106) is now refused. Like a player, put it south-east of the holding
# ground (it lands on (119,109)).
WF2 = tuple(int(v) for v in os.environ.get("MK_ALL11_WF2", "114 111").split())
p.build("GAWEAP", near=WF2)
p.add("click building 0:GAWEAP:1", "wait 2", f"click cell {hx} {hy}", "wait 2", DESEL)
# W7: no Prism/power build-out before the strike any more -- the structure
# steps kept the script busy ~2500 frames after the group stood ready, and
# the late shift met BadGuy2's first nuke (charged from the hand-over).
# Only a Prism pair beside the inland Chronosphere while the group trains
# (the structure queue is idle then): landing parties reached it from the
# north road and burnt it down right after the first shift.
p.build("ATESLA", tab=1, cat="defense", near=(csx, csy)).build("ATESLA", tab=1, cat="defense", near=(csx, csy))
p.add("until 3000 units(0,FV) >= 9 and units(0,GHOST) >= 9", "dump")
p.train("FV", 3, 2, guard=True)
p.train("GHOST", 2, 3, guard=True)
def wave(tag):
    load_burst(10)
    p.add("dump", f"shot loaded{tag}")
    # W6 (guard-range acquisition): an idle loaded IFV picks up the landing
    # invaders at its guard range and chases them out of the base. Like a
    # player, keep calling the group back to the holding ground until the
    # Chronosphere is ready.
    W = f"{G} and sw_ready_t(0,ChronoSphere) == 0"
    for _ in range(60):
        p.add(DESEL)
        for k in range(12):
            p.add(f"if {W} and units(0,FV) > {k} and ucx(0,FV,{k}) < 150 then click unit 0:FV:{k} shift")
        p.add(f"if {W} then click cell {hx} {hy} alt", f"if {W} then wait 150")
    p.add(DESEL, "until 9000 sw_ready_t(0,ChronoSphere) == 1 or outcome != RUNNING or buildings(2,NAMISL) == 0", "dump")
    load_burst(9, 150)
    p.add("dump")
    strike(tag)

wave("1")
# MR2: with the extra factories the first strike can take all three silos.
# W7: 'Win (Nukes KIA)' fires ~150 frames after the last silo falls.
p.add("waituntil 600 outcome != RUNNING or buildings(2,NAMISL) > 0",
      "if outcome == WON then end expect WON")
# Fallback (second strike): defend the base while the group re-forms.
for _ in range(2):
    p.build("GAPOWR")
    p.build("ATESLA", tab=1, cat="defense").build("ATESLA", tab=1, cat="defense")
# W6 re-tune: the first strike now takes two of the three silos (the Cuba
# garrison kills the SEALs heading for silo A). The Chronosphere recharges
# well before the nuke timer runs out: train a second, smaller group from
# the same base and shift it again; every SEAL then goes for the last silo.
p.train("FV", 3, 9, guard=True)
p.train("GHOST", 2, 10, guard=True)
# MR2: the second nuke (~6800 frames after the first) lands while this
# group trains: dodge it too, then let the factories finish.
dodge(1)
p.add(f"if {G} then wait 900")
wave("2")
p.add("shot final", "until 3000 outcome == WON", "end expect WON")
# W7: a unit named by index can vanish between the guard and the click (it
# died, or boarded/unloaded this frame): skip such a click instead of failing.
p.l = [x + " soft" if " then click unit 0:" in x and not x.endswith(" soft") else x for x in p.l]
p.save(out)
