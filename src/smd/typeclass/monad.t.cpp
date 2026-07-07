// src/smd/typeclass/monad.t.cpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/typeclass/identity.hpp>
#include <smd/typeclass/monad.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>

namespace bt = smd::typeclass;

TEST_CASE("monad: optional bind short-circuits on empty") {
    auto half_if_even = [](int x) {
        return x % 2 == 0 ? std::optional<int>{x / 2} : std::optional<int>{};
    };
    REQUIRE(bt::mbind(std::optional<int>{8}, half_if_even) ==
            std::optional<int>{4});
    REQUIRE(bt::mbind(std::optional<int>{7}, half_if_even) ==
            std::optional<int>{});
    REQUIRE(bt::mbind(std::optional<int>{}, half_if_even) ==
            std::optional<int>{});
}

TEST_CASE("monad: join flattens nested optional") {
    std::optional<std::optional<int>> nested{std::optional<int>{5}};
    REQUIRE(bt::join(nested) == std::optional<int>{5});
}

TEST_CASE("monad: apply synthesized from bind and pure") {
    const auto &m = bt::monad_typeclass<std::optional<int>>;
    auto mf = std::optional{[](int x) { return x + 100; }};
    REQUIRE(m.apply(mf, std::optional<int>{1}) == std::optional<int>{101});
}

// Spike: structural inheritance chain (Monad : Applicative : Functor).
// The monad object should now expose the full Functor + Applicative
// surface, defined exactly once, and it should agree with the hand-written
// per-concept instances.

TEST_CASE("monad: structural surface matches Functor/Applicative "
          "instances -- optional") {
    using T = std::optional<int>;
    const auto &mon = bt::monad_typeclass<T>;
    const auto &fun = bt::functor_typeclass<T>;
    const auto &app = bt::applicative_typeclass<T>;

    auto square = [](int x) { return x * x; };
    T x{6};

    REQUIRE(mon.fmap(square, x) == fun.fmap(square, x));

    auto mf = std::optional{[](int v) { return v + 1; }};
    T mx{41};
    REQUIRE(mon.apply(mf, mx) == app.apply(mf, mx));

    REQUIRE(mon.map(square, x) == std::optional<int>{36});
    REQUIRE(mon.ap(mf, mx) == std::optional<int>{42});
}

TEST_CASE("monad: structural surface matches Functor/Applicative "
          "instances -- Identity") {
    using T = bt::Identity<int>;
    const auto &mon = bt::monad_typeclass<T>;
    const auto &fun = bt::functor_typeclass<T>;
    const auto &app = bt::applicative_typeclass<T>;

    auto square = [](int x) { return x * x; };
    T x{6};

    REQUIRE(mon.fmap(square, x) == fun.fmap(square, x));

    auto mf = app.pure([](int v) { return v + 1; });
    T mx{41};
    REQUIRE(mon.apply(mf, mx) == app.apply(mf, mx));

    REQUIRE(mon.map(square, x) == T{36});
    REQUIRE(mon.ap(mf, mx) == T{42});
}
