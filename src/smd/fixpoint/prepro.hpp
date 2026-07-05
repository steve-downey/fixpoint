// src/smd/fixpoint/prepro.hpp                                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_PREPRO
#define INCLUDED_SMD_FIXPOINT_PREPRO

// hoist, prepro, postpro (design §7.4): natural-transformation plumbing.
//
// A *natural transformation* here is any polymorphic function object
// callable as `e(F<X>) -> F<X>` (or `-> G<X>` for hoist) for *every* X — not
// just the one concrete X a particular call site happens to instantiate.
// Fixture transformations therefore need a templated call operator, e.g.:
//
//   struct identity_nat {
//       template <class A>
//       constexpr auto operator()(const NatF<A> &layer) const -> NatF<A> {
//           return layer;
//       }
//   };
//
// (design §4's `cap_layer` example is the same shape). A lambda with a
// concrete layer parameter type is *not* a natural transformation: prepro
// below applies `e` at `F<Fix<F>>` (via hoist, over whole subtrees), not at
// the carrier type the surrounding algebra/coalgebra uses, so a
// non-templated call operator will fail to compile or silently pick the
// wrong overload.

#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <utility>

namespace smd::fixpoint {

/** hoist :: (forall x. f x -> g x) -> Fix f -> Fix g
 *
 * Equation: hoist e = fold_fix(wrap_fix<G> ∘ e)
 *
 * Retags every layer of the tree from functor @p F to functor @p G via the
 * natural transformation @p e, applied bottom-up (a fold whose algebra is
 * "transform this layer, then wrap it"). For the common endo case (@p G ==
 * @p F, i.e. rewriting a tree's layers without changing its functor), the
 * target functor still has to be given explicitly since it cannot be
 * deduced from @p e alone: call as `hoist<F>(e, t)`.
 *
 * @tparam G target functor (D5: explicit, cannot be deduced from @p e)
 * @tparam F source functor (deduced from @p tree)
 * @param e F<X> -> G<X> for every X
 */
template <template <class> class G, template <class> class F, class Nat>
constexpr auto hoist(const Nat &e, const Fix<F> &tree) -> Fix<G> {
    return fold_fix<Fix<G>>(
        [&](const F<Fix<G>> &layer) -> Fix<G> {
            return wrap_fix<G>(e(layer));
        },
        tree);
}

/** prepro (Fokkinga) :: (forall x. f x -> f x) -> (f a -> a) -> Fix f -> a
 *
 * Equation: prepro e φ = φ ∘ fmapF (prepro e φ ∘ hoist<F>(e)) ∘ unfix
 *
 * A fold whose algebra sees, at each child, the result of first
 * rewriting the *entire child subtree* via `hoist<F>(e)` and then
 * recursing prepro on the rewritten subtree. This is the "transform on
 * the way down" dual to postpro's "transform on the way out".
 *
 * Cumulative-cost note: because each recursive step re-hoists the whole
 * remaining subtree before descending into it, a node at depth k has had
 * @p e applied to it k times by the time prepro's algebra sees its
 * layer (once per ancestor, each hoist itself walking every layer below
 * it). This is the classical (and expensive) reading of prepro — Fokkinga's
 * equation is transcribed literally here, not fused/memoized. At the small
 * tree sizes exercised by this module's tests it is not observable, but it
 * is worth knowing before reaching for prepro over anything but small or
 * shallow trees.
 *
 * @tparam Result carrier type (D5: explicit)
 * @tparam F functor (deduced from @p tree)
 * @param e F<X> -> F<X> for every X (a natural transformation, endo case)
 * @param algebra F<Result> -> Result
 */
template <class Result, template <class> class F, class Nat, class Algebra>
constexpr auto prepro(const Nat &e, const Algebra &algebra,
                      const Fix<F> &tree) -> Result {
    const auto &layer = unwrap_fix(tree);
    auto evaluated = layer_fmap(
        [&](const Fix<F> &child) -> Result {
            return prepro<Result>(e, algebra, hoist<F>(e, child));
        },
        layer);
    return algebra(evaluated);
}

/** postpro :: (forall x. f x -> f x) -> (a -> f a) -> a -> Fix f
 *
 * Equation: postpro e ψ = fix ∘ fmapF (hoist<F>(e) ∘ postpro e ψ) ∘ ψ
 *
 * Dual to prepro: an unfold whose coalgebra's children are first unfolded
 * as usual, then the *entire resulting subtree* is rewritten via
 * `hoist<F>(e)` before being grafted in. Same cumulative-cost shape as
 * prepro, mirrored: a node at depth k has been hoisted k times by the time
 * it is grafted into the final tree.
 *
 * @tparam F functor (D5: explicit, cannot be deduced from @p seed alone)
 * @param e F<X> -> F<X> for every X (a natural transformation, endo case)
 * @param coalgebra Seed -> F<Seed>
 */
template <template <class> class F, class Nat, class Coalgebra, class Seed>
constexpr auto postpro(const Nat &e, const Coalgebra &coalgebra,
                       const Seed &seed) -> Fix<F> {
    auto layer = coalgebra(seed);
    auto expanded = layer_fmap(
        [&](const Seed &child) -> Fix<F> {
            return hoist<F>(e, postpro<F>(e, coalgebra, child));
        },
        layer);
    return wrap_fix<F>(std::move(expanded));
}

// -- Explicit-instance `_with` forms (design D12) --------------------------

/** hoist threading an explicit functor instance; delegates to
 * fold_fix_with (which validates the instance). */
template <template <class> class G, template <class> class F, class Functor,
          class Nat>
constexpr auto hoist_with(const Functor &functor, const Nat &e,
                          const Fix<F> &tree) -> Fix<G> {
    return fold_fix_with<Fix<G>>(
        functor,
        [&](const F<Fix<G>> &layer) -> Fix<G> { return wrap_fix<G>(e(layer)); },
        tree);
}

/** prepro threading an explicit functor instance. */
template <class Result, template <class> class F, class Functor, class Nat,
          class Algebra>
constexpr auto prepro_with(const Functor &functor, const Nat &e,
                           const Algebra &algebra, const Fix<F> &tree)
    -> Result {
    static_assert(
        functor_maps_to<Functor, F<Fix<F>>, Result>,
        "prepro_with: the functor instance has no fmap(fn, F<Fix<F>>) for "
        "this layer -- pass a functor typeclass object for F.");
    auto evaluated = functor.fmap(
        [&](const Fix<F> &child) -> Result {
            return prepro_with<Result>(functor, e, algebra,
                                       hoist_with<F>(functor, e, child));
        },
        unwrap_fix(tree));
    return algebra(evaluated);
}

/** postpro threading an explicit functor instance. */
template <template <class> class F, class Functor, class Nat, class Coalgebra,
          class Seed>
constexpr auto postpro_with(const Functor &functor, const Nat &e,
                            const Coalgebra &coalgebra, const Seed &seed)
    -> Fix<F> {
    auto layer = coalgebra(seed);
    static_assert(
        functor_maps_to<Functor, std::remove_cvref_t<decltype(layer)>, Fix<F>>,
        "postpro_with: the functor instance has no fmap(fn, F<Seed>) for the "
        "coalgebra's layer -- pass a functor typeclass object for F.");
    auto expanded = functor.fmap(
        [&](const Seed &child) -> Fix<F> {
            return hoist_with<F>(functor, e,
                                 postpro_with<F>(functor, e, coalgebra, child));
        },
        layer);
    return wrap_fix<F>(std::move(expanded));
}

} // namespace smd::fixpoint

#endif
