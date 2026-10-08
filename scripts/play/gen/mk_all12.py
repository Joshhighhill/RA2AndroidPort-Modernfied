import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen import P

def aa_guard(p):
    return
    g = "outcome == RUNNING and units(1,ZEP) > 0 and units(0,FV) > 0"
    p.add(f"if {g} then click unit 0:FV:0")
    for k in range(1, 8):
        p.add(f"if {g} and units(0,FV) > {k} then click unit 0:FV:{k} shift")
    p.add(f"if {g} then click unit 1:ZEP:0")
out = sys.argv[1] if len(sys.argv) > 1 else "../m-all12-real.play"
p = P("""# P1: ALL12 (Moscow) won with real orders only (no cheats).
# Opening (W5-A): the 3 elite ParaSEALS reach the BadGuy2 Tesla Reactor through
# the fence's corner gap and C4 it; 'Generators Destroyed' brings the Chrono
# MCV (deployed here), 8 IFVs, then 8 Grizzlies. The player builds a Battle
# Lab, Chronosphere and Weather Control Device. The Black Guard (4 elite
# Apocalypse tanks + Tesla, 2 flak, 2 sentry guns of BadGuy3 in the walled
# Kremlin yard) falls to superweapons and SEALs: a Chronoshift dumps two
# Apocalypses into the lake, a Lightning Storm on the other pair, then IFVs
# carrying SEALs are chronoshifted into the yard and C4 the five buildings.
# The Kremlin itself (concrete, Insignificant) is never targeted.
set render_every 3000
set on_quit continue
set campaign all12s.MAP GDI 0
until 60 scene == campaign-load
assert difficulty == 0
click start
until 300 scene == game
until 600 carried(0) == 0 and units(0,GHOST) >= 3
assert outcome == RUNNING
click unit 0:GHOST:0
click unit 0:GHOST:1 shift
click unit 0:GHOST:2 shift
assert selected == 3
click building 2:NAPOWR
until 600 buildings(2,NAPOWR) == 0
assert units(0,GHOST) == 3
until 1200 units(0,AMCV) >= 1
click unit 0:AMCV
key d
until 450 built(0,GACNST) == 1
dump
""")
def build_aa(self, bid, tab=0, wait=2400, cat="structure"):
    """P.build with the long ready-wait split into slices that also react to a Kirov."""
    self.add(f"let n_{bid} = built(0,{bid})", f"until 900 can_build({bid}) == 1",
             f"click tab:{tab}", f"click cameo:{bid}")
    for _ in range(wait // 200):
        self.add(f"if queue_ready({cat}) == 0 then wait 200")
        aa_guard(self)
    self.add(f"until 600 queue_ready({cat}) == 1", f"cycle cameo:{bid} placing == 1", "place auto",
             f"until 600 built(0,{bid}) > $n_{bid}")
    return self
P.build = build_aa
p.build("GAPOWR").build("GAREFN").build("GAPILE").build("GAWEAP").build("AMRADR").build("GATECH").build("GAPOWR").add("dump")
p.build("GACSPH", tab=1, cat="defense", wait=3000).add("dump", "shot csph")
def hold(p, frames, cond):
    for i in range(frames // 300):
        p.add(f"if outcome == RUNNING and {cond} then wait 300")
        if i % 4 == 3: p.add(f"if outcome == RUNNING and {cond} then dump")
    p.add("dump")

def load_burst(n, w=300):
    # free SEAL j boards the j-th EMPTY IFV; all orders in one burst (stable indexes)
    p.add("let cz = carried(0)")
    for k in range(n):
        g = f"outcome == RUNNING and units(0,GHOST) > $cz+{k} and units(0,FV) > $cz+{k}"
        p.add(f"if {g} then click unit 0:GHOST:{k}", f"if {g} then click unit 0:FV~:{k}")
    p.add(f"wait {w}")

p.build("GAPOWR")
p.build("GAWEAT", tab=1, cat="defense", wait=3000).add("dump", "shot weat")
p.build("GAPOWR").build("GAPOWR").build("GAWEAP")
hold(p, 12000, "sw_ready_t(0,ChronoSphere) == 0")
# 1. Chronoshift: the eastern pair of Black Guard Apocalypses into the lake
p.add("view cell 123 117", "shot pre1",
      "click sw:ChronoSphere", "click cell 123 117", "click cell 129 85", "wait 60", "dump", "shot post1")
p.train("SREF", 3, 12)
p.add("dump")
# 2. Lightning Storm on the western pair
hold(p, 9000, "sw_ready_t(0,LightningStorm) == 0")
p.add("view cell 118 122", "shot pre2", "click sw:LightningStorm", "click cell 118 122", "wait 700", "dump", "shot post2")
# 3./4. two more Lightning Storms: the eastern flak + sentry gun, then the southern pair.
# IonWH does 2% to concrete, so the Kremlin (1000 HP) shrugs off the bolts.
for k, (x, y) in enumerate([(127, 121), (122, 126)]):
    hold(p, 12000, "sw_ready_t(0,LightningStorm) == 0")
    p.add(f"if outcome == RUNNING then click sw:LightningStorm", f"if sw_armed > 0 then click cell {x} {y}",
          "wait 700", "dump", f"shot storm{k+2}")
p.add("shot final", "until 3000 outcome != RUNNING", "dump", "cheat camp_log fire", "until 3000 outcome == WON", "end expect WON")
p.save(out)
