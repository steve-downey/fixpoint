// src/smd/fixpoint/para.t.cpp                                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/para.hpp>
#include <smd/fixpoint/para.hpp> // Re-inclusion check

#include <smd/concrete/functors.hpp>
#include <smd/fixpoint/overloaded.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <variant>
#include <vector>

using smd::concrete::Cons;
using smd::concrete::IntList;
using smd::concrete::IntListF;
using smd::concrete::list_from_vector;
using smd::concrete::Nat;
using smd::concrete::nat_from_int;
using smd::concrete::NatF;
using smd::concrete::Nil;
using smd::concrete::Succ;
using smd::concrete::Zero;
using smd::fixpoint::fold_fix;
using smd::fixpoint::overloaded;
using smd::fixpoint::para;

TEST_CASE("para - HeaderIsIdempotent") { REQUIRE(true); }

// ---------------------------------------------------------------------
// Law (design §9): para projecting only `.second` at every child
// degenerates to fold_fix with the plain (non-paramorphic) algebra.
// ---------------------------------------------------------------------

TEST_CASE("para law: projecting .second degenerates to fold_fix (Nat)") {
    auto plain_algebra = [](const NatF<int> &layer) -> int {
        return std::visit(overloaded{
                              [](const Zero &) { return 0; },
                              [](const Succ<int> &s) { return *s.pred + 1; },
                          },
                          layer);
    };
    auto para_algebra = [](const NatF<std::pair<Nat, int>> &layer) -> int {
        return std::visit(overloaded{
                              [](const Zero &) { return 0; },
                              [](const Succ<std::pair<Nat, int>> &s) {
                                  return (*s.pred).second + 1;
                              },
                          },
                          layer);
    };

    for (int n = 0; n <= 10; ++n) {
        Nat nat = nat_from_int(n);
        CHECK(para<int>(para_algebra, nat) ==
              fold_fix<int>(plain_algebra, nat));
    }
}

TEST_CASE("para law: projecting .second degenerates to fold_fix (IntList)") {
    auto plain_algebra = [](const IntListF<int> &layer) -> int {
        return std::visit(
            overloaded{
                [](const Nil<int> &) { return 0; },
                [](const Cons<int, int> &c) { return c.head + *c.tail; },
            },
            layer);
    };
    auto para_algebra =
        [](const IntListF<std::pair<IntList, int>> &layer) -> int {
        return std::visit(overloaded{
                              [](const Nil<int> &) { return 0; },
                              [](const Cons<int, std::pair<IntList, int>> &c) {
                                  return c.head + (*c.tail).second;
                              },
                          },
                          layer);
    };

    for (const auto &v :
         std::vector<std::vector<int>>{{}, {1, 2, 3}, {5, 4, 3, 2, 1}}) {
        IntList list = list_from_vector(v);
        CHECK(para<int>(para_algebra, list) ==
              fold_fix<int>(plain_algebra, list));
    }
}

// ---------------------------------------------------------------------
// Behavior: "tails" — the algebra collects, at each Cons, the *original*
// tail (not the fold result) reified back into a vector, building the list
// of all suffixes. Hand-built against the classic `tails` answer.
// ---------------------------------------------------------------------

TEST_CASE("para behavior: tails collects each node's original tail (IntList)") {
    IntList list = list_from_vector({1, 2, 3});

    auto algebra =
        [](const IntListF<std::pair<IntList, std::vector<std::vector<int>>>>
               &layer) -> std::vector<std::vector<int>> {
        return std::visit(
            overloaded{
                [](const Nil<int> &) -> std::vector<std::vector<int>> {
                    return {std::vector<int>{}};
                },
                [](const Cons<
                    int, std::pair<IntList, std::vector<std::vector<int>>>> &c)
                    -> std::vector<std::vector<int>> {
                    // Original (unfolded) tail, reified back to a vector —
                    // this is exactly the "look back at the original
                    // subtree" capability fold_fix does not have.
                    std::vector<int> original_tail =
                        smd::concrete::list_to_vector((*c.tail).first);
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
    };

    auto result = para<std::vector<std::vector<int>>>(algebra, list);
    std::vector<std::vector<int>> expected{{1, 2, 3}, {2, 3}, {3}, {}};
    CHECK(result == expected);
}

// ---------------------------------------------------------------------
// constexpr (design D10).
// ---------------------------------------------------------------------

namespace {

constexpr auto para_constexpr_smoke() -> bool {
    auto nat = nat_from_int(3);
    auto para_algebra = [](const NatF<std::pair<Nat, int>> &layer) -> int {
        return std::visit(overloaded{
                              [](const Zero &) { return 0; },
                              [](const Succ<std::pair<Nat, int>> &s) {
                                  return (*s.pred).second + 1;
                              },
                          },
                          layer);
    };
    return para<int>(para_algebra, nat) == 3;
}

} // namespace

static_assert(para_constexpr_smoke());
