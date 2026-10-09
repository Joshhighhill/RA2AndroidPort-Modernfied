import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen import P
p=P("""# W5-A: SOV01 won with real orders only (no cheats). The high bridge 'Break
# Bridge' blows at f0 is repaired by an engineer the player trains (power plant,
# barracks) and walks -- escorted by the type-selected Conscripts -- to the
# revealed bridge hut (a real capture click). Then 25 more Conscripts force-fire
# (Ctrl) the four Pentagon sections; 'Win Condition 1' -> VICTORY.
set render_every 3000
set on_quit continue
set campaign sov01t.MAP Nod
until 60 scene == campaign-load
click start
until 300 scene == game
until 60 bridge_destroyed >= 4
let broken = bridge_destroyed
until 2400 input_locked == 0
if units(0,SMCV) >= 1 then click unit 0:SMCV
if units(0,SMCV) >= 1 then key d
until 900 built(0,NACNST) >= 1
""")
p.build("NAPOWR").build("NAHAND")
p.train("SENGINEER", 2, 1)
p.add("until 1200 units(0,SENGINEER) >= 1")
# W6-02-11: idle units now engage at weapon range + 1 (not Sight), so the engineer
# walks INSIDE the Conscript group to (97,92) (it used to trail it while the escorts
# covered it from their Sight range), then on alone as before.
# R-210: with the height gate the Conscripts and civilians no longer take cliff
# steps, so the fight on the way plays out differently; the escort now stays with
# the engineer for the second leg too (alone it met three GIs at (100,111)).
p.select_types(["E2", "SENGINEER"]).add("click cell 97 92", "until 900 sel_moving == 0", "wait 60")
p.select_types(["E2", "SENGINEER"]).add("click cell 99 95", "until 1500 sel_moving == 0", "wait 30")
p.select_types(["SENGINEER"]).add("click cell 99 95", "until 1500 sel_moving == 0", "dump", "click unit 0:SENGINEER:0", "click building 4:CABHUT:1", "wait 20", "dump",
      "until 1800 bridge_destroyed < $broken", "assert units(0,SENGINEER) == 0", "assert outcome == RUNNING", "shot repaired")
p.train("E2", 2, 25)
p.add("wait 1500")
# R-210: the starting Rhinos sit in the pocket west of the one-cell gully bridge
# (45,84), broken at f0 like the east span; they used to drop into the gully and
# climb out (a 4-level cliff both ways) to join the assault. The Conscripts now
# clear the GIs in the (62..72, 84..85) corridor and go round to the gully first.
p.select_types(["E2"]).add("click cell 66 85", "until 1800 sel_moving == 0", "wait 450")
p.select_types(["E2"]).add("click cell 50 84", "until 1800 sel_moving == 0", "wait 300")
for k in range(10):
    for piece in ["CAWA2A","CAWA2B","CAWA2C","CAWA2D"]:
        g = f"outcome == RUNNING and buildings(4,{piece}) > 0"
        p.add(f"if {g} and units(0,HTNK) > 0 then click unit 0:HTNK:0",
              f"if {g} and units(0,E2) > carried(0) then click unit 0:E2:0 shift",
              f"if {g} then key t", f"if {g} then key t",
              f"if {g} then click building 4:{piece}:0 ctrl", "if outcome == RUNNING then wait 300")
    p.add("if outcome == RUNNING then dump")
p.add("until 3000 outcome == WON", "end expect WON")
p.save("../scripts/play/m-sov01-real.play")
