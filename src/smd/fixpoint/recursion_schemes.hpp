// src/smd/fixpoint/recursion_schemes.hpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_RECURSION_SCHEMES
#define INCLUDED_SMD_FIXPOINT_RECURSION_SCHEMES

#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/fmap.hpp>

namespace smd::fixpoint {

template <typename Result, template <typename> class F, typename Algebra,
          typename FMap>
constexpr auto fold_fix(const Algebra &algebra, const FMap &fmap_fn,
                        const Fix<F> &tree) -> Result {
    const auto &layer = unwrap_fix(tree);
    auto evaluated = fmap_fn(
        [&](const Fix<F> &child) -> Result {
            return fold_fix<Result>(algebra, fmap_fn, child);
        },
        layer);
    return algebra(evaluated);
}

template <template <typename> class F, typename Coalgebra, typename FMap,
          typename Seed>
constexpr auto unfold_fix(const Coalgebra &coalgebra, const FMap &fmap_fn,
                          const Seed &seed) -> Fix<F> {
    auto layer = coalgebra(seed);
    auto expanded = fmap_fn(
        [&](const Seed &child) -> Fix<F> {
            return unfold_fix<F>(coalgebra, fmap_fn, child);
        },
        layer);
    return wrap_fix<F>(std::move(expanded));
}

template <typename Result, template <typename> class F, typename Algebra,
          typename Coalgebra, typename FMap, typename Seed>
constexpr auto refold(const Algebra &algebra, const Coalgebra &coalgebra,
                      const FMap &fmap_fn, const Seed &seed) -> Result {
    auto layer = coalgebra(seed);
    auto evaluated = fmap_fn(
        [&](const Seed &child) -> Result {
            return refold<Result, F>(algebra, coalgebra, fmap_fn, child);
        },
        layer);
    return algebra(evaluated);
}

template <typename Result, template <typename> class F, typename Algebra,
          typename FMap>
[[deprecated("use fold_fix")]]
auto cata(const Algebra &algebra, const FMap &fmap_fn, const Fix<F> &tree)
    -> Result {
    return fold_fix<Result>(algebra, fmap_fn, tree);
}

// Lookup-based overloads (design §6.2): dispatch fmap through
// smd::typeclass::functor_typeclass via layer_fmap (design §4) instead of
// taking an explicit fmap_fn.

template <typename Result, template <typename> class F, typename Algebra>
constexpr auto fold_fix(const Algebra &algebra, const Fix<F> &tree)
    -> Result {
    const auto &layer = unwrap_fix(tree);
    auto evaluated = layer_fmap(
        [&](const Fix<F> &child) -> Result {
            return fold_fix<Result>(algebra, child);
        },
        layer);
    return algebra(evaluated);
}

template <template <typename> class F, typename Coalgebra, typename Seed>
constexpr auto unfold_fix(const Coalgebra &coalgebra, const Seed &seed)
    -> Fix<F> {
    auto layer = coalgebra(seed);
    auto expanded = layer_fmap(
        [&](const Seed &child) -> Fix<F> {
            return unfold_fix<F>(coalgebra, child);
        },
        layer);
    return wrap_fix<F>(std::move(expanded));
}

template <typename Result, template <typename> class F, typename Algebra,
          typename Coalgebra, typename Seed>
constexpr auto refold(const Algebra &algebra, const Coalgebra &coalgebra,
                      const Seed &seed) -> Result {
    auto layer = coalgebra(seed);
    auto evaluated = layer_fmap(
        [&](const Seed &child) -> Result {
            return refold<Result, F>(algebra, coalgebra, child);
        },
        layer);
    return algebra(evaluated);
}

// Explicit-instance overloads (the `_with` forms): thread a functor instance
// by value instead of looking it up in functor_typeclass. The instance is the
// first argument and flows unchanged down the recursion, so it need not be
// registered anywhere -- the standing contract that a value-based local
// typeclass instance is always a provided API, so a caller is never stuck when
// the global lookup is wrong (no sensible default) or absent (an unregistered,
// test-local, or ODR-sensitive type).
//
// A distinct name, not an overload of the lookup form: threading is the *same*
// operation regardless of where the instance comes from, so there is no
// dispatch to resolve -- and a fresh name sidesteps any collision with the
// explicit-fmap_fn overloads above. The instance is validated with a
// static_assert (functor_instance_for, from fmap.hpp) rather than a
// requires-clause: we are asserting a requirement, not selecting an overload,
// so a bad instance earns one direct diagnostic instead of SFINAE spew.

/** Catamorphism threading an explicit functor instance. */
template <typename Result, template <typename> class F, class Functor,
          typename Algebra>
constexpr auto fold_fix_with(const Functor &functor, const Algebra &algebra,
                             const Fix<F> &tree) -> Result {
    static_assert(
        functor_maps_to<Functor, F<Fix<F>>, Result>,
        "fold_fix_with: the functor instance has no fmap(fn, F<Fix<F>>) for "
        "this layer -- pass a functor typeclass object for F.");
    auto evaluated = functor.fmap(
        [&](const Fix<F> &child) -> Result {
            return fold_fix_with<Result>(functor, algebra, child);
        },
        unwrap_fix(tree));
    return algebra(evaluated);
}

/** Anamorphism threading an explicit functor instance. */
template <template <typename> class F, class Functor, typename Coalgebra,
          typename Seed>
constexpr auto unfold_fix_with(const Functor &functor,
                               const Coalgebra &coalgebra, const Seed &seed)
    -> Fix<F> {
    auto layer = coalgebra(seed);
    static_assert(
        functor_maps_to<Functor, std::remove_cvref_t<decltype(layer)>, Fix<F>>,
        "unfold_fix_with: the functor instance has no fmap(fn, F<Seed>) for "
        "the coalgebra's layer -- pass a functor typeclass object for F.");
    auto expanded = functor.fmap(
        [&](const Seed &child) -> Fix<F> {
            return unfold_fix_with<F>(functor, coalgebra, child);
        },
        layer);
    return wrap_fix<F>(std::move(expanded));
}

/** Hylomorphism (refold) threading an explicit functor instance. */
template <typename Result, template <typename> class F, class Functor,
          typename Algebra, typename Coalgebra, typename Seed>
constexpr auto refold_with(const Functor &functor, const Algebra &algebra,
                           const Coalgebra &coalgebra, const Seed &seed)
    -> Result {
    auto layer = coalgebra(seed);
    static_assert(
        functor_maps_to<Functor, std::remove_cvref_t<decltype(layer)>, Result>,
        "refold_with: the functor instance has no fmap(fn, F<Seed>) for the "
        "coalgebra's layer -- pass a functor typeclass object for F.");
    auto evaluated = functor.fmap(
        [&](const Seed &child) -> Result {
            return refold_with<Result, F>(functor, algebra, coalgebra, child);
        },
        layer);
    return algebra(evaluated);
}

} // namespace smd::fixpoint

#endif
