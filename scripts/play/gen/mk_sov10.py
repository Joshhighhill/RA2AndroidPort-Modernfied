# P5 WORK IN PROGRESS -- does NOT win SOV10: the WCD is off the board until the Battle Lab is captured.
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen import P
out = sys.argv[1] if len(sys.argv) > 1 else "../m-sov10-real.play"
debug = len(sys.argv) > 2 and sys.argv[2] == "debug"
p = P("""# P5: SOV10 (Weather Control Device) won with real orders only (no cheats).
# 'WCD Destroyed' (the GAWEAT's tag, enabled from the start) is the win; the
# map makes the WCD a 200-HP wood building deep in BadGuy3's Prism base. The
# landed SMCV drives inland and deploys, the sidebar builds power / refinery /
# barracks / war factory / battle lab / Nuclear Missile Silo (NAMISL is
# TechLevel 8 in this map); flak cannons, Tesla coils and Rhinos / Flak Tracks
# hold the base against the harassment landings and Harriers, and the charged
# nuke is fired from its sidebar slot at the WCD.
set render_every 3000
set on_quit continue
set campaign sov10t.MAP Nod
until 60 scene == campaign-load
click start
until 300 scene == game
until 2400 input_locked == 0
until 1500 units(0,SMCV) >= 1 and carried(0) == 0
wait 60
click unit 0:SMCV
click cell 152 165
wait 30
until 900 sel_moving == 0
key d
until 900 built(0,NACNST) >= 1
dump
""")
D = dict(tab=1, cat="defense", robust=True)
R = dict(robust=True)


_b = P.build


def _bd(self, *a, **k):
    _b(self, *a, **k)
    if debug:
        self.add("dump")
    return self


P.build = _bd


def mon(n, step=400):
    if not debug:
        return
    for k in range(n):
        p.add(f"until {step} outcome != RUNNING", "if outcome == RUNNING then dump")
        if k % 3 == 0:
            p.add("view building 0:*", f"shot m{len(p.l)}")


p.build("NAPOWR", **R).build("NAREFN", **R).build("NAHAND", **R).build("NAFLAK", **D)
p.build("NAWEAP", **R)
p.add("click tab:3")
for u in ["HTNK", "HTK", "HTNK", "HTK", "HTNK", "HTNK"]:
    p.add(f"click cameo:{u}")
p.build("NAFLAK", **D).build("NAPOWR", **R).build("NATECH", **R).build("NAPOWR", **R)
p.add("dump", "shot tech")
p.build("NAMISL", tab=1, cat="defense", wait=8000, robust=True)
p.add("dump", "shot silo")
p.build("NAFLAK", **D).build("TESLA", **D).build("NAPOWR", **R).build("NAFLAK", **D)
p.add("click tab:3")
for u in ["APOC", "APOC", "HTK", "APOC"]:
    p.add(f"click cameo:{u}")
p.add("dump")
mon(40)
p.add("until 15000 sw_ready_t(0,Nuke) == 1 or outcome != RUNNING", "dump",
      "assert buildings(7,GAWEAT) == 1", "assert outcome == RUNNING",
      "click sw:Nuke", "click building 7:GAWEAT", "until 600 sw_ev_detonated >= 1", "dump", "shot nuke",
      "until 900 outcome == WON", "end expect WON")
p.save(out)



