// src/smd/fixpoint/fmap.hpp                                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_FMAP
#define INCLUDED_SMD_FIXPOINT_FMAP

#include <smd/typeclass/functor.hpp>

#include <utility>

namespace smd::fixpoint {

/** Look up the `functor_typeclass` instance for the concrete layer type of
 * @p layer (keyed per smd::typeclass::functor_typeclass, D2) and apply it.
 *
 * This is the single point where scheme bodies bridge into
 * `smd::typeclass` functor dispatch: instead of threading an explicit
 * `fmap_fn` through every scheme, they call `layer_fmap(fn, layer)` and
 * the typeclass lookup keyed on `std::remove_cvref_t<Layer>` finds the
 * right `fmap`. A layer type without an instance produces the
 * `functor_typeclass` static_assert diagnostic.
 */
template <class Fn, class Layer>
constexpr auto layer_fmap(Fn &&fn, const Layer &layer) {
    return smd::typeclass::functor_typeclass<std::remove_cvref_t<Layer>>.fmap(
        std::forward<Fn>(fn), layer);
}

} // namespace smd::fixpoint

#endif
