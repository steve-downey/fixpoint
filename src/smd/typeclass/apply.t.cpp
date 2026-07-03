// src/smd/typeclass/apply.t.cpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/typeclass/apply.hpp>

#include "test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>

namespace bt = smd::typeclass;

TEST_CASE("apply: optional invoke combines effectful arguments") {
    const auto &app = bt::applicative_typeclass<std::optional<int>>;
    auto add = [](int a, int b, int c) { return a + b + c; };
    REQUIRE(app.invoke(add, std::optional<int>{1}, std::optional<int>{2},
                       std::optional<int>{3}) == std::optional<int>{6});
    REQUIRE(app.invoke(add, std::optional<int>{1}, std::optional<int>{},
                       std::optional<int>{3}) == std::optional<int>{});
}

TEST_CASE("apply: pure and ap on optional") {
    const auto &app = bt::applicative_typeclass<std::optional<int>>;
    auto lifted = app.pure([](int x) { return x * 10; });
    REQUIRE(app.ap(lifted, std::optional<int>{5}) == std::optional<int>{50});
}

TEST_CASE("apply: identity and homomorphism laws on optional") {
    using bt::test::check_applicative_homomorphism_law;
    using bt::test::check_applicative_identity_law;
    REQUIRE(check_applicative_identity_law(std::optional<int>{7}));
    REQUIRE(check_applicative_homomorphism_law<std::optional<int>>(
        [](int x) { return x + 1; }, 41));
}
