// zygo_balanced.cpp                                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Demonstrates zygo (design §7.3): a height-balanced check on an IntTree
// (from smd/concrete/functors.hpp), where the helper fold computes each
// subtree's height and the main fold consults the helper's value at every
// child position to decide whether the tree is balanced (children's
// heights differ by at most 1, recursively).

#include <smd/concrete/functors.hpp>
#include <smd/fixpoint/zygo.hpp>

#include <print>
#include <utility>
#include <variant>

using smd::concrete::IntTree;
using smd::concrete::IntTreeF;
using smd::concrete::Leaf;
using smd::concrete::make_leaf;
using smd::concrete::make_node;
using smd::concrete::Node;
using smd::fixpoint::overloaded;
using smd::fixpoint::zygo;

namespace {

// 5b5399fc-2b3b-4c0c-81de-4a2529bada66
// Helper algebra: F<Helper> -> Helper, Helper = int (subtree height).
auto height(const IntTreeF<int> &layer) -> int {
    return std::visit(overloaded{
                          [](const Leaf<int> &) { return 0; },
                          [](const Node<int> &n) {
                              int l = *n.left;
                              int r = *n.right;
                              return 1 + (l > r ? l : r);
                          },
                      },
                      layer);
}

// Main algebra: F<std::pair<Helper, Result>> -> Result, Result = bool. Each
// child arrives as {its height, whether it's balanced}; a Node is balanced
// iff both children are balanced *and* their heights differ by at most 1.
auto is_balanced(const IntTreeF<std::pair<int, bool>> &layer) -> bool {
    return std::visit(overloaded{
                          [](const Leaf<int> &) { return true; },
                          [](const Node<std::pair<int, bool>> &n) {
                              int lh = n.left->first;
                              int rh = n.right->first;
                              int diff = lh > rh ? lh - rh : rh - lh;
                              return diff <= 1 && n.left->second &&
                                     n.right->second;
                          },
                      },
                      layer);
}
// 5b5399fc-2b3b-4c0c-81de-4a2529bada66 end

} // namespace

int main() {
    // Perfectly balanced: two subtrees of height 1 apiece.
    IntTree balanced = make_node(make_node(make_leaf(1), make_leaf(2)),
                                 make_node(make_leaf(3), make_leaf(4)));
    std::println("balanced tree is balanced: {}",
                 zygo<bool, int>(height, is_balanced, balanced));

    // Unbalanced: a height-3 left spine next to a single leaf.
    IntTree left_spine = make_node(
        make_node(make_node(make_leaf(1), make_leaf(2)), make_leaf(3)),
        make_leaf(4));
    IntTree unbalanced = make_node(left_spine, make_leaf(5));
    std::println("unbalanced tree is balanced: {}",
                 zygo<bool, int>(height, is_balanced, unbalanced));
}
