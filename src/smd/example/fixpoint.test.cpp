// example/fixpoint.test.cpp                                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/example/fixpoint.hpp>

#include <smd/example/fixpoint.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

// 03013d1f-bcc1-4d3e-9701-3ed1a15c6370
TEST_CASE("fixpoint returns Steve", "fixpoint") {
    REQUIRE(example::fixpoint() == "Steve");
}
// 03013d1f-bcc1-4d3e-9701-3ed1a15c6370 end
