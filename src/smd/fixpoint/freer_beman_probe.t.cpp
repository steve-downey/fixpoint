// src/smd/fixpoint/freer_beman_probe.t.cpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// S07d: build-machinery probe for beman::execution / beman::task. Proves
// both are fetchable and linkable in this repo's dual dependency-
// provisioning setup (see cmake/beman-deps.cmake). Not part of the Freer
// design under test yet -- S07 is the real interpreter work against these
// deps.

#include <beman/execution/execution.hpp>
#include <beman/task/task.hpp>

#include <catch2/catch_test_macros.hpp>

namespace ex = beman::execution;

namespace {

auto make_answer_task() -> ex::task<int> { co_return 42; }

} // namespace

TEST_CASE("freer_beman_probe - sync_wait(just(42))") {
    auto result = ex::sync_wait(ex::just(42));
    REQUIRE(result.has_value());
    auto [value] = result.value();
    REQUIRE(value == 42);
}

TEST_CASE("freer_beman_probe - task co_return smoke") {
    auto result = ex::sync_wait(make_answer_task());
    REQUIRE(result.has_value());
    auto [value] = result.value();
    REQUIRE(value == 42);
}
