# Handoff — S11 elgot + coelgot

- **Status:** DONE (gate passed)
- **Commit:** b73f64d — `[schemes] S11: elgot + coelgot`
- **Date / agent:** 2026-07-04, background execution agent

## What changed

- New `src/smd/fixpoint/elgot.hpp` (namespace `smd::fixpoint`): direct
  transcriptions of design §7.8's equations.
  - `elgot<Result, F>(algebra, coalgebra, seed) -> Result`: `coalgebra`
    returns `smd::typeclass::either<Result, F<Seed>>` (D4: Left =
    answer, stop; Right = one more layer, continue). Body calls
    `coalgebra(seed)` once and dispatches via `smd::typeclass::match`
    (per §5.2's own guidance to prefer `match` over hand-rolled
    `is_left`): the Left branch returns the answer directly with **no
    further recursion or fmap of any kind**; the Right branch
    `layer_fmap`s the recursive `elgot` call over the layer and applies
    `algebra`. This "Left branch does nothing but return" property is
    the entire short-circuit feature and is what the invocation-counter
    test (see below) actually verifies — a numeric-only check cannot
    tell it apart from a buggy version that peeks one layer further.
  - `coelgot<Result, F>(algebra, coalgebra, seed) -> Result`:
    `coalgebra` returns plain `F<Seed>` (no short-circuit). Body binds
    `auto layer = coalgebra(seed);` to a **local exactly once**, then
    `layer_fmap`s the recursive `coelgot` call over `layer` and calls
    `algebra(seed, evaluated)` — the seed is passed as a **separate
    first argument**, not packed into a `std::pair` (design §7.8's own
    C++ snippet comment: "pair flattened").
  - Header comment states the D4 orientation explicitly (Left =
    stop/Right = continue) and cross-references apo.hpp's identical
    convention, plus the "coalgebra evaluated exactly once" pitfall —
    per this step's own capture-in-handoff instruction, this is the one
    place a future reader (S12's `dist_apo`, or anyone else touching
    `either`-shaped short-circuit code) should look.
- New `src/smd/fixpoint/elgot.t.cpp` (7 `TEST_CASE`s):
  `HeaderIsIdempotent`; the §9 degeneracy laws — `elgot(phi,
  always-Right(psi))` ≡ `refold(phi, psi)` and `coelgot(ignore-seed(phi),
  psi)` ≡ `refold(phi, psi)`, both swept over Nat 0..10 using a shared
  local `nat_count_psi`/`nat_count_phi` pair (a plain unfold/count over
  `NatF<int>`, seed = plain `int`, not `Fix`); the central behavior +
  discriminating-power test — product over `IntListF<std::size_t>`
  generated from a `std::vector<int>{2,3,0,5,7}`, coalgebra returns
  `make_left<IntListF<std::size_t>>(0)` the instant it sees a 0 and
  otherwise `make_right<int>(...)`, a mutable `int&` invocation counter
  captured by reference asserts **both** `result == 0` **and**
  `invocations == 3` (indices 0,1,2 only; 3,4 — values 5,7 — never
  examined); the edge case — coalgebra short-circuits on the *very
  first* call (`{0,5,9}`, seed 0), asserting `invocations == 1`; a
  dedicated coelgot invariant — a counting `NatF<int>` coalgebra swept
  over seed 6 asserts `invocations == 7` (one call per seed 6..0), added
  specifically to guard the "evaluate ψ(seed) exactly once" pitfall;
  the coelgot seed-using behavior test — running-indices-via-prepend on
  Nat (`coelgot<std::vector<int>, NatF>` from seed 5 yields
  `{5,4,3,2,1,0}`, hand-verified); one `static_assert` per scheme
  (`elgot_constexpr_smoke`, `coelgot_constexpr_smoke`), each a
  self-contained local-lambda algebra/coalgebra pair over `NatF<int>`
  (house pattern from histo.t.cpp/mendler.t.cpp).
- `src/smd/fixpoint/CMakeLists.txt` / `src/examples/CMakeLists.txt`:
  append-only (`elgot.hpp`/`elgot.t.cpp` to the FILE_SET/test sources;
  one executable+install block for `elgot_shortcircuit`).
- New `src/examples/elgot_shortcircuit.cpp`: product of
  `{4,3,0,5,9,2}` via `elgot`, prints `elements examined: 3 / 6` next to
  the product (0) — the point being that the product alone (0) would
  look identical whether or not the short-circuit actually happened;
  the examined-count is what makes it visible.

## Verification evidence

- Clean rebuild (`rm -rf .build/build-gcc-16 && make TOOLCHAIN=gcc-16
  test`): **100% tests passed, 0 tests failed out of 164** (up from
  S10's 157; net +7 — 6 from the step file's own ask plus one extra
  invariant test added for the coelgot "exactly once" pitfall). Zero
  compiler warnings (`grep -i warning` over the full build log found
  only unrelated venv-path notices from the build tooling itself, no
  compiler diagnostics).
- `.build/build-gcc-16/src/examples/Asan/elgot_shortcircuit` output
  (exit 0):
  ```
  values: 6 elements
  product (bails out at the first 0): 0
  elements examined: 3 / 6
  ```
  — demonstrably short-circuits (3 < 6) per the step's own gate wording.
- Both `static_assert`s (`elgot_constexpr_smoke`,
  `coelgot_constexpr_smoke`) compile clean under gcc-16/C++26.
- **Mutation-testing (DEV-01 discriminator check), run and reverted
  before committing**, following S09's/S10's technique, applied twice
  here — once for each pitfall this step's tests are built to catch:
  1. **elgot's short-circuit reality.** Backed up `elgot.hpp` to the
     scratchpad, then inserted `(void)coalgebra(child);` immediately
     before the recursive `elgot<Result, F>(algebra, coalgebra, child)`
     call inside the Right branch's `layer_fmap` lambda — a "peek
     ahead, discard" bug: it calls the coalgebra one extra
     (discarded) time per recursive step, never changing which seed
     ultimately triggers Left or what gets multiplied. Rebuilding and
     running (`make TOOLCHAIN=gcc-16 test`) **failed exactly at
     `elgot behavior: product-with-zero bailout counts invocations
     (IntListF)`**: `2 | 1 passed | 1 failed` — the `result == 0`
     assertion still passed (confirming a numeric-only check would
     have missed this bug entirely), while `invocations == 3` failed
     (the mutated build showed 5). This is the concrete evidence the
     step's central discriminating claim is real: counting invocations
     catches a bug that the final product cannot.
  2. **coelgot's "exactly once" pitfall.** Same file, reverted first,
     then changed the body to `(void)coalgebra(seed);` followed by
     `auto layer = coalgebra(seed);` (i.e. evaluates the coalgebra
     twice, discarding the first) — the literal transcription mistake
     the step file's pitfall warns against. Rebuilding and running
     **failed exactly at `coelgot invariant: coalgebra evaluated
     exactly once per seed (Nat)`**: `result == 6` still passed,
     `invocations == 7` failed (mutated build showed 14). Confirms the
     dedicated invariant test (added specifically for this pitfall,
     beyond what the step file's own bullet list asked for) is a real
     discriminator, not just asserted-by-inspection.
  - Both times: reverted via `cp <scratchpad backup> elgot.hpp`,
    `diff`-confirmed byte-for-byte identical to the pre-mutation
    version before re-running the full gate (green, 164/164) and only
    then committing.

## Deviations from the plan / design

None. No `ops/DEVIATIONS.md` row filed — the design §7.8 equations
transcribed directly with no equation-vs-law conflict, and no
implementation reality contradicted the design doc.

## Discoveries affecting later steps

- **`match`'s Left-branch lambda return type must be named as exactly
  `Result`, not a template parameter or `auto` alone, when the two
  branches of `match` would otherwise be ambiguous to deduce a common
  return type from.** In `elgot`, the Left branch is
  `[](const Result &answer) -> Result { return answer; }` (explicit
  parameter and return type) and the Right branch is
  `[&](const auto &layer) -> Result { ... }` — both branches need an
  explicit `-> Result` so `std::visit`'s common-return-type deduction
  inside `match` doesn't have to reconcile two different deduced types;
  this compiled cleanly on the first attempt with explicit return
  types annotated on both lambdas, consistent with `apo.hpp`'s own
  worker (`match(step, [](const auto&) -> Fix<F> {...}, ...)`).
- **`layer_fmap`'s lambda closing over `elgot`/`coelgot` recursively
  needs the child parameter typed as `const Seed &`, not `auto`** — an
  abbreviated-template lambda parameter here would still deduce fine
  (unlike S10's Mendler-style pitfall, which was about passing a
  generic-parameter callable *as* the scheme's own algebra argument,
  not about a lambda used *inside* the scheme body) since `layer_fmap`
  itself is a plain template function, not the thing S10 warned about.
  Both spellings would likely have worked; `const Seed &` was used for
  consistency with every other scheme in this tree (`apo`, `zygo`,
  etc.) and because it makes the recursive-call type explicit at the
  call site.
- **`make_left<R, L>(v)` / `make_right<L, R>(v)`'s explicit-argument
  order (S03's landed convention, confirmed again here) is exactly what
  the design snippet's own comment says**: `make_left`'s *only*
  explicit argument needed in practice is `R` (the non-deducible,
  "other side" type — here `IntListF<std::size_t>` for the Left/stop
  case, since `L` deduces fine from the literal `0` argument);
  `make_right`'s only explicit argument needed is `L` (here `int`,
  since `R` deduces from the `IntListF<std::size_t>{...}` argument).
  No new usage pattern beyond what S04's apo.t.cpp already established.
- **The Mendler-style abbreviated-template-parameter pitfall (S10) does
  NOT apply here, confirmed empirically** — `elgot`'s/`coelgot`'s
  `Algebra`/`Coalgebra` template parameters are plain concrete-typed
  parameters (not `auto`-parameterized rank-2-style callables), so both
  free functions (`nat_count_phi`, `nat_count_psi`,
  `running_indices_algebra` — all declared in an anonymous namespace,
  non-template, non-constexpr, used only in runtime `TEST_CASE`s) and
  lambda variables work interchangeably as arguments, exactly as S10's
  own forward note predicted.
- **Product-with-zero is a deliberately weak numeric discriminator, and
  that weakness is the pedagogical point of this step**: any
  implementation that eventually multiplies by 0 gives the right final
  answer regardless of exactly how many extra layers past the cutoff
  it might have (wrongly) expanded and discarded. This is *not* a flaw
  in the test design — it is *why* the invocation counter, not the
  product value, is the load-bearing assertion. Future steps
  documenting or reviewing this scheme should not be tempted to trim
  the counter check as "redundant" with the product check; they check
  different things and only the counter is a real discriminator for
  this particular bug class (confirmed by the mutation test above).

## Forward notes for the NEXT step (S12 — Distributive laws)

- **S12 depends on S07, S08 (not S11)** — no file overlap with
  `elgot.hpp`/`elgot.t.cpp`. `src/smd/fixpoint/CMakeLists.txt` is again
  the only file both this step and S12 touch (S12 appends
  `dist_laws.hpp`/`dist_laws.t.cpp` to the FILE_SET/test sources; S12
  has **no** new example-binary requirement per its own step file, so
  `src/examples/CMakeLists.txt` is not touched by S12).
- **`dist_apo`'s Left/Right orientation is exactly the same D4
  convention this step (and S04's apo) already nail down**: Left =
  stop/graft, Right = continue. `either<Fix<F>, F<X>> -> F<either<Fix<F>,
  X>>` per S12's own step file — `smd::typeclass::map_left`
  (`either.hpp`, S03) is the tool to reach for on the Left side (per
  S12's own step-file hint), and `match`/`fanin` remain the preferred
  elimination tools over hand-rolled `is_left` branching (§5.2's
  guidance, followed consistently by apo.hpp and this step's
  elgot.hpp).
- **If S12's own tests want an invocation-counting-style discriminator
  for `dist_apo`'s "Left fans children out as Lefts, doesn't touch
  Right" claim, the pattern in this step's elgot.t.cpp (mutable
  reference counter captured by a lambda, asserted against an exact
  expected count alongside — not instead of — the value-level check) is
  directly reusable.** S12's step file doesn't explicitly ask for
  invocation counting (its own listed tests are shape/value round-trips
  and a naturality spot-check), but the same DEV-01 discipline (a
  fixture only discriminates correct-vs-wrong if a plausible bug would
  actually produce a different, wrong outcome) is worth applying if any
  of S12's dist-law tests turn out to have a similarly weak numeric
  fixture.
- **`either`'s `map_left` (already landed, S03,
  `src/smd/typeclass/either.hpp`) is generic and ready for `dist_apo`/
  `dist_gapo` exactly as S12's step file expects** — nothing in S11
  touched `either.hpp`; it is exactly as S03/S04/S10 left it. `match`,
  `fanin`, `is_left`, `left`/`right` are all still current (confirmed by
  inspection, not just by trusting the earlier handoffs — S11's own
  `elgot.hpp` exercises `match` directly and it behaves exactly as
  documented).
- **Nothing about S11's `elgot`/`coelgot` needs to be recovered/reused
  by S12** (S12 is scoped to `dist_laws.hpp` only, consumed starting
  S13) — S12 can proceed without reading this handoff's "What changed"
  in depth, only the D4-orientation confirmation above and the
  either.hpp-is-unchanged note are load-bearing for S12 specifically.
- Gate remains `make TOOLCHAIN=gcc-16 test` fully green; S12 has no new
  example-binary requirement (confirmed above) — its gate is "full
  suite green; the naturality checks pass" per its own step file, no
  additional binary-runs-and-exits-0 bullet.

## Open risks / TODOs

- gcc-17 secondary smoke was not re-run this step (advisory only per
  design D9/plan); still outstanding since S00, not blocking.
- None specific to S11's own scope were left open — gate is green,
  every bullet in the step file's "Tests"/"Example" sections has a
  corresponding test or example, the example binary's output was run
  and pasted above (not just claimed), and both of this step's central
  discriminating claims (elgot's short-circuit reality, coelgot's
  exactly-once coalgebra evaluation) were empirically mutation-tested
  (bug introduced, exact failing assertion observed, revert diffed
  byte-for-byte before recommitting) rather than just asserted in
  prose.
