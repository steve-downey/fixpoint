// src/smd/typeclass/functor.hpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_TYPECLASS_FUNCTOR
#define INCLUDED_SMD_TYPECLASS_FUNCTOR

#include <smd/typeclass/detail/typeclass_base.hpp>

#include <algorithm>
#include <concepts>
#include <functional>
#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace smd::typeclass {

// Functor pattern invariants:
// - Instances are single lookup objects that provide fmap(F, T).
// - replace is a derived object operation implemented from fmap.
// - Dispatch happens through a provided object or functor_typeclass<Concrete>.
// - Keep lookup explicit through typeclass objects, not ADL overloads.

/** CRTP base for Functor instances.
 * `Impl` must provide `fmap(f, container)`; `replace` is derived from it.
 */
template <class Impl>
struct Functor : protected Impl {
    using Impl::fmap;

    /** Replaces every element of `value` with `replacement`, ignoring the
     * original element values.
     */
    template <class T, class U>
    auto replace(this auto &&self, T &&value, U &&replacement) {
        return self.fmap([replacement = std::forward<U>(replacement)](
                             const auto &) { return replacement; },
                         std::forward<T>(value));
    }
};

/** Typeclass lookup variable for Functor; specialize for each container type.
 */
template <class T>
inline constexpr auto functor_typeclass = std::false_type{};

template <class VALUE_TYPE>
struct OptionalFunctorImpl {
    template <class F>
    auto fmap(this auto &&, F &&function,
              const std::optional<VALUE_TYPE> &value) {
        using Result = std::invoke_result_t<F, const VALUE_TYPE &>;
        if (!value) {
            return std::optional<remove_cvref_t<Result>>{};
        }
        return std::optional<remove_cvref_t<Result>>{
            std::invoke(std::forward<F>(function), *value)};
    }
};

template <class VALUE_TYPE>
struct VectorFunctorImpl {
    template <class F>
    auto fmap(this auto &&, F &&function,
              const std::vector<VALUE_TYPE> &values) {
        using Result = std::invoke_result_t<F, const VALUE_TYPE &>;
        std::vector<remove_cvref_t<Result>> output;
        output.reserve(values.size());

        std::ranges::transform(values, std::back_inserter(output),
                               [&function](const VALUE_TYPE &v) {
                                   return std::invoke(function, v);
                               });

        return output;
    }
};

template <class VALUE_TYPE>
struct OptionalFunctorMap : Functor<OptionalFunctorImpl<VALUE_TYPE>> {
    using OptionalFunctorImpl<VALUE_TYPE>::fmap;
};

template <class VALUE_TYPE>
struct VectorFunctorMap : Functor<VectorFunctorImpl<VALUE_TYPE>> {
    using VectorFunctorImpl<VALUE_TYPE>::fmap;
};

/** Functor instance for `std::optional<VALUE_TYPE>`. */
template <class VALUE_TYPE>
inline constexpr auto functor_typeclass<std::optional<VALUE_TYPE>> =
    OptionalFunctorMap<VALUE_TYPE>{};

/** Functor instance for `std::vector<VALUE_TYPE>`. */
template <class VALUE_TYPE>
inline constexpr auto functor_typeclass<std::vector<VALUE_TYPE>> =
    VectorFunctorMap<VALUE_TYPE>{};

} // namespace smd::typeclass

#endif // INCLUDED_SMD_TYPECLASS_FUNCTOR
