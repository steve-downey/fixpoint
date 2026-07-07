// src/smd/fixpoint/chrono.t.cpp                                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/chrono.hpp>
#include <smd/fixpoint/chrono.hpp> // Re-inclusion check

#include <smd/concrete/functors.hpp>
#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/futu.hpp>
#include <smd/fixpoint/histo.hpp>
#include <smd/fixpoint/overloaded.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <variant>

using smd::concrete::NatF;
using smd::concrete::Succ;
using smd::concrete::Zero;
using smd::fixpoint::chrono;
using smd::fixpoint::codyna;
using smd::fixpoint::Cofree;
using smd::fixpoint::dyna;
using smd::fixpoint::fold_fix;
using smd::fixpoint::Free;
using smd::fixpoint::futu;
using smd::fixpoint::histo;
using smd::fixpoint::layer_fmap;
using smd::fixpoint::make_box;
using smd::fixpoint::overloaded;
using smd::fixpoint::pure_free;
using smd::fixpoint::unfold_fix;

TEST_CASE("chrono - HeaderIsIdempotent") { REQUIRE(true); }

// ---------------------------------------------------------------------
// Shared fixtures: a countdown ana-style coalgebra (int -> NatF<int>, the
// same shape as functors.hpp's nat_from_int) and its one-layer-per-step
// Free-chunk cousin (the same degeneracy shape futu.t.cpp's law test uses).
// ---------------------------------------------------------------------

namespace {

auto countdown(int m) -> NatF<int> {
    if (m <= 0) {
        return Zero{};
    }
    return Succ<int>{make_box<int>(m - 1)};
}

auto one_layer_countdown(int m) -> NatF<Free<NatF, int>> {
    return layer_fmap(
        [](int child) -> Free<NatF, int> { return pure_free<NatF>(child); },
        countdown(m));
}

// Plain algebra (F<Result> -> Result): counts Succ layers (same shape as
// nat_to_int) — codyna's cata side never builds a Cofree, so this is the
// right shape for its law test; a wrong total (double-counted or dropped
// layer from a mishandled unroll boundary) is exactly the kind of bug this
// is meant to catch.
auto plain_count_algebra(const NatF<int> &layer) -> int {
    return std::visit(overloaded{
                          [](const Zero &) { return 0; },
                          [](const Succ<int> &s) { return *s.pred + 1; },
                      },
                      layer);
}

// Fibonacci via history (design §7.5/§9's histo-fib, reused verbatim from
// histo.t.cpp's fib_algebra): Succ c has c.head == fib(n-1); if c's own tail
// is Zero, n-1 == 0 so fib(n) == 1 (the fib(1) base case); otherwise c's
// tail is Succ(cc) and cc.head == fib(n-2), so fib(n) == c.head + cc.head.
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

// ---------------------------------------------------------------------
// Law (design §9): dyna(phi, psi, s) == histo(phi, unfold_fix<F>(psi, s)),
// 0..10. dyna must never build the intermediate Nat, but the two must still
// agree at every seed.
// ---------------------------------------------------------------------

TEST_CASE("dyna law: dyna(phi, psi, s) == histo(phi, unfold_fix(psi, s))") {
    for (int n = 0; n <= 10; ++n) {
        int via_dyna = dyna<int, NatF>(fib_algebra, countdown, n);
        int via_histo_unfold =
            histo<int>(fib_algebra, unfold_fix<NatF>(countdown, n));
        CHECK(via_dyna == via_histo_unfold);
    }
}

// ---------------------------------------------------------------------
// Law (design §9): codyna(phi, psi, s) == fold_fix(phi, futu<F>(psi, s)),
// 0..10.
// ---------------------------------------------------------------------

TEST_CASE("codyna law: codyna(phi, psi, s) == fold_fix(phi, futu(psi, s))") {
    for (int n = 0; n <= 10; ++n) {
        int via_codyna =
            codyna<int, NatF>(plain_count_algebra, one_layer_countdown, n);
        int via_fold_futu = fold_fix<int>(plain_count_algebra,
                                          futu<NatF>(one_layer_countdown, n));
        CHECK(via_codyna == via_fold_futu);
    }
}

// ---------------------------------------------------------------------
// Law (design §9): chrono(phi, psi, s) == histo(phi, futu<F>(psi, s)),
// 0..10.
// ---------------------------------------------------------------------

TEST_CASE("chrono law: chrono(phi, psi, s) == histo(phi, futu(psi, s))") {
    for (int n = 0; n <= 10; ++n) {
        int via_chrono = chrono<int, NatF>(fib_algebra, one_layer_countdown, n);
        int via_histo_futu =
            histo<int>(fib_algebra, futu<NatF>(one_layer_countdown, n));
        CHECK(via_chrono == via_histo_futu);
    }
}

// ---------------------------------------------------------------------
// Behavior: Fibonacci via dyna directly from an int seed (design §7.6/§10's
// dyna_fibonacci, the canonical dynamorphism). No Nat tree is ever built —
// dyna's refold fusion interleaves countdown (the ana half) with fib_algebra
// (the histo half) one layer at a time.
// ---------------------------------------------------------------------

TEST_CASE("dyna behavior: Fibonacci directly from an int seed, 0..10") {
    int expected[] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55};
    for (int n = 0; n <= 10; ++n) {
        CHECK(dyna<int, NatF>(fib_algebra, countdown, n) == expected[n]);
    }
}

// ---------------------------------------------------------------------
// Behavior: minimal coin-change count for coins {1, 4, 5}, via dyna directly
// from the amount (design §7.6's step file) — no pre-built Nat, unlike
// histo.t.cpp's min_coins which starts from nat_from_int. Same
// coin_change_algebra shape as histo.t.cpp/histo_coin_change.cpp.
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

auto min_coins_via_dyna(int n) -> int {
    return dyna<int, NatF>(coin_change_algebra, countdown, n);
}

} // namespace

TEST_CASE("dyna behavior: coin-change {1,4,5} directly from the amount") {
    CHECK(min_coins_via_dyna(0) == 0);
    CHECK(min_coins_via_dyna(1) == 1);
    CHECK(min_coins_via_dyna(4) == 1);
    CHECK(min_coins_via_dyna(5) == 1);
    CHECK(min_coins_via_dyna(8) == 2);  // 4 + 4
    CHECK(min_coins_via_dyna(12) == 3); // 4 + 4 + 4
}

// ---------------------------------------------------------------------
// constexpr (design D10): dyna-fib(6) == 8 at compile time. Own local
// (lambda, hence implicitly constexpr) algebra/coalgebra, following the
// house pattern (e.g. histo.t.cpp's histo_fib_constexpr_smoke) of keeping
// the constexpr smoke test self-contained.
// ---------------------------------------------------------------------

namespace {

constexpr auto dyna_fib_constexpr_smoke() -> bool {
    auto psi = [](int m) -> NatF<int> {
        if (m <= 0) {
            return Zero{};
        }
        return Succ<int>{make_box<int>(m - 1)};
    };
    auto phi = [](const NatF<Cofree<NatF, int>> &layer) -> int {
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
    return dyna<int, NatF>(phi, psi, 6) == 8;
}

} // namespace

static_assert(dyna_fib_constexpr_smoke());
