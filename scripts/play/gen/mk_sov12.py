import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen import P
out = sys.argv[1] if len(sys.argv) > 1 else "../m-sov12-real.play"
debug = len(sys.argv) > 2 and sys.argv[2] == "debug"
NEW = int(sys.argv[3]) if len(sys.argv) > 3 else 2
TESLA = len(sys.argv) > 4 and sys.argv[4] == "tesla"
p = P("""# P5: SOV12 (the Allied Chronosphere) won with real orders only (no
# cheats). 'Win Condition_Timer' (the Chronosphere's tag) is the win. The
# sidebar builds power / refinery / war factory / battle lab and Kirov
# airships; the first flies the patriot-free line along the north coast to the
# civilian airfield, where the player discovers the three captured Kirovs
# ('Kirovs Gained' hands them over); the rest follow while the Allied chrono
# raids level the island base. The gathered airships are then flown down the
# western edge of the mainland onto the Chronosphere and ordered to attack it;
# their bombs (BlimpHE, 50% vs concrete) bring the 750-HP Chronosphere down.
set render_every 3000
set on_quit continue
# W6 fix #2: Easy. Since W6-02-11 idle units engage at weapon range + 1 (not Sight)
# and W6-02-16 enemy teams set out at full strength; on Normal the defence no longer
# holds. On Easy the player reloads x0.8 and every computer house x1.2 (W6-02-01..03).
set campaign sov12s.MAP Nod 0
until 60 scene == campaign-load
click start
until 300 scene == game
wait 30
dump
""")


def dbg():
    if debug:
        p.add("if outcome == RUNNING then dump")


R = dict(robust=True)
D = dict(tab=1, cat="defense", robust=True)
p.build("NAPOWR", **R); dbg()
p.build("NAREFN", **R); dbg()
if TESLA:
    p.build("TESLA", **D); dbg()
p.build("NAWEAP", **R); dbg()
p.build("NAPOWR", **R); dbg()
p.build("NATECH", **R); dbg()
# queue the Kirovs (vehicle tab 3): one click each, the factory builds them in turn
p.add("click tab:3", "until 900 cameo(ZEP) == 1 and tab == 3", "wait 2",
      "cycle cameo:ZEP queue(vehicle) == ZEP")
for k in range(NEW - 1):
    p.add("click cameo:ZEP")
p.add("until 4000 units(0,ZEP) >= 1")
dbg()


def fly(route):
    for (x, y) in route:
        p.add("if units(0,ZEP) > 0 and selected_type(ZEP) == 0 then click unit 0:ZEP:0 -4 -4", "key t", "key t")
        p.add(f"click cell {x} {y}", "wait 30",
              f"until 3000 units(0,ZEP) == 0 or pos_x(0,ZEP) >= {x-4} and pos_x(0,ZEP) <= {x+4} "
              f"and pos_y(0,ZEP) >= {y-4} and pos_y(0,ZEP) <= {y+4}")
        dbg()


NORTH = [(161, 102), (140, 81), (119, 61), (100, 47), (76, 39)]
# wait for the second airship (or the factory's fall), select every Kirov
# (just off the pad an airship is still low: try the ground cell first)
p.add(f"until 2400 units(0,ZEP) >= {min(NEW, 2)} or buildings(0,NAWEAP) == 0", "wait 60",
      "click unit 0:ZEP:0", "if selected_type(ZEP) == 0 then click unit 0:ZEP:0 -2 -2",
      "if selected_type(ZEP) == 0 then click unit 0:ZEP:0 -4 -4", "key t", "key t",
      "assert selected_type(ZEP) >= 1")
fly(NORTH)
p.add("until 600 units(0,ZEP) >= 4", "assert units(0,ZEP) >= 4")
p.add("dump", "click xy 300 300 right", "click unit 0:ZEP:0 -4 -4", "key t", "key t")
fly([(70, 64), (57, 82), (43, 96), (40, 108)])
p.add("if units(0,ZEP) > 0 and selected_type(ZEP) == 0 then click unit 0:ZEP:0 -4 -4", "key t", "key t")
# under shroud a click on the Chronosphere is a move; once the airships see it
# (Sight 8) the same click is the attack order
for k in range(40):
    p.add("if outcome == RUNNING and units(0,ZEP) > 0 and selected == 0 then click unit 0:ZEP:0 -4 -4",
          "if outcome == RUNNING and buildings(3,GACSPH) > 0 and selected_type(ZEP) > 0 then click building 3:GACSPH", "wait 20")
for k in range(12):
    p.add("until 300 outcome != RUNNING",
          "if outcome == RUNNING and units(0,ZEP) > 0 and selected == 0 then click unit 0:ZEP:0 -4 -4",
          "if outcome == RUNNING and buildings(3,GACSPH) > 0 and selected_type(ZEP) > 0 then click building 3:GACSPH")
    dbg()
p.add("until 3000 outcome == WON", "end expect WON")
p.save(out)




