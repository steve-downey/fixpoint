# S03 — Identity, either, and the Comonad typeclass

**Goal.** Add the missing typeclass machinery the schemes need: a
public `Identity<A>` with all four instances; a symmetric
`either<L, R>` sum type (design D4 — deliberately NOT `std::expected`)
with right-biased instances, P2988-model reference sides, and the
`match`/`fanin` eliminators; the dual pair vocabulary (design D11) in
`pair.hpp`; and a `Comonad` CRTP base + `comonad_typeclass` lookup.

**Depends on:** S00 (parallel to S01/S02 — touches only smd/typeclass).
**Design refs:** §3 D4/D11, §5.1, §5.2, §6.3, §6.4.

## Do
1. **`src/smd/typeclass/identity.hpp`** — `Identity<A>` per design §5.1
   (aggregate, `value_type`, defaulted `==`), plus instances:
   `functor_typeclass<Identity<A>>`, `applicative_typeclass<Identity<A>>`
   (pure wraps, apply unwraps-invokes-wraps),
   `monad_typeclass<Identity<A>>` (bind = invoke on `.value`),
   and — after step 2 — `comonad_typeclass<Identity<A>>`.
   Follow the existing OptionalXxxImpl/Map/inline-constexpr pattern in
   `functor.hpp`/`monad.hpp` exactly; everything constexpr.
2. **`src/smd/typeclass/either.hpp`** — `either<L, R>` per design §5.2:
   variant of distinct `Left<L>`/`Right<R>` wrappers, `make_left` /
   `make_right`, `is_left`, `left`/`right` observers, defaulted `==`,
   free `map_left(f, e)`, and the copairing eliminators
   `match(e, on_left, on_right)` / `fanin(f, g)` (design D11).
   Instances: `functor_typeclass<either<L, R>>` (maps Right),
   `monad_typeclass<either<L, R>>` (pure = make_right, bind propagates
   Left). All constexpr. Header comment states the D4 convention
   (Left = short-circuit/stop, Right = continue), the rationale for not
   using `std::expected` (asymmetric sides, `expected<T, T>`
   construction ambiguity), and the D11 duality with std::pair.
3. **Reference sides for either** — per design §5.2's P2988 block
   (read P2988R12, linked there; it is the spec — gcc-16/17 libstdc++
   has no `optional<T&>` to imitate yet): partially specialize
   `Left<L&>` / `Right<R&>` to hold a pointer to the referent;
   construction binds lvalues only (rvalue constructor deleted);
   copy/assign rebind; shallow const; `left`/`right` still return
   `L&`/`R&` so generic code cannot tell the instantiations apart.
   All access in tests and later steps goes through the observers,
   never wrapper members.
4. **`src/smd/typeclass/pair.hpp`** — the pair half of the D11
   duality: `functor_typeclass<std::pair<B, A>>` (maps `.second`),
   the **env comonad instance** `comonad_typeclass<std::pair<B, A>>`
   (`extract` = `.second`, `duplicate(p)` = `{p.first, p}`,
   `fmap(f, p)` = `{p.first, f(p.second)}`), plus `map_first(f, p)`
   and `fanout(f, g)`. Header comment states the D11 rationale for the
   differing reference-assignment semantics: products assign through
   (both components always exist), sums rebind (the active alternative
   can change, so there may be no referent to assign through) — by
   construction, not convention.
5. **`src/smd/typeclass/comonad.hpp`** — the `Comonad<Impl>` CRTP base
   from design §6.3 (primitives `extract`, `duplicate`, `fmap`; derived
   `extend`), the `comonad_typeclass<T>` lookup variable, and a
   static_assert guard message mirroring `Monad`'s. (Instances live
   with their types: pair.hpp here, Cofree in S07.)
6. **Migrate `test_support.hpp`** to include `identity.hpp` and delete
   its private `Identity` + duplicated applicative instance, keeping the
   `Sequence` machinery and law helpers. Adjust the `traverse`/`apply`
   tests only where the type moved (they should not change behavior —
   the test-support Traversable instance for Identity stays, now keyed
   on the public type).
7. **Tests** — `identity.t.cpp`, `either.t.cpp`, `pair.t.cpp` and
   `comonad.t.cpp` in `src/smd/typeclass/`, added to
   `smd_typeclass_test`:
   - Identity: functor/monad laws by example, `pure`/`bind`
     round-trips, constexpr static_assert (`extract(duplicate(w)) == w`
     etc. — all four comonad laws are cheap on Identity: extract∘duplicate
     = id, fmap(extract)∘duplicate = id, duplicate∘duplicate associativity).
   - either: construction/observers both sides; fmap maps Right only;
     bind propagates Left; map_left maps Left only; `match`/`fanin`
     eliminator laws (`match(make_left(x), f, g) == f(x)`, dually for
     Right); **an `either<int, int>` test** proving same-type sides
     construct and compare unambiguously (the reason this type
     exists); constexpr static_assert.
   - either references: binds an lvalue and `&left(e)` is the original
     object; assignment **rebinds** (assign a different-referent either,
     check the pointer moved and the original object is untouched);
     `static_assert(!std::is_constructible_v<Left<const int&>, int&&>)`
     -style no-temporary checks on both sides; shallow-const check;
     an `either<const Fix&, Seed>`-shaped smoke for S04's zero-copy
     graft; constexpr static_assert.
   - pair: map_first/fanout behavior; env comonad
     extract/duplicate/extend, one law spot-check
     `extend(extract, w) == w`; a duality smoke: `fanout(f, g)` then
     `.first`/`.second` recovers `f`/`g`, `fanin(f, g)` after
     `make_left`/`make_right` recovers `f`/`g`.
8. **Wire** all four headers into `smd_typeclass_headers`.

## Build
`make TOOLCHAIN=gcc-16 test`

## Verify (gate)
- Full suite green including the pre-existing typeclass tests
  (test_support migration must not change any outcome).
- Comonad law spot-checks pass for both instances; static_asserts
  compile.

## Done when
Gate green; committed `[schemes] S03: Identity + either/pair duals + Comonad`.

## Capture in handoff
Exact primitive set the Comonad Impl requires (S07 writes the Cofree
instance from this); the either observer/constructor/match spellings as
landed (S04's apo and S11's elgot write against them); how the
reference-side wrapper specializations interact with the variant and
with `==` (mixed reference/value comparisons — what you allowed and
why); the pair-env instance's behavior with reference qualifiers (any
`remove_cvref_t` handling you needed); confirmation the old
test-support Identity is gone (S12/S13 must not find two Identitys).

## Pitfalls
- `comonad_typeclass<std::pair<B, A>>` — both parameters deduced;
  remember partial specializations of variable templates go at
  namespace scope in `smd::typeclass`.
- Keep `Identity` an aggregate (no user-declared ctors) so CTAD
  `Identity{x}` works in dist_cata later.
- The Monad CRTP's `static_assert(!is_same_v<Impl, false_type>)`
  pattern must be replicated so a missing comonad instance gives the
  helpful message, not template noise.
- Reference sides: the deleted-rvalue constraint belongs on the
  *wrapper* constructors (`Left<L&>`), and `make_left`/`make_right`
  must forward in a way that preserves it — a careless `make_left`
  taking by value would re-open the dangling hole P2988 closes. Write
  the no-temporary static_asserts against `make_left`/`make_right`
  too, not just the wrappers.
- `Left<L&>` has a user-declared constructor (not an aggregate) while
  `Left<L>` is an aggregate — keep construction in tests/library going
  through `make_left`/`make_right` so the difference never shows.
