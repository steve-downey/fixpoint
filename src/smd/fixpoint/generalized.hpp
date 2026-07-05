// src/smd/fixpoint/generalized.hpp                                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_GENERALIZED
#define INCLUDED_SMD_FIXPOINT_GENERALIZED

// Generalized schemes (design §7.10): gcata, the comonadic generalized
// catamorphism (S13), and gana/ghylo, the monadic generalized anamorphism
// and the fused generalized refold (S14). gcata factors
// fold_fix/histo/zygo/para through a single worker parameterized by a
// distributive law (dist_laws.hpp, S12) and a comonad (comonad_typeclass,
// S03/S07); gana mirrors that shape on the unfold side, factoring
// unfold_fix/apo/futu through a distributive law and a monad
// (monad_typeclass, S03/S08) -- the "recovery laws" below (design §9,
// gcata.t.cpp/gana.t.cpp/ghylo.t.cpp) are the actual proof that the
// factoring is correct, this header's own doc comments are not.

#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/dist_laws.hpp>
#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/free.hpp>

#include <smd/typeclass/comonad.hpp>
#include <smd/typeclass/either.hpp>
#include <smd/typeclass/identity.hpp>
#include <smd/typeclass/monad.hpp>
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

// ---------------------------------------------------------------------
// gana :: Monad m => (forall x. m (f x) -> f (m x))
//                  -> (a -> f (m a)) -> a -> t
// ---------------------------------------------------------------------

/** Implementation detail of gana (not part of the public API): the worker
 * `a : M<F<MSeed>> -> Fix<F>` (design §7.10's own `a`).
 *
 * Mirrors gcata_worker_t's shape (S13), but the recursion descends over
 * *values* threaded through the monad, not over an already-built `Fix<F>`
 * tree -- `a`'s own domain type `M<F<MSeed>>` is therefore the same at
 * every recursive depth (there is no analogue of gcata's `Fix<F>`
 * descent), so only two types need to be computed up front, both by
 * lifting *forward* through `monad_typeclass<MSeed>.pure` (never by
 * unwrapping backward), exactly as gcata_worker_t computes `WWR`/`C`
 * forward from `duplicate`/`dist`:
 *   `MFMS` = `M<F<MSeed>>` (`pure(declval<F<MSeed>>())`) -- `a`'s own
 *   parameter/return-domain type, and the type `gana`'s entry point below
 *   produces via `pure(coalgebra(seed))`.
 *   `MMS` = `M<MSeed>` (`pure(declval<MSeed>())`) -- the element type of
 *   `dist(m)` (a `F<MMS>` layer), matching `Dist`'s own contract
 *   (`M<F<X>> -> F<M<X>>` at `X = MSeed`).
 *
 * Only ONE `monad_typeclass` lookup is needed (keyed on `MSeed`), mirroring
 * gcata_worker_t's single-comonad-lookup discovery (S13 handoff): every
 * monad instance in this codebase (Identity, Free, either) has
 * `pure`/`bind`/`join` generic over the element type actually passed in --
 * only Free's own outer `F` and either's own `L` are fixed at the
 * instance-keying level (S08/S03 handoffs' discovery), so the single
 * lookup object already works for `X = MSeed` here, not just the
 * instance's own keying type.
 *
 * `join` is reached through the Monad CRTP's own derived operation
 * (`monad_typeclass<MSeed>.join(mms)`, monad.hpp's `Monad<Impl>::join`);
 * `fmapM(coalgebra, joined)` is spelled with the plain `layer_fmap` helper
 * (fmap.hpp) -- every monad instance here is already a Functor instance
 * too (Identity/Free/either all have `functor_typeclass` specializations),
 * and each one's `fmap` is fixed to the *same* class-level element type
 * `join` just produced (`MSeed` itself), so `layer_fmap` dispatches
 * correctly with no separate lookup.
 */
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
                auto joined =
                    smd::typeclass::monad_typeclass<MSeed>.join(mms);
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
template <template <class> class F, class MSeed, class Dist,
          class GCoalgebra, class Seed>
constexpr auto gana(const Dist &dist, const GCoalgebra &coalgebra,
                    const Seed &seed) -> Fix<F> {
    gana_worker_t<F, MSeed, Dist, GCoalgebra> worker{dist, coalgebra};
    return worker(
        smd::typeclass::monad_typeclass<MSeed>.pure(coalgebra(seed)));
}

// ---------------------------------------------------------------------
// gana recovery functions (design §9): thin wrappers pinning gana to each
// existing unfold scheme's own coalgebra shape. Each is gated in
// gana.t.cpp by exact-answer equivalence with unfold_fix/apo/futu on the
// same fixtures those schemes' own steps used (design D8).
// ---------------------------------------------------------------------

/** ana_via_gana: `gana<F, Identity<Seed>>(dist_ana, ...)` recovers
 * unfold_fix.
 *
 * unfold_fix's plain coalgebra is `Seed -> F<Seed>`; gana's own coalgebra
 * shape is `Seed -> F<MSeed>` with `MSeed = Identity<Seed>` here, so the
 * plain coalgebra must first be lifted:
 * `psi' = layer_fmap(Identity-wrap) . psi` (design §7.10's own worked
 * example, the mirror image of `cata_via_gcata`'s `.value`-projection
 * above) -- wrapping every child seed in `Identity` before handing the
 * layer to `gana`.
 */
template <template <class> class F, class Coalgebra, class Seed>
constexpr auto ana_via_gana(const Coalgebra &coalgebra, const Seed &seed)
    -> Fix<F> {
    using MSeed = smd::typeclass::Identity<Seed>;
    auto coalgebra_prime = [&coalgebra](const Seed &s) -> F<MSeed> {
        return layer_fmap([](const Seed &x) { return MSeed{x}; },
                          coalgebra(s));
    };
    return gana<F, MSeed>(dist_ana, coalgebra_prime, seed);
}

/** apo_via_gana: `gana<F, either<Fix<F>,Seed>>(dist_apo bound at X =
 * MSeed, ...)` recovers apo.
 *
 * apo's own coalgebra is already exactly `Seed -> F<either<Fix<F>,Seed>>`
 * -- `MSeed = either<Fix<F>,Seed>` here is precisely apo's own carrier
 * (D4: Left = graft the finished subtree, Right = continue unfolding), so
 * unlike `ana_via_gana` above, **no coalgebra wrapping is needed**; it is
 * passed straight through. `dist_apo` itself, however, needs its result
 * element type `X` named explicitly at the call site (DEV-02,
 * dist_laws.hpp) -- and here `X` is always `MSeed` (gana's worker only
 * ever calls `dist` at `X = MSeed`, a fixed instantiation every recursive
 * step, mirroring gcata_worker_t's own single fixed instantiation), so a
 * thin lambda binds `dist_apo.template operator()<MSeed>` once and is
 * passed as `Dist`.
 *
 * D4 orientation, traced through once: at the top level `pure` always
 * constructs Right (`monad_typeclass<MSeed>.pure` = `make_right`), so the
 * very first `dist` call takes the Right branch, threading the
 * coalgebra's own per-position `either<Fix<F>,Seed>` down to `join`
 * unchanged. A joined-Left value (a finished subtree to graft) is then
 * left untouched by `layer_fmap`'s Right-only `fmap` (either's functor
 * instance never touches Left), so it flows through to the *next*
 * recursive `dist` call still Left -- whose Left branch unfixes the
 * grafted subtree one layer at a time and rewraps every child as Left
 * again, i.e. it reconstructs the subtree node-for-node (the identity
 * graft). A joined-Right value (continue unfolding) instead gets mapped
 * through the real `coalgebra`, producing a fresh `F<MSeed>` layer that
 * resumes the unfold. Left never triggers a further coalgebra call and
 * Right never grafts -- confirming the D4 convention holds through gana,
 * not flipped.
 */
template <template <class> class F, class Coalgebra, class Seed>
constexpr auto apo_via_gana(const Coalgebra &coalgebra, const Seed &seed)
    -> Fix<F> {
    using MSeed = smd::typeclass::either<Fix<F>, Seed>;
    auto dist_apo_x = [](const auto &e) {
        return dist_apo.template operator()<MSeed>(e);
    };
    return gana<F, MSeed>(dist_apo_x, coalgebra, seed);
}

/** futu_via_gana: `gana<F, Free<F,Seed>>(dist_futu<F>, ...)` recovers
 * futu.
 *
 * futu's own coalgebra is already exactly `Seed -> F<Free<F,Seed>>` --
 * `MSeed = Free<F,Seed>` here is precisely futu's own carrier -- so, like
 * `apo_via_gana` above and unlike `ana_via_gana`, no coalgebra wrapping is
 * needed. `dist_futu<F>`'s own element type deduces normally from the
 * argument once `F` is bound (dist_laws.hpp: only `F` itself must be
 * named explicitly, per DEV-02 -- unlike `dist_apo`, `dist_futu` needs no
 * further explicit-X wrapper), so `dist_futu<F>` is passed straight
 * through as `Dist`.
 */
template <template <class> class F, class Coalgebra, class Seed>
constexpr auto futu_via_gana(const Coalgebra &coalgebra, const Seed &seed)
    -> Fix<F> {
    using MSeed = smd::fixpoint::Free<F, Seed>;
    return gana<F, MSeed>(dist_futu<F>, coalgebra, seed);
}

// ---------------------------------------------------------------------
// ghylo -- the fused generalization (design §7.10). First cut, per the
// step file: gcata applied to gana's output, materializing the
// intermediate Fix<F>. This shipped as the materializing version: it
// passed every recovery law below cleanly on the first attempt (see
// gana's own single-lookup/single-instantiation discoveries above, which
// carry over unchanged), so no fusion attempt was needed this step.
// ---------------------------------------------------------------------

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
    return gcata<Result, WResult>(
        w_dist, algebra, gana<F, MSeed>(m_dist, coalgebra, seed));
}

} // namespace smd::fixpoint

#endif
