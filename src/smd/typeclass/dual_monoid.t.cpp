// src/smd/typeclass/dual_monoid.t.cpp                         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/typeclass/dual_monoid.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace bt = smd::typeclass;

TEST_CASE("dual_monoid: flips combine argument order") {
    using D = bt::DualMonoid<std::string>;
    auto combined =
        bt::monoid_v<D>.combine(D{std::string{"ab"}}, D{std::string{"cd"}});
    // Flipped: rhs.value + lhs.value == "cd" + "ab".
    REQUIRE(combined.value == "cdab");
}

TEST_CASE("dual_monoid: identity matches underlying identity") {
    using D = bt::DualMonoid<std::string>;
    REQUIRE(bt::monoid_v<D>.identity().value.empty());
}
