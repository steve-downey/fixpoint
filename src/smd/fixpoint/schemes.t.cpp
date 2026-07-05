// src/smd/fixpoint/schemes.t.cpp                                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/schemes.hpp>
#include <smd/fixpoint/schemes.hpp> // Re-inclusion check

#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <variant>

// Compile-coverage only (design §16's own step file): one representative
// scheme per header/family in the §8 table, each checked against a trivial,
// hand-computed answer on the shared Nat fixture (functors.hpp). The real
// equivalence-law proofs live in each scheme's own *.t.cpp; this file only
// proves schemes.hpp (the umbrella) actually brings in everything needed to
// call one of each, with no missing include/ODR surprise from combining all
// of them in a single translation unit.

using smd::fixpoint::ana_via_gana;
using smd::fixpoint::cata_via_gcata;
using smd::fixpoint::Cofree;
using smd::fixpoint::dist_cata;
using smd::fixpoint::dyna;
using smd::fixpoint::elgot;
using smd::fixpoint::extract;
using smd::fixpoint::fold_fix;
using smd::fixpoint::futu;
using smd::fixpoint::histo;
using smd::fixpoint::IntListF;
using smd::fixpoint::make_box;
using smd::fixpoint::mcata;
using smd::fixpoint::Nat;
using smd::fixpoint::nat_from_int;
using smd::fixpoint::nat_to_int;
using smd::fixpoint::NatF;
using smd::fixpoint::overloaded;
using smd::fixpoint::para;
using smd::fixpoint::prepro;
using smd::fixpoint::pure_free;
using smd::fixpoint::Succ;
using smd::fixpoint::unwrap_fix;
using smd::fixpoint::Zero;

using smd::typeclass::either;
using smd::typeclass::Identity;
using smd::typeclass::make_left;
using smd::typeclass::make_right;

TEST_CASE("schemes - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

// Shared plain algebra: NatF<int> -> int, "count the Succ layers" (the same
// shape as functors.hpp's own nat_to_int, redeclared locally per this
// module's own precedent of not reaching across translation units).
constexpr auto nat_count_algebra(const NatF<int> &layer) -> int {
    return std::visit(overloaded{
                          [](const Zero &) { return 0; },
                          [](const Succ<int> &s) { return *s.pred + 1; },
                      },
                      layer);
}

constexpr auto countdown(int m) -> NatF<int> {
    if (m <= 0) {
        return Zero{};
    }
    return Succ<int>{make_box<int>(m - 1)};
}

// The natural-transformation fixture prepro.hpp's own convention requires
// (design §4): a templated call operator, generic over every layer type.
struct identity_nat {
    template <class Layer>
    constexpr auto operator()(const Layer &layer) const -> Layer {
        return layer;
    }
};

// histo's algebra: F<Cofree<F,Result>> -> Result. A heads-only projection
// (design §9's own histo-degenerates-to-fold_fix law) is enough to prove the
// header compiles and computes the right count.
auto heads_only_histo_algebra(const NatF<Cofree<NatF, int>> &layer) -> int {
    return std::visit(overloaded{
                          [](const Zero &) { return 0; },
                          [](const Succ<Cofree<NatF, int>> &s) {
                              return extract(*s.pred) + 1;
                          },
                      },
                      layer);
}

// mcata's algebra: (recurse, const F<Fix<F>>&) -> Result.
auto mcata_count_algebra = [](auto recurse, const NatF<Nat> &layer) -> int {
    return std::visit(
        overloaded{
            [](const Zero &) { return 0; },
            [&](const Succ<Nat> &s) -> int { return recurse(*s.pred) + 1; },
        },
        layer);
};

// para's algebra: F<std::pair<Fix<F>,Result>> -> Result, original ignored.
auto para_count_algebra(const NatF<std::pair<Nat, int>> &layer) -> int {
    return std::visit(overloaded{
                          [](const Zero &) { return 0; },
                          [](const Succ<std::pair<Nat, int>> &s) {
                              return s.pred->second + 1;
                          },
                      },
                      layer);
}

// futu's coalgebra: Seed -> F<Free<F,Seed>>, one plain layer per step (the
// §9 futu-degenerates-to-unfold_fix law's own fixture shape).
auto futu_one_layer_coalgebra(int m) -> NatF<smd::fixpoint::Free<NatF, int>> {
    if (m <= 0) {
        return Zero{};
    }
    return Succ<smd::fixpoint::Free<NatF, int>>{
        make_box<smd::fixpoint::Free<NatF, int>>(pure_free<NatF>(m - 1))};
}

// elgot's coalgebra: Seed -> either<Result, F<Seed>>, never short-circuits
// (the §9 elgot-degenerates-to-refold law's own fixture shape).
auto elgot_always_right_coalgebra(int m) -> either<int, NatF<int>> {
    return make_right<int>(countdown(m));
}

} // namespace

TEST_CASE("schemes: classical fold_fix/unfold_fix compile via the umbrella") {
    Nat nat = nat_from_int(5);
    CHECK(fold_fix<int>(nat_count_algebra, nat) == 5);
}

TEST_CASE("schemes: para compiles via the umbrella") {
    Nat nat = nat_from_int(5);
    CHECK(para<int>(para_count_algebra, nat) == 5);
}

TEST_CASE("schemes: prepro compiles via the umbrella") {
    Nat nat = nat_from_int(5);
    CHECK(prepro<int>(identity_nat{}, nat_count_algebra, nat) == 5);
}

TEST_CASE("schemes: histo compiles via the umbrella") {
    Nat nat = nat_from_int(5);
    CHECK(histo<int>(heads_only_histo_algebra, nat) == 5);
}

TEST_CASE("schemes: futu compiles via the umbrella") {
    Nat nat = futu<NatF>(futu_one_layer_coalgebra, 5);
    CHECK(nat_to_int(nat) == 5);
}

TEST_CASE("schemes: dyna compiles via the umbrella") {
    CHECK((dyna<int, NatF>(heads_only_histo_algebra, countdown, 5)) == 5);
}

TEST_CASE("schemes: mcata compiles via the umbrella") {
    Nat nat = nat_from_int(5);
    CHECK((mcata<int, NatF>(mcata_count_algebra, nat)) == 5);
}

TEST_CASE("schemes: elgot compiles via the umbrella") {
    CHECK((elgot<int, NatF>(nat_count_algebra, elgot_always_right_coalgebra,
                            5)) == 5);
}

TEST_CASE("schemes: dist_cata compiles via the umbrella") {
    NatF<Identity<int>> layer =
        Succ<Identity<int>>{make_box<Identity<int>>(Identity<int>{4})};
    Identity<NatF<int>> distributed = dist_cata(layer);
    CHECK(std::holds_alternative<Succ<int>>(distributed.value));
}

TEST_CASE("schemes: gcata (cata_via_gcata) compiles via the umbrella") {
    Nat nat = nat_from_int(5);
    CHECK(cata_via_gcata<int>(nat_count_algebra, nat) == 5);
}

TEST_CASE("schemes: gana (ana_via_gana) compiles via the umbrella") {
    Nat nat = ana_via_gana<NatF>(countdown, 5);
    CHECK(nat_to_int(nat) == 5);
}
