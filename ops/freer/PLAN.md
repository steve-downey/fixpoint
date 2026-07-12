# Freer over Fixpoint — Operational Plan (master)

This is the entry point. The design it implements is
`docs/freer-signature-functor-design.org` (the FD-series); this plan
exists to *test* that design in real code. Every step is gated on a
fully green dual-compiler build, and every place reality contradicts
the design is logged in `ops/freer/DEVIATIONS.md` so it can be folded
back into the FD-series.

This plan is executed by **Sonnet worker agents managed by a single
Opus orchestrator**. The division of labor is absolute:

- The **orchestrator** (Opus) owns: step dispatch, branch/PR
  management, independent gate verification, the decision checkpoints
  below, deviation triage, and keeping the design doc in sync. It
  reads `ops/freer/ORCHESTRATOR_PROTOCOL.md`.
- A **worker** (Sonnet) owns: exactly one step, executed per
  `ops/freer/AGENT_PROTOCOL.md`. Workers never make design decisions;
  anything that smells like one is a BLOCKED handoff, not an
  improvisation.

## Ground rules

- **One step per worker agent.** Never start the next step.
- **No green, no check.** A step's box is ticked only after its gate
  passes. If it can't pass, leave it unchecked, write a BLOCKED
  handoff, stop.
- **The gate is dual-compiler**: `make TOOLCHAIN=<gcc-primary> test`
  AND `make TOOLCHAIN=<clang-primary> test` fully green — every test,
  new and pre-existing. S00 pins the actual toolchain names and
  baseline counts in the Status log; those pins are authoritative for
  every later step.
- **Minimal diffs.** Touch only what the step names. Never edit
  existing passing tests except where a step explicitly says to.
- **Two commits per step**: the step commit `[freer] SNN: <title>`,
  then a bookkeeping commit `[freer] SNN: handoff` carrying the ticked
  box, Status-log row (citing the step commit's hash), the handoff
  file, and any DEVIATIONS rows. No co-author trailers.
- **House style.** New headers/tests follow the conventions of the
  existing files in `src/smd/fixpoint/` (SPDX header, include guards,
  `.t.cpp` Catch2 tests, FILE_SET + test-target wiring in the module
  CMakeLists, `make lint` clean).
- **Feedback loop.** If reality differs from the design doc, do the
  smallest thing that works, append a row to
  `ops/freer/DEVIATIONS.md`, and flag it in the handoff. The
  orchestrator folds accepted deviations back into the FD-series.
- **Observational testing (FD5).** Freer values are not
  equality-comparable; tests assert on `run(handler, prog)` results
  and traces, never on structural equality.

## Build & test (S00-pinned actuals)

```bash
cd <repo root>
make TOOLCHAIN=gcc-16 test        # primary GCC, Asan config
make TOOLCHAIN=clang-23 test      # primary Clang, Asan config
make lint                         # pre-commit hooks
```

Pins (S00): **gcc-16** primary GCC (`g++-16` on PATH,
`etc/gcc-16-toolchain.cmake`), **clang-23** primary Clang (`clang++-23`
on PATH, `etc/clang-23-toolchain.cmake` — newest clang with both a
binary and a toolchain file; clang++-17..23 are all on PATH but 23 is
the newest pairing). No substitution needed. Baseline (untouched
tree): 220/220 passed under gcc-16, 220/220 passed under clang-23.
`__cpp_lib_move_only_function` (D-B input, S00 probe): defined as
`202110L` and `std::move_only_function<int(int) &&>` well-formed under
both gcc-16 (libstdc++) and clang-23 (libstdc++, the toolchain's
default stdlib). Under clang-23 `-stdlib=libc++` (libc++-23-dev
installed locally): the feature-test macro is **not** defined (libc++'s
`version` header has the `#define` for it commented out) and
`std::move_only_function` is not even a known name in `namespace std`
under that libc++ — the type is unimplemented there, not merely
untagged. See `ops/freer/handoffs/00-ground.handoff.md` for the full
probe transcript.

## Branch & merge strategy (orchestrator-owned)

Each phase is a feature branch off `main`, PR'd on phase completion,
merged `--no-ff`. The next phase branches from the updated `main`.
Parallel phases (A∥B, and S06∥S07d/S07 inside Phase E) get separate
branches/worktrees; the orchestrator sequences their merges.

| Phase | Branch | Steps |
|-------|--------------------------|-------|
| A | `freer/baseline` | S00, S01 |
| B | `freer/consuming-traversal` | S02 |
| C | `freer/signature-layer` | S03, S04 |
| D | `freer/rows` | S05 |
| E1 | `freer/cofree-pairing` | S06 |
| E2 | `freer/sr-interpreter` | S07d, S07 |
| F | `freer/integration` | S08 |

## Decision checkpoints (orchestrator only — never a worker)

| ID | After | Decides | Input | Resolution |
|----|-------|---------|-------|------------|
| D-A | S01 | FD11 — layer-level typeclass registration: constrained variable-template partial specialization vs registry bypass (NTTP/explicit-object) vs top-level layer template | S01 probes (a),(b) verbatim results | **RESOLVED 2026-07-11 → option (a)** — constrained variable-template partial specialization of `functor_typeclass`, keyed on a Coyoneda-specific marker typedef the signature layer exposes. Probe (a) confirmed the FD3 windfall (no `Free<F,A>`-level registration); probe (b) confirmed the mechanism selects correctly, identical on gcc-16 & clang-23. Only (a) composes with the implicit-mode `layer_fmap` the const & S02-consuming `Free` fmap/bind already use (zero threading). Modes 2/3 retained as escape hatch; (c) rejected. Marker MUST be Coyoneda-specific (not a generic name). See design doc FD11 (decided). |
| D-B | S01 | FD12 — continuation representation: `std::move_only_function<...&&>` gated on the feature-test macro vs bespoke one-shot callable | S00 macro probe + S01 probe (c) | **RESOLVED 2026-07-11 → bespoke `one_shot<Sig>`** (uniform, not macro-gated). FD12's own decision rule takes the bespoke type unless probe (c) is green on every compiler the Freer layer claims; DEV-S00-1 shows libc++ (a CI row) lacks `std::move_only_function` entirely at LLVM 23, so the rule mandates bespoke. Keeps every CI row (libstdc++/libc++/appleclang), carries one-shot semantics directly (call consumes; second call is a hard error). `std::move_only_function` fast-path deferred to an FD6 optimization. S03 delivers the type. See design doc FD12 (decided). |

The orchestrator records each resolution here AND updates the FD11/
FD12 sections of the design doc (status: open → decided, with the
compiler evidence) before dispatching S03.

## Checklist

### Phase A — Ground truth
- [x] **S00** Toolchain pins + baseline capture + feature probe — `ops/freer/steps/00-ground.md`
- [x] **S01** FD9 baseline gate TU + mechanics probes — `ops/freer/steps/01-baseline-gate.md` (dep: S00)
- [ ] **D-A / D-B** resolved by orchestrator (dep: S01)

### Phase B — Substrate (parallel with Phase A after S00)
- [x] **S02** Consuming traversal: fmap.hpp + free.hpp + box.hpp — `ops/freer/steps/02-consuming-traversal.md` (dep: S00)

### Phase C — The layer
- [x] **S03** unit/operation/impure_node/signature/Freer + generic Coyoneda instance — `ops/freer/steps/03-signature-layer.md` (dep: S01+D-A+D-B, S02)
- [ ] **S04** send + trace handler + observational test vocabulary — `ops/freer/steps/04-send-trace.md` (dep: S03)

### Phase D — Composition
- [ ] **S05** Rows: variant-of-signatures, Member injection, handler adaptors — `ops/freer/steps/05-rows.md` (dep: S04)

### Phase E — Interpreters (E1 ∥ E2)
- [ ] **S06** Cofree pairing mock: unfold_cofree + lazy co-signature + pairing — `ops/freer/steps/06-cofree-pairing.md` (dep: S05)
- [ ] **S07d** Provision beman.execution/beman.task (dual-mode) — `ops/freer/steps/07d-beman-deps.md` (dep: S00; parallel to anything)
- [ ] **S07** S/R interpreter: mcata_free + task carrier — `ops/freer/steps/07-sr-interpreter.md` (dep: S05, S07d)

### Phase F — Integration
- [ ] **S08** FD10 end-to-end: retry example, both interpreters, interface comparison — `ops/freer/steps/08-integration.md` (dep: S06, S07)

## Status log (S00 + each worker appends one line)

| Step | Agent date | Commit | Gate result (per toolchain test counts) | Handoff |
|------|-----------|--------|------------------------------------------|---------|
| S00 | Sonnet 2026-07-11 | 88072d8 | gcc-16: 220/220 passed; clang-23: 220/220 passed | `ops/freer/handoffs/00-ground.handoff.md` |
| S01 | Sonnet 2026-07-11 | f3f906c (+ lint fixup 1ccd967) | gcc-16: 223/223 passed; clang-23: 223/223 passed | `ops/freer/handoffs/01-baseline-gate.handoff.md` |
| S02 | Sonnet 2026-07-11 | e8751c3 | gcc-16: 229/229 passed; clang-23: 229/229 passed | `ops/freer/handoffs/02-consuming-traversal.handoff.md` |
| S03 | Sonnet 2026-07-11 | 9dc11fe | gcc-16: 245/245 passed; clang-23: 245/245 passed | `ops/freer/handoffs/03-signature-layer.handoff.md` |
