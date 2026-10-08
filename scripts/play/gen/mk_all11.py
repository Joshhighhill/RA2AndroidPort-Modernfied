import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen import P
out = sys.argv[1] if len(sys.argv) > 1 else "../m-all11-real.play"
stage = tuple(int(v) for v in (sys.argv[2] if len(sys.argv) > 2 else "116 96").split())
dest = tuple(int(v) for v in (sys.argv[3] if len(sys.argv) > 3 else "185 194").split())
p = P("""# P1: ALL11 (Cuba) won with real orders only (no cheats).
# The map's three Chrono Miners harvest (P1 fix: Mission=Harvest). The player
# builds a war factory, Battle Lab and the Chronosphere ('Chronosphere is
# Built' reveals Cuba and hands the nuke silos to BadGuy2). IFVs, each
# carrying a Navy SEAL, gather in a block and are chronoshifted into the
# Tesla-free pocket of the silo compound; the SEALs get out and C4 the silos.
# All three silos fall -> 'Win (Nukes KIA)' -> VICTORY.
set render_every 3000
set on_quit continue
set campaign all11t.MAP GDI
until 60 scene == campaign-load
click start
until 300 scene == game
wait 30
until 3000 input_locked == 0
""")
sx, sy = stage
tx, ty = dest
G = "outcome == RUNNING and buildings(2,NAMISL) > 0"

def load_burst(n, w=400):
    # free SEAL j boards the j-th EMPTY IFV; all orders in one burst (stable indexes)
    p.add("let cz = carried(0)")
    for k in range(n):
        g = f"outcome == RUNNING and units(0,GHOST) > $cz+{k} and units(0,FV) > $cz+{k}"
        p.add(f"if {g} then click unit 0:GHOST:{k}", f"if {g} then click unit 0:FV~:{k}")
    p.add(f"wait {w}")

def pick_loaded(n):
    p.add(f"if {G} and units(0,FV) > 0 then click unit 0:FV:0")
    for k in range(1, n):
        p.add(f"if {G} and units(0,FV) > {k} then click unit 0:FV:{k} shift")

def strike(tag):
    pick_loaded(9)
    p.add(f"if {G} then click cell {sx} {sy}", f"if {G} then wait 300", "dump", f"view cell {sx} {sy}", f"shot staged{tag}")
    p.add(f"if {G} then click sw:ChronoSphere", f"if {G} then click point sel_pack",
          f"if {G} then click cell {tx} {ty}", "wait 2", "dump", f"shot chrono{tag}")
    p.add("until 300 chrono_units(0) == 0", "wait 2", f"if {G} then key d", "wait 20", "dump")
    for r in range(16):
        for k in range(9):
            want = [1, 1, 2, 2, 0, 0, 1, 2, 0][k]
            f_ = f"{G} and free(0,GHOST) > {k}"
            p.add(f"if {f_} then click unit 0:GHOST:{k}",
                  f"if {f_} and buildings(2,NAMISL) > {want} then click building 2:NAMISL:{want}",
                  f"if {f_} and buildings(2,NAMISL) <= {want} then click building 2:NAMISL:0")
        p.add("if outcome == RUNNING then wait 30", "if outcome == RUNNING then dump")
        if r == 1: p.add(f"shot fight{tag}")

p.train("GHOST", 2, 12)
p.build("GAPOWR").build("GAWEAP").build("GAPOWR").add("dump")
p.train("FV", 3, 12)
p.train("MTNK", 3, 2)
p.build("GATECH").add("dump")
p.add("until 900 can_build(GACSPH) == 1", "click tab:1", "click cameo:GACSPH", "until 3000 queue_ready(defense) == 1",
      "cycle cameo:GACSPH placing == 1", "place auto", "until 900 buildings(0,GACSPH) >= 1", "dump", "shot csph")
p.add("until 3000 units(0,FV) >= 9 and units(0,GHOST) >= 9", "dump")
load_burst(10)
p.add("dump", "shot loaded")
for _ in range(2):
    p.build("GAPOWR")
    p.build("ATESLA", tab=1, cat="defense").build("ATESLA", tab=1, cat="defense")
p.train("GHOST", 2, 5, guard=True)
p.add("until 9000 sw_ready_t(0,ChronoSphere) == 1", "dump")
load_burst(9, 150)
p.add("dump")
strike("1")
p.add("shot final", "until 3000 outcome == WON", "end expect WON")
p.save(out)
