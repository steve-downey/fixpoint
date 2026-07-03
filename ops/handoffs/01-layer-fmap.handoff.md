# Handoff — S01 layer_fmap + typeclass-lookup scheme overloads

- **Status:** DONE (gate passed)
- **Commit:** 7ceaff9 — `[schemes] S01: layer_fmap + lookup overloads`
- **Date / agent:** 2026-07-03, background execution agent

## What changed
- New `src/smd/fixpoint/fmap.hpp`: `layer_fmap(fn, layer)`, keyed on
  `std::remove_cvref_t<Layer>`, looks up
  `smd::typeclass::functor_typeclass<...>` and calls its `.fmap`.
  Includes `<smd/typeclass/functor.hpp>`.
- `src/smd/fixpoint/recursion_schemes.hpp`: the three existing
  explicit-fmap overloads (`fold_fix`, `unfold_fix`, `refold`, plus the
  deprecated `cata` shim which forwards to `fold_fix`) are unchanged in
  behavior but now `constexpr`. Added three new lookup-based overloads
  that drop the `fmap_fn` parameter and use `layer_fmap` internally:
  `fold_fix<Result>(algebra, tree)`, `unfold_fix<F>(coalgebra, seed)`,
  `refold<Result, F>(algebra, coalgebra, seed)`. Arity differs from the
  explicit-fmap overloads (2/2/3 args vs 3/3/4), so overload resolution
  is unambiguous — no ADL/SFINAE tricks needed.
- `src/smd/fixpoint/CMakeLists.txt`: added `fmap.hpp` to
  `smd_fixpoint_headers` FILE_SET, `fmap.t.cpp` to `smd_fixpoint_test`
  sources. No new CMake target — both `smd::typeclass` and
  `smd::fixpoint` headers already live on the same `fixpoint.fixpoint`
  FILE_SET target, so the cross-module include just worked.
- New `src/smd/fixpoint/fmap.t.cpp`: local NatF (Zero/Succ) type plus a
  `functor_typeclass<NatF<A>>` instance (`NatFFunctorImpl`/
  `NatFFunctorMap`), reopening `namespace smd::typeclass` verbatim per
  design §6.1. Tests: lookup `fold_fix<int>` on a hand-built tree;
  lookup `unfold_fix<NatF>` + fold round-trip; lookup `refold` checked
  against the explicit-fmap `refold` for n in 0..10; a `constexpr`
  helper `count_via_lookup` exercised both at runtime and via
  `static_assert`.
- `src/smd/fixpoint/recursion_schemes.t.cpp` was **not** touched (its
  local NatF copy and explicit-fmap tests are untouched, per the
  step's "leave existing overloads/tests alone" rule).

## Verification evidence
- `make TOOLCHAIN=gcc-16 test`: configure+build+ctest clean —
  **100% tests passed, 0 tests failed out of 50** (up from the S00
  baseline of 45; net +5 new tests, all in `fmap.t.cpp`: "fmap -
  HeaderIsIdempotent", "fmap - LookupFoldFixNatTwo", "fmap -
  LookupUnfoldFixNatFive", "fmap -
  LookupRefoldMatchesExplicitFmapRefold", "fmap -
  LookupFoldFixIsConstexpr"). All 45 pre-existing tests still pass
  unchanged.
- `static_assert(count_via_lookup(2) == 2);` in `fmap.t.cpp` compiles —
  the lookup `fold_fix` path (tree construction via `wrap_fix`/
  `make_box` + `layer_fmap` + `functor_typeclass<NatF<int>>::fmap` +
  `std::visit`) is fully constexpr-capable under gcc-16/C++26, as-is,
  no workarounds needed.
- `functor_typeclass<NatF<A>>` specialization compiles from outside
  `namespace smd::typeclass` by reopening it in the test TU (validates
  the D2 cross-module pattern) — see "Discoveries" below for the one
  ordering gotcha this surfaced.
- Deprecated `cata` shim (`recursion_schemes.hpp`) still compiles
  unchanged; it forwards to the explicit-fmap `fold_fix<Result>` 3-arg
  overload and arity keeps it unambiguous against the new 2-arg lookup
  overload.

## Deviations from the plan / design
None against the step file or design doc text. No row added to
`ops/DEVIATIONS.md`. One implementation detail not spelled out by the
step (see Discoveries) required reordering code within the test file,
but the resulting code matches design §4's stated convention exactly
("keep instance definitions in the same header as the functor types,
above any scheme calls in the TU" — from the step's own pitfalls,
which anticipated this).

## Discoveries affecting later steps
- **Ordering is load-bearing, and the failure mode is a hard error, not
  a silent wrong-answer.** Defining the `functor_typeclass<NatF<A>>`
  partial specialization *textually after* a plain (non-template)
  function that calls the lookup `fold_fix` (even though that function
  is only *invoked* later, from a `TEST_CASE`) fails to compile:
  `error: 'const struct std::integral_constant<bool, false>' has no
  member named 'fmap'` followed by `partial specialization ... after
  instantiation ... [-fpermissive]`. This is because the point of
  instantiation for a template used inside an ordinary (non-template)
  function is essentially immediate (right after that function's
  definition), not deferred to end-of-TU. Fix: put the
  `functor_typeclass<NatF<A>>` specialization block immediately after
  the `NatF`/`Zero`/`Succ` type definitions and *before* any function —
  even a `constexpr` helper never directly called yet — that invokes
  `fold_fix`/`unfold_fix`/`refold` (lookup overloads) or `layer_fmap`
  for that layer. In `fmap.t.cpp` the final layout is: (1) anonymous
  namespace with `Zero`/`Succ`/`NatF`/`Nat`; (2) `namespace
  smd::typeclass { ... }` reopened with the instance; (3) a *second*
  anonymous namespace block with `make_zero`/`make_succ`/
  `count_algebra`/`nat_coalgebra`/`count_via_lookup`; (4) `TEST_CASE`s.
  **This directly matters for S02**: `functors.hpp` will define each
  functor's node types, its `functor_typeclass` instance, *and* smart
  constructors/converters (`nat_to_int`, `eval`, etc.) that call the
  lookup `fold_fix` — all in one header. Order matters *within that
  header*: put each functor's `functor_typeclass<...>` specialization
  right after its node/layer type definitions and before any
  function in the same header that folds/unfolds/refolds over it
  (i.e. before `nat_to_int`/`nat_from_int`, before `eval`, etc.).
- `layer_fmap`'s landed signature: `template <class Fn, class Layer>
  constexpr auto layer_fmap(Fn&& fn, const Layer& layer)` in
  `smd::fixpoint`, keyed via
  `smd::typeclass::functor_typeclass<std::remove_cvref_t<Layer>>`. The
  `std::remove_cvref_t` is applied explicitly even though `Layer`
  deduced from a `const Layer&` parameter is already unqualified — kept
  for parity with D2's literal wording and as defense if a future
  caller passes `decltype(layer)` with qualifiers through a forwarding
  path.
- Marking the three classic schemes (`fold_fix`, `unfold_fix`, `refold`)
  `constexpr` under gcc-16/C++26 required no other changes — it "just
  worked" (no `Box`/`std::visit`/lambda constexpr blockers hit). The
  deprecated `cata` wrapper was left non-`constexpr` since the step
  only named the three classic schemes, not `cata`; it still compiles
  fine as-is (it's a thin forwarding call).
- Reopening `namespace smd::typeclass` from a `smd::fixpoint` test TU
  to add a layer-keyed specialization works with no include-order
  surprises beyond the instantiation-point issue above — `functor.hpp`
  only needed to be included (transitively via `fmap.hpp`) before the
  specialization.

## Forward notes for the NEXT step (S02 — functors.hpp base-functor library)
- Read the ordering discovery above first — it is the main risk for
  S02, since `functors.hpp` will pack node types + functor_typeclass
  instance + fold/unfold-based smart constructors (`nat_to_int`,
  `nat_from_int`, `eval`, `list_to_vector`, `list_from_vector`) into one
  header. Sequence: types → `functor_typeclass` specialization (reopen
  `namespace smd::typeclass`) → smart constructors/converters that call
  `fold_fix`/`unfold_fix`/`layer_fmap`. Do this per-functor (NatF's
  instance before NatF's converters, then ListF's types+instance before
  ListF's converters, etc.) rather than grouping all instances at the
  end of the header.
- Use the **lookup** overloads (`fold_fix<Result>(alg, tree)`,
  `unfold_fix<F>(coalg, seed)`) landed in this step for `nat_to_int`,
  `nat_from_int`, `eval`, list/tree converters — that's the whole point
  of S01 existing before S02. Don't hand-roll `fmap_nat`-style explicit
  fmap helpers in `functors.hpp`; call `layer_fmap` (from
  `<smd/fixpoint/fmap.hpp>`, already a transitive include once you
  include `<smd/fixpoint/recursion_schemes.hpp>`) only if you need fmap
  directly outside a scheme call.
- `src/smd/fixpoint/recursion_schemes.t.cpp` still has its own private
  `NatF`/`Zero`/`Succ`/`fmap_nat` — S02's step file explicitly says not
  to touch it and to move the *functors.hpp* NatF in
  independently. `fmap.t.cpp` (this step) also has its own private
  copy for the same reason (design D2 pattern proof, kept minimal and
  independent of functors.hpp which doesn't exist yet). Expect **three**
  independent `NatF`/`Zero`/`Succ` definitions in the tree after S02
  (recursion_schemes.t.cpp, fmap.t.cpp, functors.hpp) — this is
  intentional per each step's file, not drift to clean up.
- `functor_typeclass<NatF<A>>`'s shape to reuse/match: `NatFFunctorImpl<A>`
  (has the `fmap(this auto&&, Fn&&, const NatF<A>&)` member),
  `NatFFunctorMap<A> : Functor<NatFFunctorImpl<A>>`, then
  `template <class A> inline constexpr auto functor_typeclass<NatF<A>> =
  NatFFunctorMap<A>{};` — copy this pattern for `ListF<E,A>` (partial
  specialization over both `E` and `A`, per D3) and `TreeF`/`ExprF`.
- `ExprF`'s node structs in the current
  `src/examples/fixpoint_tree_example.cpp` (to be replaced by
  `functors.hpp` include per S02's step 4) — read that file before
  writing `functors.hpp`'s ExprF section to match its `Const`/`Add`/
  `Mul` shapes and the printed `Result: 10` expectation.
- Gate remains `make TOOLCHAIN=gcc-16 test` fully green; S02 also
  requires running
  `.build/build-gcc-16/src/examples/Asan/fixpoint_tree_example` and
  confirming it prints `Result: 10`.

## Open risks / TODOs
- gcc-17 was not re-tested in this step (advisory only per plan/D9);
  still outstanding from S00's handoff, not blocking.
- None specific to S01's own scope were left open — gate is green, all
  "Verify" bullets from the step file were satisfied.
