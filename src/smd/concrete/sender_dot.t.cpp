// src/smd/concrete/sender_dot.t.cpp                                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/concrete/sender_dot.hpp>
#include <smd/concrete/sender_dot.hpp> // Re-inclusion check

#include <smd/fixpoint/fix.hpp>

#include <beman/execution/execution.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace ex = beman::execution;

using smd::concrete::reify_sender;
using smd::concrete::rose_node;
using smd::concrete::sender_node_info;
using smd::concrete::sender_to_dot;
using smd::concrete::SenderTree;
using smd::concrete::SenderTreeF;
using smd::concrete::to_dot;
using smd::fixpoint::unwrap_fix;

TEST_CASE("sender_dot - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("sender_dot - ReifyJustIsALeaf") {
    auto tree = reify_sender(ex::just(42));
    const auto &root = unwrap_fix(tree);
    CHECK(root.label.tag_name == "just");
    CHECK(root.children.empty());
}

TEST_CASE("sender_dot - ReifyThenHasOneChild") {
    auto tree = reify_sender(ex::then(ex::just(1), [](int i) { return i + 1; }));
    const auto &root = unwrap_fix(tree);
    CHECK(root.label.tag_name == "then");
    REQUIRE(root.children.size() == 1u);
    const auto &child = unwrap_fix(root.children[0]);
    CHECK(child.label.tag_name == "just");
    CHECK(child.children.empty());
    // No assertion on data_type at the root: the lambda's spelling is
    // compiler-specific.
}

TEST_CASE("sender_dot - ReifyWhenAllPipeline") {
    auto pipeline = ex::when_all(ex::just(1), ex::just(2)) |
                    ex::then([](int a, int b) { return a + b; });
    auto tree = reify_sender(pipeline);

    const auto &root = unwrap_fix(tree);
    CHECK(root.label.tag_name == "then");
    REQUIRE(root.children.size() == 1u);

    const auto &when_all_node = unwrap_fix(root.children[0]);
    CHECK(when_all_node.label.tag_name == "when_all");
    CHECK(when_all_node.label.data_type.empty()); // make_sender_empty cleared
    REQUIRE(when_all_node.children.size() == 2u);
    CHECK(unwrap_fix(when_all_node.children[0]).label.tag_name == "just");
    CHECK(unwrap_fix(when_all_node.children[1]).label.tag_name == "just");
}

TEST_CASE("sender_dot - DotOutputStructure") {
    auto pipeline = ex::when_all(ex::just(1), ex::just(2)) |
                    ex::then([](int a, int b) { return a + b; });
    std::string dot = sender_to_dot(pipeline);

    // Preorder ids: n0 = then, n1 = when_all, n2/n3 = the two justs.
    CHECK(dot.find("digraph G {") == 0u);
    CHECK(dot.find("n0 [label=\"then") != std::string::npos);
    CHECK(dot.find("n1 [label=\"when_all\"]") != std::string::npos);
    CHECK(dot.find("n2 [label=\"just") != std::string::npos);
    CHECK(dot.find("n3 [label=\"just") != std::string::npos);
    CHECK(dot.find("n0 -> n1;") != std::string::npos);
    CHECK(dot.find("n1 -> n2;") != std::string::npos);
    CHECK(dot.find("n1 -> n3;") != std::string::npos);
    CHECK(dot.rfind("}\n") == dot.size() - 2);
}

TEST_CASE("sender_dot - NonValueChannelsAreMarked") {
    // upon_error's tag is then_t<set_error_t>: the leaf name alone would
    // render it as a plain "then".
    auto tree = reify_sender(
        ex::upon_error(ex::just(1), [](auto &&) { return 0; }));
    CHECK(unwrap_fix(tree).label.tag_name == "then/error");

    auto stopped = reify_sender(ex::just_stopped());
    CHECK(unwrap_fix(stopped).label.tag_name == "just/stopped");
}

TEST_CASE("sender_dot - OpaqueTypeBecomesLabeledLeaf") {
    struct not_a_sender {};
    auto tree = reify_sender(not_a_sender{});
    const auto &root = unwrap_fix(tree);
    CHECK(root.label.tag_name == "not_a_sender");
    CHECK(root.label.data_type.empty());
    CHECK(root.children.empty());
}

TEST_CASE("sender_dot - LabelsAreEscaped") {
    SenderTree tree = rose_node<SenderTreeF>(
        sender_node_info{"a\"b\\c", ""});
    std::string dot = to_dot(tree);
    CHECK(dot.find("n0 [label=\"a\\\"b\\\\c\"];") != std::string::npos);
}

TEST_CASE("sender_dot - ReifiedTreeFoldsWithTheLibrary") {
    // The point of reification: ordinary schemes work on the sender tree.
    auto pipeline = ex::when_all(ex::just(1), ex::just(2)) |
                    ex::then([](int a, int b) { return a + b; });
    auto tree = reify_sender(pipeline);

    auto count = smd::fixpoint::fold_fix<int>(
        [](const SenderTreeF<int> &layer) {
            int nodes = 1;
            for (int child : layer.children) {
                nodes += child;
            }
            return nodes;
        },
        tree);
    CHECK(count == 4);
}
