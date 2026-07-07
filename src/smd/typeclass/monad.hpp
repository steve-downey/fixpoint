// src/smd/typeclass/monad.hpp                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_TYPECLASS_MONAD
#define INCLUDED_SMD_TYPECLASS_MONAD

#include <smd/typeclass/apply.hpp>
#include <smd/typeclass/detail/typeclass_base.hpp>

#include <concepts>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

namespace smd::typeclass {

// See the typeclass contract in detail/typeclass_base.hpp.

/** Adapter: supplies `pure` (delegated to Impl) and synthesizes `apply` from
 * `bind` + `pure` so a Monad can also serve as an Applicative base.
 * ap mf mx = mf >>= \f -> mx >>= \a -> pure (f a)
 *
 * constexpr (design D10, S14): `apply` was not marked constexpr before S14
 * (gana.t.cpp's ana_via_gana constexpr smoke test is the first caller to
 * invoke `join` in a constant expression, via
 * `monad_typeclass<MSeed>.join(...)` -- generalized.hpp) -- a pre-existing
 * gap, not a new requirement; adding `constexpr` here is purely additive
 * (Impl::bind/pure, which this forwards to, were already constexpr in every
 * instance in this codebase).
 */
template <class Impl>
struct monad_applicative_adapter : protected Impl {
    using Impl::pure;

    template <class MF, class MA>
    constexpr auto apply(this auto &&self, MF &&mf, MA &&ma) {
        return self.bind(std::forward<MF>(mf), [&self, &ma](auto &&f) {
            return self.bind(ma, [&self, &f](auto &&a) {
                return self.pure(std::invoke(std::forward<decltype(f)>(f),
                                             std::forward<decltype(a)>(a)));
            });
        });
    }
};

/** CRTP base for Monad instances.
 * `Impl` must provide `pure(value)` and `bind(ma, f)`.
 * `apply` is synthesized; `join` and `kleisli` are derived.
 * Structurally chains onto `Applicative` (via `monad_applicative_adapter`),
 * which itself chains onto `Functor`, so the full Functor + Applicative
 * surface is also available on a Monad object.
 */
template <class Impl>
struct Monad : protected Applicative<monad_applicative_adapter<Impl>> {
    static_assert(!std::is_same_v<Impl, std::false_type>,
                  "No monad_typeclass<T> specialization found. "
                  "Specialize smd::typeclass::monad_typeclass<T> for your "
                  "type T and provide pure(...) and bind(...) operations.");

    // Re-expose the full Functor + Applicative surface (protected
    // inheritance hides it by default); Monad's own derived ops (join,
    // kleisli, bind_with) stay declared below as normal public members.
    using Applicative<monad_applicative_adapter<Impl>>::apply;
    using Applicative<monad_applicative_adapter<Impl>>::apply_pure;
    using Applicative<monad_applicative_adapter<Impl>>::apply_pure_with;
    using Applicative<monad_applicative_adapter<Impl>>::ap;
    using Applicative<monad_applicative_adapter<Impl>>::discard_first;
    using Applicative<monad_applicative_adapter<Impl>>::discard_second;
    using Applicative<monad_applicative_adapter<Impl>>::fmap;
    using Applicative<monad_applicative_adapter<Impl>>::invoke;
    using Applicative<monad_applicative_adapter<Impl>>::invoke_with;
    using Applicative<monad_applicative_adapter<Impl>>::lift;
    using Applicative<monad_applicative_adapter<Impl>>::map;
    using Applicative<monad_applicative_adapter<Impl>>::pure;
    using Applicative<monad_applicative_adapter<Impl>>::replace;
    using Applicative<monad_applicative_adapter<Impl>>::zip_with;
    using Impl::bind;

    // join: flatten nested monad.
    // join mma = mma >>= id
    template <class MMA>
    constexpr auto join(this auto &&self, MMA &&mma) {
        return self.bind(std::forward<MMA>(mma),
                         [](auto &&inner) { return inner; });
    }

    // kleisli: forward Kleisli composition (>=>).
    // (f >=> g) a = f a >>= g
    template <class F, class G>
    constexpr auto kleisli(this auto &&self, F f, G g) {
        return [&self, f = std::move(f), g = std::move(g)](auto &&a) {
            return self.bind(f(std::forward<decltype(a)>(a)), g);
        };
    }

    // bind_with: explicit monad object override.
    template <class MONAD_MAP, class MA, class F>
    constexpr auto bind_with(this auto &&, const MONAD_MAP &monad_map, MA &&ma,
                             F &&f) {
        return monad_map.bind(std::forward<MA>(ma), std::forward<F>(f));
    }
};

/** Typeclass lookup variable for Monad; specialize for each type. */
template <class T>
inline constexpr auto monad_typeclass = std::false_type{};

// -- std::optional monad instance --
// Delegates pure to the existing applicative_typeclass.

template <class VALUE_TYPE>
struct OptionalMonadImpl {
    using element_type = VALUE_TYPE;

    template <class VALUE>
    auto pure(this auto &&, VALUE &&value)
        -> std::optional<remove_cvref_t<VALUE>> {
        return applicative_typeclass<std::optional<VALUE_TYPE>>.pure(
            std::forward<VALUE>(value));
    }

    template <class A, class F>
    auto bind(this auto &&, const std::optional<A> &ma, F &&f) {
        using Result = remove_cvref_t<std::invoke_result_t<F, const A &>>;
        if (!ma)
            return Result{};
        return Result{std::invoke(std::forward<F>(f), *ma)};
    }
};

template <class VALUE_TYPE>
struct OptionalMonadMap : Monad<OptionalMonadImpl<VALUE_TYPE>> {
    using OptionalMonadImpl<VALUE_TYPE>::bind;
    using OptionalMonadImpl<VALUE_TYPE>::pure;
};

/** Monad instance for `std::optional<VALUE_TYPE>`. */
template <class VALUE_TYPE>
inline constexpr auto monad_typeclass<std::optional<VALUE_TYPE>> =
    OptionalMonadMap<VALUE_TYPE>{};

// -- Free-function API --

/** Sequences a monadic value `ma` through function `f` (Haskell's `>>=`). */
template <class MA, class F>
constexpr auto mbind(MA &&ma, F &&f) {
    const auto &map = monad_typeclass<remove_cvref_t<MA>>;
    return map.bind(std::forward<MA>(ma), std::forward<F>(f));
}

/** Flattens a nested monadic value; equivalent to `bind(mma, id)`. */
template <class MMA>
constexpr auto join(MMA &&mma) {
    const auto &map = monad_typeclass<remove_cvref_t<MMA>>;
    return map.join(std::forward<MMA>(mma));
}

} // namespace smd::typeclass

#endif // INCLUDED_SMD_TYPECLASS_MONAD
