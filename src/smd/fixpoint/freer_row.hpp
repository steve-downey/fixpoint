// src/smd/fixpoint/freer_row.hpp                                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_FREER_ROW
#define INCLUDED_SMD_FIXPOINT_FREER_ROW

// Rows: variant-of-signatures composition, recovering openness after FD1
// closed each individual signature (design doc FD1's closing paragraph,
// section 6 forward reference, FD10's Clock+Network example).
//
// row<Sigs...> is a composed signature: its layer `type<X>` exposes the
// SAME nested-typedef surface signature<Ops...>::type<X> does
// (coyoneda_signature/coyoneda_value_type/node_variant/node), but
// node_variant is FLATTENED over every member signature's operations --
// one std::variant<impure_node<Op,X>...>, not a variant of per-signature
// variants. That is what makes the S03 generic Coyoneda Functor instance
// (functor_typeclass<coyoneda_layer>, freer.hpp) cover rows for free: no
// new fmap is written here, or ever needs to be (FD6/FD11).
//
// member<Op, SigOrRow> is the C++ image of the 2015 paper's `Member`
// typeclass -- concept-checked injection, not the paper's open-union
// Typeable/index machinery (FD1 explicitly rejects that). discharge<Sig>
// is the handler-as-adaptor: it walks a row program consuming it, handles
// the operations belonging to one member signature inline via a
// synchronous handler (the same shape run's handler is), and re-emits
// every other operation via send into the smaller row (Row minus Sig).

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/freer.hpp>
#include <smd/fixpoint/one_shot.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <smd/typeclass/detail/typeclass_base.hpp>
#include <smd/typeclass/monad.hpp>

#include <type_traits>
#include <utility>
#include <variant>

namespace smd::fixpoint {

template <class... Sigs>
struct row;

// ===========================================================================
// detail: alias-level pack recovery/concatenation. These operate on bare
// operation-type packs (a lightweight ops_pack<Ops...> tag), never on
// instantiated impure_node<Op,X> types -- the row type itself is computed
// without ever building a "variant of impure_node" per member signature
// first (the pitfall list's "avoid instantiating every impure_node just to
// compute the row type" -- alias-level pack concatenation only). The single
// std::variant<impure_node<AllOps...,X>> is built exactly once, at the very
// end (ops_pack_variant), for the full flattened pack.
// ===========================================================================

namespace detail {

template <class... Ops>
struct ops_pack {};

// Recover a signature<Ops...>'s Ops pack. Primary left undefined so a
// non-signature argument (e.g. accidentally nesting a row inside a row,
// which this step does not support) fails here with a "incomplete type"
// diagnostic rather than silently doing the wrong thing.
template <class Sig>
struct signature_ops_pack;

template <operation... Ops>
struct signature_ops_pack<signature<Ops...>> {
    using type = ops_pack<Ops...>;
};

template <class Sig>
using signature_ops_pack_t = typename signature_ops_pack<Sig>::type;

// Concatenate any number of ops_pack<...>s into one -- pure type-list
// manipulation, no impure_node in sight.
template <class... Packs>
struct concat_ops_pack;

template <>
struct concat_ops_pack<> {
    using type = ops_pack<>;
};

template <class... Ops>
struct concat_ops_pack<ops_pack<Ops...>> {
    using type = ops_pack<Ops...>;
};

template <class... Ops1, class... Ops2, class... Rest>
struct concat_ops_pack<ops_pack<Ops1...>, ops_pack<Ops2...>, Rest...>
    : concat_ops_pack<ops_pack<Ops1..., Ops2...>, Rest...> {};

template <class... Packs>
using concat_ops_pack_t = typename concat_ops_pack<Packs...>::type;

/** row_ops_pack_t<Sigs...>: the row's flattened operation pack, recovered
 * from each member signature's own Ops pack and concatenated -- the whole
 * point of this being alias-level: no impure_node is instantiated to reach
 * this type.
 */
template <class... Sigs>
using row_ops_pack_t = concat_ops_pack_t<signature_ops_pack_t<Sigs>...>;

// -- Duplicate-operation detection (readable diagnostic, not variant vomit) --

template <class... Ts>
struct all_distinct : std::true_type {};

template <class T, class... Rest>
struct all_distinct<T, Rest...>
    : std::bool_constant<(!std::is_same_v<T, Rest> && ...) &&
                         all_distinct<Rest...>::value> {};

template <class Pack>
struct pack_all_distinct;

template <class... Ops>
struct pack_all_distinct<ops_pack<Ops...>> : all_distinct<Ops...> {};

template <class Pack>
inline constexpr bool pack_all_distinct_v = pack_all_distinct<Pack>::value;

/** ops_pack_variant_t<Pack, X>: the ONE place a flattened operation pack
 * turns into a std::variant<impure_node<Op,X>...> -- built exactly once,
 * over the fully-concatenated pack.
 */
template <class Pack, class X>
struct ops_pack_variant;

template <class... Ops, class X>
struct ops_pack_variant<ops_pack<Ops...>, X> {
    using type = std::variant<impure_node<Ops, X>...>;
};

template <class Pack, class X>
using ops_pack_variant_t = typename ops_pack_variant<Pack, X>::type;

} // namespace detail

/** row<Sigs...>: a composed signature over several independent member
 * signatures. Its layer `type<X>` exposes exactly the nested-typedef
 * surface signature<Ops...>::type<X> does (coyoneda_signature,
 * coyoneda_value_type, node_variant, and a `node` data member of that
 * variant type) -- so the S03 generic Coyoneda Functor instance covers
 * rows with zero new fmap, and `run`/`run_trace`/`send` (freer_run.hpp,
 * freer.hpp) all work over a row unmodified (see the S05 handoff for
 * confirmation this actually holds).
 *
 * `coyoneda_signature` names `row<Sigs...>` itself (not any one member
 * signature) -- the mapped-result layer S03's generic fmap builds is
 * `row<Sigs...>::template type<Y>`, per that instance's own contract; this
 * is why `row` needs its own `template<class X> struct type`, exactly like
 * `signature` (FD6/FD11's whole point, verified to fall out here).
 */
template <class... Sigs>
struct row {
    static_assert(
        detail::pack_all_distinct_v<detail::row_ops_pack_t<Sigs...>>,
        "row<Sigs...>: two (or more) of the row's member signatures share "
        "an operation type -- every operation type must be unique across "
        "all member signatures of a row (a shared operation type would "
        "collide as a duplicate std::variant alternative and make "
        "injection ambiguous)");

    /** The row's own flattened operation pack. Doubles as the marker the
     * `row_type` concept keys on (send<Row>'s overload, below) and as what
     * `member_ops_pack` (the `member` concept) reads for a row argument.
     */
    using row_member_signatures = detail::row_ops_pack_t<Sigs...>;

    template <class X>
    struct type {
        using coyoneda_signature = row<Sigs...>;
        using coyoneda_value_type = X;
        using node_variant =
            detail::ops_pack_variant_t<detail::row_ops_pack_t<Sigs...>, X>;

        node_variant node;
    };
};

/** row_type<T>: T is (an instantiation of) `row<Sigs...>`. Keys the
 * send<Row> overload below -- deliberately NOT the same marker `member`
 * checks membership against, so that overload stays viable (and wins
 * partial ordering against freer.hpp's unconstrained send<Sig,Op>) even
 * when Op turns out not to be a member; see that overload's comment.
 */
template <class T>
concept row_type = requires { typename T::row_member_signatures; };

namespace detail {

// member_ops_pack<SigOrRow>: recover the flattened operation pack from
// either a bare signature<Ops...> or a row<Sigs...> -- the `member`
// concept's uniform lookup over "a signature or a row".
template <class SigOrRow>
struct member_ops_pack;

template <operation... Ops>
struct member_ops_pack<signature<Ops...>> {
    using type = ops_pack<Ops...>;
};

template <class... Sigs>
struct member_ops_pack<row<Sigs...>> {
    using type = row_ops_pack_t<Sigs...>;
};

template <class SigOrRow>
using member_ops_pack_t = typename member_ops_pack<SigOrRow>::type;

template <class Op, class Pack>
struct op_in_pack;

template <class Op, class... Ops>
struct op_in_pack<Op, ops_pack<Ops...>>
    : std::bool_constant<(std::is_same_v<Op, Ops> || ...)> {};

template <class Op, class Pack>
inline constexpr bool op_in_pack_v = op_in_pack<Op, Pack>::value;

} // namespace detail

/** member<Op, SigOrRow>: Op is an operation of the signature or row
 * `SigOrRow` -- the C++ image of the 2015 paper's `Member` typeclass
 * (concept-checked injection, not open-union index machinery, FD1).
 * `SigOrRow` may be a bare `signature<Ops...>` or a `row<Sigs...>`.
 */
template <class Op, class SigOrRow>
concept member = operation<Op> &&
                 detail::op_in_pack_v<Op, detail::member_ops_pack_t<SigOrRow>>;

/** send<Row>(op): send<Sig,Op> (freer.hpp) extended to any row containing
 * Op as an operation of one of its member signatures.
 *
 * Deliberately constrained on `row_type<Row>` alone, NOT on
 * `member<Op,Row>`: freer.hpp's send<Sig,Op> has no constraint tying Op to
 * Sig at all (it is viable for any Sig/Op pair, valid or not), so if THIS
 * overload were instead constrained on `member<Op,Row>`, an invalid Op
 * would SFINAE this overload away and leave freer.hpp's unconstrained one
 * as the only candidate -- which would then instantiate and fail deep
 * inside node_variant's converting constructor, exactly the "variant
 * vomit" the design doc warns against. Constraining on `row_type<Row>`
 * instead keeps this overload viable (and, by constraint subsumption in
 * C++20 partial ordering, strictly more specialized than freer.hpp's
 * unconstrained template whenever Row actually is a row) regardless of
 * whether Op is a member -- so THIS overload, not freer.hpp's, is always
 * the one selected for a row argument, and the readable diagnostic below
 * is what actually fires for a non-member Op. Verified empirically (not
 * just reasoned through) before landing; see the S05 handoff for the
 * verbatim diagnostic and confirmation this overload -- not freer.hpp's --
 * is the one selected for valid calls too.
 */
template <row_type Row, operation Op>
auto send(Op op) -> Freer<Row, typename Op::response> {
    static_assert(member<Op, Row>,
                  "send<Row>(op): Op is not an operation of any member "
                  "signature of Row");
    using R = typename Op::response;
    using X = Freer<Row, R>;
    return roll_free<Row::template type>(
        typename Row::template type<X>{impure_node<Op, X>{
            std::move(op), one_shot<Box<X>(R)>([](R r) -> Box<X> {
                return make_box<X>(pure_free<Row::template type>(std::move(r)));
            })}});
}

namespace detail {

// -- row_without_t<Sig, Row>: Row minus the one member signature Sig,
// preserving the order of the rest. Sig must be literally (is_same_v) one
// of Row's own Sigs... -- discharge<Sig> discharges one whole member
// signature, not an arbitrary subset of operations.

template <class Head, class Row>
struct row_prepend;

template <class Head, class... Rest>
struct row_prepend<Head, row<Rest...>> {
    using type = row<Head, Rest...>;
};

template <class Head, class Row>
using row_prepend_t = typename row_prepend<Head, Row>::type;

template <class Target, class... Sigs>
struct row_filter_out {
    using type = row<>;
};

template <class Target, class Head, class... Tail>
struct row_filter_out<Target, Head, Tail...> {
    using tail_type = typename row_filter_out<Target, Tail...>::type;
    using type = std::conditional_t<std::is_same_v<Target, Head>, tail_type,
                                    row_prepend_t<Head, tail_type>>;
};

template <class Target, class Row>
struct row_without;

template <class Target, class... Sigs>
struct row_without<Target, row<Sigs...>> {
    static_assert((std::is_same_v<Target, Sigs> || ...),
                  "discharge<Sig>: Sig is not one of Row's own member "
                  "signatures -- Sig must be exactly one of the "
                  "signature<...> types Row was instantiated with");
    using type = typename row_filter_out<Target, Sigs...>::type;
};

template <class Target, class Row>
using row_without_t = typename row_without<Target, Row>::type;

} // namespace detail

/** discharge<Sig>(handler, prog): reinterpret a Freer<Row,A> program,
 * handling every operation belonging to the member signature `Sig`
 * inline via `handler` (a complete, synchronous handler for `Sig` alone --
 * the same callable-set shape `run`'s handler is), and re-emitting every
 * other operation via `send` into the smaller row (Row minus Sig).
 *
 * Consuming and self-recursive through STORED continuations: every
 * closure built here that outlives this call -- the re-emitted
 * continuation wrapper in the non-matching branch -- owns its captures
 * (`k`, `handler`) BY MOVE, never by reference (FD4's capture-ownership
 * rule). Built directly on `std::visit(overloaded{...}, ...)` over the
 * row's node_variant (run's pattern, freer_run.hpp), not on
 * `tracing_handler`: discharge conditionally re-emits rather than always
 * delegating-and-recording (S04 handoff's forward note).
 *
 * Sig, Row, and A must all be given explicitly at every call site: Sig
 * does not appear in any function parameter at all, and Row/A are hidden
 * inside the `Freer<Row,A>` alias, which template argument deduction
 * cannot invert (S04's discovery -- confirmed to apply identically here).
 * Handler deduces normally from the function argument, like `run`.
 */
template <class Sig, class Row, class A, class Handler>
auto discharge(Handler &&handler, Freer<Row, A> &&prog)
    -> Freer<detail::row_without_t<Sig, Row>, A> {
    using SmallRow = detail::row_without_t<Sig, Row>;
    using X = Freer<Row, A>;
    using SmallX = Freer<SmallRow, A>;
    using Layer = typename Row::template type<X>;

    Freer<Row, A> current = std::move(prog);
    if (is_pure(current)) {
        return pure_free<SmallRow::template type>(
            std::get<A>(std::move(current.node)));
    }

    Layer layer = std::get<Layer>(std::move(current.node));
    return std::visit(
        overloaded{[handler = std::forward<Handler>(handler)]<operation Op>(
                       impure_node<Op, X> &&node) mutable -> SmallX {
            if constexpr (member<Op, Sig>) {
                // Sig's own op: handle inline, then keep discharging
                // whatever the resumed continuation yields next.
                typename Op::response response =
                    std::invoke(handler, std::move(node.op));
                X next = *std::move(node.k)(std::move(response));
                return discharge<Sig, Row, A>(std::move(handler),
                                              std::move(next));
            } else {
                static_assert(
                    member<Op, SmallRow>,
                    "discharge<Sig>: Op belongs to neither Sig nor the "
                    "remaining row -- every operation of Row must be a "
                    "member of exactly one of its signatures");
                // Not Sig's op: re-emit into the smaller row. Built
                // DIRECTLY -- the same literal aggregate-init shape
                // send<Row> uses (freer.hpp's send, this file's
                // send<Row>) -- rather than via
                // smd::typeclass::mbind(send<SmallRow>(op), ...):
                // mbind/FreeMonadImpl::bind's generic recursive
                // traversal (free.hpp) re-threads its continuation
                // through a nested, NON-mutable closure (`[self, fn =
                // std::forward<Fn>(fn)](auto &&child) { return
                // self.bind(child, fn); }`) that the *template*
                // instantiates for both the Pure and Roll cases of
                // whatever `child` might structurally be -- including
                // the Roll case's OWN re-capture of `fn`, which, once
                // `fn` has been forced const by that outer non-mutable
                // closure, requires COPY-constructing `fn`. A
                // continuation that owns a move-only `one_shot` (as
                // this one must, holding `node.k`) has no copy
                // constructor, so that generic path cannot be
                // instantiated with it -- confirmed empirically (see
                // the S05 handoff for the diagnostic) before landing
                // this direct construction instead. Building the node
                // by hand sidesteps `bind`'s multi-child-fanout
                // generality entirely, which a single suspended
                // Coyoneda node never needed in the first place (FD6:
                // layer_fmap for the impure layer is O(1) closure
                // post-composition, not a traversal).
                //
                // This closure is STORED inside the returned SmallRow
                // node's continuation -- it outlives this call's stack
                // frame entirely, so both captures are owned by move
                // (FD4), never by reference.
                using R = typename Op::response;
                return roll_free<SmallRow::template type>(
                    typename SmallRow::template type<SmallX>{
                        impure_node<Op, SmallX>{
                            std::move(node.op),
                            one_shot<Box<SmallX>(R)>([k = std::move(node.k),
                                                      handler = std::move(
                                                          handler)](R r) mutable
                                                         -> Box<SmallX> {
                                X next = *std::move(k)(std::move(r));
                                return make_box<SmallX>(discharge<Sig, Row, A>(
                                    std::move(handler), std::move(next)));
                            })}});
            }
        }},
        std::move(layer.node));
}

} // namespace smd::fixpoint

#endif
