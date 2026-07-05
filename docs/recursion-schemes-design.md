# Recursion Schemes for smd::fixpoint — Design

This document is the paper-proof for implementing the full Kmett
`recursion-schemes` catalog on top of this repository's `smd::fixpoint`
core (`Fix`, `Box`, `fold_fix`/`unfold_fix`/`refold`) and the
`smd::typeclass` machinery (Functor, Applicative, Monad, Monoid, Foldable,
Traversable). The operational plan in `ops/PLAN.md` exists to *test* this
design in real code; every place reality contradicts it is recorded in
`ops/DEVIATIONS.md` and reconciled back here.

Section references (§n) below are what the step files in `ops/steps/` cite.

## §1 Purpose and scope

Implement, with tests and motivating examples, the folds, unfolds and
refolds of Edward Kmett's `recursion-schemes` package:

- **Classical**: cata, ana, hylo (already present as `fold_fix`,
  `unfold_fix`, `refold`), para, apo.
- **Fokkinga**: zygo, mutu, prepro, postpro.
- **Course-of-values** (via Cofree/Free): histo, futu, dyna, codyna, chrono.
- **Mendler-style**: mcata, mhisto.
- **Elgot (co)algebras**: elgot, coelgot.
- **Generalizations**: distributive laws, gcata, gana, ghylo, gprepro,
  gpostpro, and the capstone `zygo_histo_prepro`.

Each scheme ships with: a header in `src/smd/fixpoint/`, Catch2 tests
(including the equivalence laws of §9), and a runnable motivating example
in `src/examples/`.

**Toolchain**: C++26, GCC 16 primary (`make TOOLCHAIN=gcc-16 test`),
GCC 17 secondary smoke build. See §3 D9.

## §2 Ground: what already exists

- `src/smd/fixpoint/fix.hpp` — `Fix<F>`, `wrap_fix`, `unwrap_fix`.
  `F` is a unary template (the base functor); `Fix<F>` holds one layer
  `F<Fix<F>>` by value.
- `src/smd/fixpoint/box.hpp` — `Box<A>`: constexpr-capable, nullable,
  deep-copying owning pointer; `make_box<A>(args...)`.
- `src/smd/fixpoint/recursion_schemes.hpp` — `fold_fix<Result>(alg,
  fmap_fn, tree)`, `unfold_fix<F>(coalg, fmap_fn, seed)`,
  `refold<Result, F>(alg, coalg, fmap_fn, seed)`. All take fmap
  explicitly as a callable `(recurse, layer) -> mapped-layer`.
- `src/smd/fixpoint/overloaded.hpp` — exhaustive visitor helper with a
  consteval catch-all that turns missed variant alternatives into compile
  errors.
- `src/smd/typeclass/` — `functor_typeclass<T>`, `applicative_typeclass<T>`,
  `monad_typeclass<T>`, `Monoid<T>`/`monoid_v<T>`,
  `foldable_typeclass<T>`, `traversable_typeclass<T>`. All are inline
  constexpr variable templates specialized per concrete type, holding CRTP
  "Map" objects (deducing-this style). Vector and optional instances exist.
- Build: CMake + vcpkg/Catch2, `make [TOOLCHAIN=gcc-16] test` builds the
  Asan config and runs ctest. Baseline before this plan: 45 tests green.

## §3 Decisions log

- **D1 — Naming.** The existing library deliberately renamed cata/ana/hylo
  to `fold_fix`/`unfold_fix`/`refold` (and `[[deprecated]]`-ed `cata`).
  Those three keep their descriptive names. Every *new* scheme uses its
  literature name (`para`, `apo`, `zygo`, `mutu`, `histo`, `futu`,
  `prepro`, `postpro`, `dyna`, `codyna`, `chrono`, `mcata`, `mhisto`,
  `elgot`, `coelgot`, `gcata`, `gana`, `ghylo`, `gprepro`, `gpostpro`,
  `zygo_histo_prepro`): they are terms of art with no adequate descriptive
  equivalents. Do not resurrect `cata`.
- **D2 — Functor dispatch is keyed on the concrete layer type.** Schemes
  need `fmap : (A -> B) -> F<A> -> F<B>` for the base functor. We reuse
  `smd::typeclass::functor_typeclass<T>` specialized on concrete layer
  instantiations (e.g. `functor_typeclass<NatF<A>>` for all `A`, via
  partial specialization of the variable template). Generic scheme code
  looks it up from the deduced layer type:
  `functor_typeclass<std::remove_cvref_t<decltype(layer)>>.fmap(f, layer)`.
  This keying (a) reuses the imported typeclass machinery unchanged, and
  (b) unlike keying on the template-template parameter, admits partial
  specialization for element-parameterized functors like `ListF<E, A>`.
- **D3 — Element-parameterized base functors.** A base functor family
  with payload type `E` is a *binary* template `ListF<E, A>`; users bind
  the payload with an alias template to obtain the unary shape `Fix`
  wants: `template <class A> using IntListF = ListF<int, A>;
  using IntList = Fix<IntListF>;`. Alias templates are valid
  template-template arguments (P0522). functor_typeclass instances are
  partially specialized over `<E, A>`, so one instance covers the family.
- **D4 — Short-circuit sums use a project `either<L, R>`, not
  `std::expected`.** `std::expected` was considered and rejected: its
  sides are asymmetric (`unexpected` is a wrapper type, error-side
  mapping is spelled `transform_error`, and `expected<T, T>` — which
  arises naturally, e.g. apo with `Seed = Fix<F>` — forces tag-only
  construction and invites ambiguities in generic code). Instead,
  `smd::typeclass::either<L, R>` (§5.2): a symmetric variant-backed sum
  with distinct `Left`/`Right` construction on both sides. Fixed
  convention, identical to the Haskell sources: **Left =
  short-circuit/stop, Right = continue recursing**. Concretely: apo
  coalgebras return `F<either<Fix<F>, Seed>>` (Left = finished subtree,
  embed as-is); elgot coalgebras return `either<Result, F<Seed>>`
  (Left = final answer, stop). The functor/monad instances are
  right-biased, matching Haskell's `Either` instances, so the §7
  transcriptions need no side-flipping. `either` supports reference
  sides (`either<L&, R>`, `either<L, R&>`, both) following the C++26
  `std::optional<T&>` model of P2988R12
  (https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p2988r12.pdf):
  wrapper partial specializations holding a pointer to the referent,
  rebinding assignment, lvalue-only binding — see §5.2.
- **D11 — `std::pair` and `either` are proper duals.** pair is the
  product, either the coproduct, and the library treats them
  symmetrically throughout:
  - *slot convention*: the non-value slot comes first, the value slot
    second — pair's env comonad has `extract = .second`; either's monad
    has `pure = make_right`. zygo's `pair<Helper, Result>` and D4's
    `either<Stop, Continue>` are the same convention.
  - *dual vocabulary* (§5.2): introduction `pair{a, b}` (both) ↔
    `make_left`/`make_right` (one of); elimination `.first`/`.second`
    (one of) ↔ `match(e, on_left, on_right)` (both — the copairing
    `[f,g]`); `fanout(f, g)` (the pairing `⟨f,g⟩`, returns
    `x ↦ pair{f(x), g(x)}`) ↔ `fanin(f, g)` (returns the matcher);
    `map_first(f, p)` ↔ `map_left(f, e)`; functor instance maps
    `.second` ↔ maps Right.
  - *dual instances*: env comonad `pair<B, ·>` ↔ either monad
    `either<B, ·>`; correspondingly `dist_zygo`/`dist_para` (pair side,
    gcata) ↔ `dist_gapo`/`dist_apo` (either side, gana), and para
    (pair carrier) ↔ apo (either carrier).
  - *reference-assignment semantics differ by construction, and that is
    correct*: `std::pair<T&, U>` members assign *through*, while
    `either<L&, R>` *rebinds* per P2988. This is not an inconsistency
    to fix — it is the only reasonable way for value-semantic reference
    types to behave in products vs. sums. In a product both components
    always exist, so assignment can meaningfully forward to the
    referents; in a sum the active alternative can change under
    assignment, so there may be no referent to assign through —
    rebinding is the only coherent model. The pair.hpp/either.hpp
    header comments state this rationale.
- **D5 — Explicit carrier type parameters.** C++ cannot infer fold
  carriers through recursive calls the way Haskell infers types. Following
  the existing `fold_fix<Result>(...)` convention, every scheme takes its
  carrier(s) as leading explicit template parameters: `para<Result>`,
  `zygo<Result, Helper>`, `mutu<A, B>`, `gcata<Result, WResult>` (the
  wrapped carrier `W<Result>` must be nameable to break the
  type-deduction cycle; see §7.10).
- **D6 — Value semantics; copies are accepted.** Layers, Cofree
  annotations and Free chunks are copied freely (Box deep-copies). No
  sharing, no move-optimization pass, no memoization beyond what the
  scheme itself provides. Performance work is a non-goal (§11).
- **D7 — No stack-safety work.** All schemes are recursive functions;
  deep structures can overflow the stack, as the naive Haskell versions
  can. Tests keep structures small (≤ ~1000 nodes). Non-goal (§11).
- **D8 — Tests are the spec.** Every scheme's step gates on the
  equivalence laws in §9 (e.g. `para` degenerates to `fold_fix`,
  `gcata(dist_cata) ≡ fold_fix`). The recursive equations in §7 were
  transcribed from recursion-schemes; if an equation and its law
  disagree, trust the law, fix the equation, and record a deviation.
- **D9 — Toolchain.** C++26 (`-std=gnu++26`), primary compiler `g++-16`
  (Ubuntu package), secondary `g++-17` (`~/.local/bin/g++-17`,
  personal trunk build). The gate for every step is
  `make TOOLCHAIN=gcc-16 test` fully green. `etc/gcc-flags.cmake` is
  bumped to gnu++26 in S00 (shared by all gcc toolchain files — GCC 15
  still accepts the flag; only the gcc-16+ *library* is guaranteed to
  have C++26 additions, so the gate toolchain is gcc-16).
- **D10 — constexpr as a design pillar.** `Box` exists precisely to make
  fixpoint trees constexpr-capable. All new schemes and supporting types
  are declared `constexpr`, and each scheme's test file includes at least
  one `static_assert` exercising it at compile time (a small structure,
  built-in payloads). If a particular scheme cannot be made constexpr
  under GCC 16, keep the runtime tests, drop the static_assert, and
  record a deviation. `std::indirect` exists in GCC 16's C++26 library
  and could replace `Box`; we deliberately stay with `Box` for
  consistency and its nullable default (see box.hpp's rationale).
  Migration to `std::indirect` is out of scope (§11).
- **D12 — Explicit-instance `_with` forms; thread the instance by value.**
  Every generic scheme provides a `_with` form taking the functor instance
  *by value* as an explicit first argument
  (`fold_fix_with(functor, alg, tree)`, `unfold_fix_with`, `refold_with`,
  `zygo_with`, …), threaded unchanged down the recursion. This is the
  standing contract that a value-based local typeclass instance is *always*
  a provided API, so a caller is never stuck when the global lookup is wrong
  (a typeclass with no canonical default — e.g. Monoid over `int`: sum vs
  product vs min vs max, with no transparent newtype in C++ to disambiguate)
  or absent (unregistered, test-local, or ODR-sensitive types). A **distinct
  `_with` name**, not an overload of the scheme name: the `_with`/overload
  question was prototyped both ways (branches `experiment-schemes-with` and
  `experiment-schemes-overload`), and overloading the name loses decisively on
  diagnostics — a mistyped instance to the 3-arg-colliding schemes falls
  through to the unconstrained `fmap_fn` overload and errors deep in its body
  instead of at a named `static_assert`. Each `_with` validates its instance
  with a `static_assert` (`functor_instance_for`), not a `requires`-clause:
  threading is not overload selection, so a bad instance earns one direct
  diagnostic. At multi-site schemes (e.g. zygo's two fmap sites at different
  element types) the single threaded instance must be element-generic. Full
  record: `docs/explicit-typeclass-instance-dispatch.md`.

## §4 Core conventions

- **Boxing is the functor's responsibility.** Recursive positions inside
  base functor alternatives hold `Box<A>` (e.g. `Succ<A>{Box<A> pred}`).
  Consequently `F<X>` is a *complete type for incomplete X*, which is what
  lets `Fix<F>{F<Fix<F>>}`, `Cofree`, and `Free` contain their own layers
  by value.
- **Algebras/coalgebras are taken by `const&`** and invoked with
  `std::invoke`-compatible call syntax (plain call is fine; these are
  function objects/lambdas).
- **fmap through the typeclass**: inside scheme bodies, define once:
  ```cpp
  template <class Fn, class Layer>
  constexpr auto layer_fmap(Fn&& fn, const Layer& layer) {
      return smd::typeclass::functor_typeclass<Layer>.fmap(
          std::forward<Fn>(fn), layer);
  }
  ```
  (put this helper in `src/smd/fixpoint/fmap.hpp`, S01, and use it
  everywhere). A layer type without an instance produces the
  `functor_typeclass` static_assert diagnostic.
- **Natural transformations** (prepro/postpro/hoist) are polymorphic
  function objects: callable on `F<X>` for every `X`, returning the same
  shape (`F<X>` for endo-transformations, `G<X>` for `hoist`).
  E.g. `struct cap_layer { template <class A> constexpr NatF<A>
  operator()(const NatF<A>&) const; };`
- **Distributive laws** are likewise polymorphic function objects
  (§7.9): `dist(F<W<X>>) -> W<F<X>>` for every `X`.
- Headers follow house style: `-*-C++-*-` first line, SPDX second,
  `INCLUDED_SMD_FIXPOINT_*` guards, one class/scheme family per header,
  tests in sibling `.t.cpp`, re-inclusion check at top of each test.

## §5 Supporting types

### §5.1 Identity (smd/typeclass/identity.hpp, S03)

```cpp
template <class A>
struct Identity {
    using value_type = A;
    A value;
    friend constexpr bool operator==(const Identity&, const Identity&) = default;
};
```
Instances: `functor_typeclass`, `applicative_typeclass`,
`monad_typeclass`, `comonad_typeclass` — all four, trivially. (There is a
test-only `Identity` in `smd/typeclass/test_support.hpp`; the new public
one replaces it — migrate test_support to use the public type.)

### §5.2 either (smd/typeclass/either.hpp, S03)

Symmetric sum type per D4 (Left = stop, Right = continue):

```cpp
template <class L>
struct Left { L value; };
template <class R>
struct Right { R value; };

template <class L, class R>
struct either {
    using left_type = L;
    using right_type = R;
    std::variant<Left<L>, Right<R>> node;
    friend constexpr bool operator==(const either&, const either&) = default;
};
template <class R, class L>            // result side named first
constexpr either<L, R> make_left(L v);
template <class L, class R>
constexpr either<L, R> make_right(R v);
// observers: is_left(e); left(e) -> const L&; right(e) -> const R&
```
`Left<L>`/`Right<R>` are distinct wrapper types on *both* sides, so
`either<T, T>` constructs unambiguously — the failure mode that ruled
out `std::expected`. Instances (right-biased, as in Haskell):
`functor_typeclass<either<L, R>>` maps the Right side;
`monad_typeclass<either<L, R>>` — `pure` = `make_right`, `bind`
propagates Left untouched. A left-side map `map_left(f, e)` ships as a
plain free function (dist_gapo uses it; no left-functor typeclass
needed).

**Elimination — the copairing.** `match(e, on_left, on_right)` applies
the matching branch to the *unwrapped* referent and returns the common
result type — this is the coproduct eliminator `[f,g]`, the categorical
dual of building a pair (D11). `fanin(f, g)` returns the matcher as a
callable; `fanout(f, g)` (in pair.hpp) is its dual `⟨f,g⟩`. Scheme
workers (apo, elgot, gana) should prefer `match` over hand-rolled
`is_left` branching.

**Reference sides — the P2988 model.** `either<L&, R>` etc. work via
partial specializations of the *wrappers*; `either` itself is
untouched:

```cpp
template <class L>
struct Left<L&> {
    L* ptr;
    constexpr Left(L& ref) : ptr(std::addressof(ref)) {}
    Left(L&&) = delete;   // no binding to temporaries — P2988 rule
};
// Right<R&> identically.
```
Semantics, all per P2988R12 (std::optional<T&>, C++26,
https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p2988r12.pdf):
- **holds a pointer** to the referent; copy/assignment are trivial and
  **rebind** (variant replacement rebinds naturally — never assigns
  through);
- **lvalue-only binding**: construction from rvalues is deleted, so no
  dangling from temporaries;
- **shallow const**: `left(e)` on a `const either<L&, R>` still yields
  `L&`;
- observers `left`/`right` return `L&`/`R&` exactly as in the
  by-value case, so generic scheme code is oblivious to which
  instantiation it holds.
Note: gcc-16/17 libstdc++ does not yet ship `optional<T&>`
(`__cpp_lib_optional_ref` undefined), so the paper — not the std
implementation — is the reference for behavior.
Payoff in-plan: a zero-copy apo graft coalgebra can return
`F<either<const Fix<F>&, Seed>>` — the worker copies once at the graft
point either way (§7.2).

### §5.3 Cofree comonad (src/smd/fixpoint/cofree.hpp, S07)

```cpp
template <template <class> class F, class A>
struct Cofree {
    A head;                 // the annotation at this node
    F<Cofree<F, A>> tail;   // one functor layer of annotated children
};
```
Complete-type reasoning: `F<Cofree>`'s recursive positions are
`Box<Cofree>` (§4), so the member is well-formed exactly like
`Fix<F>::inner`. Helpers: `extract(c) -> const A&` (returns `head`),
`unwrap_cofree(c) -> const F<Cofree<F,A>>&`.
`comonad_typeclass<Cofree<F, A>>` instance (partial specialization over
template-template `F` and `A`): `extract` = head; `duplicate(c)` =
`Cofree<F, Cofree<F,A>>{c, fmap(duplicate, c.tail)}`; `fmap(f, c)` =
`{f(c.head), layer_fmap(fmap-recursively, c.tail)}`.

### §5.4 Free monad (src/smd/fixpoint/free.hpp, S08)

```cpp
template <template <class> class F, class A>
struct Free {
    std::variant<A, F<Free<F, A>>> node;   // Pure | Roll
};
template <template <class> class F, class A>
constexpr Free<F, A> pure_free(A a);
template <template <class> class F, class A>
constexpr Free<F, A> roll_free(F<Free<F, A>> layer);
```
`monad_typeclass<Free<F, A>>` instance: `pure` = `pure_free`;
`bind(m, k)`: Pure a → `k(a)`; Roll layer → `roll_free(layer_fmap(bind
with k, layer))`.

## §6 Typeclass extensions

### §6.1 functor_typeclass instances for base functors (S01/S02)

Instance shape, following the OptionalFunctorImpl pattern but keyed on
layer instantiations (D2). For `NatF<A> = std::variant<Zero, Succ<A>>`:

```cpp
namespace smd::typeclass {
template <class A>
struct NatFFunctorImpl {
    template <class Fn>
    constexpr auto fmap(this auto&&, Fn&& fn, const NatF<A>& layer) {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const A&>>;
        return std::visit(smd::fixpoint::overloaded{
            [](const Zero&) -> NatF<B> { return Zero{}; },
            [&](const Succ<A>& s) -> NatF<B> {
                return Succ<B>{smd::fixpoint::make_box<B>(
                    std::invoke(fn, *s.pred))};
            },
        }, layer);
    }
};
template <class A>
struct NatFFunctorMap : Functor<NatFFunctorImpl<A>> {
    using NatFFunctorImpl<A>::fmap;
};
template <class A>
inline constexpr auto functor_typeclass<NatF<A>> = NatFFunctorMap<A>{};
} // namespace smd::typeclass
```
(Specializing `functor_typeclass` requires reopening `namespace
smd::typeclass`; the functor's own header does this after defining the
layer types.) Note the specialization key is really the underlying
`std::variant<Zero, Succ<A>>` — that is fine and deducible.

### §6.2 Lookup-based scheme overloads (S01)

Add to `recursion_schemes.hpp` overloads that drop the `fmap_fn`
parameter and use `layer_fmap` (§4): `fold_fix<Result>(alg, tree)`,
`unfold_fix<F>(coalg, seed)`, `refold<Result, F>(alg, coalg, seed)`.
Existing explicit-fmap overloads stay untouched (tests depend on them;
they also remain the escape hatch for one-off functors).

### §6.3 Comonad typeclass (smd/typeclass/comonad.hpp, S03)

Mirror the Monad CRTP:

```cpp
template <class Impl>
struct Comonad : protected Impl {
    // primitives an Impl must provide: extract, duplicate, fmap
    using Impl::extract;
    using Impl::duplicate;
    using Impl::fmap;
    // derived: extend f = fmap f . duplicate
    template <class F, class WA>
    constexpr auto extend(this auto&& self, F&& f, WA&& wa) {
        return self.fmap(std::forward<F>(f),
                         self.duplicate(std::forward<WA>(wa)));
    }
};
template <class T>
inline constexpr auto comonad_typeclass = std::false_type{};
```
Instances: `Identity<A>` (S03); env/product comonad
`std::pair<B, A>` — `extract` = `.second`, `duplicate(p)` =
`pair<B, pair<B,A>>{p.first, p}`, `fmap` maps `.second` — living in
`smd/typeclass/pair.hpp` together with pair's functor instance,
`map_first(f, p)` and `fanout(f, g)` (the D11 dual vocabulary; S03);
`Cofree<F, A>` (S07, §5.3).

### §6.4 Instance/spec summary table

| Type | functor | applicative | monad | comonad | step |
|------|---------|-------------|-------|---------|------|
| `Identity<A>` | ✓ | ✓ | ✓ | ✓ | S03 |
| `std::pair<B, A>` | ✓ (maps second) | — | — | ✓ (env) | S03 |
| `either<L, R>` | ✓ (maps Right) | — | ✓ (right-biased) | — | S03 |
| `Cofree<F, A>` | ✓ | — | — | ✓ | S07 |
| `Free<F, A>` | ✓ | — | ✓ | — | S08 |
| each base functor layer | ✓ | — | — | — | S01/S02 |

### §6.5 Explicit-instance `_with` scheme forms (D12)

Alongside the lookup overloads (§6.2), each scheme provides a `_with` form
that takes the functor instance by value as the first argument and threads it
down the recursion — the third of the three lookup modes (implicit lookup /
NTTP pinning / explicit object). Signatures:
`fold_fix_with(functor, alg, tree)`, `unfold_fix_with(functor, coalg, seed)`,
`refold_with(functor, alg, coalg, seed)`, `zygo_with(functor, helper, main,
tree)`. The instance need not be registered in `functor_typeclass`; validation
is a `static_assert` (`functor_instance_for`, `fmap.hpp`). See D12 and the full
design-surface record in `docs/explicit-typeclass-instance-dispatch.md`.

## §7 Scheme catalog

Uniform notation: `F` base functor (unary template, D3 for families),
`t : Fix<F>`, `unfix = unwrap_fix`, `fix = wrap_fix`, `fmapF` =
functor_typeclass fmap of the layer at hand, `φ` algebra, `ψ` coalgebra.
All functions are `constexpr` (D10). Haskell types are given for
cross-checking against recursion-schemes; the C++ signature is normative.

### §7.1 Classical recap (existing; S01 adds lookup overloads)

- `fold_fix  : (F<A> -> A) -> Fix<F> -> A` — cata. `φ ∘ fmapF(fold) ∘ unfix`.
- `unfold_fix: (S -> F<S>) -> S -> Fix<F>` — ana. `fix ∘ fmapF(unfold) ∘ ψ`.
- `refold    : (F<A> -> A) -> (S -> F<S>) -> S -> A` — hylo.
  `φ ∘ fmapF(refold) ∘ ψ`.

### §7.2 para and apo (S04) — para.hpp, apo.hpp

**para** `:: (f (t, a) -> a) -> t -> a`

Equation: `para φ = φ ∘ fmapF (λc. (c, para φ c)) ∘ unfix`.

```cpp
template <class Result, template <class> class F, class Algebra>
constexpr auto para(const Algebra& algebra, const Fix<F>& tree) -> Result;
// algebra : F<std::pair<Fix<F>, Result>> -> Result
```
The algebra sees, at every child position, both the *original subtree*
and the fold result for it.

**apo** `:: (a -> f (Either t a)) -> a -> t`

Equation: `apo ψ = fix ∘ fmapF (either id (apo ψ)) ∘ ψ` — with D4:
Left(Fix<F>) = graft finished subtree as-is, Right(Seed) = keep
unfolding. (Same side assignment as the Haskell type above.)

```cpp
template <template <class> class F, class Coalgebra, class Seed>
constexpr auto apo(const Coalgebra& coalgebra, const Seed& seed) -> Fix<F>;
// coalgebra : Seed -> F<either<Fix<F>, Seed>>
```
The worker copies the grafted subtree at the graft point; written with
`match`, it is oblivious to whether Left holds `Fix<F>` or
`const Fix<F>&` — so coalgebras may return
`F<either<const Fix<F>&, Seed>>` to graft without an intermediate copy
(§5.2 reference sides).

Examples: `para_pretty_print.cpp` — precedence-aware ExprF printer that
inspects original children to decide parenthesization;
`apo_sorted_insert.cpp` — insert into a sorted IntList, embedding the
untouched tail once the position is found.

### §7.3 zygo and mutu (S05) — zygo.hpp, mutu.hpp

**zygo** `:: (f b -> b) -> (f (b, a) -> a) -> t -> a`

Semantics: fold with an auxiliary "helper" fold running alongside; the
main algebra sees `(helper value, main value)` at child positions.
Implementation via pairing: `zygo f g = second ∘ fold_fix(λx.
pair(f(fmapF(first, x)), g(x)))` where the fold carrier is
`std::pair<Helper, Result>`.

```cpp
template <class Result, class Helper,
          template <class> class F, class HelperAlg, class MainAlg>
constexpr auto zygo(const HelperAlg& helper, const MainAlg& main,
                    const Fix<F>& tree) -> Result;
// helper : F<Helper> -> Helper
// main   : F<std::pair<Helper, Result>> -> Result
```

**mutu** (Fokkinga's mutumorphism) — two mutually recursive folds:

```cpp
template <class A, class B, template <class> class F,
          class AlgA, class AlgB>
constexpr auto mutu(const AlgA& alg_a, const AlgB& alg_b,
                    const Fix<F>& tree) -> std::pair<A, B>;
// alg_a : F<std::pair<A, B>> -> A ;  alg_b : F<std::pair<A, B>> -> B
```
Implementation: single `fold_fix` with carrier `pair<A,B>` and algebra
`λx. pair(alg_a(x), alg_b(x))` (the banana-split theorem).

Examples: `mutu_even_odd.cpp` — is-even/is-odd on Nat, each defined in
terms of the other; `zygo_balanced.cpp` — height-balanced check on a
TreeF where the helper algebra computes height.

### §7.4 hoist, prepro, postpro (S06) — prepro.hpp

**hoist** `:: (forall x. f x -> g x) -> Fix f -> Fix g` — apply a natural
transformation at every layer. `hoist e = fold_fix(fix_G ∘ e)`.

```cpp
template <template <class> class G, template <class> class F, class Nat>
constexpr auto hoist(const Nat& e, const Fix<F>& tree) -> Fix<G>;
```
(For the common endo case `G = F` the call is `hoist<F>(e, t)`.)

**prepro** (Fokkinga) `:: (forall x. f x -> f x) -> (f a -> a) -> t -> a`

Equation (recursion-schemes): `prepro e φ = φ ∘ fmapF (prepro e φ ∘
hoist<F>(e)) ∘ unfix`. The transformation is applied *cumulatively*: a
node at depth k has been rewritten k times. Document this cost.

**postpro** — dual: `postpro e ψ = fix ∘ fmapF (hoist<F>(e) ∘ postpro e ψ) ∘ ψ`.

```cpp
template <class Result, template <class> class F, class Nat, class Algebra>
constexpr auto prepro(const Nat& e, const Algebra& algebra,
                      const Fix<F>& tree) -> Result;
template <template <class> class F, class Nat, class Coalgebra, class Seed>
constexpr auto postpro(const Nat& e, const Coalgebra& coalgebra,
                       const Seed& seed) -> Fix<F>;
```

Example: `prepro_takewhile_sum.cpp` — sum an IntList through a natural
transformation that rewrites `Cons(x, rest)` to `Nil` when `x < 0`:
take-while fused into the fold.

### §7.5 histo and futu (S07, S08) — cofree.hpp+histo.hpp, free.hpp+futu.hpp

**histo** `:: (f (Cofree f a) -> a) -> t -> a` — course-of-values fold;
each child position carries the *entire annotated history* of that
subtree.

Implementation: `histo φ = extract ∘ fold_fix(λx. Cofree{φ(x), x})` with
fold carrier `Cofree<F, Result>` (the layer `x` is
`F<Cofree<F,Result>>`; it is both the algebra argument and the stored
tail).

```cpp
template <class Result, template <class> class F, class Algebra>
constexpr auto histo(const Algebra& algebra, const Fix<F>& tree) -> Result;
// algebra : F<Cofree<F, Result>> -> Result
```

**futu** `:: (a -> f (Free f a)) -> a -> t` — course-of-values unfold;
one step may emit several layers at once (the Free chunk), with seeds at
the leaves.

Equation: `futu ψ = fix ∘ fmapF(worker) ∘ ψ` where
`worker(Pure s) = futu ψ s;  worker(Roll layer) = fix(fmapF(worker, layer))`.

```cpp
template <template <class> class F, class Coalgebra, class Seed>
constexpr auto futu(const Coalgebra& coalgebra, const Seed& seed) -> Fix<F>;
// coalgebra : Seed -> F<Free<F, Seed>>
```

Examples: `histo_coin_change.cpp` — minimal-coin-count DP where the
algebra indexes into the Cofree history (the classic histomorphism
example); `futu_rle_decode.cpp` — run-length decoding onto IntList,
emitting `n` Cons layers per input pair.

### §7.6 dyna, codyna, chrono (S09) — chrono.hpp

All three are refold fusions — none materializes an intermediate
`Fix<F>`:

- **dyna** `:: (f (Cofree f b) -> b) -> (a -> f a) -> a -> b`
  = histo ∘ ana, fused: `dyna φ ψ = extract(refold<Cofree<F,B>>(λx.
  Cofree{φ(x), x}, ψ, seed))`.
- **codyna** `:: (f b -> b) -> (a -> f (Free f a)) -> a -> b`
  = cata ∘ futu, fused: `codyna φ ψ = refold<B>(φ, unroll, pure_free(seed))`
  where the coalgebra `unroll : Free<F,S> -> F<Free<F,S>>` is
  `Pure s ↦ ψ(s); Roll layer ↦ layer`.
- **chrono** `:: (f (Cofree f b) -> b) -> (a -> f (Free f a)) -> a -> b`
  = histo ∘ futu, fused: `chrono φ ψ = extract(refold<Cofree<F,B>>(λx.
  Cofree{φ(x), x}, unroll, pure_free(seed)))` with the same `unroll`.

```cpp
template <class Result, template <class> class F, class Algebra,
          class Coalgebra, class Seed>
constexpr auto dyna(const Algebra&, const Coalgebra&, const Seed&) -> Result;
// algebra : F<Cofree<F,Result>> -> Result ; coalgebra : Seed -> F<Seed>
// codyna: algebra : F<Result> -> Result ; coalgebra : Seed -> F<Free<F,Seed>>
// chrono: algebra : F<Cofree<F,Result>> -> Result ; coalgebra as codyna
```

Example: `dyna_fibonacci.cpp` — fib(n) directly from `int` seed, the
canonical dynamorphism.

### §7.7 Mendler-style: mcata, mhisto (S10) — mendler.hpp

Mendler algebras receive the recursive call as an explicit argument
instead of relying on fmap — **no Functor instance required**.

- **mcata** `:: (forall y. (y -> c) -> f y -> c) -> Fix f -> c`
  Equation: `mcata Φ t = Φ (mcata Φ) (unfix t)`.
- **mhisto** `:: (forall y. (y -> c) -> (y -> f y) -> f y -> c) -> Fix f -> c`
  Equation: `mhisto Φ t = Φ (mhisto Φ) unfix (unfix t)` — the second
  argument lets the algebra unroll deeper layers on demand
  (course-of-values without Cofree).

```cpp
template <class Result, template <class> class F, class MAlgebra>
constexpr auto mcata(const MAlgebra& phi, const Fix<F>& tree) -> Result;
// phi : (Recurse, const F<Fix<F>>&) -> Result, Recurse callable Fix<F> -> Result
template <class Result, template <class> class F, class MAlgebra>
constexpr auto mhisto(const MAlgebra& phi, const Fix<F>& tree) -> Result;
// phi : (Recurse, Unroll, const F<Fix<F>>&) -> Result,
//   Unroll callable Fix<F> -> const F<Fix<F>>&
```
Note in the header docs: C++ cannot enforce the rank-2 abstraction (the
algebra could inspect `Fix<F>` children directly); the discipline is by
convention — that *is* the pedagogical point of the example.

Examples: `mendler_eval.cpp` — ExprF evaluation via mcata (no
functor_typeclass instance used — demonstrate by evaluating a functor
that deliberately has none); mhisto Fibonacci in the tests.

### §7.8 Elgot (co)algebras (S11) — elgot.hpp

- **elgot** `:: (f a -> a) -> (b -> Either a (f b)) -> b -> a` —
  hylo whose coalgebra can short-circuit with a finished answer.
  Equation: `elgot φ ψ s = case ψ(s) of Left(a) → a; Right(layer) →
  φ(fmapF(elgot φ ψ, layer))`.
- **coelgot** `:: ((a, f b) -> b) -> (a -> f a) -> a -> b` — hylo whose
  algebra also sees the seed that produced the layer.
  Equation: `coelgot φ ψ s = φ(s, fmapF(coelgot φ ψ, ψ(s)))`.

```cpp
template <class Result, template <class> class F,
          class Algebra, class Coalgebra, class Seed>
constexpr auto elgot(const Algebra& algebra, const Coalgebra& coalgebra,
                     const Seed& seed) -> Result;
// coalgebra : Seed -> either<Result, F<Seed>>   (D4: Left = answer, stop)
// algebra   : F<Result> -> Result
template <class Result, template <class> class F,
          class Algebra, class Coalgebra, class Seed>
constexpr auto coelgot(const Algebra& algebra, const Coalgebra& coalgebra,
                       const Seed& seed) -> Result;
// coalgebra : Seed -> F<Seed>
// algebra   : (const Seed&, F<Result>) -> Result       (pair flattened)
```

Example: `elgot_shortcircuit.cpp` — product of a list of ints that
bails out with 0 the moment a 0 is seen, never expanding the rest.

### §7.9 Distributive laws (S12) — dist_laws.hpp

Each is a polymorphic function object; the comment block gives its
Haskell type. Transcribed equations (verify via §9 laws — D8):

```cpp
// distCata :: f (Identity a) -> Identity (f a)
struct dist_cata_t {
    template <class Layer>          // Layer = F<Identity<A>>
    constexpr auto operator()(const Layer& l) const
    { return Identity{layer_fmap([](const auto& i){ return i.value; }, l)}; }
};
inline constexpr dist_cata_t dist_cata{};

// distAna :: Identity (f a) -> f (Identity a)
//   fmap(wrap-in-Identity) over the layer inside.
// distHisto :: f (Cofree f a) -> Cofree f (f a)
//   head = fmapF(extract), tail = fmapF(λc. distHisto(c.tail)) — i.e.
//   distHisto l = Cofree{ fmapF(extract, l),
//                         fmapF(λc. dist_histo(c.tail), l) }.
// distFutu :: Free f (f a) -> f (Free f a)
//   Pure layer → fmapF(pure_free, layer)
//   Roll layer → fmapF(roll_free ∘ dist_futu, layer)
// distZygo :: (f b -> b) -> f (b, a) -> (b, f a)     (a factory: dist_zygo(f))
//   λl. pair(f(fmapF(first, l)), fmapF(second, l))
// distPara = dist_zygo(wrap_fix)                      (b = Fix<F>)
// distApo :: Either t (f a) -> f (Either t a)         (either, D4)
//   Right(layer) → fmapF(make_right, layer)
//   Left(t)      → fmapF(λchild. make_left(child), unfix(t))
// distGApo :: (b -> f b) -> Either b (f a) -> f (Either b a)
//   generalizes distApo with a coalgebra for the Left side
//   (Left(b) → fmapF(make_left, coalg(b)) — this is where either's
//   symmetric construction pays off; with std::expected it would have
//   been transform_error gymnastics).
```

### §7.10 gcata, gana, ghylo (S13, S14) — generalized.hpp

**gcata** `:: Comonad w => (forall x. f (w x) -> w (f x)) -> (f (w a) -> a) -> t -> a`

Definition (recursion-schemes): `gcata k g = g (extract (c t))` where

```
c : Fix<F> -> W<F<W<A>>>
c = k ∘ fmapF (duplicate ∘ fmapW g ∘ c) ∘ unfix
```

C++ shape (D5 — `WResult` = `W<Result>` explicit so the worker's return
type `C` is nameable):

```cpp
template <class Result, class WResult,
          template <class> class F, class Dist, class GAlgebra>
constexpr auto gcata(const Dist& dist, const GAlgebra& algebra,
                     const Fix<F>& tree) -> Result;
// dist : F<W<X>> -> W<F<X>> for all X; algebra : F<WResult> -> Result
```
Implementation notes: the comonad ops come from
`comonad_typeclass<std::remove_cvref_t<decltype(w)>>` at each use site.
The worker return type is computable up front:
`WWR = decltype(comonad.duplicate(declval<WResult>()))`;
`C = decltype(dist(declval<F<WWR>>()))`. Write the worker as a private
function template with that explicit return type; recursion then
type-checks.

**gana** `:: Monad m => (forall x. m (f x) -> f (m x)) -> (a -> f (m a)) -> a -> t`

`gana k ψ = a(pure_m(ψ(seed)))` where

```
a : M<F<M<S>>> -> Fix<F>
a = fix ∘ fmapF (a ∘ fmapM ψ' ∘ join) ∘ k
     where ψ' = ψ lifted: fmapM ψ : M<S> -> M<F<M<S>>>
```
(i.e. `a(m) = fix(fmapF(λmms. a(fmapM(ψ, join(mms))), k(m)))`).
Monad ops via `monad_typeclass` lookups; explicit `MSeed` template
parameter mirrors `WResult`.

```cpp
template <template <class> class F, class MSeed,
          class Dist, class GCoalgebra, class Seed>
constexpr auto gana(const Dist& dist, const GCoalgebra& coalgebra,
                    const Seed& seed) -> Fix<F>;
// dist : M<F<X>> -> F<M<X>> for all X; coalgebra : Seed -> F<MSeed>
```

**ghylo** — the fused generalization; implement as
`gcata`-after-`gana` semantics *fused through refold*, or (acceptable
first cut, gated by the same laws) literally
`ghylo w m φ ψ = gcata<Result,WResult>(w, φ, gana<F,MSeed>(m, ψ, seed))`
materializing the middle — then fuse if straightforward. Record which
variant shipped.

Equivalence gates (§9): gcata(dist_cata) ≡ fold_fix; gcata(dist_histo) ≡
histo; gcata(dist_zygo(f)) ≡ zygo(f); gana(dist_ana) ≡ unfold_fix;
gana(dist_apo) ≡ apo; gana(dist_futu) ≡ futu; ghylo(dist_cata, dist_ana)
≡ refold; ghylo(dist_histo, dist_ana) ≡ dyna; ghylo(dist_cata,
dist_futu) ≡ codyna; ghylo(dist_histo, dist_futu) ≡ chrono.

### §7.11 gprepro, gpostpro, zygo_histo_prepro (S15)

**gprepro** `:: Comonad w => dist -> (forall x. f x -> f x) -> (f (w a) -> a) -> t -> a`

`gprepro k e φ = φ(extract(c t))` where
`c = fmapW φ?` — no: `c = k ∘ fmapF (duplicate ∘ fmapW φ ∘ c ∘ hoist<F>(e)) ∘ unfix`
and result `= φ(extract(c t))`. Cross-check by law:
`gprepro(dist_cata, e, φ) ≡ prepro(e, φ)` and
`gprepro(k, identity-nat, φ) ≡ gcata(k, φ)` — trust the laws (D8).

**gpostpro** — dual, gated by `gpostpro(dist_ana, e, ψ) ≡ postpro(e, ψ)`.

**zygo_histo_prepro** — Kmett's famous capstone:
zygomorphic histomorphism with a prepromorphism pass. Implement it as
`gprepro` with the concrete composed comonad `W<X> = std::pair<Helper,
Cofree<F, X>>` and a hand-written distributive law for exactly that W
(a one-off `dist_zygo_histo(f)` function object; do NOT build general
comonad-transformer machinery):

```cpp
template <class Result, class Helper, template <class> class F,
          class HelperAlg, class Nat, class MainAlg>
constexpr auto zygo_histo_prepro(const HelperAlg& f, const Nat& e,
                                 const MainAlg& g, const Fix<F>& tree)
    -> Result;
// f : F<Helper> -> Helper ; e : natural transformation F<X> -> F<X>
// g : F<std::pair<Helper, Cofree<F, Result>>> -> Result
```
Gate: with `e = identity`, `f` ignored by `g`, and `g` using only
`extract` of the Cofree — degenerates to `fold_fix`. Plus one real use in
`generalized_tour.cpp`.

## §8 File and target layout

New headers, all under `src/smd/fixpoint/` unless noted, each added to
the `smd_fixpoint_headers` FILE_SET (or `smd_typeclass_headers` for
typeclass module files) in the step that creates them; each tested in a
sibling `.t.cpp` added to `smd_fixpoint_test` (resp.
`smd_typeclass_test`):

| Header | Contents | Step |
|--------|----------|------|
| `fmap.hpp` | `layer_fmap` helper (§4) | S01 |
| `functors.hpp` | NatF, ListF<E,·>, TreeF<E,·>, ExprF + instances + smart ctors + to/from std converters | S02 |
| `smd/typeclass/identity.hpp` | Identity + 4 instances | S03 |
| `smd/typeclass/either.hpp` | either (+ P2988 reference sides) + instances + match/fanin/map_left | S03 |
| `smd/typeclass/pair.hpp` | pair functor + env comonad instances + map_first/fanout | S03 |
| `smd/typeclass/comonad.hpp` | Comonad CRTP + lookup (instances live with their types) | S03 |
| `para.hpp`, `apo.hpp` | §7.2 | S04 |
| `zygo.hpp`, `mutu.hpp` | §7.3 | S05 |
| `prepro.hpp` | hoist, prepro, postpro | S06 |
| `cofree.hpp`, `histo.hpp` | §5.3, §7.5 | S07 |
| `free.hpp`, `futu.hpp` | §5.4, §7.5 | S08 |
| `chrono.hpp` | dyna, codyna, chrono | S09 |
| `mendler.hpp` | mcata, mhisto | S10 |
| `elgot.hpp` | elgot, coelgot | S11 |
| `dist_laws.hpp` | §7.9 | S12 |
| `generalized.hpp` | gcata, gana, ghylo, gprepro, gpostpro, zygo_histo_prepro | S13–S15 |
| `schemes.hpp` | umbrella include | S16 |

Examples: one `.cpp` per §7 family in `src/examples/`, each an
`add_executable` + `target_link_libraries(... fixpoint.fixpoint)` +
`install(... EXCLUDE_FROM_ALL)` block following the existing
`fixpoint_tree_example` pattern. Examples must *run* and print their
result; the step gate executes them.

## §9 Testing strategy — the equivalence-law suite

Two kinds of tests, both Catch2, both required per scheme:

1. **Behavioral**: the scheme computes the right answer on its motivating
   example structure (small, hand-checkable).
2. **Equivalence laws** (D8) — each new scheme is pinned to an
   already-tested scheme:
   - `para(λl. φ(fmapF(second,l)))` ≡ `fold_fix(φ)` (ignore originals)
   - `apo(λs. fmapF(value, ψ(s)))` ≡ `unfold_fix(ψ)` (never short-circuit)
   - `zygo(f, λl. φ(fmapF(second,l)))` ≡ `fold_fix(φ)` (ignore helper)
   - `mutu(f, g).first/second` consistency with the paired `fold_fix`
   - `prepro(identity_nat, φ)` ≡ `fold_fix(φ)`; `postpro(identity_nat, ψ)` ≡ `unfold_fix(ψ)`
   - `histo(φ ∘ heads-only)` ≡ `fold_fix(φ)`; histo-fib ≡ naive fib
   - `futu(fmapF(pure_free) ∘ ψ)` ≡ `unfold_fix(ψ)`
   - `dyna(φ, ψ)` ≡ `histo(φ, unfold_fix(ψ, seed))`
   - `codyna(φ, ψ)` ≡ `fold_fix(φ, futu(ψ, seed))`
   - `chrono(φ, ψ)` ≡ `histo(φ, futu(ψ, seed))`
   - `mcata(λ(rec,l). φ(fmapF(rec,l)))` ≡ `fold_fix(φ)`; mhisto-fib ≡ fib
   - `elgot(φ, value ∘ ψ)` ≡ `refold(φ, ψ)`; `coelgot(ignore-seed(φ), ψ)` ≡ `refold(φ, ψ)`
   - the full §7.10 gcata/gana/ghylo recovery table
   - §7.11 degeneracies
3. **constexpr** (D10): per scheme, one `static_assert` evaluating it on
   a small structure at compile time.

Property-style loops (e.g. `for n in 0..10`) over the Nat/List fixtures
are encouraged — see the existing `Refold - EquivalentToFoldOfUnfold`
test for the house pattern.

## §10 Examples catalog (src/examples/)

| Executable | Scheme | Story |
|------------|--------|-------|
| `para_pretty_print` | para | minimal-parens expression printing |
| `apo_sorted_insert` | apo | ordered insert, tail embedded untouched |
| `zygo_balanced` | zygo | height-balanced check, height as helper |
| `mutu_even_odd` | mutu | mutually recursive even/odd on Nat |
| `prepro_takewhile_sum` | prepro | take-while fused into sum |
| `histo_coin_change` | histo | minimal coins DP over the history |
| `futu_rle_decode` | futu | run-length decode, multi-layer steps |
| `dyna_fibonacci` | dyna | fib from int seed, no tree materialized |
| `mendler_eval` | mcata | eval without any Functor instance |
| `elgot_shortcircuit` | elgot | product-with-zero bailout |
| `generalized_tour` | gcata/gana/ghylo/zygo_histo_prepro | recover the zoo from dist laws |

Each example: ≤ ~120 lines, heavily commented, prints inputs and outputs
with `std::println`, exits 0. These are documentation first.

## §11 Non-goals

- Performance tuning, sharing, laziness emulation, memo tables beyond
  what a scheme's own carrier provides (D6).
- Stack safety / iterative rewrites (D7).
- General comonad-transformer machinery (§7.11 uses one concrete W).
- Migrating `Box` to `std::indirect` (D10) — revisit after the catalog
  is complete.
- GADTs/indexed functors, `Recursive`/`Corecursive` typeclasses over
  arbitrary containers (we fold `Fix<F>` only), template metaprogram
  variants (`compile-time-scheme` explores that separately).
- CI workflow changes; the gate is local `make TOOLCHAIN=gcc-16 test`.
