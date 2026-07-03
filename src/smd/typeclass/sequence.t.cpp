// src/smd/typeclass/sequence.t.cpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/typeclass/sequence.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <vector>

namespace bt = smd::typeclass;

TEST_CASE("sequence: vector foldable length and to_vector") {
    const auto &f = bt::foldable_typeclass<std::vector<int>>;
    std::vector<int> xs{4, 5, 6};
    REQUIRE(f.length(xs) == 3);
    REQUIRE(f.to_vector(xs) == std::vector<int>{4, 5, 6});
    REQUIRE(f.fold_left(xs, 0, [](int a, int x) { return a + x; }) == 15);
}

TEST_CASE("sequence: vector traversable primitive sequences effects") {
    const auto &t = bt::traversable_typeclass<std::vector<int>>;
    const auto &app = bt::applicative_typeclass<std::optional<int>>;
    auto result = t.traverse(
        app, [](int x) { return std::optional<int>{x + 1}; },
        std::vector<int>{1, 2, 3});
    REQUIRE(result == std::optional<std::vector<int>>{{2, 3, 4}});
}
