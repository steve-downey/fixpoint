// src/smd/fixpoint/cofree.t.cpp                                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/cofree.hpp> // Re-inclusion check

#include <smd/concrete/functors.hpp>
#include <smd/fixpoint/box.hpp>

#include <catch2/catch_test_macros.hpp>

#include <variant>

using smd::concrete::NatF;
using smd::concrete::Succ;
using smd::concrete::Zero;
using smd::fixpoint::Cofree;
using smd::fixpoint::extract;
using smd::fixpoint::make_box;
using smd::fixpoint::unwrap_cofree;

TEST_CASE("cofree - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

using AnnNat = Cofree<NatF, int>;

// A small annotated Nat, built by hand: 0 <- 1 <- 2, each node annotated
// with its own value (design §5.3's "F<Cofree> tail" shape, exercised
// directly rather than through histo).
auto make_ann_nat_012() -> AnnNat {
    AnnNat ann0{0, Zero{}};
    AnnNat ann1{1, Succ<AnnNat>{make_box<AnnNat>(ann0)}};
    AnnNat ann2{2, Succ<AnnNat>{make_box<AnnNat>(ann1)}};
    return ann2;
}

} // namespace

TEST_CASE("cofree: hand-built annotated Nat exposes head/tail") {
    AnnNat ann2 = make_ann_nat_012();
    CHECK(extract(ann2) == 2);
    CHECK(std::holds_alternative<Succ<AnnNat>>(unwrap_cofree(ann2)));

    const auto &succ2 = std::get<Succ<AnnNat>>(unwrap_cofree(ann2));
    CHECK(extract(*succ2.pred) == 1);
    const auto &succ1 = std::get<Succ<AnnNat>>(unwrap_cofree(*succ2.pred));
    CHECK(extract(*succ1.pred) == 0);
    CHECK(std::holds_alternative<Zero>(unwrap_cofree(*succ1.pred)));
}

TEST_CASE("cofree: equality is structural") {
    AnnNat lhs = make_ann_nat_012();
    AnnNat rhs = make_ann_nat_012();
    CHECK(lhs == rhs);

    AnnNat different{99, Zero{}};
    CHECK_FALSE(lhs == different);
}

// ---------------------------------------------------------------------
// Functor instance: fmap touches every head throughout the whole tree, not
// just the root.
// ---------------------------------------------------------------------

TEST_CASE("cofree functor: fmap increments every head, all the way down") {
    AnnNat ann2 = make_ann_nat_012();
    auto &functor = smd::typeclass::functor_typeclass<AnnNat>;
    AnnNat mapped = functor.fmap([](int head) { return head + 10; }, ann2);

    CHECK(extract(mapped) == 12);
    const auto &succ2 = std::get<Succ<AnnNat>>(unwrap_cofree(mapped));
    CHECK(extract(*succ2.pred) == 11);
    const auto &succ1 = std::get<Succ<AnnNat>>(unwrap_cofree(*succ2.pred));
    CHECK(extract(*succ1.pred) == 10);
}

// ---------------------------------------------------------------------
// Comonad laws (design §6.3/§9): extract . duplicate == id;
// fmap(extract) . duplicate == id. Both need Cofree's ==.
// ---------------------------------------------------------------------

TEST_CASE("cofree comonad law: extract . duplicate == id") {
    AnnNat ann2 = make_ann_nat_012();
    const auto &comonad = smd::typeclass::comonad_typeclass<AnnNat>;
    CHECK(comonad.extract(comonad.duplicate(ann2)) == ann2);
}

TEST_CASE("cofree comonad law: fmap(extract) . duplicate == id") {
    AnnNat ann2 = make_ann_nat_012();
    const auto &comonad = smd::typeclass::comonad_typeclass<AnnNat>;
    auto duplicated = comonad.duplicate(ann2);
    auto roundtripped = comonad.fmap(
        [&comonad](const AnnNat &inner) { return comonad.extract(inner); },
        duplicated);
    CHECK(roundtripped == ann2);
}

TEST_CASE("cofree comonad: extend is fmap . duplicate") {
    AnnNat ann2 = make_ann_nat_012();
    const auto &comonad = smd::typeclass::comonad_typeclass<AnnNat>;
    auto via_extend = comonad.extend(
        [](const AnnNat &inner) { return extract(inner) * 2; }, ann2);
    auto via_fmap_duplicate =
        comonad.fmap([](const AnnNat &inner) { return extract(inner) * 2; },
                     comonad.duplicate(ann2));
    CHECK(via_extend == via_fmap_duplicate);
}

// ---------------------------------------------------------------------
// constexpr (design D10).
// ---------------------------------------------------------------------

namespace {

constexpr auto cofree_constexpr_smoke() -> bool {
    AnnNat ann0{0, Zero{}};
    AnnNat ann1{1, Succ<AnnNat>{make_box<AnnNat>(ann0)}};

    const auto &comonad = smd::typeclass::comonad_typeclass<AnnNat>;
    bool law_ok = comonad.extract(comonad.duplicate(ann1)) == ann1;

    const auto &functor = smd::typeclass::functor_typeclass<AnnNat>;
    AnnNat mapped = functor.fmap([](int h) { return h + 1; }, ann1);
    bool fmap_ok = extract(mapped) == 2;

    return law_ok && fmap_ok;
}

} // namespace

static_assert(cofree_constexpr_smoke());
