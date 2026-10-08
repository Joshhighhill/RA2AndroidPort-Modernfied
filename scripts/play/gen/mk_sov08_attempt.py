"""P2b: SOV08 best real-order attempt -- it does NOT win (see docs/gap/wave4/05-mission-sequences.md "P2b").
MCV to (59,97) before deploying; two Sentry Guns before the refinery hold Vlad1's three 15-Conscript
paradrops; war factory, radar, three Tesla Coils, Battle Lab, second factory/refinery; Apocalypse
production with barracks rebuilds; then the army (+5 engineers) pushes to the White House south gate.
Typical result: 16 Apocalypse tanks reach the gate at campaign ~53000, the White House drops to ~800/1000,
the army dies to the four Tesla Coils, the sentry guns and Vlad2's infantry.
Usage: python scripts/play/gen/mk_sov08_attempt.py OUT.play
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mk_sov07 import P7
HEADER = """set render_every 6000
set on_quit continue
set max_frames 60000
set campaign sov08u.MAP Nod
until 60 scene == campaign-load
click start
until 300 scene == game
until 3000 units(0,SMCV) >= 1
wait 15
"""
p = P7(HEADER)
p.select_types(["APOC", "HTNK", "DESO", "E2"])
p.add("click cell 57 101", "wait 10", "click xy 500 400 right", "click unit 0:SMCV", "click cell 59 97", "wait 30",
      "until 900 sel_moving == 0", "wait 10", "key d", "until 900 built(0,NACNST) >= 1", "click xy 500 400 right", "dump")
p.build("NAPOWR").build("NAHAND").build("NALASR", tab=1, cat="defense").build("NAREFN").build("NALASR", tab=1, cat="defense").build("NAWEAP")
p.train("HTNK", 3, 2)
p.build("NAPOWR").build("NARADR").build("NAPOWR")
p.build("TESLA", tab=1, cat="defense").build("TESLA", tab=1, cat="defense").build("NAPOWR").build("TESLA", tab=1, cat="defense").build("NAHAND").build("NAWEAP").build("NAPOWR").build("NATECH").build("NAREFN").build("NAPOWR")
p.add("dump")
g = "outcome == RUNNING"
for r in range(34):
    p.train("APOC" if r > 5 else "HTNK", 3, 1, guard=True)
    if r % 6 == 3:
        p.build("NAHAND")

    if r % 4 == 0:
        p.select_types(["APOC", "HTNK", "HTK"])
        p.add(f"if {g} then click cell {64 if r % 8 else 66} {94 if r % 8 else 96}", "click xy 500 400 right")
    p.add(f"if {g} then wait 600")
    if r % 3 == 2: p.add(f"if {g} then dump")
ARMY = ["HTNK", "APOC", "DESO", "SENGINEER"]
def army_go(x, y, w=3000):
    p.select_types(ARMY)
    p.add(f"if {g} then click cell {x} {y}", "wait 30", f"until {w} sel_moving == 0 or outcome != RUNNING", "click xy 500 400 right", f"if {g} then dump")
# engineer strike team: 4 Flak Tracks, one engineer in each
p.train("SENGINEER", 2, 5, guard=True)
p.add("until 3000 units(0,SENGINEER) >= 5", "dump")
for (x, y) in [(64, 80), (64, 55), (70, 36), (84, 31), (99, 31)]:
    army_go(x, y)
for k in range(30):
    p.select_types(["SENGINEER"])
    p.add(f"if {g} and units(0,SENGINEER) > 0 then click building 4:CAWASH01:0", "click xy 500 400 right")
    p.select_types(ARMY)
    p.add(f"if {g} then click building 4:CAWASH01:0", "click xy 500 400 right", f"if {g} then wait 200")
    if k % 5 == 4: p.add(f"if {g} then dump")
p.add("until 1500 outcome != RUNNING", "end expect WON")
p.save(sys.argv[1])

