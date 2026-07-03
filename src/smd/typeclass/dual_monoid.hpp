// src/smd/typeclass/dual_monoid.hpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_TYPECLASS_DUAL_MONOID
#define INCLUDED_SMD_TYPECLASS_DUAL_MONOID

// DualMonoid<M> wraps a monoidal value M and flips the argument order of
// combine.  The identity element is the same as M's identity.
//
// This is the standard algebraic dual: if (M, ·, e) is a monoid then
// (M, ·ᵒᵖ, e) where a ·ᵒᵖ b = b · a is also a monoid.
//
// split-by-measure style operations require >= on the measure type;
// DualMonoid<M> forwards >= from M so those operations work on reversed
// structures.

#include <smd/typeclass/monoid.hpp>

#include <concepts>

namespace smd::typeclass {

template <typename M>
struct DualMonoid {
    M value;
    friend auto operator==(const DualMonoid &, const DualMonoid &)
        -> bool = default;

    // Forward >= from M so split-by-measure compiles on reversed structures.
    friend auto operator>=(const DualMonoid &lhs, const DualMonoid &rhs) -> bool
        requires requires(const M &a, const M &b) {
            { a >= b } -> std::convertible_to<bool>;
        }
    {
        return lhs.value >= rhs.value;
    }
};

template <typename M>
struct Monoid<DualMonoid<M>> {
    constexpr auto identity() const -> DualMonoid<M> {
        return DualMonoid<M>{monoid_v<M>.identity()};
    }
    constexpr auto combine(const DualMonoid<M> &lhs,
                           const DualMonoid<M> &rhs) const -> DualMonoid<M> {
        // Flip the argument order — that's the entire mechanism.
        return DualMonoid<M>{monoid_v<M>.combine(rhs.value, lhs.value)};
    }
};

} // namespace smd::typeclass

#endif // INCLUDED_SMD_TYPECLASS_DUAL_MONOID
