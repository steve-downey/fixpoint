// src/smd/fixpoint/generalized.hpp                                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_GENERALIZED
#define INCLUDED_SMD_FIXPOINT_GENERALIZED

// Generalized schemes (design §7.10/§7.11): gcata, the comonadic generalized
// catamorphism (S13), gana/ghylo, the monadic generalized anamorphism and the
// fused generalized refold (S14), and gprepro/gpostpro/zygo_histo_prepro, the
// transformed generalized folds/unfolds and Kmett's zygoHistoPrepro capstone
// (S15). gcata factors fold_fix/histo/zygo/para through a single worker
// parameterized by a distributive law (dist_laws.hpp, S12) and a comonad
// (comonad_typeclass, S03/S07); gana mirrors that shape on the unfold side,
// factoring unfold_fix/apo/futu through a distributive law and a monad
// (monad_typeclass, S03/S08); gprepro/gpostpro splice prepro.hpp's own
// `hoist<F>(e)` transformation into gcata's/gana's workers (S06's exact
// "transform on the way down/out" delta against fold_fix/unfold_fix) --
// the "recovery laws" and "degeneracy laws" below (design §9, gcata.t.cpp/
// gana.t.cpp/ghylo.t.cpp/gprepro.t.cpp) are the actual proof that the
// factoring is correct, this header's own doc comments are not.

#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/dist_laws.hpp>
#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/prepro.hpp>

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
// 25f4fc8f-7775-4c47-b2ea-7960f0dbedac
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
// 25f4fc8f-7775-4c47-b2ea-7960f0dbedac end

// ---------------------------------------------------------------------
// Recovery functions (design §9): thin wrappers pinning gcata to each
// existing scheme's own algebra shape. Each is gated in gcata.t.cpp by
// exact-answer equivalence with fold_fix/histo/zygo/para on the same
// fixtures those schemes' own steps used -- that equivalence, not this
// header's doc comments, is the actual proof the factoring is correct
// (design D8).
// ---------------------------------------------------------------------

// e34a7536-66fe-42a4-b83b-63b606224eda
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
// e34a7536-66fe-42a4-b83b-63b606224eda end

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
// ceed4381-6731-4b6d-9415-7685ce4e2730
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
// ceed4381-6731-4b6d-9415-7685ce4e2730 end

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
        return layer_fmap([](const Seed &x) { return MSeed{x}; }, coalgebra(s));
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

// 70f74abb-92b4-4ee2-9023-820b89366ebc
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
// 70f74abb-92b4-4ee2-9023-820b89366ebc end

// ---------------------------------------------------------------------
// gprepro :: Comonad w => dist -> (forall x. f x -> f x)
//                      -> (f (w a) -> a) -> t -> a
// ---------------------------------------------------------------------

/** Implementation detail of gprepro (not part of the public API): the
 * worker `c : Fix<F> -> W<F<WResult>>` (design §7.11's own `c`) --
 * gcata_worker_t's (S13) exact shape with one splice: each recursive call
 * first hoists the *child subtree* through the natural transformation `e`
 * via `hoist<F>(e, child)` (prepro.hpp, S06) before folding it. This is the
 * literal "compare §7.4's prepro vs fold_fix -- same delta" the step file
 * points at: `prepro`'s own body (prepro.hpp) recurses via
 * `prepro<Result>(e, algebra, hoist<F>(e, child))`, and gprepro's worker
 * does the same thing one level further out (`(*this)(hoist<F>(e, child))`
 * instead of `(*this)(child)`), everything else copied verbatim from
 * gcata_worker_t.
 *
 * WWR/C are computed up front exactly as gcata_worker_t's -- from
 * Result/WResult/F/Dist alone, the same "name the type before writing the
 * recursive body" discipline S07/S08/S12/S13/S14 all found necessary
 * (Nat/e plays no part in either type, only in *which* subtree gets folded
 * at each recursive step).
 */
// 49128f37-fc96-4a14-9162-c384c06ec067
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
// 49128f37-fc96-4a14-9162-c384c06ec067 end

// ---------------------------------------------------------------------
// gpostpro :: Monad m => dist -> (forall x. f x -> f x)
//                     -> (a -> f (m a)) -> a -> t
// ---------------------------------------------------------------------

/** Implementation detail of gpostpro (not part of the public API): the
 * worker `a : M<F<MSeed>> -> Fix<F>` (design §7.11) -- gana_worker_t's
 * (S14) exact shape with one splice, dual to gprepro_worker_t's: each
 * recursive *result* is hoisted through the natural transformation `e` via
 * `hoist<F>(e, (*this)(next))` before being grafted in, mirroring
 * postpro.hpp's own "unfold the child as usual, then hoist the whole
 * resulting subtree before grafting" shape (S06) one level further out.
 *
 * MFMS/MMS are computed up front exactly as gana_worker_t's.
 */
template <template <class> class F, class MSeed, class Dist, class Nat,
          class GCoalgebra>
struct gpostpro_worker_t {
    const Dist &dist;
    const Nat &e;
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
                return hoist<F>(e, (*this)(next));
            },
            dist(m)));
    }
};

/** gpostpro :: Monad m => dist -> (forall x. f x -> f x)
 *                     -> (a -> f (m a)) -> a -> t
 *
 * Dual to gprepro: gana's own `a` (design §7.10) with `hoist<F>(e)`
 * spliced around each recursive result before it is grafted in.
 *
 * Degeneracy laws (design §9/§7.11, gprepro.t.cpp, D8):
 * `gpostpro(dist_ana, identity_nat, psi') == unfold_fix(psi)`;
 * `gpostpro(dist_ana, e, psi') == postpro(e, psi)` (S06's own fixtures).
 *
 * @tparam F unary template functor (D5: explicit, mirrors gana's own F)
 * @tparam MSeed `m a` -- the monad-wrapped seed (D5: explicit, mirrors
 *   gana's own MSeed)
 * @param dist a distributive law `M<F<X>> -> F<M<X>>` for every X
 * @param e a natural transformation `F<X> -> F<X>` for every X
 * @param coalgebra `Seed -> F<MSeed>`
 */
template <template <class> class F, class MSeed, class Dist, class Nat,
          class GCoalgebra, class Seed>
constexpr auto gpostpro(const Dist &dist, const Nat &e,
                        const GCoalgebra &coalgebra, const Seed &seed)
    -> Fix<F> {
    gpostpro_worker_t<F, MSeed, Dist, Nat, GCoalgebra> worker{dist, e,
                                                              coalgebra};
    return worker(smd::typeclass::monad_typeclass<MSeed>.pure(coalgebra(seed)));
}

} // namespace smd::fixpoint

// ---------------------------------------------------------------------
// zygo_histo_prepro's composed comonad (design §7.11): W<X> =
// std::pair<Helper, Cofree<F,X>>, Kmett's `EnvT Helper (Cofree F) X`
// (Control.Comonad.Trans.Env, recursion-schemes' own zygoHistoPrepro).
// ---------------------------------------------------------------------

namespace smd::typeclass {

/** Comonad instance for the *composed* `std::pair<Helper, Cofree<F,X>>` --
 * NOT the same instance as pair.hpp's generic `comonad_typeclass<pair<B,A>>`
 * (`PairComonadImpl<B, A>`) instantiated at `A = Cofree<F,X>`. This is the
 * "thin ice" the step file names: the two give *different* `duplicate`
 * shapes, and only this one is right for gprepro's needs.
 *
 * `PairComonadImpl<B, A>::duplicate` re-nests only the *pair* layer itself:
 * `duplicate(pair<B,X>) -> pair<B, pair<B,X>>` for its own generic template
 * parameter (design's own "generic over its own element-type parameter,
 * not fixed to the instance's keying type" pattern, S03/S07/S12/S13/S14).
 * Plugging in `A = Cofree<F,X>`/its own generic param `= Cofree<F,X>` gives
 * `pair<Helper, pair<Helper, Cofree<F,X>>>` -- a visibly different type from
 * what gcata's/gprepro's worker needs here, `W<W<X>> = pair<Helper,
 * Cofree<F, pair<Helper, Cofree<F,X>>>>` (`comonad_typeclass<WResult>
 * .duplicate(declval<WResult>())`, generalized.hpp's `gprepro_worker_t`).
 * These two types do not match (`pair<Helper,Cofree<F,X>>` vs
 * `Cofree<F,pair<Helper,Cofree<F,X>>>` nested one level differently), so the
 * generic pair instance cannot serve gprepro's needs at this W -- a
 * dedicated instance, keyed specifically on `pair<Helper, Cofree<F,X>>`
 * (more specialized than pair.hpp's `pair<B,A>` pattern, so this partial
 * specialization is selected whenever `A` actually is `Cofree<F,X>` for
 * some F/X), is required.
 *
 * This mirrors Haskell's `EnvT e w` Comonad instance
 * (Control.Comonad.Trans.Env): `duplicate (EnvT e wa) = EnvT e (extend
 * (EnvT e) wa)`, i.e. re-attach the *same* environment `e` to every
 * position of the *underlying* comonad `w`'s own duplicate -- using `w`'s
 * (here Cofree's) own `duplicate`/`fmap`, not a trivial re-pairing of the
 * outer product. `extract`/`fmap` simply defer to the underlying Cofree
 * comonad's own operations, threading `Helper` through unchanged (`fmap`)
 * or dropping it (`extract`), exactly as `EnvT`'s own `Functor`/`Comonad`
 * instances do.
 */
// 30ef7d6e-fd4b-4ef9-8e98-872e5f1dfe88
template <template <class> class F, class Helper, class X>
struct ZygoHistoComonadImpl {
    template <class Y>
    constexpr auto
    extract(this auto &&,
            const std::pair<Helper, smd::fixpoint::Cofree<F, Y>> &w)
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
        return std::pair<
            Helper, smd::fixpoint::Cofree<
                        F, std::pair<Helper, smd::fixpoint::Cofree<F, Y>>>>{
            env, std::move(reattached)};
    }

    template <class Fn, class Y>
    constexpr auto
    fmap(this auto &&, Fn &&fn,
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
// 30ef7d6e-fd4b-4ef9-8e98-872e5f1dfe88 end

} // namespace smd::typeclass

namespace smd::fixpoint {

// ---------------------------------------------------------------------
// dist_zygo_histo(f) :: (f b -> b) -> f (EnvT b (Cofree f) a)
//                     -> EnvT b (Cofree f) (f a)
// ---------------------------------------------------------------------

/** The object returned by dist_zygo_histo<F>(f) (factory, see below): a
 * one-off distributive law for zygo_histo_prepro's own composed comonad
 * `W<X> = std::pair<Helper, Cofree<F,X>>` (design §7.11 -- capstone
 * specific, deliberately NOT added to dist_laws.hpp, per the step file).
 * Mirrors Kmett's `distZygoT`: `distZygoT g k fe = EnvT (g (extract <$>
 * fe)) (k (lower <$> fe))` with `k` bound here to `dist_histo<F>` --
 * `g (helper) : F<Helper> -> Helper` folds the helper component (`.first`,
 * matching S05/zygo's own convention), `dist_histo<F>` (S12) redistributes
 * the Cofree component (`.second`).
 *
 * Like `dist_para`/`dist_histo`/`dist_futu` (dist_laws.hpp, DEV-02), `F`
 * must be bound explicitly at the struct level (called as
 * `dist_zygo_histo<F>(f)`): the call operator's own argument type contains
 * a *bare* `F<...>` application (an alias-template functor family), which
 * GCC 16 cannot deduce a template-template parameter back out of. `Helper`
 * and the per-call element type `X` both deduce normally from the argument
 * once `F` is bound, mirroring `dist_histo_t`'s own `A`.
 */
// b830487f-689d-498c-ada1-47728dba1581
template <template <class> class F, class HelperAlg>
struct dist_zygo_histo_t {
    HelperAlg helper;

    template <class Helper, class X>
    constexpr auto operator()(const F<std::pair<Helper, Cofree<F, X>>> &l) const
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
// b830487f-689d-498c-ada1-47728dba1581 end

// ---------------------------------------------------------------------
// zygo_histo_prepro -- Kmett's famous capstone (design §7.11).
// ---------------------------------------------------------------------

// 4bdc612f-bf67-40a5-8f3d-e276ffed7e41
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
template <class Result, class Helper, template <class> class F, class HelperAlg,
          class Nat, class MainAlg>
constexpr auto zygo_histo_prepro(const HelperAlg &f, const Nat &e,
                                 const MainAlg &g, const Fix<F> &tree)
    -> Result {
    using WResult = std::pair<Helper, Cofree<F, Result>>;
    return gprepro<Result, WResult>(dist_zygo_histo<F>(f), e, g, tree);
}
// 4bdc612f-bf67-40a5-8f3d-e276ffed7e41 end

} // namespace smd::fixpoint

#endif
