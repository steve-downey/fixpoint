# Handoff — S06 hoist + prepro + postpro

- **Status:** DONE (gate passed)
- **Commit:** b10a095 — `[schemes] S06: hoist + prepro + postpro`
- **Date / agent:** 2026-07-04, background execution agent

## What changed

- New `src/smd/fixpoint/prepro.hpp` (namespace `smd::fixpoint`), all three
  schemes in one header per the step file:
  - `template <template <class> class G, template <class> class F, class Nat>
    constexpr auto hoist(const Nat &e, const Fix<F> &tree) -> Fix<G>` —
    `fold_fix<Fix<G>>` (lookup-based overload) with algebra `wrap_fix<G> ∘
    e`. `G` is the first explicit template parameter, `F` is deduced from
    `tree`, so the endo case reads `hoist<F>(e, t)` exactly as the step file
    specifies.
  - `template <class Result, template <class> class F, class Nat, class
    Algebra> constexpr auto prepro(const Nat &e, const Algebra &algebra,
    const Fix<F> &tree) -> Result` — direct transcription of Fokkinga's
    equation: `layer_fmap` over `unwrap_fix(tree)`, and at every child
    position, `prepro<Result>(e, algebra, hoist<F>(e, child))` — the
    *whole child subtree* is hoisted (via a full `fold_fix`-based hoist)
    before recursing, not just the child's top layer.
  - `template <template <class> class F, class Nat, class Coalgebra, class
    Seed> constexpr auto postpro(const Nat &e, const Coalgebra &coalgebra,
    const Seed &seed) -> Fix<F>` — dual: `layer_fmap` over `coalgebra(seed)`,
    and at every child position, `hoist<F>(e, postpro<F>(e, coalgebra,
    child))` before wrapping.
  - Header comment documents the natural-transformation shape (templated
    call operator, callable on `F<X>` for every `X`) and the cumulative-cost
    note: a node at depth k has had `e` applied to it (via nested hoists) k
    times by the time prepro's algebra/postpro's grafting sees it — this is
    the literal (unfused) reading of Fokkinga's equation, not a
    memoized/optimized one.
- `src/smd/fixpoint/CMakeLists.txt`: added `prepro.hpp` to
  `smd_fixpoint_headers` FILE_SET; `prepro.t.cpp` to `smd_fixpoint_test`
  sources (append-only diff, same two lines every prior step touches).
- New `src/smd/fixpoint/prepro.t.cpp` (8 discovered tests +
  `HeaderIsIdempotent`): fixture natural transformations, each with a
  *templated* call operator (`template <class A> operator()(const
  NatF<A>&/IntListF<A>&) const -> ...`) — `identity_nat` (NatF endo),
  `nat_to_intlist_nat` (NatF -> IntListF, Zero→Nil/Succ(pred)→Cons(1,pred)),
  `take_while_positive_nat` (IntListF endo, Cons(x,·)→Nil once x<0),
  `cap_at_three_nat` (IntListF endo, Cons(x,·)→Cons(min(x,3),·)). Tests:
  hoist-identity reproduces the original tree (Nat 0..10, compared via
  `nat_to_int` since `Fix<F>` has no `operator==`); hoist NatF→IntListF then
  sum ≡ `nat_to_int` (0..10); prepro-with-identity ≡ `fold_fix` law (Nat
  0..10); postpro-with-identity ≡ `unfold_fix` law (0..10, compared via
  `nat_to_int` of both sides); take-while-positive sum behavior
  (`[3,4,-1,5] -> 7`) *and* the step file's requested `[3,-1,4]`-shaped case
  (`-> 3`) that specifically exercises the cumulative-hoist depth (see
  Discoveries below); postpro cap-at-three behavior below an unfold
  (`{1,5,2,9,3} -> {1,3,2,3,3}`); three constexpr `static_assert`s (hoist,
  prepro, postpro — the step file only asked for hoist+prepro, postpro's was
  added too since it compiled cleanly and D10 asks one per scheme).
- New `src/examples/prepro_takewhile_sum.cpp`: same
  `take_while_positive`/`sum_algebra` shape as the test, prints three sums
  (`[3,4,-1,5] -> 7`, `[3,-1,4] -> 3`, `[1,2,3] -> 6`) and exits 0.
- `src/examples/CMakeLists.txt`: added executable+install block for
  `prepro_takewhile_sum`, copied verbatim from the `mutu_even_odd` pattern.

## Verification evidence

- `make TOOLCHAIN=gcc-16 test`: configure+build+ctest clean — **100% tests
  passed, 0 tests failed out of 121** (up from S05's 113; net +8).
  `ctest -N` confirms exactly 8 new discovered tests (`#5` take-while
  [3,-1,4]->3, `#18` hoist NatF->IntListF, `#30` take-while [3,4,-1,5]->7,
  `#36` prepro HeaderIsIdempotent, `#40` postpro law, `#42` prepro law, `#45`
  hoist identity law, `#58` postpro cap behavior) — matching 121 − 113 = 8
  exactly. All 113 pre-existing tests still pass unchanged.
- Explicit rebuild of just `smd_fixpoint_test` and `prepro_takewhile_sum`
  (after `touch`ing the three new/changed source files) with
  `grep -i "warning\|error"` over the build output: empty (no compiler
  warnings).
- `.build/build-gcc-16/src/examples/Asan/prepro_takewhile_sum` prints:
  ```
  sum(take_while(>=0, [3, 4, -1, 5])) = 7
  sum(take_while(>=0, [3, -1, 4])) = 3
  sum(take_while(>=0, [1, 2, 3])) = 6
  ```
  and exits 0 — matches the hand-checked values.
- All three `static_assert`s (`hoist_constexpr_smoke`,
  `prepro_constexpr_smoke`, `postpro_constexpr_smoke`) compile clean under
  gcc-16/C++26.

## Deviations from the plan / design

None requiring an `ops/DEVIATIONS.md` row. The step file, design §7.4/§4/§9,
and every prior handoff (especially S05's forward notes for this step) were
followed as authoritative with no conflicts. One small addition beyond the
step's literal minimum: a third constexpr `static_assert` for postpro (the
step file's "Do" list only named hoist and prepro) — added because it
compiled cleanly and design D10 asks for one per scheme; not a deviation,
just slightly more coverage than the minimum.

## Discoveries affecting later steps

- **The `[3,-1,4]`-shaped take-while test does not, by itself, distinguish
  every possible wrong-depth transcription from the correct one** — I
  worked through several plausible "wrong" implementations by hand (e.g.
  hoisting only a child's immediate top layer instead of its whole subtree,
  or hoisting the entire original tree once up front instead of
  per-recursive-step) before writing the header, and for *this specific*
  take-while-positive transformation (a monotone single-cut rewrite) several
  of those wrong shapes happen to produce the same numeric answer as the
  correct cumulative version, because cutting a list at its first negative
  element is order-insensitive to how many times the cut gets re-evaluated.
  The `[3,-1,4]` case is still worth keeping (it's what the step file and
  design ask for, and it does catch the "forgot to hoist at all" class of
  bug loudly), but a *future* agent designing an equivalence test for a
  transformation that is genuinely non-idempotent under repeated application
  (e.g. one that increments a counter, or that behaves differently on an
  already-transformed layer than a fresh one) would be a strictly stronger
  discriminator of the cumulative-vs-single-pass distinction than take-while
  is. Nothing to fix here — the implementation is a literal transcription of
  Fokkinga's equation (verified by hand-tracing the `[3,-1,4]` case through
  the recursion, landing on 3 as documented) — just flagging that the
  "wrong version stops too early or too late" pitfall note is a weaker
  guarantee than it reads for this particular fixture.
- **`Fix<F>` has no `operator==`** (confirmed by grep — `fix.hpp` never
  defines one, and none of NatF/IntListF/IntTreeF/ExprF's containing `Fix`
  gets a defaulted comparison). Every "compare two `Fix<F>` trees" test in
  this step compares via a fold instead (`nat_to_int` for Nat,
  `list_to_vector` for IntList) rather than `==` — the step file's "or `==`
  if Fix layers compare" alternative doesn't actually apply here; use the
  fold-comparison idiom, don't try to add `operator==` to `Fix` itself
  (out of scope, not asked for, would touch a file every other module
  depends on).
- **`hoist`'s `G`-first explicit-template-parameter shape works cleanly with
  no special deduction tricks** — `hoist<F>(e, t)` (endo) and
  `hoist<IntListF>(nat_to_intlist_nat{}, nat)` (G != F) both compile and
  deduce `F` from the `Fix<F>` argument with zero ceremony; S05's forward
  note flagged this as untested territory (first scheme leading with a
  `template <class> class` parameter) and it turned out to be a non-issue.
- **Natural transformations as templated-call-operator structs (not
  lambdas) work exactly as design §4 prescribes**, including when the
  target functor differs from the source (`nat_to_intlist_nat`, `NatF<A> ->
  IntListF<A>` for every `A`). No gcc-16/C++26 friction.
- `layer_fmap` + `hoist` compose fine when `hoist`'s own algebra itself
  calls `layer_fmap` internally (via `fold_fix`) one recursion level below
  `prepro`/`postpro`'s own `layer_fmap` call — no aliasing/lifetime
  surprises, consistent with every prior step's "layer_fmap composes"
  discovery (S04, S05).

## Forward notes for the NEXT step (S07 — Cofree comonad and histo)

- **S07 depends on S02 and S03, not S06** — no file overlap with
  `prepro.hpp`/`prepro.t.cpp`. `src/smd/fixpoint/CMakeLists.txt` and
  `src/examples/CMakeLists.txt` are again the only two files this step
  touched that S07 will also touch (append-only diff, same pattern as every
  prior step: add `cofree.hpp histo.hpp` / `cofree.t.cpp histo.t.cpp` to the
  FILE_SET/test sources, add one executable+install block for
  `histo_coin_change`).
- **`Fix<F>` has no `operator==`** (see Discoveries above) — S07's step file
  asks for Cofree `==` laws (`extract(duplicate(c)) == c`, `fmap(extract,
  duplicate(c)) == c`) and says "needs Cofree ==". Since `Cofree<F,A>` is a
  *new* aggregate type (not `Fix`), adding a defaulted `friend constexpr auto
  operator==(const Cofree&, const Cofree&) = default;` to it directly is in
  scope and different from patching `Fix` — go ahead and default it on
  `Cofree` itself; it doesn't need to recurse through `Fix`'s absence of
  `==`, only through `F<Cofree<F,A>>` (a variant, which does support `==` if
  every alternative does — `Box` already provides `==`, and all of
  NatF/IntListF/IntTreeF/ExprF's alternatives are then comparable
  transitively) and `A` itself.
- **`layer_fmap` is the right tool for `duplicate`'s recursive-through-tail
  step** (`layer_fmap(duplicate, c.tail)` per the step file's own equation)
  — this module (`prepro.hpp`) confirms `layer_fmap` composes cleanly when
  used as the recursive step inside a hand-written traversal (not just
  inside `fold_fix`/`unfold_fix` themselves), which is exactly the shape
  `comonad_typeclass<Cofree<F,A>>::duplicate` needs.
- **Watch the `Box`-vs-value distinction when writing `Cofree<F,A>`'s
  `tail` member**: unlike `Fix<F>` (`F<Fix<F>>` inline, recursive positions
  Box'd *inside* each functor alternative), the step file's own aggregate
  shape (`{A head; F<Cofree<F,A>> tail;}`) puts `Cofree` itself as the
  functor's type parameter directly — the Box'ing still happens inside `F`'s
  alternatives (e.g. `Succ<Cofree<F,A>>{Box<Cofree<F,A>> pred}`), not on
  `tail` itself, so no new technique is needed there; this module didn't
  need to touch `Box` at all, so nothing to add beyond confirming the
  existing `Box::operator->()`/`operator*()` idiom (S02's discovery) is
  still the way to reach into a boxed recursive position.
- Gate remains `make TOOLCHAIN=gcc-16 test` fully green; S07 additionally
  requires one new example binary (`histo_coin_change`) to run and exit 0
  with counts matching hand-checked values (n=8 → 2, n=12 → 3 per the step
  file).

## Open risks / TODOs

- gcc-17 secondary smoke was not re-run this step (advisory only per design
  D9/plan); still outstanding since S00, not blocking.
- None specific to S06's own scope were left open — gate is green, every
  bullet in the step file's "Tests"/"Example" sections has a corresponding
  test or example, and the example binary was run and its output pasted
  above (not just claimed).
