// src/smd/typeclass/functor.t.cpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/typeclass/functor.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <vector>

namespace bt = smd::typeclass;

TEST_CASE("functor: fmap over optional") {
    const auto &f = bt::functor_typeclass<std::optional<int>>;
    REQUIRE(f.fmap([](int x) { return x + 1; }, std::optional<int>{41}) ==
            std::optional<int>{42});
    REQUIRE(f.fmap([](int x) { return x + 1; }, std::optional<int>{}) ==
            std::optional<int>{});
}

TEST_CASE("functor: fmap over vector") {
    const auto &f = bt::functor_typeclass<std::vector<int>>;
    REQUIRE(f.fmap([](int x) { return x * 2; }, std::vector<int>{1, 2, 3}) ==
            std::vector<int>{2, 4, 6});
}

TEST_CASE("functor: replace is derived from fmap") {
    const auto &f = bt::functor_typeclass<std::vector<int>>;
    REQUIRE(f.replace(std::vector<int>{1, 2, 3}, 0) ==
            std::vector<int>{0, 0, 0});
}
