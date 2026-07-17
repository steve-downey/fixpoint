<div class="abstract" id="org4c6b005">
<p>
Ten parts of schemes, each "fold_fix with a richer carrier". Here is the
theorem behind the pattern: every fold in the catalog is one construction
&mdash; gcata &mdash; instantiated at a comonad, every unfold is gana at a monad,
and the only scheme-specific ingredient is a distributive law that pushes
the functor through the (co)monad. The proof is executable and ships as an
example.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Part 10 - Elgot Algebras ←](part-10-elgot.md)

</nav>


# The Carriers Were (Co)monads All Along

Line up the fold carriers this series has used: `Result` bare, then `pair<Fix<F>, Result>` (para), `pair<Helper, Result>` (zygo), `Cofree<F, Result>` (histo). Each is a comonad `W` applied to `Result` &mdash; the Identity comonad, the env comonad twice, Cofree. The unfold carriers: `Seed` bare (Identity monad), `either<Fix<F>, Seed>` (apo &mdash; either's right-biased monad), `Free<F, Seed>` (futu). Monads, every one. The library registered exactly these `comonad_typeclass` and `monad_typeclass` instances back in their own parts, quietly.

Uustalu, Vene, and Pardo made the pattern a theorem (Uustalu, Tarmo and Vene, Varmo and Pardo, Alberto, 2001): a fold with carrier `W<Result>` needs only one scheme-specific ingredient &mdash; a *distributive law* of the functor over the comonad, `dist : F<W<X>> -> W<F<X>>` for every `X`. Kmett's library packages that as `gcata~/~gana` (Kmett, Edward, 2011); Hinze, Wu, and Gibbons later unified further (Hinze, Ralf and Wu, Nicolas and Gibbons, Jeremy, 2013). This library transcribes the whole apparatus.


# The Laws Themselves

A distributive law is another polymorphic function object &mdash; Part 6's natural transformation, one level up. The simplest two, from [`src/smd/fixpoint/dist_laws.hpp`](../../src/smd/fixpoint/dist_laws.hpp):

```cpp
/** distCata :: f (Identity a) -> Identity (f a)
 * Design §7.9's own worked transcription, verbatim.
 */
struct dist_cata_t {
    template <class Layer> // Layer = F<Identity<A>>
    constexpr auto operator()(const Layer &l) const {
        return smd::typeclass::Identity{
            layer_fmap([](const auto &i) { return i.value; }, l)};
    }
};
inline constexpr dist_cata_t dist_cata{};

// ---------------------------------------------------------------------
// dist_ana :: Identity (f a) -> f (Identity a)
// ---------------------------------------------------------------------

/** distAna :: Identity (f a) -> f (Identity a)
 * fmapF(wrap-in-Identity) over the layer inside.
 *
 * The wrapping lambda names its result type explicitly
 * (`Identity<remove_cvref_t<decltype(x)>>{x}`) rather than writing
 * `Identity{x}` and relying on CTAD (DEV-03, S14): when `A` (the layer's
 * own element type) is itself already an `Identity<...>` -- which is
 * exactly what happens when `dist_ana` is used as `gana`'s distributive
 * law with `MSeed = Identity<Seed>` (generalized.hpp's `ana_via_gana`) --
 * `Identity{x}` hits CTAD's implicit *copy* deduction candidate
 * ([over.match.class.deduct]) and silently collapses to `Identity<T>`
 * (a copy of `x`) instead of wrapping it one level deeper as
 * `Identity<Identity<T>>`, exactly as `vector{v}` for `v : vector<int>`
 * copies rather than nesting. Naming the template argument explicitly
 * sidesteps CTAD entirely, so the lambda always wraps regardless of what
 * `x`'s own type happens to be.
 */
struct dist_ana_t {
    template <class Layer> // Layer = F<A>; argument is Identity<Layer>
    constexpr auto
    operator()(const smd::typeclass::Identity<Layer> &ident) const {
        return layer_fmap(
            [](const auto &x) {
                return smd::typeclass::Identity<
                    std::remove_cvref_t<decltype(x)>>{x};
            },
            ident.value);
    }
};
inline constexpr dist_ana_t dist_ana{};
```

`dist_cata` pushes Identity out of a layer; `dist_ana` pushes it in. (The `dist_ana` comment preserves a real bug hunt: `Identity{x}` with CTAD silently *copies* an `x` that is already an `Identity` instead of nesting it &mdash; the deduction-guide copy candidate, the same reason `vector{v}` copies. Naming the template argument sidesteps CTAD. Deviations like this are logged, not patched over.)

The interesting law is histo's:

```cpp
template <template <class> class F>
struct dist_histo_t {
    template <class A>
    constexpr auto operator()(const F<Cofree<F, A>> &l) const
        -> Cofree<F, F<A>> {
        auto head =
            layer_fmap([](const Cofree<F, A> &c) { return extract(c); }, l);
        auto tail = layer_fmap(
            [this](const Cofree<F, A> &c) -> Cofree<F, F<A>> {
                return (*this)(c.tail);
            },
            l);
        return Cofree<F, F<A>>{std::move(head), std::move(tail)};
    }
};

template <template <class> class F>
inline constexpr dist_histo_t<F> dist_histo{};
```

`F<Cofree<F,A>> -> Cofree<F, F<A>>`: heads out, tails redistributed, recursively. Note it is a variable *template*, called `dist_histo<F>(layer)` with `F` explicit &mdash; a genuine C++ deduction limit, recorded as a deviation: GCC cannot deduce a template-template parameter back out of an already-elaborated alias application. The full roster &mdash; `dist_zygo(helper)`, `dist_para<F>`, `dist_apo`, `dist_futu<F>`, `dist_gapo(coalg)` &mdash; lives in the same header, each side of Part 4's pair/either duality getting its mirror-image law.


# gcata: One Fold to Rule Them

From [`src/smd/fixpoint/generalized.hpp`](../../src/smd/fixpoint/generalized.hpp):

```cpp
template <class Result, class WResult, template <class> class F, class Dist,
          class GAlgebra>
struct gcata_worker_t {
    const Dist &dist;
    const GAlgebra &algebra;

    using WWR = decltype(smd::typeclass::comonad_typeclass<WResult>.duplicate(
        std::declval<WResult>()));
    using C = decltype(dist(std::declval<F<WWR>>()));

    constexpr auto operator()(const Fix<F> &t) const -> C {
        return dist(layer_fmap(
            [this](const Fix<F> &child) -> WWR {
                return smd::typeclass::comonad_typeclass<WResult>.duplicate(
                    smd::typeclass::comonad_typeclass<WResult>.fmap(
                        algebra, (*this)(child)));
            },
            unwrap_fix(t)));
    }
};

/** gcata :: Comonad w => (forall x. f (w x) -> w (f x))
 *                     -> (f (w a) -> a) -> t -> a
 *
 * Equation (design §7.10): `gcata k g = g(extract(c t))` where
 * `c = k . fmapF(duplicate . fmapW g . c) . unfix`.
 *
 * @tparam Result the fold's result type `a` (D5: explicit)
 * @tparam WResult `w a` -- the comonad-wrapped carrier; the recovery
 *   functions below instantiate it to `Identity<Result>` (cata),
 *   `Cofree<F,Result>` (histo), `std::pair<Helper,Result>` (zygo), or
 *   `std::pair<Fix<F>,Result>` (para) (D5: explicit, mirrors `WResult` in
 *   the design text)
 * @param dist a distributive law `F<w<X>> -> w<F<X>>` for every X
 *   (dist_laws.hpp)
 * @param algebra `F<WResult> -> Result`
 */
template <class Result, class WResult, template <class> class F, class Dist,
          class GAlgebra>
constexpr auto gcata(const Dist &dist, const GAlgebra &algebra,
                     const Fix<F> &tree) -> Result {
    gcata_worker_t<Result, WResult, F, Dist, GAlgebra> worker{dist, algebra};
    return algebra(
        smd::typeclass::comonad_typeclass<WResult>.extract(worker(tree)));
}
```

The worker `c : Fix<F> -> W<F<WResult>>` is the equation `c = k ∘ fmapF(duplicate ∘ fmapW g ∘ c) ∘ unfix` made into a struct, and the struct-ness is itself a lesson: `c` recurses into itself through a lambda, and a deduced return type cannot be used before its own deduction completes &mdash; so the return type `C` is computed *up front*, from `Result~/~WResult~/~F~/~Dist` alone, and the worker is a named type recursing via `(*this)`. Every self-recursive helper in this codebase independently rediscovered that discipline; the header comments cite the trail.

One subtlety the comment documents: only *one* `comonad_typeclass` lookup happens, keyed on `WResult`, even though the worker needs `extract~/~duplicate~/~fmap` at other instantiations along the way. That works because every comonad instance in the library keeps its operations generic over their element type &mdash; the instance's keying type pins the *shape* (which comonad), not the element. A design invariant, discovered the hard way and then enforced everywhere.

The recoveries are almost embarrassing in their brevity:

```cpp
/** cata_via_gcata: `gcata(dist_cata, ...)` recovers fold_fix.
 *
 * fold_fix's plain algebra is `F<Result> -> Result`; gcata's own algebra
 * shape is `F<WResult> -> Result` with `WResult = Identity<Result>` here,
 * so the plain algebra must first be lifted:
 * `phi' = phi . layer_fmap(.value)` (design §7.10's own worked example)
 * -- projecting every child's `Identity<Result>` down to `Result` before
 * calling the plain algebra.
 */
template <class Result, template <class> class F, class Algebra>
constexpr auto cata_via_gcata(const Algebra &algebra, const Fix<F> &tree)
    -> Result {
    auto algebra_prime =
        [&algebra](const F<smd::typeclass::Identity<Result>> &layer) -> Result {
        return algebra(layer_fmap([](const smd::typeclass::Identity<Result> &i)
                                      -> Result { return i.value; },
                                  layer));
    };
    return gcata<Result, smd::typeclass::Identity<Result>>(dist_cata,
                                                           algebra_prime, tree);
}

/** histo_via_gcata: `gcata(dist_histo<F>, ...)` recovers histo.
 *
 * histo's own algebra is already exactly `F<Cofree<F,Result>> -> Result`
 * -- `WResult = Cofree<F,Result>` here is precisely histo's own carrier
 * (histo.hpp), so no projection is needed; the plain histo algebra is
 * passed straight through as gcata's algebra.
 */
template <class Result, template <class> class F, class Algebra>
constexpr auto histo_via_gcata(const Algebra &algebra, const Fix<F> &tree)
    -> Result {
    return gcata<Result, Cofree<F, Result>>(dist_histo<F>, algebra, tree);
}

/** zygo_via_gcata: `gcata(dist_zygo(helper), ...)` recovers zygo.
 *
 * zygo's own main algebra is already exactly
 * `F<std::pair<Helper,Result>> -> Result` -- `WResult =
 * std::pair<Helper,Result>` here is precisely zygo's own carrier (helper
 * first, main second, S05's convention), so no projection is needed.
 */
template <class Result, class Helper, template <class> class F, class HelperAlg,
          class MainAlg>
constexpr auto zygo_via_gcata(const HelperAlg &helper, const MainAlg &main,
                              const Fix<F> &tree) -> Result {
    return gcata<Result, std::pair<Helper, Result>>(dist_zygo(helper), main,
                                                    tree);
}

/** para_via_gcata: `gcata(dist_para<F>, ...)` recovers para.
 *
 * para's own algebra is already exactly
 * `F<std::pair<Fix<F>,Result>> -> Result` -- `WResult =
 * std::pair<Fix<F>,Result>` here is precisely para's own carrier
 * (original subtree first, fold result second, S04's convention), so no
 * projection is needed.
 */
template <class Result, template <class> class F, class Algebra>
constexpr auto para_via_gcata(const Algebra &algebra, const Fix<F> &tree)
    -> Result {
    return gcata<Result, std::pair<Fix<F>, Result>>(dist_para<F>, algebra,
                                                    tree);
}
```

`histo`, `zygo`, `para`: their algebras pass through *unchanged*, because their carriers were already exactly `W<Result>` for the right `W`. Only `cata` needs a shim &mdash; its plain algebra never knew about the Identity wrappers. The catalog's folds were `gcata` instances all along; the recovery functions just say so in the type system.


# gana and ghylo: The Mirror and the Fusion

```cpp
template <template <class> class F, class MSeed, class Dist, class GCoalgebra>
struct gana_worker_t {
    const Dist &dist;
    const GCoalgebra &coalgebra;

    using MFMS = decltype(smd::typeclass::monad_typeclass<MSeed>.pure(
        std::declval<F<MSeed>>()));
    using MMS = decltype(smd::typeclass::monad_typeclass<MSeed>.pure(
        std::declval<MSeed>()));

    constexpr auto operator()(const MFMS &m) const -> Fix<F> {
        return wrap_fix<F>(layer_fmap(
            [this](const MMS &mms) -> Fix<F> {
                auto joined = smd::typeclass::monad_typeclass<MSeed>.join(mms);
                auto next = layer_fmap(coalgebra, joined);
                return (*this)(next);
            },
            dist(m)));
    }
};

/** gana :: Monad m => (forall x. m (f x) -> f (m x))
 *                   -> (a -> f (m a)) -> a -> t
 *
 * Equation (design §7.10): `gana k psi = a(pure(psi(seed)))` where
 * `a(m) = fix(fmapF(lambda mms. a(fmapM(psi, join(mms))), k(m)))`.
 *
 * @tparam F unary template functor (D5-style explicit; not deducible from
 *   any argument here, mirroring apo/futu's own convention -- `gana`
 *   returns a `Fix<F>`, it is never given one)
 * @tparam MSeed `m a` -- the monad-wrapped seed; the recovery functions
 *   below instantiate it to `Identity<Seed>` (ana), `either<Fix<F>,Seed>`
 *   (apo), `Free<F,Seed>` (futu) (D5: explicit, mirrors `WResult`)
 * @param dist a distributive law `M<F<X>> -> F<M<X>>` for every X
 *   (dist_laws.hpp)
 * @param coalgebra `Seed -> F<MSeed>`
 */
template <template <class> class F, class MSeed, class Dist, class GCoalgebra,
          class Seed>
constexpr auto gana(const Dist &dist, const GCoalgebra &coalgebra,
                    const Seed &seed) -> Fix<F> {
    gana_worker_t<F, MSeed, Dist, GCoalgebra> worker{dist, coalgebra};
    return worker(smd::typeclass::monad_typeclass<MSeed>.pure(coalgebra(seed)));
}
```

`gana` mirrors `gcata` arrow for arrow: monad for comonad, `pure` for `extract`, `join` for `duplicate`, the distributive law flipped to `M<F<X>> -> F<M<X>>`. `ana_via_gana`, `apo_via_gana`, and `futu_via_gana` recover the unfolds exactly as their fold counterparts did &mdash; apo and futu passing their coalgebras straight through.

`ghylo` composes both halves:

```cpp
/** ghylo :: (Comonad w, Monad m) =>
 *   (forall x. f (w x) -> w (f x)) -> (f (w a) -> a)
 *   -> (forall x. m (f x) -> f (m x)) -> (b -> f (m b)) -> b -> a
 *
 * `ghylo w_dist algebra m_dist coalgebra seed = gcata<Result,WResult>(
 *    w_dist, algebra, gana<F,MSeed>(m_dist, coalgebra, seed))` --
 * materializes the intermediate `Fix<F>` gana builds, then folds it with
 * gcata. Recovers refold/dyna/codyna/chrono per the recovery table
 * (design §9) once `algebra`/`coalgebra` are lifted the same way
 * `cata_via_gcata`/`ana_via_gana` lift `fold_fix`'s/`unfold_fix`'s plain
 * algebra/coalgebra -- ghylo.t.cpp reuses those exact wrapping shapes
 * inline, since only cata/ana need a projection at all (S13's gcata
 * discovery, which carries over unchanged to ghylo's gcata half; gana's
 * apo/futu halves need none either, per the recovery functions above).
 *
 * @tparam Result the fold's result type `a` (D5: explicit)
 * @tparam WResult `w a` (D5: explicit, gcata's own carrier)
 * @tparam F unary template functor (D5: explicit, gana's own requirement)
 * @tparam MSeed `m b` (D5: explicit, gana's own carrier)
 */
template <class Result, class WResult, template <class> class F, class MSeed,
          class WDist, class GAlgebra, class MDist, class GCoalgebra,
          class Seed>
constexpr auto ghylo(const WDist &w_dist, const GAlgebra &algebra,
                     const MDist &m_dist, const GCoalgebra &coalgebra,
                     const Seed &seed) -> Result {
    return gcata<Result, WResult>(w_dist, algebra,
                                  gana<F, MSeed>(m_dist, coalgebra, seed));
}
```

The header is candid: this shipped *materializing* &mdash; `gcata` after `gana`, intermediate tree and all &mdash; because it passed every recovery law on the first attempt and performance is an explicit non-goal. An honest engineering judgment, written where users will read it.


# The Proof Runs

The example [`src/examples/generalized_tour.cpp`](../../src/examples/generalized_tour.cpp) computes each answer twice &mdash; classical scheme and generalized recovery &mdash; and exits nonzero on any mismatch:

```cpp
// ---------------------------------------------------------------------
// 1. fold_fix vs cata_via_gcata(dist_cata) -- Nat count, n = 5.
// ---------------------------------------------------------------------
{
        Nat nat = nat_from_int(5);
        int specialized = fold_fix<int>(nat_count_algebra, nat);
        int generalized = cata_via_gcata<int>(nat_count_algebra, nat);
        all_ok &= report("fold_fix / cata_via_gcata", "dist_cata", specialized,
                         generalized);
}

// ---------------------------------------------------------------------
// 2. histo vs histo_via_gcata(dist_histo) -- Fibonacci, n = 10.
// ---------------------------------------------------------------------
{
        Nat nat = nat_from_int(10);
        int specialized = histo<int>(fib_algebra, nat);
        int generalized = histo_via_gcata<int>(fib_algebra, nat);
        all_ok &= report("histo / histo_via_gcata", "dist_histo", specialized,
                         generalized);
}

// ---------------------------------------------------------------------
// 3. dyna vs ghylo(dist_histo, dist_ana) -- Fibonacci as a fused
//    refold, n = 10 (design §9's own ghylo/dyna recovery law).
// ---------------------------------------------------------------------
{
        int specialized = dyna<int, NatF>(fib_algebra, countdown, 10);
        int generalized = (ghylo<int, Cofree<NatF, int>, NatF, Identity<int>>(
            dist_histo<NatF>, fib_algebra, dist_ana, ana_coalgebra_prime, 10));
        all_ok &= report("dyna / ghylo-as-dyna", "histo+ana", specialized,
                         generalized);
}
```

`fold_fix` against `dist_cata`, Fibonacci-by-histo against `dist_histo`, `dyna` against a `ghylo` wired from `dist_histo` plus `dist_ana`. Three schemes from ten parts of this series, reconstructed before your eyes from a distributive law and a (co)monad each. One reveal remains: what happens when one `W` is not enough, and two comonads have to be composed.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [Next: Part 12 - The Capstone: zygoHistoPrepro →](part-12-capstone.md)

</nav>


# References

Uustalu, Tarmo, Vene, Varmo, and Pardo, Alberto (2001). **Recursion Schemes from Comonads**, Nordic Journal of Computing 8(3).

Hinze, Ralf, Wu, Nicolas, and Gibbons, Jeremy (2013). **Unifying Structured Recursion Schemes**, ICFP '13.

Kmett, Edward (2011&ndash;). **recursion-schemes**, Hackage, <https://hackage.haskell.org/package/recursion-schemes>.
