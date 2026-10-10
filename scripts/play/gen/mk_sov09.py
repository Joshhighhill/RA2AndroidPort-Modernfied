"""R3: SOV09 (Mexico) WON with real player orders only (no cheats), Easy.

Easy is the original's difficulty slider 0: three Yuris drop (`Yuri Drop -
Easy`), the British get a second engineer, the northern IFV patrol and the
six-SEAL group stay home; the player reloads x0.8, every computer house x1.2.

  1. Battle control (input unlocks at ~65 s): the three Yuris, walking east
     into the Prisms, are pulled back west at once. A Yuri takes the x=37 IFV
     patrol as it drives past (or is ordered to).
  2. Yuri 0 walks round the west side of the Alliance compound (out of the
     door GIs' reach), mind-controls the British engineer, which captures the
     Alliance barracks (`Captured Barracks`); dogs from it clear the door GIs,
     then the American GI and dogs south of the Prism compound.
  3. The IFV, at (66,75) -- out of every Prism's reach -- force-fires the
     compound's south wall at (66,71) until the section is gone.
  4. From that gap (still outside all seven Prisms' 8 cells) the IFV shells
     the three power plants it can reach, (68,66), (70,66), (68,64): American
     output 1000 -> 400 < drain 575 (output scales with health,
     FUN_0044E7B0), the Prisms go offline. No unit of the PlayerControl
     house Africans steps on the `Player in Base` ring (36 cells, all inside
     the walls), so the Americans never turn to Hunt.
  5. The IFV leaves the gap; Yuris 1 and 2 walk up to it. Yuri 1, just
     inside, mind-controls the British guard SEAL south of the President
     from 7.3 cells (its acquisition is range 6 + 1).
  6. Yuri 2 stands at (67,68): 7.6 cells from the President's (64,61) stop
     and from the guard standing on it -- out of the guard's reach, inside
     Yuri's 8 (the map's [MindControl] Range=8). When the President walks
     to (64,61), Yuri 2 takes him: `Pres is Captured` -> Win.

Engine fixes this needed (R3): force-fire/Stop/Guard/Scatter for PlayerControl
units, and an attack on a building stopping at weapon range.
Run from the repo root: python scripts/play/gen/mk_sov09.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen import P  # noqa: E402

HEADER = """# R3: SOV09 (Mexico) WON with real orders only (no cheats), Easy. Generated
# by scripts/play/gen/mk_sov09.py -- edit the generator, not this file.
# Yuris pulled back at battle control; the British engineer (mind-controlled)
# captures the Alliance barracks; dogs clear the south; the mind-controlled
# IFV breaches the compound's south wall and, from the gap (out of the
# Prisms' reach), destroys three power plants: output 400 < 575, Prisms
# offline; a Yuri takes the President's south guard, another the President.
set render_every 900
set on_quit continue
set campaign sov09u.MAP Nod 0
until 60 scene == campaign-load
click start
until 300 scene == game
until 600 airborne(6) >= 1
until 2000 input_locked == 0
"""


def pick(p, spec):
    """Right-click (deselect), then select one unit: clicking a friendly
    transport with a Yuri selected would order the Yuri inside it."""
    return p.add("click xy 500 400 right", f"click unit {spec}")


def order(p, spec, target, wait=None):
    pick(p, spec).add(f"click {target}")
    if wait:
        p.add(f"wait {wait}")
    return p


def go(p, spec, x, y, w=2500):
    order(p, spec, f"cell {x} {y}", 20)
    h, t, n = spec.split(":")
    return p.add(f"until {w} umov({h},{t},{n}) == 0")


def main(out):
    p = P(HEADER)
    # 1. battle control: the Yuris walk east into the Prisms -> back west at once
    p.add("click xy 500 400 right", "click unit 6:YURI:0", "click unit 6:YURI:1 shift",
          "click unit 6:YURI:2 shift", "click cell 36 74", "wait 60",
          "until 600 umov(6,YURI,0) == 0 and umov(6,YURI,1) == 0",
          "assert units(6,YURI) >= 3", "dump")
    # the x=37 IFV patrol (the Yuris often take it on their own as it drives past)
    p.add("until 900 units(1,FV) >= 2 or units(6,FV) >= 1")
    c = "units(6,FV) == 0"
    p.add(f"if {c} then click xy 500 400 right", f"if {c} then click unit 6:YURI:1",
          f"if {c} then click unit 1:FV:1")
    p.add("until 300 units(6,FV) >= 1")
    order(p, "6:FV:0", "cell 30 76")
    # 2. Yuri 0: round the west side, the British engineer, the Alliance barracks
    go(p, "6:YURI:0", 26, 74, 1500)
    go(p, "6:YURI:0", 25, 47, 1500)
    order(p, "6:YURI:0", "unit 5:SENGINEER:0")
    p.add("until 900 units(6,SENGINEER) >= 1")
    order(p, "6:SENGINEER:0", "building 2:GAPILE:0")
    order(p, "6:YURI:0", "cell 25 66")
    p.add("until 900 buildings(0,GAPILE) >= 1",
          "until 900 can_build(DOG) == 1", "click tab:2", "click cameo:DOG", "click cameo:DOG",
          "wait 400")
    # last-fix: the barracks now falls sooner (an idle engineer no longer chases
    # targets with its DefuseKit, fv-b) and a dog can die at the door GIs --
    # train another until two are out
    for _ in range(3):
        c = "units(0,DOG) < 2 and queue_stage(infantry) == 0"
        p.add(f"if {c} then click tab:2", f"if {c} then click cameo:DOG", "wait 300")
    p.add("until 900 units(0,DOG) >= 2", "until 900 units(2,E1) <= 5",
          "assert buildings(0,GAPILE) >= 1", "dump", "shot barracks")
    # 3. three more dogs clear the American GI and dogs south of the compound
    p.add("click tab:2", "click cameo:DOG", "click cameo:DOG", "click cameo:DOG",
          "until 1500 units(0,DOG) >= 4")
    p.add("click xy 500 400 right", "click unit 0:DOG:0", "key t", "key t")
    # last-fix: the American GI patrols y=73 between x=59 and 73; the dogs wait for
    # it at (58,76) (outside every Prism's reach) and keep picking for longer
    for (x, y) in [(36, 64), (40, 76), (55, 76), (58, 76)]:
        p.add(f"click cell {x} {y}", "wait 30", "until 1500 sel_moving == 0")
    for _ in range(12):
        p.add("click xy 500 400 right", "click unit 0:DOG:0", "key t", "key t",
              "if units(1,E1) > 0 then click unit e:E1:near soft", "wait 60",
              "click xy 500 400 right", "click unit 0:DOG:0", "key t", "key t",
              "if units(1,DOG) > 0 then click unit e:DOG:near soft", "wait 60")
    p.add("dump")
    # 4. the IFV to (66,75), out of every Prism's reach; it breaks the wall at (66,71)
    for (x, y) in [(40, 77), (55, 78), (66, 75)]:
        go(p, "6:FV:0", x, y)
    for _ in range(4):   # a car or civilian on the way can stop it short
        c = "ucell(6,FV,0) != 66075"
        p.add(f"if {c} then click xy 500 400 right", f"if {c} then click unit 6:FV:0",
              f"if {c} then click cell 66 75", f"if {c} then wait 20",
              "until 600 umov(6,FV,0) == 0")
    # last-fix: no idle time at (66,75) -- left idle, the IFV picks the American
    # GI by (67,73) and its approach takes it into the Prisms; the force-fire
    # order below starts on arrival and is kept up until the section is gone
    p.add("assert ucell(6,FV,0) == 66075")
    for k in range(20):
        g = "" if k < 10 else "if cursor_name != move then "
        p.add(f"{g}click xy 500 400 right", f"{g}click unit 6:FV:0",
              f"{g}click cell 66 71 ctrl")   # Ctrl = force-fire on the wall cell
        p.add("wait 100", "move cell 66 71", "wait 1")
    p.add("until 1500 cursor_name == move", "assert cursor_name == move", "dump", "shot breach")
    # 5. from the gap the IFV shells the plants in its range, nearest first:
    #    output 1000 -> 400 < 575, the Prisms go offline
    for _ in range(40):
        c = "power_out(1) >= 575"
        p.add(f"if {c} then click xy 500 400 right", f"if {c} then click unit 6:FV:0",
              f"if {c} then click building 1:GAPOWR:near", f"if {c} then wait 200")
    p.add("until 900 power_out(1) < 575", "assert power_out(1) < 575",
          "assert outcome == RUNNING", "dump", "shot blackout")
    # 6. the IFV clears the gap; Yuris 1 and 2 walk up to it
    order(p, "6:FV:0", "cell 70 74")
    for (x, y) in [(40, 77), (44, 78), (53, 78), (58, 78), (58, 76), (62, 76), (64, 74)]:
        for n in (1, 2):
            order(p, f"6:YURI:{n}", f"cell {x + n - 1} {y}")
        p.add("wait 30", "until 1500 umov(6,YURI,1) == 0 and umov(6,YURI,2) == 0")
    p.add("dump")
    # 7. Yuri 1, just inside the gap, takes the British guard SEAL south of
    #    the President from 7.3 cells (beyond its 6 + 1 acquisition)
    go(p, "6:YURI:1", 66, 70, 600)
    for _ in range(8):
        c = "units(6,GHOST) == 0"
        p.add(f"if {c} then click xy 500 400 right", f"if {c} then click unit 6:YURI:1",
              f"if {c} then click unit 4:GHOST:near soft", "wait 40")
    p.add("dump")
    # 8. Yuri 2 at (67,68); the President is taken when he steps onto (64,61)
    go(p, "6:YURI:2", 66, 71, 600)
    go(p, "6:YURI:2", 67, 68, 600)
    p.add("dump")
    for _ in range(30):
        c = "outcome == RUNNING and ucell(3,PRES,0) == 64061"
        p.add(f"if {c} then click xy 500 400 right", f"if {c} then click unit 6:YURI:2",
              f"if {c} then click unit 3:PRES:0", "wait 30")
    p.add("until 900 outcome != RUNNING", "dump", "end expect WON")
    p.save(out)


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "m-sov09-real.play"))
