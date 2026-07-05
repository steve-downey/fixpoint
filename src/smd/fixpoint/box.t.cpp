// src/smd/fixpoint/box.t.cpp                                         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/box.hpp> // Re-inclusion check

#include <catch2/catch_test_macros.hpp>

#include <string>

using smd::fixpoint::Box;
using smd::fixpoint::make_box;

static_assert(std::is_default_constructible_v<Box<int>>);

TEST_CASE("Box - MakeBoxInt") {
    auto b = make_box<int>(42);
    CHECK(*b == 42);
}

TEST_CASE("Box - MakeBoxString") {
    auto b = make_box<std::string>("hello");
    CHECK(*b == "hello");
}

TEST_CASE("Box - DeepCopyOnCopy") {
    auto b1 = make_box<int>(7);
    Box<int> b2 = b1;
    CHECK(*b1 == *b2);
    *b2 = 99;
    CHECK(*b1 == 7);
    CHECK(*b2 == 99);
}
