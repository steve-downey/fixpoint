# Handoff — S15 gprepro + gpostpro + zygo_histo_prepro capstone

- **Status:** DONE (gate passed) — capstone shipped, not BLOCKED
- **Commit:** 52193af — `[schemes] S15: gprepro + gpostpro + zygo_histo_prepro`
- **Date / agent:** 2026-07-05, background execution agent

## What changed

- `src/smd/fixpoint/generalized.hpp` extended (S13/S14's shared header, per
  its own top-of-file doc comment, now updated through S15) with:
  - `#include <smd/fixpoint/prepro.hpp>` (new — needed for `hoist`/`prepro`/
    `postpro`).
  - `gprepro_worker_t<Result, WResult, F, Dist, Nat, GAlgebra>` +
    `gprepro(dist, e, algebra, tree) -> Result`: `gcata_worker_t`'s (S13)
    exact shape with one splice — each recursive call is
    `(*this)(hoist<F>(e, child))` instead of `(*this)(child)`, the literal
    "compare §7.4's prepro vs fold_fix — same delta" the step file names.
    `WWR`/`C` computed up front exactly as `gcata_worker_t`'s (Nat/e plays
    no part in either type).
  - `gpostpro_worker_t<F, MSeed, Dist, Nat, GCoalgebra>` +
    `gpostpro(dist, e, coalgebra, seed) -> Fix<F>`: `gana_worker_t`'s (S14)
    exact shape with the dual splice — `hoist<F>(e, (*this)(next))` wraps
    each recursive *result* before it is grafted in (on the way out,
    mirroring postpro.hpp's own shape one level further out). `MFMS`/`MMS`
    computed up front exactly as `gana_worker_t`'s.
  - A dedicated `smd::typeclass::comonad_typeclass<std::pair<Helper,
    Cofree<F,X>>>` partial specialization (`ZygoHistoComonadImpl`/
    `ZygoHistoComonadMap`, in a `namespace smd::typeclass` block between two
    `namespace smd::fixpoint` blocks, cofree.hpp/pair.hpp's own two-namespace
    pattern) — the composed comonad `W<X> = pair<Helper, Cofree<F,X>>`
    zygo_histo_prepro needs, mirroring Haskell's `EnvT e w` instance
    (`duplicate (EnvT e wa) = EnvT e (extend (EnvT e) wa)`): `extract`
    defers to the Cofree's own `extract`; `duplicate` calls the Cofree's own
    `duplicate` + `fmap` to re-attach the *same* Helper environment to every
    position of the Cofree's own duplicated structure (NOT pair.hpp's
    trivial `PairComonadImpl` duplicate, which only re-nests the pair layer
    itself); `fmap` defers to the Cofree's own `fmap`, threading `Helper`
    through unchanged. See Discoveries below — this route was empirically
    *required*, not just chosen for elegance.
  - `dist_zygo_histo_t<F, HelperAlg>` + `dist_zygo_histo<F>(helper)`
    (factory) — capstone-specific, in `generalized.hpp` per the step file
    (NOT `dist_laws.hpp`). Mirrors Kmett's `distZygoT`: folds the Helper
    component via the helper algebra, redistributes the Cofree component via
    `dist_histo<F>` (S12). `F` must be bound explicitly at the call site
    (`dist_zygo_histo<F>(f)`), same DEV-02-class reason as
    `dist_histo`/`dist_para`/`dist_futu`.
  - `zygo_histo_prepro<Result, Helper>(f, e, g, tree) -> Result`: thin
    wrapper, `gprepro<Result, std::pair<Helper, Cofree<F,Result>>>(
    dist_zygo_histo<F>(f), e, g, tree)`.
- `src/smd/fixpoint/CMakeLists.txt`: append-only (`gprepro.t.cpp` added to
  the test-sources list; `generalized.hpp` was already in the FILE_SET).
- New `src/smd/fixpoint/gprepro.t.cpp` (8 tests + `HeaderIsIdempotent`): the
  three gprepro laws (identity-vs-fold_fix on Nat 0..10; take-while-vs-prepro
  on `[3,4,-1,5]`, S06's own fixture, reused verbatim by re-declaration —
  translation-unit boundary, same as every prior step's precedent; k-vs-gcata
  with `dist_histo<NatF>`/Fibonacci, identity_nat, 0..10), the two gpostpro
  laws (identity-vs-unfold_fix on Nat 0..10; cap-at-three-vs-postpro, S06's
  own postpro fixture reused verbatim), the zygo_histo_prepro degeneracy law
  (identity transform, helper ignored, Cofree via `extract` only, degenerates
  to `fold_fix` — three vectors: `{}`, `{1,2,3}`, `{5,4,3,2,1}`), and one
  behavior test (`[3,4,-1,5] -> 4`, helper = remaining-list-length, transform
  = take-while-positive, main algebra keeps the head wherever the *tail's*
  remaining length is even, consulting one step of Cofree history via
  `extract` to accumulate — see Verification evidence for the by-hand trace).
  Three constexpr `static_assert`s (gprepro, gpostpro, zygo_histo_prepro;
  the last reuses the Nat-based degeneracy shape for a compile-time-cheap
  smoke, matching every prior step's "reuse the degenerate case" pattern).
- New `src/examples/generalized_tour.cpp` (design §10): walks fold_fix vs
  `cata_via_gcata` (Nat count, n=5), histo vs `histo_via_gcata` (Fibonacci,
  n=10), dyna vs `ghylo(dist_histo, dist_ana)` (Fibonacci, n=10), and the
  zygo_histo_prepro capstone's own behavior-test computation
  (`[3,4,-1,5] -> 4`), printing each "specialized vs generalized" pair and a
  `bool` match flag; returns 1 (not 0) if any pair mismatches, so a
  regression fails the gate loudly rather than needing eyeballing.
- `src/examples/CMakeLists.txt`: added executable+install block for
  `generalized_tour`, copied from the existing pattern.

## Verification evidence

- `make TOOLCHAIN=gcc-16 test`: **100% tests passed, 0 tests failed out of
  202** (baseline going into this step was **194**, S14's row; net +8,
  matching `gprepro.t.cpp`'s 8 `TEST_CASE`s exactly — confirmed via
  `ctest -N | grep -i "gprepro\|gpostpro\|zygo_histo"`, all 202 pre-existing
  and new tests pass). All 194 pre-existing tests still pass unchanged.
- Explicit rebuild after `touch`ing every changed/new file (`make
  TOOLCHAIN=gcc-16 compile`) with `grep -i warning` over the output
  (excluding the unrelated `VIRTUAL_ENV` venv notice every prior handoff has
  also seen): empty — no compiler warnings, for either the library/test
  target or the new example.
- All three constexpr `static_assert`s (`gprepro_constexpr_smoke`,
  `gpostpro_constexpr_smoke`, `zygo_histo_prepro_constexpr_smoke`) compile
  clean under gcc-16/C++26.
- `.build/build-gcc-16/src/examples/Asan/generalized_tour` runs and exits 0:
  ```
  fold_fix / cata_via_gcata    via dist_cata     : specialized = 5, generalized = 5 (match)
  histo / histo_via_gcata      via dist_histo    : specialized = 55, generalized = 55 (match)
  dyna / ghylo-as-dyna         via histo+ana     : specialized = 55, generalized = 55 (match)
  zygo_histo_prepro capstone   via dist_zygo_histo: specialized = 4, generalized = 4 (match)
  ```
  every pair matches, exit code 0.
- **By-hand trace of the zygo_histo_prepro behavior test** (`[3,4,-1,5] ->
  4`): `take_while_positive_nat` cumulative-hoists `[3,4,-1,5]` to `[3,4]`
  (DEV-01's own discovery: this monotone single-cut transformation is
  observationally identical under cumulative vs. single-pass hoisting, so
  the truncation matches S06's own `prepro` fixture exactly). Over the
  truncated `[3,4]`: `length_helper` (bottom-up remaining-length) gives
  helper=1 at the "4" node (tail is Nil, helper=0) and helper=2 at the "3"
  node (tail is the "4" node, helper=1). `even_tail_length_main` keeps a
  head only when *its own tail's* helper is even: at "4", tail=Nil has
  helper=0 (even) → keep 4, running sum = 4 + 0 = 4; at "3", tail="4"-node
  has helper=1 (odd) → drop 3, running sum = 0 + 4 = 4. Final result = 4,
  matching both the test's `CHECK(result == 4)` and the example's printed
  value.
- **Mutation testing (per the task's DEV-01/S09-S14 precedent, run on the
  capstone's degeneracy law specifically, as instructed)**: two mutations,
  both reverted after confirming discrimination:
  1. Changed `ZygoHistoComonadImpl::extract` to `return w.first;` (the
     Helper component) instead of `return smd::fixpoint::extract(w.second);`
     (the Cofree's head). This is exactly the kind of same-type blind spot
     S12/S14 flagged (Helper=int, Result=int in every one of this file's
     tests, so the mutation still *type-checks* at first glance). Result:
     **hard compile failure**, not a silent wrong answer — because the
     top-level `extract` call in `gprepro`'s own entry point is actually
     invoked with `Y = F<WResult>` (a whole *layer*, the type gcata's engine
     needs `algebra` to consume), never `Y = Result` directly, so `w.first`
     (type `Helper = int`) can never bind to `const Y&` where `Y` is a
     `std::variant<...>` layer type — confirmed by re-running the full gate
     and observing the exact compile error (`invalid initialization of
     reference ... from expression of type 'const int'`), across *every*
     zygo_histo_prepro instantiation (both the constexpr smoke on Nat and
     the runtime test on IntList), not just one test. This is a stronger
     result than a discriminating unit test: the type system itself, not
     just the degeneracy law, catches this class of bug.
  2. Disabled the dedicated `comonad_typeclass<pair<Helper,Cofree<F,X>>>`
     specialization entirely (commented out), falling back to pair.hpp's
     generic `comonad_typeclass<pair<B,A>>` instance for the same type.
     Result: **hard compile failure** in `gprepro_worker_t`'s own `C` type
     computation (`dist(declval<F<WWR>>())`) — the generic instance's
     `duplicate` produces `WWR = pair<Helper, pair<Helper, Cofree<F,X>>>`
     (only re-nesting the pair layer), which does not match
     `dist_zygo_histo_t::operator()`'s required argument shape
     `F<pair<Helper2, Cofree<F,X2>>>` (the actual `WWR` needed is
     `pair<Helper, Cofree<F, pair<Helper, Cofree<F,X>>>>` — Cofree nested one
     level *differently*). This directly and empirically confirms the step
     file's own "thin ice" framing: the generic pair-comonad instance is
     *not* sufficient for gprepro's needs at this `W`, and the dedicated
     instance route (rather than "maybe the existing one already works") was
     the only viable option — settled by evidence, not by inspection alone.
  Both mutations reverted (`diff` against a pre-mutation backup confirmed an
  exact match to the committed state) and the full 202-test gate re-run
  green before committing.

## Deviations from the plan / design

None. No new row added to `ops/DEVIATIONS.md`. The step file's own "thin
ice" question (step 2 of its "Do" list) was resolved empirically in favor of
the dedicated-instance route it flagged as *one* of the two possibilities —
this is exactly the step file's own anticipated outcome space, not a
contradiction of the design, so it is recorded here and in Discoveries
rather than as a deviation row. DEV-03's CTAD hazard (S14) was proactively
avoided: every wrapper construction in the new code
(`std::pair<Helper,...>{...}`, `Cofree<F,...>{...}`) names its type
explicitly rather than relying on CTAD — grepped for bare `Identity{`/
`Cofree{`/`std::pair{` constructions inside the new generic code before
committing; none found.

## Discoveries affecting later steps

- **The composed-comonad question (S15's own "thin ice") is settled: a
  dedicated `comonad_typeclass` specialization is required, not optional,
  whenever a scheme's `W` is built by composing two existing comonads
  through a *specific* categorical construction** (here, the environment
  comonad transformer `EnvT`) rather than through a data type that is merely
  "a pair/Cofree of the pieces". The generic per-type-shape instances in
  this codebase (`PairComonadImpl<B,A>`, `IdentityComonadImpl<A>`,
  `CofreeComonadImpl<F,A>`) are each correct for *their own* comonad in
  isolation, but composing two comonads' *data representations* (a
  `pair<Helper, Cofree<F,X>>` is structurally just a product of an
  environment and a Cofree-annotated value) does not automatically compose
  their *comonad instances* — that requires writing the composed
  `duplicate`/`extract`/`fmap` by hand against the specific composition
  being modeled (`EnvT` here), and the two are different enough that the
  generic instance produces a different (here: differently-nested, hard
  incompatible) type, not merely a "less efficient but equivalent" one. This
  generalizes as a caution for any future step that might compose two
  existing comonads/monads via a shared data shape (S16 does not do this,
  per its own step file, so this is advisory only).
- **A "same Helper/Result type" fixture is a blind spot for
  extract/duplicate orientation bugs in the composed comonad, exactly as
  S12/S14's handoffs found for `dist_zygo`/`apo_via_gana`'s same-type
  fixtures** — every test in this step uses `Helper = int` and `Result =
  int`, so a bug swapping which component `extract` reads would, in
  isolation, still type-check at the call site that matters most naively
  (the *scheme's own* `Result`). What actually catches it here is not a
  same-vs-different-type fixture choice (as it was for `dist_zygo`/`apo`) but
  the fact that `gprepro`'s entry-point `extract` call is invoked with `Y =
  F<WResult>` (a whole functor layer), which is *never* the same type as
  `Helper` regardless of what `Helper`/`Result` happen to be — confirmed by
  mutation testing (see Verification evidence above). Future agents adding
  tests for composed-comonad code should still prefer distinguishable
  types where practical, but should also check whether the *structural*
  types already prevent the blind spot before concluding a stronger fixture
  is needed.
- **`prepro.hpp`'s `hoist`/`prepro`/`postpro` splice directly into
  `gcata_worker_t`/`gana_worker_t`'s recursive call sites with zero type
  friction** — no new type-deduction trick was needed beyond what S06/S13/
  S14 already established; `gprepro_worker_t`/`gpostpro_worker_t` are
  otherwise byte-for-byte copies of `gcata_worker_t`/`gana_worker_t` with one
  line changed each. This is strong evidence the design's own framing
  ("compare §7.4's prepro vs fold_fix — same delta") was exactly right.
- **DEV-01's finding (S06) carries over unchanged to the generalized case**:
  `take_while_positive_nat`'s cumulative-vs-single-pass hoisting distinction
  remains unobservable for this specific monotone-single-cut transformation,
  which is why the `[3,4,-1,5] -> [3,4]` truncation used in this step's
  behavior test matches S06's own fixture's truncation exactly. A future
  agent wanting a *stronger* discriminator for gprepro's own depth semantics
  specifically (as opposed to reusing S06's already-established prepro
  behavior) would need a non-idempotent transformation
  (`decrement_nat`-style, DEV-01) — not attempted here since the step's own
  "Tests" bullet only asks for the take-while fixture, matching S06's own.

## Forward notes for the NEXT step (S16 — packaging + docs)

- **The capstone shipped — nothing is BLOCKED.** S16 can reference
  `zygo_histo_prepro` freely; no fallback/partial-scope handling is needed.
- **`generalized.hpp` is now feature-complete per §8's table** (`gcata,
  gana, ghylo, gprepro, gpostpro, zygo_histo_prepro` — S13/S14/S15's shared
  file, no further schemes are added to it by the plan). When S16 writes
  `schemes.hpp`'s umbrella include and doc-comment map, `generalized.hpp`'s
  own top-of-file comment (updated through this step) already summarizes
  what it contains — a reasonable source to crib the umbrella doc-comment's
  one-line description from, though S16's own step file says to source
  signatures from the headers, not transcribe design-doc prose, which this
  handoff's own descriptions above already do (verified against the actual
  landed code, not the design doc).
- **New test file to wire into `schemes.t.cpp`'s "one scheme from each
  family" compile-coverage list**: `gprepro`/`gpostpro`/`zygo_histo_prepro`
  all live in `generalized.hpp` already covered by S13/S14's inclusion; no
  new header needs adding to the FILE_SET beyond what's already there.
- **`generalized_tour.cpp` (this step, §10's last example) is the reference
  shape for S16's "every §10 example builds, runs, exits 0" sweep** — it
  already returns a non-zero exit code on any specialized-vs-generalized
  mismatch (not just printing), which is stronger than most prior examples'
  "print and trust visual inspection" pattern; worth noting in `docs/
  recursion-schemes.md`'s own generalized_tour section if S16 wants to
  highlight the self-checking design, but not required.
- **Signature to crib for docs**: `zygo_histo_prepro<Result, Helper>(f, e,
  g, tree)` with `f : F<Helper> -> Helper`, `e : F<X> -> F<X>` (natural
  transformation, any `X`), `g : F<std::pair<Helper, Cofree<F,Result>>> ->
  Result` — landed exactly as the design doc's own §7.11 signature
  specifies, no drift to note in the deviations ledger for this particular
  signature.
- Gate remains `make TOOLCHAIN=gcc-16 test` fully green; S16 is a
  documentation/packaging step with no further equivalence-law gates of its
  own beyond the consistency sweep its step file already describes.

## Open risks / TODOs

- gcc-17 secondary smoke was not re-run this step (advisory only per design
  D9/plan); still outstanding since S00, not blocking. S16's own step file
  explicitly asks for one gcc-17 `compile` run as part of its consistency
  sweep — that is the natural place to finally close this out.
- None specific to S15's own scope were left open — gate is green, every
  bullet in the step file's "Do"/"Tests"/"Example" sections has
  corresponding code or a test, the capstone shipped (not BLOCKED), and the
  mutation-testing check was actually run twice (not just asserted in
  prose) — once on the degeneracy law's own `extract` wiring, once on the
  "thin ice" comonad-instance question itself — with both results recorded
  above alongside the exact compile errors observed.
