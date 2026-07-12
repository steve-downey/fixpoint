// src/smd/fixpoint/freer.hpp                                         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_FREER
#define INCLUDED_SMD_FIXPOINT_FREER

// The closed-world Coyoneda signature layer (design doc FD1-FD3, FD6, FD11,
// FD12): Freer<Sig, A> is Free<F,A> (free.hpp) instantiated at the layer
// functor a closed effect signature generates, plus the generic Functor
// instance that makes `bind`/`fmap` on that layer O(1) closure
// post-composition instead of a per-signature hand-written traversal.
//
// No new fixpoint: Freer is an alias over the existing Free, and (FD3's
// windfall, confirmed at instantiation by this header's own tests) the
// existing functor_typeclass<Free<F,A>>/monad_typeclass<Free<F,A>>
// specializations already select for Freer<Sig,A> with zero new
// registration -- only the *layer*-level Functor instance
// (functor_typeclass<Sig::type<X>>) is new, registered per FD11's decision
// (D-A, option (a)): a constrained variable-template partial specialization
// keyed on a Coyoneda-specific marker (signature<Ops...>::type<X>'s nested
// `coyoneda_signature`/`coyoneda_value_type` typedefs), so the existing
// implicit-mode `layer_fmap` dispatch (fmap.hpp) resolves to it without any
// call-site change.

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/one_shot.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <smd/typeclass/detail/typeclass_base.hpp>
#include <smd/typeclass/functor.hpp>
#include <smd/typeclass/monad.hpp>

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>
#include <variant>

namespace smd::fixpoint {

/** unit: the response of operations that return no information.
 * A bespoke empty type rather than std::monostate: monostate carries
 * "empty variant alternative" connotations and comparison semantics we do
 * not want conflated with "trivial response".
 */
struct unit {
    friend constexpr auto operator==(const unit &, const unit &)
        -> bool = default;
};

/** An effect operation: a movable request payload that names the type a
 * handler must supply to resume the program. The `response` member type is
 * the entire protocol -- the per-constructor index of the Haskell GADT
 * FD1 dissolves into a closed variant, spelled the only way C++ can spell
 * it.
 */
template <class Op>
concept operation = std::movable<Op> && requires { typename Op::response; };

/** impure_node<Op, X>: one suspended request. Holds the operation payload
 * and the one-shot continuation from the handler's response to the rest of
 * the program.
 *
 * The continuation returns Box<X>, not X, for the same complete-type
 * reasoning as Fix/Free/Cofree: at the point this member is declared,
 * X = Free<..., A> is still incomplete, and Box is the completeness
 * firewall. `one_shot` is itself a second completeness firewall of the
 * same shape (one_shot.hpp) -- FD12's bespoke continuation representation,
 * uniform on every toolchain (D-B: libc++ at LLVM 23 does not declare
 * std::move_only_function at all).
 *
 * one_shot's rvalue-qualified call operator makes one-shot-ness a type
 * property: the continuation can only be invoked on an rvalue, i.e.
 * std::move(k)(r). See FD4.
 */
template <operation Op, class X>
struct impure_node {
    Op op;
    one_shot<Box<X>(typename Op::response)> k;
};

/** signature<Ops...>: a closed effect signature. Its nested member template
 * `type` is the layer functor handed to Free -- the closed-world Coyoneda
 * of the signature.
 *
 * A member template rather than an alias template: alias templates as
 * template-template arguments sit on P0522 matching, where GCC and Clang
 * still diverge in practice; the nested class template is the portable
 * spelling (FD3).
 *
 * `coyoneda_signature`/`coyoneda_value_type` are the nested typedefs FD11's
 * decision (D-A) requires: they let the generic Functor instance recover,
 * from the layer type alone, the owning signature and the recursive
 * position it needs to build the mapped layer's result type.
 * `coyoneda_signature` is deliberately signature-specific (not a generic
 * name like "recursive_position") so the constrained specialization it
 * keys never overlaps the Free<F,A> specialization or any pure-data
 * functor instance in the library.
 */
template <operation... Ops>
struct signature {
    template <class X>
    struct type {
        using coyoneda_signature = signature<Ops...>;
        using coyoneda_value_type = X;
        using node_variant = std::variant<impure_node<Ops, X>...>;

        node_variant node;
    };
};

/** Freer<Sig, A>: the freer monad over a closed signature -- the existing
 * Free applied to the signature's Coyoneda layer. No new fixpoint.
 */
template <class Sig, class A>
using Freer = Free<Sig::template type, A>;

/** Concept satisfied by a Coyoneda layer type produced by some
 * `signature<Ops...>::type<X>` -- keys the generic Functor instance's
 * constrained partial specialization (FD11, D-A). Deliberately keyed on
 * the Coyoneda-specific `coyoneda_signature`/`coyoneda_value_type`
 * typedefs rather than a generic name, per FD11's binding constraint.
 */
template <class T>
concept coyoneda_layer = requires {
    typename T::coyoneda_signature;
    typename T::coyoneda_value_type;
    typename T::node_variant;
};

/** send<Sig>(op): lift one operation into Freer<Sig, Op::response> -- the
 * unit of the adjunction (FD7). The continuation is the identity-into-Pure
 * -- Coyoneda's `id` -- so the program suspends with the request and
 * resumes with the handler's response as its value.
 *
 * The continuation is a `one_shot<Box<X>(R)>` (FD12/D-B), not a bare
 * lambda or `std::move_only_function`: `impure_node`'s `k` member is
 * declared at that type, and a plain lambda converts to it implicitly via
 * `one_shot`'s templated constructor.
 *
 * Variant construction: unlike S01's/S03's defensive precedent (which
 * never actually attempted this), FD7's literal aggregate-init spelling
 * over `Sig::template type<X>` -- relying on `node_variant`'s converting
 * constructor to pick the alternative matching the exact `impure_node<Op,
 * X>` prvalue -- compiles cleanly here on both gcc-16 and clang-23,
 * verified by a standalone probe before landing this; no `in_place_type`
 * needed for this particular construction (see the S04 handoff for the
 * probe and a hypothesis on why this call site differs from S03's
 * `CoyonedaFunctorImpl::fmap`, which does still need it).
 */
template <class Sig, operation Op>
auto send(Op op) -> Freer<Sig, typename Op::response> {
    using R = typename Op::response;
    using X = Freer<Sig, R>;
    return roll_free<Sig::template type>(
        typename Sig::template type<X>{impure_node<Op, X>{
            std::move(op), one_shot<Box<X>(R)>([](R r) -> Box<X> {
                return make_box<X>(pure_free<Sig::template type>(std::move(r)));
            })}});
}

} // namespace smd::fixpoint

namespace smd::typeclass {

// -- The generic Coyoneda Functor instance (FD6, granularity correction) --
//
// One instance object serves every closed-world signature: `fmap` takes
// the whole layer `Sig::type<X>&&`, std::visits the variant, and per
// alternative post-composes `fn` onto the stored one_shot continuation --
// O(1), no traversal of the rest of the program. Consuming-only: no
// const-lvalue overload exists, or ever should (FD4) -- the impure layer's
// continuation is move-only, so a const path could not compile; leaving it
// unwritten turns "no const path" into a clear error at the call site
// instead of template vomit.

template <class Layer>
    requires smd::fixpoint::coyoneda_layer<Layer>
struct CoyonedaFunctorImpl {
    template <class Fn>
    constexpr auto fmap(this auto &&, Fn &&fn, Layer &&layer) ->
        typename Layer::coyoneda_signature::template type<remove_cvref_t<
            std::invoke_result_t<Fn, typename Layer::coyoneda_value_type &&>>> {
        using X = typename Layer::coyoneda_value_type;
        using Y = remove_cvref_t<std::invoke_result_t<Fn, X &&>>;
        using Sig = typename Layer::coyoneda_signature;
        using Result = typename Sig::template type<Y>;
        using ResultVariant = typename Result::node_variant;

        // Capture-ownership rule (FD4): the closure handed to std::visit
        // owns `fn` by move -- never `[&fn]` -- because the *inner* per-node
        // continuation it builds is stored past this call's return. The
        // outer visitor lambda itself runs synchronously, exactly once
        // (std::visit dispatches to exactly one live alternative), so
        // moving `fn` into it here and moving it again into the inner
        // closure below is safe: there is only ever one live alternative
        // to move from, no double-move race (contrast free.hpp's own
        // consuming bind/fmap, which branch instead of using
        // std::visit(overloaded{...}) for a different reason -- there, one
        // *externally shared* fn is captured by two eagerly-constructed
        // visitor alternatives; here each alternative's inner continuation
        // captures its *own* per-node `k`, so the hazard those comments
        // describe does not apply to this visit).
        return std::visit(
            smd::fixpoint::overloaded{
                [fn = std::forward<Fn>(fn)]<smd::fixpoint::operation Op>(
                    smd::fixpoint::impure_node<Op, X> &&node) mutable
                    -> Result {
                    return Result{ResultVariant{
                        std::in_place_type<smd::fixpoint::impure_node<Op, Y>>,
                        smd::fixpoint::impure_node<Op, Y>{
                            std::move(node.op),
                            smd::fixpoint::one_shot<smd::fixpoint::Box<Y>(
                                typename Op::response)>(
                                [k = std::move(node.k), fn = std::move(fn)](
                                    typename Op::response r) mutable
                                    -> smd::fixpoint::Box<Y> {
                                    return smd::fixpoint::make_box<Y>(
                                        std::invoke(
                                            fn, *std::move(k)(std::move(r))));
                                })}}};
                }},
            std::move(layer.node));
    }
};

template <class Layer>
    requires smd::fixpoint::coyoneda_layer<Layer>
struct CoyonedaFunctorMap : Functor<CoyonedaFunctorImpl<Layer>> {
    using CoyonedaFunctorImpl<Layer>::fmap;
};

/** Functor instance for every closed-world Coyoneda layer: user signatures
 * never write fmap (FD6). Registered per FD11 (D-A, option (a)): a
 * constrained variable-template partial specialization of
 * `functor_typeclass`, so the existing implicit-mode `layer_fmap`
 * (fmap.hpp) that free.hpp's consuming Free fmap/bind already call finds
 * it with no call-site change. Functor only: the Monad is Free over the
 * layer (the FD3 windfall), so no layer-level `monad_typeclass`
 * specialization is written here.
 */
template <class Layer>
    requires smd::fixpoint::coyoneda_layer<Layer>
inline constexpr auto functor_typeclass<Layer> = CoyonedaFunctorMap<Layer>{};

} // namespace smd::typeclass

#endif
