// src/smd/fixpoint/gana.t.cpp                                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/generalized.hpp>
#include <smd/fixpoint/generalized.hpp> // Re-inclusion check

#include <smd/fixpoint/apo.hpp>
#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/dist_laws.hpp>
#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/functors.hpp>
#include <smd/fixpoint/futu.hpp>
#include <smd/fixpoint/overloaded.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <smd/typeclass/either.hpp>
#include <smd/typeclass/identity.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <utility>
#include <variant>
#include <vector>

using smd::fixpoint::ana_via_gana;
using smd::fixpoint::apo;
using smd::fixpoint::apo_via_gana;
using smd::fixpoint::Cons;
using smd::fixpoint::dist_apo;
using smd::fixpoint::Fix;
using smd::fixpoint::Free;
using smd::fixpoint::futu;
using smd::fixpoint::futu_via_gana;
using smd::fixpoint::gana;
using smd::fixpoint::IntList;
using smd::fixpoint::IntListF;
using smd::fixpoint::layer_fmap;
using smd::fixpoint::list_from_vector;
using smd::fixpoint::list_to_vector;
using smd::fixpoint::make_box;
using smd::fixpoint::Nat;
using smd::fixpoint::nat_to_int;
using smd::fixpoint::NatF;
using smd::fixpoint::Nil;
using smd::fixpoint::overloaded;
using smd::fixpoint::pure_free;
using smd::fixpoint::roll_free;
using smd::fixpoint::Succ;
using smd::fixpoint::unfold_fix;
using smd::fixpoint::unwrap_fix;
using smd::fixpoint::Zero;

using smd::typeclass::either;
using smd::typeclass::make_left;
using smd::typeclass::make_right;

TEST_CASE("gana - HeaderIsIdempotent") { REQUIRE(true); }

// ---------------------------------------------------------------------
// ana_via_gana ≡ unfold_fix (Nat, 0..10; design §7.10/§9).
// ---------------------------------------------------------------------

namespace {

// constexpr (not a lambda) so it can be shared between the runtime
// TEST_CASEs and the constexpr smoke test below (house pattern, e.g.
// generalized.t.cpp's nat_count_algebra).
constexpr auto countdown(int m) -> NatF<int> {
    if (m <= 0) {
        return Zero{};
    }
    return Succ<int>{make_box<int>(m - 1)};
}

} // namespace

TEST_CASE("gana law: ana_via_gana equals unfold_fix (Nat, 0..10)") {
    for (int n = 0; n <= 10; ++n) {
        Nat via_gana = ana_via_gana<NatF>(countdown, n);
        Nat via_unfold = unfold_fix<NatF>(countdown, n);
        CHECK(nat_to_int(via_gana) == nat_to_int(via_unfold));
    }
}

// ---------------------------------------------------------------------
// apo_via_gana ≡ apo (the S04 sorted-insert fixture, apo.t.cpp, reused
// verbatim here since it lives in apo.t.cpp's own anonymous namespace).
// ---------------------------------------------------------------------

namespace {

auto make_insert_coalgebra(int value) {
    return [value](const IntList &l) -> IntListF<either<IntList, IntList>> {
        const auto &layer = unwrap_fix(l);
        return std::visit(
            overloaded{
                [&](const Nil<int> &) -> IntListF<either<IntList, IntList>> {
                    return Cons<int, either<IntList, IntList>>{
                        value,
                        make_box<either<IntList, IntList>>(
                            make_left<IntList>(l))};
                },
                [&](const Cons<int, IntList> &c)
                    -> IntListF<either<IntList, IntList>> {
                    if (value <= c.head) {
                        return Cons<int, either<IntList, IntList>>{
                            value,
                            make_box<either<IntList, IntList>>(
                                make_left<IntList>(l))};
                    }
                    return Cons<int, either<IntList, IntList>>{
                        c.head, make_box<either<IntList, IntList>>(
                                    make_right<IntList>(*c.tail))};
                },
            },
            layer);
    };
}

} // namespace

TEST_CASE("gana law: apo_via_gana equals apo (sorted insert fixture)") {
    IntList sorted = list_from_vector({1, 3, 7, 9});

    auto via_gana = apo_via_gana<IntListF>(make_insert_coalgebra(5), sorted);
    auto via_apo = apo<IntListF>(make_insert_coalgebra(5), sorted);

    std::vector<int> expected{1, 3, 5, 7, 9};
    CHECK(list_to_vector(via_gana) == expected);
    CHECK(list_to_vector(via_gana) == list_to_vector(via_apo));
}

// ---------------------------------------------------------------------
// apo_via_gana ≡ apo, with Left != Right as *types* (Seed = int, graft
// side = IntList): the sorted-insert fixture above has Seed = IntList =
// Fix<IntListF>, so Left and Right happen to be the *same* type there and
// a flipped D4 orientation (Left = continue, Right = graft) would not be
// caught by it (confirmed by mutation testing below, see handoff) -- the
// same same-type blind spot S12's handoff flagged for dist_zygo. This
// fixture's Left (IntList) and Right (int) are genuinely different types,
// so a flipped orientation fails to even compile the same way (apo's own
// coalgebra fixes which side means what), and if it did compile, would
// produce a different graft point.
// ---------------------------------------------------------------------

namespace {

auto make_countdown_apo_coalgebra(const IntList &graft_tail) {
    return [graft_tail](int n) -> IntListF<either<IntList, int>> {
        if (n <= 0) {
            return Cons<int, either<IntList, int>>{
                0, make_box<either<IntList, int>>(make_left<int>(graft_tail))};
        }
        return Cons<int, either<IntList, int>>{
            n, make_box<either<IntList, int>>(make_right<IntList>(n - 1))};
    };
}

} // namespace

TEST_CASE("gana law: apo_via_gana equals apo (Left != Right types, graft)") {
    IntList graft_tail = list_from_vector({100, 200});
    auto coalgebra = make_countdown_apo_coalgebra(graft_tail);

    auto via_gana = apo_via_gana<IntListF>(coalgebra, 5);
    auto via_apo = apo<IntListF>(coalgebra, 5);

    std::vector<int> expected{5, 4, 3, 2, 1, 0, 100, 200};
    CHECK(list_to_vector(via_gana) == expected);
    CHECK(list_to_vector(via_gana) == list_to_vector(via_apo));
}

// ---------------------------------------------------------------------
// futu_via_gana ≡ futu (the S08 RLE fixture, futu.t.cpp, reused verbatim
// here since it lives in futu.t.cpp's own anonymous namespace).
// ---------------------------------------------------------------------

namespace {

using IndexFree = Free<IntListF, std::size_t>;

auto make_run(int value, int count, std::size_t seed) -> IndexFree {
    if (count <= 0) {
        return pure_free<IntListF>(seed);
    }
    return roll_free<IntListF>(IntListF<IndexFree>{Cons<int, IndexFree>{
        value, make_box<IndexFree>(make_run(value, count - 1, seed))}});
}

auto make_rle_coalgebra(const std::vector<std::pair<int, int>> &pairs) {
    return [&pairs](std::size_t i) -> IntListF<IndexFree> {
        if (i >= pairs.size()) {
            return Nil<int>{};
        }
        auto [count, value] = pairs[i];
        return Cons<int, IndexFree>{
            value, make_box<IndexFree>(make_run(value, count - 1, i + 1))};
    };
}

} // namespace

TEST_CASE("gana law: futu_via_gana equals futu (RLE fixture {{2,7},{3,1}})") {
    std::vector<std::pair<int, int>> rle{
        {2, 7},
        {3, 1},
    };
    IntList via_gana =
        futu_via_gana<IntListF>(make_rle_coalgebra(rle), std::size_t{0});
    IntList via_futu =
        futu<IntListF>(make_rle_coalgebra(rle), std::size_t{0});

    std::vector<int> expected{7, 7, 1, 1, 1};
    CHECK(list_to_vector(via_gana) == expected);
    CHECK(list_to_vector(via_gana) == list_to_vector(via_futu));
}

// ---------------------------------------------------------------------
// Direct gana call (design §7.10/step file): pins the public spelling,
// explicit template args, mirroring gcata.t.cpp's direct-call pinning
// test. Builds a small IntList via a never-grafting apo-shaped coalgebra
// (Seed = int), with dist_apo's explicit X bound to MSeed once, exactly
// as apo_via_gana does internally.
// ---------------------------------------------------------------------

TEST_CASE("gana direct call: explicit template args pin the public spelling") {
    using MSeed = either<Fix<IntListF>, int>;

    auto coalgebra = [](int n) -> IntListF<MSeed> {
        if (n <= 0) {
            return Nil<int>{};
        }
        return Cons<int, MSeed>{n, make_box<MSeed>(make_right<IntList>(n - 1))};
    };
    auto dist_apo_x = [](const auto &e) {
        return dist_apo.template operator()<MSeed>(e);
    };

    auto via_gana = gana<IntListF, MSeed>(dist_apo_x, coalgebra, 4);
    std::vector<int> expected{4, 3, 2, 1};
    CHECK(list_to_vector(via_gana) == expected);
}

// ---------------------------------------------------------------------
// constexpr (design D10).
// ---------------------------------------------------------------------

namespace {

constexpr auto ana_via_gana_constexpr_smoke() -> bool {
    return nat_to_int(ana_via_gana<NatF>(countdown, 4)) == 4;
}

} // namespace

static_assert(ana_via_gana_constexpr_smoke());
