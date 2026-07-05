# S02 — functors.hpp: the reusable base-functor library

**Goal.** One installed header providing the base functors every later
step's tests and examples use — NatF, ListF<E,·>, TreeF<E,·>, ExprF —
each with its functor_typeclass instance, smart constructors, and
converters to/from ordinary C++ values.

**Depends on:** S01.
**Design refs:** §3 D3, §4, §6.1, §10 (which examples consume what).

## Do
1. **New header `src/smd/fixpoint/functors.hpp`** (namespace
   `smd::fixpoint`), containing, for each functor: the node structs, the
   layer alias, the Fix alias, smart constructors, converters, and the
   functor_typeclass instance (reopening `namespace smd::typeclass`):

   - **NatF** — `struct Zero {}; template <class A> struct Succ {
     Box<A> pred; };  template <class A> using NatF = std::variant<Zero,
     Succ<A>>;  using Nat = Fix<NatF>;`
     Constructors `make_zero()`, `make_succ(Nat)`; converters
     `nat_from_int(int) -> Nat` (unfold), `nat_to_int(const Nat&) -> int`
     (fold). Move the equivalents currently local to
     `recursion_schemes.t.cpp` here *without* changing that test file.
   - **ListF** (design D3, element-parameterized) —
     `template <class E> struct Nil {};
      template <class E, class A> struct Cons { E head; Box<A> tail; };
      template <class E, class A> using ListF = std::variant<Nil<E>, Cons<E, A>>;`
     Per-element unary aliases users write themselves; provide
     `template <class A> using IntListF = ListF<int, A>;
      using IntList = Fix<IntListF>;` as the worked instance plus
     `list_from_vector(const std::vector<int>&)` /
     `list_to_vector(const IntList&)`.
     The functor_typeclass instance is partially specialized over
     `<E, A>` so *any* element type is covered.
   - **TreeF** — external binary tree with element payload at leaves or
     nodes (pick: `Leaf<E>` holding E, `Node<E,A>{Box<A> left; Box<A>
     right;}` — payloadless internal nodes keep zygo_balanced clean).
     `template <class A> using IntTreeF = TreeF<int, A>; using IntTree =
     Fix<IntTreeF>;` plus a `make_leaf`/`make_node` pair.
   - **ExprF** — `Const<A>{int}`, `Add<A>{Box<A>,Box<A>}`,
     `Mul<A>{Box<A>,Box<A>}` (as in
     `src/examples/fixpoint_tree_example.cpp`), smart constructors
     `const_node/add_node/mul_node`, `eval(const Expr&) -> int` via the
     lookup `fold_fix`.
2. **Wire** `functors.hpp` into `smd_fixpoint_headers`.
3. **Tests** `functors.t.cpp` (add to `smd_fixpoint_test`): round-trips
   (`nat_to_int(nat_from_int(n)) == n` for 0..10; vector↔list; expr
   eval == hand-computed), fmap-through-typeclass smoke for each
   functor, one `static_assert` per functor family (D10) exercising a
   small round-trip at compile time. `std::string` payload ListF fmap
   test to prove the `<E, A>` partial specialization really is generic.
4. **Rework `src/examples/fixpoint_tree_example.cpp`** to include
   `<smd/fixpoint/functors.hpp>` instead of redefining ExprF locally
   (delete its local definitions, keep its printed output identical
   or better). This validates the header from example context.

## Build
`make TOOLCHAIN=gcc-16 test` and run
`.build/build-gcc-16/src/examples/Asan/fixpoint_tree_example`.

## Verify (gate)
- Full suite green; new functors tests pass; static_asserts compile.
- `fixpoint_tree_example` prints `Result: 10`.

## Done when
Gate green; committed `[schemes] S02: functors.hpp base-functor library`.

## Capture in handoff
Final spellings of every alias and smart constructor (later steps write
tests against them — list them explicitly); whether alias templates as
template-template args (`Fix<IntListF>`) behaved under gcc-16; the exact
form the ListF functor_typeclass partial specialization took.

## Pitfalls
- Keep `Nil`/`Cons`/`Leaf`/`Node` names namespaced or suffixed enough
  not to collide with test-local types in later steps; they are public
  vocabulary now.
- variant alternatives must remain distinct types for every
  instantiation (`Nil<E>` is parameterized by E precisely so
  `ListF<E,A>` alternatives never unify).
- If `Fix<IntListF>` (alias template as template-template argument)
  misbehaves, fall back to a real
  `template <class A> struct IntListF : ListF<int, A> {...}` wrapper —
  but that breaks variant visitation; prefer recording a deviation and
  asking before inventing wrappers.
- Do not modify `recursion_schemes.t.cpp` — its local NatF copy keeps
  the explicit-fmap path independently tested.
