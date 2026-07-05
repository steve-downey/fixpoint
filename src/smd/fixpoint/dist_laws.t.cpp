// src/smd/fixpoint/dist_laws.t.cpp                                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/dist_laws.hpp>
#include <smd/fixpoint/dist_laws.hpp> // Re-inclusion check

#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/functors.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <smd/typeclass/either.hpp>
#include <smd/typeclass/functor.hpp>
#include <smd/typeclass/identity.hpp>

#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <variant>

using smd::fixpoint::Cofree;
using smd::fixpoint::dist_apo;
using smd::fixpoint::dist_ana;
using smd::fixpoint::dist_cata;
using smd::fixpoint::dist_futu;
using smd::fixpoint::dist_gapo;
using smd::fixpoint::dist_histo;
using smd::fixpoint::dist_para;
using smd::fixpoint::dist_zygo;
using smd::fixpoint::extract;
using smd::fixpoint::Fix;
using smd::fixpoint::Free;
using smd::fixpoint::IntList;
using smd::fixpoint::IntListF;
using smd::fixpoint::is_pure;
using smd::fixpoint::layer_fmap;
using smd::fixpoint::make_box;
using smd::fixpoint::Nat;
using smd::fixpoint::nat_from_int;
using smd::fixpoint::NatF;
using smd::fixpoint::overloaded;
using smd::fixpoint::pure_free;
using smd::fixpoint::roll_free;
using smd::fixpoint::Succ;
using smd::fixpoint::unwrap_fix;
using smd::fixpoint::wrap_fix;
using smd::fixpoint::Zero;

using smd::fixpoint::Cons;
using smd::fixpoint::Nil;

using smd::typeclass::either;
using smd::typeclass::functor_typeclass;
using smd::typeclass::Identity;
using smd::typeclass::is_left;
using smd::typeclass::left;
using smd::typeclass::make_left;
using smd::typeclass::make_right;
using smd::typeclass::right;

TEST_CASE("dist_laws - HeaderIsIdempotent") { REQUIRE(true); }

// ---------------------------------------------------------------------
// dist_cata :: F<Identity<A>> -> Identity<F<A>>
// Round-trips both NatF alternatives (Zero has no payload to check beyond
// shape; Succ carries a value).
// ---------------------------------------------------------------------

TEST_CASE("dist_cata: round-trips NatF<Identity<int>> shapes") {
    NatF<Identity<int>> zero_layer{Zero{}};
    CHECK(dist_cata(zero_layer) == Identity<NatF<int>>{NatF<int>{Zero{}}});

    NatF<Identity<int>> succ_layer{Succ<Identity<int>>{make_box<Identity<int>>(Identity<int>{5})}};
    CHECK(dist_cata(succ_layer) ==
          Identity<NatF<int>>{NatF<int>{Succ<int>{make_box<int>(5)}}});
}

// ---------------------------------------------------------------------
// dist_ana :: Identity<F<A>> -> F<Identity<A>>
// Round-trips both IntListF alternatives.
// ---------------------------------------------------------------------

TEST_CASE("dist_ana: round-trips IntListF<int> shapes") {
    Identity<IntListF<int>> nil_ident{IntListF<int>{Nil<int>{}}};
    CHECK(dist_ana(nil_ident) == IntListF<Identity<int>>{Nil<int>{}});

    Identity<IntListF<int>> cons_ident{
        IntListF<int>{Cons<int, int>{7, make_box<int>(3)}}};
    CHECK(dist_ana(cons_ident) ==
          IntListF<Identity<int>>{
              Cons<int, Identity<int>>{7, make_box<Identity<int>>(Identity<int>{3})}});
}

// ---------------------------------------------------------------------
// dist_histo :: F<Cofree<F,A>> -> Cofree<F, F<A>>
// Two-level Nat history: c0 (head 0, Zero tail) <- c1 (head 1, tail holds
// c0). dist_histo is called on c1's tail (an F-layer), matching its own
// signature.
// ---------------------------------------------------------------------

namespace {

auto make_nat_cofree_chain() -> Cofree<NatF, int> {
    Cofree<NatF, int> c0{0, NatF<Cofree<NatF, int>>{Zero{}}};
    Cofree<NatF, int> c1{
        1, NatF<Cofree<NatF, int>>{Succ<Cofree<NatF, int>>{
               make_box<Cofree<NatF, int>>(c0)}}};
    return c1;
}

} // namespace

TEST_CASE("dist_histo: F<Cofree<F,A>> -> Cofree<F, F<A>> on a two-level Nat "
          "history") {
    Cofree<NatF, int> c1 = make_nat_cofree_chain();
    Cofree<NatF, int> c0 = *std::get<Succ<Cofree<NatF, int>>>(c1.tail).pred;

    Cofree<NatF, NatF<int>> expected_inner{
        NatF<int>{Zero{}}, NatF<Cofree<NatF, NatF<int>>>{Zero{}}};
    Cofree<NatF, NatF<int>> expected{
        NatF<int>{Succ<int>{make_box<int>(extract(c0))}},
        NatF<Cofree<NatF, NatF<int>>>{Succ<Cofree<NatF, NatF<int>>>{
            make_box<Cofree<NatF, NatF<int>>>(expected_inner)}}};

    CHECK(dist_histo<NatF>(c1.tail) == expected);

    // Zero-layer input degenerates to a Zero-headed, Zero-tailed Cofree.
    NatF<Cofree<NatF, int>> zero_layer{Zero{}};
    Cofree<NatF, NatF<int>> expected_zero{NatF<int>{Zero{}},
                                         NatF<Cofree<NatF, NatF<int>>>{Zero{}}};
    CHECK(dist_histo<NatF>(zero_layer) == expected_zero);
}

// ---------------------------------------------------------------------
// dist_futu :: Free<F, F<A>> -> F<Free<F,A>>
// Pure and Roll chunks on Nat.
// ---------------------------------------------------------------------

TEST_CASE("dist_futu: Free<F, F<A>> -> F<Free<F,A>> on Pure and Roll chunks "
          "(Nat)") {
    // Pure layer -> fmapF(pure_free, layer).
    Free<NatF, NatF<int>> pure_chunk = pure_free<NatF>(NatF<int>{
        Succ<int>{make_box<int>(5)}});
    NatF<Free<NatF, int>> expected_from_pure{
        Succ<Free<NatF, int>>{make_box<Free<NatF, int>>(pure_free<NatF>(5))}};
    CHECK(dist_futu<NatF>(pure_chunk) == expected_from_pure);

    // Roll layer -> fmapF(roll_free . dist_futu, layer): one outer Succ
    // wrapping a Pure child chunk holding NatF<int>{Zero{}}.
    Free<NatF, NatF<int>> child_chunk = pure_free<NatF>(NatF<int>{Zero{}});
    Free<NatF, NatF<int>> roll_chunk = roll_free<NatF>(
        NatF<Free<NatF, NatF<int>>>{Succ<Free<NatF, NatF<int>>>{
            make_box<Free<NatF, NatF<int>>>(child_chunk)}});

    NatF<Free<NatF, int>> expected_inner{Zero{}};
    NatF<Free<NatF, int>> expected_from_roll{
        Succ<Free<NatF, int>>{make_box<Free<NatF, int>>(
            roll_free<NatF>(expected_inner))}};
    CHECK(dist_futu<NatF>(roll_chunk) == expected_from_roll);
}

// ---------------------------------------------------------------------
// dist_zygo(helper) :: F<pair<B,X>> -> pair<B, F<X>>
// ---------------------------------------------------------------------

namespace {

constexpr auto sum_helper(const NatF<int> &layer) -> int {
    return std::visit(overloaded{
                          [](const Zero &) { return 0; },
                          [](const Succ<int> &s) { return *s.pred + 1; },
                      },
                      layer);
}

} // namespace

TEST_CASE("dist_zygo(helper): F<pair<B,X>> -> pair<B, F<X>> on NatF") {
    auto law = dist_zygo(sum_helper);

    NatF<std::pair<int, char>> zero_layer{Zero{}};
    auto zero_result = law(zero_layer);
    CHECK(zero_result.first == 0);
    CHECK(zero_result.second == NatF<char>{Zero{}});

    NatF<std::pair<int, char>> succ_layer{
        Succ<std::pair<int, char>>{make_box<std::pair<int, char>>(
            std::pair<int, char>{3, 'z'})}};
    auto succ_result = law(succ_layer);
    CHECK(succ_result.first == 4); // sum_helper(F<int> with the projected 3)
    CHECK(succ_result.second == NatF<char>{Succ<char>{make_box<char>('z')}});
}

// ---------------------------------------------------------------------
// dist_para :: F<pair<Fix<F>, X>> -> pair<Fix<F>, F<X>>   (B = Fix<F>)
// ---------------------------------------------------------------------

TEST_CASE("dist_para: F<pair<Fix<F>,X>> -> pair<Fix<F>, F<X>> on NatF") {
    Nat two = nat_from_int(2);

    NatF<std::pair<Nat, char>> layer{
        Succ<std::pair<Nat, char>>{make_box<std::pair<Nat, char>>(
            std::pair<Nat, char>{two, 'p'})}};

    auto result = dist_para<NatF>(layer);
    // First component reconstructs Fix<F> from every child's original
    // subtree (wrap_fix of the projected-.first layer) — i.e. Succ(two).
    CHECK(smd::fixpoint::nat_to_int(result.first) == 3);
    CHECK(result.second == NatF<char>{Succ<char>{make_box<char>('p')}});
}

// ---------------------------------------------------------------------
// dist_apo :: Either<Fix<F>, F<X>> -> F<Either<Fix<F>, X>>
// Left (graft), Right (continue), and the same-type-Seed = Fix<F> case.
// ---------------------------------------------------------------------

TEST_CASE("dist_apo: Left graft and Right continue on NatF") {
    Nat existing = nat_from_int(3); // Succ(Succ(Succ(Zero)))

    // Right(layer): fmapF(make_right<L>, layer) — an int seed continues.
    either<Nat, NatF<int>> right_input =
        make_right<Nat>(NatF<int>{Succ<int>{make_box<int>(7)}});
    auto right_result = dist_apo.operator()<int>(right_input);
    REQUIRE(std::holds_alternative<Succ<either<Nat, int>>>(right_result));
    const auto &succ_alt = std::get<Succ<either<Nat, int>>>(right_result);
    CHECK_FALSE(is_left(*succ_alt.pred));
    CHECK(right(*succ_alt.pred) == 7);

    // Left(t): fmapF(make_left, unfix(t)) — fans existing's children out as
    // Lefts (design §7.9's "graft" behavior).
    either<Nat, NatF<int>> left_input = make_left<NatF<int>>(existing);
    auto left_result = dist_apo.operator()<int>(left_input);
    REQUIRE(std::holds_alternative<Succ<either<Nat, int>>>(left_result));
    const auto &left_succ_alt = std::get<Succ<either<Nat, int>>>(left_result);
    REQUIRE(is_left(*left_succ_alt.pred));
    // The grafted child is existing's own predecessor subtree (Succ(Succ(Zero))).
    CHECK(smd::fixpoint::nat_to_int(left(*left_succ_alt.pred)) == 2);
}

TEST_CASE("dist_apo: either<Fix<F>, F<Fix<F>>>-shaped case (Seed = Fix<F>)") {
    Nat existing = nat_from_int(1); // Succ(Zero)
    Nat seed_tree = nat_from_int(5);

    // Right side holds a full F<Fix<F>> layer (Seed = Fix<F> itself).
    either<Nat, NatF<Nat>> right_input = make_right<Nat>(
        NatF<Nat>{Succ<Nat>{make_box<Nat>(seed_tree)}});
    auto right_result = dist_apo.operator()<Nat>(right_input);
    REQUIRE(std::holds_alternative<Succ<either<Nat, Nat>>>(right_result));
    const auto &right_succ = std::get<Succ<either<Nat, Nat>>>(right_result);
    CHECK_FALSE(is_left(*right_succ.pred));
    CHECK(smd::fixpoint::nat_to_int(right(*right_succ.pred)) == 5);

    either<Nat, NatF<Nat>> left_input = make_left<NatF<Nat>>(existing);
    auto left_result = dist_apo.operator()<Nat>(left_input);
    REQUIRE(std::holds_alternative<Succ<either<Nat, Nat>>>(left_result));
    const auto &left_succ = std::get<Succ<either<Nat, Nat>>>(left_result);
    REQUIRE(is_left(*left_succ.pred));
    CHECK(smd::fixpoint::nat_to_int(left(*left_succ.pred)) == 0); // existing's child
}

// ---------------------------------------------------------------------
// dist_gapo(coalg) :: (b -> F<b>) -> Either<b, F<X>> -> F<Either<b,X>>
// Sanity check with a small int-counting coalgebra standing in for the
// "b" carrier (design §7.9: generalizes dist_apo with a coalgebra for the
// Left side).
// ---------------------------------------------------------------------

TEST_CASE("dist_gapo(coalg): generalizes dist_apo with a coalgebra") {
    auto coalg = [](int b) -> NatF<int> {
        if (b <= 0) {
            return Zero{};
        }
        return Succ<int>{make_box<int>(b - 1)};
    };
    auto law = dist_gapo(coalg);

    either<int, NatF<char>> right_input = make_right<int>(
        NatF<char>{Succ<char>{make_box<char>('r')}});
    auto right_result = law.operator()<char>(right_input);
    REQUIRE(std::holds_alternative<Succ<either<int, char>>>(right_result));
    CHECK_FALSE(is_left(*std::get<Succ<either<int, char>>>(right_result).pred));
    CHECK(right(*std::get<Succ<either<int, char>>>(right_result).pred) == 'r');

    either<int, NatF<char>> left_input = make_left<NatF<char>>(4);
    auto left_result = law.operator()<char>(left_input);
    REQUIRE(std::holds_alternative<Succ<either<int, char>>>(left_result));
    REQUIRE(is_left(*std::get<Succ<either<int, char>>>(left_result).pred));
    CHECK(left(*std::get<Succ<either<int, char>>>(left_result).pred) == 3);
}

// ---------------------------------------------------------------------
// Naturality spot-checks (design §4/step file):
//   dist(layer_fmap(fmap_W(f), l)) == fmap_W(layer_fmap(f))(dist(l))
// ---------------------------------------------------------------------

TEST_CASE("dist_cata naturality: commutes with fmap on a concrete NatF "
          "layer") {
    NatF<Identity<int>> l{
        Succ<Identity<int>>{make_box<Identity<int>>(Identity<int>{3})}};
    auto f = [](int x) { return x + 10; };

    // LHS: dist_cata(layer_fmap(fmap_Identity(f), l))
    auto lhs = dist_cata(layer_fmap(
        [&](const Identity<int> &i) {
            return functor_typeclass<Identity<int>>.fmap(f, i);
        },
        l));

    // RHS: fmap_Identity(layer_fmap(f))(dist_cata(l))
    auto dist_l = dist_cata(l);
    auto rhs = functor_typeclass<Identity<NatF<int>>>.fmap(
        [&](const NatF<int> &layer) { return layer_fmap(f, layer); }, dist_l);

    CHECK(lhs == rhs);
}

TEST_CASE("dist_histo naturality: commutes with fmap on a concrete NatF "
          "cofree layer") {
    Cofree<NatF, int> c1 = make_nat_cofree_chain();
    auto f = [](int x) { return x + 10; };

    // LHS: dist_histo<NatF>(layer_fmap(fmap_Cofree(f), c1.tail))
    auto lhs = dist_histo<NatF>(layer_fmap(
        [&](const Cofree<NatF, int> &c) {
            return functor_typeclass<Cofree<NatF, int>>.fmap(f, c);
        },
        c1.tail));

    // RHS: fmap_Cofree(layer_fmap(f))(dist_histo<NatF>(c1.tail))
    auto dist_l = dist_histo<NatF>(c1.tail);
    auto rhs = functor_typeclass<Cofree<NatF, NatF<int>>>.fmap(
        [&](const NatF<int> &layer) { return layer_fmap(f, layer); }, dist_l);

    CHECK(lhs == rhs);
}

// ---------------------------------------------------------------------
// constexpr (design D10).
// ---------------------------------------------------------------------

namespace {

constexpr auto dist_cata_constexpr_smoke() -> bool {
    NatF<Identity<int>> layer{
        Succ<Identity<int>>{make_box<Identity<int>>(Identity<int>{6})}};
    return dist_cata(layer) == Identity<NatF<int>>{NatF<int>{Succ<int>{make_box<int>(6)}}};
}

constexpr auto dist_zygo_constexpr_smoke() -> bool {
    auto law = dist_zygo(sum_helper);
    NatF<std::pair<int, int>> layer{
        Succ<std::pair<int, int>>{make_box<std::pair<int, int>>(
            std::pair<int, int>{2, 9})}};
    auto result = law(layer);
    return result.first == 3 && result.second == NatF<int>{Succ<int>{make_box<int>(9)}};
}

} // namespace

static_assert(dist_cata_constexpr_smoke());
static_assert(dist_zygo_constexpr_smoke());
