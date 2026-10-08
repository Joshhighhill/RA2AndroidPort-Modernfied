import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen import P

DEBUG = "--debug" in sys.argv
STOP = int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2].isdigit() else 9
U = "cheat units_dump D:/ra2-wt/p4-scratch/a9units.txt"
p = P("""# P4: ALL09 (Yucatan) OPENING proven with real orders only (no cheats) -- NOT a win.
# The elite SEALs clear the village; the freed Grizzlies crush out of their camp and
# open the POW fence (crushable walls); the Hack engineers land; the village holds
# against the POW-check teams and the flak patrol; an engineer captures the Tech
# Airport (ParaDrop: 8 GIs every 4 min); the paradropped GIs push north, destroy
# Pyramid 1 and an engineer takes Reactor 2; the SEALs plant C4 on Reactor 1 and
# an engineer behind the GI wave captures the BadGuy1 Construction Yard. Beyond
# this point BadGuy1's 'Attk Response' teams overrun the small force (see
# docs/gap/wave4/05-mission-sequences.md "P4 status").
set render_every 3000
set on_quit continue
set campaign ALL09t.MAP GDI
until 60 scene == campaign-load
click start
until 300 scene == game
wait 30
until 1500 input_locked == 0
until 600 units(0,GHOST) >= 5 and carried(0) == 0
""")


def dump(tag=None):
    p.add("dump")
    if DEBUG:
        p.add(U)
        if tag:
            p.add(f"shot {tag}")


def sel(types):
    first = True
    for t in types:
        p.add(f"if units(0,{t}) > 0 then click unit 0:{t}:0{'' if first else ' shift'} soft")
        first = False
    p.add("key t", "key t")


def para(x, y):
    p.add(f"if sw_ready(0) == 1 then click sw:ParaDrop soft", f"if sw_armed == 1 then click cell {x} {y} soft")


ARMY = ["MTNK", "GHOST", "E1"]
import json
VAR = json.loads(os.environ.get("A9VAR", "{}"))
ENG = VAR.get("eng", [62, 70])
HOLD = VAR.get("hold", [64, 74])
# P1: the SEALs clear the village guards (frees the POWs and the vehicles)
sel(["GHOST"])
p.add("click cell 58 84", "wait 200")
for k in range(10):
    for t in ("DOG", "E2"):
        c = f"units(2,{t}) > 0 and units(9,ENGINEER) == 0"
        p.add(f"if {c} then click unit 0:GHOST:0 soft", f"if {c} then key t", f"if {c} then key t",
              f"if {c} then click unit 2:{t}:near soft", f"if {c} then wait 120")
dump("p1")
# P2: the Hack engineers step back; the Grizzlies crush out and open the POW camp
p.add("until 2000 units(9,ENGINEER) >= 5 and carried(9) == 0",
      "click unit 9:ENGINEER:0", "key t", "key t", f"click cell {ENG[0]} {ENG[1]}")
sel(["MTNK"])
p.add("click cell 62 74", "wait 200", "click unit 0:MTNK:0", "click cell 66 80", "wait 150",
      "click unit 0:MTNK:0", "click cell 63 76", "wait 60")
sel(["E1"])
p.add("click cell 60 76", "wait 100")
dump("p2")
# P3: hold the village against the POW-check teams and the flak patrol
for k in range(10):
    sel(ARMY)
    p.add(f"click cell {HOLD[0]} {HOLD[1]}", "wait 150")
    if k % 3 == 2:
        dump()
dump("p3")
if STOP >= 4:
    # P4: everyone to the Tech Airport; one engineer captures it (ParaDrop)
    sel(ARMY)
    p.add("click cell 25 87")
    p.add("click unit 9:ENGINEER:0", "key t", "key t", "click cell 27 85")
    for k in range(5):
        p.add("wait 200")
        dump()
    p.add("click unit 9:ENGINEER:0", "click building 5:CAAIRP:0", "until 900 buildings(0,CAAIRP) >= 1")
    dump("p4")
if STOP >= 5:
    # P5: hold at the airport, paradropping GIs as the ParaDrop recharges
    for k in range(40):
        para(26, 84)
        sel(ARMY)
        p.add("click cell 25 86", "wait 300")
        if k % 4 == 3:
            dump()
    dump("p5")
if STOP >= 6:
    # P6: north to the pyramid ruins with the paradropped GIs, stage by stage
    INF_T = [(1, "SHK"), (1, "DOG"), (1, "E2")]
    VEH_T = [(1, "HTK"), (1, "HTNK")]
    def micro(types, targets, radius, wait=60):
        sel(types)
        for h, t in targets:
            c = f"sel_near({h},{t}) < {radius}"
            p.add(f"if {c} then click unit {h}:{t}:near soft", f"if {c} then wait {wait}")
    p.add("click unit 9:ENGINEER:0", "key t", "key t", f"click cell {VAR.get('eng2', [70, 84])[0]} {VAR.get('eng2', [70, 84])[1]}")
    for stage in VAR.get("stages", [[60, 80], [72, 84], [80, 86], [84, 89], [86, 94], [87, 99]]):
        para(stage[0] - 4, stage[1] - 4)
        sel(ARMY)
        p.add(f"click cell {stage[0]} {stage[1]}", "wait 200")
        for k in range(3):
            micro(["MTNK"], VEH_T, 8)
            micro(["GHOST", "E1"], INF_T + VEH_T, 7)
            p.add("wait 100")
        dump()
    dump("p6")
if STOP >= 7:
    sel(["GHOST"])
    p.add("if buildings(1,NALASR) > 2 then click building 1:NALASR:2 soft", "wait 250")
    p.add("click unit 9:ENGINEER:0", "key t", "key t", "click cell 84 97", "wait 250",
          "click unit 9:ENGINEER:0", "click building 8:MAYAN:0 soft", "wait 120",
          "click unit 9:ENGINEER:0", "click building 1:NAPOWR:2 soft")
    for k in range(8):
        para(84, 92)
        sel(ARMY)
        p.add("click cell 86 100", "wait 200")
        dump(f"q{k}")
if STOP >= 8:
    # P8: SEAL C4 on Reactor 1 -- BadGuy1 drops below its drain (Reactor 2 is ours):
    # every powered defence (Tesla, flak, sentry) goes offline
    sel(["E1"])
    p.add("click cell 86 98", "wait 30", "key d")
    for k in range(4):
        sel(["GHOST"])
        p.add("if low_power(1) == 0 then click building 1:NAPOWR:near soft", "wait 200")
        dump()
    dump("p8")
if STOP >= 9:
    # P9: an engineer behind the GI wave to the Construction Yard
    for stage in VAR.get("cy_stages", [[87, 106], [88, 114], [88, 121]]):
        para(stage[0], stage[1] - 6)
        sel(["MTNK", "E1"])
        p.add(f"click cell {stage[0]} {stage[1]}", "wait 250")
        dump()
    p.add("click unit 9:ENGINEER:0", "click building 1:NACNST:0 soft")
    for k in range(6):
        p.add("wait 200")
        dump()
    dump("p9")
if STOP >= 9:
    p.add("until 1500 buildings(0,NACNST) >= 1", "assert buildings(0,CAAIRP) >= 1",
          "assert buildings(8,MAYAN) <= 1", "assert outcome == RUNNING")
p.add("end expect RUNNING")
p.save(sys.argv[1] if len(sys.argv) > 1 and sys.argv[1].endswith(".play") else "../m-all09-opening.play")
