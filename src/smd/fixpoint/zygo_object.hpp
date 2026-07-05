// src/smd/fixpoint/zygo_object.hpp                                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// EXPERIMENT (branch experiment-fold-fix-object): zygo with the functor
// instance threaded by value (`_with` form), the stress test for the
// thread-by-value shape. Unlike a single-operation scheme, zygo touches the
// functor at TWO fmap sites -- the delegated fold_fix recursion AND the local
// helper-projection inside `combined` -- both over the same functor F. That
// is precisely the case where "name the object once, hand it to both places"
// is the natural spelling, and where an inherit-from-instance object buys
// nothing: zygo is not itself recursive (it delegates recursion to fold_fix),
// so there is no `this->fmap` recursion body to host.
#ifndef INCLUDED_SMD_FIXPOINT_ZYGO_OBJECT
#define INCLUDED_SMD_FIXPOINT_ZYGO_OBJECT

#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/fold_fix_object.hpp>

#include <utility>

namespace smd::fixpoint::experimental {

/** zygo_with :: functor -> (f b -> b) -> (f (b, a) -> a) -> t -> a
 *
 * Same construction as the in-tree zygo (banana-split: a single fold with
 * carrier std::pair<Helper, Result>), but the functor instance @p functor is
 * supplied explicitly and used at both fmap sites, so it need not be
 * registered in functor_typeclass at all.
 *
 * @tparam Result main fold's carrier (explicit)
 * @tparam Helper helper fold's carrier (explicit)
 */
template <class Result, class Helper, class Functor, template <class> class F,
          class HelperAlg, class MainAlg>
constexpr auto zygo_with(const Functor &functor, const HelperAlg &helper,
                         const MainAlg &main, const Fix<F> &tree) -> Result {
    using Carrier = std::pair<Helper, Result>;

    // Direct, in-context validation of the projection fmap site (the fold
    // site is validated inside fold_fix_threaded). static_assert, not a
    // requires-clause: a bad instance should say exactly this.
    static_assert(
        functor_maps<Functor, Helper (*)(const Carrier &), F<Carrier>>,
        "zygo_with: the functor instance cannot project the helper "
        "component -- it has no fmap(fn, F<pair<Helper,Result>>).");

    auto combined = [&](const F<Carrier> &layer) -> Carrier {
        auto helper_layer = functor.fmap(
            [](const Carrier &c) -> Helper { return c.first; }, layer);
        return Carrier{helper(helper_layer), main(layer)};
    };
    return fold_fix_threaded<Carrier>(functor, combined, tree).second;
}

} // namespace smd::fixpoint::experimental

#endif
