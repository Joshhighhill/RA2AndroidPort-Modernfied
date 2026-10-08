import sys
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen import P
army = int(sys.argv[1]) if len(sys.argv) > 1 else 30   # 25 before the P4 crushable walls
stage = sys.argv[2] if len(sys.argv) > 2 else "80 80"
p=P("""# W5-A: ALL03 (Washington) won with real orders only (no cheats). Base from
# the sidebar (power, 2 refineries, barracks, war factory), Grizzly batches
# trained to an army of 30+ (topped up after raids), staged mid-map, then the type-selected tanks + IFVs
# attack the Psychic Beacon until it falls; the scripted Dugan/helicopter exit
# then wins the mission. (Needs the W5-A campaign economy: harvest income.)
set render_every 3000
set on_quit continue
set campaign ALL03u.MAP GDI
until 60 scene == campaign-load
click start
until 300 scene == game
wait 30
until 1500 input_locked == 0
""")
p.build("GAPOWR").build("GAREFN").build("GAPILE").build("GAPOWR").build("GAWEAP").build("GAREFN").build("GAPOWR")
for r in range(4):
    p.train("MTNK", 3, 10)
    want = min(army, (r+1)*9)
    # A player tops the batch up when the AI's raids thin it out (since the P4
    # crushable walls the AI tanks drive through the sandbags/fences, and the
    # raid losses vary): re-queue while short, then insist.
    for _ in range(3):
        p.add(f"waituntil 2000 units(0,MTNK) >= {want}")
        p.add(
            f"if units(0,MTNK) < {want} and cameo(MTNK) == 1 then click tab:3",
            *[f"if units(0,MTNK) < {want} and cameo(MTNK) == 1 then click cameo:MTNK" for _ in range(4)])
    p.add(f"until 4000 units(0,MTNK) >= {want}")
p.add("dump")
types = ["MTNK", "FV"]
p.select_types(types).add(f"click cell {stage}", "wait 600", "dump")
p.train("MTNK", 3, 10, guard=True)
for k in range(16):
    g = "outcome == RUNNING and buildings(1,NAPSYB) > 0"
    p.add(f"if {g} and units(0,MTNK) > 0 then click unit 0:MTNK:0",
          f"if {g} and units(0,FV) > 0 then click unit 0:FV:0 shift",
          f"if {g} then key t", f"if {g} then key t",
          f"if {g} then click building 1:NAPSYB:0", "if outcome == RUNNING then wait 300",
          "if outcome == RUNNING then dump")
    if k % 4 == 3: p.train("MTNK", 3, 6, guard=True)
p.add("until 6000 outcome == WON", "end expect WON")
p.save(sys.argv[3] if len(sys.argv) > 3 else "t-all03.play")
