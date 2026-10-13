import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen import P
import json
VAR = json.loads(os.environ.get("A10VAR", "{}"))
ARMY = ["MTNK", "FV", "MGTK"]
p = P("""# P4: ALL10 (Mirage) won with real orders only (no cheats). After the intro
# lock the base grows from the sidebar (two refineries, power, a second war
# factory) and Grizzlies / IFVs are trained while the Soviet invasion breaks on
# the base defences and blows up the German villages (barrel chains). Then the
# type-selected army (Grizzlies, IFVs, Einstein's Mirage tanks) attacks every
# BadGuy1/2/3 building and unit in turn -- the three ConYards the landed MCVs
# deployed first; 'Win' (DestroyedAll BG1+BG2+BG3) -> VICTORY.
# W6 re-tune (full-strength team start + Patrol attack-move left BadGuy2's
# 14-unit team waiting at its Con Yard): two extra Chrono Miners fund the
# army, and the lab guard joins the final sweeps. Normal (loading-screen
# default).
# fv-b re-tune (R3 hold-at-range + fv-a return-to-anchor): the start tanks
# stage in the base and hunt the invaders (V3s first) while it is built; the
# lab guard stands on the lab's south-west side and hunts what comes near.
set render_every 3000
set on_quit continue
set campaign ALL10s.MAP GDI{DIFF}
until 60 scene == campaign-load
click start
until 300 scene == game
wait 30
until 1500 input_locked == 0
""".replace("{DIFF}", "" if VAR.get("diff") is None else f" {VAR['diff']}"))
# fv-b: Einstein's lab (CALAB, house Einstein) is the loss condition (Loss
# Einstein KIA). With R3 a lone Conscript of an assault squad stops at weapon
# range (95,175) and shoots the lab down from outside the GIs' guard radius
# at (101,168); the lab guard therefore stands at (97,170), on the lab's
# south-west side, and hunts every invader
# within LAB_R cells of it (engineers last = first priority).
LAB = VAR.get("lab", [97, 170])
LAB_R = VAR.get("lab_r", 12)
LAB_MICRO = VAR.get("lab_micro", 1)
# Einstein's lab (99,173) stands among barrels: the 12 GIs and the Rocketeers
# guard it against the assault squads (they are not in the attack army).
p.select_types(["E1", "JUMPJET"]).add(f"click cell {LAB[0]} {LAB[1]}", "wait 300")
# fv-b re-tune (R3: an attack on a building stops at weapon range): the
# invasion's V3 launchers now shell the base from ~10 cells instead of
# driving into it, out of the idle defenders' guard range, and the base was
# razed by f6600 (LOST at f7494). A player answers that: while the base is
# built and the army trained, the tanks go after every invader near them,
# the V3s first.
HUNT_R = VAR.get("hunt_r", 22)
HUNT_TYPES = ["DOG", "E2", "SHK", "FLAKT", "HTK", "HTNK", "V3"]   # ascending priority (the last click wins)
# fv7b: since w7r2's Approach_Target rings (FUN_004D55B0) the invasion's V3
# launchers shell the base from their range ring, outside the Prisms', and
# the BadGuy MCVs (unstuck by the ScatterContent push) build up; the base was
# razed by f8400 (LOST). The two Rocketeers -- out of reach of the V3s and
# Rhinos -- hunt with the tanks.
HUNTERS = VAR.get("hunters", ["MTNK", "FV", "MGTK", "JUMPJET"])
LAB_TYPES = ["DOG", "E2", "SHK", "FLAKT", "HTK", "HTNK", "V3", "IVAN", "TERROR", "SENGINEER"]


def lab_guard(guard="outcome == RUNNING"):
    p.add(f"if {guard} and units(0,E1) > 0 then click unit 0:E1:0 soft",
          f"if {guard} and units(0,JUMPJET) > 0 then click unit 0:JUMPJET:0 shift soft",
          f"if {guard} then key t", f"if {guard} then key t",
          f"if {guard} then click cell {LAB[0]} {LAB[1]} soft")
    for t in LAB_TYPES:
        for h in (1, 2, 6):
            p.add(f"if {guard} and sel_near({h},{t}) < {LAB_R} then click unit {h}:{t}:near soft")


def hunt(types=HUNTERS, r=HUNT_R, guard=None):
    g = f"{guard} and " if guard else ""
    first = True
    for t in types:
        p.add(f"if {g}units(0,{t}) > 0 then click unit 0:{t}:0{'' if first else ' shift'} soft")
        first = False
    p.add(f"if {g}outcome == RUNNING then key t", f"if {g}outcome == RUNNING then key t")
    for t in HUNT_TYPES:
        for h in (1, 2, 6):
            p.add(f"if {g}sel_near({h},{t}) < {r} then click unit {h}:{t}:near soft")


# The start tanks (Grizzlies at the lab, Mirage tanks south-east of it) move
# up to the base, where the invasion comes in, and hunt from there.
STAGE = VAR.get("stage", [110, 122])
if STAGE:
    p.select_types(["MTNK", "MGTK"]).add(f"click cell {STAGE[0]} {STAGE[1]}")


def build_h(bid):
    """P.build(lenient) with hunting while the structure is in production."""
    pl = "place auto"
    p.add(f"let n_{bid} = built(0,{bid})", f"until 900 can_build({bid}) == 1", "click tab:0",
          f"click cameo:{bid}")
    for _ in range(VAR.get("build_polls", 24)):
        p.add("waituntil 100 queue_ready(structure) == 1")
        hunt(guard="queue_ready(structure) == 0")
    p.add("until 2400 queue_ready(structure) == 1", "click tab:0", f"click cameo:{bid}", pl)
    for _ in range(4):
        p.add("wait 1", f"if placing == 1 then {pl}")
    p.add(f"until 600 placing == 0 and queue_ready(structure) == 0 or built(0,{bid}) > $n_{bid}")


build_h("GAREFN")
build_h("GAPOWR")
build_h("GAREFN")
p.train("MTNK", 3, 5)
build_h("GAWEAP")
build_h("GAPOWR")
# W6 re-tune: two more Chrono Miners -- the start miner dies and ~9400
# credits used to sit idle while the army was trained; without them the
# army is broke before BadGuy2's (now full-strength, waiting) team falls.
p.train("CMIN", 3, 2)
for r in range(6):
    p.train("MTNK", 3, 5)
    p.train("FV", 3, 1)
    for k in range(5):
        hunt()
        if LAB_MICRO:
            lab_guard()
        p.add("wait 100")
    p.add("dump")
p.add("until 9000 campaign_frame >= 5400", "dump")
HOUSES = [1, 2, 6]
BASE = {1: (80, 60), 2: (110, 44), 6: (143, 72)}   # around wp1/wp2/wp3, where the MCVs deploy
UT = ["SMCV", "APOC", "HTNK", "HTK", "V3", "FLAKT", "SHK", "E2", "DOG", "DESO", "DRON", "TTNK", "HARV",
      "SENGINEER", "IVAN", "SAPC", "ZEP", "TERROR", "CRAZYV"]
BT = ["NACNST", "NAPOWR", "NAREFN", "NAHAND", "NAWEAP", "NARADR", "NALASR", "NAFLAK", "TESLA", "NAIRON",
      "NANRCT", "NATECH", "NAMISL", "NADEPT", "NAYARD", "NABNKR"]
g = "outcome == RUNNING"
def select_army(types=ARMY):
    first = True
    for t in types:
        p.add(f"if {g} and units(0,{t}) > 0 then click unit 0:{t}:0{'' if first else ' shift'} soft")
        first = False
    p.add(f"if {g} then key t", f"if {g} then key t")
def house_round(h, wait, types=ARMY):
    targets = [(f"buildings({h},{t}) > 0", f"building {h}:{t}:0") for t in BT]
    targets += [(f"units({h},{t}) > occupants({h}) and units({h},{t}) > carried({h})", f"unit {h}:{t}:0") for t in UT]
    select_army(types)
    for cond, tgt in targets:
        p.add(f"if {g} and {cond} then click {tgt} soft", f"if {g} and {cond} then wait {wait}")
        if LAB_MICRO and "E1" not in types:
            lab_guard(f"{g} and {cond}")
            select_army(types)
    # stragglers a click cannot reach: walk the army through the house's base
    bx, by = BASE[h]
    p.add(f"if {g} and buildings({h}) == 0 and units({h}) > 0 then click cell {bx} {by} soft",
          f"if {g} and buildings({h}) == 0 and units({h}) > 0 then wait 300")
    if LAB_MICRO and "E1" not in types:
        lab_guard(g)
# One house at a time (nearest first), so the army arrives instead of
# shuttling between bases; then sweeps of all three.
for h in [6, 1, 2]:
    # regroup: wait for a fresh army before each base (credits pile up)
    p.train("MTNK", 3, 6, guard=True)
    for _ in range(3):
        p.add(f"if {g} and buildings({h}) > 0 then wait 300")
        if LAB_MICRO:
            lab_guard(f"{g} and buildings({h}) > 0")
    for k in range(5):
        house_round(h, 300)
        p.add(f"if {g} then dump")
        p.train("MTNK", 3, 3, guard=True)
# W6 re-tune: BadGuy2 keeps its waiting full-strength team (14 infantry and
# Flak Tracks) at its Con Yard [RE: TeamClass::AI FUN_006E86B0 -- a team
# starts only at full strength]; once BadGuy1/3 are gone (the assault squads
# with them) the lab guard -- GIs and Rocketeers -- joins the final sweeps.
for k in range(8):
    for h in HOUSES:
        house_round(h, 150, ARMY + ["E1", "JUMPJET"])
    p.add(f"if {g} then dump")
    p.train("MTNK", 3, 4, guard=True)
# last-fix: a last straggler far from its base (a BadGuy2 Shock Trooper that
# finished its approach walk in the south once its target died -- the
# original keeps the cell Destination, FootClass::PointerExpired FUN_004D9870)
# is out of reach of the 150-frame round clicks, and each round's walk
# through the empty base pulled the army back north. Chase what is left.
for k in range(4):
    for h in HOUSES:
        for t in UT:
            cond = f"{g} and buildings({h}) == 0 and units({h},{t}) > occupants({h}) and units({h},{t}) > carried({h})"
            p.add(f"if {cond} then click xy 500 400 right")
            for n, a in enumerate(ARMY + ["E1", "JUMPJET"]):
                p.add(f"if {cond} and units(0,{a}) > 0 then click unit 0:{a}:0{'' if n == 0 else ' shift'} soft")
            p.add(f"if {cond} then key t", f"if {cond} then key t",
                  f"if {cond} then click unit {h}:{t}:0 soft", f"if {cond} then wait 900")
p.add("until 3000 outcome == WON", "end expect WON")
p.save(sys.argv[1] if len(sys.argv) > 1 else "../m-all10-real.play")
