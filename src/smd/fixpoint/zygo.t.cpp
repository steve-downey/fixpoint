// src/smd/fixpoint/zygo.t.cpp                                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/zygo.hpp>
#include <smd/fixpoint/zygo.hpp> // Re-inclusion check

#include <smd/fixpoint/functors.hpp>
#include <smd/fixpoint/overloaded.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <type_traits>
#include <utility>
#include <variant>

using smd::fixpoint::fold_fix;
using smd::fixpoint::make_box;
using smd::fixpoint::IntTree;
using smd::fixpoint::IntTreeF;
using smd::fixpoint::Leaf;
using smd::fixpoint::make_leaf;
using smd::fixpoint::make_node;
using smd::fixpoint::Nat;
using smd::fixpoint::nat_from_int;
using smd::fixpoint::NatF;
using smd::fixpoint::Node;
using smd::fixpoint::overloaded;
using smd::fixpoint::Succ;
using smd::fixpoint::Zero;
using smd::fixpoint::zygo;
using smd::fixpoint::zygo_with;

TEST_CASE("zygo - HeaderIsIdempotent") { REQUIRE(true); }

// ---------------------------------------------------------------------
// Law (design §9): zygo with a main algebra that ignores the helper
// component at every child position degenerates to fold_fix with the
// plain (non-zygomorphic) algebra. The helper algebra is otherwise
// arbitrary (its value is thrown away) — reuse the same counting shape for
// simplicity.
// ---------------------------------------------------------------------

TEST_CASE("zygo law: ignoring helper degenerates to fold_fix (Nat)") {
    auto plain_algebra = [](const NatF<int> &layer) -> int {
        return std::visit(overloaded{
                               [](const Zero &) { return 0; },
                               [](const Succ<int> &s) { return *s.pred + 1; },
                           },
                           layer);
    };
    auto helper_algebra = [](const NatF<int> &layer) -> int {
        return std::visit(overloaded{
                               [](const Zero &) { return 0; },
                               [](const Succ<int> &s) { return *s.pred + 1; },
                           },
                           layer);
    };
    auto main_algebra =
        [](const NatF<std::pair<int, int>> &layer) -> int {
        return std::visit(
            overloaded{
                [](const Zero &) { return 0; },
                [](const Succ<std::pair<int, int>> &s) {
                    return s.pred->second + 1;
                },
            },
            layer);
    };

    for (int n = 0; n <= 10; ++n) {
        Nat nat = nat_from_int(n);
        CHECK(zygo<int, int>(helper_algebra, main_algebra, nat) ==
              fold_fix<int>(plain_algebra, nat));
    }
}

// ---------------------------------------------------------------------
// Behavior: zygo_balanced — helper computes height, main computes
// "is height-balanced" (every subtree's children's heights differ by at
// most 1). Checked against a hand-built balanced tree and an unbalanced
// one (design §7.3/§10 zygo_balanced).
// ---------------------------------------------------------------------

namespace {

auto height_helper(const IntTreeF<int> &layer) -> int {
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

auto balanced_main(const IntTreeF<std::pair<int, bool>> &layer) -> bool {
    return std::visit(
        overloaded{
            [](const Leaf<int> &) { return true; },
            [](const Node<std::pair<int, bool>> &n) {
                int lh = n.left->first;
                int rh = n.right->first;
                int diff = lh > rh ? lh - rh : rh - lh;
                return diff <= 1 && n.left->second && n.right->second;
            },
        },
        layer);
}

} // namespace

TEST_CASE("zygo behavior: zygo_balanced on IntTree") {
    // Perfectly balanced: both sides height 1, all leaves height 0.
    IntTree balanced_tree =
        make_node(make_node(make_leaf(1), make_leaf(2)),
                  make_node(make_leaf(3), make_leaf(4)));
    CHECK(zygo<bool, int>(height_helper, balanced_main, balanced_tree));

    // Unbalanced: left spine has height 3, right side is a single leaf
    // (height 0) — the top-level diff is 2, breaking the invariant.
    IntTree left_spine = make_node(
        make_node(make_node(make_leaf(1), make_leaf(2)), make_leaf(3)),
        make_leaf(4));
    IntTree unbalanced_tree = make_node(left_spine, make_leaf(5));
    CHECK_FALSE(zygo<bool, int>(height_helper, balanced_main, unbalanced_tree));
}

// ---------------------------------------------------------------------
// _with form: zygo threading an element-generic functor instance. zygo has
// two fmap sites (fold + helper projection) at different element types, so
// the threaded object must be element-generic; the library's per-element-type
// registered instances would trip zygo_with's static_assert. This local
// object is never registered anywhere.
// ---------------------------------------------------------------------

namespace {

struct GenericNatFunctor {
    template <typename Fn, typename A>
    auto fmap(Fn &&fn, const NatF<A> &layer) const {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return std::visit(overloaded{
                              [](const Zero &) -> NatF<B> { return Zero{}; },
                              [&](const Succ<A> &s) -> NatF<B> {
                                  return Succ<B>{
                                      make_box<B>(std::invoke(fn, *s.pred))};
                              },
                          },
                          layer);
    }
};
inline constexpr GenericNatFunctor generic_nat_functor{};

} // namespace

TEST_CASE("zygo_with: matches zygo with an unregistered element-generic "
          "instance") {
    auto helper_algebra = [](const NatF<int> &layer) -> int {
        return std::visit(overloaded{
                               [](const Zero &) { return 0; },
                               [](const Succ<int> &s) { return *s.pred + 1; },
                           },
                           layer);
    };
    auto main_algebra = [](const NatF<std::pair<int, int>> &layer) -> int {
        return std::visit(
            overloaded{
                [](const Zero &) { return 0; },
                [](const Succ<std::pair<int, int>> &s) {
                    return s.pred->second + 1;
                },
            },
            layer);
    };

    for (int n = 0; n <= 10; ++n) {
        Nat nat = nat_from_int(n);
        CHECK(zygo_with<int, int>(generic_nat_functor, helper_algebra,
                                  main_algebra, nat) ==
              zygo<int, int>(helper_algebra, main_algebra, nat));
    }
}

// ---------------------------------------------------------------------
// constexpr (design D10).
// ---------------------------------------------------------------------

namespace {

constexpr auto zygo_constexpr_smoke() -> bool {
    auto helper_algebra = [](const NatF<int> &layer) -> int {
        return std::visit(overloaded{
                               [](const Zero &) { return 0; },
                               [](const Succ<int> &s) { return *s.pred + 1; },
                           },
                           layer);
    };
    auto main_algebra =
        [](const NatF<std::pair<int, int>> &layer) -> int {
        return std::visit(
            overloaded{
                [](const Zero &) { return 0; },
                [](const Succ<std::pair<int, int>> &s) {
                    return s.pred->second + 1;
                },
            },
            layer);
    };
    auto nat = nat_from_int(3);
    return zygo<int, int>(helper_algebra, main_algebra, nat) == 3;
}

} // namespace

static_assert(zygo_constexpr_smoke());
