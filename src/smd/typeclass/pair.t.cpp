// src/smd/typeclass/pair.t.cpp                                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/typeclass/pair.hpp>
#include <smd/typeclass/pair.hpp> // Re-inclusion check

#include <smd/typeclass/either.hpp>

#include <catch2/catch_test_macros.hpp>

#include <utility>

namespace bt = smd::typeclass;

TEST_CASE("pair - HeaderIsIdempotent") { REQUIRE(true); }

// -- functor: maps .second only --

TEST_CASE("pair functor: fmap maps .second, leaves .first untouched") {
    const auto &f = bt::functor_typeclass<std::pair<std::string, int>>;
    auto mapped =
        f.fmap([](int x) { return x + 1; }, std::pair<std::string, int>{"env", 41});
    REQUIRE(mapped.first == "env");
    REQUIRE(mapped.second == 42);
}

// -- D11 dual vocabulary: map_first / fanout --

TEST_CASE("pair: map_first maps .first, leaves .second untouched") {
    auto mapped = bt::map_first([](int x) { return x * 10; },
                               std::pair<int, std::string>{3, "value"});
    REQUIRE(mapped.first == 30);
    REQUIRE(mapped.second == "value");
}

TEST_CASE("pair: fanout <f, g> recovers f/g via .first/.second") {
    auto f = [](int x) { return x + 1; };
    auto g = [](int x) { return x * 2; };
    auto paired = bt::fanout(f, g);

    auto result = paired(10);
    REQUIRE(result.first == f(10));
    REQUIRE(result.second == g(10));
}

// -- env comonad --

TEST_CASE("pair env comonad: extract reads .second") {
    const auto &c = bt::comonad_typeclass<std::pair<std::string, int>>;
    std::pair<std::string, int> p{"env", 7};
    REQUIRE(c.extract(p) == 7);
}

TEST_CASE("pair env comonad: duplicate nests the whole pair into .second") {
    const auto &c = bt::comonad_typeclass<std::pair<std::string, int>>;
    std::pair<std::string, int> p{"env", 7};
    auto dup = c.duplicate(p);
    REQUIRE(dup.first == "env");
    REQUIRE(dup.second == p);
}

TEST_CASE("pair env comonad: fmap maps .second, matches functor instance") {
    const auto &c = bt::comonad_typeclass<std::pair<std::string, int>>;
    auto mapped = c.fmap([](int x) { return x + 1; },
                         std::pair<std::string, int>{"env", 41});
    REQUIRE(mapped.first == "env");
    REQUIRE(mapped.second == 42);
}

TEST_CASE("pair env comonad: extend(extract, w) == w (comonad law)") {
    const auto &c = bt::comonad_typeclass<std::pair<std::string, int>>;
    std::pair<std::string, int> w{"env", 13};
    auto result = c.extend(
        [&c](const std::pair<std::string, int> &inner) {
            return c.extract(inner);
        },
        w);
    REQUIRE(result == w);
}

// -- duality smoke: pair/either introduction+elimination round-trip (D11) --

TEST_CASE("pair/either duality: fanout then fanin recovers the originals") {
    auto f = [](int x) { return x + 1; };
    auto g = [](int x) { return x * 2; };

    // fanout(f, g) then .first/.second recovers f/g.
    auto paired = bt::fanout(f, g)(5);
    REQUIRE(paired.first == f(5));
    REQUIRE(paired.second == g(5));

    // fanin(f, g) after make_left/make_right recovers f/g.
    auto matcher = bt::fanin(f, g);
    REQUIRE(matcher(bt::make_left<int>(5)) == f(5));
    REQUIRE(matcher(bt::make_right<int>(5)) == g(5));
}

// -- constexpr --

namespace {
constexpr auto pair_constexpr_smoke() -> bool {
    const auto &c = bt::comonad_typeclass<std::pair<int, int>>;
    std::pair<int, int> w{1, 2};
    bool extract_ok = c.extract(w) == 2;
    bool duplicate_ok = c.duplicate(w) == std::pair<int, std::pair<int, int>>{1, w};
    bool extend_law_ok =
        c.extend(
            [&c](const std::pair<int, int> &inner) { return c.extract(inner); },
            w) == w;

    auto mapped = bt::map_first([](int x) { return x + 1; }, w);
    bool map_first_ok = mapped.first == 2 && mapped.second == 2;

    auto paired = bt::fanout([](int x) { return x + 1; },
                            [](int x) { return x * 2; })(10);
    bool fanout_ok = paired.first == 11 && paired.second == 20;

    return extract_ok && duplicate_ok && extend_law_ok && map_first_ok &&
          fanout_ok;
}
} // namespace

static_assert(pair_constexpr_smoke());
