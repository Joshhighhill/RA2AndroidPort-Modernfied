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
p.select_types(["E2"]).add("click cell 97 92", "wait 300")
p.select_types(["SENGINEER"]).add("click cell 99 95", "until 1500 sel_moving == 0", "dump", "click unit 0:SENGINEER:0", "click building 4:CABHUT:1", "wait 20", "dump",
      "until 1800 bridge_destroyed < $broken", "assert units(0,SENGINEER) == 0", "assert outcome == RUNNING", "shot repaired")
p.train("E2", 2, 25)
p.add("wait 1500")
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
