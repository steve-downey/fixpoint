// src/smd/typeclass/monoid.hpp                                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_TYPECLASS_MONOID
#define INCLUDED_SMD_TYPECLASS_MONOID

#include <smd/typeclass/detail/typeclass_base.hpp>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace smd::typeclass {

// Monoid pattern invariants:
// - Monoid<T> is the customization point; specialize identity/combine together.
// - monoid_v<T> is the canonical lookup object used by generic algorithms.
// - identity and combine must stay coherent for a single associative law
//   domain.
// - Prefer adding new Monoid<T> specializations over ad hoc free functions.

/** Customization point for the Monoid typeclass.
 * Specialize this struct for type `VALUE_TYPE` and provide
 * `identity()` and `combine(lhs, rhs)` to make that type usable
 * wherever a Monoid is required (e.g., as the result type of fold_map).
 */
template <class VALUE_TYPE>
struct Monoid;

/** Canonical lookup object for Monoid<VALUE_TYPE>; used by generic algorithms.
 */
template <class VALUE_TYPE>
inline constexpr Monoid<VALUE_TYPE> monoid_v = Monoid<VALUE_TYPE>{};

/** Opaque count accumulator; the Monoid combines by addition. */
struct Count {
    std::size_t d_value;

    friend constexpr bool operator==(const Count &lhs,
                                     const Count &rhs) = default;
};

/** Monoid<Count>: identity is zero, combine adds counts. */
template <>
struct Monoid<Count> {
    constexpr auto identity() const -> Count { return Count{0}; }

    constexpr auto combine(const Count &lhs, const Count &rhs) const -> Count {
        return Count{lhs.d_value + rhs.d_value};
    }
};

/** Monoid<int>: additive monoid with identity 0. */
template <>
struct Monoid<int> {
    constexpr auto identity() const -> int { return 0; }

    constexpr auto combine(int lhs, int rhs) const -> int { return lhs + rhs; }
};

/** Monoid<long>: additive monoid with identity 0. */
template <>
struct Monoid<long> {
    constexpr auto identity() const -> long { return 0L; }

    constexpr auto combine(long lhs, long rhs) const -> long {
        return lhs + rhs;
    }
};

/** Monoid<std::size_t>: additive monoid with identity 0. */
template <>
struct Monoid<std::size_t> {
    constexpr auto identity() const -> std::size_t { return 0U; }

    constexpr auto combine(std::size_t lhs, std::size_t rhs) const
        -> std::size_t {
        return lhs + rhs;
    }
};

/** Monoid<std::string>: concatenation monoid with identity "". */
template <>
struct Monoid<std::string> {
    auto identity() const -> std::string { return {}; }

    auto combine(const std::string &lhs, const std::string &rhs) const
        -> std::string {
        return lhs + rhs;
    }
};

/** Monoid<std::vector<T>>: concatenation monoid with identity empty vector. */
template <class VALUE_TYPE>
struct Monoid<std::vector<VALUE_TYPE>> {
    auto identity() const -> std::vector<VALUE_TYPE> { return {}; }

    auto combine(std::vector<VALUE_TYPE> lhs,
                 const std::vector<VALUE_TYPE> &rhs) const
        -> std::vector<VALUE_TYPE> {
        lhs.insert(lhs.end(), rhs.begin(), rhs.end());
        return lhs;
    }
};

/** Returns the identity element for the Monoid of VALUE_TYPE. */
template <class VALUE_TYPE>
auto monoid_identity() -> VALUE_TYPE {
    return monoid_v<VALUE_TYPE>.identity();
}

/** Combines two values using the Monoid of VALUE_TYPE. */
template <class VALUE_TYPE>
auto monoid_combine(const VALUE_TYPE &lhs, const VALUE_TYPE &rhs)
    -> VALUE_TYPE {
    return monoid_v<VALUE_TYPE>.combine(lhs, rhs);
}

} // namespace smd::typeclass

#endif // INCLUDED_SMD_TYPECLASS_MONOID
