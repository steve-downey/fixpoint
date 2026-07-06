// fixpoint_tree_example.cpp                                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Demonstrates the smd::fixpoint library: build a small arithmetic
// expression tree and evaluate it via a catamorphism (fold_fix). There is
// no hand-written tree-walking code — only the shape of the data (ExprF,
// from smd/fixpoint/functors.hpp) and the per-node evaluation logic
// (eval, also from functors.hpp).

#include <smd/fixpoint/functors.hpp>

#include <print>

using smd::fixpoint::add_node;
using smd::fixpoint::const_node;
using smd::fixpoint::eval;
using smd::fixpoint::Expr;
using smd::fixpoint::mul_node;

// 7a31aed8-07e7-4c69-afad-4f5fcb2e08f8
int main() {
    // Build the tree: (2 * 3) + 4
    Expr tree = add_node(mul_node(const_node(2), const_node(3)), const_node(4));

    std::println("Result: {}", eval(tree));
}
// 7a31aed8-07e7-4c69-afad-4f5fcb2e08f8 end
