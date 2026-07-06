// src/smd/fixpoint/generalized.t.cpp                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/generalized.hpp>
#include <smd/fixpoint/generalized.hpp> // Re-inclusion check

#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/dist_laws.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/functors.hpp>
#include <smd/fixpoint/histo.hpp>
#include <smd/fixpoint/overloaded.hpp>
#include <smd/fixpoint/para.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>
#include <smd/fixpoint/zygo.hpp>

#include <smd/typeclass/identity.hpp>

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

using smd::fixpoint::Cofree;
using smd::fixpoint::cata_via_gcata;
using smd::fixpoint::cata_via_gcata_with;
using smd::fixpoint::dist_cata;
using smd::fixpoint::fold_fix;
using smd::fixpoint::gcata;
using smd::fixpoint::histo;
using smd::fixpoint::histo_via_gcata;
using smd::fixpoint::IntList;
using smd::fixpoint::IntListF;
using smd::fixpoint::IntTree;
using smd::fixpoint::IntTreeF;
using smd::fixpoint::layer_fmap;
using smd::fixpoint::Leaf;
using smd::fixpoint::list_from_vector;
using smd::fixpoint::list_to_vector;
using smd::fixpoint::make_leaf;
using smd::fixpoint::make_node;
using smd::fixpoint::Nat;
using smd::fixpoint::nat_from_int;
using smd::fixpoint::NatF;
using smd::fixpoint::Node;
using smd::fixpoint::overloaded;
using smd::fixpoint::para;
using smd::fixpoint::para_via_gcata;
using smd::fixpoint::Succ;
using smd::fixpoint::zygo;
using smd::fixpoint::zygo_via_gcata;
using smd::fixpoint::Zero;

using smd::typeclass::Identity;

TEST_CASE("gcata - HeaderIsIdempotent") { REQUIRE(true); }

// ---------------------------------------------------------------------
// cata_via_gcata ≡ fold_fix (design §9/§7.10's recovery table).
// ---------------------------------------------------------------------

namespace {

// constexpr (not a lambda) so it can be shared between the runtime
// TEST_CASEs above and the constexpr smoke test below (mutu.t.cpp's house
// pattern, S05 handoff).
constexpr auto nat_count_algebra(const NatF<int> &layer) -> int {
    return std::visit(overloaded{
                           [](const Zero &) { return 0; },
                           [](const Succ<int> &s) { return *s.pred + 1; },
                       },
                       layer);
}

auto list_sum_algebra(const IntListF<int> &layer) -> int {
    return std::visit(
        overloaded{
            [](const smd::fixpoint::Nil<int> &) { return 0; },
            [](const smd::fixpoint::Cons<int, int> &c) {
                return c.head + *c.tail;
            },
        },
        layer);
}

// An element-generic NatF functor object, deliberately NOT registered in
// functor_typeclass -- exercises gcata_with's multi-typeclass threading with
// an unregistered functor (the comonad, Identity's, stays canonical/looked-up).
struct GenericNatFunctor {
    template <class Fn, class A>
    auto fmap(Fn &&fn, const NatF<A> &layer) const {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return std::visit(
            overloaded{
                [](const Zero &) -> NatF<B> { return Zero{}; },
                [&](const Succ<A> &s) -> NatF<B> {
                    return Succ<B>{smd::fixpoint::make_box<B>(
                        std::invoke(fn, *s.pred))};
                },
            },
            layer);
    }
};
inline constexpr GenericNatFunctor generic_nat{};

} // namespace

TEST_CASE("gcata law: cata_via_gcata equals fold_fix (Nat, 0..10)") {
    for (int n = 0; n <= 10; ++n) {
        Nat nat = nat_from_int(n);
        CHECK(cata_via_gcata<int>(nat_count_algebra, nat) ==
              fold_fix<int>(nat_count_algebra, nat));
    }
}

// gcata_with (design D12, multi-typeclass): threading an unregistered functor
// (comonad stays canonical) still recovers fold_fix.
TEST_CASE("gcata_with: cata_via_gcata_with matches fold_fix (unregistered "
          "functor)") {
    for (int n = 0; n <= 10; ++n) {
        Nat nat = nat_from_int(n);
        CHECK(cata_via_gcata_with<int>(generic_nat, nat_count_algebra, nat) ==
              fold_fix<int>(nat_count_algebra, nat));
    }
}

TEST_CASE("gcata law: cata_via_gcata equals fold_fix (IntList sum)") {
    for (const auto &v :
         std::vector<std::vector<int>>{{}, {1, 2, 3}, {5, 4, 3, 2, 1}}) {
        IntList list = list_from_vector(v);
        CHECK(cata_via_gcata<int>(list_sum_algebra, list) ==
              fold_fix<int>(list_sum_algebra, list));
    }
}

// ---------------------------------------------------------------------
// histo_via_gcata ≡ histo (fib 0..10, design §7.5/§9).
// ---------------------------------------------------------------------

namespace {

auto fib_algebra(const NatF<Cofree<NatF, int>> &layer) -> int {
    return std::visit(
        overloaded{
            [](const Zero &) { return 0; },
            [](const Succ<Cofree<NatF, int>> &s) -> int {
                const Cofree<NatF, int> &c = *s.pred;
                return std::visit(
                    overloaded{
                        [&](const Zero &) { return 1; },
                        [&](const Succ<Cofree<NatF, int>> &cc) -> int {
                            return c.head + cc.pred->head;
                        },
                    },
                    c.tail);
            },
        },
        layer);
}

} // namespace

TEST_CASE("gcata law: histo_via_gcata equals histo (Fibonacci, 0..10)") {
    for (int n = 0; n <= 10; ++n) {
        Nat nat = nat_from_int(n);
        CHECK(histo_via_gcata<int>(fib_algebra, nat) ==
              histo<int>(fib_algebra, nat));
    }
}

// ---------------------------------------------------------------------
// zygo_via_gcata ≡ zygo (the S05 zygo_balanced fixture).
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

TEST_CASE("gcata law: zygo_via_gcata equals zygo (zygo_balanced fixture)") {
    IntTree balanced_tree =
        make_node(make_node(make_leaf(1), make_leaf(2)),
                  make_node(make_leaf(3), make_leaf(4)));
    IntTree left_spine = make_node(
        make_node(make_node(make_leaf(1), make_leaf(2)), make_leaf(3)),
        make_leaf(4));
    IntTree unbalanced_tree = make_node(left_spine, make_leaf(5));

    for (const IntTree &tree : {balanced_tree, unbalanced_tree}) {
        CHECK((zygo_via_gcata<bool, int>(height_helper, balanced_main, tree)) ==
              (zygo<bool, int>(height_helper, balanced_main, tree)));
    }
}

// ---------------------------------------------------------------------
// para_via_gcata ≡ para (the S04 tails fixture).
// ---------------------------------------------------------------------

namespace {

auto tails_algebra(
    const IntListF<std::pair<IntList, std::vector<std::vector<int>>>> &layer)
    -> std::vector<std::vector<int>> {
    return std::visit(
        overloaded{
            [](const smd::fixpoint::Nil<int> &) -> std::vector<std::vector<int>> {
                return {std::vector<int>{}};
            },
            [](const smd::fixpoint::Cons<
                int, std::pair<IntList, std::vector<std::vector<int>>>> &c)
                -> std::vector<std::vector<int>> {
                std::vector<int> original_tail = list_to_vector((*c.tail).first);
                std::vector<int> current{c.head};
                current.insert(current.end(), original_tail.begin(),
                               original_tail.end());

                std::vector<std::vector<int>> result{std::move(current)};
                const auto &rest = (*c.tail).second;
                result.insert(result.end(), rest.begin(), rest.end());
                return result;
            },
        },
        layer);
}

} // namespace

TEST_CASE("gcata law: para_via_gcata equals para (tails fixture)") {
    IntList list = list_from_vector({1, 2, 3});
    auto via_gcata =
        para_via_gcata<std::vector<std::vector<int>>>(tails_algebra, list);
    auto via_para = para<std::vector<std::vector<int>>>(tails_algebra, list);
    CHECK(via_gcata == via_para);
}

// ---------------------------------------------------------------------
// Direct gcata call (design §7.10/step file): pins the public spelling,
// explicit template args, dist_cata needing none of its own.
// ---------------------------------------------------------------------

TEST_CASE("gcata direct call: explicit template args pin the public spelling") {
    auto algebra_prime = [](const NatF<Identity<int>> &layer) -> int {
        return nat_count_algebra(
            layer_fmap([](const Identity<int> &i) { return i.value; }, layer));
    };
    for (int n = 0; n <= 10; ++n) {
        Nat nat = nat_from_int(n);
        CHECK((gcata<int, Identity<int>>(dist_cata, algebra_prime, nat)) ==
              fold_fix<int>(nat_count_algebra, nat));
    }
}

// ---------------------------------------------------------------------
// constexpr (design D10).
// ---------------------------------------------------------------------

namespace {

constexpr auto cata_via_gcata_constexpr_smoke() -> bool {
    auto nat = nat_from_int(4);
    return cata_via_gcata<int>(nat_count_algebra, nat) == 4;
}

} // namespace

static_assert(cata_via_gcata_constexpr_smoke());
