// src/smd/fixpoint/freer_cosignature.hpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_FREER_COSIGNATURE
#define INCLUDED_SMD_FIXPOINT_FREER_COSIGNATURE

// The Free/Cofree pairing (design doc FD8): a scripted mock as a Cofree
// value over the CO-signature -- the dual of the request functor
// signature<Ops...> generates (freer.hpp) -- paired against a Freer program
// by a second, synchronous interpreter alongside run/run_trace
// (freer_run.hpp). Where signature<Ops...>::type<X> is a VARIANT of
// impure_node<Op,X> (exactly one pending request), cosignature<Sig>::type<X>
// is a PRODUCT of one responder per operation (an answer ready for every
// possible request) -- function-space shaped, per FD8's strict-Cofree
// correction: Cofree's tail is by-value (cofree.hpp:37), so the only finite
// value that represents "an answer to every possible next request" is a
// callable per operation, not an enumerated case.
//
// The co-signature's Functor instance is LAZY -- the exact dual of FD6's
// Coyoneda instance (freer.hpp): fmap post-composes onto each stored
// responder WITHOUT invoking it. This laziness is what makes unfold_cofree
// (unfold_cofree.hpp) terminate over the co-signature after building exactly
// one node, regardless of the coalgebra -- the coalgebra's recursive calls
// are only forced when a responder is actually invoked, at pairing time.
// Same registration route as FD11/D-A (a constrained functor_typeclass
// partial specialization, disjoint from freer.hpp's coyoneda_layer marker),
// same FD4 capture-ownership rule applied to its instance.

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/freer.hpp>
#include <smd/fixpoint/freer_row.hpp>
#include <smd/fixpoint/freer_run.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <smd/typeclass/detail/typeclass_base.hpp>
#include <smd/typeclass/functor.hpp>

#include <functional>
#include <type_traits>
#include <utility>
#include <variant>

namespace smd::fixpoint {

/** responder<Op, X>: one entry of a co-signature product (FD8) -- a stored
 * callable answering Op with (Op::response, X), where X is the co-signature
 * functor's own recursive position (the mock's next state).
 *
 * std::function, not one_shot (D-B's move-only continuation type): the mock
 * is conceptually re-askable (FD8's own text) -- Cofree's tail is a
 * by-value/shared structure, unlike Freer's move-only one-shot continuation
 * -- so the simpler, copyable, multi-shot std::function shape is both
 * sufficient and the more natural fit. Recorded choice, not a deviation:
 * FD8 leaves the responder representation open ("plain std::function-shaped
 * ... OR match D-B's one-shot type ... pick the simpler spelling").
 */
template <operation Op, class X>
struct responder {
    std::function<std::pair<typename Op::response, X>(Op)> respond;
};

namespace detail {

// ===========================================================================
// cosignature_layer<Pack>: the product-of-responders struct, built once from
// a flattened operation pack -- reused by both cosignature<signature<Ops...>>
// and cosignature<row<Sigs...>> below. Mirrors freer_row.hpp's own
// "flattened pack -> concrete type" step (ops_pack_variant_t there; the
// struct-of-responders here instead of a variant is the only thing that
// differs, per that file's own forward note to this step).
// ===========================================================================

template <class Pack>
struct cosignature_layer;

template <operation... Ops>
struct cosignature_layer<ops_pack<Ops...>> {
    /** The co-signature layer functor: one responder<Op,X> base per
     * operation in Pack. Marker typedefs (dual of freer.hpp's
     * coyoneda_signature/coyoneda_value_type, deliberately
     * co-signature-specific names so the two constrained
     * functor_typeclass partial specializations never overlap) key the
     * generic Functor instance below.
     */
    template <class X>
    struct type : responder<Ops, X>... {
        using co_signature_ops = ops_pack<Ops...>;
        using co_signature_value_type = X;
    };

    /** Lazy fmap over the whole product (FD8, the dual of FD6's Coyoneda
     * fmap, freer.hpp): build ONE NEW responder per operation, each
     * post-composing `fn` onto the OLD stored responder WITHOUT invoking
     * it -- the new responder's own callable, when eventually invoked
     * (by a pairing interpreter, never here), calls the old responder and
     * applies `fn` to the X it returns. Each operation's responder is an
     * independent base subobject of `layer`, so extracting/moving one
     * does not disturb the others -- no aliasing between the pack
     * expansion's N calls below.
     */
    template <class Fn, class X>
    static constexpr auto fmap_layer(Fn fn, type<X> &&layer)
        -> type<std::remove_cvref_t<std::invoke_result_t<Fn, X &&>>> {
        using Y = std::remove_cvref_t<std::invoke_result_t<Fn, X &&>>;
        return type<Y>{fmap_one<Ops>(
            fn, std::move(static_cast<responder<Ops, X> &>(layer)))...};
    }

  private:
    // Capture-ownership rule (FD4): `fn` is captured by value/copy (never
    // by reference) into the new responder's closure -- that closure is
    // STORED past this call's return (the entire point of the co-signature
    // being lazy), so a reference-captured `fn` would dangle the moment
    // fmap_layer returns, exactly the hazard FD4 documents for the
    // Coyoneda side. `fn` is duplicated once per operation (a genuinely
    // different closure owns each copy), unlike CoyonedaFunctorImpl::fmap's
    // single std::visit-selected alternative -- the co-signature is a
    // PRODUCT (every responder coexists), not a variant, so `fn` must be
    // copyable here.
    template <operation Op, class Fn, class X>
    static constexpr auto fmap_one(Fn fn, responder<Op, X> &&r)
        -> responder<Op, std::remove_cvref_t<std::invoke_result_t<Fn, X &&>>> {
        using Y = std::remove_cvref_t<std::invoke_result_t<Fn, X &&>>;
        return responder<Op, Y>{
            [old = std::move(r.respond),
             fn](Op op) mutable -> std::pair<typename Op::response, Y> {
                auto [response, next] = old(std::move(op));
                return {std::move(response), std::invoke(fn, std::move(next))};
            }};
    }
};

} // namespace detail

/** cosignature<Sig>: FD8's co-signature functor for a closed signature or
 * row Sig -- the dual of signature<Ops...>/row<Sigs...> (freer.hpp,
 * freer_row.hpp). `Sig` may be a bare `signature<Ops...>` or a
 * `row<Sigs...>`; the row case flattens over every member signature's
 * operations (mirroring row's own FLATTENED node_variant encoding, not a
 * product-of-products), reusing freer_row.hpp's detail::row_ops_pack_t
 * verbatim to recover the flattened Ops pack.
 *
 * Primary template intentionally left undefined: naming cosignature<T> for
 * a T that is neither shape fails here with an incomplete-type diagnostic
 * rather than silently doing the wrong thing (mirrors freer_row.hpp's
 * signature_ops_pack).
 */
template <class Sig>
struct cosignature;

template <operation... Ops>
struct cosignature<signature<Ops...>>
    : detail::cosignature_layer<detail::ops_pack<Ops...>> {};

template <class... Sigs>
struct cosignature<row<Sigs...>>
    : detail::cosignature_layer<detail::row_ops_pack_t<Sigs...>> {};

/** co_signature_layer<T>: T is (an instantiation of) some
 * cosignature<Sig>::type<X> -- the dual of freer.hpp's coyoneda_layer,
 * keying the generic lazy Functor instance below via the SAME registration
 * route (FD11/D-A: a constrained functor_typeclass partial specialization).
 * Deliberately co-signature-specific marker names, never overlapping
 * coyoneda_layer's (a Layer cannot satisfy both concepts at once).
 */
template <class T>
concept co_signature_layer = requires {
    typename T::co_signature_ops;
    typename T::co_signature_value_type;
};

} // namespace smd::fixpoint

namespace smd::typeclass {

// -- The generic co-signature Functor instance (FD8, dual of FD6) --
//
// One instance object serves every co-signature layer, exactly as
// CoyonedaFunctorMap (freer.hpp) serves every signature layer: `fmap`
// recovers the flattened operation pack from the layer's own marker
// typedef and delegates to detail::cosignature_layer<Pack>::fmap_layer,
// which never invokes a stored responder -- see that function's own
// comment for the laziness/capture-ownership argument. Consuming-only, no
// const-lvalue overload: the responders are stored by value inside the
// product, but fmap must still route a `Layer&&` through so the moved-from
// old responders can be relocated into the new closures without copying an
// entire product of std::function objects.
template <class Layer>
    requires smd::fixpoint::co_signature_layer<Layer>
struct CoSignatureFunctorImpl {
    template <class Fn>
    constexpr auto fmap(this auto &&, Fn &&fn, Layer &&layer)
        -> smd::fixpoint::detail::
            cosignature_layer<typename Layer::co_signature_ops>::template type<
                remove_cvref_t<std::invoke_result_t<
                    Fn, typename Layer::co_signature_value_type &&>>> {
        using Pack = typename Layer::co_signature_ops;
        return smd::fixpoint::detail::cosignature_layer<Pack>::fmap_layer(
            std::forward<Fn>(fn), std::move(layer));
    }
};

template <class Layer>
    requires smd::fixpoint::co_signature_layer<Layer>
struct CoSignatureFunctorMap : Functor<CoSignatureFunctorImpl<Layer>> {
    using CoSignatureFunctorImpl<Layer>::fmap;
};

/** Functor instance for every co-signature layer: user signatures never
 * write this fmap, same as the Coyoneda side (FD6/FD8). Functor only --
 * the co-signature never rides Free/Freer's Monad windfall (it isn't a
 * Free-layer functor; it is Cofree's tail functor), so no monad_typeclass
 * specialization is written here.
 */
template <class Layer>
    requires smd::fixpoint::co_signature_layer<Layer>
inline constexpr auto functor_typeclass<Layer> = CoSignatureFunctorMap<Layer>{};

} // namespace smd::typeclass

namespace smd::fixpoint {

/** Mock<Sig, B>: the pairing partner's type -- a Cofree tree over the
 * co-signature, head = mock state annotation (type B), tail = one
 * responder per operation. The alias S06's tests/S08's paper build against.
 */
template <class Sig, class B>
using Mock = Cofree<cosignature<Sig>::template type, B>;

/** pair_run(mock, prog) -> (A, B): the Free/Cofree pairing (FD8, Piponi
 * "Cofree Meets Free") -- a second, synchronous interpreter alongside
 * run/run_trace (freer_run.hpp), driven by a scripted Mock instead of a
 * handler function.
 *
 * A LOOP, not recursion (mirrors run's own shape/rationale exactly,
 * freer_run.hpp): each iteration handles one suspended operation, selects
 * the matching responder from the CURRENT mock state's tail (a direct
 * upcast to responder<Op, Mock<Sig,B>> -- one base subobject per
 * operation, so selection is O(1), no scanning), resumes the program's
 * continuation with the response, and REPLACES the mock state with the
 * responder's own next state -- so the C++ call stack never grows with the
 * number of effects a program performs, same as run.
 *
 * Sig, A, B must all be given explicitly at every call site (the
 * deduction-limitation discovery is universal across this whole layer --
 * S04/S05 handoffs): Sig cannot be deduced from a Freer<Sig,A> parameter,
 * and B cannot be deduced from a Mock<Sig,B> (=Cofree<...,B>) parameter for
 * the same alias-transparency reason.
 */
template <class Sig, class A, class B>
auto pair_run(Mock<Sig, B> &&mock, Freer<Sig, A> &&prog) -> std::pair<A, B> {
    using X = Freer<Sig, A>;
    using Layer = typename Sig::template type<X>;
    using M = Mock<Sig, B>;

    Freer<Sig, A> current = std::move(prog);
    M state = std::move(mock);
    while (!is_pure(current)) {
        Layer layer = std::get<Layer>(std::move(current.node));
        Box<X> resumed = std::visit(
            overloaded{
                [&state]<operation Op>(impure_node<Op, X> &&node) -> Box<X> {
                    auto &r = static_cast<responder<Op, M> &>(state.tail);
                    auto [response, next_state] = r.respond(std::move(node.op));
                    state = std::move(next_state);
                    return std::move(node.k)(std::move(response));
                }},
            std::move(layer.node));
        current = *std::move(resumed);
    }
    return {std::get<A>(std::move(current.node)), std::move(state.head)};
}

/** pair_run_trace(mock, prog) -> (A, B, trace): pair_run through the SAME
 * `trace` type run_trace uses (freer_run.hpp) -- FD8's "tests read
 * identically against either interpreter" claim made literal: an op needs
 * only its own operator<< (traceable_operation, freer_run.hpp) to be
 * assertable here, exactly as for run_trace. A short, self-contained loop
 * rather than a thin wrapper over pair_run (contrast run_trace, built by
 * wrapping run's handler argument via `tracing`): pair_run has no handler
 * argument to wrap -- the mock's responders play that role -- so recording
 * would otherwise have to rebuild an equivalent "tracing Cofree" adaptor
 * for no benefit over a second small loop.
 */
template <class Sig, class A, class B>
auto pair_run_trace(Mock<Sig, B> &&mock, Freer<Sig, A> &&prog)
    -> std::tuple<A, B, trace> {
    using X = Freer<Sig, A>;
    using Layer = typename Sig::template type<X>;
    using M = Mock<Sig, B>;

    trace log;
    Freer<Sig, A> current = std::move(prog);
    M state = std::move(mock);
    while (!is_pure(current)) {
        Layer layer = std::get<Layer>(std::move(current.node));
        Box<X> resumed = std::visit(
            overloaded{[&state, &log]<traceable_operation Op>(
                           impure_node<Op, X> &&node) -> Box<X> {
                log.push_back(render_operation(node.op));
                auto &r = static_cast<responder<Op, M> &>(state.tail);
                auto [response, next_state] = r.respond(std::move(node.op));
                state = std::move(next_state);
                return std::move(node.k)(std::move(response));
            }},
            std::move(layer.node));
        current = *std::move(resumed);
    }
    return {std::get<A>(std::move(current.node)), std::move(state.head),
            std::move(log)};
}

} // namespace smd::fixpoint

#endif
