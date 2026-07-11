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

// 11de0b77-f3c3-413b-9949-5bc28a2430c2
/** Free<F, A>: a Pure value of type A, or one F-layer of further Free
 * computations (a Roll).
 * @tparam F unary template functor (the base functor being sequenced)
 * @tparam A the seed/value type at Pure leaves
 */
template <template <class> class F, class A>
struct Free {
    std::variant<A, F<Free<F, A>>> node; // Pure a | Roll layer

    // Hand-written (not = default): see cofree.hpp's/functors.hpp's own
    // comment — Free<F,A> is self-referential through F<Free<F,A>>, exactly
    // the shape that triggers Clang's eager defaulted-comparison
    // completeness check inside a self-embedding class template.
    friend constexpr auto operator==(const Free &lhs, const Free &rhs) -> bool {
        return lhs.node == rhs.node;
    }
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
// 11de0b77-f3c3-413b-9949-5bc28a2430c2 end

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

    // FD4: consuming overload. Explicit trailing return type for the same
    // deduction-cycle reason as the const overload above. `fr.node` is
    // visited by moving out of it (`std::get<...>(std::move(fr.node))`
    // rather than `std::visit(overloaded{...}, ...)`): both alternatives of
    // `overloaded` are constructed eagerly before dispatch, so two lambdas
    // sharing one move-captured `fn` would race to move the same object;
    // branching on `std::holds_alternative` first and constructing only the
    // live alternative's closure sidesteps that. Capture-ownership rule
    // (FD4): the closure handed to `layer_fmap` owns `self` and `fn` by
    // value -- no `[&self, &fn]`, ever, on this path.
    //
    // The recursive closure's `child` parameter is `auto&&`, not a fixed
    // `Free<F, A>&&`: a lazy layer's Functor instance delivers a genuine
    // rvalue child (S03's Coyoneda), but a *pure-data* F (e.g. IntListF)
    // only ever owned the const-lvalue fmap above -- FD4's fallthrough
    // rule -- and that legacy traversal hands children out as lvalues
    // (`*c.tail`, box.hpp's `operator*() const -> A&`). Forwarding
    // `child` on to the recursive `self.fmap(...)` call, rather than
    // hard-coding `std::move`, lets ordinary overload resolution pick
    // whichever of the two `fmap` overloads matches what the layer
    // actually delivered. This is what keeps chained/prvalue-composed
    // calls over pure-data F (e.g. this file's pre-existing const-path
    // tests, reached via this overload whenever an intermediate Free
    // value is a prvalue) correct without touching that const path.
    template <class Fn>
    constexpr auto fmap(this auto &&self, Fn &&fn,
                        smd::fixpoint::Free<F, A> &&fr)
        -> smd::fixpoint::Free<F,
                               remove_cvref_t<std::invoke_result_t<Fn, A &&>>> {
        using B = remove_cvref_t<std::invoke_result_t<Fn, A &&>>;
        if (std::holds_alternative<A>(fr.node)) {
            return smd::fixpoint::pure_free<F>(std::invoke(
                std::forward<Fn>(fn), std::get<A>(std::move(fr.node))));
        }
        auto layer = std::get<F<smd::fixpoint::Free<F, A>>>(std::move(fr.node));
        auto mapped = smd::fixpoint::layer_fmap(
            [self, fn = std::forward<Fn>(fn)](
                auto &&child) -> smd::fixpoint::Free<F, B> {
                return self.fmap(fn, std::forward<decltype(child)>(child));
            },
            std::move(layer));
        return smd::fixpoint::roll_free<F>(std::move(mapped));
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

    // FD4: consuming overload -- the load-bearing one for S03's Coyoneda
    // layer, whose stored continuation is move-only. Same visitation and
    // capture-ownership reasoning as FreeFunctorImpl::fmap's consuming
    // overload above (branch on holds_alternative rather than
    // std::visit(overloaded{...}) to avoid two lambdas racing to move one
    // `fn`; the closure handed to `layer_fmap` owns `self`/`fn` by value,
    // never `[&self, &fn]`; `child` is `auto&&` and forwarded, not
    // hard-coded `&&`, so a pure-data F's lvalue-yielding legacy fmap
    // still routes to the matching const-path `bind` overload -- see the
    // comment on FreeFunctorImpl::fmap's consuming overload above for why).
    // `pure` needs no rvalue overload: it is already by-value.
    template <class X, class Fn>
    constexpr auto bind(this auto &&self, smd::fixpoint::Free<F, X> &&m,
                        Fn &&fn)
        -> remove_cvref_t<std::invoke_result_t<Fn, X &&>> {
        using ResultFree = remove_cvref_t<std::invoke_result_t<Fn, X &&>>;
        if (std::holds_alternative<X>(m.node)) {
            return std::invoke(std::forward<Fn>(fn),
                               std::get<X>(std::move(m.node)));
        }
        auto layer = std::get<F<smd::fixpoint::Free<F, X>>>(std::move(m.node));
        auto mapped = smd::fixpoint::layer_fmap(
            [self, fn = std::forward<Fn>(fn)](auto &&child) -> ResultFree {
                return self.bind(std::forward<decltype(child)>(child), fn);
            },
            std::move(layer));
        return smd::fixpoint::roll_free<F>(std::move(mapped));
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
