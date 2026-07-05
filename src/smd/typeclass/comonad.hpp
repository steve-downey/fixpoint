// src/smd/typeclass/comonad.hpp                                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_TYPECLASS_COMONAD
#define INCLUDED_SMD_TYPECLASS_COMONAD

#include <smd/typeclass/detail/typeclass_base.hpp>

#include <type_traits>
#include <utility>

namespace smd::typeclass {

// Comonad pattern invariants:
// - Instances are single lookup objects that provide extract(w), duplicate(w)
//   and fmap(f, w).
// - extend is a derived object operation implemented from fmap + duplicate.
// - Dispatch happens through a provided object or comonad_typeclass<Concrete>.
// - Comonad is the categorical dual of Monad: extract/duplicate mirror
//   pure/join, extend mirrors bind.

/** CRTP base for Comonad instances.
 * `Impl` must provide `extract(wa)`, `duplicate(wa)`, and `fmap(f, wa)`.
 * `extend` is derived: `extend f = fmap f . duplicate`.
 */
template <class Impl>
struct Comonad : protected Impl {
    static_assert(!std::is_same_v<Impl, std::false_type>,
                  "No comonad_typeclass<T> specialization found. "
                  "Specialize smd::typeclass::comonad_typeclass<T> for your "
                  "type T and provide extract(...), duplicate(...), and "
                  "fmap(...) operations.");

    using Impl::duplicate;
    using Impl::extract;
    using Impl::fmap;

    // extend: derived from fmap + duplicate.
    // extend f wa = fmap f (duplicate wa)
    template <class F, class WA>
    constexpr auto extend(this auto &&self, F &&f, WA &&wa) {
        return self.fmap(std::forward<F>(f),
                         self.duplicate(std::forward<WA>(wa)));
    }
};

/** Typeclass lookup variable for Comonad; specialize for each type. */
template <class T>
inline constexpr auto comonad_typeclass = std::false_type{};

} // namespace smd::typeclass

#endif // INCLUDED_SMD_TYPECLASS_COMONAD
