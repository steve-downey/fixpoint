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

} // namespace smd::fixpoint

#endif
