import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen import P
NEAR = (61, 143)   # w8fix-place: the ConYard (SMCV deploys at 60,142)
p = P("""# P4: SOV04 (Korea) won with real orders only (no cheats). The SMCV deploys; the
# base grows from the sidebar (`place near 61 143`, the ConYard: w8fix-place
# W8-03-01 refuses gem/ore cells like the original FUN_0047C530, so the old
# `place auto` anchor -- the lamp post by the northern gem field -- found no
# legal cell any more): refineries, power, barracks, three war
# factories, flak. Four Typhoons and ~50 Rhinos are trained while the 10-minute
# timer runs; the feints and paradrops die to the Rhinos guarding the base. When
# the Allied MCV lands, the type-selected Rhinos attack it, then every BadGuy1
# unit / building in turn (the garrisoned farm first) and the Typhoons hunt the
# destroyers; 'Win Timer' (DestroyedAll BadGuy1) -> VICTORY.
set render_every 3000
set on_quit continue
set campaign sov04s.MAP Nod
until 60 scene == campaign-load
click start
until 300 scene == game
wait 30
click unit 0:SMCV
key d
until 900 built(0,NACNST) >= 1
""")
p.build("NAREFN", near=NEAR).build("NAPOWR", near=NEAR).build("NAHAND", near=NEAR).build("NAWEAP", near=NEAR)
p.train("HTNK", 3, 5)
p.build("NAREFN", near=NEAR).build("NAPOWR", near=NEAR)
p.train("HTNK", 3, 5)
p.build("NAWEAP", near=NEAR).build("NAFLAK", near=NEAR, tab=1, cat="defense").build("NAPOWR", near=NEAR)
p.add("dump")
for r in range(4):
    p.train("HTNK", 3, 5)
    p.add("wait 600", "dump")
p.add("until 9000 campaign_frame >= 11400", "dump")
p.select_types(["HTNK"]).add("click cell 108 48", "dump")
p.train("HTNK", 3, 5, guard=True)
p.add("until 3000 units(1,AMCV) >= 1 or buildings(1,GACNST) >= 1", "dump")
# fv7e: with the faithful Beach block (fv7c, [Beach] Foot/Track/Wheel 0%) the
# landing craft beach and unload on a different schedule: the first feint
# infantry were already ashore (far to the south-west) while the MCV was still
# aboard, so the target loop sent the Rhinos after them and the MCV deployed
# unopposed. The Rhinos now hold at (108,48) until the MCV itself is ashore.
p.add("until 3000 free(1,AMCV) >= 1 or buildings(1,GACNST) >= 1", "dump")
p.train("SUB", 3, 3, guard=True)
targets_u = ["AMCV", "MTNK", "E1", "GGI", "FV", "CMIN", "SENGINEER", "ENGINEER", "JUMPJET", "ASW"]
targets_b = ["CAFARM06", "GACNST", "GAPILL", "GAPOWR", "GAREFN", "GAWEAP", "GAPILE", "GAAIRC", "GADEPT", "GAYARD"]
g = "outcome == RUNNING"
def sel(cond, tgt, ut="HTNK"):
    p.add(f"if {cond} and units(0,{ut}) > 0 then click unit 0:{ut}:0",
          f"if {cond} and units(0,{ut}) > 0 then key t",
          f"if {cond} and units(0,{ut}) > 0 then key t",
          f"if {cond} and units(0,{ut}) > 0 then click {tgt}",
          f"if {cond} then wait 150")
for k in range(16):
    for t in targets_u:   # only a unit on the map is clickable (not garrisoned / aboard)
        sel(f"{g} and units(1,{t}) > occupants(1) and units(1,{t}) > carried(1)", f"unit 1:{t}:0")
    for t in targets_b:
        sel(f"{g} and buildings(1,{t}) > 0", f"building 1:{t}:0")
    sel(f"{g} and units(1,DEST) > 0", "unit 1:DEST:0", ut="SUB")
    p.add(f"if {g} then wait 50", f"if {g} then dump")
    if k % 5 == 4: p.train("HTNK", 3, 5, guard=True)
p.add("until 3000 outcome == WON", "end expect WON")
p.save(sys.argv[1] if len(sys.argv) > 1 else "../m-sov04-real.play")
