# Freer effort — bootstrap session record (2026-07-09)

State capture so this can be picked up exactly. What exists, why it
exists, and what the next action is. Written by the session that
performed the code audit, folded it into the design doc, and
constructed the execution plan. Nothing described here is committed
yet (see "Current git state" at the bottom).

## What happened, in order

1. `docs/freer-signature-functor-design.org` (the FD-series design
   fragment for Freer-over-Fixpoint, §4) was audited against the
   *landed* library code — `free.hpp`, `box.hpp`, `fmap.hpp`,
   `fix.hpp`, `mendler.hpp`, `cofree.hpp`, `concrete/functors.hpp`,
   the typeclass CRTP layer, `vcpkg.json`, and the CI matrix.
2. The audit findings were folded back into the design doc as marked
   "code-audit 2026-07-09" amendments.
3. An execution plan was constructed at `ops/freer/` — modeled on the
   completed recursion-schemes `ops/` plan, adapted for **one Opus
   orchestrator managing Sonnet worker agents**.
4. A memory pointer was saved (auto-memory:
   `freer-effort-ops-plan.md`) so future sessions find this.

## The audit findings (the substance behind every amendment)

Confirmed by the code:

- `Free` (`free.hpp:38`) is exactly the right substrate;
  `Freer<Sig,A> = Free<Sig::template type, A>` is sound; recursive
  positions boxed inside the functor per house convention, so
  `impure_node`'s continuation returning `Box<X>` fits with no
  changes to `Free`.
- FD3's "load-bearing workaround" claim is accurate: `Free`'s
  hand-written `operator==` (`free.hpp:45`) is a non-template friend,
  body instantiated only on call; the clang-22 cascade FD9 gates
  against is documented verbatim at `cofree.hpp:39-50`.
- FD4 is right that const paths can't serve: existing fmap/bind
  (`free.hpp:87,149`) are `const&`, rebuild by copy, compute results
  from `invoke_result_t<Fn, const X&>`.
- FD6's O(1)-bind claim survives: the Coyoneda layer has no direct
  children, so bind per Roll is one post-composition.
- Windfall: `functor_typeclass<Free<F,A>>` / `monad_typeclass<...>`
  partial specializations deduce `F = Sig::template type` (member
  class template as template-template argument), so Free-level
  instances extend to Freer with no new registrations.

Problems found (each now has a home in the doc/plan):

1. **Layer-level typeclass registration is unwritable as designed.**
   `layer_fmap` dispatches on
   `functor_typeclass<remove_cvref_t<Layer>>` (`fmap.hpp:63`); a
   partial specialization keyed on `signature<Ops...>::type<X>` (a
   member class template of a class template) is a non-deduced
   context. → New **FD11** (open): constrained variable-template
   partial specialization vs registry bypass via layer_fmap modes 2/3
   vs top-level layer template. Decision **D-A**.
2. **Reference captures become dangling under a lazy layer.**
   Existing traversals capture `[&self, &fn]` (`free.hpp:100,158`) —
   safe only because pure-data instances are eager; the Coyoneda
   layer *stores* the closure. Derived Monad ops have the same
   assumption (`monad.hpp:37,91` — apply captures `[&self, &ma]`,
   kleisli `&self`). → FD4 now carries the **capture-ownership
   rule**, the boundary "derived Monad ops off-limits for Freer",
   and an Asan deferred-invocation gate in S02.
3. **The S/R interpreter needs a new scheme.** `mcata`
   (`mendler.hpp:47`) is `Fix`-only, `const&`; Freer values are
   `Free` with Pure leaves. → S07 names **`mcata_free`** (consuming
   Mendler fold over `Free`). Also beman.execution/beman.task are not
   provisioned (`vcpkg.json` has only catch2) → new step **S07d**.
4. **FD8's "existing ana" doesn't exist.** `unfold_fix` targets `Fix`
   only; nothing unfolds into `Cofree`. → S06 adds
   **`unfold_cofree`**. Deeper point now recorded in FD8: this
   library's `Cofree` is strict (`cofree.hpp:37`, by-value tail), so
   the co-signature functor must be function-space-shaped with a
   *lazy* (post-composing) fmap — the dual of FD6 — or the unfold
   diverges.
5. **`std::move_only_function` vs the real CI floor.** CI runs
   clang 19–22 against libc++ and appleclang; libc++ gained
   `move_only_function` in LLVM 20, Apple later; libstdc++ since
   GCC 12. → New **FD12** (open): feature-macro-gated
   `move_only_function<...&&>` vs a bespoke one-shot callable box.
   Decision **D-B**.
6. **Minor mechanics.** `Box::operator*` is const-qualified returning
   `A&` (`box.hpp:54`) — the consuming path needs `operator*() &&`
   (S02). FD6's sketch granularity corrected: the shipped artifact is
   a Functor typeclass instance over the *whole variant layer*, not a
   per-node free function. S02's scope is `fmap.hpp` + `free.hpp` +
   `box.hpp`, not just `free.hpp`.

## Design-doc amendments (docs/freer-signature-functor-design.org)

All marked "code-audit 2026-07-09" in place:

- **FD3**: addendum — confirmed == mechanics; the deduction windfall;
  the nested-member registration cost (points at FD11).
- **FD4**: corrected scope (no Free unwrap; fmap.hpp + box.hpp
  included); capture-ownership rule; derived-ops boundary; Asan gate.
- **FD6**: granularity correction; Box rvalue-deref note.
- **FD8**: "existing ana" corrected to new `unfold_cofree`; the
  strict-Cofree / lazy function-space co-signature paragraphs.
- **FD9**: three mechanics probes added — (a) member-template
  template-template deduction, (b) constrained variable-template
  partial specialization, (c) `&&`-qualified move_only_function in
  the self-embedding variant; concrete toolchain-floor paragraph.
- **FD11** (new, open): layer-level typeclass registration; candidate
  resolutions in preference order; decided by S01 evidence,
  orchestrator-owned.
- **FD12** (new, open): one-shot callable representation; decision
  rule spelled out; orchestrator-owned.
- **S-sequence**: renumbered — S00 ground; S01 baseline+probes with
  orchestrator checkpoint after; S02 widened; S03 layer per D-A/D-B;
  S04 send+trace; S05 rows; S06 pairing (+unfold_cofree); S07d beman
  provisioning; S07 interpreter (mcata_free); S08 integration. Notes
  S06 ∥ S07d/S07 after S05.
- **§5 forward reference**: mcata_free named as new; beman deps
  flagged as unprovisioned.
- One spelling normalization for codespell in FD10 ("preempted", the
  hyphenated form trips the hook).

## Execution plan (ops/freer/), files as created

- `PLAN.md` — master. Ground rules (one step per worker; dual-compiler
  gate `make TOOLCHAIN=<gcc-pin> test` AND `make
  TOOLCHAIN=<clang-pin> test`; two commits `[freer] SNN: <title>` +
  `[freer] SNN: handoff`; no co-author trailers; observational
  testing only). Phase→branch table (A `freer/baseline` S00–S01; B
  `freer/consuming-traversal` S02; C `freer/signature-layer` S03–S04;
  D `freer/rows` S05; E1 `freer/cofree-pairing` S06; E2
  `freer/sr-interpreter` S07d+S07; F `freer/integration` S08), PR per
  phase, --no-ff merges. **Decisions table with D-A/D-B pending.**
  Checklist with dependencies. Empty status log.
- `ORCHESTRATOR_PROTOCOL.md` — the Opus role: dispatch-prompt
  contents (branch, protocol path, step file, pins, dependency
  handoffs; D-A/D-B quoted in full for S03); independent gate
  re-verification (never trust a worker's DONE); decision
  checkpoints; deviation triage; branch/PR ownership; what workers
  may not do; escalation-to-Steve list (changing a decided FD,
  dropping a CI compiler, twice-failed gate, scope growth).
- `AGENT_PROTOCOL.md` — the Sonnet role: assigned step only; read
  step file + dependency handoffs (authoritative over step files) +
  design refs; never resolve design questions (BLOCKED handoff
  instead); gate; two commits; handoff with forward notes; stop.
- `HANDOFF_TEMPLATE.md` — adds a "Cross-compiler divergences" section
  (paper evidence per FD9).
- `DEVIATIONS.md` — empty table scaffold.
- `steps/00-ground.md` — pins (expected gcc-16 + newest clang with an
  etc/ toolchain file, likely clang-22/23), dual baselines,
  `__cpp_lib_move_only_function` probe per toolchain (D-B input). No
  library changes; a red pre-existing baseline is recorded/BLOCKED,
  never fixed.
- `steps/01-baseline-gate.md` — all probes in ONE test file
  `src/smd/fixpoint/freer_baseline.t.cpp` with *test-local* structs
  (real API lands in S03): FD3-shape TU, ==-non-instantiation, probes
  (a)/(b)/(c), dual-compiler capture, per-probe independence,
  compiler-conditional quarantine for single-compiler failures.
  Produces the D-A/D-B evidence table.
- `steps/02-consuming-traversal.md` — box.hpp `operator*() &&`;
  fmap.hpp rvalue-constrained layer_fmap overloads (all three modes;
  lvalue calls must still select const overloads); free.hpp consuming
  fmap/bind obeying the capture-ownership rule; move-only smoke;
  the named Asan test `"consuming bind: continuation owns its
  captures (FD4)"` with a test-local lazy layer. Explicitly forbids
  "fixing" monad.hpp's derived ops.
- `steps/03-signature-layer.md` — `src/smd/fixpoint/freer.hpp`: unit,
  operation, impure_node (continuation per D-B), signature (+ nested
  typedefs per D-A), Freer alias, generic consuming-only Coyoneda
  Functor instance registered per D-A. Tests re-express S01 against
  real headers; laziness assertion; bind-through-layer via the
  deduction windfall; lifetime pattern against the real layer.
  BLOCKED if dispatch prompt lacks D-A/D-B.
- `steps/04-send-trace.md` — send per FD7; `freer_run.hpp`: loop-based
  (not recursive) `run`, `tracing` adaptor + `run_trace` → the FD5
  assertion vocabulary every later step reuses; KV example tests;
  one-shot-ness as a compile property. Sugar (>>=) explicitly
  deferred to orchestrator.
- `steps/05-rows.md` — `freer_row.hpp`: flattened `row<Sigs...>`
  satisfying D-A's registration keying; `member` concept; send
  injection; `discharge<Sig>(handler, prog)`; FD10's Clock+Network
  as the gate; order-independence via observational equality;
  discharge-shaped Asan lifetime sibling.
- `steps/06-cofree-pairing.md` — `unfold_cofree.hpp` (standalone
  scheme, tested on pure data too); `freer_cosignature.hpp`: lazy
  function-space co-signature + pairing `pair_run`/`pair_run_trace`
  sharing the S04 trace type; gate = FD10 failing-twice script drives
  the retry program synchronously with the exact FD10 assertions
  (three Sends, 1s/2s, no fourth). Retry program kept liftable for
  S08.
- `steps/07d-beman-deps.md` — beman.execution + beman.task, both
  provisioning modes if possible (vcpkg availability to be verified,
  likely FetchContent-only — pinned tags); probe-target-only linkage;
  never hand-edit `infra/`; `make reconf` from-scratch fetch proof.
- `steps/07-sr-interpreter.md` — `mcata_free.hpp` (signature sketch
  in the step: `mcata_free<Result>(pure_alg, phi, Free&&)`, no
  functor lookup, house Mendler stance); `freer_task.hpp`:
  handler = op → sender, `run_task` via mcata_free with
  `beman::task<A>` carrier; all tests deterministic/single-threaded
  on inline/manual schedulers; frame-per-effect pitfall flagged.
- `steps/08-integration.md` — consolidate the retry program once;
  `src/examples/freer_retry.cpp` (three runs: all-mock, live S/R,
  composed mocked-Network/live-Clock); `docs/freer-vs-interface.md`
  side-by-side (fair fight, no straw man; FD10's three points);
  same-assertion-helper test for both interpreters; `make
  testinstall` sweep; closing handoff addressed to the orchestrator
  listing FD-series updates for final doc reconciliation.
- `handoffs/.gitkeep` — placeholder.

All new .md files pass markdownlint and codespell (`make lint`
hooks run on them via pre-commit).

## Current git state (as of end of session)

On branch `main`. Untracked, uncommitted:

- `docs/freer-signature-functor-design.org` (was already untracked
  before this session; now carries all amendments)
- `ops/freer/` (everything above)
- `CLAUDE.md` at repo parent (pre-existing untracked, unrelated)

Nothing was committed or pushed, per standing instructions.

## To pick this up

1. Commit the design doc + `ops/freer/` (suggested: on a
   `freer/baseline` branch as the plan's Phase A opener, or commit
   the plan scaffolding to main first if you prefer the plan itself
   reviewed separately — either is consistent with the plan's rules;
   the branch table starts mattering at S00).
2. Start an Opus session with:
   "Read `ops/freer/ORCHESTRATOR_PROTOCOL.md` and `ops/freer/PLAN.md`
   and begin executing the plan (dispatch S00)."
3. The orchestrator's first real decision point is after S01: resolve
   D-A (FD11) and D-B (FD12) from the probe evidence, record them in
   PLAN.md's Decisions table AND the design doc's FD11/FD12 sections,
   then dispatch S03. S02 can be dispatched in parallel with S01
   (separate branch) any time after S00.
