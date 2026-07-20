// src/examples/sender_dot.cpp                                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Render a composed sender pipeline as Graphviz DOT on stdout:
//
//   .build/<toolchain>/src/examples/<config>/sender_dot | dot -Tsvg > graph.svg
//
// The pipeline below is never connected or started -- reify_sender only
// reads the expression's structure (tag, data, children per node), so the
// picture is of the *program*, not of a run.

#include <smd/concrete/sender_dot.hpp>

#include <beman/execution/execution.hpp>

#include <iostream>

namespace ex = beman::execution;

int main() {
    auto pipeline =
        ex::when_all(ex::just(1),
                     ex::just(2) | ex::then([](int i) { return i * 2; })) |
        ex::then([](int a, int b) { return a + b; }) |
        ex::upon_error([](auto &&) { return 0; });

    std::cout << smd::concrete::sender_to_dot(pipeline);
}
