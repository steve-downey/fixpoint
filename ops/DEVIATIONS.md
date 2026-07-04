# Deviation ledger — implementation reality vs. the design

Each row is a place the build taught us something the design didn't
know. The design author reconciles these into
`docs/recursion-schemes-design.md` §3 (decisions log) and the relevant
section. Equivalence-law failures that forced an equation fix (design
§3 D8) belong here too.

| ID | Step | Design section affected | What the design said | What was true | Recommended doc change |
|----|------|------------------------|----------------------|---------------|------------------------|
| DEV-01 | S06 | step S06's Pitfalls | "the identity law won't catch a wrong version, but the take-while test will (a wrong version stops too early or too late on `[3,-1,4]`-shaped inputs)" | Empirically false for `take_while_positive`: verified by building both `prepro` and a "shallow" variant (applies the transformation once per node when visited, never cumulatively re-hoisting the whole remaining subtree) side by side — both give `3` on `[3,-1,4]`. The transformation is a monotone single-cut: the decision that collapses a subtree to `Nil` is made the first time any layer of it is inspected, so applying it once per node (in visitation order) is observationally identical to applying it cumulatively for *this* transformation family. A non-idempotent transformation (e.g. `decrement every head by 1`) does discriminate: on `[10,10,10]`, correct cumulative `prepro` gives `27`, the shallow variant gives `28`. | Reword the S06 pitfall (and design §7.4's cumulative-cost note) to require a **non-idempotent** natural transformation for the discriminating test, with `decrement_nat`/`[10,10,10]`-style as the worked example, rather than a monotone cut like take-while. The `[3,-1,4]` take-while test is still worth keeping (it does catch "forgot to hoist at all" — root-only application gives `6` instead of `3`) but should not be relied on as the sole prepro depth-correctness gate. |
