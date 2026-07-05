// src/smd/typeclass/monoid_explicit.hpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// EXPERIMENT (branch experiment-fold-fix-object): the thread-by-value
// *contract* for typeclass algorithms -- a value-based local instance is
// ALWAYS a provided API, so no caller is ever stuck when the global lookup is
// wrong or missing.
//
// Monoid over `int` is the case that makes this non-optional. There is no
// canonical default: sum, product, min, and max are all equally valid
// monoids over int. The library's monoid_v<int> picks additive, which is
// merely *one* choice imposed on everyone; combine_all can then only ever
// sum. Haskell disambiguates with newtypes (Sum, Product, Min, Max), but C++
// has no transparent, zero-overhead monomorphizing newtype -- wrapping would
// mean a wrapper-type explosion and conversions at every boundary. Passing
// the instance by value sidesteps all of it: one vector<int>, folded four
// ways, no wrappers, no global mutation.
#ifndef INCLUDED_SMD_TYPECLASS_MONOID_EXPLICIT
#define INCLUDED_SMD_TYPECLASS_MONOID_EXPLICIT

#include <type_traits>
#include <utility>

namespace smd::typeclass::experimental {

/** In-context check that @p M is a Monoid over @p T: it offers `identity()`
 * and `combine(T, T)`. A plain compile-time bool for `static_assert` -- not a
 * requires-clause. Threading an instance is one operation with no overload to
 * select, so a bad instance should produce one direct diagnostic, not SFINAE
 * candidate spew. */
template <class M, class T>
constexpr bool is_monoid_over =
    requires(const M &monoid, const T &a, const T &b) {
        monoid.identity();
        monoid.combine(a, b);
    };

/** Fold @p xs into the monoid @p monoid, mapping each element through @p fn
 * first. The monoid instance is threaded by value; nothing is looked up and
 * nothing need be registered. */
template <class Monoid, class Fn, class Range>
constexpr auto fold_map_with(const Monoid &monoid, Fn &&fn, const Range &xs) {
    // Guard identity() first, before it is used to deduce the carrier, so a
    // non-monoid leads with this message instead of a raw "no member" error.
    static_assert(requires(const Monoid &m) { m.identity(); },
                  "fold_map_with: the instance has no identity() -- it is not "
                  "a Monoid.");
    auto acc = monoid.identity();
    using Carrier = decltype(acc);
    static_assert(
        is_monoid_over<Monoid, Carrier>,
        "fold_map_with: the instance has no combine(m, m) over its "
        "identity()'s type -- it is not a Monoid.");
    for (const auto &x : xs) {
        acc = monoid.combine(std::move(acc), fn(x));
    }
    return acc;
}

/** Combine every element of @p xs under the explicit monoid @p monoid
 * (elements are already the monoid's carrier). */
template <class Monoid, class Range>
constexpr auto combine_all_with(const Monoid &monoid, const Range &xs) {
    return fold_map_with(
        monoid, [](const auto &x) { return x; }, xs);
}

} // namespace smd::typeclass::experimental

#endif
