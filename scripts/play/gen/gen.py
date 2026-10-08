"""Tiny generator for long real-order .play scripts (W5-A mission proofs)."""


class P:
    def __init__(self, header):
        self.l = [header.rstrip("\n")]

    def add(self, *lines):
        for x in lines:
            self.l.append(x)
        return self

    def build(self, bid, tab=0, wait=2400, cat="structure", count_key=None, robust=False):
        """Queue one structure, wait for it, place it, wait until it exists.
        robust: re-click the cameo until the queue holds it (P5: a cameo that
        shifts on the frame a new prerequisite unlocks swallows one click)."""
        ck = count_key or f"buildings(0,{bid})"
        self.add(f"let n_{bid} = built(0,{bid})",
                 f"until 900 can_build({bid}) == 1",
                 f"click tab:{tab}")
        if robust:
            self.add("wait 2", f"cycle cameo:{bid} queue({cat}) == {bid}")
        else:
            self.add(f"click cameo:{bid}")
        self.add(
                 f"until {wait} queue_ready({cat}) == 1",
                 f"click cameo:{bid}",
                 "place auto",
                 f"until 600 built(0,{bid}) > $n_{bid}")
        return self

    def train(self, uid, tab, n, guard=False):
        if guard:
            g = f"outcome == RUNNING and cameo({uid}) == 1"
            self.add(f"if {g} then click tab:{tab}")
            for _ in range(n):
                self.add(f"if {g} then click cameo:{uid}")
            return self
        self.add(f"until 900 cameo({uid}) == 1", f"click tab:{tab}")
        for _ in range(n):
            self.add(f"click cameo:{uid}")
        return self

    def select_types(self, types):
        """Select every live unit of these types (click + shift-click one each, T twice)."""
        first = True
        for t in types:
            self.add(f"if units(0,{t}) > 0 then click unit 0:{t}:0{'' if first else ' shift'}")
            first = False
        self.add("key t", "key t")
        return self

    def attack(self, types, target):
        self.select_types(types)
        self.add(f"click {target}")
        return self

    def text(self):
        return "\n".join(self.l) + "\n"

    def save(self, path):
        open(path, "w").write(self.text())
