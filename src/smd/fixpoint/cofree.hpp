// src/smd/fixpoint/cofree.hpp                                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_COFREE
#define INCLUDED_SMD_FIXPOINT_COFREE

// Cofree comonad (design §5.3): annotates every node of an F-shaped tree
// with a value of type A at the head, keeping one F-layer of already
// annotated children as the tail. Cofree<F, Result> is histo's fold carrier
// (§7.5, histo.hpp): each child position an algebra sees carries the
// *entire* annotated history of that subtree, not just its final result.
//
// Complete-type reasoning mirrors Fix<F> (design §4): F's recursive
// positions are Box<Cofree<F,A>> (e.g. Succ<Cofree<F,A>>{Box<Cofree<F,A>>
// pred}), which is what makes `F<Cofree<F,A>> tail` a complete member type
// even though Cofree<F,A> is still being defined.

#include <smd/fixpoint/fmap.hpp>

#include <smd/typeclass/comonad.hpp>
#include <smd/typeclass/detail/typeclass_base.hpp>
#include <smd/typeclass/functor.hpp>

#include <functional>
#include <type_traits>
#include <utility>

namespace smd::fixpoint {

// bfa61c28-7fe2-46f9-8ca6-1006b3348836
/** Cofree<F, A>: an F-tree where every node is annotated with an A.
 * @tparam F unary template functor (the base functor being annotated)
 * @tparam A the annotation/history type at each node
 */
template <template <class> class F, class A>
struct Cofree {
    A head;               // the annotation at this node
    F<Cofree<F, A>> tail; // one functor layer of annotated children

    // Hand-written (not = default): this is the exact type the clang-22
    // "incomplete type" cascade (dist_laws.t.cpp/gprepro.t.cpp) traces
    // through — forming F<WWR> for WWR = Cofree<F,X> (generalized.hpp's
    // gcata_worker_t/gprepro_worker_t) requires std::variant to ask
    // is_trivially_destructible_v<Succ<WWR>>, which (per Clang, more
    // eagerly than GCC) forces this defaulted friend's deleted-ness check,
    // which requires Cofree<F,X> itself complete — a cycle. A plain friend
    // body sidesteps this: it is only instantiated when actually called.
    friend constexpr auto operator==(const Cofree &lhs, const Cofree &rhs)
        -> bool {
        return lhs.head == rhs.head && lhs.tail == rhs.tail;
    }
};

/** extract(c) -> const A& : the annotation at this node. */
template <template <class> class F, class A>
constexpr auto extract(const Cofree<F, A> &c) -> const A & {
    return c.head;
}

/** unwrap_cofree(c) -> const F<Cofree<F,A>>& : one layer of annotated
 * children.
 */
template <template <class> class F, class A>
constexpr auto unwrap_cofree(const Cofree<F, A> &c) -> const F<Cofree<F, A>> & {
    return c.tail;
}
// bfa61c28-7fe2-46f9-8ca6-1006b3348836 end

} // namespace smd::fixpoint

namespace smd::typeclass {

// -- Functor --

template <template <class> class F, class A>
struct CofreeFunctorImpl {
    // Explicit (not deduced) trailing return type: fmap recurses on itself
    // through the nested lambda below, and a deduced `auto` return type
    // cannot be used before its own deduction completes, even when the
    // recursive call is one level down inside a lambda (GCC 16 diagnoses
    // this as "use of ... before deduction of 'auto'"). Naming the return
    // type up front breaks the cycle.
    template <class Fn>
    constexpr auto fmap(this auto &&self, Fn &&fn,
                        const smd::fixpoint::Cofree<F, A> &c)
        -> smd::fixpoint::Cofree<
            F, remove_cvref_t<std::invoke_result_t<Fn, const A &>>> {
        using B = remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        auto mapped_tail = smd::fixpoint::layer_fmap(
            [&self, &fn](const smd::fixpoint::Cofree<F, A> &child)
                -> smd::fixpoint::Cofree<F, B> { return self.fmap(fn, child); },
            c.tail);
        return smd::fixpoint::Cofree<F, B>{std::invoke(fn, c.head),
                                           std::move(mapped_tail)};
    }
};

template <template <class> class F, class A>
struct CofreeFunctorMap : Functor<CofreeFunctorImpl<F, A>> {
    using CofreeFunctorImpl<F, A>::fmap;
};

/** Functor instance for Cofree<F, A>: maps every head throughout the whole
 * annotated tree (not just the root), recursing through the tail via
 * layer_fmap.
 */
template <template <class> class F, class A>
inline constexpr auto functor_typeclass<smd::fixpoint::Cofree<F, A>> =
    CofreeFunctorMap<F, A>{};

// -- Comonad --

// extract/duplicate/fmap are generic over their own element-type parameter
// `X`, not fixed to the instance's own `A` (S03 handoff's discovery,
// design §6.3): the derived `extend = fmap . duplicate` needs `fmap` to
// consume the *doubled* structure `Cofree<F, Cofree<F, X>>` that
// `duplicate` produces, which is not `Cofree<F, A>` for the class-level
// `A`. Only the outer functor `F` stays fixed at the class level; `A` here
// serves solely to key the `comonad_typeclass<Cofree<F, A>>` specialization
// below.
template <template <class> class F, class A>
struct CofreeComonadImpl {
    template <class X>
    constexpr auto extract(this auto &&, const smd::fixpoint::Cofree<F, X> &c)
        -> const X & {
        return c.head;
    }

    template <class X>
    constexpr auto duplicate(this auto &&self,
                             const smd::fixpoint::Cofree<F, X> &c)
        -> smd::fixpoint::Cofree<F, smd::fixpoint::Cofree<F, X>> {
        auto mapped_tail = smd::fixpoint::layer_fmap(
            [&self](const smd::fixpoint::Cofree<F, X> &child) {
                return self.duplicate(child);
            },
            c.tail);
        return smd::fixpoint::Cofree<F, smd::fixpoint::Cofree<F, X>>{
            c, std::move(mapped_tail)};
    }

    // Explicit trailing return type for the same reason as
    // CofreeFunctorImpl::fmap above: it is directly self-recursive (through
    // the nested lambda), and a deduced `auto` return type cannot be used
    // before its own deduction completes.
    template <class Fn, class X>
    constexpr auto fmap(this auto &&self, Fn &&fn,
                        const smd::fixpoint::Cofree<F, X> &c)
        -> smd::fixpoint::Cofree<
            F, remove_cvref_t<std::invoke_result_t<Fn, const X &>>> {
        using B = remove_cvref_t<std::invoke_result_t<Fn, const X &>>;
        auto mapped_tail = smd::fixpoint::layer_fmap(
            [&self, &fn](const smd::fixpoint::Cofree<F, X> &child)
                -> smd::fixpoint::Cofree<F, B> { return self.fmap(fn, child); },
            c.tail);
        return smd::fixpoint::Cofree<F, B>{std::invoke(fn, c.head),
                                           std::move(mapped_tail)};
    }
};

template <template <class> class F, class A>
struct CofreeComonadMap : Comonad<CofreeComonadImpl<F, A>> {
    using CofreeComonadImpl<F, A>::duplicate;
    using CofreeComonadImpl<F, A>::extract;
    using CofreeComonadImpl<F, A>::fmap;
};

/** Comonad instance for Cofree<F, A>: extract reads the head, duplicate
 * re-annotates every node with the (sub)tree rooted there.
 */
template <template <class> class F, class A>
inline constexpr auto comonad_typeclass<smd::fixpoint::Cofree<F, A>> =
    CofreeComonadMap<F, A>{};

} // namespace smd::typeclass

#endif
