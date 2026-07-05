# Handoff — S08 Free monad and futu

- **Status:** DONE (gate passed)
- **Commit:** ee50780 — `[schemes] S08: Free + futu`
- **Date / agent:** 2026-07-04, background execution agent

## What changed

- New `src/smd/fixpoint/free.hpp` (namespace `smd::fixpoint` for the type +
  free helpers, reopens `namespace smd::typeclass` for the instances,
  exactly the house layout `cofree.hpp` uses): `Free<F, A>` per design
  §5.4 — `std::variant<A, F<Free<F,A>>> node` (Pure a | Roll layer) plus a
  defaulted `friend operator==`. Free helpers `pure_free(a) -> Free<F,A>`,
  `roll_free(layer) -> Free<F,A>`, `is_pure(f) -> bool`. Instances:
  - `functor_typeclass<Free<F,A>>` (`FreeFunctorImpl`/`Map`) — `fmap` maps
    the Pure value via `fn`, recurses through Roll layers via
    `layer_fmap`. Fixed to the class-level `A` (mirrors
    `CofreeFunctorImpl`'s reasoning: Functor's only derived op, `replace`,
    never re-wraps). **Explicit trailing return type**
    (`-> Free<F, remove_cvref_t<invoke_result_t<Fn, const A&>>>`), not
    deduced `auto` — same GCC 16 self-recursion-through-a-lambda reason as
    `CofreeFunctorImpl::fmap` (S07).
  - `monad_typeclass<Free<F,A>>` (`FreeMonadImpl`/`Map`) — `pure` =
    `pure_free`; `bind`: Pure a → `k(a)`; Roll layer →
    `roll_free(layer_fmap(recursive-bind-with-k, layer))`. **Both `pure`
    and `bind` are templated over their own element-type parameter (`X`),
    not the class-level `A`** — the S03/S07 handoffs' Comonad/Monad
    genericity discovery, needed here too since `Monad`'s derived
    `join`/`kleisli` (monad.hpp) require `bind` to consume the doubled
    `Free<F, Free<F,X>>`. `bind`'s trailing return type is explicit
    (`remove_cvref_t<invoke_result_t<Fn, const X&>>`, computed from
    `Fn`/`X` alone, never from `bind`'s own return type) — confirmed the
    S07 forward note's prediction exactly: without this, GCC 16 rejects
    the self-recursive call inside `layer_fmap`'s lambda with "use of ...
    before deduction of 'auto'".
- New `src/smd/fixpoint/futu.hpp`: `futu<F>(coalgebra, seed) -> Fix<F>`,
  a direct (not bind-based) transcription of design §7.5's equation
  (`futu ψ = fix ∘ fmapF(worker) ∘ ψ`). A free function template
  `futu_worker<F>(coalgebra, chunk)` (forward-declares/calls `futu`
  mutually) implements `worker(Pure s) = futu(coalgebra, s); worker(Roll
  layer) = fix(fmapF(worker, layer))`. **`futu`'s/`futu_worker`'s return
  type is always the concrete `Fix<F>` (never deduced from the
  coalgebra)**, so — unlike Free's own `bind` — these two do **not** hit
  the GCC 16 deduced-auto self-recursion quirk at all; the trailing return
  type is written explicitly only because every other scheme in this
  module does (house style), not as a workaround for a real cycle. No
  `detail` namespace used (no header in this module uses one; both are at
  `smd::fixpoint` scope, `futu_worker` documented as an implementation
  detail via its doc comment only).
- `src/smd/fixpoint/functors.hpp`: **`Nil<E>` and `Cons<E,A>` (ListF) both
  gained a defaulted `friend operator==`.** Discovered while compiling
  `free.t.cpp`'s monad-law tests (which compare whole `Free<IntListF,
  int>` values): `Free<F,A>`'s defaulted `==` needs
  `std::variant<A, F<Free<F,A>>>` comparable, which for `F = IntListF`
  needs both `Nil<int>` *and* `Cons<int, Free<IntListF,int>>` comparable —
  and **`Cons` never had an `operator==` at all** (unlike `Succ<A>`, which
  S07 already fixed for the same reason; `Cons` was the one alternative
  S07's forward note didn't call out, since it assumed only the *empty*
  alternatives were at risk — turns out a plain non-empty aggregate
  doesn't get one either, only `Zero`/`Nil` had been checked). Not filed
  as an `ops/DEVIATIONS.md` row, same reasoning as S07's analogous fix
  (completeness fix to a pre-existing type, not a design contradiction) —
  and per the S07-follow-up commit `7f28bd8`, no dangling ledger reference
  was added pointing at a nonexistent row.
- `src/smd/fixpoint/CMakeLists.txt` / `src/examples/CMakeLists.txt`:
  append-only additions (`free.hpp futu.hpp` / `free.t.cpp futu.t.cpp` to
  the FILE_SET/test sources; one executable+install block for
  `futu_rle_decode`), same two-file pattern every prior step touched.
- New `src/smd/fixpoint/free.t.cpp` (8 tests): `pure_free`/`roll_free`/
  `is_pure` shape smoke; structural-equality smoke; functor `fmap` (maps
  Pure value, recurses through Roll, Cons heads untouched); the three
  monad laws by example (left/right identity, associativity spot-check,
  `for` loops 0..3/0..5) on `Free<IntListF, int>`; bind sequencing through
  a hand-built one-layer Roll chunk with an explicit expected-structure
  comparison; `HeaderIsIdempotent`; a `free_constexpr_smoke()` static_assert
  (`pure` then `bind` on a small `Free<IntListF,int>`). A test-local
  `make_run(value, count, seed)` helper (not shipped in `free.hpp`, per the
  step's own pitfalls section) builds n-deep Free chunks by looping
  `roll_free(Cons{value, box(inner)})`.
- New `src/smd/fixpoint/futu.t.cpp` (4 tests): the §9 law (a futu whose
  coalgebra emits exactly one layer per step —
  `layer_fmap(pure_free, ψ(s))` — degenerates to `unfold_fix(ψ)`, compared
  via `nat_to_int` since `Fix<F>` has no `==`, Nat 0..10); RLE decode
  `{{2,7},{3,1}}` → `[7,7,1,1,1]` (the step file's own worked example, byte
  for byte); pairwise-swap-adjacent-elements behavior test that **also
  runs the DEV-01 sanity check inline**: the same `futu` engine fed a
  naive one-layer-per-step coalgebra (the `layer_fmap(pure_free, ...)`
  degeneracy shape) on the identical input reproduces the *unswapped*
  list, verifiably different from the genuinely-swapped result — proving
  the fixture actually discriminates multi- vs. single-layer emission
  rather than coincidentally landing on the same answer either way;
  `HeaderIsIdempotent`; a `futu_constexpr_smoke()` static_assert (decode a
  single `{2,7}`-shaped run). Reuses the same test-local `make_run`
  n-deep-chunk-building shape as `free.t.cpp` (duplicated, not shared —
  each `.t.cpp` keeps its own copy, matching the step file's "keep it out
  of the public header" guidance literally per test file).
- New `src/examples/futu_rle_decode.cpp`: decodes two RLE inputs
  (`{{2,7},{3,1}}` and `{{1,9},{4,0},{2,5}}`), printing the input pairs and
  decoded vectors, the multi-layer-per-step move commented inline (a
  `build_run_tail` helper mirroring the test files' `make_run`).

## Verification evidence

- `make TOOLCHAIN=gcc-16 test`: **100% tests passed, 0 tests failed out of
  145** (baseline going into this step was **133**, confirmed via `git log`
  — the one commit between S07's handoff and this step,
  `7f28bd8 [schemes] S07 follow-up: drop dangling DEVIATIONS.md reference
  in comment`, touched only a comment, no test-count change; net for S08
  is +12, matching the 8 + 4 new `TEST_CASE`s across
  `free.t.cpp`/`futu.t.cpp` exactly). All 133 pre-existing tests still
  pass unchanged.
- Explicit rebuild of the touched/new files (`touch` +
  `make TOOLCHAIN=gcc-16 compile`) with `grep -i warning` over the output:
  the only match is the unrelated `uv`/`VIRTUAL_ENV` venv notice on
  stderr, not a compiler warning (same false-positive `grep` noted in the
  S07 handoff).
- `.build/build-gcc-16/src/examples/Asan/futu_rle_decode` output (exit 0):
  ```
  input: [(2, 7), (3, 1)]
  decoded: [7, 7, 1, 1, 1]
  input: [(1, 9), (4, 0), (2, 5)]
  decoded: [9, 0, 0, 0, 0, 5, 5]
  ```
  both match hand-computed values (2 sevens + 3 ones; 1 nine + 4 zeros +
  2 fives).
- Both `static_assert`s (`free_constexpr_smoke`, `futu_constexpr_smoke`)
  compile clean under gcc-16/C++26.

## Deviations from the plan / design

No `ops/DEVIATIONS.md` row filed. The `Nil<E>`/`Cons<E,A>` defaulted `==`
addition (see above) is a completeness fix to a pre-existing type filling
a gap the design already assumed away (same precedent as S07's
`Zero`/`Succ<A>` fix), not a place reality contradicted a documented
decision.

## Discoveries affecting later steps

- **The GCC 16 deduced-auto self-recursion quirk (S07's Discoveries) hit
  `Free`'s `bind` exactly as predicted, but did *not* hit `futu`/
  `futu_worker` at all.** The distinguishing factor: `Cofree`'s `fmap` and
  `Free`'s `bind` are *member functions of a CRTP `Impl`* whose return
  type is genuinely deduced from `Fn`'s `invoke_result_t` — that's what
  creates the chicken-and-egg cycle GCC 16 rejects. `futu`/`futu_worker`
  are *free function templates* whose return type is always the concrete,
  non-deduced `Fix<F>` (exactly like every other scheme in this module —
  `apo`, `prepro`, `postpro`, `hoist` all already return `Fix<F>`/`Result`
  explicitly and recurse through lambdas with zero friction). **The
  lesson for S09 (`dyna`/`codyna`/`chrono`, all free function templates
  with an explicit `Result`/`Fix<F>` return type): you almost certainly
  will not need the explicit-trailing-return-type workaround at all** —
  it's specific to CRTP `Impl` methods with deduced return types
  (typeclass instances), not to scheme free functions in general. Only
  reach for it if a scheme body itself becomes a method on some `Impl`
  (none of S09's do, per its own step file).
- **`Cons<E,A>` (`functors.hpp`, S02) had no `operator==` either** — S07's
  forward note flagged `Nil<E>`/`Zero` (the *empty* alternatives) as the
  only things at risk, reasoning that non-empty aggregates like
  `Cons`/`Leaf`/`Node`/`Const`/`Add`/`Mul` were "already comparable, every
  field is either a plain comparable value or a `Box`". **That reasoning
  was wrong**: a plain struct never gets an implicit `operator==` in C++
  regardless of whether its fields are comparable — only an explicit
  `friend ... = default` (or hand-written) gives one. `Cons` simply hadn't
  been exercised through an aggregate whose own `==` needed it yet (Nat's
  `Succ<A>` had been, via `Cofree<NatF,A>` in S07; `Cons<E,A>` hadn't,
  until `Free<IntListF, A>` in this step). **Any future step comparing a
  `Free<F,A>`, `Cofree<F,A>`, or other aggregate built on `TreeF`'s
  `Leaf`/`Node` or `ExprF`'s `Const`/`Add`/`Mul` should not assume "it has
  a Box or a plain field, so it's fine" — check for an actual
  `operator==` (or add one, same one-line pattern) before relying on `==`
  on any base-functor-layer type not yet exercised this way.**
- **`layer_fmap` remains the right composition tool for both the
  CRTP-member-recursion case (`Free`'s `bind`) and the free-function
  mutual-recursion case (`futu`/`futu_worker`)** — no new technique
  needed, this is now confirmed across every shape S01–S08 have thrown at
  it.
- **`Free<F, A>`'s Pure-vs-Roll distinction is a real `std::variant`
  discriminant** (unlike `Cofree`, which always has both `head` and
  `tail` — no choice to make), so unlike `CofreeFunctorImpl::fmap` (no
  `std::visit` needed), `FreeFunctorImpl::fmap`/`FreeMonadImpl::bind` both
  need `std::visit(smd::fixpoint::overloaded{...}, node)` internally —
  `free.hpp` includes `overloaded.hpp` directly for this (the only new
  header-to-header include difference from `cofree.hpp`'s pattern).

## Forward notes for the NEXT step (S09 — dyna, codyna, chrono)

- **`pure_free<F>(seed)`'s call convention**: explicit `F` only, `A`
  deduced from the argument (by-value parameter — `pure_free<F>(seed)`
  deduces `A = remove_cvref_t<decltype(seed)>`). `codyna`/`chrono`'s
  `pure_free<F>(seed)` initial-seed construction (design §7.6) will look
  exactly like this step's own usage throughout `free.t.cpp`/`futu.t.cpp`.
- **`refold`'s lookup (fmap-free) overload already exists** at
  `src/smd/fixpoint/recursion_schemes.hpp` (S01, untouched by this step):
  `template <typename Result, template <typename> class F, typename
  Algebra, typename Coalgebra, typename Seed> constexpr auto refold(const
  Algebra&, const Coalgebra&, const Seed&) -> Result`. For `codyna`/
  `chrono`, `Seed` will deduce to `Free<F, S>` (S08's own type) with no
  special handling needed — `refold` doesn't care what `Seed` is beyond
  what the coalgebra/algebra need.
- **`unroll`'s exact shape, spelled out**: `unroll(chunk: Free<F,S>) ->
  F<Free<F,S>>` is `Pure s → ψ(s); Roll layer → layer` — note the Roll
  branch returns the *already-F<Free<F,S>>-shaped* `layer` **unchanged**,
  not re-wrapped or recursed into; `refold` itself supplies the recursion
  (via `layer_fmap` over `unroll`'s *result*, on each child `Free<F,S>`).
  This is a single non-recursive step function, structurally simple —
  do not confud it with `futu_worker` (which *does* recurse to fully
  resolve a chunk down to `Fix<F>`); `unroll` only ever peels one layer
  for `refold` to then recurse on.
- **On the step file's own suggestion of a header-internal `detail`
  namespace for `unroll`**: no header in this module (`cofree.hpp`,
  `free.hpp`, `futu.hpp`, or any S01–S07 scheme header) actually uses a
  `detail` namespace for internal helpers — they're all plain functions
  at `smd::fixpoint` scope (this step's `futu_worker` included, despite
  being just as much an "implementation detail" as `unroll` will be).
  Consider matching that established precedent (skip the `detail`
  namespace, just document `unroll` as internal via a comment) unless
  there's a concrete reason S09 needs actual visibility hiding — either
  way is a small, local decision, not a correctness question.
- **Watch for the same missing-`operator==` gap one more time** if any
  S09 law/behavior test wants structural equality on a `Cofree<F,
  Result>` or `Free<F, Seed>` built over a functor family not yet
  exercised this way (e.g. `IntTreeF`/`ExprF`) — see this step's
  Discoveries above: don't assume a non-empty aggregate is comparable
  without checking.
- Gate remains `make TOOLCHAIN=gcc-16 test` fully green; S09 additionally
  requires one new example binary (`dyna_fibonacci`) to run and exit 0
  with correct fib values (0..10), per its own step file.

## Open risks / TODOs

- gcc-17 secondary smoke was not re-run this step (advisory only per
  design D9/plan); still outstanding since S00, not blocking.
- None specific to S08's own scope were left open — gate is green, every
  bullet in the step file's "Tests"/"Example" sections has a
  corresponding test or example, and the example binary's output was run
  and pasted above (not just claimed). The DEV-01-flagged discriminating
  test (futu pairwise-swap) was empirically sanity-checked inline in
  `futu.t.cpp` itself (the naive one-layer coalgebra CHECK), not just
  asserted in prose.
