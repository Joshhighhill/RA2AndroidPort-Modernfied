import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen import P
out = sys.argv[1] if len(sys.argv) > 1 else "../m-sov12-real.play"
debug = len(sys.argv) > 2 and sys.argv[2] == "debug"
NEW = int(sys.argv[3]) if len(sys.argv) > 3 else 3
TESLA = len(sys.argv) > 4 and sys.argv[4] == "tesla"
REST = int(sys.argv[5]) if len(sys.argv) > 5 else 0
p = P("""# P5: SOV12 (the Allied Chronosphere) won with real orders only (no
# cheats). 'Win Condition_Timer' (the Chronosphere's tag) is the win. The
# sidebar builds power / refinery / war factory / battle lab and Kirov
# airships; the first flies the patriot-free line along the north coast to the
# civilian airfield, where the player discovers the three captured Kirovs
# ('Kirovs Gained' hands them over); the rest follow while the Allied chrono
# raids level the island base. The gathered airships are then flown down the
# western edge of the mainland onto the Chronosphere and ordered to attack it;
# their bombs (BlimpHE, 50% vs concrete) bring the 750-HP Chronosphere down.
# W7 STATUS: NOT WON since the faithful jumpjet speed (~0.29 cells/s). With
# the changes below six airships reach the Chronosphere's ring of seven
# Patriots but die inside it; see docs/gap/wave4/05-mission-sequences.md
# "W7 restore R3".
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
RHINOS = NEW <= 2
if RHINOS:
    # two Rhinos from the new war factory guard the base against the raid IFVs
    p.add("click tab:3", "until 900 cameo(HTNK) == 1 and tab == 3", "wait 2",
          "cycle cameo:HTNK queue(vehicle) == HTNK", "click cameo:HTNK")
p.build("NAPOWR", **R); dbg()
p.build("NATECH", **R); dbg()
# queue the Kirovs (vehicle tab 3): one click each, the factory builds them in turn
if RHINOS:
    p.add("until 2000 units(0,HTNK) >= 2 or queue(vehicle) == \"\"")
p.add("click tab:3", "until 900 cameo(ZEP) == 1 and tab == 3", "wait 2",
      "cycle cameo:ZEP queue(vehicle) == ZEP")
for k in range(NEW - 1):
    p.add("click cameo:ZEP")
p.add("until 4000 units(0,ZEP) >= 1")
dbg()


def fly(route, hunt=False):
    for (x, y) in route:
        arrived = (f"units(0,ZEP) == 0 or pos_x(0,ZEP) >= {x-4} and pos_x(0,ZEP) <= {x+4} "
                   f"and pos_y(0,ZEP) >= {y-4} and pos_y(0,ZEP) <= {y+4}")
        if not hunt:
            p.add("if units(0,ZEP) > 0 and selected_type(ZEP) == 0 then click unit 0:ZEP:0 -4 -4", "key t", "key t")
            p.add(f"click cell {x} {y}", "wait 30", f"until 3000 {arrived}")
            dbg()
            continue
        # W7: a raid IFV within reach is bombed first (a move order does not
        # return fire), then the flight goes on
        for k in range(50):
            p.add("if units(0,ZEP) > 0 and selected_type(ZEP) == 0 then click unit 0:ZEP:0 -4 -4",
                  "if selected_type(ZEP) > 0 and selected_type(ZEP) < units(0,ZEP) then key t",
                  f"click cell {x} {y}",
                  "if sel_near(1,FV) <= 9 then click unit 1:FV:near soft",
                  "if sel_near(2,FV) <= 5 then click unit 2:FV:near soft",
                  "if sel_near(3,FV) <= 5 then click unit 3:FV:near soft",
                  f"waituntil 60 {arrived}")
        dbg()


NORTH = [(162, 101), (109, 49), (101, 46), (92, 40), (77, 40)]   # W7: clear of the Patriot (101,60)
# wait for the second airship (or the factory's fall), select every Kirov
# (just off the pad an airship is still low: try the ground cell first)
# W7: the airships wait over the base for the last one, bombing raid IFVs
# that come in reach (one bomb kills one; a move order would not return fire)
for k in range(40 * (NEW - 1)):
    p.add("if units(0,ZEP) > 0 and selected_type(ZEP) == 0 then click unit 0:ZEP:0",
          "if units(0,ZEP) > 0 and selected_type(ZEP) == 0 then click unit 0:ZEP:0 -4 -4",
          "if selected_type(ZEP) > 0 and selected_type(ZEP) < units(0,ZEP) then key t",
          "if selected_type(ZEP) > 0 and selected_type(ZEP) < units(0,ZEP) then key t",
          "if selected_type(ZEP) > 0 and sel_near(1,FV) <= 8 then click unit 1:FV:near soft",
          f"waituntil 60 units(0,ZEP) >= {NEW} or buildings(0,NAWEAP) == 0")
p.add(f"until 2400 units(0,ZEP) >= {NEW} or buildings(0,NAWEAP) == 0", "wait 60",
      "click unit 0:ZEP:0", "if selected_type(ZEP) == 0 then click unit 0:ZEP:0 -2 -2",
      "if selected_type(ZEP) == 0 then click unit 0:ZEP:0 -4 -4", "key t", "key t",
      "assert selected_type(ZEP) >= 1")
fly(NORTH, hunt=True)
p.add("until 600 units(0,ZEP) >= 4", "assert units(0,ZEP) >= 4")
p.add("dump", "click xy 300 300 right", "click unit 0:ZEP:0 -4 -4", "key t", "key t")
fly([(76, 60), (42, 94), (40, 99), (40, 110)], hunt=True)
# W7: seven Patriots (RedEye2, Range 12) ring the Chronosphere; at ~0.29
# cells/s the last eight cells are flown under six or seven of them. The
# airships first rest out of their reach on the west shore (SelfHealing=yes)
# and pick off IFVs that come close, then go in along the least-Patriot line.
for k in range(REST):
    p.add("if units(0,ZEP) > 0 and selected_type(ZEP) == 0 then click unit 0:ZEP:0 -4 -4",
          "if selected_type(ZEP) > 0 and selected_type(ZEP) < units(0,ZEP) then key t",
          "if sel_near(2,FV) <= 4 then click unit 2:FV:near soft",
          "if sel_near(3,FV) <= 4 then click unit 3:FV:near soft",
          "wait 60")
    if k % 10 == 9:
        dbg()
fly([(40, 112), (55, 127)])
# Bomb two ring Patriots on the way in (NASAM 900, steel: BlimpHE 75%, x1.25
# against a computer house on Easy -- four bombs, one volley), the west one
# (63,124) and the one beside the Chronosphere (69,126); each kill takes one
# seventh of the fire off the airships for the rest of the approach.
for i, (px, py) in enumerate([(63, 124), (69, 126)]):
    p.add(f"let ns{i} = buildings(3,NASAM)")
    for k in range(30):
        p.add("if units(0,ZEP) > 0 and selected_type(ZEP) == 0 then click unit 0:ZEP:0 -4 -4",
              "if selected_type(ZEP) > 0 and selected_type(ZEP) < units(0,ZEP) then key t",
              "if selected_type(ZEP) > 0 and selected_type(ZEP) < units(0,ZEP) then key t",
              f"if buildings(3,NASAM) == $ns{i} then click cell {px} {py}",
              f"waituntil 40 buildings(3,NASAM) < $ns{i} or units(0,ZEP) == 0")
    dbg()
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




