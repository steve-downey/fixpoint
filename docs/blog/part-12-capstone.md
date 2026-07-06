<div class="abstract" id="org6e65643">
<p>
"Can I have a zygomorphism, a histomorphism, and a prepromorphism at the
same time?" Kmett's zygoHistoPrepro answers by composition: transform the
generalized fold with a natural transformation, compose the env comonad
onto Cofree, write one distributive law, done. This part builds it, and
meets the one place where the generic machinery genuinely could not serve
&#x2014; the composed comonad needed its own instance, and the reason is worth
the whole part.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Part 11 - Distributive Laws and the Generalized Schemes ←](part-11-generalized.md)

</nav>


# gprepro: Part 6's Delta, One Level Up

Part 6 observed that `prepro` is `fold_fix` with one insertion &#x2014; `hoist<F>(e, child)` before the recursive call. `gprepro` makes the same insertion in `gcata`'s worker. From [`src/smd/fixpoint/generalized.hpp`](../../src/smd/fixpoint/generalized.hpp):

```cpp
template <class Result, class WResult, template <class> class F, class Dist,
          class Nat, class GAlgebra>
struct gprepro_worker_t {
    const Dist &dist;
    const Nat &e;
    const GAlgebra &algebra;

    using WWR = decltype(smd::typeclass::comonad_typeclass<WResult>.duplicate(
        std::declval<WResult>()));
    using C = decltype(dist(std::declval<F<WWR>>()));

    constexpr auto operator()(const Fix<F> &t) const -> C {
        return dist(layer_fmap(
            [this](const Fix<F> &child) -> WWR {
                return smd::typeclass::comonad_typeclass<WResult>.duplicate(
                    smd::typeclass::comonad_typeclass<WResult>.fmap(
                        algebra, (*this)(hoist<F>(e, child))));
            },
            unwrap_fix(t)));
    }
};

/** gprepro :: Comonad w => dist -> (forall x. f x -> f x)
 *                      -> (f (w a) -> a) -> t -> a
 *
 * Equation (design §7.11): `gprepro k e phi = phi(extract(c t))` where
 * `c = k . fmapF(duplicate . fmapW phi . c . hoist<F>(e)) . unfix` --
 * gcata's own `c` (design §7.10) with `hoist<F>(e)` spliced in front of
 * each recursive call.
 *
 * Degeneracy laws (design §9/§7.11, gprepro.t.cpp, D8 -- these gate the
 * implementation, not the equation above):
 * `gprepro(dist_cata, identity_nat, phi') == fold_fix(phi)`;
 * `gprepro(dist_cata, e, phi') == prepro(e, phi)` (S06's own take-while
 * fixture); `gprepro(k, identity_nat, phi) == gcata(k, phi)` (any k, e.g.
 * dist_histo).
 *
 * @tparam Result the fold's result type `a` (D5: explicit)
 * @tparam WResult `w a` -- the comonad-wrapped carrier (D5: explicit,
 *   mirrors gcata's own WResult)
 * @param dist a distributive law `F<w<X>> -> w<F<X>>` for every X
 *   (dist_laws.hpp)
 * @param e a natural transformation `F<X> -> F<X>` for every X (endo, the
 *   same contract as hoist's/prepro's own `e`)
 * @param algebra `F<WResult> -> Result`
 */
template <class Result, class WResult, template <class> class F, class Dist,
          class Nat, class GAlgebra>
constexpr auto gprepro(const Dist &dist, const Nat &e, const GAlgebra &algebra,
                      const Fix<F> &tree) -> Result {
    gprepro_worker_t<Result, WResult, F, Dist, Nat, GAlgebra> worker{dist, e,
                                                                     algebra};
    return algebra(
        smd::typeclass::comonad_typeclass<WResult>.extract(worker(tree)));
}
```

Diff it against Part 11's `gcata_worker_t`: one expression changed, `(*this)(hoist<F>(e, child))` where `(*this)(child)` was. The degeneracy laws triangulate it: identity transformation recovers `gcata`; `dist_cata` recovers `prepro`; both at once recover `fold_fix`. (`gpostpro` mirrors the splice on `gana`'s side &#x2014; hoist each recursive *result* before grafting &#x2014; completing the dual pair.)

With `gprepro` in hand, Kmett's capstone is a two-liner:

```cpp
/** zygo_histo_prepro :: (f b -> b) -> (forall x. f x -> f x)
 *                     -> (f (EnvT b (Cofree f) a) -> a) -> t -> a
 *
 * Zygomorphic histomorphism with a prepromorphism pass: `gprepro` with the
 * concrete composed comonad `W<X> = std::pair<Helper, Cofree<F,X>>` and
 * `dist_zygo_histo<F>(f)` as the distributive law (design §7.11).
 *
 * Degeneracy law (design §9/§7.11, gprepro.t.cpp): with `e = identity`,
 * `f` ignored by `g`, and `g` using only `extract` of the Cofree,
 * degenerates to `fold_fix`.
 *
 * @tparam Result the fold's result type `a` (D5: explicit)
 * @tparam Helper the helper algebra's carrier `b` (D5: explicit, mirrors
 *   zygo's own Helper)
 * @param f the helper algebra `F<Helper> -> Helper`
 * @param e a natural transformation `F<X> -> F<X>` for every X
 * @param g the main algebra `F<std::pair<Helper, Cofree<F,Result>>> ->
 *   Result`
 */
template <class Result, class Helper, template <class> class F,
          class HelperAlg, class Nat, class MainAlg>
constexpr auto zygo_histo_prepro(const HelperAlg &f, const Nat &e,
                                 const MainAlg &g, const Fix<F> &tree)
    -> Result {
    using WResult = std::pair<Helper, Cofree<F, Result>>;
    return gprepro<Result, WResult>(dist_zygo_histo<F>(f), e, g, tree);
}
```

The composed comonad is `W<X> = std::pair<Helper, Cofree<F, X>>` &#x2014; Haskell's `EnvT Helper (Cofree F)`: zygo's env layer wrapped around histo's Cofree. A zygomorphism, a histomorphism, and a prepromorphism, one pass. Everything interesting is in the two ingredients that make the two-liner legal.


# The Distributive Law for a Composed Comonad

```cpp
template <template <class> class F, class HelperAlg>
struct dist_zygo_histo_t {
    HelperAlg helper;

    template <class Helper, class X>
    constexpr auto
    operator()(const F<std::pair<Helper, Cofree<F, X>>> &l) const
        -> std::pair<Helper, Cofree<F, F<X>>> {
        auto helper_layer = layer_fmap(
            [](const std::pair<Helper, Cofree<F, X>> &p) -> Helper {
                return p.first;
            },
            l);
        auto w_layer = layer_fmap(
            [](const std::pair<Helper, Cofree<F, X>> &p) -> Cofree<F, X> {
                return p.second;
            },
            l);
        return std::pair<Helper, Cofree<F, F<X>>>{helper(helper_layer),
                                                   dist_histo<F>(w_layer)};
    }
};

/** distZygoHisto :: (f b -> b) -> f (EnvT b (Cofree f) a)
 *                 -> EnvT b (Cofree f) (f a)
 *
 * Factory: takes the helper algebra `F<Helper> -> Helper` and returns the
 * one-off law above. Called as `dist_zygo_histo<F>(f)`.
 */
template <template <class> class F, class HelperAlg>
constexpr auto dist_zygo_histo(HelperAlg helper)
    -> dist_zygo_histo_t<F, HelperAlg> {
    return dist_zygo_histo_t<F, HelperAlg>{std::move(helper)};
}
```

`dist_zygo_histo` is a factory (it closes over the helper algebra, as `dist_zygo` did) transcribing Kmett's `distZygoT`: fold the helper components out of the layer with `f`, redistribute the Cofree components with `dist_histo<F>`, pair the results. A distributive law for a composed comonad is the two component laws, composed. It lives here rather than in `dist_laws.hpp` deliberately &#x2014; it is capstone-specific, and the library keeps one-off machinery next to its one use.


# The Thin Ice: Why the Generic Pair Instance Is Wrong

Here is the part worth the price of admission. The library already has a comonad instance for `std::pair<B, A>` &#x2014; the env comonad, serving zygo and para since Part 11. `W<X> = pair<Helper, Cofree<F, X>>` *is* a pair. Why not let the generic instance serve?

Because it computes the wrong `duplicate`. The generic pair instance re-nests the *pair* layer: `duplicate(pair<B, X>)` is `pair<B, pair<B, X>>`. For the composed comonad, `W<W<X>>` must be `pair<Helper, Cofree<F, pair<Helper, Cofree<F, X>>>>` &#x2014; the duplication has to happen *inside the Cofree*, re-annotating every node of the history with the environment re-attached. Same outer type, different nesting, and only one of them is the `EnvT` comonad. The dedicated instance does what Haskell's `EnvT` does: run Cofree's own `duplicate`, then `fmap` the environment back onto every position:

```cpp
template <template <class> class F, class Helper, class X>
struct ZygoHistoComonadImpl {
    template <class Y>
    constexpr auto
    extract(this auto &&, const std::pair<Helper, smd::fixpoint::Cofree<F, Y>> &w)
        -> const Y & {
        return smd::fixpoint::extract(w.second);
    }

    template <class Y>
    constexpr auto
    duplicate(this auto &&,
              const std::pair<Helper, smd::fixpoint::Cofree<F, Y>> &w)
        -> std::pair<Helper,
                    smd::fixpoint::Cofree<
                        F, std::pair<Helper, smd::fixpoint::Cofree<F, Y>>>> {
        const Helper &env = w.first;
        auto duplicated_cofree =
            comonad_typeclass<smd::fixpoint::Cofree<F, Y>>.duplicate(w.second);
        auto reattached = comonad_typeclass<smd::fixpoint::Cofree<F, Y>>.fmap(
            [&env](const smd::fixpoint::Cofree<F, Y> &c)
                -> std::pair<Helper, smd::fixpoint::Cofree<F, Y>> {
                return std::pair<Helper, smd::fixpoint::Cofree<F, Y>>{env, c};
            },
            duplicated_cofree);
        return std::pair<Helper,
                         smd::fixpoint::Cofree<
                             F, std::pair<Helper,
                                         smd::fixpoint::Cofree<F, Y>>>>{
            env, std::move(reattached)};
    }

    template <class Fn, class Y>
    constexpr auto fmap(this auto &&, Fn &&fn,
                        const std::pair<Helper, smd::fixpoint::Cofree<F, Y>> &w) {
        using Z = remove_cvref_t<std::invoke_result_t<Fn, const Y &>>;
        return std::pair<Helper, smd::fixpoint::Cofree<F, Z>>{
            w.first, comonad_typeclass<smd::fixpoint::Cofree<F, Y>>.fmap(
                         std::forward<Fn>(fn), w.second)};
    }
};

template <template <class> class F, class Helper, class X>
struct ZygoHistoComonadMap : Comonad<ZygoHistoComonadImpl<F, Helper, X>> {
    using ZygoHistoComonadImpl<F, Helper, X>::duplicate;
    using ZygoHistoComonadImpl<F, Helper, X>::extract;
    using ZygoHistoComonadImpl<F, Helper, X>::fmap;
};

/** The composed comonad instance, keyed on `std::pair<Helper,
 * Cofree<F,X>>` specifically -- more specialized than pair.hpp's own
 * `comonad_typeclass<std::pair<B,A>>` pattern, so this specialization wins
 * whenever the value slot actually is `Cofree<F,X>` for some F/X (i.e.
 * exactly and only for zygo_histo_prepro's own W); pair.hpp's generic
 * instance still serves every other `pair<B,A>` use in this codebase
 * (zygo_via_gcata's `pair<Helper,Result>`, para_via_gcata's
 * `pair<Fix<F>,Result>`) unchanged.
 */
template <template <class> class F, class Helper, class X>
inline constexpr auto
    comonad_typeclass<std::pair<Helper, smd::fixpoint::Cofree<F, X>>> =
        ZygoHistoComonadMap<F, Helper, X>{};
```

The registration is a partial specialization keyed on `pair<Helper, Cofree<F, X>>` &#x2014; strictly more specialized than the generic `pair<B, A>`, so it wins exactly when the value slot is a Cofree and never otherwise. Zygo's and para's pairs still use the generic instance, untouched. This is the typeclass-object machinery of Part 2 earning its keep under real load: two lawful comonad structures on the same C++ type, disambiguated by specialization order, no newtype wrapper required &#x2014; where Haskell *must* wrap in `EnvT` to select the instance, the C++ registry selects by type structure. (Whether that is a feature or a hazard is a fair fight; the header calls it "thin ice" and documents which side it skated to.)


# The Capstone Computes

From [`src/examples/generalized_tour.cpp`](../../src/examples/generalized_tour.cpp), the fourth section:

```cpp
// ---------------------------------------------------------------------
// 4. zygo_histo_prepro capstone -- helper (remaining length) + one-step
//    Cofree history + a prepro pass (take-while-positive), driven by the
//    one-off dist_zygo_histo law. [3,4,-1,5] truncates to [3,4]; only "4"
//    (whose tail's remaining length, 0, is even) is kept: result = 4.
//    There is no independent "specialized" scheme to compare against
//    here (this computation only exists because gprepro/dist_zygo_histo
//    make it expressible at all) -- the pair printed is the capstone's
//    own result against the hand-checked expectation.
// ---------------------------------------------------------------------
{
        IntList list = list_from_vector({3, 4, -1, 5});
        int computed = zygo_histo_prepro<int, int>(
            length_helper, take_while_positive_nat{}, even_tail_length_main,
            list);
        all_ok &= report("zygo_histo_prepro capstone", "dist_zygo_histo",
                         /*specialized (hand-checked)=*/4, computed);
}
```

The fixture computation: helper folds each tail's remaining length, the transformation truncates at the first negative (take-while-positive, Part 6's own fixture), and the main algebra keeps a head only when its tail's length is even &#x2014; consulting the helper (zygo), the history (histo), and the pre-transformed structure (prepro) in one algebra. `[3, 4, -1, 5]` truncates to `[3, 4]`; only `4` survives the even-length test; the answer is `4`, against a hand-checked expectation &#x2014; because there *is* no specialized scheme to compare with. This computation exists only because the composition machinery makes it expressible. That is the capstone's real claim: not that it reproduces the catalog, but that the catalog's pieces compose into schemes nobody bothered to name.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [Next: Part 13 - Conclusion →](part-13-conclusion.md)

</nav>


# References

Kmett, Edward (2009). **Recursion Schemes: A Field Guide (Redux)**, The Comonad.Reader &#x2014; zygoHistoPrepro's origin, <http://comonad.com/reader/2009/recursion-schemes/>.

Uustalu, Tarmo, Vene, Varmo, and Pardo, Alberto (2001). **Recursion Schemes from Comonads**, Nordic Journal of Computing 8(3).
