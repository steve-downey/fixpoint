// src/smd/typeclass/detail/typeclass_base.hpp                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_TYPECLASS_DETAIL_TYPECLASS_BASE
#define INCLUDED_SMD_TYPECLASS_DETAIL_TYPECLASS_BASE

#include <optional>
#include <type_traits>

namespace smd::typeclass {

// Design invariants for the typeclass object pattern:
// - Per-concept lookup objects (for example *_typeclass<T>) are the
//   customization lookup points for typeclass dispatch.
// - Generic algorithms call through looked-up typeclass objects.
// - New concepts should keep lookup static and explicit.
// - Avoid adding parallel ADL-only customization paths for the same concept.

template <class T>
using remove_cvref_t = std::remove_cvref_t<T>;

/** Trait that extracts the element type from an applicative container.
 * Primary template uses the nested `value_type` alias when present.
 */
template <class T, class = void>
struct applicative_value;

template <class T>
struct applicative_value<T,
                         std::void_t<typename remove_cvref_t<T>::value_type>> {
    using type = typename remove_cvref_t<T>::value_type;
};

template <class T>
struct applicative_value<std::optional<T>, void> {
    using type = T;
};

/** Convenience alias for `applicative_value<T>::type`. */
template <class T>
using applicative_value_t = typename applicative_value<remove_cvref_t<T>>::type;

} // namespace smd::typeclass

#endif // INCLUDED_SMD_TYPECLASS_DETAIL_TYPECLASS_BASE
