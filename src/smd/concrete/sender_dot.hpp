// src/smd/concrete/sender_dot.hpp                                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_CONCRETE_SENDER_DOT
#define INCLUDED_SMD_CONCRETE_SENDER_DOT

// Render a concrete sender expression as a Graphviz DOT graph, in two
// stages that split the compile-time work from the runtime work:
//
//   1. reify_sender: a *structural* anamorphism from the heterogeneous
//      sender expression (every node its own static type) into a
//      first-class Fix<RoseF<sender_node_info, ·>> value. This cannot be
//      the library unfold_fix -- an unfold's seed type is fixed through
//      the recursion, and here every child seed is a different sender
//      type -- so the coalgebra runs as template recursion instead, one
//      instantiation per node type. The projection of one node is the
//      standard's exposition-only sender decomposition (tag, data,
//      children...), reached through beman::execution's implementation of
//      it, get_sender_data. Nothing is copied, connected, or started;
//      children are observed by reference.
//
//   2. to_dot: an ordinary catamorphism (fold_fix) over the reified tree.
//      Node numbering must be globally unique, so the algebra's carrier is
//      State-shaped -- a function from the next free id to the emitted
//      text -- and the fold composes those functions; running the composed
//      program from 0 assigns preorder ids and emits nodes and edges in
//      one pass. The same trick as fold.hpp's LeftFoldProgram: when a fold
//      needs threading, fold to a *program* and feed it the initial state.
//
// After reification every algorithm the library has for Fix works on the
// sender tree: the Foldable/Traversable instances of RoseF give label
// collection and effectful relabeling for free, and any other scheme
// (para, histo, ...) applies unchanged.

#include <smd/concrete/functors.hpp>

#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>
#include <smd/fixpoint/type_name.hpp>

#include <beman/execution/detail/sender_decompose.hpp>

#include <functional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace smd::concrete {

// 7bd7f2e6-4f23-4d19-90a1-c9dcdcf27aac
/** What the DOT rendering keeps from one sender node. */
struct sender_node_info {
    std::string tag_name;  // "just", "then", "when_all", ...; non-value
                           // channels marked ("then/error" is upon_error);
                           // a non-decomposable sender gets its own type
                           // name
    std::string data_type; // short name of the node's data payload; empty
                           // when the payload carries no information

    friend auto operator==(const sender_node_info &, const sender_node_info &)
        -> bool = default;
};

/** The sender tree's base functor and its fixed point. */
template <typename A>
using SenderTreeF = RoseF<sender_node_info, A>;

using SenderTree = smd::fixpoint::Fix<SenderTreeF>;
// 7bd7f2e6-4f23-4d19-90a1-c9dcdcf27aac end

namespace detail {

/** Display name for a sender tag: its leaf identifier, with the
 * completion channel appended when the tag is pinned to a non-value
 * channel. Several adaptors share one tag template parameterized by
 * channel -- then_t<set_error_t> is upon_error -- and the leaf name alone
 * would render them all as "then". Everything is computed in constexpr
 * contexts; see short_type_name<T>() for why that is required, not style.
 */
template <typename Tag>
auto tag_display_name() -> std::string {
    constexpr std::string_view full = smd::fixpoint::type_name<Tag>();
    constexpr std::string_view leaf = smd::fixpoint::short_type_name<Tag>();
    constexpr bool on_error =
        full.find("set_error_t") != std::string_view::npos;
    constexpr bool on_stopped =
        full.find("set_stopped_t") != std::string_view::npos;
    std::string name(leaf);
    if constexpr (on_error) {
        name += "/error";
    } else if constexpr (on_stopped) {
        name += "/stopped";
    }
    return name;
}

} // namespace detail

// 97147085-770b-41a0-8f85-6c999f52f787
/** Reify a sender expression into a first-class SenderTree value.
 * Deliberately unconstrained: a non-decomposable type becomes a leaf
 * labeled with its own type name, which is also what keeps this header
 * away from the completion-signature machinery.
 */
template <typename Sender>
auto reify_sender(const Sender &sndr) -> SenderTree {
    auto &&decomposed = ::beman::execution::detail::get_sender_data(sndr);
    using Decomposed = std::remove_cvref_t<decltype(decomposed)>;

    if constexpr (requires { decomposed.tag; }) {
        sender_node_info info{
            detail::tag_display_name<typename Decomposed::tag_type>(),
            std::string(smd::fixpoint::short_type_name<
                        typename Decomposed::data_type>())};
        if (info.data_type == "make_sender_empty") {
            info.data_type.clear(); // when_all's "no payload" marker
        }
        std::vector<SenderTree> children;
        std::apply(
            [&](const auto &...child) {
                children.reserve(sizeof...(child));
                (children.push_back(reify_sender(child)), ...);
            },
            decomposed.children);
        return smd::fixpoint::wrap_fix<SenderTreeF>(
            SenderTreeF<SenderTree>{std::move(info), std::move(children)});
    } else {
        return smd::fixpoint::wrap_fix<SenderTreeF>(SenderTreeF<SenderTree>{
            sender_node_info{std::string(smd::fixpoint::short_type_name<
                                         std::remove_cvref_t<Sender>>()),
                             std::string{}},
            {}});
    }
}
// 97147085-770b-41a0-8f85-6c999f52f787 end

namespace detail {

/** One emitted subtree: the id its root received, the next free id after
 * the whole subtree, and the accumulated node and edge lines.
 */
struct dot_result {
    int node_id;
    int next_id;
    std::string text;
};

/** The fold's State-shaped carrier: next free id -> emitted subtree. */
struct dot_program {
    std::function<dot_result(int)> run;
};

/** Escape a label for a double-quoted DOT string. */
inline auto dot_escape(std::string_view text) -> std::string {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        default:
            out += c;
        }
    }
    return out;
}

} // namespace detail

// 56efdc8a-b32e-4780-8f98-29a9659590d6
/** Render a SenderTree as a Graphviz digraph. Ids are assigned in
 * preorder -- the root is n0, each subtree owns a contiguous id range --
 * by folding to a dot_program and running it from 0.
 */
inline auto to_dot(const SenderTree &tree) -> std::string {
    auto algebra = [](const SenderTreeF<detail::dot_program> &layer)
        -> detail::dot_program {
        return detail::dot_program{[label = layer.label, kids = layer.children](
                                       int next) -> detail::dot_result {
            const int my_id = next;
            int cursor = next + 1;
            std::string text = "  n" + std::to_string(my_id) + " [label=\"" +
                               detail::dot_escape(label.tag_name);
            if (!label.data_type.empty()) {
                text += "\\n" + detail::dot_escape(label.data_type);
            }
            text += "\"];\n";
            for (const auto &kid : kids) {
                detail::dot_result sub = kid.run(cursor);
                cursor = sub.next_id;
                text += sub.text;
                text += "  n" + std::to_string(my_id) + " -> n" +
                        std::to_string(sub.node_id) + ";\n";
            }
            return {my_id, cursor, std::move(text)};
        }};
    };
    detail::dot_program program =
        smd::fixpoint::fold_fix<detail::dot_program>(algebra, tree);
    return "digraph G {\n  node [shape=box];\n" + program.run(0).text + "}\n";
}
// 56efdc8a-b32e-4780-8f98-29a9659590d6 end

// e2c93e21-48e6-4dee-9549-939f59ea8cf7
/** Reify @p sndr and render it: the whole pipeline in one call. */
template <typename Sender>
auto sender_to_dot(const Sender &sndr) -> std::string {
    return to_dot(reify_sender(sndr));
}
// e2c93e21-48e6-4dee-9549-939f59ea8cf7 end

} // namespace smd::concrete

#endif // INCLUDED_SMD_CONCRETE_SENDER_DOT
