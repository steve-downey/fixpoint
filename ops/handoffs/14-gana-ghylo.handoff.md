# Handoff — S14 gana + ghylo + recovery laws

- **Status:** DONE (gate passed)
- **Commit:** 0b6affe — `[schemes] S14: gana + ghylo + recovery laws`
- **Date / agent:** 2026-07-04, background execution agent

## What changed

- `src/smd/fixpoint/generalized.hpp` extended (S13's header, per its own
  forward note) with:
  - `gana_worker_t<F, MSeed, Dist, GCoalgebra>` — implementation detail,
    mirrors `gcata_worker_t`'s namespace-scope-struct/explicit-trailing-
    return-type/`(*this)(...)`-recursion shape exactly, but simpler: `a`'s
    domain type `M<F<MSeed>>` is the *same* at every recursive depth (no
    growing-type bookkeeping, unlike gcata's tree descent), so only two
    types are computed up front, both *forward* through
    `monad_typeclass<MSeed>.pure` (never by unwrapping backward): `MFMS =
    M<F<MSeed>>` (`pure(declval<F<MSeed>>())`, the worker's own
    parameter/return-domain type) and `MMS = M<MSeed>` (`pure(declval
    <MSeed>())`, `dist(m)`'s element type). `operator()(const MFMS&) const
    -> Fix<F>` body: `wrap_fix<F>(layer_fmap([this](const MMS& mms) ->
    Fix<F> { auto joined = monad_typeclass<MSeed>.join(mms); auto next =
    layer_fmap(coalgebra, joined); return (*this)(next); }, dist(m)))` —
    the literal `a(m) = fix(fmapF(λmms. a(fmapM(ψ,join(mms))), k(m)))`
    from design §7.10, with `fmapM(ψ, ·)` spelled as the plain
    `layer_fmap(coalgebra, ·)` helper (every monad instance here —
    Identity/Free/either — is already a Functor instance too, S03/S07/S08,
    and each one's `fmap` is fixed to the *same* class-level element type
    `join` just produced, so no separate lookup is needed).
  - `gana<F, MSeed, Dist, GCoalgebra, Seed>(dist, coalgebra, seed) ->
    Fix<F>`: builds one `gana_worker_t` and returns
    `worker(monad_typeclass<MSeed>.pure(coalgebra(seed)))` — literally
    `a(pure(ψ(seed)))`.
  - `ana_via_gana<F>(coalgebra, seed)`, `apo_via_gana<F>`,
    `futu_via_gana<F>` — recovery wrappers. Only `ana_via_gana` needs a
    coalgebra-wrapping adapter (`ψ' = layer_fmap(Identity-wrap) ∘ ψ`,
    `MSeed = Identity<Seed>`); `apo_via_gana` (`MSeed =
    either<Fix<F>,Seed>`) and `futu_via_gana` (`MSeed = Free<F,Seed>`) pass
    their scheme's own coalgebra straight through unchanged, since apo's
    own coalgebra shape (`Seed -> F<either<Fix<F>,Seed>>`) and futu's own
    (`Seed -> F<Free<F,Seed>>`) already *are* gana's expected `Seed ->
    F<MSeed>` shape verbatim — this is the asymmetry the S13 handoff
    flagged as needing verification rather than assumption, confirmed here
    by checking each scheme's actual coalgebra type, not by a rule of
    thumb. `apo_via_gana` additionally needs a thin lambda binding
    `dist_apo.template operator()<MSeed>` once (DEV-02, S12) since gana's
    worker only ever calls `dist` at the single fixed `X = MSeed`
    instantiation every recursive step (mirroring `gcata_worker_t`'s own
    single fixed instantiation) — no growing-X wrapper is needed.
  - `ghylo<Result, WResult, F, MSeed, WDist, GAlgebra, MDist, GCoalgebra,
    Seed>(w_dist, algebra, m_dist, coalgebra, seed) -> Result`: shipped
    **materializing** (`gcata<Result,WResult>(w_dist, algebra,
    gana<F,MSeed>(m_dist, coalgebra, seed))`) — this passed every recovery
    law cleanly on the first attempt, so no fusion attempt was made (the
    step file explicitly sanctions this as an acceptable first cut). No
    named `_via_ghylo` recovery aliases were added (the step file only
    asks for the four ghylo(...) recovery *tests*, not named wrapper
    functions); ghylo.t.cpp builds the necessary
    algebra_prime/coalgebra_prime adapters inline, reusing the exact
    shapes `cata_via_gcata`/`ana_via_gana` already use internally.
- `src/smd/fixpoint/dist_laws.hpp` — **DEV-03 fix**: `dist_ana_t`'s
  wrapping lambda now writes `Identity<std::remove_cvref_t<decltype(x)>>
  {x}` instead of `Identity{x}` (CTAD). See Deviations below.
- `src/smd/typeclass/monad.hpp` — **DEV-04 fix**: `Monad<Impl>`'s four
  CRTP-derived operations (`apply`, `join`, `kleisli`, `bind_with`) and the
  two free functions (`mbind`, `join`) are now all marked `constexpr`. See
  Deviations below.
- `src/smd/fixpoint/CMakeLists.txt`: append-only (`gana.t.cpp`,
  `ghylo.t.cpp` added to the test-sources list; `generalized.hpp` was
  already in the FILE_SET from S13, unchanged).
- New `src/smd/fixpoint/gana.t.cpp` (7 tests): `ana_via_gana` ≡
  `unfold_fix` on Nat, 0..10; `apo_via_gana` ≡ `apo` on the S04
  sorted-insert fixture (reused verbatim, apo.t.cpp's own anonymous
  namespace); **a second `apo_via_gana` ≡ `apo` test with Left/Right as
  genuinely different types** (Seed = int, graft side = IntList) —
  added after mutation testing showed the sorted-insert fixture alone
  (Seed = IntList = Fix<IntListF>, so Left and Right coincide) does not
  discriminate a flipped D4 orientation (see Verification evidence);
  `futu_via_gana` ≡ `futu` on the S08 RLE fixture (reused verbatim,
  futu.t.cpp's own anonymous namespace); one direct `gana<IntListF,
  either<Fix<IntListF>, int>>(...)` call pinning the public spelling (per
  the step file's own request); one constexpr `static_assert` on
  `ana_via_gana`; `HeaderIsIdempotent`.
- New `src/smd/fixpoint/ghylo.t.cpp` (6 tests): the four §9 recovery laws
  (`ghylo(dist_cata,dist_ana)` ≡ `refold`, `ghylo(dist_histo,dist_ana)` ≡
  `dyna` fib, `ghylo(dist_cata,dist_futu)` ≡ `codyna`,
  `ghylo(dist_histo,dist_futu)` ≡ `chrono` fib), all 0..10 on Nat, reusing
  chrono.t.cpp's exact fixture shapes (`countdown`, `one_layer_countdown`,
  `plain_count_algebra`, `fib_algebra`); one constexpr `static_assert` on
  `ghylo` itself (design D10 — the step file only explicitly named
  ana_via_gana's smoke test, but ghylo is itself a listed scheme per D1,
  so it gets its own); `HeaderIsIdempotent`.

## Verification evidence

- `make TOOLCHAIN=gcc-16 test`: **100% tests passed, 0 tests failed out of
  194** (baseline going into this step was **183**, S13's row; net +11 —
  10 originally-planned tests + 1 added after the mutation-testing
  discovery below — matching `ctest -N | grep -iE "gana|ghylo"`'s count
  exactly). All 183 pre-existing tests still pass unchanged.
- Explicit rebuild after `touch`ing every changed/new file (`make
  TOOLCHAIN=gcc-16 compile`) with `grep -i warning` over the output
  (excluding the unrelated `VIRTUAL_ENV` venv notice every prior handoff
  has also seen): empty — no compiler warnings.
- Both constexpr `static_assert`s (`ana_via_gana_constexpr_smoke`,
  `ghylo_constexpr_smoke`) compile clean under gcc-16/C++26 — this is what
  originally surfaced DEV-04 (the build failed with "call to non-constexpr
  function ... join" before that fix).
- **Mutation testing (per the task's DEV-01 precedent)**: flipped
  `apo_via_gana`'s `MSeed = either<Fix<F>,Seed>` to `either<Seed,Fix<F>>`
  (a deliberate, plausible "swapped D4 orientation" bug — exactly the risk
  the step file's Pitfalls section named). Result: the existing
  sorted-insert law test (Seed = IntList = Fix<IntListF>, so Left and
  Right are the *same* type) **still compiled and passed unchanged** —
  confirmed this is a genuine same-type blind spot, the same class of gap
  S12's handoff found for `dist_zygo` (a same-type fixture can't
  distinguish a swap when both sides coincide). Added the second
  `apo_via_gana` test above (Seed = int, graft side = IntList — genuinely
  different types) specifically to close this gap; re-ran the same
  mutation against it and got a **hard compile error**
  (`gana_worker_t::operator()`: "no known conversion... MFMS" — the
  coalgebra's own `either<Fix<F>,Seed>` no longer matched the flipped
  `MSeed`), confirming the new fixture does discriminate the orientation.
  Reverted the mutation (`diff` against a pre-mutation backup confirmed an
  exact match) and re-ran the full gate (194/194) before committing.

## Deviations from the plan / design

Two rows added to `ops/DEVIATIONS.md`:

- **DEV-03** (`dist_laws.hpp`): `dist_ana_t`'s wrapping lambda wrote
  `Identity{x}` and relied on CTAD. This silently collapses to a *copy*
  (via CTAD's implicit copy-deduction candidate, [over.match.class.deduct]
  — the same rule that makes `vector{v}` for `v : vector<int>` copy rather
  than nest) instead of wrapping one level deeper, whenever `x` is itself
  already an `Identity<T>` — exactly `ana_via_gana`'s own use (`gana<F,
  Identity<Seed>>(dist_ana, ...)`). Caught as a hard compile error (not a
  silent wrong answer): `gana_worker_t`'s type arithmetic requires
  `dist_ana(m)`'s element type to be `Identity<Identity<Seed>>`, and the
  collapse produced `Identity<Seed>` instead, which then failed
  `functor_typeclass` instantiation. Fixed by naming the wrapped type
  explicitly (`Identity<std::remove_cvref_t<decltype(x)>>{x}`);
  `dist_laws.t.cpp`'s own tests (which only ever exercise `dist_ana` at
  plain, non-Identity element types) still pass unchanged.
- **DEV-04** (`monad.hpp`): `Monad<Impl>`'s four CRTP-derived operations
  (`apply`/`join`/`kleisli`/`bind_with`) and the two free functions
  (`mbind`/`join`) were never marked `constexpr`, even though every
  `Impl::bind`/`Impl::pure` they forward to already was — a pre-existing
  gap (D10 predates S14; monad.hpp is S03/S08-vintage) that simply had no
  caller reaching it in a constant expression before this step's
  `ana_via_gana_constexpr_smoke`/`ghylo_constexpr_smoke`, both of which use
  `monad_typeclass<MSeed>.join(...)` inside `gana_worker_t`. Fixed by
  adding `constexpr` to all six (purely additive, no signature/behavior
  change — verified via the full 194-test gate).

## Discoveries affecting later steps

- **The single-`monad_typeclass`/single-`comonad_typeclass`-lookup pattern
  (S13's own discovery) is now confirmed to hold on the gana side too**:
  one `monad_typeclass<MSeed>` lookup suffices for `pure` (top-level entry)
  and `join` (inside the worker) alike, at every recursive depth, for all
  three monads this step exercises (Identity, `either`, Free) — matching
  S13's prediction exactly. The mechanism differs slightly per monad:
  `IdentityMonadImpl`'s `pure`/`bind` are fully generic over their own
  element-type parameter (nothing fixed beyond the instance's own `A`,
  which only keys the lookup); `FreeMonadImpl<F,A>`'s `pure`/`bind` are
  generic over element type but fix `F`; `EitherMonadImpl<L,R>`'s `pure`
  fixes `L` (not generic — `make_right<L>(value)`, `L` is the class-level
  type) while `bind` is generic over the incoming Right type. All three
  shapes compose correctly with `gana`'s recursion because the *type being
  looked up on* (`MSeed`) is always the once-and-for-all-fixed carrier —
  never re-derived mid-recursion.
- **`layer_fmap` (the `functor_typeclass`-based helper, not a monadic
  operation) is the correct spelling of `fmapM(ψ, ·)`** in the gana
  equation — not `bind`/`apply`. This works because Identity/Free/either
  all already have `functor_typeclass` instances (needed elsewhere in this
  codebase regardless of gana), and each one's `fmap` is fixed to exactly
  the class-level element type that `join` just produced (confirmed: no
  case in this step needed a *generic* `fmap` the way `bind`/`pure`
  sometimes need to be).
- **CTAD's implicit copy-deduction candidate is a general hazard for any
  generic scheme body that constructs a wrapper type from an unconstrained
  element via braced-init CTAD** (DEV-03) — not specific to `dist_ana`.
  Any future header (S15's `gprepro`/`gpostpro`/`zygo_histo_prepro`
  included) that writes `SomeWrapper{x}` inside a template body where `x`'s
  type is a deduced/generic parameter should name the wrapped type
  explicitly if there is *any* chance `x` could itself already be a
  `SomeWrapper<T>` at some instantiation (composed-comonad/composed-monad
  contexts — exactly what S15's `pair<Helper, Cofree<F,X>>` composed
  comonad is about — are prime candidates). Grep for bare `Identity{`,
  `Cofree{`, etc. constructions inside generic bodies before assuming
  they're safe.
- **Typeclass CRTP base classes' derived operations need `constexpr`
  marked explicitly and proactively, not reactively** (DEV-04) — the gap
  sat unnoticed since S03/S08 because no `static_assert` had reached
  `join`/`apply`/`kleisli`/`bind_with` in a constant expression until this
  step. If S15 adds any derived operation to `Comonad`'s or `Applicative`'s
  CRTP base (unlikely per its own step file, but worth checking), mark it
  `constexpr` at declaration time rather than waiting for a compile
  failure to reveal the gap.
- **A same-type Left/Right (or same-type Helper/Result, etc.) fixture is a
  blind spot for orientation-flip bugs specifically, even when it's a
  perfectly good behavioral/value-correctness test otherwise** — this
  step's own mutation-testing experience (the S04 sorted-insert fixture,
  Seed = IntList = Fix<IntListF>) is now a second data point alongside
  S12's `dist_zygo` finding. When a step's Pitfalls section names an
  orientation-flip risk specifically (as S14's did for
  apo_via_gana/dist_apo), budget time to check whether the law test's own
  fixture actually has visibly different types on both sides of the sum
  in question — if not, add a second fixture that does, the way this
  step's handoff does above.

## Forward notes for the NEXT step (S15 — gprepro/gpostpro/zygo_histo_prepro capstone)

- **`generalized.hpp` is again the file to extend** (S13/S14/S15's shared
  header per its own top-of-file doc comment, now updated to say so
  through S14) — add `gprepro`/`gpostpro`/`zygo_histo_prepro` alongside
  `gcata`/`gana`/`ghylo`. New test file per the step file: `gprepro.t.cpp`
  (one file for all three, per its own "Tests" bullet — unlike S14's
  split into `gana.t.cpp`/`ghylo.t.cpp`, S15 asks for a single test file).
  Update `CMakeLists.txt`'s test-sources list with just that one new
  entry; the FILE_SET entry for `generalized.hpp` is already present,
  unchanged.
- **`gprepro`'s worker is `gcata_worker_t`'s shape with `hoist<F>(e)`
  spliced in before the recursive call** — re-read `gcata_worker_t` in
  generalized.hpp (this step's own code, unchanged) as the base to modify:
  its `operator()` currently does `dist(layer_fmap([this](child){ ... duplicate(fmap(algebra, (*this)(child))) ...}, unwrap_fix(t)))`; per the step file, gprepro's analogous worker should apply
  the natural transformation `e` to the unfixed layer before recursing —
  compare against `prepro.hpp` (S06) for the exact "where does `hoist`
  go" shape (prepro's own body is the model the step file explicitly
  points at: "compare §7.4's prepro vs fold_fix — same delta").
- **`gpostpro` is dual against `gana_worker_t`** (this step's code) the
  same way — the mirror-image splice, this time on the unfold side. The
  single-lookup-suffices pattern (`monad_typeclass<MSeed>`,
  `comonad_typeclass<WResult>`) this step and S13 both reconfirmed should
  carry over unchanged; no reason to expect otherwise, but per this step's
  own repeated lesson ("check, don't assume symmetry"), verify rather than
  presume.
- **Watch for DEV-03's CTAD hazard directly**: `zygo_histo_prepro`'s
  composed comonad `pair<Helper, Cofree<F,X>>` is exactly the kind of
  "wrapper containing another instance of a wrapper-shaped thing" context
  where a generic `Identity{x}`/`pair{x}`/`Cofree{x}`-via-CTAD
  construction could silently collapse or misdeduce if `x` happens to
  already carry a matching shape — grep any new construction sites in
  `generalized.hpp` for bare CTAD braced-init before trusting them, per
  this step's Discoveries above.
- **The composed-comonad question (step 2 of S15's own "Do" list) is
  explicitly flagged by that step's own file as "thin ice"** — whether
  `comonad_typeclass<std::pair<B,A>>` with `A = Cofree<F,X>` (S03's
  pair-env comonad, unchanged since S12 confirmed it's generic over `B`)
  already gives the right *composed* `duplicate` for gprepro's needs, or
  whether a dedicated instance keyed on `pair<Helper, Cofree<F,X>>` is
  needed. This step's own `gana`/`ghylo` work did not touch comonad
  composition at all (gana is monad-only, ghylo materializes rather than
  fusing), so it offers no direct evidence either way — S15's own
  step file already correctly flags this as needing empirical
  verification via the degeneracy law, not assumption.
- **If S15's own §7.11 equations don't type-check as transcribed, the
  step file already anticipates this ("this step has the highest chance
  of a genuine design error") and pre-authorizes budgeting accordingly** —
  get gprepro/gpostpro's own laws green before attempting the capstone,
  and if the capstone blocks, ship gprepro/gpostpro alone with a BLOCKED
  note on the capstone specifically (S16 does not depend on it, per the
  step file).
- Gate remains `make TOOLCHAIN=gcc-16 test` fully green; S15 additionally
  requires the new `generalized_tour` example binary (§10) to run and
  exit 0, with every printed "specialized vs generalized" pair matching —
  this is the first example binary any of S12/S13/S14 has produced (all
  three were library-only steps).

## Open risks / TODOs

- gcc-17 secondary smoke was not re-run this step (advisory only per
  design D9/plan); still outstanding since S00, not blocking.
- `ghylo` shipped materializing (gcata-after-gana), not fused through a
  single combined refold pass — explicitly sanctioned by the step file as
  an acceptable first cut, and not revisited since it passed every
  recovery law cleanly. A future step (not currently planned in
  ops/PLAN.md) could attempt fusion if performance ever becomes a design
  goal (currently a non-goal, design §11/D6).
- DEV-03/DEV-04 are both narrowly-scoped, additive fixes verified not to
  affect any pre-existing test — but neither `dist_laws.t.cpp` nor
  `monad.t.cpp` (both pre-existing, S12/S03-vintage test files) were
  extended with a regression test for the specific scenarios these fixes
  address (Identity-of-Identity via `dist_ana`; `join`/`apply` in a
  constant expression for `either`/`Free`). This step's own new tests
  (`gana.t.cpp`'s `ana_via_gana_constexpr_smoke`, `ghylo.t.cpp`'s
  `ghylo_constexpr_smoke`) do exercise both fixes end-to-end, so the gate
  does cover them, but a future documentation pass (S16) could consider
  whether either deserves a narrower, standalone regression test closer
  to its own header.
- None specific to S14's own scope were left open — gate is green, every
  bullet in the step file's "Do"/"Tests" sections has corresponding code
  or a test, the direct-call pinning test uses the exact spelling the
  step file itself requested, and the mutation-testing check was actually
  run (not just asserted in prose), including the follow-up fixture added
  after the first mutation attempt exposed a blind spot.
