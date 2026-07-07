// src/smd/fixpoint/histo.t.cpp                                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/histo.hpp>
#include <smd/fixpoint/histo.hpp> // Re-inclusion check

#include <smd/concrete/functors.hpp>
#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/overloaded.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <variant>

using smd::concrete::Nat;
using smd::concrete::nat_from_int;
using smd::concrete::NatF;
using smd::concrete::Succ;
using smd::concrete::Zero;
using smd::fixpoint::Cofree;
using smd::fixpoint::extract;
using smd::fixpoint::fold_fix;
using smd::fixpoint::histo;
using smd::fixpoint::layer_fmap;
using smd::fixpoint::overloaded;

TEST_CASE("histo - HeaderIsIdempotent") { REQUIRE(true); }

// ---------------------------------------------------------------------
// Law (design §9): histo with an algebra that only looks at children's
// heads (i.e. discards the history and only uses the folded Result at each
// child) degenerates to fold_fix with the corresponding plain algebra.
// ---------------------------------------------------------------------

namespace {

auto plain_count_algebra(const NatF<int> &layer) -> int {
    return std::visit(overloaded{
                          [](const Zero &) { return 0; },
                          [](const Succ<int> &s) { return *s.pred + 1; },
                      },
                      layer);
}

auto heads_only_algebra(const NatF<Cofree<NatF, int>> &layer) -> int {
    // Project every child's head, then defer to the plain algebra — this is
    // exactly "ignore the rest of the history", the degeneracy §9 asks for.
    auto heads = layer_fmap(
        [](const Cofree<NatF, int> &c) -> int { return extract(c); }, layer);
    return plain_count_algebra(heads);
}

} // namespace

TEST_CASE("histo law: heads-only algebra degenerates to fold_fix (Nat)") {
    for (int n = 0; n <= 10; ++n) {
        Nat nat = nat_from_int(n);
        CHECK(histo<int>(heads_only_algebra, nat) ==
              fold_fix<int>(plain_count_algebra, nat));
    }
}

// ---------------------------------------------------------------------
// Behavior: Fibonacci via histo on Nat (design §7.5/§9's histo-fib).
// Succ c: c.head is fib(n-1); if c's own tail is Zero, n-1 == 0, so
// fib(n) = 1 (the fib(1) base case); otherwise c's tail is Succ(cc), and
// cc.head is fib(n-2), so fib(n) = c.head + cc.head.
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

TEST_CASE("histo behavior: Fibonacci via histo on Nat, 0..10") {
    int expected[] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55};
    for (int n = 0; n <= 10; ++n) {
        CHECK(histo<int>(fib_algebra, nat_from_int(n)) == expected[n]);
    }
}

// ---------------------------------------------------------------------
// Behavior: minimal coin-change count for coins {1, 4, 5} via histo on Nat
// (design §7.5/§10's histo_coin_change, the classic histomorphism example).
// At a node representing amount n, s.pred is the history for amount n-1;
// look_back walks further back through that history to reach n-4 and n-5.
// ---------------------------------------------------------------------

namespace {

constexpr auto look_back(const Cofree<NatF, int> &c, int steps)
    -> std::optional<int> {
    if (steps == 0) {
        return c.head;
    }
    if (std::holds_alternative<Zero>(c.tail)) {
        return std::nullopt; // ran off the front of the history: n - k < 0
    }
    return look_back(*std::get<Succ<Cofree<NatF, int>>>(c.tail).pred,
                     steps - 1);
}

auto coin_change_algebra(const NatF<Cofree<NatF, int>> &layer) -> int {
    return std::visit(
        overloaded{
            [](const Zero &) { return 0; },
            [](const Succ<Cofree<NatF, int>> &s) -> int {
                const Cofree<NatF, int> &pred = *s.pred; // history for n-1
                int best = pred.head;                    // use coin 1
                if (auto m4 = look_back(pred, 3)) {      // n-1-3 == n-4
                    if (*m4 < best) {
                        best = *m4;
                    }
                }
                if (auto m5 = look_back(pred, 4)) { // n-1-4 == n-5
                    if (*m5 < best) {
                        best = *m5;
                    }
                }
                return best + 1;
            },
        },
        layer);
}

auto min_coins(int n) -> int {
    return histo<int>(coin_change_algebra, nat_from_int(n));
}

} // namespace

TEST_CASE("histo behavior: coin-change {1,4,5} minimal counts") {
    CHECK(min_coins(0) == 0);
    CHECK(min_coins(1) == 1);
    CHECK(min_coins(4) == 1);
    CHECK(min_coins(5) == 1);
    CHECK(min_coins(8) == 2);  // 4 + 4
    CHECK(min_coins(12) == 3); // 4 + 4 + 4
}

// ---------------------------------------------------------------------
// constexpr (design D10): histo-fib(6) == 8 at compile time. Uses its own
// local (lambda, hence implicitly constexpr) algebra rather than the
// free-function `fib_algebra` above, following the house pattern (e.g.
// zygo.t.cpp's `zygo_constexpr_smoke`) of keeping the constexpr smoke test
// self-contained.
// ---------------------------------------------------------------------

namespace {

constexpr auto histo_fib_constexpr_smoke() -> bool {
    auto algebra = [](const NatF<Cofree<NatF, int>> &layer) -> int {
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
    };
    return histo<int>(algebra, nat_from_int(6)) == 8;
}

} // namespace

static_assert(histo_fib_constexpr_smoke());
