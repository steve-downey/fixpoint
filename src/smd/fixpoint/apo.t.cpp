// src/smd/fixpoint/apo.t.cpp                                         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/apo.hpp>
#include <smd/fixpoint/apo.hpp> // Re-inclusion check

#include <smd/fixpoint/functors.hpp>
#include <smd/fixpoint/overloaded.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <smd/typeclass/either.hpp>

#include <catch2/catch_test_macros.hpp>

#include <variant>
#include <vector>

using smd::fixpoint::apo;
using smd::fixpoint::Cons;
using smd::fixpoint::IntList;
using smd::fixpoint::IntListF;
using smd::fixpoint::list_from_vector;
using smd::fixpoint::list_to_vector;
using smd::fixpoint::make_box;
using smd::fixpoint::Nat;
using smd::fixpoint::nat_from_int;
using smd::fixpoint::nat_to_int;
using smd::fixpoint::NatF;
using smd::fixpoint::Nil;
using smd::fixpoint::overloaded;
using smd::fixpoint::Succ;
using smd::fixpoint::unwrap_fix;
using smd::fixpoint::Zero;
using smd::typeclass::either;
using smd::typeclass::make_left;
using smd::typeclass::make_right;

TEST_CASE("apo - HeaderIsIdempotent") { REQUIRE(true); }

// ---------------------------------------------------------------------
// Law (design §9): a coalgebra that always returns Right (never
// short-circuits) degenerates apo to unfold_fix.
// ---------------------------------------------------------------------

TEST_CASE("apo law: always-Right degenerates to unfold_fix (Nat)") {
    auto coalgebra = [](int n) -> NatF<either<Nat, int>> {
        if (n <= 0) {
            return Zero{};
        }
        return Succ<either<Nat, int>>{
            make_box<either<Nat, int>>(make_right<Nat>(n - 1))};
    };

    for (int n = 0; n <= 10; ++n) {
        auto via_apo = apo<NatF>(coalgebra, n);
        auto via_unfold = nat_from_int(n);
        CHECK(nat_to_int(via_apo) == nat_to_int(via_unfold));
    }
}

// ---------------------------------------------------------------------
// Seed = Fix<F>: the either<Fix<F>, Fix<F>> same-type-sides case that
// ruled out std::expected (D4). A trivial identity-ish unfold suffices.
// ---------------------------------------------------------------------

TEST_CASE("apo: Seed = Fix<F> compiles and behaves (either<Nat, Nat>)") {
    auto identity_ish_coalgebra = [](const Nat &n) -> NatF<either<Nat, Nat>> {
        const auto &layer = unwrap_fix(n);
        return std::visit(
            overloaded{
                [](const Zero &) -> NatF<either<Nat, Nat>> { return Zero{}; },
                [](const Succ<Nat> &s) -> NatF<either<Nat, Nat>> {
                    return Succ<either<Nat, Nat>>{
                        make_box<either<Nat, Nat>>(make_right<Nat>(*s.pred))};
                },
            },
            layer);
    };

    auto seed = nat_from_int(3);
    auto result = apo<NatF>(identity_ish_coalgebra, seed);
    CHECK(nat_to_int(result) == 3);
}

// ---------------------------------------------------------------------
// Behavior + zero-copy graft: sorted insert into an IntList. Once the
// insertion point is found, the coalgebra short-circuits, grafting the
// untouched remainder of the original list. Two coalgebras — one Left =
// IntList (by value copy), one Left = const IntList& (zero-copy graft,
// §5.2 reference sides) — must produce the same observable result.
// ---------------------------------------------------------------------

namespace {

/** By-value graft: Left holds a fresh copy of the ungrafted remainder. */
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
                        // Insertion point: graft the whole current list
                        // (untouched) after `value`.
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

/** Zero-copy graft: Left holds a reference into the original list — no
 * intermediate copy at the graft point.
 */
auto make_insert_coalgebra_graft(int value) {
    return [value](const IntList &l)
               -> IntListF<either<const IntList &, IntList>> {
        const auto &layer = unwrap_fix(l);
        return std::visit(
            overloaded{
                [&](const Nil<int> &)
                    -> IntListF<either<const IntList &, IntList>> {
                    return Cons<int, either<const IntList &, IntList>>{
                        value,
                        make_box<either<const IntList &, IntList>>(
                            make_left<IntList, const IntList &>(l))};
                },
                [&](const Cons<int, IntList> &c)
                    -> IntListF<either<const IntList &, IntList>> {
                    if (value <= c.head) {
                        return Cons<int, either<const IntList &, IntList>>{
                            value,
                            make_box<either<const IntList &, IntList>>(
                                make_left<IntList, const IntList &>(l))};
                    }
                    return Cons<int, either<const IntList &, IntList>>{
                        c.head,
                        make_box<either<const IntList &, IntList>>(
                            make_right<const IntList &>(*c.tail))};
                },
            },
            layer);
    };
}

} // namespace

TEST_CASE("apo behavior: sorted insert grafts the untouched tail (IntList)") {
    IntList sorted = list_from_vector({1, 3, 7, 9});

    auto by_value = apo<IntListF>(make_insert_coalgebra(5), sorted);
    std::vector<int> expected{1, 3, 5, 7, 9};
    CHECK(list_to_vector(by_value) == expected);
}

TEST_CASE("apo: zero-copy graft matches the by-value graft (IntList)") {
    IntList sorted = list_from_vector({1, 3, 7, 9});

    auto by_value = apo<IntListF>(make_insert_coalgebra(5), sorted);
    auto grafted = apo<IntListF>(make_insert_coalgebra_graft(5), sorted);

    CHECK(list_to_vector(by_value) == list_to_vector(grafted));
    std::vector<int> expected{1, 3, 5, 7, 9};
    CHECK(list_to_vector(grafted) == expected);

    // Insert at both ends too, to exercise Nil and "smaller than every
    // element" positions where the untouched-tail graft matters most.
    auto front_value = apo<IntListF>(make_insert_coalgebra_graft(0), sorted);
    CHECK(list_to_vector(front_value) ==
          std::vector<int>{0, 1, 3, 7, 9});

    auto back_value = apo<IntListF>(make_insert_coalgebra_graft(10), sorted);
    CHECK(list_to_vector(back_value) ==
          std::vector<int>{1, 3, 7, 9, 10});
}

// ---------------------------------------------------------------------
// constexpr (design D10).
// ---------------------------------------------------------------------

namespace {

constexpr auto apo_constexpr_smoke() -> bool {
    auto coalgebra = [](int n) -> NatF<either<Nat, int>> {
        if (n <= 0) {
            return Zero{};
        }
        return Succ<either<Nat, int>>{
            make_box<either<Nat, int>>(make_right<Nat>(n - 1))};
    };
    auto result = apo<NatF>(coalgebra, 3);
    return nat_to_int(result) == 3;
}

} // namespace

static_assert(apo_constexpr_smoke());
