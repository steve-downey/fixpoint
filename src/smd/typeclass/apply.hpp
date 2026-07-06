// src/smd/typeclass/apply.hpp                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_TYPECLASS_APPLY
#define INCLUDED_SMD_TYPECLASS_APPLY

#include <smd/typeclass/detail/typeclass_base.hpp>
#include <smd/typeclass/functor.hpp>

#include <concepts>
#include <functional>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace smd::typeclass {

namespace detail {

template <class FUNCTION, class... BOUND_ARGS>
struct terminating_partial {
    FUNCTION function;
    std::tuple<BOUND_ARGS...> bound_args;

    template <class NEXT_ARG>
    auto operator()(NEXT_ARG &&next_arg) {
        return invoke_or_extend(std::forward<NEXT_ARG>(next_arg),
                                std::index_sequence_for<BOUND_ARGS...>{});
    }

    template <class NEXT_ARG>
    auto operator()(NEXT_ARG &&next_arg) const {
        return invoke_or_extend_const(std::forward<NEXT_ARG>(next_arg),
                                      std::index_sequence_for<BOUND_ARGS...>{});
    }

  private:
    template <class NEXT_ARG, std::size_t... IDX>
    auto invoke_or_extend(NEXT_ARG &&next_arg, std::index_sequence<IDX...>) {
        if constexpr (std::invocable<FUNCTION &, BOUND_ARGS &..., NEXT_ARG>) {
            return std::invoke(function, std::get<IDX>(bound_args)...,
                               std::forward<NEXT_ARG>(next_arg));
        } else {
            using NEXT_PARTIAL = terminating_partial<FUNCTION, BOUND_ARGS...,
                                                     remove_cvref_t<NEXT_ARG>>;
            return NEXT_PARTIAL{
                function,
                std::tuple_cat(std::move(bound_args),
                               std::tuple<remove_cvref_t<NEXT_ARG>>{
                                   std::forward<NEXT_ARG>(next_arg)})};
        }
    }

    template <class NEXT_ARG, std::size_t... IDX>
    auto invoke_or_extend_const(NEXT_ARG &&next_arg,
                                std::index_sequence<IDX...>) const {
        if constexpr (std::invocable<const FUNCTION &, const BOUND_ARGS &...,
                                     NEXT_ARG>) {
            return std::invoke(function, std::get<IDX>(bound_args)...,
                               std::forward<NEXT_ARG>(next_arg));
        } else {
            using NEXT_PARTIAL = terminating_partial<FUNCTION, BOUND_ARGS...,
                                                     remove_cvref_t<NEXT_ARG>>;
            return NEXT_PARTIAL{
                function,
                std::tuple_cat(bound_args,
                               std::tuple<remove_cvref_t<NEXT_ARG>>{
                                   std::forward<NEXT_ARG>(next_arg)})};
        }
    }
};

template <class FUNCTION>
auto make_terminating_partial(FUNCTION &&function) {
    using STORED_FUNCTION = remove_cvref_t<FUNCTION>;
    return terminating_partial<STORED_FUNCTION>{
        std::forward<FUNCTION>(function), std::tuple<>{}};
}

// apply_chain: left-folds `self.ap` over the remaining effectful arguments.
// A free function (not a class member) so it is unaffected by the
// class-hierarchy access-control subtlety noted below: with deducing-`this`,
// a call of the form `self.member(...)` is checked as an access through
// `self`'s *reified* type, not merely by where the call is lexically
// written. Once `self`'s type is reached by crossing a `protected`
// inheritance step two or more class-template layers below the class
// declaring `member`, a private *or* protected class member becomes
// inaccessible even though the call site is textually inside the
// declaring class -- exactly the shape of the `Applicative` structural
// chain here (Functor <- Applicative <- Monad, each `protected Impl`).
// Keeping this helper as a free function sidesteps the issue entirely: it
// only ever calls the already-public `self.ap`.
template <class SELF, class ACCUMULATED>
auto apply_chain(SELF &&, ACCUMULATED &&accumulated) {
    return std::forward<ACCUMULATED>(accumulated);
}

template <class SELF, class ACCUMULATED, class NEXT_ARGUMENT,
         class... REST_ARGUMENTS>
auto apply_chain(SELF &&self, ACCUMULATED &&accumulated,
                 NEXT_ARGUMENT &&next_argument,
                 REST_ARGUMENTS &&...rest_arguments) {
    auto next = self.ap(std::forward<ACCUMULATED>(accumulated),
                        std::forward<NEXT_ARGUMENT>(next_argument));
    if constexpr (sizeof...(REST_ARGUMENTS) == 0) {
        return next;
    } else {
        return apply_chain(std::forward<SELF>(self), std::move(next),
                           std::forward<REST_ARGUMENTS>(rest_arguments)...);
    }
}

} // namespace detail

// Applicative pattern invariants:
// - Minimal required operations are pure and apply.
// - invoke is derived from pure/apply via terminating partial application,
//   but an Impl may provide a custom invoke for different semantics or
//   efficiency.
// - Derived operations (lift/ap/zip_with/discard_*) live on that object.
// - Dispatch happens through a provided object or
//   applicative_typeclass<Concrete>.
// - Do not introduce hidden alternate semantics without a distinct map/type.

/** Adapter: supplies a single-argument `fmap` so an Applicative can also
 * serve as a Functor base, defined as `self.invoke(fn, x)` so it defers to
 * the most-derived object's `invoke` (respecting any Impl-provided override).
 */
template <class Impl>
struct applicative_functor_adapter : protected Impl {
    using Impl::apply;
    using Impl::pure;

    template <class FUNCTION, class ARGUMENT>
    auto fmap(this auto &&self, FUNCTION &&function, ARGUMENT &&argument) {
        return self.invoke(std::forward<FUNCTION>(function),
                           std::forward<ARGUMENT>(argument));
    }
};

/** CRTP base for Applicative instances.
 * `Impl` must provide `pure(value)` and `apply(f_in_context, arg_in_context)`.
 * All other operations are derived. Structurally chains onto `Functor` (via
 * `applicative_functor_adapter`) so the full Functor surface (`fmap`,
 * `replace`) is also available on an Applicative object.
 */
template <class Impl>
struct Applicative : protected Functor<applicative_functor_adapter<Impl>> {
    static_assert(
        !std::is_same_v<Impl, std::false_type>,
        "No applicative_typeclass<T> specialization found. "
        "Specialize smd::typeclass::applicative_typeclass<T> for "
        "your type T and provide pure(...) and apply(...) operations.");
    // Alternate-core: pure + apply are the primitives; invoke and all others
    // derive from them. fmap/replace come from the Functor base.
    using Functor<applicative_functor_adapter<Impl>>::fmap;
    using Functor<applicative_functor_adapter<Impl>>::replace;
    using Impl::apply;
    using Impl::pure;

    // Teaching-friendly alias for "apply pure function to effectful arguments".
    // Prefer invoke as the primary C++ spelling (std::invoke model).
    template <class FUNCTION, class FIRST_ARGUMENT, class... REST_ARGUMENTS>
    auto apply_pure(this auto &&self, FUNCTION &&function,
                    FIRST_ARGUMENT &&first_argument,
                    REST_ARGUMENTS &&...rest_arguments) {
        return self.invoke(std::forward<FUNCTION>(function),
                           std::forward<FIRST_ARGUMENT>(first_argument),
                           std::forward<REST_ARGUMENTS>(rest_arguments)...);
    }

    /** Lifts `function` into the applicative and applies it to one or more
     * effectful arguments left-to-right, producing a single effectful result.
     * @param function  A plain callable; it is wrapped with `pure` internally.
     * @param first_argument  First effectful argument (e.g., optional or
     * vector).
     * @param rest_arguments  Additional effectful arguments, if any.
     */
    template <class FUNCTION, class FIRST_ARGUMENT, class... REST_ARGUMENTS>
    auto invoke(this auto &&self, FUNCTION &&function,
                FIRST_ARGUMENT &&first_argument,
                REST_ARGUMENTS &&...rest_arguments) {
        using SELF = std::remove_reference_t<decltype(self)>;
        using IMPL_BASE =
            std::conditional_t<std::is_const_v<SELF>, const Impl, Impl>;

        if constexpr (requires(IMPL_BASE &impl) {
                          impl.invoke(
                              std::forward<FUNCTION>(function),
                              std::forward<FIRST_ARGUMENT>(first_argument),
                              std::forward<REST_ARGUMENTS>(rest_arguments)...);
                      }) {
            return static_cast<IMPL_BASE &>(self).invoke(
                std::forward<FUNCTION>(function),
                std::forward<FIRST_ARGUMENT>(first_argument),
                std::forward<REST_ARGUMENTS>(rest_arguments)...);
        } else {
            auto lifted_function = self.pure(detail::make_terminating_partial(
                std::forward<FUNCTION>(function)));
            return detail::apply_chain(
                self,
                self.ap(std::move(lifted_function),
                        std::forward<FIRST_ARGUMENT>(first_argument)),
                std::forward<REST_ARGUMENTS>(rest_arguments)...);
        }
    }

    /** Single-argument fmap via invoke; applies `function` to one effectful
     * arg. */
    template <class FUNCTION, class ARGUMENT>
    auto map(this auto &&self, FUNCTION &&function, ARGUMENT &&argument) {
        return self.invoke(std::forward<FUNCTION>(function),
                           std::forward<ARGUMENT>(argument));
    }

    /** Alias for `pure`; embeds a plain value into the applicative context. */
    template <class VALUE>
    auto lift(this auto &&self, VALUE &&value) {
        return self.pure(std::forward<VALUE>(value));
    }

    /** Alias for the primitive `apply`: applies an effectful function to an
     * effectful argument.
     */
    template <class FUNCTION_IN_CONTEXT, class ARGUMENT_IN_CONTEXT>
    auto ap(this auto &&self, FUNCTION_IN_CONTEXT &&function,
            ARGUMENT_IN_CONTEXT &&argument) {
        return self.apply(std::forward<FUNCTION_IN_CONTEXT>(function),
                          std::forward<ARGUMENT_IN_CONTEXT>(argument));
    }

    /** Lifts a binary function and applies it to two effectful arguments. */
    template <class FUNCTION, class FIRST_ARGUMENT, class SECOND_ARGUMENT>
    auto zip_with(this auto &&self, FUNCTION &&function,
                  FIRST_ARGUMENT &&first_argument,
                  SECOND_ARGUMENT &&second_argument) {
        return self.invoke(std::forward<FUNCTION>(function),
                           std::forward<FIRST_ARGUMENT>(first_argument),
                           std::forward<SECOND_ARGUMENT>(second_argument));
    }

    /** Sequences two effectful values; returns the second, ignoring the first.
     */
    template <class FIRST_ARGUMENT, class SECOND_ARGUMENT>
    auto discard_first(this auto &&self, FIRST_ARGUMENT &&first_argument,
                       SECOND_ARGUMENT &&second_argument) {
        return self.invoke(
            [](const auto &, auto &&rhs) {
                return std::forward<decltype(rhs)>(rhs);
            },
            std::forward<FIRST_ARGUMENT>(first_argument),
            std::forward<SECOND_ARGUMENT>(second_argument));
    }

    /** Sequences two effectful values; returns the first, ignoring the second.
     */
    template <class FIRST_ARGUMENT, class SECOND_ARGUMENT>
    auto discard_second(this auto &&self, FIRST_ARGUMENT &&first_argument,
                        SECOND_ARGUMENT &&second_argument) {
        return self.invoke(
            [](auto &&lhs, const auto &) {
                return std::forward<decltype(lhs)>(lhs);
            },
            std::forward<FIRST_ARGUMENT>(first_argument),
            std::forward<SECOND_ARGUMENT>(second_argument));
    }

    /** Delegates invoke to a different applicative instance at runtime. */
    template <class APPLICATIVE_MAP, class FUNCTION, class FIRST_ARGUMENT,
              class... REST_ARGUMENTS>
    auto invoke_with(this auto &&, const APPLICATIVE_MAP &applicative_map,
                     FUNCTION &&function, FIRST_ARGUMENT &&first_argument,
                     REST_ARGUMENTS &&...rest_arguments) {
        return applicative_map.invoke(
            std::forward<FUNCTION>(function),
            std::forward<FIRST_ARGUMENT>(first_argument),
            std::forward<REST_ARGUMENTS>(rest_arguments)...);
    }

    /** Delegates apply_pure to a different applicative instance at runtime. */
    template <class APPLICATIVE_MAP, class FUNCTION, class FIRST_ARGUMENT,
              class... REST_ARGUMENTS>
    auto apply_pure_with(this auto &&, const APPLICATIVE_MAP &applicative_map,
                         FUNCTION &&function, FIRST_ARGUMENT &&first_argument,
                         REST_ARGUMENTS &&...rest_arguments) {
        return applicative_map.invoke(
            std::forward<FUNCTION>(function),
            std::forward<FIRST_ARGUMENT>(first_argument),
            std::forward<REST_ARGUMENTS>(rest_arguments)...);
    }

    /** Delegates invoke to a compile-time constant applicative instance. */
    template <const auto &APPLICATIVE_MAP, class FUNCTION, class FIRST_ARGUMENT,
              class... REST_ARGUMENTS>
    auto invoke_with(this auto &&, FUNCTION &&function,
                     FIRST_ARGUMENT &&first_argument,
                     REST_ARGUMENTS &&...rest_arguments) {
        return APPLICATIVE_MAP.invoke(
            std::forward<FUNCTION>(function),
            std::forward<FIRST_ARGUMENT>(first_argument),
            std::forward<REST_ARGUMENTS>(rest_arguments)...);
    }

    /** Delegates apply_pure to a compile-time constant applicative instance. */
    template <const auto &APPLICATIVE_MAP, class FUNCTION, class FIRST_ARGUMENT,
              class... REST_ARGUMENTS>
    auto apply_pure_with(this auto &&, FUNCTION &&function,
                         FIRST_ARGUMENT &&first_argument,
                         REST_ARGUMENTS &&...rest_arguments) {
        return APPLICATIVE_MAP.invoke(
            std::forward<FUNCTION>(function),
            std::forward<FIRST_ARGUMENT>(first_argument),
            std::forward<REST_ARGUMENTS>(rest_arguments)...);
    }
};

/** Typeclass lookup variable for Applicative; specialize for each type. */
template <class T>
inline constexpr auto applicative_typeclass = std::false_type{};

template <class VALUE_TYPE>
struct OptionalApplicativeImpl {
    template <class VALUE>
    auto pure(this auto &&, VALUE &&value)
        -> std::optional<remove_cvref_t<VALUE>> {
        return std::optional<remove_cvref_t<VALUE>>{std::forward<VALUE>(value)};
    }

    template <class FUNCTION_IN_CONTEXT, class ARGUMENT_IN_CONTEXT>
    auto apply(this auto &&, FUNCTION_IN_CONTEXT &&function,
               ARGUMENT_IN_CONTEXT &&argument) {
        using Result =
            std::invoke_result_t<decltype(*function), decltype(*argument)>;

        if (!function || !argument) {
            return std::optional<remove_cvref_t<Result>>{};
        }

        return std::optional<remove_cvref_t<Result>>{
            std::invoke(*std::forward<FUNCTION_IN_CONTEXT>(function),
                        *std::forward<ARGUMENT_IN_CONTEXT>(argument))};
    }
};

template <class VALUE_TYPE>
struct OptionalApplicativeMap
    : Applicative<OptionalApplicativeImpl<VALUE_TYPE>> {
    using OptionalApplicativeImpl<VALUE_TYPE>::apply;
    using OptionalApplicativeImpl<VALUE_TYPE>::pure;
};

/** Applicative instance for `std::optional<VALUE_TYPE>`. */
template <class VALUE_TYPE>
inline constexpr auto applicative_typeclass<std::optional<VALUE_TYPE>> =
    OptionalApplicativeMap<VALUE_TYPE>{};

} // namespace smd::typeclass

#endif // INCLUDED_SMD_TYPECLASS_APPLY
