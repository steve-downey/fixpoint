// src/smd/fixpoint/zygo.hpp                                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_ZYGO
#define INCLUDED_SMD_FIXPOINT_ZYGO

// Zygomorphism (design §7.3): a fold with an auxiliary "helper" fold running
// alongside it. The main algebra sees, at every child position, both the
// helper's value and the main fold's result for that child.
//
// Implemented via pairing (the banana-split construction): a single
// fold_fix with carrier std::pair<Helper, Result> and combined algebra
// `λx. pair(helper(fmapF(first, x)), main(x))`, then `.second` of the
// overall result.

#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <utility>

namespace smd::fixpoint {

/** zygo :: (f b -> b) -> (f (b, a) -> a) -> t -> a
 *
 * Equation: zygo f g = second ∘ fold_fix(λx. pair(f(fmapF(first, x)), g(x)))
 *
 * Convention (design §7.3, matches dist_zygo in S12): the carrier pair is
 * `{helper value, main value}` — **helper first, main second**. This is the
 * opposite slot order from para's `std::pair<Fix<F>, Result>` (original
 * subtree first, fold result second); the two pairs serve different
 * purposes and are not to be confused.
 *
 * @tparam Result main fold's carrier (D5: explicit)
 * @tparam Helper helper fold's carrier (D5: explicit)
 * @param helper F<Helper> -> Helper
 * @param main   F<std::pair<Helper, Result>> -> Result
 */
template <class Result, class Helper, template <class> class F,
          class HelperAlg, class MainAlg>
constexpr auto zygo(const HelperAlg &helper, const MainAlg &main,
                    const Fix<F> &tree) -> Result {
    using Carrier = std::pair<Helper, Result>;
    auto combined = [&](const F<Carrier> &layer) -> Carrier {
        // A *second* fmap over the already fold_fix-mapped layer, projecting
        // just the helper component at each child position — inherent to
        // the pairing construction, not a fusion opportunity (design §7.3
        // pitfall).
        auto helper_layer = layer_fmap(
            [](const Carrier &c) -> Helper { return c.first; }, layer);
        return Carrier{helper(helper_layer), main(layer)};
    };
    return fold_fix<Carrier>(combined, tree).second;
}

/** zygo threading an explicit functor instance (OVERLOAD variant, same name).
 *
 * Distinguished from the 3-arg zygo above by *arity* (4 args), not by a
 * constraint -- so unlike fold_fix/unfold_fix/refold, reusing the name here
 * costs nothing and this keeps a static_assert for a clean diagnostic. That
 * asymmetry is the point: the overload-vs-distinct-name tension only bites
 * where an added instance-first argument collides in arity with an existing
 * overload (the 3-arg fmap_fn schemes); where arities differ, overloading is
 * free.
 *
 * The threaded @p functor is used at both fmap sites and so must be
 * element-generic (fold layer F<Fix<F>> vs projection layer F<Carrier>).
 */
template <class Result, class Helper, class Functor, template <class> class F,
          class HelperAlg, class MainAlg>
constexpr auto zygo(const Functor &functor, const HelperAlg &helper,
                    const MainAlg &main, const Fix<F> &tree) -> Result {
    using Carrier = std::pair<Helper, Result>;
    static_assert(
        functor_instance_for<Functor, Helper (*)(const Carrier &), F<Carrier>>,
        "zygo: the functor instance cannot project the helper component -- it "
        "has no fmap(fn, F<pair<Helper,Result>>). Pass an element-generic "
        "functor object usable at both fmap sites.");
    auto combined = [&](const F<Carrier> &layer) -> Carrier {
        auto helper_layer = functor.fmap(
            [](const Carrier &c) -> Helper { return c.first; }, layer);
        return Carrier{helper(helper_layer), main(layer)};
    };
    return fold_fix<Carrier>(functor, combined, tree).second;
}

} // namespace smd::fixpoint

#endif
