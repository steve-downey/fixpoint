// src/smd/typeclass/fold.t.cpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/typeclass/fold.hpp>

#include "test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace bt = smd::typeclass;

TEST_CASE("fold: length and to_vector over Sequence") {
    const auto &f = bt::foldable_typeclass<bt::test::Sequence<int>>;
    bt::test::Sequence<int> seq{{10, 20, 30}};
    REQUIRE(f.length(seq) == 3);
    REQUIRE(f.to_vector(seq) == std::vector<int>{10, 20, 30});
}

TEST_CASE("fold: fold_left and fold_right accumulate") {
    const auto &f = bt::foldable_typeclass<bt::test::Sequence<int>>;
    bt::test::Sequence<int> seq{{1, 2, 3, 4}};
    REQUIRE(f.fold_left(seq, 0, [](int acc, int x) { return acc + x; }) == 10);
    REQUIRE(f.fold_right(seq, std::string{}, [](int x, std::string acc) {
        return acc + std::to_string(x);
    }) == "4321");
}

TEST_CASE("fold: any_of, all_of, find_first") {
    const auto &f = bt::foldable_typeclass<bt::test::Sequence<int>>;
    bt::test::Sequence<int> seq{{2, 4, 6, 7}};
    REQUIRE(f.any_of(seq, [](int x) { return x % 2 == 1; }));
    REQUIRE_FALSE(f.all_of(seq, [](int x) { return x % 2 == 0; }));
    REQUIRE(f.find_first(seq, [](int x) { return x > 5; }) == 6);
}
