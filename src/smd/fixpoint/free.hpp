// src/smd/fixpoint/free.hpp                                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_FREE
#define INCLUDED_SMD_FIXPOINT_FREE

// Free monad (design §5.4): either a Pure value, or one functor layer of
// further Free computations (a "Roll"). Free<F, Seed> is futu's unfold
// carrier (§7.5, futu.hpp): a single coalgebra step can emit a whole
// *chunk* of F-layers at once (built with nested roll_free), with fresh
// seeds sitting at the Pure leaves for futu's worker to keep unfolding.
//
// Complete-type reasoning mirrors Fix<F>/Cofree<F,A> (design §4): F's
// recursive positions are Box<Free<F,A>>, which is what makes
// `F<Free<F,A>>` a complete member type even though Free<F,A> is still
// being defined.

#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <smd/typeclass/detail/typeclass_base.hpp>
#include <smd/typeclass/functor.hpp>
#include <smd/typeclass/monad.hpp>

#include <functional>
#include <type_traits>
#include <utility>
#include <variant>

namespace smd::fixpoint {

/** Free<F, A>: a Pure value of type A, or one F-layer of further Free
 * computations (a Roll).
 * @tparam F unary template functor (the base functor being sequenced)
 * @tparam A the seed/value type at Pure leaves
 */
template <template <class> class F, class A>
struct Free {
    std::variant<A, F<Free<F, A>>> node; // Pure a | Roll layer

    friend constexpr auto operator==(const Free &, const Free &) -> bool =
        default;
};

/** pure_free(a) -> Free<F, A>: a Pure leaf holding @p a. */
template <template <class> class F, class A>
constexpr auto pure_free(A a) -> Free<F, A> {
    return Free<F, A>{std::variant<A, F<Free<F, A>>>{std::move(a)}};
}

/** roll_free(layer) -> Free<F, A>: one F-layer of further Free
 * computations.
 */
template <template <class> class F, class A>
constexpr auto roll_free(F<Free<F, A>> layer) -> Free<F, A> {
    return Free<F, A>{std::variant<A, F<Free<F, A>>>{std::move(layer)}};
}

/** is_pure(f) -> bool: true iff @p f is a Pure leaf (not a Roll layer). */
template <template <class> class F, class A>
constexpr auto is_pure(const Free<F, A> &f) -> bool {
    return std::holds_alternative<A>(f.node);
}

} // namespace smd::fixpoint

namespace smd::typeclass {

// -- Functor --

template <template <class> class F, class A>
struct FreeFunctorImpl {
    // Explicit (not deduced) trailing return type: fmap recurses on itself
    // through the nested lambda below (same shape as CofreeFunctorImpl,
    // cofree.hpp/S07) — a deduced `auto` return type cannot be used before
    // its own deduction completes, even one level down inside a lambda.
    // Fixed to the class-level `A` (Functor's only derived op, `replace`,
    // never re-wraps into a doubled structure — same reasoning as
    // CofreeFunctorImpl staying fixed to its own `A`).
    template <class Fn>
    constexpr auto fmap(this auto &&self, Fn &&fn,
                         const smd::fixpoint::Free<F, A> &fr)
        -> smd::fixpoint::Free<
            F, remove_cvref_t<std::invoke_result_t<Fn, const A &>>> {
        using B = remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return std::visit(
            smd::fixpoint::overloaded{
                [&](const A &a) -> smd::fixpoint::Free<F, B> {
                    return smd::fixpoint::pure_free<F>(std::invoke(fn, a));
                },
                [&](const F<smd::fixpoint::Free<F, A>> &layer)
                    -> smd::fixpoint::Free<F, B> {
                    auto mapped = smd::fixpoint::layer_fmap(
                        [&self, &fn](const smd::fixpoint::Free<F, A> &child)
                            -> smd::fixpoint::Free<F, B> {
                            return self.fmap(fn, child);
                        },
                        layer);
                    return smd::fixpoint::roll_free<F>(std::move(mapped));
                },
            },
            fr.node);
    }
};

template <template <class> class F, class A>
struct FreeFunctorMap : Functor<FreeFunctorImpl<F, A>> {
    using FreeFunctorImpl<F, A>::fmap;
};

/** Functor instance for Free<F, A>: maps the Pure value; recurses through
 * Roll layers.
 */
template <template <class> class F, class A>
inline constexpr auto functor_typeclass<smd::fixpoint::Free<F, A>> =
    FreeFunctorMap<F, A>{};

// -- Monad --

// pure/bind are generic over their own element-type parameter `X`, not
// fixed to the instance's own `A` (S03/S07 handoffs' discovery, design
// §6.3): Monad's derived operations (join/kleisli, monad.hpp) need `bind`
// to consume a doubled structure `Free<F, Free<F, X>>` that is not
// `Free<F, A>` for the class-level `A`. Only the outer functor `F` stays
// fixed at the class level; `A` here serves solely to key the
// `monad_typeclass<Free<F, A>>` specialization below.
template <template <class> class F, class A>
struct FreeMonadImpl {
    template <class X>
    constexpr auto pure(this auto &&, X &&x)
        -> smd::fixpoint::Free<F, remove_cvref_t<X>> {
        return smd::fixpoint::pure_free<F>(std::forward<X>(x));
    }

    // Explicit trailing return type for the same reason as
    // FreeFunctorImpl::fmap above: bind recurses on itself through the
    // nested lambda, and — per the S07 handoff's forward note anticipating
    // exactly this — the return type is computed from Fn/X alone (never
    // from bind's own deduced return type) to break the chicken-and-egg
    // cycle GCC 16 otherwise diagnoses ("use of ... before deduction of
    // 'auto'").
    template <class X, class Fn>
    constexpr auto bind(this auto &&self, const smd::fixpoint::Free<F, X> &m,
                         Fn &&fn)
        -> remove_cvref_t<std::invoke_result_t<Fn, const X &>> {
        using ResultFree = remove_cvref_t<std::invoke_result_t<Fn, const X &>>;
        return std::visit(
            smd::fixpoint::overloaded{
                [&](const X &a) -> ResultFree { return std::invoke(fn, a); },
                [&](const F<smd::fixpoint::Free<F, X>> &layer) -> ResultFree {
                    auto mapped = smd::fixpoint::layer_fmap(
                        [&self, &fn](const smd::fixpoint::Free<F, X> &child)
                            -> ResultFree { return self.bind(child, fn); },
                        layer);
                    return smd::fixpoint::roll_free<F>(std::move(mapped));
                },
            },
            m.node);
    }
};

template <template <class> class F, class A>
struct FreeMonadMap : Monad<FreeMonadImpl<F, A>> {
    using FreeMonadImpl<F, A>::bind;
    using FreeMonadImpl<F, A>::pure;
};

/** Monad instance for Free<F, A>: pure = pure_free; bind sequences through
 * Pure leaves, recursing through Roll layers.
 */
template <template <class> class F, class A>
inline constexpr auto monad_typeclass<smd::fixpoint::Free<F, A>> =
    FreeMonadMap<F, A>{};

} // namespace smd::typeclass

#endif
