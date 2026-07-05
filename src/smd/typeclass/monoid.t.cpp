// src/smd/typeclass/monoid.t.cpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/typeclass/monoid.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace bt = smd::typeclass;

TEST_CASE("monoid: int additive identity and combine") {
    REQUIRE(bt::monoid_identity<int>() == 0);
    REQUIRE(bt::monoid_combine(3, 4) == 7);
}

TEST_CASE("monoid: string concatenation") {
    REQUIRE(bt::monoid_identity<std::string>().empty());
    REQUIRE(bt::monoid_combine(std::string{"ab"}, std::string{"cd"}) == "abcd");
}

TEST_CASE("monoid: vector concatenation") {
    std::vector<int> lhs{1, 2};
    std::vector<int> rhs{3, 4};
    REQUIRE(bt::monoid_combine(lhs, rhs) == std::vector<int>{1, 2, 3, 4});
    REQUIRE(bt::monoid_identity<std::vector<int>>().empty());
}

TEST_CASE("monoid: Count combines by addition") {
    auto combined = bt::monoid_combine(bt::Count{2}, bt::Count{5});
    REQUIRE(combined == bt::Count{7});
}
