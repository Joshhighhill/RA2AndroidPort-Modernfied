import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen import P
ARMY = ["MTNK", "FV", "MGTK"]
p = P("""# P4: ALL10 (Mirage) won with real orders only (no cheats). After the intro
# lock the base grows from the sidebar (two refineries, power, a second war
# factory) and Grizzlies / IFVs are trained while the Soviet invasion breaks on
# the base defences and blows up the German villages (barrel chains). Then the
# type-selected army (Grizzlies, IFVs, Einstein's Mirage tanks) attacks every
# BadGuy1/2/3 building and unit in turn -- the three ConYards the landed MCVs
# deployed first; 'Win' (DestroyedAll BG1+BG2+BG3) -> VICTORY.
set render_every 3000
set on_quit continue
set campaign ALL10s.MAP GDI
until 60 scene == campaign-load
click start
until 300 scene == game
wait 30
until 1500 input_locked == 0
""")
# Einstein's lab (99,173) stands among barrels: the 12 GIs and the Rocketeers
# guard it against the assault squads (they are not in the attack army).
p.select_types(["E1", "JUMPJET"]).add("click cell 101 168", "wait 300")
p.build("GAREFN").build("GAPOWR").build("GAREFN")
p.train("MTNK", 3, 5)
p.build("GAWEAP").build("GAPOWR")
for r in range(5):
    p.train("MTNK", 3, 4)
    p.train("FV", 3, 1)
    p.add("wait 500", "dump")
p.add("until 9000 campaign_frame >= 5400", "dump")
HOUSES = [1, 2, 6]
BASE = {1: (80, 60), 2: (110, 44), 6: (143, 72)}   # around wp1/wp2/wp3, where the MCVs deploy
UT = ["SMCV", "APOC", "HTNK", "HTK", "V3", "FLAKT", "SHK", "E2", "DOG", "DESO", "DRON", "TTNK", "HARV",
      "SENGINEER", "IVAN", "SAPC", "ZEP", "TERROR", "CRAZYV"]
BT = ["NACNST", "NAPOWR", "NAREFN", "NAHAND", "NAWEAP", "NARADR", "NALASR", "NAFLAK", "TESLA", "NAIRON",
      "NANRCT", "NATECH", "NAMISL", "NADEPT", "NAYARD", "NABNKR"]
g = "outcome == RUNNING"
def select_army():
    first = True
    for t in ARMY:
        p.add(f"if {g} and units(0,{t}) > 0 then click unit 0:{t}:0{'' if first else ' shift'} soft")
        first = False
    p.add(f"if {g} then key t", f"if {g} then key t")
def house_round(h, wait):
    targets = [(f"buildings({h},{t}) > 0", f"building {h}:{t}:0") for t in BT]
    targets += [(f"units({h},{t}) > occupants({h}) and units({h},{t}) > carried({h})", f"unit {h}:{t}:0") for t in UT]
    select_army()
    for cond, tgt in targets:
        p.add(f"if {g} and {cond} then click {tgt} soft", f"if {g} and {cond} then wait {wait}")
    # stragglers a click cannot reach: walk the army through the house's base
    bx, by = BASE[h]
    p.add(f"if {g} and buildings({h}) == 0 and units({h}) > 0 then click cell {bx} {by} soft",
          f"if {g} and buildings({h}) == 0 and units({h}) > 0 then wait 300")
# One house at a time (nearest first), so the army arrives instead of
# shuttling between bases; then sweeps of all three.
for h in [6, 1, 2]:
    for k in range(5):
        house_round(h, 300)
        p.add(f"if {g} then dump")
        if k % 2 == 1: p.train("MTNK", 3, 4, guard=True)
for k in range(6):
    for h in HOUSES:
        house_round(h, 150)
    p.add(f"if {g} then dump")
p.add("until 3000 outcome == WON", "end expect WON")
p.save(sys.argv[1] if len(sys.argv) > 1 else "../m-all10-real.play")
