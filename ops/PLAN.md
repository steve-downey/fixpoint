# Recursion Schemes — Operational Plan (master)

This is the entry point. Read it fully before doing anything. The
architecture it implements is specified in
`docs/recursion-schemes-design.md`; this plan exists to *test* that
design in real C++26 code. Every step is gated on a fully green build and
test run, and every place reality contradicts the design is logged so the
design author can fold it back in.

## How to use this plan
1. Read `ops/AGENT_PROTOCOL.md` — it defines exactly how to execute one step.
2. Find the first unchecked step below whose dependencies are all checked.
3. Execute only that step. Then stop.

## Ground rules
- **One step per agent.** Never start the next step.
- **No green, no check.** A step's box is ticked only after its
  verification gate passes. If it can't pass, leave it unchecked, write a
  BLOCKED handoff, stop.
- **The gate is `make TOOLCHAIN=gcc-16 test` fully green** — every test,
  new and pre-existing (baseline before this plan: 45). Steps that add
  examples also require each new example binary to run and exit 0.
- **Minimal diffs.** Touch only what the step names. Never edit existing
  passing tests except where a step explicitly says to.
- **One commit per step**, message `[schemes] SNN: <title>`.
- **House style.** New headers/tests follow the conventions in design §4
  and the existing files in `src/smd/fixpoint/` (SPDX, include guards,
  `.t.cpp` tests with re-inclusion check, FILE_SET + test-target wiring
  in the module CMakeLists).
- **Feedback loop.** If reality differs from
  `docs/recursion-schemes-design.md`, do the smallest thing that works,
  append a row to `ops/DEVIATIONS.md`, and note it in your handoff.
- **Trust the laws over the equations** (design §3 D8): if a transcribed
  recursive equation fights its equivalence test, the test wins.

## Build & test (S00 pins these; expected values below)
```bash
cd <repo root>
make TOOLCHAIN=gcc-16 test          # configure+build+ctest, Asan config
make TOOLCHAIN=gcc-16 ctest         # ctest only, after a build
# run one example:
.build/build-gcc-16/src/examples/Asan/<example_name>
# secondary smoke (optional unless a step says otherwise):
make TOOLCHAIN=gcc-17 compile
```
Toolchain: g++-16 (`/usr/bin/g++-16`), C++26 (`-std=gnu++26` via
`etc/gcc-flags.cmake`, bumped in S00). Secondary: g++-17
(`~/.local/bin/g++-17`, personal trunk build; toolchain file added in S00).

## Checklist

### Phase A — Foundations
- [ ] **S00** Toolchain to C++26/gcc-16 + baseline — `ops/steps/00-toolchain-baseline.md`
- [ ] **S01** layer_fmap + typeclass-lookup scheme overloads — `ops/steps/01-layer-fmap.md` (dep: S00)
- [ ] **S02** functors.hpp: reusable base functors — `ops/steps/02-functors.md` (dep: S01)
- [ ] **S03** Identity + either/pair duals + Comonad typeclass — `ops/steps/03-identity-comonad.md` (dep: S00)

### Phase B — Fokkinga's classical extensions
- [ ] **S04** para + apo — `ops/steps/04-para-apo.md` (dep: S02, S03)
- [ ] **S05** zygo + mutu — `ops/steps/05-zygo-mutu.md` (dep: S02)
- [ ] **S06** hoist + prepro + postpro — `ops/steps/06-prepro-postpro.md` (dep: S02)

### Phase C — Course-of-values
- [ ] **S07** Cofree + histo — `ops/steps/07-cofree-histo.md` (dep: S02, S03)
- [ ] **S08** Free + futu — `ops/steps/08-free-futu.md` (dep: S02, S03)
- [ ] **S09** dyna + codyna + chrono — `ops/steps/09-dyna-chrono.md` (dep: S07, S08)

### Phase D — Mendler & Elgot
- [ ] **S10** mcata + mhisto — `ops/steps/10-mendler.md` (dep: S02)
- [ ] **S11** elgot + coelgot — `ops/steps/11-elgot.md` (dep: S02, S03)

### Phase E — Generalizations
- [ ] **S12** Distributive laws — `ops/steps/12-dist-laws.md` (dep: S07, S08)
- [ ] **S13** gcata + recovery laws — `ops/steps/13-gcata.md` (dep: S12, S05)
- [ ] **S14** gana + ghylo + recovery laws — `ops/steps/14-gana-ghylo.md` (dep: S13, S09)
- [ ] **S15** gprepro + gpostpro + zygo_histo_prepro — `ops/steps/15-gprepro-capstone.md` (dep: S14, S06)

### Phase F — Packaging
- [ ] **S16** Umbrella header, docs catalog, final sweep — `ops/steps/16-packaging-docs.md` (dep: all of A–E)

## Status log (S00 + each agent appends one line)
| Step | Agent date | Commit | Gate result (test count) | Handoff |
|------|-----------|--------|--------------------------|---------|
