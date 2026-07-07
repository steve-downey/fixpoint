# Recursion schemes — usage catalog

This is the user-facing catalog for `smd::fixpoint`'s recursion-schemes
library: one section per scheme, in the order they appear in
[`docs/recursion-schemes-design.md`](recursion-schemes-design.md) §7. Each
section gives the signature *as landed* (copied from the header, not from
the design doc's original sketch — a handful of signatures drifted via the
deviations logged in [`ops/DEVIATIONS.md`](../ops/DEVIATIONS.md)), a
one-sentence "when you want it", the recursive equation it implements, a
short usage snippet lifted from the matching example, and a pointer to the
runnable example.

Everything below is available from a single umbrella include:

```cpp
#include <smd/fixpoint/schemes.hpp>
```

Build and run the examples with:

```bash
make TOOLCHAIN=gcc-16 test
.build/build-gcc-16/src/examples/Asan/<example_name>
```

All schemes are `constexpr`-capable (design D10) and take their fold/unfold
carrier(s) as leading, explicit template parameters (design D5) — C++
cannot infer a carrier type through a recursive call the way Haskell can.

---

## §7.1 The classical recap — `fold_fix`, `unfold_fix`, `refold`

Header: `smd/fixpoint/recursion_schemes.hpp`. These predate this catalog
(the library's existing cata/ana/hylo, renamed per design D1) but every
other scheme here is described relative to them.

```cpp
template <class Result, template <class> class F, class Algebra>
constexpr auto fold_fix(const Algebra &algebra, const Fix<F> &tree) -> Result;

template <template <class> class F, class Coalgebra, class Seed>
constexpr auto unfold_fix(const Coalgebra &coalgebra, const Seed &seed) -> Fix<F>;

template <class Result, template <class> class F, class Algebra,
          class Coalgebra, class Seed>
constexpr auto refold(const Algebra &algebra, const Coalgebra &coalgebra,
                      const Seed &seed) -> Result;
```

Use `fold_fix` to consume a `Fix<F>` tree bottom-up, `unfold_fix` to build
one top-down from a seed, and `refold` to do both in one pass without ever
materializing the intermediate tree. Every scheme's own degeneracy law (§9)
is stated against one of these three.

---

## §7.2 `para` and `apo` — Fokkinga's classical extensions

Header: `smd/fixpoint/para.hpp`, `smd/fixpoint/apo.hpp`.

### para

```cpp
template <class Result, template <class> class F, class Algebra>
constexpr auto para(const Algebra &algebra, const Fix<F> &tree) -> Result;
```

Use it when an algebra needs to see a child's *original, un-folded*
subtree alongside its fold result — e.g. deciding whether to parenthesize
based on what a child actually is, not just what it evaluates to.

Equation: `para φ = φ ∘ fmapF (λc. (c, para φ c)) ∘ unfix` — the pair built
at each child is `{original_subtree, fold_result}` (original first).

```cpp
// Each Mul child arrives as {original subtree, its rendered string};
// only inspect .first when deciding whether .second needs parens.
auto pretty = [](const ExprF<std::pair<Expr, std::string>> &layer)
    -> std::string {
    return std::visit(
        overloaded{
            [](const Const<std::pair<Expr, std::string>> &c) {
                return std::to_string(c.val);
            },
            [](const Add<std::pair<Expr, std::string>> &a) {
                return a.left->second + " + " + a.right->second;
            },
            [](const Mul<std::pair<Expr, std::string>> &m) {
                auto render = [](const auto &child) {
                    return is_add(child.first) ? "(" + child.second + ")"
                                               : child.second;
                };
                return render(*m.left) + " * " + render(*m.right);
            },
        },
        layer);
};

std::println("{}", para<std::string>(pretty, tree)); // "2 * 3 + 4"
```

Example: `para_pretty_print` — minimal-parens expression pretty-printing.

### apo

```cpp
template <template <class> class F, class Coalgebra, class Seed>
constexpr auto apo(const Coalgebra &coalgebra, const Seed &seed) -> Fix<F>;
```

Use it when unfolding may short-circuit any branch by grafting an
already-built subtree instead of continuing to expand — e.g. once an
insertion point is found, graft the untouched remainder back in as-is.

Equation: `apo ψ = fix ∘ fmapF (either id (apo ψ)) ∘ ψ`, with the
coalgebra returning `F<either<Fix<F>, Seed>>` (D4: Left = finished subtree
to graft, Right = keep unfolding). Coalgebras may return
`F<either<const Fix<F>&, Seed>>` for a zero-copy graft.

```cpp
auto make_insert_coalgebra(int value) {
    return [value](const IntList &remaining)
               -> IntListF<either<IntList, IntList>> {
        // ... walk `remaining`; once the insertion point is found,
        // make_left<IntList>(remaining) grafts the rest untouched;
        // otherwise make_right<IntList>(*c.tail) keeps unfolding.
    };
}

IntList inserted = apo<IntListF>(make_insert_coalgebra(5), sorted);
```

Example: `apo_sorted_insert` — ordered insert into a sorted list, the
untouched tail grafted rather than rebuilt.

---

## §7.3 `zygo` and `mutu` — auxiliary and mutual folds

Header: `smd/fixpoint/zygo.hpp`, `smd/fixpoint/mutu.hpp`.

### zygo

```cpp
template <class Result, class Helper, template <class> class F,
          class HelperAlg, class MainAlg>
constexpr auto zygo(const HelperAlg &helper, const MainAlg &main,
                    const Fix<F> &tree) -> Result;
```

Use it when a fold needs a second, auxiliary fold running alongside it,
whose value the main algebra consults at every child — e.g. checking
height-balance, where the main predicate needs each child's *height*.

Equation: `zygo f g = second ∘ fold_fix(λx. pair(f(fmapF(first, x)), g(x)))`.
Carrier convention: `std::pair<Helper, Result>` — **helper first, main
second** (matches `dist_zygo`, S12).

```cpp
auto height = [](const IntTreeF<int> &layer) -> int { /* ... */ };
auto is_balanced = [](const IntTreeF<std::pair<int, bool>> &layer) -> bool {
    // n.left->first / n.right->first are the children's heights;
    // n.left->second / n.right->second are their balanced-ness.
};

std::println("{}", zygo<bool, int>(height, is_balanced, tree));
```

Example: `zygo_balanced` — height-balanced check, height as the helper
fold.

### mutu

```cpp
template <class A, class B, template <class> class F, class AlgA, class AlgB>
constexpr auto mutu(const AlgA &alg_a, const AlgB &alg_b,
                    const Fix<F> &tree) -> std::pair<A, B>;
```

Use it for two mutually recursive folds where each needs the *other's*
value at every child — e.g. is-even/is-odd, each defined in terms of the
other with no modulo arithmetic.

Equation: `mutu f g = fold_fix(λx. pair(f(x), g(x)))` — unlike zygo, both
algebras see the *same* `F<std::pair<A,B>>` layer directly, as peers (no
projection).

```cpp
auto alg_even = [](const NatF<std::pair<bool, bool>> &layer) -> bool {
    // Succ is even iff its predecessor (s.pred->second) is odd.
};
auto alg_odd = [](const NatF<std::pair<bool, bool>> &layer) -> bool {
    // Succ is odd iff its predecessor (s.pred->first) is even.
};

auto [is_even, is_odd] = mutu<bool, bool>(alg_even, alg_odd, nat);
```

Example: `mutu_even_odd` — mutually recursive even/odd on `Nat`.

---

## §7.4 `hoist`, `prepro`, `postpro` — natural-transformation plumbing

Header: `smd/fixpoint/prepro.hpp`.

```cpp
template <template <class> class G, template <class> class F, class Nat>
constexpr auto hoist(const Nat &e, const Fix<F> &tree) -> Fix<G>;

template <class Result, template <class> class F, class Nat, class Algebra>
constexpr auto prepro(const Nat &e, const Algebra &algebra,
                      const Fix<F> &tree) -> Result;

template <template <class> class F, class Nat, class Coalgebra, class Seed>
constexpr auto postpro(const Nat &e, const Coalgebra &coalgebra,
                       const Seed &seed) -> Fix<F>;
```

`hoist` retags every layer of a tree through a natural transformation
`e : F<X> -> G<X>` (for every `X`); the common endo case (`G == F`) is
called `hoist<F>(e, t)`. `prepro` fuses a *cumulative* application of `e`
into a fold, applied to each child's whole subtree on the way down before
recursing — e.g. take-while fused into a sum, so a negative element (and
everything past it) never reaches the summing algebra. `postpro` is the
dual, applying `e` to each already-unfolded subtree before grafting it in
on the way out.

Equations: `hoist e = fold_fix(wrap_fix<G> ∘ e)`;
`prepro e φ = φ ∘ fmapF (prepro e φ ∘ hoist<F>(e)) ∘ unfix`;
`postpro e ψ = fix ∘ fmapF (hoist<F>(e) ∘ postpro e ψ) ∘ ψ`.

Natural transformations must be polymorphic function objects with a
*templated* call operator (design §4) — callable on `F<X>` for every `X`,
not just one concrete carrier:

```cpp
struct take_while_positive {
    template <class A>
    constexpr auto operator()(const IntListF<A> &layer) const -> IntListF<A> {
        // Cons(x, rest) -> Nil once x < 0; Nil -> Nil.
    }
};

int sum = prepro<int>(take_while_positive{}, sum_algebra, list);
```

Example: `prepro_takewhile_sum` — take-while fused into a fold; prints
`sum(take_while(>=0, [3, 4, -1, 5])) = 7`.

---

## §7.5 `histo` and `futu` — course-of-values (co)recursion

Header: `smd/fixpoint/cofree.hpp` + `smd/fixpoint/histo.hpp`,
`smd/fixpoint/free.hpp` + `smd/fixpoint/futu.hpp`.

### histo

```cpp
template <class Result, template <class> class F, class Algebra>
constexpr auto histo(const Algebra &algebra, const Fix<F> &tree) -> Result;
```

Use it when an algebra needs to look arbitrarily far back into
already-computed history, not just its immediate child's result — the
classic example is coin-change DP, where computing `minCoins(n)` needs
`minCoins(n-1)`, `minCoins(n-4)`, and `minCoins(n-5)` simultaneously.

Equation: `histo φ = extract ∘ fold_fix(λx. Cofree{φ(x), x})`, carrier
`Cofree<F, Result>` (`cofree.hpp`) — the algebra's argument
`F<Cofree<F,Result>>` carries each child's *entire* annotated history, not
just its folded value.

```cpp
using History = Cofree<NatF, int>;

// look_back(c, k) walks k layers further back through an already-computed
// history, returning minCoins(m - k) or nullopt if it runs off the front.

auto min_coins_algebra = [](const NatF<History> &layer) -> int {
    // s.pred->head is minCoins(n-1); look_back(pred, 3)/look_back(pred, 4)
    // reach minCoins(n-4)/minCoins(n-5) without ever re-folding.
};

int result = histo<int>(min_coins_algebra, nat_from_int(n));
```

Example: `histo_coin_change` — minimal-coins DP over the history; prints
`minCoins(8, coins={1,4,5}) = 2`, `minCoins(12, coins={1,4,5}) = 3`.

### futu

```cpp
template <template <class> class F, class Coalgebra, class Seed>
constexpr auto futu(const Coalgebra &coalgebra, const Seed &seed) -> Fix<F>;
```

Use it when a single unfold step needs to emit *several* layers at once,
not just one — e.g. run-length decoding, where one `{count, value}` pair
expands into `count` list layers in a single coalgebra call.

Equation: `futu ψ = fix ∘ fmapF(worker) ∘ ψ`, coalgebra
`Seed -> F<Free<F, Seed>>` (`free.hpp`) — a whole *chunk* of `F`-layers,
built with nested `roll_free`, with fresh seeds at the chunk's `Pure`
leaves for `futu` to keep unfolding from.

```cpp
auto make_rle_coalgebra(const std::vector<std::pair<int, int>> &pairs) {
    return [&pairs](std::size_t i) -> IntListF<IndexFree> {
        // Emits `count` Cons(value) layers as one Free chunk (via
        // roll_free), resuming at the next pair's index.
    };
}

IntList decoded = futu<IntListF>(make_rle_coalgebra(pairs), std::size_t{0});
```

Example: `futu_rle_decode` — run-length decode; prints
`decoded: [7, 7, 1, 1, 1]` for input `[(2, 7), (3, 1)]`.

---

## §7.6 `dyna`, `codyna`, `chrono` — fused course-of-values refolds

Header: `smd/fixpoint/chrono.hpp`.

```cpp
template <class Result, template <class> class F, class Algebra,
          class Coalgebra, class Seed>
constexpr auto dyna(const Algebra &algebra, const Coalgebra &coalgebra,
                    const Seed &seed) -> Result;      // histo . ana, fused

template <class Result, template <class> class F, class Algebra,
          class Coalgebra, class Seed>
constexpr auto codyna(const Algebra &algebra, const Coalgebra &coalgebra,
                      const Seed &seed) -> Result;    // cata . futu, fused

template <class Result, template <class> class F, class Algebra,
          class Coalgebra, class Seed>
constexpr auto chrono(const Algebra &algebra, const Coalgebra &coalgebra,
                      const Seed &seed) -> Result;    // histo . futu, fused
```

Use these to get `histo`'s/`futu`'s course-of-values power directly from a
seed, in a single `refold` pass, without ever materializing the
intermediate `Fix<F>` tree — e.g. computing Fibonacci from an `int`
directly, where the `Cofree<NatF,int>` "history" *is* the DP table.

```cpp
auto countdown(int m) -> NatF<int> { /* n -> n-1 -> ... -> 0 */ }
auto fib_algebra(const NatF<Cofree<NatF, int>> &layer) -> int {
    // c.head == fib(n-1); if c's tail is Succ(cc), cc.head == fib(n-2).
}

int fib_n = dyna<int, NatF>(fib_algebra, countdown, n);
```

Example: `dyna_fibonacci` — fib(n) for n in 0..10, no `Nat` tree ever
built.

---

## §7.7 Mendler-style `mcata` and `mhisto`

Header: `smd/fixpoint/mendler.hpp`.

```cpp
template <class Result, template <class> class F, class MAlgebra>
constexpr auto mcata(const MAlgebra &phi, const Fix<F> &tree) -> Result;

template <class Result, template <class> class F, class MAlgebra>
constexpr auto mhisto(const MAlgebra &phi, const Fix<F> &tree) -> Result;
```

Use these when you want to fold a base functor `F` with **no
`functor_typeclass<F<...>>` instance at all** — the algebra receives the
recursive call itself (`recurse`, and for `mhisto` also `unroll`) as an
explicit callable argument, and decides which children to fold and how, so
no `fmap` is ever looked up.

Equations: `mcata Φ t = Φ (mcata Φ) (unfix t)`;
`mhisto Φ t = Φ (mhisto Φ) unfix (unfix t)`.

```cpp
// (Recurse, const ExprF<Expr>&) -> int. No functor_typeclass<ExprF<A>>
// lookup happens anywhere in this call.
auto eval_via_mcata = [](auto recurse, const ExprF<Expr> &layer) -> int {
    return std::visit(
        overloaded{
            [](const Const<Expr> &c) { return c.val; },
            [&](const Add<Expr> &a) { return recurse(*a.left) + recurse(*a.right); },
            [&](const Mul<Expr> &m) { return recurse(*m.left) * recurse(*m.right); },
        },
        layer);
};

std::println("mcata eval: {}", mcata<int, ExprF>(eval_via_mcata, e));
```

Example: `mendler_eval` — expression evaluation via `mcata`, with no
`Functor` instance in sight.

---

## §7.8 Elgot (co)algebras — `elgot` and `coelgot`

Header: `smd/fixpoint/elgot.hpp`.

```cpp
template <class Result, template <class> class F, class Algebra,
          class Coalgebra, class Seed>
constexpr auto elgot(const Algebra &algebra, const Coalgebra &coalgebra,
                     const Seed &seed) -> Result;

template <class Result, template <class> class F, class Algebra,
          class Coalgebra, class Seed>
constexpr auto coelgot(const Algebra &algebra, const Coalgebra &coalgebra,
                       const Seed &seed) -> Result;
```

`elgot` is a `refold` whose *coalgebra* gets an extra say: it may
short-circuit any seed (including the very first) with a finished answer,
skipping all further recursion below it — e.g. multiplying a list of ints,
bailing out the instant a `0` is seen. `coelgot`'s *algebra* instead
additionally receives the seed that produced the current layer, alongside
the already-folded children; it never short-circuits.

Equations: `elgot φ ψ s = case ψ(s) of Left(a) -> a; Right(l) -> φ(fmapF(elgot φ ψ, l))`
(D4: Left = answer/stop, Right = one more layer/continue — same convention
as `apo`); `coelgot φ ψ s = φ(s, fmapF(coelgot φ ψ, ψ(s)))`.

```cpp
auto coalgebra = [&](std::size_t i) -> either<int, IntListF<std::size_t>> {
    ++examined;
    if (i >= values.size()) return make_right<int>(IntListF<std::size_t>{Nil<int>{}});
    if (values[i] == 0) return make_left<IntListF<std::size_t>>(0); // stop
    return make_right<int>(IntListF<std::size_t>{Cons<int, std::size_t>{
        values[i], make_box<std::size_t>(i + 1)}});
};

int product = elgot<int, IntListF>(product_algebra, coalgebra, std::size_t{0});
```

Example: `elgot_shortcircuit` — product-with-zero bailout; prints
`elements examined: 3 / 6` alongside the product, since the product alone
(0) can't show whether the short-circuit actually happened.

---

## §7.9 Distributive laws

Header: `smd/fixpoint/dist_laws.hpp`. Not schemes themselves — the
polymorphic function objects `dist(F<W<X>>) -> W<F<X>>` (comonadic) or
`dist(M<F<X>>) -> F<M<X>>` (monadic) that parameterize gcata/gana/ghylo
(§7.10) below. Per [`DEV-02`](../ops/DEVIATIONS.md), not every one of
these is callable with zero explicit template arguments the way design §4
first suggested — three need their functor named explicitly, and two need
an explicit result-element type, both genuine C++ deduction limits (a
template-template parameter can't be recovered from an already-elaborated
alias-template application; `make_left`'s result-side type is never
deducible from its argument).

| Object | Signature | Call convention |
|---|---|---|
| `dist_cata` | `F<Identity<A>> -> Identity<F<A>>` | `dist_cata(layer)` |
| `dist_ana` | `Identity<F<A>> -> F<Identity<A>>` | `dist_ana(ident)` |
| `dist_histo<F>` | `F<Cofree<F,A>> -> Cofree<F, F<A>>` | `dist_histo<F>(layer)` — **F explicit** |
| `dist_futu<F>` | `Free<F, F<A>> -> F<Free<F, A>>` | `dist_futu<F>(chunk)` — **F explicit** |
| `dist_zygo(helper)` | `F<pair<B,X>> -> pair<B, F<X>>` | `dist_zygo(helper_algebra)` — factory |
| `dist_para<F>` | `F<pair<Fix<F>,X>> -> pair<Fix<F>, F<X>>` | `dist_para<F>(layer)` — **F explicit** |
| `dist_apo` | `either<Fix<F>,F<X>> -> F<either<Fix<F>,X>>` | `dist_apo.operator()<X>(e)` — **X explicit** |
| `dist_gapo(coalg)` | `either<B,F<X>> -> F<either<B,X>>` | `dist_gapo(coalg).operator()<X>(e)` — factory, **X explicit** |

No dedicated example; exercised end-to-end by `generalized_tour` (§7.10/11
below) and pinned by `dist_laws.t.cpp`'s own shape/naturality tests.

---

## §7.10 `gcata`, `gana`, `ghylo` — comonadic/monadic generalizations

Header: `smd/fixpoint/generalized.hpp`.

```cpp
template <class Result, class WResult, template <class> class F, class Dist,
          class GAlgebra>
constexpr auto gcata(const Dist &dist, const GAlgebra &algebra,
                     const Fix<F> &tree) -> Result;

template <template <class> class F, class MSeed, class Dist,
          class GCoalgebra, class Seed>
constexpr auto gana(const Dist &dist, const GCoalgebra &coalgebra,
                    const Seed &seed) -> Fix<F>;

template <class Result, class WResult, template <class> class F, class MSeed,
          class WDist, class GAlgebra, class MDist, class GCoalgebra,
          class Seed>
constexpr auto ghylo(const WDist &w_dist, const GAlgebra &algebra,
                     const MDist &m_dist, const GCoalgebra &coalgebra,
                     const Seed &seed) -> Result;
```

Use these to see *why* `fold_fix`/`histo`/`zygo`/`para` (via `gcata`) and
`unfold_fix`/`apo`/`futu` (via `gana`) are all one shape underneath: each
recovers from nothing but a distributive law (§7.9) plus a comonad
(`comonad_typeclass`, for `gcata`) or monad (`monad_typeclass`, for
`gana`). `ghylo` fuses both sides (shipped **materializing**: `gcata`
applied to `gana`'s output — it passed every recovery law cleanly on the
first attempt, so no further fusion was attempted; see design §11's
non-goals on performance). Ready-made recovery aliases:
`cata_via_gcata`, `histo_via_gcata`, `zygo_via_gcata`, `para_via_gcata`,
`ana_via_gana`, `apo_via_gana`, `futu_via_gana`.

Equations (design §7.10): `gcata k g = g(extract(c t))` where
`c = k . fmapF(duplicate . fmapW g . c) . unfix`; `gana k ψ = a(pure(ψ(seed)))`
where `a(m) = fix(fmapF(λmms. a(fmapM(ψ,join(mms))), k(m)))`.

```cpp
// Same answer, two ways: the classical scheme directly, and gcata driven
// by the matching distributive law.
int specialized = fold_fix<int>(nat_count_algebra, nat);
int generalized = cata_via_gcata<int>(nat_count_algebra, nat);
// specialized == generalized

int fib_specialized = dyna<int, NatF>(fib_algebra, countdown, 10);
int fib_generalized = ghylo<int, Cofree<NatF, int>, NatF, Identity<int>>(
    dist_histo<NatF>, fib_algebra, dist_ana, ana_coalgebra_prime, 10);
// fib_specialized == fib_generalized
```

Example: `generalized_tour` — walks fold_fix/histo/dyna against their
`gcata`/`ghylo` equivalents, printing each "specialized vs generalized"
pair (and failing loudly, exit 1, on any mismatch).

---

## §7.11 `gprepro`, `gpostpro`, `zygo_histo_prepro` — the capstone

Header: `smd/fixpoint/generalized.hpp`.

```cpp
template <class Result, class WResult, template <class> class F, class Dist,
          class Nat, class GAlgebra>
constexpr auto gprepro(const Dist &dist, const Nat &e, const GAlgebra &algebra,
                      const Fix<F> &tree) -> Result;

template <template <class> class F, class MSeed, class Dist, class Nat,
          class GCoalgebra, class Seed>
constexpr auto gpostpro(const Dist &dist, const Nat &e,
                        const GCoalgebra &coalgebra, const Seed &seed)
    -> Fix<F>;

template <class Result, class Helper, template <class> class F,
          class HelperAlg, class Nat, class MainAlg>
constexpr auto zygo_histo_prepro(const HelperAlg &f, const Nat &e,
                                 const MainAlg &g, const Fix<F> &tree)
    -> Result;
```

`gprepro`/`gpostpro` splice `prepro.hpp`'s own `hoist<F>(e)` transformation
into `gcata`'s/`gana`'s workers — the exact "transform on the way
down/out" delta `prepro`/`postpro` already have against `fold_fix`/
`unfold_fix`, one level further out. `zygo_histo_prepro` is Kmett's famous
capstone: `gprepro` with the composed comonad
`W<X> = std::pair<Helper, Cofree<F,X>>` (the `EnvT Helper (Cofree F)`
comonad transformer) and a one-off distributive law, `dist_zygo_histo<F>`
— a zygomorphism, a histomorphism, and a prepromorphism running in a
single pass. The composed comonad needed a *dedicated*
`comonad_typeclass<pair<Helper, Cofree<F,X>>>` instance — the generic
`pair<B,A>` instance nests the wrong way for this `W` (see the header's own
"thin ice" comment and this scheme's S15 handoff for the empirically
confirmed reason).

Degeneracy laws (design §9/§7.11): with the identity transformation, the
helper ignored, and the Cofree consulted only via `extract`,
`zygo_histo_prepro` degenerates to `fold_fix`.

```cpp
// helper = remaining-list-length, e = take-while-positive, main algebra
// sums the head wherever the *tail's* remaining length is even.
int result = zygo_histo_prepro<int, int>(
    length_helper, take_while_positive_nat{}, even_tail_length_main, list);
// [3, 4, -1, 5] -> truncated to [3, 4] by e -> result == 4
```

Example: `generalized_tour`'s fourth section — the capstone computation
above, printed against its hand-checked expectation (`4`).

---

## Supporting types

These aren't schemes but are used throughout the catalog above:

- **`Identity<A>`** (`smd/typeclass/identity.hpp`) — the effect-free
  context; Functor/Applicative/Monad/Comonad all coincide with plain
  application. Used as `gcata`'s/`gana`'s `WResult`/`MSeed` for the
  cata/ana recoveries.
- **`either<L, R>`** (`smd/typeclass/either.hpp`) — the symmetric sum used
  everywhere a scheme may short-circuit (`apo`, `elgot`, `dist_apo`/
  `dist_gapo`, `gana`'s apo recovery). D4: Left = stop, Right = continue.
  Supports reference sides (P2988) for zero-copy grafts.
- **`std::pair<B, A>`'s env comonad** (`smd/typeclass/pair.hpp`) —
  `extract = .second`, dual to `either`'s monad; used as `zygo`'s and
  `para`'s carrier and `gcata`'s `WResult` for their recoveries.
- **`Cofree<F, A>`** (`smd/fixpoint/cofree.hpp`) — annotates every node of
  an `F`-tree with an `A`, keeping one already-annotated `F`-layer of
  children as the tail; `histo`'s fold carrier, `gcata`'s `WResult` for the
  histo recovery.
- **`Free<F, A>`** (`smd/fixpoint/free.hpp`) — a Pure value or one `F`-layer
  of further Free computations; `futu`'s unfold carrier, `gana`'s `MSeed`
  for the futu recovery.
- **`NatF`, `ListF<E,·>`, `TreeF<E,·>`, `ExprF`** (`smd/concrete/functors.hpp`)
  — the reusable base functors every example above builds its fixtures
  from, plus smart constructors and `std::vector`/`int` converters.

See [`docs/recursion-schemes-design.md`](recursion-schemes-design.md) for
the full design rationale, decisions log, and conventions, and
[`ops/DEVIATIONS.md`](../ops/DEVIATIONS.md) for the places implementation
reality drifted from the original design text.
