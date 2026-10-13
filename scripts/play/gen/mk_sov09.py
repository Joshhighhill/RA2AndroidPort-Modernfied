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

MR1 (after L0's FastMath/_ftol and frame timers changed the fights; no
engine change): a pack of 8 more dogs (not 3) clears the American dogs and GI
south of the compound; the IFV's legs to (66,75) re-issue the move whenever it
stops short or picks a target (since R-212 a vanished auto-target leaves a unit
walking on to its old firing cell); the plant shelling is polled every 10
frames so a retaliation chase into the Prisms is cancelled at once; Yuri 1
comes back after taking the IFV, Yuri 0 after the breach, and an idle Yuri
walking off after a target during the shelling is called back.

Engine fixes this needed (R3): force-fire/Stop/Guard/Scatter for PlayerControl
units, and an attack on a building stopping at weapon range.

w7r2 (after wave 7's faithful combat model -- area-guard threat scans, call
for help, 1 % Verses fire): steps 2b-5 are re-planned. The door dogs are
parked; the IFV takes a route 12+ cells from the Prism at (57,66) and shoots
the south patrol (2 dogs + GI) from (66,77); Yuri 2 takes the east IFV patrol;
the plant shelling calls for help, so the breaching IFV hits a plant once and
backs off -- the two IFVs shoot the six SEALs that answer -- then once more,
and Yuri 0 takes one of the two Grizzlies that answer, which with the IFVs
kills the other; then the plants. Engine fix found on the way: an ability
weapon (the dog's ParasiteDog jaw) no longer holds a target its warhead does
0 % to (a patrol team ordered the dogs at the IFV, which they rode to death).
Run from the repo root: python scripts/play/gen/mk_sov09.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen import P  # noqa: E402

HEADER = """# R3: SOV09 (Mexico) WON with real orders only (no cheats), Easy. Generated
# by scripts/play/gen/mk_sov09.py -- edit the generator, not this file.
# Yuris pulled back at battle control; the British engineer (mind-controlled)
# captures the Alliance barracks; the mind-controlled IFV shoots the south
# patrol and breaches the compound's south wall; Yuri 2 takes the east IFV;
# two lures draw out the SEALs (shot by the IFVs) and the Grizzlies (Yuri 0
# takes one); from the gap the IFV destroys three power plants: output < 575,
# Prisms offline; a Yuri takes the President's south guard, another the
# President.
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


PLANT_POLLS = int(os.environ.get("MK_SOV09_PLANT_POLLS", "500"))


def _cells(env, default):
    v = os.environ.get(env, default)
    return [tuple(int(c) for c in xy.split(",")) for xy in v.split()] if v else []


DOG_PARK = (_cells("MK_SOV09_DOG_PARK", "26,50") or [None])[0]
# w7r2: the IFV keeps 12+ cells from the Prism at (57,66) -- the old leg via
# (55,78) (a building cell) took it along y=75, 9 cells from the Prism.
FV_ROUTE = _cells("MK_SOV09_FV_ROUTE", "40,77 48,82 64,82 66,77")
FV_HOLD = int(os.environ.get("MK_SOV09_FV_HOLD", "60"))
LURE = int(os.environ.get("MK_SOV09_LURE", "1"))   # 0: the pre-wave-7 plan (fails)
BREACH_POLLS = int(os.environ.get("MK_SOV09_BREACH_POLLS", "30"))
LURE_POLLS = int(os.environ.get("MK_SOV09_LURE_POLLS", "60"))


def drive(p, spec, x, y, polls=60):
    """MR1: like go(), but re-issue the move whenever the unit stops short or
    picks a target on the way: an idle unit auto-acquires, and since R-212 a
    vanished target leaves it walking on to its old firing cell (into the
    Prisms), so the player keeps it on its move order."""
    order(p, spec, f"cell {x} {y}", 10)
    k = spec.replace(":", ",")
    base = f"units({k.rsplit(',', 1)[0]}) > 0 and ucell({k}) != {x * 1000 + y}"
    p.add(f"repeat {polls}", "wait 10")
    # w7r2: also when it walks anywhere else -- after its target dies a unit
    # finishes the walk to its old firing cell (R-212), here into the Prisms
    for c in (f"{base} and utgt({k}) != -1", f"{base} and umov({k}) == 0",
              f"{base} and upath({k}) != {x * 1000 + y}"):   # no parentheses in --play
        p.add(f"if {c} then click xy 500 400 right", f"if {c} then click unit {spec}",
              f"if {c} then click cell {x} {y}")
    return p.add("endrepeat")


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
    # MR1: Yuri 1 (sent after the IFV) back with Yuri 2 -- left out at (44,74)
    # it is worn down to 13 HP and dies before step 6
    p.add("if ucell(6,YURI,1) != 30075 then click xy 500 400 right",
          "if ucell(6,YURI,1) != 30075 then click unit 6:YURI:1",
          "if ucell(6,YURI,1) != 30075 then click cell 31 76")
    # 2. Yuri 0: round the west side, the British engineer, the Alliance barracks
    go(p, "6:YURI:0", 26, 74, 1500)
    go(p, "6:YURI:0", 25, 47, 1500)
    pick(p, "6:YURI:0").add(
        "# W7-09-17: the civilian house's engineer is rules id ENGINEER (the probe used to",
        "# report the image-matched Soviet SENGINEER).",
        "click unit 5:ENGINEER:0")
    p.add("until 900 units(6,ENGINEER) >= 1")
    order(p, "6:ENGINEER:0", "building 2:GAPILE:0")
    order(p, "6:YURI:0", "cell 25 66")
    p.add("until 900 buildings(0,GAPILE) >= 1",
          "until 900 can_build(DOG) == 1", "click tab:2", "click cameo:DOG", "click cameo:DOG",
          "wait 400")
    # last-fix: the barracks now falls sooner (an idle engineer no longer chases
    # targets with its DefuseKit, fv-b) and a dog can die at the door GIs --
    # train another until two are out
    # w7r2: the dogs are needed only for the door GIs now (the IFV takes the
    # south patrol), so the retraining stops once two door GIs are down and no
    # dog count is asserted (a dog out of the door still often dies there)
    for _ in range(3):
        c = "units(2,E1) > 5 and units(0,DOG) < 2 and queue_stage(infantry) == 0"
        p.add(f"if {c} then click tab:2", f"if {c} then click cameo:DOG", "wait 300")
    p.add("until 900 units(2,E1) <= 5",
          "assert buildings(0,GAPILE) >= 1", "dump", "shot barracks")
    # w7r2: dogs are DefaultToGuardArea=yes -- idle, they scan 2 x the
    # VirtualScanner's 5 cells (W7 threat scan) and run at the SEALs by
    # (44..52,57..59), whose MP5 kills a dog per 10 frames from 6 cells. The
    # door dogs wait west of the barracks, out of that reach.
    if DOG_PARK:
        p.add("if units(0,DOG) > 0 then click xy 500 400 right", "if units(0,DOG) > 0 then click unit 0:DOG:0",
              "key t", "key t", f"if units(0,DOG) > 0 then click cell {DOG_PARK[0]} {DOG_PARK[1]}")
    # 3. (w7r2) the south patrol -- two American dogs and a GI walking y=73
    #    between x=57 and 75, under the Prisms -- is shot by the IFV from
    #    (66,77): HoverMissile (HE, 2 x 25 per 50 frames, range 6) reaches the
    #    road at y=73 for x=61..71, the Prisms at (57,66)/(74,67) are 12+
    #    cells away, and a dog cannot bite a vehicle. The old dog pack (MR1)
    #    now dies on the way: since wave 7 every DefaultToGuardArea dog and
    #    every computer unit scans its area-guard radius (2 x the larger
    #    weapon range: 10 cells for a dog, 12 for a SEAL's MP5), so the pack
    #    ran at the SEAL by (44,59) and chased the patrol into the Prisms.
    # 4. the IFV to (66,77) (it shoots the patrol), then (66,75), out of
    #    every Prism's reach; it breaks the wall at (66,71)
    for (x, y) in FV_ROUTE:
        drive(p, "6:FV:0", x, y)
    hold = "units(6,FV) > 0 and ucell(6,FV,0) != 66077"
    p.add(f"repeat {FV_HOLD}", "wait 20", f"if {hold} then click xy 500 400 right",
          f"if {hold} then click unit 6:FV:0", f"if {hold} then click cell 66 77", "endrepeat", "dump")
    # 3b. (w7r2) Yuri 2 takes the east IFV patrol (x=78, y=53..75) at its south
    #     turn from (73,81) (MindControl Range=8 in this map, 7.2 cells). Left
    #     alone it answers the plant shelling (call for help, FUN_00707500,
    #     W7) and kills our IFV at the gap; the six SEALs that answer with it
    #     cannot hurt an IFV (HollowPoint 1 % vs light) and die to the two.
    #     The captured IFV has the lower index: from here 6:FV:0 is it and the
    #     breaching IFV is 6:FV:1 (asserted below).
    #     It sets off once the patrol's dogs are dead (they bite infantry
    #     within their 10-cell scan) and keeps south of y=79 on the way.
    for (x, y) in [(40, 80), (48, 82), (64, 82), (73, 81)]:
        go(p, "6:YURI:2", x, y, 1500)
    for _ in range(40):
        c = "units(6,FV) < 2 and units(1,FV) > 0"
        p.add(f"if {c} then click xy 500 400 right", f"if {c} then click unit 6:YURI:2",
              f"if {c} then click unit 1:FV:near soft", "wait 40")
    p.add("until 300 units(6,FV) >= 2", "assert ucell(6,FV,1) == 66077", "dump")
    # Yuri 2 out of the SEALs' way (they come to the gap) until step 6
    order(p, "6:YURI:2", "cell 40 86")
    # (the captured IFV stays at its patrol's south turn: any way west along
    # the road passes within 8 cells of the Prism at (74,67))
    main_from = len(p.l)
    order(p, "6:FV:0", "cell 66 75", 20)
    p.add("until 600 umov(6,FV,0) == 0")
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
    # MR1: Yuri 0, left at (25,66), is worn down there (62/100 HP by ~f10400 and
    # dead before step 6 -- the n-th-unit indices of Yuris 1/2 then shift); it
    # walks back to the others now (one order, no wait: the IFV goes on)
    if not LURE:
        order(p, "6:YURI:0", "cell 29 77")
    else:
        # 4b. (w7r2) since wave 7 a hit on an unarmed structure calls for help
        #     (FUN_00707500, 45-frame cooldown per attacker): the six most
        #     valuable idle units of the house come at the attacker -- first
        #     the SEALs (1000 each), on the next call the two Grizzlies parked
        #     at (71,58..59). A recruited SEAL fires its MP5 at an IFV for 1 %
        #     (1 per shot, ROF 10): six of them killed the lone IFV in ~300
        #     frames, the Grizzlies in fewer. So:
        #     (A) both IFVs post at (66,82) / (70,81); the breaching IFV hits a
        #         plant once and backs off at once -- only the SEALs answer,
        #         and the two IFVs shoot them (a hit on an armed unit calls no
        #         help);
        #     (B) Yuri 0 (free: its engineer went into the barracks) waits at
        #         (62,81); the next hit brings the Grizzlies, Yuri 0 takes one,
        #         which with the IFVs kills the other;
        #     (C) then the plants (step 5). Every unit keeps a post out of the
        #     Prisms' 8 cells; a Yuri that has its unit backs off out of every
        #     enemy's reach (its own scan would swap the unit for another).
        for (x, y) in [(29, 77), (40, 80), (48, 82), (62, 81)]:
            go(p, "6:YURI:0", x, y, 2500)
        for (x, y) in [(83, 78), (76, 81), (70, 81)]:
            drive(p, "6:FV:9", x, y, 40)   # 6:FV:9 -> the captured IFV (6:FV:0), see the remap
        p.add("dump")
        posts = [("6:FV:0", "(6,FV,0)", 66, 82, "units(6,FV) > 1"),
                 ("6:FV:9", "(6,FV,9)", 70, 81, "units(6,FV) > 0"),
                 ("6:MTNK:0", "(6,MTNK,0)", 64, 78, "units(6,MTNK) > 0")]

        def hold_posts():
            for spec, kk, x, y, alive in posts:
                c = f"{alive} and ucell{kk} != {x * 1000 + y} and upath{kk} != {x * 1000 + y} and utgt{kk} == -1"
                p.add(f"if {c} then click xy 500 400 right", f"if {c} then click unit {spec}",
                      f"if {c} then click cell {x} {y}")
                # chasing a target 7+ cells off (towards the Prisms): back
                far = f"{alive} and ucell{kk} != {x * 1000 + y} and utgt{kk} != -1 and nenemy{kk} > 6"
                p.add(f"if {far} then click xy 500 400 right", f"if {far} then click unit {spec}",
                      f"if {far} then click cell {x} {y}")

        def yuri0_poll():
            c = "units(1,MTNK) > 0 and units(6,MTNK) == 0 and units(6,YURI) > 0"
            p.add(f"if {c} then click xy 500 400 right", f"if {c} then click unit 6:YURI:0",
                  f"if {c} then click unit 1:MTNK:near soft")
            back = "units(6,MTNK) > 0 and ucell(6,YURI,0) != 44087 and upath(6,YURI,0) != 44087"
            p.add(f"if {back} then click xy 500 400 right", f"if {back} then click unit 6:YURI:0",
                  f"if {back} then click cell 44 87")

        for phase in ("A", "B"):
            # fv7b: since w7r3 (a Prism Tower no longer shoots at airborne
            # units -- FUN_007718B0 AA legality) the opening plays out a
            # little differently and the idle breaching IFV had drifted to
            # (69,80); from there its plant order took it into a Prism's
            # reach (dead in 90 frames). Back to the gap first, from where
            # it shells the plants in step 5.
            drive(p, "6:FV:0", 66, 75, BREACH_POLLS)
            p.add("let pw = power_out(1)", "click xy 500 400 right", "click unit 6:FV:0",
                  "click building 1:GAPOWR:near", "until 400 power_out(1) < $pw")
            order(p, "6:FV:0", "cell 66 82")
            for _ in range(LURE_POLLS):
                if phase == "B":
                    yuri0_poll()
                hold_posts()
                p.add("wait 20")
            p.add("dump")
    # 5. from the gap the IFV shells the plants in its range, nearest first:
    #    output 1000 -> 400 < 575, the Prisms go offline
    # MR1: polled every 10 frames instead of re-clicked every 200 -- a unit
    # that steps up to the IFV makes it retaliate and chase it north into the
    # Prisms (dead in ~50 frames); a fresh attack order on the plant drops
    # that target at once (and picks the next plant when one is gone).
    p.add(f"repeat {PLANT_POLLS // 20}")   # 20 polls, then the Yuri check
    for _ in range(20):
        base = "power_out(1) >= 575 and units(6,FV) > 0"
        for c in (f"{base} and utgt(6,FV,0) != -1", f"{base} and utgtb(6,FV,0) == -1"):
            p.add(f"if {c} then click xy 500 400 right", f"if {c} then click unit 6:FV:0",
                  f"if {c} then click building 1:GAPOWR:near")
        if LURE:   # (w7r2) the captured Grizzlies stay on their posts meanwhile
            for k, (gx, gy) in enumerate([(64, 78), (68, 78)]):
                c = (f"power_out(1) >= 575 and units(6,MTNK) > {k} and ucell(6,MTNK,{k}) != {gx * 1000 + gy}"
                     f" and upath(6,MTNK,{k}) != {gx * 1000 + gy} and nenemy(6,MTNK,{k}) > 6")
                p.add(f"if {c} then click xy 500 400 right", f"if {c} then click unit 6:MTNK:{k}",
                      f"if {c} then click cell {gx} {gy}")
        p.add("if power_out(1) >= 575 then wait 10")
    # MR1: an idle Yuri walking off after a target (R-212) is called back
    for n, home in ((1, (31, 76)), (2, (40, 86))):   # w7r2: Yuri 2 waits at (40,86)
        c = (f"power_out(1) >= 575 and umov(6,YURI,{n}) == 1 and "
             f"upath(6,YURI,{n}) != {home[0] * 1000 + home[1]}")
        p.add(f"if {c} then click xy 500 400 right", f"if {c} then click unit 6:YURI:{n}",
              f"if {c} then click cell {home[0]} {home[1]}")
    p.add("endrepeat")
    p.add("until 900 power_out(1) < 575", "assert power_out(1) < 575",
          "assert outcome == RUNNING", "dump", "shot blackout")
    # 6. the IFV clears the gap; Yuris 1 and 2 walk up to it
    order(p, "6:FV:0", "cell 70 74")
    # (w7r2: Yuri 2 comes from (56,84), Yuri 1 from the west)
    order(p, "6:YURI:2", "cell 62 80")
    for (x, y) in [(40, 77), (44, 78), (53, 78), (58, 78), (58, 76), (62, 76), (64, 74)]:
        order(p, "6:YURI:1", f"cell {x} {y}")
        if (x, y) == (58, 76):
            order(p, "6:YURI:2", "cell 63 76")
        if (x, y) == (64, 74):
            order(p, "6:YURI:2", "cell 65 74")
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
    # (w7r2) from step 4 on the breaching IFV is the second house-6 IFV
    for i in range(main_from, len(p.l)):
        p.l[i] = (p.l[i].replace("6:FV:0", "6:FV:1").replace("(6,FV,0)", "(6,FV,1)")
                  .replace("6:FV:9", "6:FV:0").replace("(6,FV,9)", "(6,FV,0)"))
    p.save(out)


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "m-sov09-real.play"))
