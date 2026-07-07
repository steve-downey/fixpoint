// src/smd/typeclass/identity.t.cpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/typeclass/identity.hpp>
#include <smd/typeclass/identity.hpp> // Re-inclusion check

#include "test_support.hpp"

#include <catch2/catch_test_macros.hpp>

namespace bt = smd::typeclass;

TEST_CASE("identity - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("identity: functor fmap wraps the transformed value") {
    const auto &f = bt::functor_typeclass<bt::Identity<int>>;
    REQUIRE(f.fmap([](int x) { return x + 1; }, bt::Identity<int>{41}) ==
            bt::Identity<int>{42});
}

TEST_CASE("identity: applicative pure/apply round-trip") {
    const auto &a = bt::applicative_typeclass<bt::Identity<int>>;
    auto wrapped_fn = a.pure([](int x) { return x * 2; });
    REQUIRE(a.apply(wrapped_fn, bt::Identity<int>{21}) ==
            bt::Identity<int>{42});
    REQUIRE(bt::test::check_applicative_identity_law<bt::Identity<int>>(
        bt::Identity<int>{7}));
    REQUIRE(bt::test::check_applicative_homomorphism_law<bt::Identity<int>>(
        [](int x) { return x + 3; }, 9));
}

TEST_CASE("identity: monad bind invokes directly on .value") {
    const auto &m = bt::monad_typeclass<bt::Identity<int>>;
    auto to_string_len = [](int x) { return bt::Identity<int>{x + 1}; };
    REQUIRE(m.bind(bt::Identity<int>{9}, to_string_len) ==
            bt::Identity<int>{10});
    REQUIRE(bt::mbind(bt::Identity<int>{9}, to_string_len) ==
            bt::Identity<int>{10});
}

TEST_CASE("identity: comonad laws hold") {
    const auto &c = bt::comonad_typeclass<bt::Identity<int>>;
    bt::Identity<int> w{5};

    // extract . duplicate == id (extract(duplicate(w)) unwraps the outer
    // Identity<Identity<int>> exactly once, recovering the inner
    // Identity<int> — i.e. w itself, not w.value).
    REQUIRE(c.extract(c.duplicate(w)) == w);

    // fmap extract . duplicate == id
    auto fmap_extract_duplicate = c.fmap(
        [&c](const bt::Identity<int> &inner) { return c.extract(inner); },
        c.duplicate(w));
    REQUIRE(fmap_extract_duplicate == w);

    // duplicate . duplicate associativity: fmap duplicate . duplicate ==
    // duplicate . duplicate
    auto lhs = c.fmap(
        [&c](const bt::Identity<int> &inner) { return c.duplicate(inner); },
        c.duplicate(w));
    auto rhs = c.duplicate(c.duplicate(w));
    REQUIRE(lhs == rhs);

    // extend derived operation matches fmap-after-duplicate directly.
    REQUIRE(
        c.extend(
            [&c](const bt::Identity<int> &inner) { return c.extract(inner); },
            w) == w);
}

static_assert(bt::Identity<int>{1} == bt::Identity<int>{1});
static_assert(bt::Identity<int>{1} != bt::Identity<int>{2});

namespace {
constexpr auto identity_comonad_laws_hold() -> bool {
    const auto &c = bt::comonad_typeclass<bt::Identity<int>>;
    bt::Identity<int> w{5};
    bool extract_duplicate_is_id = c.extract(c.duplicate(w)) == w;
    bool extend_extract_is_id =
        c.extend(
            [&c](const bt::Identity<int> &inner) { return c.extract(inner); },
            w) == w;
    return extract_duplicate_is_id && extend_extract_is_id;
}
} // namespace

static_assert(identity_comonad_laws_hold());
