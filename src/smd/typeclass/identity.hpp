// src/smd/typeclass/identity.hpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_TYPECLASS_IDENTITY
#define INCLUDED_SMD_TYPECLASS_IDENTITY

#include <smd/typeclass/apply.hpp>
#include <smd/typeclass/comonad.hpp>
#include <smd/typeclass/detail/typeclass_base.hpp>
#include <smd/typeclass/functor.hpp>
#include <smd/typeclass/monad.hpp>

#include <functional>
#include <type_traits>
#include <utility>

namespace smd::typeclass {

/** The trivial context: wraps exactly one value, with no additional effect.
 * Functor/Applicative/Monad/Comonad all coincide with plain function
 * application; Identity exists so schemes and law tests have a context with
 * no effect to compare against ("the effect-free case"). Kept an aggregate
 * (no user-declared constructors) so `Identity{x}` CTAD works, matching
 * dist_cata's expected usage in later steps.
 */
template <class A>
struct Identity {
    using value_type = A;
    A value;

    friend constexpr bool operator==(const Identity &,
                                     const Identity &) = default;
};

// -- Functor --

template <class A>
struct IdentityFunctorImpl {
    template <class Fn>
    constexpr auto fmap(this auto &&, Fn &&fn, const Identity<A> &ident) {
        using B = remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return Identity<B>{std::invoke(std::forward<Fn>(fn), ident.value)};
    }
};

template <class A>
struct IdentityFunctorMap : Functor<IdentityFunctorImpl<A>> {
    using IdentityFunctorImpl<A>::fmap;
};

/** Functor instance for `Identity<A>`: maps the wrapped value. */
template <class A>
inline constexpr auto functor_typeclass<Identity<A>> = IdentityFunctorMap<A>{};

// -- Applicative --

template <class A>
struct IdentityApplicativeImpl {
    template <class VALUE>
    constexpr auto pure(this auto &&, VALUE &&value) {
        return Identity<remove_cvref_t<VALUE>>{std::forward<VALUE>(value)};
    }

    template <class FUNCTION_IN_CONTEXT, class ARGUMENT_IN_CONTEXT>
    constexpr auto apply(this auto &&, const FUNCTION_IN_CONTEXT &function,
                         const ARGUMENT_IN_CONTEXT &argument) {
        using Result = std::invoke_result_t<
            const typename remove_cvref_t<FUNCTION_IN_CONTEXT>::value_type &,
            const typename remove_cvref_t<ARGUMENT_IN_CONTEXT>::value_type &>;
        return Identity<remove_cvref_t<Result>>{
            std::invoke(function.value, argument.value)};
    }
};

template <class A>
struct IdentityApplicativeMap : Applicative<IdentityApplicativeImpl<A>> {
    using IdentityApplicativeImpl<A>::apply;
    using IdentityApplicativeImpl<A>::pure;
};

/** Applicative instance for `Identity<A>`: pure wraps, apply unwraps, invokes,
 * rewraps. */
template <class A>
inline constexpr auto applicative_typeclass<Identity<A>> =
    IdentityApplicativeMap<A>{};

// -- Monad --

template <class A>
struct IdentityMonadImpl {
    template <class VALUE>
    constexpr auto pure(this auto &&, VALUE &&value) {
        return Identity<remove_cvref_t<VALUE>>{std::forward<VALUE>(value)};
    }

    // Generic over the wrapped type (not fixed to the instance's own `A`),
    // matching OptionalMonadImpl's `bind` — needed so the Monad CRTP's
    // derived `join`/`apply`/`kleisli` can invoke `bind` on the doubled
    // structure `Identity<Identity<X>>` they produce.
    template <class X, class F>
    constexpr auto bind(this auto &&, const Identity<X> &ma, F &&f) {
        return std::invoke(std::forward<F>(f), ma.value);
    }
};

template <class A>
struct IdentityMonadMap : Monad<IdentityMonadImpl<A>> {
    using IdentityMonadImpl<A>::bind;
    using IdentityMonadImpl<A>::pure;
};

/** Monad instance for `Identity<A>`: bind is direct invocation on `.value`. */
template <class A>
inline constexpr auto monad_typeclass<Identity<A>> = IdentityMonadMap<A>{};

// -- Comonad --

// `extract`/`duplicate`/`fmap` are generic over the wrapped type (their own
// template parameter `X`), not fixed to the instance's own `A` — needed so
// the Comonad CRTP's derived `extend = fmap f . duplicate` can invoke `fmap`
// on the doubled structure `Identity<Identity<X>>` that `duplicate`
// produces. `A` only serves to key the `comonad_typeclass<Identity<A>>`
// lookup below (mirroring how OptionalMonadImpl's `VALUE_TYPE` only keys
// `monad_typeclass<optional<VALUE_TYPE>>` while `bind` is generic).
template <class A>
struct IdentityComonadImpl {
    template <class X>
    constexpr auto extract(this auto &&, const Identity<X> &wa) -> const X & {
        return wa.value;
    }

    template <class X>
    constexpr auto duplicate(this auto &&, const Identity<X> &wa)
        -> Identity<Identity<X>> {
        return Identity<Identity<X>>{wa};
    }

    template <class Fn, class X>
    constexpr auto fmap(this auto &&, Fn &&fn, const Identity<X> &wa) {
        using B = remove_cvref_t<std::invoke_result_t<Fn, const X &>>;
        return Identity<B>{std::invoke(std::forward<Fn>(fn), wa.value)};
    }
};

template <class A>
struct IdentityComonadMap : Comonad<IdentityComonadImpl<A>> {
    using IdentityComonadImpl<A>::duplicate;
    using IdentityComonadImpl<A>::extract;
    using IdentityComonadImpl<A>::fmap;
};

/** Comonad instance for `Identity<A>`: extract unwraps, duplicate re-wraps
 * the whole context (there is nothing else to remember). */
template <class A>
inline constexpr auto comonad_typeclass<Identity<A>> = IdentityComonadMap<A>{};

} // namespace smd::typeclass

#endif // INCLUDED_SMD_TYPECLASS_IDENTITY
