import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen import P
out = sys.argv[1] if len(sys.argv) > 1 else "../m-sov11-real.play"
debug = len(sys.argv) > 2 and sys.argv[2] == "debug"
NEW = int(sys.argv[3]) if len(sys.argv) > 3 else 4
p = P("""# P5: SOV11 (Moscow) won with real orders only (no cheats). 'Yuri Defeated'
# (the Kremlin CARUS03's tag) is the win. Once battle control is online the
# sidebar builds power / refinery / barracks / war factory / radar / battle
# lab and four more Kirov airships join the map's two. All four are selected
# (a click on an airship, T T) and flown by move orders along the cheapest
# line through the flak belts -- past BadGuy1's south-west corner, up the
# western map edge, east onto the Kremlin -- and ordered to attack it; their
# bombs (BlimpHE, 50% vs concrete) bring the 800-HP Kremlin down.
# W7 STATUS: NOT WON since the faithful jumpjet speed (w7fix-move: JumpjetSpeed
# 5 = raw leptons/frame, ~0.29 cells/s, 4x slower). BadGuy1's south-west flak
# corner (no route avoids it) now costs all six airships on Normal and five of
# six on Easy; see docs/gap/wave4/05-mission-sequences.md "W7 restore R3".
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
# Kirovs from the war factory (vehicle tab 3): one click each, the factory
# builds them in turn while the ground army works
p.add("click tab:3", "until 900 cameo(ZEP) == 1 and tab == 3", "wait 2",
      "cycle cameo:ZEP queue(vehicle) == ZEP")
for k in range(NEW - 1):
    p.add("click cameo:ZEP")


def sel_kirovs():
    # an airship is drawn ~4 cells above its ground cell: click the sprite
    p.add("if units(0,ZEP) > 0 and selected_type(ZEP) == 0 then click unit 0:ZEP:0 -4 -4", "key t", "key t")


# W7 (move: JumpjetSpeed is raw leptons/frame, the Kirov flies ~0.29 cells/s,
# RE FUN_0054CDA0): a 4x slower airship can no longer outrun the flak belts.
# NAFLAK has Range 12 and Powered=yes. The airships hug the western usable edge
# (x - y = -54, LocalSize) and the north edge, the least-flak line; BadGuy1's
# south-west corner (flak (70,123), (72,121), (77,117), (79,115)) sits on that
# edge and no route avoids it (it is on a level-6 plateau whose only ramp is
# inside BadGuy1's main base, and flak shoots V3 rockets down), so six
# airships cross it and lose about two thirds of their strength. Then they bomb
# BadGuy3's nuclear reactor (27,53): 2000 of its 4900 power against a 3315
# drain, so BadGuy3 drops to low power and its flak cannons -- the two beside
# the Kremlin among them -- go offline. The Kremlin falls to the survivors.
# The airships cross in single file, 2.8 cells apart on the x - y = -53 line
# (a FlakWH burst, CellSpread 1, then hurts one, not the bunch): each is sent
# alone to its slot as it leaves the factory, the map's two first (leader in
# front).
STAGE = [(94 + 2 * i, 147 + 2 * i) for i in range(2 + NEW)]


def go_alone(i):
    x, y = STAGE[i]
    p.add(f"click unit 0:ZEP:{i} -4 -4",
          f"if selected_type(ZEP) == 0 then click unit 0:ZEP:{i} -2 -2",
          f"if selected_type(ZEP) == 0 then click unit 0:ZEP:{i}",
          f"if selected_type(ZEP) == 1 and selected == 1 then click cell {x} {y}",
          "click xy 300 300 right")


go_alone(0)
go_alone(1)
for k in range(2, 2 + NEW):
    p.add(f"until 6000 units(0,ZEP) >= {k + 1}", "wait 60")
    go_alone(k)
    dbg()
k = 1 + NEW
x, y = STAGE[k]
p.add(f"until 2000 ucx(0,ZEP,{k}) >= {x-1} and ucx(0,ZEP,{k}) <= {x+1} and ucy(0,ZEP,{k}) >= {y-1} and ucy(0,ZEP,{k}) <= {y+1}",
      "dump")
p.add("click unit 0:ZEP:0 -4 -4", "key t", "key t", "assert selected_type(ZEP) >= 4")
# up the west edge, then along the north edge (x + y = 75) out of the reach of
# BadGuy3's flak (28,65)
route = [(17, 71), (14, 62), (19, 56), (24, 51)]
for (x, y) in route:
    sel_kirovs()
    p.add(f"click cell {x} {y}", "wait 30",
          f"until 9000 units(0,ZEP) == 0 or pos_x(0,ZEP) >= {x-2} and pos_x(0,ZEP) <= {x+2} "
          f"and pos_y(0,ZEP) >= {y-2} and pos_y(0,ZEP) <= {y+2}")
    dbg()
# under shroud a click on the reactor is a move; once seen, the attack order
for k in range(40):
    p.add("if units(0,ZEP) > 0 and selected_type(ZEP) == 0 then click unit 0:ZEP:0 -4 -4",
          "if selected_type(ZEP) > 0 and selected_type(ZEP) < units(0,ZEP) then key t",
          "if buildings(3,NANRCT) == 2 then click cell 27 53",
          "waituntil 40 buildings(3,NANRCT) < 2 or units(0,ZEP) == 0")
    if k % 10 == 9:
        dbg()
p.add("dump")
sel_kirovs()
p.add("click building 3:CARUS03", "wait 30")
for k in range(12):
    p.add("until 300 outcome != RUNNING",
          "if outcome == RUNNING and units(0,ZEP) > 0 and selected == 0 then click unit 0:ZEP:0 -4 -4",
          "if outcome == RUNNING and selected_type(ZEP) > 0 then click building 3:CARUS03")
    dbg()
p.add("until 3000 outcome == WON", "end expect WON")
p.save(out)

