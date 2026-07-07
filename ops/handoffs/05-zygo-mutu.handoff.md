# Handoff — S05 zygo + mutu

- **Status:** DONE (gate passed)
- **Commit:** acdf6b4 — `[schemes] S05: zygo + mutu`
- **Date / agent:** 2026-07-04, background execution agent

## What changed

- New `src/smd/fixpoint/zygo.hpp` (namespace `smd::fixpoint`):
  `template <class Result, class Helper, template <class> class F, class
  HelperAlg, class MainAlg> constexpr auto zygo(const HelperAlg &helper,
  const MainAlg &main, const Fix<F> &tree) -> Result`. Implemented as a
  single `fold_fix<std::pair<Helper, Result>>` (the lookup-based overload
  from `recursion_schemes.hpp`) with a combined algebra that (a) does a
  *second* `layer_fmap` projecting `.first` off the already-mapped
  `F<Carrier>` layer to build the `F<Helper>` the helper algebra expects,
  (b) calls `main` directly on the same `F<Carrier>` layer (its signature
  is already `F<std::pair<Helper,Result>> -> Result`, no projection
  needed), then returns `.second` of the whole fold. **Carrier is
  `std::pair<Helper, Result>` — helper `.first`, main `.second`**, per the
  step file and S04's forward note; verified against S12's `dist_zygo`
  is not yet written, so nothing to cross-check yet, but the convention is
  documented in the header comment for whoever writes it.
- New `src/smd/fixpoint/mutu.hpp`: `template <class A, class B, template
  <class> class F, class AlgA, class AlgB> constexpr auto mutu(const AlgA
  &alg_a, const AlgB &alg_b, const Fix<F> &tree) -> std::pair<A, B>`. One
  `fold_fix<std::pair<A,B>>` with combined algebra `{alg_a(layer),
  alg_b(layer)}` — both algebras see the *same* `F<std::pair<A,B>>` layer
  directly (no projection, unlike zygo — mutu's two algebras are peers,
  not helper/main). No `mutu_fst`/`mutu_snd` convenience projections added
  (step file said add only if tests want them; the tests destructure the
  pair with structured bindings instead, so skipped — minimal diff).
- `src/smd/fixpoint/CMakeLists.txt`: added `zygo.hpp mutu.hpp` to
  `smd_fixpoint_headers` FILE_SET; `zygo.t.cpp mutu.t.cpp` to
  `smd_fixpoint_test` sources.
- New `src/smd/fixpoint/zygo.t.cpp` (4 discovered tests +
  `HeaderIsIdempotent`): fold_fix-degeneracy law over Nat 0..10 (main
  algebra ignores the helper component); zygo_balanced behavior test
  (helper = height, main = balanced-check, on a perfectly-balanced
  `IntTree` and a deliberately-unbalanced one with a height-3 left spine
  next to a single leaf — top-level height diff 2); one constexpr
  `static_assert`.
- New `src/smd/fixpoint/mutu.t.cpp` (3 discovered tests +
  `HeaderIsIdempotent`): law test comparing `mutu(count_alg,
  double_count_alg, nat)` against a hand-paired `fold_fix<pair<int,int>>`
  over Nat 0..10 (drift guard, design §9); even/odd behavior test over Nat
  0..10 checked against `n % 2`; one constexpr `static_assert`. **Note:**
  `alg_even`/`alg_odd` are free functions marked `constexpr` (not
  lambdas) specifically because they're shared between the runtime test
  and the constexpr smoke test — a lambda would have needed to be
  `constexpr` too but keeping them as named functions reads more like the
  "alg_even says X iff pred is odd" prose in the step file.
- `src/examples/zygo_balanced.cpp` (new): same height/balanced algebras
  as the test, prints `balanced tree is balanced: true` /
  `unbalanced tree is balanced: false`.
- `src/examples/mutu_even_odd.cpp` (new): prints `{n}: even={} odd={}`
  for n = 0..10.
- `src/examples/CMakeLists.txt`: added executable+install blocks for
  `zygo_balanced` and `mutu_even_odd`, copied verbatim from the
  `apo_sorted_insert` pattern.

## Verification evidence

- `make TOOLCHAIN=gcc-16 test`: configure+build+ctest clean — **100%
  tests passed, 0 tests failed out of 113** (up from S04's 107; net +6).
  `ctest -N` confirms exactly 6 new discovered tests: `#8` mutu
  HeaderIsIdempotent, `#16` mutu law (hand-paired fold_fix), `#22` zygo
  behavior (zygo_balanced), `#28` zygo law (degenerates to fold_fix),
  `#31` mutu behavior (even/odd), `#52` zygo HeaderIsIdempotent — matching
  113 − 107 = 6 exactly. All 107 pre-existing tests still pass unchanged.
- Explicit rebuild of `smd_fixpoint_test`, `zygo_balanced`, and
  `mutu_even_odd` with `grep -i "warning\|error"` over the build output:
  empty (no compiler warnings).
- `.build/build-gcc-16/src/examples/Asan/zygo_balanced` prints:
  ```
  balanced tree is balanced: true
  unbalanced tree is balanced: false
  ```
  and exits 0.
- `.build/build-gcc-16/src/examples/Asan/mutu_even_odd` prints
  `{n}: even={} odd={}` for n = 0..10, all correct against `n % 2`, and
  exits 0.
- Both `static_assert`s (`zygo_constexpr_smoke`, `mutu_constexpr_smoke`)
  compile clean under gcc-16/C++26.

## Deviations from the plan / design

None. No row added to `ops/DEVIATIONS.md`. The step file, S02's handoff,
and S04's handoff (the pair-convention gotcha in particular) were
followed as authoritative with no conflicts. The "double fmap" pitfall
the step file warned about is exactly what `zygo.hpp`'s combined algebra
does (a second `layer_fmap` over the already-`fold_fix`-mapped
`F<Carrier>` layer to project `.first`) — implemented as described, no
attempt made to fuse it away.

## Discoveries affecting later steps

- **zygo's combined algebra needs no separate "first fmap" step** — unlike
  the Haskell equation `f(fmapF(first, x))` which reads as two composed
  operations, in this implementation the *single* `fold_fix` call already
  produces `F<Carrier>` at the algebra boundary (via its own internal
  `layer_fmap` recursing into children), so the combined algebra only adds
  *one* extra `layer_fmap` (projecting `.first`), not two. Worth knowing
  for anyone reading the header comment expecting to count fmaps against
  the Haskell equation literally.
- **mutu's two algebras are structurally peers, not helper/main** — both
  `alg_a` and `alg_b` receive the exact same `F<std::pair<A,B>>` layer with
  no projection in between (confirmed by `mutu.hpp`'s combined algebra
  being a one-liner `{alg_a(layer), alg_b(layer)}`). This is a simpler
  shape than zygo's and later steps should not assume mutu needs anything
  like zygo's helper-projection machinery.
- **Free (non-lambda) `constexpr` functions are the right call when an
  algebra is shared between a runtime Catch2 test and a `static_assert`
  smoke test** — `mutu.t.cpp`'s `alg_even`/`alg_odd` are ordinary
  `constexpr auto foo(...) -> bool` functions in an anonymous namespace,
  reused by both `TEST_CASE` and the `static_assert`, rather than being
  redefined as a second copy of local lambdas inside the constexpr
  function (contrast with `para.t.cpp`/`zygo.t.cpp`'s pattern of
  redeclaring the same lambda body twice — either idiom works, named
  `constexpr` functions read slightly better when three separate laws all
  reuse the exact same algebra).
- Confirmed again (as S04 predicted) that `Box<std::pair<Helper,
  Result>>`/`Box<std::pair<A,B>>` compose with the same `->first`/
  `->second` idiom used throughout — no new technique needed for either
  header.

## Forward notes for the NEXT step (S06 — hoist + prepro + postpro)

- **S06 depends only on S02**, not S05 — no file overlap with
  `zygo.hpp`/`mutu.hpp`/their tests. `src/smd/fixpoint/CMakeLists.txt` and
  `src/examples/CMakeLists.txt` are again the only two files this step
  touched that S06 will also touch (append-only diff, same pattern as
  every prior step).
- **New header is `src/smd/fixpoint/prepro.hpp`** containing `hoist`,
  `prepro`, and `postpro` together (not split into separate files) per
  the step file — plan for one FILE_SET/test-source entry, not three.
- **Natural transformations must have templated call operators**, per the
  step file's own pitfall note — this codebase's existing convention for
  "polymorphic function object callable on `F<X>` for every `X`" hasn't
  been exercised yet in any of S01–S05 (`layer_fmap`, `zygo`, `mutu` all
  take *algebras* keyed to one concrete `Result`/`Helper` type, not
  natural transformations); S06 is the first step that needs a
  `template <class A> constexpr F<A> operator()(const F<A>&) const`-style
  functor object, e.g. `struct identity_nat { template <class A> constexpr
  auto operator()(const NatF<A> &layer) const -> NatF<A> { return layer;
  } };` — matches the step file's request for an `identity_nat` spelling
  S15 will reuse. Design §4 has a code-comment example (`struct
  cap_layer`) to copy the shape from; make sure it's a real templated
  struct, not a generic lambda (generic lambdas work as call operators too,
  but the step file's design-§4 example is a named struct, and S15 will
  look for that name).
- **`hoist<G>`'s first explicit template parameter is the *target*
  functor `G`**, distinct from every scheme so far (`para<Result>`,
  `zygo<Result, Helper>`, `mutu<A, B>` all lead with *value* carriers, not
  a functor template) — `F` must still be deducible from the `Fix<F>`
  argument for the endo case `hoist<F>(e, t)` to read naturally. No
  existing scheme in this tree leads its explicit template parameter list
  with a `template <class> class` parameter, so there's no precedent to
  copy from `para.hpp`/`zygo.hpp` here; the step file's own signature
  sketch is the source of truth.
- **`prepro`'s cumulative-hoist cost**: this step (S05) did not exercise
  anything like it — `zygo`/`mutu` are both single-pass folds with no
  repeated whole-subtree transformation. S06's step file explicitly asks
  for a header-comment note on the cumulative cost and a test case
  ([3,-1,4]-shaped) that would catch a wrong-depth transcription; there is
  no shortcut from this step's work to reuse there.
- Gate remains `make TOOLCHAIN=gcc-16 test` fully green; S06 additionally
  requires one new example binary (`prepro_takewhile_sum`) to run and
  exit 0 with correct printed sums.

## Open risks / TODOs

- gcc-17 secondary smoke was not re-run this step (advisory only per
  design D9/plan); still outstanding since S00, not blocking.
- zygo's header comment notes the convention should match S12's
  `dist_zygo` but S12 doesn't exist yet — nothing to verify against now;
  flagged for whoever writes S12/S13 to check.
- None specific to S05's own scope were left open — gate is green, every
  bullet in the step file's "Tests"/"Examples" sections has a
  corresponding test or example, and both example binaries were run and
  their output pasted above (not just claimed).
