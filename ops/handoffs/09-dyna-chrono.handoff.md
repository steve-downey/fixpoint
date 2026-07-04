# Handoff — S09 dyna, codyna, chrono (fused refolds)

- **Status:** DONE (gate passed)
- **Commit:** 5c9e169 — `[schemes] S09: dyna + codyna + chrono`
- **Date / agent:** 2026-07-04, background execution agent

## What changed

- New `src/smd/fixpoint/chrono.hpp` (namespace `smd::fixpoint`, no
  `smd::typeclass` reopening needed — unlike cofree.hpp/free.hpp, these are
  plain free-function-template schemes, not typeclass instances): all three
  course-of-values refold fusions per design §7.6, transcribed directly and
  landed exactly as the design/step file specify (no equation fights, no
  `ops/DEVIATIONS.md` row needed this step):
  - `unroll<F>(coalgebra, chunk) -> F<Free<F, Seed>>` — the shared
    codyna/chrono bridge. `Coalgebra`/`Seed` are template parameters (not
    fixed), `chunk : const Free<F, Seed>&`. Body is a two-arm
    `std::visit(overloaded{...}, chunk.node)`: `Pure s -> coalgebra(s)`;
    `Roll layer -> layer` (returned **by value** — the return type is the
    concrete `F<Free<F,Seed>>`, not a reference, so the S08 handoff's "do
    not return a reference into the variant" pitfall doesn't even arise
    here; it only matters if you're tempted to write `-> const F<...>&`).
    Landed at plain `smd::fixpoint` scope with a doc comment marking it an
    implementation detail — no `detail` namespace, matching the established
    precedent (S08 handoff's forward note; confirmed still true, no header
    in this module uses one) — documented as "implementation detail of
    codyna/chrono" the same way `futu_worker` is documented in futu.hpp.
  - `dyna<Result, F>(algebra, coalgebra, seed) -> Result`: `algebra :
    F<Cofree<F,Result>> -> Result`, `coalgebra : Seed -> F<Seed>` (plain,
    non-Free — dyna never touches Free at all). Body: builds a local
    `combined` lambda (`F<Cofree<F,Result>> -> Cofree<F,Result>`, identical
    shape to histo.hpp's own `combined`) and calls
    `extract(refold<Cofree<F,Result>, F>(combined, coalgebra, seed))`
    directly — `coalgebra` is passed straight through to `refold`, no
    wrapping.
  - `codyna<Result, F>(algebra, coalgebra, seed) -> Result`: `algebra :
    F<Result> -> Result` (plain cata-style, no Cofree), `coalgebra : Seed
    -> F<Free<F,Seed>>` (futu-style). Body: wraps `coalgebra` in a local
    `unroll_step` lambda (`const Free<F,Seed>& -> F<Free<F,Seed>>`, calls
    `unroll<F>(coalgebra, chunk)`) and calls `refold<Result, F>(algebra,
    unroll_step, pure_free<F>(seed))` — the initial seed is
    `pure_free<F>(seed)`, exactly per the S08 handoff's forward note on
    `pure_free`'s call convention (explicit `F` only, `A` deduced from the
    by-value argument).
  - `chrono<Result, F>(algebra, coalgebra, seed) -> Result`: combines both —
    `algebra : F<Cofree<F,Result>> -> Result` (histo-style), `coalgebra :
    Seed -> F<Free<F,Seed>>` (futu-style). Body builds both `combined` and
    `unroll_step` and calls `extract(refold<Cofree<F,Result>, F>(combined,
    unroll_step, pure_free<F>(seed)))`.
  - **None of the three, nor `unroll`, hit the GCC 16 deduced-auto
    self-recursion quirk** (S07/S08 handoffs' Discoveries) — exactly as the
    S08 handoff predicted for this step: all four are free function
    templates with an explicit, non-deduced `Result`/`F<Free<F,Seed>>`
    return type, never a CRTP `Impl` method with a return type deduced from
    `Fn`. No explicit-trailing-return-type workaround was needed anywhere in
    `chrono.hpp`.
  - Confirmed by inspection and by the mutation-test below: **no
    intermediate `Fix<F>` is ever constructed** in any of the three —
    `chrono.hpp` doesn't even `#include <smd/fixpoint/fix.hpp>`'s
    `wrap_fix`/`unwrap_fix` in any scheme body (only pulled in transitively
    via `recursion_schemes.hpp`'s `Fix<F>` type used by `refold`'s
    unrelated `Fix`-based overloads, never invoked here).
- `src/smd/fixpoint/CMakeLists.txt` / `src/examples/CMakeLists.txt`:
  append-only additions (`chrono.hpp` / `chrono.t.cpp` to the FILE_SET/test
  sources; one executable+install block for `dyna_fibonacci`), same
  two-file pattern every prior step touched.
- New `src/smd/fixpoint/chrono.t.cpp` (6 tests): re-inclusion check; the
  three §9 equivalence laws (`dyna` vs. `histo(unfold_fix(...))`, `codyna`
  vs. `fold_fix(futu(...))`, `chrono` vs. `histo(futu(...))`), each looped
  0..10 on `Nat`; Fibonacci directly from an int seed via `dyna` (reusing
  the exact `fib_algebra` shape from `histo.t.cpp`, 0..10); coin-change
  {1,4,5} directly from the amount via `dyna` (reusing the exact
  `coin_change_algebra`/`look_back` shape from `histo.t.cpp`, no
  `nat_from_int` call anywhere in this test); a `dyna_fib_constexpr_smoke()`
  helper (own local lambdas, house pattern) backing
  `static_assert(dyna-fib(6) == 8)`. The dyna/chrono law tests both use
  `fib_algebra` (not a shallower "count" algebra) deliberately — see
  Discoveries below on why a 2-level-history-dependent algebra is a
  meaningfully stronger discriminator here than one that only reads one
  level back.
- New `src/examples/dyna_fibonacci.cpp`: prints `fib(n)` for n in 0..10 via
  `dyna`, with inline comments emphasizing that no `Nat`/`Fix<NatF>` tree
  ever exists — the `Cofree<NatF,int>` history *is* the DP table, built and
  consumed one layer at a time by the same refold pass.

## Verification evidence

- `make TOOLCHAIN=gcc-16 test`: **100% tests passed, 0 tests failed out of
  151** (baseline going into this step was **145**, confirmed via
  `ops/PLAN.md`'s S08 row and `git log` — no commits landed between S08's
  handoff and this step's start; net for S09 is +6, matching the 6 new
  `TEST_CASE`s in `chrono.t.cpp` exactly — HeaderIsIdempotent + 3 laws + 2
  behavior tests). All 145 pre-existing tests still pass unchanged.
- Explicit rebuild of the touched/new files (`touch` + `make
  TOOLCHAIN=gcc-16 compile`) with `grep -i warning` over the output: the
  only match is the unrelated `uv`/`VIRTUAL_ENV` venv notice on stderr, not
  a compiler warning (same false positive noted in every prior handoff).
- `.build/build-gcc-16/src/examples/Asan/dyna_fibonacci` output (exit 0):
  ```
  fib(0) = 0
  fib(1) = 1
  fib(2) = 1
  fib(3) = 2
  fib(4) = 3
  fib(5) = 5
  fib(6) = 8
  fib(7) = 13
  fib(8) = 21
  fib(9) = 34
  fib(10) = 55
  ```
  matches the naive-Fibonacci sequence exactly (also cross-checked against
  `histo.t.cpp`'s own `fib_algebra`-based expectation array, which this test
  file's `dyna behavior: Fibonacci` test reuses verbatim).
- The `static_assert(dyna_fib_constexpr_smoke())` compiles clean under
  gcc-16/C++26.
- **DEV-01 sanity check, run and reverted before committing** (per this
  task's own instructions and the S08 futu.t.cpp precedent): temporarily
  mutated `dyna`'s `combined` lambda in `chrono.hpp` to drop the real layer
  from the stored Cofree tail (`Carrier{algebra(layer), F<Carrier>{}}`
  instead of `Carrier{algebra(layer), layer}`) and re-ran `make
  TOOLCHAIN=gcc-16 compile`. Result: **the build failed outright** —
  `static_assert(dyna_fib_constexpr_smoke())` (a compile-time assertion)
  failed to hold, because the mutated code makes every node's history
  degenerate to a single `Zero` tail, so `fib_algebra`'s two-level lookback
  (`cc.pred->head`) can never see past one layer and returns `1` for every
  `n >= 1` instead of the real Fibonacci sequence. Reverted via
  `cp /tmp/chrono.hpp.bak chrono.hpp` (diffed against the original to
  confirm an exact match) before the full gate re-run and the commit above.
  This confirms the `fib_algebra`-based law/behavior fixtures are not
  degenerate discriminators — a plausible "forgot to thread the history
  through" bug in the fusion genuinely produces a different, wrong answer,
  and does so early (at compile time via the `static_assert`, not just at
  runtime via `CHECK`).

## Deviations from the plan / design

None. No `ops/DEVIATIONS.md` row filed — the design §7.6 equations
transcribed directly with no equation-vs-law conflict (unlike S06's DEV-01),
and no new base-functor-layer `operator==` gaps were hit (this step never
compares whole `Cofree<F,Result>`/`Free<F,Seed>` values with `==`, only
scalar `int` results, so the `Nil`/`Cons`/`Zero`/`Succ` `operator==` gaps
S07/S08 found never come up here).

## Discoveries affecting later steps

- **A law/behavior test built on an algebra that only reads *one* level of
  Cofree/Free history (e.g. a plain "count the layers" algebra) is a
  noticeably weaker discriminator than one that reads two or more levels
  deep** (e.g. `fib_algebra`'s `cc.pred->head`, or `coin_change_algebra`'s
  `look_back(pred, 3)`/`look_back(pred, 4)`), because a "forgot to thread
  the real history through" bug in the fusion (see the mutation-test above)
  still produces a *correct-looking* one-level answer (the count is right;
  only deeper lookups silently go missing) — a one-level fixture would not
  have caught it, or would have caught it only much later, at a shallower
  layer than a shallow test would necessarily probe. Prefer reusing an
  already-multi-level fixture (histo.t.cpp's `fib_algebra`/
  `coin_change_algebra` were both available and reused verbatim here) over
  inventing a new shallow one, when the point of the test is to check that
  a *fusion* correctly threads history/chunks through, not just that a
  scheme computes some Result at all.
- **None of `dyna`/`codyna`/`chrono`/`unroll` needed the S07/S08
  explicit-trailing-return-type workaround** — confirmed the S08 handoff's
  prediction exactly (free function templates with a concrete, explicit
  `Result`/`F<...>` return type never hit the GCC 16 "use of ... before
  deduction of 'auto'" self-recursion diagnostic; only CRTP `Impl` methods
  with a return type *deduced from* the algebra/coalgebra's own
  `invoke_result_t` do). If a later step (S10's Mendler schemes, S11's
  Elgot schemes, etc.) is also a plain free-function template with an
  explicit `Result` return type, expect the same: no workaround needed.
- **`refold`'s lookup (fmap-free) overload
  (`src/smd/fixpoint/recursion_schemes.hpp`, S01, untouched again this
  step) needed zero changes or special-casing to accept `Free<F,Seed>` as
  its `Seed` type** — it genuinely doesn't care what `Seed` is, only that
  the coalgebra/algebra agree on it. This is worth remembering for S12–S14
  (`gcata`/`gana`/`ghylo`, which generalize this exact fusion pattern via
  distributive laws) — `refold` itself is already general enough; the work
  in `dyna`/`codyna`/`chrono` was entirely in choosing the right carrier
  (`Cofree<F,Result>` or plain `Result`) and the right seed-wrapping
  (`pure_free<F>(seed)` or none), not in `refold` itself.
- **The `unroll` coalgebra is intentionally non-recursive** (it peels
  exactly one `F`-layer per call and hands the rest back to `refold` to
  recurse on) — this is structurally different from `futu_worker`
  (futu.hpp), which *does* recurse to fully resolve a `Free` chunk down to
  `Fix<F>`. Do not conflate the two if a future step needs something
  `unroll`-shaped again.

## Forward notes for the NEXT step (S10 — Mendler mcata and mhisto)

- **S10 depends only on S02, not S09** — no file overlap with
  `chrono.hpp`/`chrono.t.cpp`. `src/smd/fixpoint/CMakeLists.txt` and
  `src/examples/CMakeLists.txt` are again the only two files this step
  touched that S10 will also touch (append `mendler.hpp` / `mendler.t.cpp`
  to the FILE_SET/test sources, add one executable+install block for
  `mendler_eval`).
- **S10's mcata/mhisto are a genuinely different shape from every scheme in
  this module so far**: `phi` receives the recursive call itself as a
  callable argument (`phi(recurse, layer)` for mcata, `phi(recurse, unroll,
  layer)` for mhisto) instead of `layer_fmap`ing over pre-computed
  children — **no `functor_typeclass` instance is consulted at all**. This
  step's `dyna`/`codyna`/`chrono`/`unroll` are not a useful template for
  that calling convention (they're all still `layer_fmap`/`refold`-based);
  S10's own step file's worked description (`phi(mcata Φ)(unfix t)`) is the
  right one to transcribe from, not anything in `chrono.hpp`.
- **The GCC 16 deduced-auto quirk (S07/S08, reconfirmed by this step) is
  specific to CRTP `Impl` typeclass-instance methods, not free function
  templates** — mcata/mhisto's `recurse`/`unroll` callables are plain
  lambdas closing over `phi`/the tree, not typeclass methods, so per this
  step's own confirmation you almost certainly will not need the
  explicit-trailing-return-type workaround for S10 either. Only reach for
  it if a step's own step file explicitly says a scheme body is a method on
  some `Impl` (S10's step file doesn't).
- **The DEV-01 "verify your fixture actually discriminates" instruction
  applies to S10's own tests too** — its step file's own Pitfalls section
  already anticipates the analogous risk for `mcata`'s law test (make sure
  the "ignore-fmap, call algebra directly" no-instance proof and the
  mhisto-fib test are checked against *some* mutation, the same way this
  step's handoff did above with `dyna`'s `combined` lambda) — reuse the
  same "temporarily break it, confirm the test fails, revert, diff to
  confirm exact restoration" technique if there's any doubt about a
  fixture's discriminating power.
- Gate remains `make TOOLCHAIN=gcc-16 test` fully green; S10 additionally
  requires the new `mendler_eval` example binary to run and exit 0, and a
  genuine "no functor_typeclass instance" proof test (S10's step file: grep
  the test file for `functor_typeclass` to confirm none was added for the
  test-local opaque functor).

## Open risks / TODOs

- gcc-17 secondary smoke was not re-run this step (advisory only per design
  D9/plan); still outstanding since S00, not blocking.
- None specific to S09's own scope were left open — gate is green, every
  bullet in the step file's "Tests"/"Example" sections has a corresponding
  test or example, the example binary's output was run and pasted above
  (not just claimed), and the DEV-01 fixture-discrimination check was
  empirically run (mutation introduced, build failure observed, revert
  diffed to confirm exact restoration) rather than just asserted in prose.
