// src/smd/fixpoint/generalized.hpp                                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_GENERALIZED
#define INCLUDED_SMD_FIXPOINT_GENERALIZED

// Generalized schemes (design §7.10): gcata, the comonadic generalized
// catamorphism (S13; S14 extends this header with gana/ghylo). gcata
// factors fold_fix/histo/zygo/para through a single worker parameterized
// by a distributive law (dist_laws.hpp, S12) and a comonad
// (comonad_typeclass, S03/S07) -- the "recovery laws" below (design §9,
// gcata.t.cpp) are the actual proof that the factoring is correct, this
// header's own doc comments are not.

#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/dist_laws.hpp>
#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/fmap.hpp>

#include <smd/typeclass/comonad.hpp>
#include <smd/typeclass/identity.hpp>
#include <smd/typeclass/pair.hpp>

#include <utility>

namespace smd::fixpoint {

// ---------------------------------------------------------------------
// gcata :: Comonad w => (forall x. f (w x) -> w (f x))
//                    -> (f (w a) -> a) -> t -> a
// ---------------------------------------------------------------------

/** Implementation detail of gcata (not part of the public API): the
 * worker `c : Fix<F> -> W<F<WResult>>` (design §7.10's own `c`).
 *
 * A namespace-scope struct rather than a local class or lambda, following
 * this codebase's existing convention for self-recursive scheme helpers
 * (dist_histo_t/dist_futu_t, dist_laws.hpp): `c` recurses into itself
 * through a lambda captured by `layer_fmap`, and a deduced `auto` return
 * type cannot be used before its own deduction completes even one level
 * down inside that lambda (S07/S08's discovery) -- so the return type `C`
 * is computed up front and named explicitly. Recurses via `(*this)(...)`.
 *
 * `WWR` = `W<WResult>` (`comonad.duplicate(WResult) -> W<WResult>`);
 * `C` = `W<F<WResult>>` (`dist(F<WWR>) -> W<F<WResult>>`) -- both computed
 * once, up front, from Result/WResult/F/Dist alone (design §7.10's own
 * prescription), so the recursive call type-checks.
 *
 * Only ONE `comonad_typeclass` lookup is needed (keyed on `WResult`, not
 * re-keyed at each recursive instantiation): every comonad instance in
 * this codebase (Identity, pair's env comonad, Cofree) has
 * `extract`/`duplicate`/`fmap` generic over their own element-type
 * parameter, not fixed to the instance's keying type (S03/S07/S12
 * handoffs' discovery) -- so the single lookup object's methods already
 * work for any X, including X = WResult or X = F<WResult> here.
 */
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

// ---------------------------------------------------------------------
// Recovery functions (design §9): thin wrappers pinning gcata to each
// existing scheme's own algebra shape. Each is gated in gcata.t.cpp by
// exact-answer equivalence with fold_fix/histo/zygo/para on the same
// fixtures those schemes' own steps used -- that equivalence, not this
// header's doc comments, is the actual proof the factoring is correct
// (design D8).
// ---------------------------------------------------------------------

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
        [&algebra](const F<smd::typeclass::Identity<Result>> &layer)
        -> Result {
        return algebra(layer_fmap(
            [](const smd::typeclass::Identity<Result> &i) -> Result {
                return i.value;
            },
            layer));
    };
    return gcata<Result, smd::typeclass::Identity<Result>>(
        dist_cata, algebra_prime, tree);
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
template <class Result, class Helper, template <class> class F,
          class HelperAlg, class MainAlg>
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

} // namespace smd::fixpoint

#endif
