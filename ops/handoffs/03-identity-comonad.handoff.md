# Handoff — S03 Identity + either/pair duals + Comonad

- **Status:** DONE (gate passed)
- **Commit:** cb6d15d — `[schemes] S03: Identity + either/pair duals + Comonad`
- **Date / agent:** 2026-07-03, background execution agent

## What changed

- New `src/smd/typeclass/identity.hpp` (namespace `smd::typeclass`):
  `Identity<A>` exactly per design §5.1 (aggregate, `value_type`,
  defaulted `==`). Instances, in this order, each a
  `XxxImpl`/`XxxMap : Base<XxxImpl>` pair following the
  `OptionalFunctorImpl`/`Map` house pattern:
  - `functor_typeclass<Identity<A>>` — `IdentityFunctorImpl`/`Map`.
  - `applicative_typeclass<Identity<A>>` — `IdentityApplicativeImpl`/`Map`
    (`pure` wraps, `apply` unwraps-invokes-wraps, copied from the
    `TestIdentityApplicativeImpl` shape that used to live in
    `test_support.hpp`).
  - `monad_typeclass<Identity<A>>` — `IdentityMonadImpl`/`Map` (`bind`
    invokes directly on `.value`).
  - `comonad_typeclass<Identity<A>>` — `IdentityComonadImpl`/`Map`
    (`extract` = `.value`, `duplicate(w)` = `Identity<Identity<A>>{w}`,
    `fmap` as functor).
- New `src/smd/typeclass/comonad.hpp`: `Comonad<Impl>` CRTP base
  (`extract`/`duplicate`/`fmap` from `Impl`, derived `extend = fmap f .
  duplicate`), `comonad_typeclass<T>` lookup variable, and the
  `static_assert(!is_same_v<Impl, false_type>, ...)` guard mirroring
  `Monad`'s message. Mirrors design §6.3 essentially verbatim.
- New `src/smd/typeclass/either.hpp`: `Left<L>`/`Right<R>` (aggregates,
  defaulted `==`, each with a `.referent() const` accessor) plus P2988
  reference-side partial specializations `Left<L&>`/`Right<R&>` (hold a
  raw pointer, `explicit Left(L&)`, `Left(L&&) = delete`, defaulted
  rebinding copy/assign, hand-written value-comparing `==`). `either<L,
  R>` (aggregate: `left_type`, `right_type`, `std::variant<Left<L>,
  Right<R>> node`, defaulted `==`). `make_left<R, L>(L v) ->
  either<L,R>`, `make_right<L, R>(R v) -> either<L,R>`, `is_left`,
  `left`/`right` (both declared `-> const L&`/`-> const R&` — see
  Discoveries for why this is the whole trick), `map_left(fn, e)`,
  `match(e, on_left, on_right)`, `fanin(f, g)`. Instances:
  `functor_typeclass<either<L,R>>` (maps Right),
  `monad_typeclass<either<L,R>>` (`pure` = `make_right`, `bind`
  propagates Left, generic in the incoming Right type — see
  Discoveries). Uses `smd::fixpoint::overloaded` for all `std::visit`
  calls per the S02 handoff's forward note.
- New `src/smd/typeclass/pair.hpp`: `functor_typeclass<std::pair<B,A>>`
  (maps `.second`), `comonad_typeclass<std::pair<B,A>>` (the env
  comonad: `extract` = `.second`, `duplicate(p)` =
  `{p.first, p}`, generic in the value-slot type — see Discoveries),
  `map_first(fn, p)`, `fanout(f, g)`.
- `src/smd/typeclass/test_support.hpp`: added `#include
  <smd/typeclass/identity.hpp>`; deleted the private `test::Identity`
  struct and `TestIdentityApplicativeImpl`/`Map`/instance (now redundant
  with the public `Identity`); retargeted
  `TestIdentityTraversableImpl`/`Map`/`traversable_typeclass<...>` from
  `test::Identity<VALUE_TYPE>` to the public `smd::typeclass::Identity<VALUE_TYPE>`
  (unchanged behavior — same `.value`/`value_type` shape). `test::Sequence`
  and both `check_applicative_*_law` helpers untouched.
- `src/smd/typeclass/traverse.t.cpp`: the one call site using
  `test::Identity` (`"traverse: for_each on Identity..."`) updated to
  `bt::Identity<int>` — no behavior change.
- `src/smd/typeclass/CMakeLists.txt`: added `identity.hpp`, `either.hpp`,
  `pair.hpp`, `comonad.hpp` to `smd_typeclass_headers`; `identity.t.cpp`,
  `either.t.cpp`, `pair.t.cpp`, `comonad.t.cpp` to `smd_typeclass_test`.
- New test files `identity.t.cpp`, `either.t.cpp`, `pair.t.cpp`,
  `comonad.t.cpp` (all with a `HeaderIsIdempotent` re-inclusion check per
  design §4, even though most pre-existing `smd/typeclass/*.t.cpp` files
  predate that convention and don't have one — added it anyway since §4
  states it as a blanket rule).

## Verification evidence

- Clean rebuild (`rm -rf .build/build-gcc-16 && make TOOLCHAIN=gcc-16
  test`): **100% tests passed, 0 tests failed out of 98** (up from S02's
  62; net +36: 4 `HeaderIsIdempotent` tests, and full coverage of every
  bullet in the step file's "Tests" section across the four new files).
  Zero compiler warnings on a from-scratch build (`grep -i warning` over
  the full build log was empty).
- Every `static_assert` in `identity.t.cpp` (comonad laws +
  `Identity<int>{1}==...`), `either.t.cpp` (`either_constexpr_smoke`:
  construction, `is_left`, `left`/`right`, functor `fmap`, `match`, all
  constexpr), `pair.t.cpp` (`pair_constexpr_smoke`: extract, duplicate,
  extend law, `map_first`, `fanout`, constexpr), `comonad.t.cpp`
  (`comonad_generic_laws_hold`, exercising both instances through the
  same CRTP base) compiles clean.

## Deviations from the plan / design

None against the design doc's stated types/signatures. No row added to
`ops/DEVIATIONS.md`. One thing worth flagging as a *correction to a bug
in my own first draft*, not a design deviation: my first implementation
of `IdentityComonadImpl`/`PairComonadImpl` fixed `extract`/`duplicate`/
`fmap` to the instance's own class-level type parameter (`A` for
Identity, the value slot for pair) — mirroring how
`OptionalFunctorImpl<VALUE_TYPE>::fmap` is written. That compiles for
`extract`/`duplicate` alone but **fails for `extend`**, because
`extend`'s derived body is `self.fmap(f, self.duplicate(wa))`, and
`self.duplicate(wa)` produces the *doubled* structure
(`Identity<Identity<A>>` / `pair<B, pair<B,A>>`) — `self.fmap` then has
to accept that doubled type, not the original `Identity<A>` /
`pair<B,A>`. The fix (now landed): make `extract`/`duplicate`/`fmap`
templates over their own element-type parameter (`X`), exactly the way
`OptionalMonadImpl::bind` is templated over its own `A` rather than
tied to the class's `VALUE_TYPE` — the class-level parameter becomes
vestigial, used only to key the `comonad_typeclass<...>` variable-template
specialization, not referenced inside the Impl's method bodies. Applied
the same fix to `EitherMonadImpl::bind` (generic in the incoming Right
type, `L` fixed) even though no current test exercises `either`'s
`join`, purely for consistency/robustness against S04/S11 needing it.
**This is the single most important thing for future Comonad/Monad
instance authors (S07 Cofree, S08 Free) to get right the first time —
see Discoveries below.**

## Discoveries affecting later steps

- **Comonad/Monad `Impl`s must template their primitives over the
  element/payload type, not the instance's own class-level type
  parameter, whenever a derived CRTP operation (`extend`, `join`,
  `apply`, `kleisli`) needs to operate on a doubled/nested structure.**
  This is *not* obvious from `functor.hpp`'s `OptionalFunctorImpl`
  (which is legitimately fixed, since `Functor`'s only derived op,
  `replace`, never re-wraps). It only bites for `Comonad` (`duplicate`
  produces `W(W A)`, and `extend`'s `fmap` must consume it) and `Monad`
  (`join`/`kleisli` similarly need `bind` to consume `M(M A)`). The
  existing `OptionalMonadImpl::bind` already does this correctly
  (`template <class A, class F> auto bind(this auto&&, const
  std::optional<A>&, F&&)` — `A` here shadows/ignores the class's own
  `VALUE_TYPE`), but it's easy to miss since nothing before this step
  exercised `extend`/`join` against a *newly authored* instance. **S07's
  `Cofree<F,A>` comonad instance and S08's `Free<F,A>` monad instance
  must follow this same pattern**: `extract`/`duplicate`/`fmap` (Cofree)
  and `pure`/`bind` (Free) need to be generic over the wrapped type, with
  only the outer functor `F` (and, for pair, the environment `B`) held
  fixed at the class level.
- **The `left`/`right` observer trick**: both are declared with a fixed
  signature `template <class L, class R> constexpr auto left(const
  either<L,R>& e) -> const L&` (and dually for `right`). When `L` is
  itself instantiated as a reference type (e.g. `L = const Nat&` for
  S04's zero-copy graft), the language drops the `const` qualifier
  applied to a reference type during substitution, so `const L&`
  collapses to plain `L&` — i.e. `const (const Nat&)` = `const Nat&`,
  unchanged. This is the *entire* mechanism behind "shallow const" and
  "generic code cannot tell the instantiations apart" (design §5.2/step
  pitfalls): no `if constexpr`, no tag dispatch, just one template
  signature and reference-collapsing. `Left<L>::referent()`/`Left<L&>::referent()`
  mirror this at the wrapper level (primary: `const L&` from a stored
  value; reference spec: `L&` — non-const! — from a stored pointer,
  callable on a `const Left<L&>`, which is the actual "shallow const"
  bit: the wrapper's own constness only ever protects the pointer, never
  the pointee).
- **`make_left<R, L>(L v)` / `make_right<L, R>(R v)`'s no-temporary
  guarantee comes largely for free from ordinary reference-binding
  rules, not extra machinery**: when `L` is explicitly instantiated as a
  reference type, the parameter `L v` is itself a reference parameter
  (e.g. `int& v`), and a non-const lvalue-reference parameter simply
  cannot bind an rvalue — so passing a temporary to `make_left<R, L&>(...)`
  is a hard compile error at the call site, on top of (not instead of)
  `Left<L&>`'s own deleted `Left(L&&)` constructor. Both static_asserts
  in `either.t.cpp` (`is_constructible_v` on the wrappers directly,
  `is_invocable_v` on `make_left`/`make_right` themselves) are present
  and both pass — test whichever one matters for whatever you're adding
  next.
- **Equality on the reference-side wrappers is value-comparing, not
  identity-comparing** (`Left<L&>::operator==` dereferences both
  pointers and compares `*lhs.ptr == *rhs.ptr`, hand-written — NOT
  `=default`, which would compare pointer identity instead). This
  matches the rest of the codebase's value-semantics convention
  (`Box::operator==` does the same dereference-and-compare). `either<L,R>`'s
  own `operator==` stays `=default` (delegates to `std::variant`'s
  `==`, which needs each alternative comparable — satisfied either way).
- **`std::pair<B,A>` needed zero special-casing for reference
  components.** libstdc++/gcc-16's `std::pair` already has the C++23
  (P2321R2) assignment-through machinery for reference-typed members
  (verified directly in `/usr/include/c++/16/bits/stl_pair.h`), so
  design D11's "pair assigns through" claim required no extra code in
  `pair.hpp` — it's inherited from the standard library for free. This
  is the intended asymmetry with `either.hpp`'s hand-written rebinding
  wrapper.
- Confirmed: only one `Identity` exists in the tree now (the public
  `smd::typeclass::Identity<A>` in `identity.hpp`); `test_support.hpp`'s
  private copy and its duplicated applicative instance are gone. S12/S13
  will not find two Identitys.
- `smd::fixpoint::overloaded`'s `consteval` catch-all (S02's forward
  note) continues to work fine inside `either.hpp`'s constexpr
  `fmap`/`bind`/`match`/`map_left` — no new friction beyond what S01/S02
  already proved for `NatF`/`ListF`/etc.

## Forward notes for the NEXT step (S04 — para + apo)

- **`match`'s landed signature**: `template <class L, class R, class
  OnLeft, class OnRight> constexpr auto match(const either<L,R>& e,
  OnLeft&& on_left, OnRight&& on_right)` — applies `on_left`/`on_right`
  to the *unwrapped referent* (`.referent()`), not the `Left`/`Right`
  wrapper. Use this directly for apo's worker per the step's own
  guidance; don't hand-roll `is_left`/`std::get` branching.
- **For apo's Left branch, `left(e)` already does the right thing for
  "return by value, don't move out of a const&"**: since `left`/`match`'s
  branch return `const L&` (which, for the zero-copy graft's `L = const
  Fix<F>&`, collapses to `const Fix<F>&` per the Discoveries note
  above), writing `return left(e);` (or the `on_left` branch of `match`
  returning its argument) from a function returning `Fix<F>` by value
  triggers an ordinary copy-construction — exactly the "by value, not
  moved-from-const&" behavior the step's pitfalls ask for, with no
  special-casing needed in the apo worker itself.
- **`make_left`/`make_right`'s explicit-template-argument order is `<R,
  L>` and `<L, R>` respectively** (result/non-deduced side named first,
  per design). For the zero-copy graft coalgebra, construct the Left
  side with an explicit reference template argument, e.g.
  `make_left<Seed, const Fix<F>&>(existing_subtree)` — confirmed
  compiling and round-tripping in `either.t.cpp`'s
  `"either<const Fix&, Seed>-shaped smoke: zero-copy graft (S04)"` test
  (uses a local `Nat`/`NatF` stand-in for `Fix<F>`, not the real
  `functors.hpp` types — S04 will use whatever `F` its own test fixture
  needs, this was just a shape proof).
- **`either<Fix<F>, Fix<F>>` (the same-type-sides case) needs nothing
  special** — confirmed generically in `either.t.cpp`'s
  `"either<int, int>: ..."` test (using `int` for both sides rather than
  `Fix<F>`, but the mechanism — distinct `Left`/`Right` wrapper types
  making the variant unambiguous — is exactly what makes
  `either<Fix<F>, Fix<F>>` work too, and does not depend on the payload
  type at all).
- `either.hpp`'s `EitherMonadImpl::bind` is generic in the incoming
  Right type (see Discoveries) — if S04's `apo` or later steps end up
  wanting `join`/`kleisli` over `either`, that already works; no further
  changes needed there.
- Gate remains `make TOOLCHAIN=gcc-16 test` fully green; S04 additionally
  requires two new example binaries (`para_pretty_print`,
  `apo_sorted_insert`) to run and exit 0 with visibly-correct output —
  follow `fixpoint_tree_example`'s pattern in
  `src/examples/CMakeLists.txt` for the executable+install blocks.

## Open risks / TODOs

- gcc-17 secondary smoke was not re-run this step (advisory only per
  design D9/plan); still outstanding since S00, not blocking.
- None specific to S03's own scope were left open — gate is green, all
  "Verify" bullets from the step file were satisfied, and the bug caught
  during self-testing (Comonad/Monad genericity, see Deviations) was
  fixed before landing, not deferred.
