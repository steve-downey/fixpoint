# Handoff — S13 gcata + recovery laws

- **Status:** DONE (gate passed)
- **Commit:** 03f8a51 — `[schemes] S13: gcata + recovery laws`
- **Date / agent:** 2026-07-04, background execution agent

## What changed

- New `src/smd/fixpoint/generalized.hpp` (namespace `smd::fixpoint`;
  S14/S15 extend this same header):
  - `gcata_worker_t<Result, WResult, F, Dist, GAlgebra>` — implementation
    detail, a namespace-scope struct (not part of the public API) holding
    `const Dist&`/`const GAlgebra&` and computing, up front, `using WWR =
    decltype(comonad_typeclass<WResult>.duplicate(declval<WResult>()))`
    and `using C = decltype(dist(declval<F<WWR>>()))` exactly as design
    §7.10 prescribes. `operator()(const Fix<F>&) const -> C` is the
    worker `c`: `dist(layer_fmap(λchild -> WWR { return
    comonad_typeclass<WResult>.duplicate(comonad_typeclass<WResult>.fmap(
    algebra, (*this)(child))); }, unwrap_fix(t)))`. Recurses via
    `(*this)(...)`, matching dist_histo_t/dist_futu_t's established
    pattern (S12) for self-recursion-through-a-lambda with an explicit
    trailing return type.
  - `gcata<Result, WResult, F, Dist, GAlgebra>(dist, algebra, tree) ->
    Result`: builds one `gcata_worker_t` and returns
    `algebra(comonad_typeclass<WResult>.extract(worker(tree)))` — the
    literal `g(extract(c t))` from design §7.10.
  - `cata_via_gcata<Result>(algebra, tree)`, `histo_via_gcata<Result>`,
    `zygo_via_gcata<Result, Helper>`, `para_via_gcata<Result>` — thin
    wrappers, see Discoveries below for which need a projection and
    which don't.
- `src/smd/fixpoint/CMakeLists.txt`: append-only (`generalized.hpp` /
  `generalized.t.cpp` added to the FILE_SET/test-sources), same pattern
  every prior step used.
- New `src/smd/fixpoint/generalized.t.cpp` (7 discovered tests +
  `HeaderIsIdempotent`): `cata_via_gcata` ≡ `fold_fix` on Nat count 0..10
  and on `IntListF` sum (three vectors incl. empty); `histo_via_gcata` ≡
  `histo` on the Fibonacci fixture, 0..10; `zygo_via_gcata` ≡ `zygo` on
  the S05 `zygo_balanced`/height fixture (both the balanced and
  unbalanced tree); `para_via_gcata` ≡ `para` on the S04 `tails` fixture
  ([1,2,3]); one direct `gcata<int, Identity<int>>(dist_cata,
  algebra_prime, tree)` call pinning the public spelling (per the step
  file's own request), checked against `fold_fix` over Nat 0..10; one
  constexpr `static_assert` on `cata_via_gcata`.

## Verification evidence

- `make TOOLCHAIN=gcc-16 test`: **100% tests passed, 0 tests failed out
  of 183** (baseline going into this step was **176**, S12's row; net
  +7, matching the 7 new `TEST_CASE`s in `generalized.t.cpp` exactly —
  confirmed via `ctest -N | grep -i gcata`, which lists all 7 by name).
  All 176 pre-existing tests still pass unchanged.
- Explicit rebuild after `touch`ing both new files
  (`make TOOLCHAIN=gcc-16 compile`) with `grep -i warning` over the
  output: only the unrelated `uv`/`VIRTUAL_ENV` venv notice every prior
  handoff has also seen — no compiler warnings.
- The constexpr `static_assert` (`cata_via_gcata_constexpr_smoke`)
  compiles clean under gcc-16/C++26.
- **The whole thing compiled and passed on the first attempt** — no
  iteration was needed on the worker's type composition (see Discoveries
  for why: the design's own §7.10 math is exactly type-consistent once
  written out).

## Deviations from the plan / design

None. No row added to `ops/DEVIATIONS.md`. The §7.10 composition
transcribed exactly as given (`c = k . fmapF(duplicate . fmapW g . c) .
unfix`, `gcata k g = g(extract(c t))`) and type-checked without needing
any correction — the step file's own hedge ("if the composition doesn't
type-check as written, trust the recovery laws") did not need to be
invoked. The one simplification made (a single `comonad_typeclass`
lookup instead of "one lookup from WResult and one from C", which the
step file flagged as an open question — "let the code drive which
lookups are actually required and record it") is not a deviation from
the design's *equations*; it's exactly the guidance being followed, and
is recorded below under Discoveries rather than in the ledger, since
nothing in the design's prose or equations turned out to be wrong.

## Discoveries affecting later steps

- **Only ONE `comonad_typeclass` lookup is needed per `gcata` call, keyed
  on `WResult`, not re-keyed at `C` or anywhere else** — confirmed by
  this step compiling and passing with exactly one lookup site
  (`comonad_typeclass<WResult>`, used for `duplicate`, `fmap`, and
  `extract` alike). This works because every comonad instance already in
  this codebase (`Identity`, `pair`'s env comonad, `Cofree`) has
  `extract`/`duplicate`/`fmap` declared generic over their *own*
  element-type template parameter (not fixed to the instance's keying
  type `A`) — a discovery S03/S07 already made and S12's handoff
  reconfirmed for `pair`. The practical upshot: `comonad_typeclass<WResult>
  .fmap(algebra, c(child))` (where `c(child)` is actually `W<F<WResult>>`,
  not `W<X>` for the instance's own keyed `X = Result`) just works, no
  second lookup keyed on anything else is ever needed. **This generalizes
  directly to S14's `monad_typeclass<MSeed>` lookup for `gana`** — expect
  the same one-lookup-suffices shape there (S08's `Free`/either's monad
  instances are almost certainly generic over their own element type the
  same way, though this step did not verify that explicitly; check it
  before assuming, but expect it to hold).
- **Only `cata_via_gcata` needs an algebra projection; `histo_via_gcata`/
  `zygo_via_gcata`/`para_via_gcata` do not** — this is the one place this
  step's result differs from what a naive reading of the S13 step file's
  own forward-note draft (in S12's handoff) suggested might be needed
  ("the analogous projection is extract/.second/.second respectively").
  Working through the actual types shows why: `gcata`'s algebra parameter
  has type `F<WResult> -> Result`. For **histo**, `WResult =
  Cofree<F,Result>`, and histo's own algebra (histo.hpp) is *already*
  exactly `F<Cofree<F,Result>> -> Result` — no unwrapping needed, pass it
  straight through. For **zygo**, `WResult = std::pair<Helper,Result>`,
  and zygo's own main algebra (zygo.hpp) is *already* exactly
  `F<std::pair<Helper,Result>> -> Result` — again pass straight through.
  For **para**, `WResult = std::pair<Fix<F>,Result>`, and para's own
  algebra (para.hpp) is *already* exactly
  `F<std::pair<Fix<F>,Result>> -> Result` — same story. Only **cata**
  differs: fold_fix's plain algebra is the *unwrapped* `F<Result> ->
  Result`, one layer thinner than `F<Identity<Result>> -> Result`, so
  `cata_via_gcata` is the only one of the four that needs
  `layer_fmap(.value)` composed in front. **Lesson for S14/S15**: don't
  assume every recovery function needs a projection-adapter by default —
  check whether the target scheme's own algebra/coalgebra shape already
  matches gcata's/gana's expected shape before writing one. For gana:
  the step file's own §7.10 recovery aliases bullet already says which
  ones need a seed-wrapping adapter (`ana_via_gana` needs
  `ψ' = layer_fmap(Identity-wrap) ∘ ψ`, by the mirror-image of the same
  reasoning as `cata_via_gcata` here) — expect `apo_via_gana`/
  `futu_via_gana` to need adapters too since, unlike histo/zygo/para
  above, `apo`'s/`futu`'s own coalgebra shapes are *not* already
  `Seed -> F<MSeed>` verbatim (apo's coalgebra returns
  `F<either<Fix<F>,Seed>>`, futu's returns `F<Free<F,Seed>>` roughly —
  check each against gana's expected `Seed -> F<MSeed>` shape rather than
  assuming either direction).
- **The worker struct pattern from S12's `dist_histo_t`/`dist_futu_t`
  ports directly to a two-level generalized scheme with zero surprises**:
  namespace-scope struct, `const Dist&`/`const GAlgebra&` members, an
  explicit trailing return type on `operator()` computed from the
  template parameters alone (not from the call's own deduced result), and
  `(*this)(...)` for the recursive call. No GCC 16 diagnostic was hit on
  the first attempt using this shape — the "explicit trailing return
  type" fix from S07/S08 continues to be exactly sufficient for every
  self-recursive-through-a-lambda case in this codebase, gcata included.
- **`gcata`'s own `F` deduces normally from the `Fix<F>` tree argument**,
  with no template-template-parameter deduction issue at all — because
  `Fix<F>` (fix.hpp) is a *real* class template recording `F` directly
  (S12's handoff explains why this differs from `wrap_fix<F>`'s bare
  `F<Fix<F>>` parameter, which does need `F` explicit). `gcata` itself
  therefore needs no explicit `F` at any call site; only `Result` and
  `WResult` are ever passed explicitly, exactly matching the step file's
  own pinned spelling `gcata<int, Identity<int>>(dist_cata, φ', tree)`.
  The *distributive law* passed in as `dist` may itself need `F` bound
  explicitly at its own call site (`dist_histo<F>`, `dist_para<F>`) per
  S12's DEV-02 — but that's `F` being bound on the `Dist` argument
  *before* it's passed to `gcata`, not on `gcata`'s own template
  parameter list.

## Forward notes for the NEXT step (S14 — gana + ghylo + recovery laws)

- **`generalized.hpp` is the file to extend** — add `gana`/`ghylo`
  alongside `gcata` in the same header (per the step file's own
  numbering, `generalized.hpp` is explicitly S13/S14/S15's shared file).
  `generalized.t.cpp` stays gcata-only; the step file asks for separate
  `gana.t.cpp`/`ghylo.t.cpp` — that means **three new test files this
  time**, not one: update `CMakeLists.txt`'s FILE_SET (just the header,
  unchanged) and test-sources list with `gana.t.cpp` and `ghylo.t.cpp`
  (two entries, not one).
- **Mirror `gcata_worker_t`'s exact shape for `gana`'s worker `a`**: a
  namespace-scope struct `gana_worker_t<F, MSeed, Dist, GCoalgebra>` (or
  similar — Result doesn't appear on the ana side, note the parameter
  list is shorter) holding `const Dist&`/`const GCoalgebra&`, computing
  its own analogous "up front" typedefs from `MSeed`/`F`/`Dist` alone,
  `operator()` with an explicit trailing return type, recursing via
  `(*this)(...)`. The step file's own equation:
  `a(m) = wrap_fix(layer_fmap(λmms. a(fmapM(ψ, join(mms))), dist(m)))`
  with entry `gana = a(pure(ψ(seed)))`. Work out the up-front-computable
  return type the same way this step did for `WWR`/`C` before writing the
  recursive body — that discipline (compute the type from the template
  parameters alone, name it, then write the body) is exactly why this
  step compiled clean on the first try; don't skip it and try to let
  `auto` deduce through the recursion, it won't work (S07/S08/S12/this
  step's shared discovery).
- **Expect a single `monad_typeclass<MSeed>` lookup to suffice**, exactly
  as this step found for `comonad_typeclass<WResult>` (see Discoveries
  above) — but verify it: check that `Identity`/`Free<F,·>`/`either<L,·>`
  (whichever monads gana's recoveries need — `ana_via_gana` needs
  `Identity`, `futu_via_gana` needs `Free`, `apo_via_gana` needs `either`)
  each have `bind`/`pure`/`join` generic over their own element-type
  parameter, the same way every comonad instance in this codebase turned
  out to be. `join` is available two ways: the `Monad` CRTP's own derived
  `join` (`monad.hpp`'s `Monad<Impl>::join`, `= bind(mma, id)`) reachable
  through the looked-up instance object directly (e.g.
  `monad_typeclass<MSeed>.join(mms)`), or the free function
  `smd::typeclass::join(mms)` (`monad.hpp`, does the same lookup
  internally) — either spelling works; picking the instance-object one
  keeps everything keyed through the same single lookup variable, mirror
  this step's single-lookup pattern.
- **Which projections `ana_via_gana`/`apo_via_gana`/`futu_via_gana`
  actually need is NOT symmetric with gcata's recoveries** — do not
  assume "only the Identity one needs an adapter" carries over
  unchanged; check each of `apo`'s and `futu`'s own coalgebra shapes
  (`apo.hpp`, `futu.hpp`) against gana's expected `Seed -> F<MSeed>`
  before writing wrappers. This step's own experience (only cata needed
  a projection, the other three didn't) came from checking the actual
  types, not from a rule of thumb — do the same check for gana rather
  than porting this step's specific finding.
- **The S04 sorted-insert fixture (`apo_via_gana`'s target) and the S08
  RLE fixture (`futu_via_gana`'s target)** are the ones named in the step
  file — go find them in `apo.t.cpp`/`futu.t.cpp` rather than inventing
  new ones, matching this step's own approach of reusing S04/S05/S07's
  exact fixtures for gcata's recoveries.
- **`ghylo`'s "first cut" (materializing: `gcata` applied to `gana`'s
  output) is explicitly sanctioned by the step file** as an acceptable
  first attempt, fusing only if straightforward — this step's `gcata`
  signature (`gcata<Result, WResult, F, Dist, GAlgebra>`) and this
  forward note's sketch of `gana`'s signature are both now landed and
  should compose directly for the materializing version; try that first
  before attempting fusion.
- **Direct-call pinning test**: the step file explicitly asks to keep a
  `gana<IntListF, either<Fix<IntListF>, int>>(...)` (or similar) direct
  call with explicit template arguments in the tests, mirroring this
  step's own `gcata<int, Identity<int>>(dist_cata, ...)` test — don't
  skip it, it's the interface-pinning test the whole chain relies on.
- Gate remains `make TOOLCHAIN=gcc-16 test` fully green; S14 adds no new
  example binary (design §10's table's first generalized-scheme example,
  `generalized_tour`, is S15's, per S13's own step file note carried
  forward unchanged).

## Open risks / TODOs

- gcc-17 secondary smoke was not re-run this step (advisory only per
  design D9/plan); still outstanding since S00, not blocking.
- This step's tests exercise gcata's four recoveries individually but do
  not exercise `gcata` with a hand-rolled *novel* comonad/distributive
  law pair beyond the four design already names — not required by the
  step file (which only asks for the named four plus one direct-call
  pin), so not a gap, just noting the scope for whoever eventually writes
  design-doc-facing documentation (S16).
- None specific to S13's own scope were left open — gate is green, every
  bullet in the step file's "Do"/"Tests" sections has corresponding code
  or a test, and the direct-call pinning test uses the exact spelling
  the step file itself requested.
