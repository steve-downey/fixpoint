// src/smd/fixpoint/one_shot.t.cpp                                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/one_shot.hpp>
#include <smd/fixpoint/one_shot.hpp> // Re-inclusion check

#include <catch2/catch_test_macros.hpp>

#include <concepts>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

using smd::fixpoint::one_shot;

static_assert(std::movable<one_shot<int(int)>>);
static_assert(!std::copyable<one_shot<int(int)>>);

TEST_CASE("one_shot - default-constructed is empty") {
    one_shot<int(int)> os;
    CHECK_FALSE(static_cast<bool>(os));
}

TEST_CASE("one_shot - holds a target after construction from a callable") {
    one_shot<int(int)> os{[](int x) { return x + 1; }};
    CHECK(static_cast<bool>(os));
}

TEST_CASE("one_shot - invoking returns the target's result") {
    one_shot<int(int)> os{[](int x) { return x * 2; }};
    int result = std::move(os)(21);
    CHECK(result == 42);
}

TEST_CASE("one_shot - invoking consumes the target") {
    one_shot<int(int)> os{[](int x) { return x; }};
    REQUIRE(static_cast<bool>(os));
    std::move(os)(0);
    CHECK_FALSE(static_cast<bool>(os));
}

TEST_CASE("one_shot - move construction transfers the target") {
    one_shot<int(int)> src{[](int x) { return x + 10; }};
    one_shot<int(int)> dst{std::move(src)};
    CHECK_FALSE(static_cast<bool>(src)); // NOLINT(bugprone-use-after-move)
    CHECK(static_cast<bool>(dst));
    CHECK(std::move(dst)(5) == 15);
}

TEST_CASE("one_shot - move assignment transfers the target and releases the "
          "destination's own") {
    one_shot<int(int)> src{[](int x) { return x + 100; }};
    one_shot<int(int)> dst{[](int x) { return x; }};
    dst = std::move(src);
    CHECK_FALSE(static_cast<bool>(src)); // NOLINT(bugprone-use-after-move)
    CHECK(static_cast<bool>(dst));
    CHECK(std::move(dst)(1) == 101);
}

TEST_CASE("one_shot - carries a move-only capture and multiple arguments") {
    auto owned = std::make_unique<int>(7);
    one_shot<int(int, int)> os{
        [owned = std::move(owned)](int a, int b) { return *owned + a + b; }};
    CHECK(std::move(os)(1, 2) == 10);
}

TEST_CASE("one_shot - real captured state (not stateless), matching the "
          "impure_node continuation shape") {
    struct request {
        int key;
    };
    struct response {
        std::string value;
    };

    int lookups = 3; // pretend "warm cache" state the continuation owns
    one_shot<response(request)> os{[lookups](request r) mutable -> response {
        ++lookups;
        return response{std::to_string(r.key) + "#" + std::to_string(lookups)};
    }};

    response r = std::move(os)(request{42});
    CHECK(r.value == "42#4");
}

TEST_CASE("one_shot - the target is destroyed (and the box emptied) even "
          "when invoking it throws") {
    one_shot<int(int)> os{[](int) -> int { throw std::runtime_error{"boom"}; }};
    CHECK_THROWS_AS(std::move(os)(0), std::runtime_error);
    CHECK_FALSE(static_cast<bool>(os));
}
