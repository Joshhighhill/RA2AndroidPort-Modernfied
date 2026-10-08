import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen import P
out = sys.argv[1] if len(sys.argv) > 1 else "../m-sov11-real.play"
debug = len(sys.argv) > 2 and sys.argv[2] == "debug"
NEW = int(sys.argv[3]) if len(sys.argv) > 3 else 2
p = P("""# P5: SOV11 (Moscow) won with real orders only (no cheats). 'Yuri Defeated'
# (the Kremlin CARUS03's tag) is the win. Once battle control is online the
# sidebar builds power / refinery / barracks / war factory / radar / battle
# lab and two more Kirov airships join the map's two. All four are selected
# (a click on an airship, T T) and flown by move orders along the cheapest
# line through the flak belts -- past BadGuy1's south-west corner, up the
# western map edge, east onto the Kremlin -- and ordered to attack it; their
# bombs (BlimpHE, 50% vs concrete) bring the 800-HP Kremlin down.
set render_every 3000
set on_quit continue
set campaign sov11s.MAP Nod
until 60 scene == campaign-load
click start
until 300 scene == game
until 900 built(0,NACNST) >= 1
until 900 input_locked == 0
dump
""")


def dbg():
    if debug:
        p.add("if outcome == RUNNING then dump")


R = dict(robust=True)
for b in ["NAPOWR", "NAREFN", "NAHAND", "NAWEAP", "NARADR", "NAPOWR", "NATECH"]:
    p.build(b, **R)
    dbg()
# Kirovs from the war factory (vehicle tab 3)
p.add("click tab:3", "until 900 cameo(ZEP) == 1 and tab == 3", "wait 2")
for k in range(NEW):
    p.add(f"until 3000 queue(vehicle) == \"\" or queue(vehicle) == ZEP", "cycle cameo:ZEP queue(vehicle) == ZEP",
          f"until 4000 units(0,ZEP) >= {2 + k + 1} or credits < 100")
    dbg()
p.add("dump")


def sel_kirovs():
    # an airship is drawn ~4 cells above its ground cell: click the sprite
    p.add("if units(0,ZEP) > 0 and selected_type(ZEP) == 0 then click unit 0:ZEP:0 -4 -4", "key t", "key t")


# gather the new airships with the starting two first
p.add("click unit 0:ZEP:0 -4 -4", "key t", "key t", "assert selected_type(ZEP) >= 4")
p.add("click cell 100 148", "wait 30",
      "until 2000 pos_x(0,ZEP) >= 97 and pos_x(0,ZEP) <= 103 and pos_y(0,ZEP) >= 145 and pos_y(0,ZEP) <= 151",
      "wait 200")
route = [(86, 138), (68, 121), (51, 104), (27, 80), (16, 64), (25, 50), (37, 40)]
for (x, y) in route:
    sel_kirovs()
    p.add(f"click cell {x} {y}", "wait 30",
          f"until 3000 units(0,ZEP) == 0 or pos_x(0,ZEP) >= {x-3} and pos_x(0,ZEP) <= {x+3} "
          f"and pos_y(0,ZEP) >= {y-3} and pos_y(0,ZEP) <= {y+3}")
    dbg()
sel_kirovs()
p.add("click building 3:CARUS03", "wait 30")
for k in range(12):
    p.add("until 300 outcome != RUNNING",
          "if outcome == RUNNING and units(0,ZEP) > 0 and selected == 0 then click unit 0:ZEP:0 -4 -4",
          "if outcome == RUNNING and selected_type(ZEP) > 0 then click building 3:CARUS03")
    dbg()
p.add("until 3000 outcome == WON", "end expect WON")
p.save(out)

