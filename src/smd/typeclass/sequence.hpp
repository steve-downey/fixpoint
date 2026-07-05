// src/smd/typeclass/sequence.hpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_TYPECLASS_SEQUENCE
#define INCLUDED_SMD_TYPECLASS_SEQUENCE

// Foldable and Traversable instances for std::vector.
//
// std::vector is the shared traversable *structure* for all three of Paper A's
// motivating domains. Pairing this one Traversable instance with three
// different applicative contexts gives the three front-door results through a
// single API:
//
//   traverse(f, vector<optional<T>>) -> optional<vector<T>>   (fallible value)
//   traverse(f, vector<sender<T>>)   -> sender<vector<T>>     (deferred result)
//   traverse(f, vector<zip_list<T>>) -> zip_list<vector<T>>   (lanewise / SIMD)
//
// The traverse primitive sequences each element's effect left-to-right, lifting
// element results back into a vector while preserving the original shape.

#include <smd/typeclass/apply.hpp>
#include <smd/typeclass/detail/typeclass_base.hpp>
#include <smd/typeclass/fold.hpp>
#include <smd/typeclass/monoid.hpp>
#include <smd/typeclass/traverse.hpp>

#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace smd::typeclass {

/** Foldable instance for std::vector: fold_map accumulates left-to-right. */
template <class VALUE_TYPE>
struct VectorFoldableImpl {
    template <class FUNCTION>
    auto fold_map(this auto &&, FUNCTION &&function,
                  const std::vector<VALUE_TYPE> &values) {
        using Result =
            remove_cvref_t<std::invoke_result_t<FUNCTION, const VALUE_TYPE &>>;
        auto accumulated = monoid_identity<Result>();
        for (const auto &value : values) {
            accumulated = monoid_combine(std::move(accumulated),
                                         std::invoke(function, value));
        }
        return accumulated;
    }
};

template <class VALUE_TYPE>
struct VectorFoldableMap : Foldable<VectorFoldableImpl<VALUE_TYPE>> {
    using VectorFoldableImpl<VALUE_TYPE>::fold_map;
};

/** Foldable instance for `std::vector<VALUE_TYPE>`. */
template <class VALUE_TYPE>
inline constexpr auto foldable_typeclass<std::vector<VALUE_TYPE>> =
    VectorFoldableMap<VALUE_TYPE>{};

/** Traversable instance for std::vector.
 * The primitive `traverse` sequences each element's effect via the supplied
 * applicative, collecting the in-context element results into a vector while
 * preserving the original element order and count.
 */
template <class VALUE_TYPE>
struct VectorTraversableImpl {
    using element_type = VALUE_TYPE;

    template <class APPLICATIVE, class FUNCTION>
    auto traverse(this auto &&, const APPLICATIVE &applicative,
                  FUNCTION &&function, const std::vector<VALUE_TYPE> &values) {
        using Effect =
            remove_cvref_t<std::invoke_result_t<FUNCTION, const VALUE_TYPE &>>;
        using Element = applicative_value_t<Effect>;

        auto accumulated = applicative.pure(std::vector<Element>{});
        for (const auto &value : values) {
            accumulated = applicative.invoke(
                [](std::vector<Element> collected, const Element &element) {
                    collected.push_back(element);
                    return collected;
                },
                accumulated, std::invoke(function, value));
        }
        return accumulated;
    }
};

template <class VALUE_TYPE>
struct VectorTraversableMap : Traversable<VectorTraversableImpl<VALUE_TYPE>> {
    using VectorTraversableImpl<VALUE_TYPE>::traverse;
};

/** Traversable instance for `std::vector<VALUE_TYPE>`. */
template <class VALUE_TYPE>
inline constexpr auto traversable_typeclass<std::vector<VALUE_TYPE>> =
    VectorTraversableMap<VALUE_TYPE>{};

} // namespace smd::typeclass

#endif // INCLUDED_SMD_TYPECLASS_SEQUENCE
