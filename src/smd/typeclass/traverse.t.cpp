// src/smd/typeclass/traverse.t.cpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/typeclass/traverse.hpp>

#include <smd/typeclass/sequence.hpp>

#include "test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <vector>

namespace bt = smd::typeclass;

TEST_CASE("traverse: front-door over vector with optional-returning function") {
    auto positive = [](int x) {
        return x > 0 ? std::optional<int>{x * 2} : std::optional<int>{};
    };
    auto ok = bt::traverse(positive, std::vector<int>{1, 2, 3});
    REQUIRE(ok == std::optional<std::vector<int>>{{2, 4, 6}});

    auto bad = bt::traverse(positive, std::vector<int>{1, -1, 3});
    REQUIRE(bad == std::optional<std::vector<int>>{});
}

TEST_CASE("traverse: preserves element order and count") {
    auto wrap = [](int x) { return std::optional<int>{x}; };
    auto result = bt::traverse(wrap, std::vector<int>{5, 4, 3, 2, 1});
    REQUIRE(result.has_value());
    REQUIRE(*result == std::vector<int>{5, 4, 3, 2, 1});
}

TEST_CASE("traverse: empty vector yields pure empty structure") {
    auto wrap = [](int x) { return std::optional<int>{x}; };
    auto result = bt::traverse(wrap, std::vector<int>{});
    REQUIRE(result == std::optional<std::vector<int>>{std::vector<int>{}});
}

TEST_CASE(
    "traverse: for_each on Identity infers applicative from return type") {
    const auto &t = bt::traversable_typeclass<bt::test::Identity<int>>;
    auto result = t.for_each(bt::test::Identity<int>{20},
                             [](int x) { return std::optional<int>{x + 1}; });
    REQUIRE(result == std::optional<bt::test::Identity<int>>{
                          bt::test::Identity<int>{21}});
}
