// src/smd/typeclass/detail/typeclass_base.t.cpp               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/typeclass/detail/typeclass_base.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <type_traits>
#include <vector>

namespace bt = smd::typeclass;

TEST_CASE("typeclass_base: applicative_value_t extracts element type") {
    STATIC_REQUIRE(
        std::is_same_v<bt::applicative_value_t<std::optional<int>>, int>);
    STATIC_REQUIRE(
        std::is_same_v<bt::applicative_value_t<std::vector<double>>, double>);
}
