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
# W7R1: since wave 7 (threat scan, retaliation, call-for-help) the idle GIs
# answer the CivilianArmy raids, walk out after them and die in the streets.
# Like a player, walk them into the base (beside the Construction Yard, the
# side the raids come from) and dig them in (deploy) there.
p.select_types(["E1"]).add("click cell 87 127", "wait 300")
p.select_types(["E1"]).add("key d", "wait 5")
p.build("GAPOWR").build("GAREFN").build("GAPILE")
# fv8b: since w8fix-layer (the deck counts 4 levels in the elevation bonus and
# the cliff tracer, FUN_00487C60) the starting IFVs lose the bonus range at
# raiders on the x=98 bridge deck, drive up to the bridge head and die there,
# and the raid that killed them walks on into the dug-in GIs. Like a player,
# put two Pillboxes on the bridge head right after the barracks.
PILLS = [tuple(int(c) for c in xy.split(",")) for xy in os.environ.get("MK_ALL03_PILLS", "97,123 100,123").split()]
PILLS_AT = os.environ.get("MK_ALL03_PILLS_AT", "pile")
if PILLS_AT == "pile":
    for xy in PILLS:
        p.build("GAPILL", tab=1, cat="defense", near=xy)
p.build("GAPOWR").build("GAWEAP")
# W7R1: since the original BuildTimeMultiplier / build speed (wave 7) a Grizzly
# takes ~480 frames at one factory, so the AI raid reached the lone war factory
# before the army stood. The Grizzlies now train as soon as the first war
# factory stands while two more war factories go up (MultipleFactory 0.8).
# fv9a: since the faithful team Load (FUN_006EC760: the line holds until every
# member is ordered aboard; w9fix-speed's one-per-pass fan-out had boarded ONE
# member and 43 WaitLoaded sat out its cap) the House-1 Flak Track team rides
# in loaded and the raids thin the first 10 Grizzlies before the batch stands;
# the factory then idled while the script waited (credits piled up to 27k) and
# the base fell. Like a player, queue 15 up front (14..15 and RAID_R 7..9 WON;
# 10, 13, 16+ lost or ran out).
p.train("MTNK", 3, int(os.environ.get("MK_ALL03_TRAIN0", "15")))
if PILLS_AT == "weap":
    for xy in PILLS:
        p.build("GAPILL", tab=1, cat="defense", near=xy)


RAIDERS = ["E2", "SHK", "HTK", "HTNK"]   # ascending priority
RAID_R = int(os.environ.get("MK_ALL03_RAID_R", "8"))


def regroup():
    # W7R1: the Grizzlies idle where they leave the factory, out of range of
    # the raids on the yard's east side, and meet them one by one. Like a
    # player, pull them together east of the Construction Yard.
    c = "outcome == RUNNING and units(0,MTNK) > 0"
    p.add(f"if {c} then click unit 0:MTNK:0 soft", f"if {c} and selected > 0 then key t",
          f"if {c} and selected > 0 then key t")
    # fv7b: with the w7r2 engine changes merged on top of w7r1 (the vehicle
    # ScatterContent push and the Approach_Target 0x4D55B0 rings -- either
    # one alone still won) the raids razed the base from f10200 while the
    # Grizzlies idled at the regroup cell; a human house gets no call for
    # help (FUN_00707500). Like a player, send the group at a raider that
    # comes within 8 cells (last click wins: highest priority last; 10+
    # cells pulls the tanks out piecemeal and loses).
    near = []
    for t in RAIDERS:
        q = f"sel_near(1,{t}) < {RAID_R}"
        p.add(f"if {c} and selected > 0 and {q} then click unit 1:{t}:near soft")
        near.append(f"sel_near(1,{t}) >= {RAID_R}")
    p.add(f"if {c} and selected > 0 and {' and '.join(near)} then click cell 90 127 alt")


for b in ("GAWEAP", "GAREFN", "GAPOWR", "GAWEAP"):
    p.build(b)
    regroup()
for r in range(4):
    if r > 0:
        p.train("MTNK", 3, 10)
    want = min(army, (r+1)*9)
    # A player tops the batch up when the AI's raids thin it out (since the P4
    # crushable walls the AI tanks drive through the sandbags/fences, and the
    # raid losses vary): re-queue while short, then insist.
    for _ in range(3):
        for _ in range(6):
            regroup()
            p.add(f"waituntil 330 units(0,MTNK) >= {want}")
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
