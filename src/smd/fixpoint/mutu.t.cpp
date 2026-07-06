// src/smd/fixpoint/mutu.t.cpp                                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/mutu.hpp>
#include <smd/fixpoint/mutu.hpp> // Re-inclusion check

#include <smd/concrete/functors.hpp>
#include <smd/fixpoint/overloaded.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <variant>

using smd::concrete::Nat;
using smd::concrete::nat_from_int;
using smd::concrete::NatF;
using smd::concrete::Succ;
using smd::concrete::Zero;
using smd::fixpoint::fold_fix;
using smd::fixpoint::mutu;
using smd::fixpoint::overloaded;

TEST_CASE("mutu - HeaderIsIdempotent") { REQUIRE(true); }

// ---------------------------------------------------------------------
// Law (design §9): mutu(f, g) equals the pair computed by a hand-paired
// fold_fix over the same two algebras — the two are the same construction
// (banana-split), and this test guards against the implementation drifting
// away from it.
// ---------------------------------------------------------------------

namespace {

// alg_a: counts Succ layers (same shape as nat_to_int).
auto count_alg(const NatF<std::pair<int, int>> &layer) -> int {
    return std::visit(overloaded{
                          [](const Zero &) { return 0; },
                          [](const Succ<std::pair<int, int>> &s) {
                              return s.pred->first + 1;
                          },
                      },
                      layer);
}

// alg_b: counts Succ layers by twos — deliberately a different function of
// the structure so the pairing isn't degenerate.
auto double_count_alg(const NatF<std::pair<int, int>> &layer) -> int {
    return std::visit(overloaded{
                          [](const Zero &) { return 0; },
                          [](const Succ<std::pair<int, int>> &s) {
                              return s.pred->second + 2;
                          },
                      },
                      layer);
}

} // namespace

TEST_CASE("mutu law: consistent with a hand-paired fold_fix (Nat)") {
    auto hand_paired =
        [](const NatF<std::pair<int, int>> &layer) -> std::pair<int, int> {
        return std::pair<int, int>{count_alg(layer), double_count_alg(layer)};
    };

    for (int n = 0; n <= 10; ++n) {
        Nat nat = nat_from_int(n);
        CHECK((mutu<int, int>(count_alg, double_count_alg, nat)) ==
              fold_fix<std::pair<int, int>>(hand_paired, nat));
    }
}

// ---------------------------------------------------------------------
// Behavior: even/odd on Nat via mutu (design §7.3/§10 mutu_even_odd).
// Carrier is pair<bool, bool> = {is_even, is_odd}. Zero is even, not odd;
// a Succ is even iff its predecessor is odd (.second), and odd iff its
// predecessor is even (.first).
// ---------------------------------------------------------------------

namespace {

constexpr auto alg_even(const NatF<std::pair<bool, bool>> &layer) -> bool {
    return std::visit(
        overloaded{
            [](const Zero &) { return true; },
            [](const Succ<std::pair<bool, bool>> &s) { return s.pred->second; },
        },
        layer);
}

constexpr auto alg_odd(const NatF<std::pair<bool, bool>> &layer) -> bool {
    return std::visit(
        overloaded{
            [](const Zero &) { return false; },
            [](const Succ<std::pair<bool, bool>> &s) { return s.pred->first; },
        },
        layer);
}

} // namespace

TEST_CASE("mutu behavior: even/odd on Nat") {
    for (int n = 0; n <= 10; ++n) {
        Nat nat = nat_from_int(n);
        auto [is_even, is_odd] = mutu<bool, bool>(alg_even, alg_odd, nat);
        CHECK(is_even == (n % 2 == 0));
        CHECK(is_odd == (n % 2 == 1));
    }
}

// ---------------------------------------------------------------------
// constexpr (design D10).
// ---------------------------------------------------------------------

namespace {

constexpr auto mutu_constexpr_smoke() -> bool {
    auto nat = nat_from_int(4);
    auto result = mutu<bool, bool>(alg_even, alg_odd, nat);
    return result.first && !result.second;
}

} // namespace

static_assert(mutu_constexpr_smoke());
