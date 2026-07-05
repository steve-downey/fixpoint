# Handoff — S02 functors.hpp: the reusable base-functor library

- **Status:** DONE (gate passed)
- **Commit:** b6619a8 — `[schemes] S02: functors.hpp base-functor library`
- **Date / agent:** 2026-07-03, background execution agent

## What changed
- New `src/smd/fixpoint/functors.hpp` (namespace `smd::fixpoint`, reopens
  `namespace smd::typeclass` once per functor family), laid out per
  family as types → `functor_typeclass` instance → smart
  constructors/converters (S01 handoff's ordering discovery, applied
  four times):
  - **NatF** — `struct Zero {};` `template <class A> struct Succ { Box<A>
    pred; };` `template <class A> using NatF = std::variant<Zero,
    Succ<A>>;` `using Nat = Fix<NatF>;`. `functor_typeclass<NatF<A>>`
    (`NatFFunctorImpl`/`NatFFunctorMap`, same shape as design §6.1).
    `make_zero()`, `make_succ(Nat)`, `nat_from_int(int) -> Nat` (ana via
    lookup `unfold_fix<NatF>`), `nat_to_int(const Nat&) -> int` (cata via
    lookup `fold_fix<int>`).
  - **ListF** — `template <class E> struct Nil {};` `template <class E,
    class A> struct Cons { E head; Box<A> tail; };` `template <class E,
    class A> using ListF = std::variant<Nil<E>, Cons<E, A>>;`
    `template <class A> using IntListF = ListF<int, A>;` `using IntList
    = Fix<IntListF>;`. `functor_typeclass<ListF<E, A>>` partially
    specialized over **both** `E` and `A`
    (`ListFFunctorImpl<E,A>`/`ListFFunctorMap<E,A>`) — `Nil<E>` maps to
    itself unchanged (E untouched), `Cons<E,A>` maps `.tail`'s boxed `A`
    only. `list_from_vector(const std::vector<int>&) -> IntList`:
    coalgebra is a lambda capturing the vector **by reference**, seed =
    `std::size_t` index (avoids inventing a seed struct; simpler than a
    vector-slicing seed). `list_to_vector(const IntList&) ->
    std::vector<int>`: algebra pattern-matches Nil/Cons and grows the
    result via `Box::operator->()`.
  - **TreeF** — `template <class E> struct Leaf { E value; };`
    `template <class A> struct Node { Box<A> left; Box<A> right; };`
    `template <class E, class A> using TreeF = std::variant<Leaf<E>,
    Node<A>>;` (payloadless internal nodes, per the step's pitfall
    steer). `template <class A> using IntTreeF = TreeF<int, A>;` `using
    IntTree = Fix<IntTreeF>;`. `functor_typeclass<TreeF<E,A>>` partial
    spec over `<E,A>` — `Leaf<E>` unchanged, `Node<A>` maps both boxed
    children. `make_leaf(int) -> IntTree`, `make_node(IntTree, IntTree)
    -> IntTree`. **No public fold/eval shipped for TreeF** — the step
    only asked for constructors; a `sum_leaves` cata lives
    test-locally in `functors.t.cpp` to exercise the instance (see
    Discoveries/forward notes: S05's `zygo_balanced` will need its own
    height/balance algebras against this same `IntTree`/`IntTreeF`).
  - **ExprF** — `Const<A>{int val;}`, `Add<A>{Box<A> left,right;}`,
    `Mul<A>{Box<A> left,right;}`, `ExprF<A> = std::variant<Const<A>,
    Add<A>, Mul<A>>`, `Expr = Fix<ExprF>` — field names/shape copied
    verbatim from the pre-existing `fixpoint_tree_example.cpp`.
    `functor_typeclass<ExprF<A>>` (unary, `Const` unaffected, `Add`/`Mul`
    map both children). `const_node(int)`, `add_node(Expr,Expr)`,
    `mul_node(Expr,Expr)`, `eval(const Expr&) -> int` (cata via lookup
    `fold_fix<int>`).
- `src/smd/fixpoint/CMakeLists.txt`: added `functors.hpp` to
  `smd_fixpoint_headers` FILE_SET, `functors.t.cpp` to
  `smd_fixpoint_test` sources.
- New `src/smd/fixpoint/functors.t.cpp`: round-trips
  (`nat_to_int(nat_from_int(n)) == n` for 0..10; vector↔list for a
  populated and an empty vector; `eval` on the `(2*3)+4` tree == 10);
  `layer_fmap` smoke tests per functor family, each covering every
  variant alternative; one `static_assert` per functor family (NatF,
  ListF, TreeF via the local `sum_leaves` helper, ExprF); a
  `ListFmapStringPayloadIsGeneric` test using `ListF<std::string, int>`
  to prove the `<E, A>` partial specialization isn't accidentally
  monomorphic to `int`.
- `src/examples/fixpoint_tree_example.cpp` reworked to `#include
  <smd/fixpoint/functors.hpp>` and use its `Expr`/`const_node`/
  `add_node`/`mul_node`/`eval` instead of local `ExprF`/`fmap_expr`
  definitions; output unchanged (`Result: 10`).
- `src/smd/fixpoint/recursion_schemes.t.cpp` and `fmap.t.cpp` **not**
  touched — both keep their own private `NatF`/`Zero`/`Succ` copies as
  intended (three independent `NatF` definitions now exist in the tree:
  `recursion_schemes.t.cpp`, `fmap.t.cpp`, `functors.hpp`'s public one —
  this is intentional, not drift).

## Verification evidence
- `make TOOLCHAIN=gcc-16 test`: configure+build+ctest clean — **100%
  tests passed, 0 tests failed out of 62** (up from S01's 50; net +12,
  all in `functors.t.cpp`: HeaderIsIdempotent, NatRoundTrip,
  NatMakeZeroSucc, NatFmapSmoke, ListVectorRoundTrip,
  ListVectorRoundTripEmpty, ListFmapSmoke, ListFmapStringPayloadIsGeneric,
  TreeMakeLeafNode, TreeFmapSmoke, ExprEvalMatchesHandComputed,
  ExprFmapSmoke). All 50 pre-existing tests still pass unchanged.
- `.build/build-gcc-16/src/examples/Asan/fixpoint_tree_example` prints
  `Result: 10` and exits 0.
- Four `static_assert`s compile (one per functor family):
  `nat_to_int(nat_from_int(4)) == 4`;
  `list_to_vector(list_from_vector(std::vector<int>{1,2,3})) ==
  std::vector<int>{1,2,3}` (constexpr `std::vector` + `Box` transient
  allocation both fine under gcc-16/C++26, no workaround needed);
  `sum_leaves(make_node(make_leaf(2), make_leaf(3))) == 5`;
  `eval(add_node(mul_node(const_node(2), const_node(3)), const_node(4)))
  == 10`.

## Deviations from the plan / design
None. No row added to `ops/DEVIATIONS.md`. `Fix<IntListF>`/`Fix<IntTreeF>`
(alias templates as template-template arguments) worked with no gcc-16
issues, so the step's fallback plan (a real wrapper struct) was not
needed.

## Discoveries affecting later steps
- **Alias templates as template-template args are unproblematic here.**
  `IntListF`, `IntTreeF` (both `template <class A> using ... = Family<Fixed,
  A>;`) work as `Fix<IntListF>`/`Fix<IntTreeF>` with no special handling —
  confirms design D3 cleanly, no fallback wrapper struct needed.
- **The `<E, A>` `functor_typeclass` partial specialization is exactly a
  two-parameter version of the S01 NatF pattern** — no new technique
  required, just an extra deduced parameter on `Impl`/`Map`/the variable
  template. Both `ListF<E,A>` and `TreeF<E,A>` use it; `ListFmapStringPayloadIsGeneric`
  in `functors.t.cpp` is the proof point later steps can point to if they
  doubt genericity.
- **`list_from_vector`'s coalgebra captures its input by reference**
  (`[&v](std::size_t i) -> IntListF<std::size_t> {...}`) rather than
  inventing a seed struct that bundles a pointer+index. Seed is plain
  `std::size_t`. This is constexpr-safe (the lambda and its captured
  reference don't escape the call), simpler than the alternative, and
  is the pattern to reach for whenever a later step's unfold needs to
  walk an existing container without copying: capture by reference in
  the coalgebra lambda, keep the seed as just the "position" state.
- **TreeF ships with no public fold/eval** — the step file only asked
  for `make_leaf`/`make_node`. Anything that needs to compute over an
  `IntTree` (S05's `zygo_balanced` example in particular) writes its own
  algebra against `IntTreeF<A>` via lookup `fold_fix`/`zygo`, the same
  way `functors.t.cpp`'s test-local `sum_leaves` does. `TreeF<E,A>`'s
  functor instance is public and ready for that.
- `Box::operator->()` returning `A*` is what makes
  `c.tail->begin()`/`c.tail->end()` work directly on a boxed
  `std::vector<int>` in `list_to_vector`'s Cons branch — no need to
  dereference-then-dot.

## Forward notes for the NEXT step (S03 — Identity + either/pair duals + Comonad)
- S03 depends only on S00 and touches exclusively `src/smd/typeclass/`
  (`identity.hpp`, `either.hpp`, `pair.hpp`, `comonad.hpp` +
  `test_support.hpp` migration) — it does not build on anything S02
  added to `src/smd/fixpoint/`. No file-overlap risk; S02 leaves
  `smd/typeclass/functor.hpp`/`monad.hpp` untouched, so the
  `OptionalXxxImpl`/`Map`/inline-constexpr pattern S03's step file asks
  you to imitate is exactly what you already saw at
  `src/smd/typeclass/functor.hpp` (`OptionalFunctorImpl`/
  `OptionalFunctorMap`) — same file S02 read, still current.
- S02 did add one thing S03 should know about even though it doesn't
  depend on it: `smd::fixpoint::overloaded` (from
  `src/smd/fixpoint/overloaded.hpp`) is the exhaustive-visitor helper
  used throughout this codebase for `std::visit` over hand-rolled
  variants (`NatF`, `ListF`, `TreeF`, `ExprF`, and now `either`'s
  `std::variant<Left<L>, Right<R>>` will be a fifth). S03's
  `either.hpp`/`pair.hpp` implementations should use
  `smd::fixpoint::overloaded` for `match`/`fmap`/`bind`'s internal
  `std::visit` calls, consistent with every existing instance in this
  tree (`functor.hpp`'s Optional/Vector instances are the one exception
  — they don't need visit since they're not variant-backed).
- Nothing in S02 changed `smd/typeclass/functor.hpp`'s
  `functor_typeclass` primary template or `Functor<Impl>` CRTP base —
  S03's `functor_typeclass<Identity<A>>`/`functor_typeclass<either<L,R>>`/
  `functor_typeclass<std::pair<B,A>>` instances plug into the exact same
  lookup machinery S02's four instances already use successfully
  (confirms the D2 dispatch mechanism scales past layer types built for
  `Fix`).
- Gate remains `make TOOLCHAIN=gcc-16 test` fully green; S03 has no
  example-binary requirement (no new `src/examples/` entry named in its
  step file).

## Open risks / TODOs
- gcc-17 secondary smoke was not re-run this step (advisory only per
  design D9/plan); still outstanding since S00, not blocking.
- None specific to S02's own scope were left open — gate is green, all
  "Verify" bullets from the step file were satisfied.
