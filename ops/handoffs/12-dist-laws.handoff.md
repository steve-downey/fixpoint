# Handoff — S12 Distributive laws

- **Status:** DONE (gate passed)
- **Commit:** 73daaba — `[schemes] S12: distributive laws`
- **Date / agent:** 2026-07-04, background execution agent

## What changed

- New `src/smd/fixpoint/dist_laws.hpp` (namespace `smd::fixpoint`): the
  eight distributive-law objects design §7.9 asks for, each a polymorphic
  function object with its Haskell type in a comment:
  - `dist_cata_t`/`inline constexpr dist_cata_t dist_cata{}` — `F<Identity<A>>
    -> Identity<F<A>>`, verbatim from the design's own given snippet.
    Call: `dist_cata(layer)` — no explicit template arguments ever needed.
  - `dist_ana_t`/`dist_ana` — `Identity<F<A>> -> F<Identity<A>>`. Call:
    `dist_ana(ident)` — same, no explicit args.
  - `dist_histo_t<F>`/`template<F> inline constexpr dist_histo_t<F>
    dist_histo<F>` — `F<Cofree<F,A>> -> Cofree<F, F<A>>`. **Is a variable
    *template* on F, not a bare singleton** — see Discoveries. Call:
    `dist_histo<F>(layer)` (F explicit, A deduced).
  - `dist_futu_t<F>`/`dist_futu<F>` — `Free<F, F<A>> -> F<Free<F, A>>`. Same
    F-explicit shape as dist_histo. Call: `dist_futu<F>(chunk)`.
  - `dist_zygo_t<HelperAlg>`/`dist_zygo(helper) -> dist_zygo_t<HelperAlg>`
    (factory) — `F<pair<B,X>> -> pair<B, F<X>>` (B/X extracted via
    `.first`/`.second`, no F-naming or X-naming needed anywhere). Call:
    `auto law = dist_zygo(helper_algebra); law(layer)`.
  - `dist_para_t<F>`/`dist_para<F>` — `F<pair<Fix<F>,X>> -> pair<Fix<F>,
    F<X>>`, "its own small object" per the step file rather than literally
    `dist_zygo(wrap_fix)` (wrap_fix always needs F explicit — same reason
    dist_histo/dist_futu do, see Discoveries). Call: `dist_para<F>(layer)`.
  - `dist_apo_t`/`dist_apo` — `either<Fix<F>,F<X>> -> F<either<Fix<F>,X>>`.
    **Its `operator()` takes an explicit leading `X`** (L, R deduced from
    the `either`) — see Discoveries. Call: `dist_apo.operator()<X>(e)` (or
    `dist_apo.template operator()<X>(e)` inside a dependent context).
  - `dist_gapo_t<Coalg>`/`dist_gapo(coalg) -> dist_gapo_t<Coalg>` (factory)
    — `(b -> F<b>) -> either<b,F<X>> -> F<either<b,X>>`, generalizes
    dist_apo with `coalg` in place of `unwrap_fix`. Same explicit-X call
    convention: `dist_gapo(coalg).operator()<X>(e)`.
- `src/smd/fixpoint/CMakeLists.txt`: append-only (`dist_laws.hpp` /
  `dist_laws.t.cpp` to the FILE_SET/test sources), same two-file pattern
  every prior step touched. No example added (S12 has no entry in design
  §10's examples table — confirmed before skipping it).
- New `src/smd/fixpoint/dist_laws.t.cpp` (12 tests): shape/value round-trips
  for each law on NatF/IntListF (Zero/Succ, Nil/Cons alternatives); a
  two-level Nat `Cofree` history exercising dist_histo's real recursion;
  Pure and Roll chunks exercising dist_futu; dist_apo on Left, Right, and
  the `either<Fix<F>, F<Fix<F>>>`-shaped (Seed = Fix<F>) case per the step
  file; a dist_gapo sanity check; naturality spot-checks for dist_cata and
  dist_histo (`dist(layer_fmap(fmap_W(f), l)) == fmap_W(layer_fmap(f))
  (dist(l))`, both computed via the actual objects, not hand-derived
  constants); `HeaderIsIdempotent`; two `static_assert`s (dist_cata,
  dist_zygo outputs).

## Verification evidence

- `make TOOLCHAIN=gcc-16 test`: **100% tests passed, 0 tests failed out of
  176** (baseline going into this step was **164**, S11's row; net +12,
  matching the 12 new `TEST_CASE`s in `dist_laws.t.cpp` exactly). All 164
  pre-existing tests still pass unchanged.
- Explicit rebuild of the touched/new files (`touch` + `make
  TOOLCHAIN=gcc-16 compile`) with `grep -i warning` over the output: empty
  (the only match is the unrelated `uv`/`VIRTUAL_ENV` venv notice, not a
  compiler warning — same false positive every prior handoff has noted).
- Both `static_assert`s (`dist_cata_constexpr_smoke`,
  `dist_zygo_constexpr_smoke`) compile clean under gcc-16/C++26.
- **Mutation testing (per the task's DEV-01 guidance)**: two deliberate,
  plausible bugs were introduced, confirmed to break a test, then reverted
  and confirmed exact via re-compile + full gate re-run:
  1. Swapped `.first`/`.second` in `dist_zygo_t::operator()` (a classic
     "forgot the helper-first/main-second convention" bug). Caught two
     ways: a hard compile error on the mixed-type (`int`/`char`) runtime
     fixture (type mismatch feeding `sum_helper`), and a `static_assert`
     failure on the same-type (`int`/`int`) constexpr fixture (wrong
     numeric answer, 10 instead of 3) — proving the constexpr fixture
     alone discriminates the bug even when the types happen to coincide.
  2. Swapped `make_left`→`make_right` in `dist_apo_t`'s Left branch (a
     "forgot D4's Left=stop convention" bug). Caught immediately as a hard
     compile error (`std::visit` "requires the visitor to have the same
     return type for all alternatives" — the Left branch's mutated output
     type no longer matched the Right branch's).
  Both reverted; `git status`/re-diff and a full green gate re-run (176/176
  again) confirmed the revert was exact before the step commit.

## Deviations from the plan / design

One row added to `ops/DEVIATIONS.md` (**DEV-02**): design §4 describes
distributive laws as uniform polymorphic function objects ("for every X"),
matching dist_cata's own given code exactly — no explicit template
arguments implied at any call site. In practice only `dist_cata`,
`dist_ana`, and `dist_zygo(helper)` achieve that ideal; `dist_histo`,
`dist_futu`, and `dist_para` must be variable templates parameterized on
`F` (called as `dist_histo<F>(...)` etc.), and `dist_apo`/`dist_gapo(coalg)`
must take an explicit leading `X` (called as `dist_apo.operator()<X>(...)`)
— both are genuine C++ template-deduction limits (template-template
parameter deduction from an already-elaborated alias-template application
fails; `make_left`'s result-side type parameter is never deducible from its
argument), not implementation mistakes. See the ledger row for the full
before/after and the recommended doc wording. The underlying *equations*
(D8) are unchanged — only the *call convention* differs from what §4's
prose implies.

## Discoveries affecting later steps

- **Template-template parameter `F` cannot be deduced from an
  already-elaborated alias-template application, ever, in this codebase.**
  This is the single biggest discovery of this step, and it is *not* new —
  it is exactly why `wrap_fix<F>` (fix.hpp, S00) always takes `F` explicit
  at every call site in every prior scheme (`apo.hpp`, `futu.hpp`,
  `functors.hpp`), while `unwrap_fix` never does (its parameter type is the
  *real* class template `Fix<F>`, which records `F` directly; `wrap_fix`'s
  parameter type is a *bare* `F<Fix<F>>`, which does not). This step is the
  first to hit the *same* limit for schemes whose argument type is a bare
  `F<X>` with no enclosing named struct (`dist_histo`'s `F<Cofree<F,A>>`,
  `dist_futu`'s `Free<F, F<A>>` — the *inner* `F<A>` is the problem even
  though the outer `Free<F,·>` deduces fine, since `Free` is a real class
  template). **Confirmed empirically**: writing
  `template <template<class> class F, class A> operator()(const
  F<Cofree<F,A>>&)` and calling it on a concrete `NatF<Cofree<NatF,int>>`
  fails outright ("is not derived from const F<Cofree<F,A>>"), for both
  dist_histo and dist_futu. **The fix, generalized from dist_para's own
  step-file guidance ("implement as its own small object so the layer type
  works out")**: make the whole object a variable *template* on `F` (a
  struct `dist_histo_t<F>` wrapping the templated `operator()<A>`, exposed
  as `template<F> inline constexpr dist_histo_t<F> dist_histo<F>`); once
  `F` is fixed at the *struct* level (an ordinary, non-deduced template
  parameter supplied by the caller), the parameter pattern `F<Cofree<F,A>>`
  becomes a concrete, already-expanded alias with only `A` left to deduce
  — which works completely normally, the same way `dist_para_t<F>`'s `X`
  already did. **Any future step (S13/S14/S15) writing a NEW distributive
  law or generalized-scheme helper whose argument type contains a bare
  `F<something>` (not wrapped in a named struct recording F) will hit this
  immediately** — reach for "variable template on F" from the start, don't
  discover it via a compile error.
- **`make_left`'s result-side type parameter is never deducible — this
  bites any code building an `either` value inside a generic `fmap`/`match`
  context, not just S03's original observation.** `dist_apo`/`dist_gapo`
  both need it (see the header's own `dist_apo_t` doc comment for the full
  reasoning: the "X" needed for the Left branch's `make_left<X>` doesn't
  appear anywhere in the Left branch's own data — `unfix(t)`/`coalg(b)`
  only ever mention the *other* side's type). The fix: give the whole
  `operator()` an explicit leading template parameter for exactly that
  missing type (`template <class X, class L, class R>
  operator()(const either<L,R>&)`), with L/R still deducing normally from
  the argument (the either's own two type parameters, which — unlike a bare
  `F<X>` — `either` *does* record directly, being a real class template).
  This composes fine with the "F-explicit" pattern above if a future law
  needs both simultaneously (none of this step's laws did).
- **Self-recursive-through-a-lambda still needs the explicit-trailing-
  return-type fix (S07/S08's discovery), and `(*this)(...)` is the
  cleanest way to spell the recursive call** — used for both
  `dist_histo_t::operator()` (recurses into `c.tail`, same `F`/`A`
  instantiation every level) and `dist_futu_t::operator()`'s Roll branch
  (recurses into a child chunk, same `F`/`A` instantiation). Both compiled
  clean on the first attempt using `(*this)(...)` plus an explicit trailing
  return type computed from `F`/`A` alone — no GCC 16 diagnostic hit,
  unlike S07's first draft. Calling by name (`dist_histo(...)`) instead of
  `(*this)(...)` would also work (ADL finds the not-yet-fully-defined
  namespace-scope object at instantiation time, since the recursive call is
  dependent) but was not needed and was not tried; `(*this)` avoids relying
  on that reasoning at all.
- **`std::visit`'s hard requirement that every visitor branch return
  identically the same type** is the mechanism that makes the "X must be
  named explicitly" problem *fail loudly* (a hard compile error, not a
  silent wrong answer) whenever the mismatch is real (different types on
  each side) — confirmed directly via the mutation test above. It only
  produces a silently wrong *value* (not a compile error) when the two
  sides happen to share the same concrete type, which is exactly why the
  step's own `dist_zygo` mutation needed a same-type (`int`/`int`) fixture
  to catch as a *value* bug (the constexpr smoke test) as well as a
  mixed-type fixture to catch as a *type* bug (the runtime test) — both are
  now in `dist_laws.t.cpp`, not just one.
- **`pair`'s env-comonad instance (S03) already covers every `B`
  including `Fix<F>` and any zygo `Helper` type** — confirmed by using
  `std::pair<Fix<F>, X>`/`std::pair<int, char>` directly in this step's
  tests with zero new instance code needed; S13's own pitfall
  ("If WResult-driven lookup produces 'no comonad instance' for
  pair<Fix<F>, X>...") is answered: it already works, nothing to add.

## Forward notes for the NEXT step (S13 — gcata + recovery laws)

- **Exact call spellings to use inside `generalized.hpp`/`gcata.t.cpp`**
  (S13's own "Recovery aliases" bullet needs every one of these):
  - `cata_via_gcata`: pass `dist_cata` directly (no template args) —
    `gcata<Result, Identity<Result>>(dist_cata, algebra_prime, tree)`.
  - `histo_via_gcata`: pass `dist_histo<F>` (F explicit!) —
    `gcata<Result, Cofree<F,Result>>(dist_histo<F>, algebra_prime, tree)`.
    Forgetting the `<F>` will fail to compile with a deduction error citing
    `dist_histo_t<F>` — see Discoveries above for why.
  - `zygo_via_gcata`: pass `dist_zygo(helper_algebra)` (a factory *call*,
    producing a `dist_zygo_t<HelperAlg>` value) —
    `gcata<Result, std::pair<Helper,Result>>(dist_zygo(helper_algebra),
    algebra_prime, tree)`.
  - `para_via_gcata`: pass `dist_para<F>` (F explicit) —
    `gcata<Result, std::pair<Fix<F>,Result>>(dist_para<F>, algebra_prime,
    tree)`.
  - The step file's own direct-call test (`gcata<int,
    Identity<int>>(dist_cata, φ', tree)`) matches this exactly — `dist_cata`
    needs no template arguments, only `gcata` itself does (per D5).
- **`WResult` for each recovery, spelled out**: `Identity<Result>` (cata),
  `Cofree<F, Result>` (histo), `std::pair<Helper, Result>` (zygo — helper
  first, matching S05/dist_zygo's own convention), `std::pair<Fix<F>,
  Result>` (para — env slot is `Fix<F>`, matching dist_para's `B = Fix<F>`).
  All four already have `comonad_typeclass` instances (Identity/S03,
  Cofree/S07, pair/S03 — generic over its `B`) — no new instance code
  needed anywhere, confirmed by this step's own dist_zygo/dist_para tests
  already exercising `std::pair<int,char>`/`std::pair<Nat,char>` directly.
- **`algebra_prime` (the `φ'` wrapping mentioned in the step file) for each
  recovery**: for cata, `φ' = φ ∘ layer_fmap(.value)` (project each
  `Identity<Result>` child down to `Result` before calling the plain
  algebra) — this step's `dist_ana`/`dist_cata` tests already use exactly
  this `.value`-projection shape via `layer_fmap`, reuse it verbatim rather
  than reinventing. For histo/zygo/para, the analogous projection is
  `extract`/`.second`/`.second` respectively (this step's own naturality
  tests already build these projections inline — see
  `dist_laws.t.cpp`'s `layer_fmap([](const auto& c){ return extract(c); },
  ...)` and the `.first`/`.second` lambdas in `dist_zygo_t`/`dist_para_t`
  for the exact shape to copy).
- **If S13 needs a NEW distributive law or any generalized-scheme helper
  whose argument type is a bare `F<something>`** (not wrapped in a named
  struct recording `F`), apply the "variable template on `F`" fix
  immediately (see Discoveries) rather than discovering the deduction
  failure via a compile error — this is now a proven, repeatable pattern in
  this codebase (`dist_histo<F>`/`dist_futu<F>`/`dist_para<F>`), not a
  one-off.
- Gate remains `make TOOLCHAIN=gcc-16 test` fully green; S13 adds no new
  example binary (design §10's table has no entry for gcata alone — the
  first generalized-scheme example, `generalized_tour`, is S15's).

## Open risks / TODOs

- gcc-17 secondary smoke was not re-run this step (advisory only per
  design D9/plan); still outstanding since S00, not blocking.
- **This step's own tests are explicitly not the final correctness proof**
  for most of these laws (per the task brief and the step file's own
  Pitfalls) — S13/S14's recovery-law gates (`gcata(dist_cata) ≡ fold_fix`,
  `gcata(dist_histo) ≡ histo`, `gcata(dist_zygo(f)) ≡ zygo(f)`,
  `gcata(dist_para-equivalent) ≡ para`, and the full S14 gana/ghylo table)
  are the real proof. This step deliberately did not try to pre-empt that
  work (per the task's own instruction not to duplicate S13/S14's job).
- None specific to S12's own scope were left open — gate is green, every
  bullet in the step file's "Tests" section has a corresponding test, and
  the mutation-testing checks (dist_zygo, dist_apo) were run and reverted
  cleanly, not just asserted in prose.
