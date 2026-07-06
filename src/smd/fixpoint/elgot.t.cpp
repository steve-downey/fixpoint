// src/smd/fixpoint/elgot.t.cpp                                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/elgot.hpp>
#include <smd/fixpoint/elgot.hpp> // Re-inclusion check

#include <smd/concrete/functors.hpp>
#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/overloaded.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <smd/typeclass/either.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <variant>
#include <vector>

using smd::concrete::Cons;
using smd::concrete::IntListF;
using smd::concrete::NatF;
using smd::concrete::Nil;
using smd::concrete::Succ;
using smd::concrete::Zero;
using smd::fixpoint::coelgot;
using smd::fixpoint::elgot;
using smd::fixpoint::make_box;
using smd::fixpoint::overloaded;
using smd::fixpoint::refold;
using smd::typeclass::either;
using smd::typeclass::make_left;
using smd::typeclass::make_right;

TEST_CASE("elgot - HeaderIsIdempotent") { REQUIRE(true); }

// ---------------------------------------------------------------------
// Law (design §9): elgot(phi, always-Right(psi)) == refold(phi, psi) — a
// coalgebra that never short-circuits degenerates elgot to an ordinary
// refold.
// ---------------------------------------------------------------------

namespace {

auto nat_count_psi(int n) -> NatF<int> {
    if (n <= 0) {
        return Zero{};
    }
    return Succ<int>{make_box<int>(n - 1)};
}

auto nat_count_phi(const NatF<int> &layer) -> int {
    return std::visit(overloaded{
                          [](const Zero &) { return 0; },
                          [](const Succ<int> &s) { return *s.pred + 1; },
                      },
                      layer);
}

} // namespace

TEST_CASE("elgot law: always-Right degenerates to refold (Nat)") {
    auto always_right = [](int n) -> either<int, NatF<int>> {
        return make_right<int>(nat_count_psi(n));
    };

    for (int n = 0; n <= 10; ++n) {
        CHECK(elgot<int, NatF>(nat_count_phi, always_right, n) ==
              refold<int, NatF>(nat_count_phi, nat_count_psi, n));
    }
}

// ---------------------------------------------------------------------
// Law (design §9): coelgot(ignore-seed(phi), psi) == refold(phi, psi) — an
// algebra that ignores the seed it's handed degenerates coelgot to the
// same refold.
// ---------------------------------------------------------------------

TEST_CASE("coelgot law: ignoring the seed degenerates to refold (Nat)") {
    auto ignore_seed = [](const int &, const NatF<int> &layer) -> int {
        return nat_count_phi(layer);
    };

    for (int n = 0; n <= 10; ++n) {
        CHECK(coelgot<int, NatF>(ignore_seed, nat_count_psi, n) ==
              refold<int, NatF>(nat_count_phi, nat_count_psi, n));
    }
}

// ---------------------------------------------------------------------
// Invariant: coelgot evaluates the coalgebra exactly once per seed. A
// careless transcription that binds ψ(seed) once to build the layer for
// fmap-ing but then calls it again elsewhere (or simply forgets to bind it
// to a local at all and re-invokes it) would double the invocation count
// here without changing the final numeric answer for a pure coalgebra —
// same discriminating-by-count discipline as elgot's short-circuit test
// above.
// ---------------------------------------------------------------------

TEST_CASE("coelgot invariant: coalgebra evaluated exactly once per seed "
          "(Nat)") {
    int invocations = 0;
    auto counting_psi = [&invocations](int n) -> NatF<int> {
        ++invocations;
        return nat_count_psi(n);
    };
    auto ignore_seed_counting = [](const int &, const NatF<int> &layer) -> int {
        return nat_count_phi(layer);
    };

    int result = coelgot<int, NatF>(ignore_seed_counting, counting_psi, 6);

    CHECK(result == 6);
    // Seeds 6, 5, 4, 3, 2, 1, 0 — exactly one coalgebra call each.
    CHECK(invocations == 7);
}

// ---------------------------------------------------------------------
// Behavior + the central discriminating claim (short-circuit is REAL):
// product over an IntListF generated from a vector, coalgebra returns
// Left(0) the moment it sees a 0. The invocation counter is essential
// here, not decorative: a buggy elgot that discards the Left result but
// still expands one extra layer before doing so would still compute the
// right *product* on this input (multiplying by 0 swallows the
// difference) but would show a higher invocation count. Checking the
// count is what actually proves the coalgebra is never called again past
// the point it returns Left.
// ---------------------------------------------------------------------

namespace {

auto make_counting_product_coalgebra(const std::vector<int> &v,
                                     int &invocations) {
    return [&v,
            &invocations](std::size_t i) -> either<int, IntListF<std::size_t>> {
        ++invocations;
        if (i >= v.size()) {
            return make_right<int>(IntListF<std::size_t>{Nil<int>{}});
        }
        if (v[i] == 0) {
            return make_left<IntListF<std::size_t>>(0);
        }
        return make_right<int>(IntListF<std::size_t>{
            Cons<int, std::size_t>{v[i], make_box<std::size_t>(i + 1)}});
    };
}

auto product_algebra(const IntListF<int> &layer) -> int {
    return std::visit(
        overloaded{
            [](const Nil<int> &) { return 1; },
            [](const Cons<int, int> &c) { return c.head * *c.tail; },
        },
        layer);
}

} // namespace

TEST_CASE("elgot behavior: product-with-zero bailout counts invocations "
          "(IntListF)") {
    std::vector<int> v{2, 3, 0, 5, 7};
    int invocations = 0;
    auto coalgebra = make_counting_product_coalgebra(v, invocations);

    int result =
        elgot<int, IntListF>(product_algebra, coalgebra, std::size_t{0});

    CHECK(result == 0);
    // The 0 sits at index 2 (0-based): the coalgebra is invoked for
    // indices 0, 1, 2 — exactly 3 times — and never for indices 3, 4
    // (values 5, 7), which are never examined.
    CHECK(invocations == 3);
}

// ---------------------------------------------------------------------
// Edge case: the coalgebra can short-circuit on the VERY FIRST call (the
// seed itself triggers Left) — elgot must return that answer directly
// without ever looking at the rest of the (never-built) layer.
// ---------------------------------------------------------------------

TEST_CASE("elgot edge case: coalgebra short-circuits on the very first "
          "call") {
    std::vector<int> v{0, 5, 9};
    int invocations = 0;
    auto coalgebra = make_counting_product_coalgebra(v, invocations);

    int result =
        elgot<int, IntListF>(product_algebra, coalgebra, std::size_t{0});

    CHECK(result == 0);
    CHECK(invocations == 1);
}

// ---------------------------------------------------------------------
// Behavior: coelgot where the algebra uses the seed — building the list
// of running indices. The fold result at seed n prepends n to the
// already-folded result for n - 1.
// ---------------------------------------------------------------------

namespace {

auto running_indices_algebra(const int &seed,
                             const NatF<std::vector<int>> &layer)
    -> std::vector<int> {
    return std::visit(
        overloaded{
            [&](const Zero &) -> std::vector<int> { return {seed}; },
            [&](const Succ<std::vector<int>> &s) -> std::vector<int> {
                std::vector<int> result{seed};
                result.insert(result.end(), s.pred->begin(), s.pred->end());
                return result;
            },
        },
        layer);
}

} // namespace

TEST_CASE("coelgot behavior: running indices, algebra uses the seed (Nat)") {
    std::vector<int> expected{5, 4, 3, 2, 1, 0};
    auto result = coelgot<std::vector<int>, NatF>(running_indices_algebra,
                                                  nat_count_psi, 5);
    CHECK(result == expected);
}

// ---------------------------------------------------------------------
// constexpr (design D10). Self-contained local lambda algebras/coalgebras
// per scheme, per house pattern (histo.t.cpp/mendler.t.cpp).
// ---------------------------------------------------------------------

namespace {

constexpr auto elgot_constexpr_smoke() -> bool {
    auto algebra = [](const NatF<int> &layer) -> int {
        return std::visit(overloaded{
                              [](const Zero &) { return 0; },
                              [](const Succ<int> &s) { return *s.pred + 1; },
                          },
                          layer);
    };
    auto coalgebra = [](int n) -> either<int, NatF<int>> {
        if (n <= 0) {
            return make_left<NatF<int>>(0);
        }
        NatF<int> layer = Succ<int>{make_box<int>(n - 1)};
        return make_right<int>(layer);
    };
    return elgot<int, NatF>(algebra, coalgebra, 4) == 4;
}

constexpr auto coelgot_constexpr_smoke() -> bool {
    auto algebra = [](const int &seed, const NatF<int> &layer) -> int {
        return std::visit(overloaded{
                              [&](const Zero &) { return seed; },
                              [](const Succ<int> &s) { return *s.pred + 1; },
                          },
                          layer);
    };
    auto coalgebra = [](int n) -> NatF<int> {
        if (n <= 0) {
            return Zero{};
        }
        return Succ<int>{make_box<int>(n - 1)};
    };
    return coelgot<int, NatF>(algebra, coalgebra, 4) == 4;
}

} // namespace

static_assert(elgot_constexpr_smoke());
static_assert(coelgot_constexpr_smoke());
