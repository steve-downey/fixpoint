// src/smd/fixpoint/ghylo.t.cpp                                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/generalized.hpp>
#include <smd/fixpoint/generalized.hpp> // Re-inclusion check

#include <smd/fixpoint/chrono.hpp>
#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/dist_laws.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/functors.hpp>
#include <smd/fixpoint/overloaded.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <smd/typeclass/identity.hpp>

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <type_traits>
#include <variant>

using smd::fixpoint::chrono;
using smd::fixpoint::codyna;
using smd::fixpoint::Cofree;
using smd::fixpoint::dist_ana;
using smd::fixpoint::dist_cata;
using smd::fixpoint::dist_futu;
using smd::fixpoint::dist_histo;
using smd::fixpoint::dyna;
using smd::fixpoint::Free;
using smd::fixpoint::ghylo;
using smd::fixpoint::ghylo_with;
using smd::fixpoint::layer_fmap;
using smd::fixpoint::make_box;
using smd::fixpoint::NatF;
using smd::fixpoint::overloaded;
using smd::fixpoint::pure_free;
using smd::fixpoint::refold;
using smd::fixpoint::Succ;
using smd::fixpoint::Zero;

using smd::typeclass::Identity;
using smd::typeclass::comonad_typeclass;
using smd::typeclass::monad_typeclass;

TEST_CASE("ghylo - HeaderIsIdempotent") { REQUIRE(true); }

// ---------------------------------------------------------------------
// Shared fixtures (reused verbatim shape from chrono.t.cpp, which keeps
// them in its own anonymous namespace).
// ---------------------------------------------------------------------

namespace {

constexpr auto countdown(int m) -> NatF<int> {
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

// Plain algebra (F<Result> -> Result): counts Succ layers (refold's own
// algebra shape, and codyna's cata side, which never builds a Cofree).
constexpr auto plain_count_algebra(const NatF<int> &layer) -> int {
    return std::visit(overloaded{
                           [](const Zero &) { return 0; },
                           [](const Succ<int> &s) { return *s.pred + 1; },
                       },
                       layer);
}

// Fibonacci via history (design §7.5/§9's histo-fib, reused verbatim from
// histo.t.cpp/chrono.t.cpp's fib_algebra).
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

// cata-style algebra projection (F<Identity<Result>> -> Result), mirrors
// generalized.hpp's own cata_via_gcata wrapping exactly.
constexpr auto cata_algebra_prime(const NatF<Identity<int>> &layer) -> int {
    return plain_count_algebra(
        layer_fmap([](const Identity<int> &i) { return i.value; }, layer));
}

// ana-style coalgebra lift (Seed -> F<Identity<Seed>>), mirrors
// generalized.hpp's own ana_via_gana wrapping exactly.
constexpr auto ana_coalgebra_prime(int m) -> NatF<Identity<int>> {
    return layer_fmap([](int x) { return Identity<int>{x}; }, countdown(m));
}

// Element-generic NatF functor, deliberately NOT registered -- exercises
// ghylo_with's multi-typeclass threading with an unregistered functor. The
// projection/lift fixtures below thread it too (via layer_fmap's mode-3 form).
struct GenericNatFunctor {
    template <class Fn, class A>
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
inline constexpr GenericNatFunctor generic_nat{};

} // namespace

// ---------------------------------------------------------------------
// Law (design §9): ghylo(dist_cata, dist_ana) == refold, 0..10.
// ---------------------------------------------------------------------

TEST_CASE("ghylo law: ghylo(dist_cata, dist_ana) equals refold (Nat count)") {
    for (int n = 0; n <= 10; ++n) {
        int via_ghylo = (ghylo<int, Identity<int>, NatF, Identity<int>>(
            dist_cata, cata_algebra_prime, dist_ana, ana_coalgebra_prime, n));
        int via_refold = refold<int, NatF>(plain_count_algebra, countdown, n);
        CHECK(via_ghylo == via_refold);
    }
}

// ghylo_with (design D12, multi-typeclass): threading an unregistered functor
// (comonad + monad stay canonical) still recovers refold. The algebra/coalgebra
// fixtures thread the same functor via layer_fmap's mode-3 form, so nothing
// touches the global registry.
TEST_CASE("ghylo_with: ghylo_with(dist_cata, dist_ana) matches refold "
          "(unregistered functor)") {
    auto alg_prime = [](const NatF<Identity<int>> &layer) -> int {
        return plain_count_algebra(layer_fmap(
            generic_nat, [](const Identity<int> &i) { return i.value; },
            layer));
    };
    auto coalg_prime = [](int m) -> NatF<Identity<int>> {
        return layer_fmap(
            generic_nat, [](int x) { return Identity<int>{x}; }, countdown(m));
    };
    for (int n = 0; n <= 10; ++n) {
        int via_ghylo_with =
            (ghylo_with<int, Identity<int>, NatF, Identity<int>>(
                generic_nat, comonad_typeclass<Identity<int>>,
                monad_typeclass<Identity<int>>, dist_cata, alg_prime, dist_ana,
                coalg_prime, n));
        int via_refold = refold<int, NatF>(plain_count_algebra, countdown, n);
        CHECK(via_ghylo_with == via_refold);
    }
}

// ---------------------------------------------------------------------
// Law (design §9): ghylo(dist_histo, dist_ana) == dyna (Fibonacci, 0..10).
// ---------------------------------------------------------------------

TEST_CASE("ghylo law: ghylo(dist_histo, dist_ana) equals dyna (Fibonacci)") {
    for (int n = 0; n <= 10; ++n) {
        int via_ghylo =
            (ghylo<int, Cofree<NatF, int>, NatF, Identity<int>>(
                dist_histo<NatF>, fib_algebra, dist_ana, ana_coalgebra_prime,
                n));
        int via_dyna = dyna<int, NatF>(fib_algebra, countdown, n);
        CHECK(via_ghylo == via_dyna);
    }
}

// ---------------------------------------------------------------------
// Law (design §9): ghylo(dist_cata, dist_futu) == codyna, 0..10.
// ---------------------------------------------------------------------

TEST_CASE("ghylo law: ghylo(dist_cata, dist_futu) equals codyna") {
    for (int n = 0; n <= 10; ++n) {
        int via_ghylo = (ghylo<int, Identity<int>, NatF, Free<NatF, int>>(
            dist_cata, cata_algebra_prime, dist_futu<NatF>,
            one_layer_countdown, n));
        int via_codyna =
            codyna<int, NatF>(plain_count_algebra, one_layer_countdown, n);
        CHECK(via_ghylo == via_codyna);
    }
}

// ---------------------------------------------------------------------
// Law (design §9): ghylo(dist_histo, dist_futu) == chrono (Fibonacci,
// 0..10).
// ---------------------------------------------------------------------

TEST_CASE("ghylo law: ghylo(dist_histo, dist_futu) equals chrono (Fibonacci)") {
    for (int n = 0; n <= 10; ++n) {
        int via_ghylo =
            (ghylo<int, Cofree<NatF, int>, NatF, Free<NatF, int>>(
                dist_histo<NatF>, fib_algebra, dist_futu<NatF>,
                one_layer_countdown, n));
        int via_chrono = chrono<int, NatF>(fib_algebra, one_layer_countdown, n);
        CHECK(via_ghylo == via_chrono);
    }
}

// ---------------------------------------------------------------------
// constexpr (design D10).
// ---------------------------------------------------------------------

namespace {

constexpr auto ghylo_constexpr_smoke() -> bool {
    return (ghylo<int, Identity<int>, NatF, Identity<int>>(
               dist_cata, cata_algebra_prime, dist_ana, ana_coalgebra_prime,
               4)) == 4;
}

} // namespace

static_assert(ghylo_constexpr_smoke());
