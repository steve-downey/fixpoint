// src/smd/fixpoint/fix.t.cpp                                         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/fix.hpp> // Re-inclusion check

#include <smd/fixpoint/box.hpp>

#include <catch2/catch_test_macros.hpp>

#include <variant>

using smd::fixpoint::Box;
using smd::fixpoint::Fix;
using smd::fixpoint::make_box;
using smd::fixpoint::unwrap_fix;
using smd::fixpoint::wrap_fix;

namespace {

struct Zero {};

template <typename A>
struct Succ {
    Box<A> pred;
};

template <typename A>
using NatF = std::variant<Zero, Succ<A>>;

} // namespace

TEST_CASE("Fix - NatFZero") {
    using Nat = Fix<NatF>;
    auto zero = wrap_fix<NatF>(NatF<Nat>{Zero{}});
    const auto &layer = unwrap_fix(zero);
    CHECK(std::holds_alternative<Zero>(layer));
}

TEST_CASE("Fix - NatFSucc") {
    using Nat = Fix<NatF>;
    auto zero = wrap_fix<NatF>(NatF<Nat>{Zero{}});
    auto one = wrap_fix<NatF>(NatF<Nat>{Succ<Nat>{make_box<Nat>(zero)}});
    auto two = wrap_fix<NatF>(NatF<Nat>{Succ<Nat>{make_box<Nat>(one)}});

    const auto &layer2 = unwrap_fix(two);
    REQUIRE(std::holds_alternative<Succ<Nat>>(layer2));

    const auto &layer1 = unwrap_fix(*std::get<Succ<Nat>>(layer2).pred);
    REQUIRE(std::holds_alternative<Succ<Nat>>(layer1));

    const auto &layer0 = unwrap_fix(*std::get<Succ<Nat>>(layer1).pred);
    CHECK(std::holds_alternative<Zero>(layer0));
}

TEST_CASE("Fix - WrapUnwrapRoundTrip") {
    using Nat = Fix<NatF>;
    NatF<Nat> layer{Zero{}};
    auto fixed = wrap_fix<NatF>(layer);
    const auto &recovered = unwrap_fix(fixed);
    CHECK(std::holds_alternative<Zero>(recovered));
}
