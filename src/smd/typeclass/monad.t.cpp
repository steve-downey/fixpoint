// src/smd/typeclass/monad.t.cpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/typeclass/monad.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>

namespace bt = smd::typeclass;

TEST_CASE("monad: optional bind short-circuits on empty") {
    auto half_if_even = [](int x) {
        return x % 2 == 0 ? std::optional<int>{x / 2} : std::optional<int>{};
    };
    REQUIRE(bt::mbind(std::optional<int>{8}, half_if_even) ==
            std::optional<int>{4});
    REQUIRE(bt::mbind(std::optional<int>{7}, half_if_even) ==
            std::optional<int>{});
    REQUIRE(bt::mbind(std::optional<int>{}, half_if_even) ==
            std::optional<int>{});
}

TEST_CASE("monad: join flattens nested optional") {
    std::optional<std::optional<int>> nested{std::optional<int>{5}};
    REQUIRE(bt::join(nested) == std::optional<int>{5});
}

TEST_CASE("monad: apply synthesized from bind and pure") {
    const auto &m = bt::monad_typeclass<std::optional<int>>;
    auto mf = std::optional{[](int x) { return x + 100; }};
    REQUIRE(m.apply(mf, std::optional<int>{1}) == std::optional<int>{101});
}
