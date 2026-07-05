// src/smd/typeclass/comonad.t.cpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/typeclass/comonad.hpp>
#include <smd/typeclass/comonad.hpp> // Re-inclusion check

#include <smd/typeclass/identity.hpp>
#include <smd/typeclass/pair.hpp>

#include <catch2/catch_test_macros.hpp>

#include <utility>

namespace bt = smd::typeclass;

TEST_CASE("comonad - HeaderIsIdempotent") { REQUIRE(true); }

// The Comonad<Impl> CRTP base is exercised through its two S03 instances
// (Identity, the env comonad on std::pair). Both must satisfy the same
// generic law spot-checks through the *same* base-class machinery — this is
// the proof point that `extend` really is derived once, in comonad.hpp, not
// duplicated per instance.

TEST_CASE("comonad: extend is derived as fmap . duplicate — Identity") {
    const auto &c = bt::comonad_typeclass<bt::Identity<int>>;
    bt::Identity<int> w{9};

    auto via_extend = c.extend(
        [](const bt::Identity<int> &inner) { return inner.value + 1; }, w);
    auto via_fmap_duplicate = c.fmap(
        [](const bt::Identity<int> &inner) { return inner.value + 1; },
        c.duplicate(w));
    REQUIRE(via_extend == via_fmap_duplicate);
}

TEST_CASE("comonad: extend is derived as fmap . duplicate — pair env") {
    const auto &c = bt::comonad_typeclass<std::pair<int, int>>;
    std::pair<int, int> w{100, 9};

    auto via_extend = c.extend(
        [](const std::pair<int, int> &inner) { return inner.second + 1; }, w);
    auto via_fmap_duplicate = c.fmap(
        [](const std::pair<int, int> &inner) { return inner.second + 1; },
        c.duplicate(w));
    REQUIRE(via_extend == via_fmap_duplicate);
}

// -- the four comonad laws, spot-checked on both instances --

TEST_CASE("comonad laws: extract . duplicate == id — both instances") {
    // extract(duplicate(w)) unwraps the doubled structure exactly once,
    // recovering w itself (not w's own payload) — extract's return type at
    // the doubled layer is the un-doubled container, not its element.
    const auto &ci = bt::comonad_typeclass<bt::Identity<int>>;
    bt::Identity<int> wi{4};
    REQUIRE(ci.extract(ci.duplicate(wi)) == wi);

    const auto &cp = bt::comonad_typeclass<std::pair<int, int>>;
    std::pair<int, int> wp{1, 4};
    REQUIRE(cp.extract(cp.duplicate(wp)) == wp);
}

TEST_CASE("comonad laws: fmap(extract) . duplicate == id — both instances") {
    const auto &ci = bt::comonad_typeclass<bt::Identity<int>>;
    bt::Identity<int> wi{4};
    REQUIRE(ci.fmap([&ci](const bt::Identity<int> &inner) {
        return ci.extract(inner);
    }, ci.duplicate(wi)) == wi);

    const auto &cp = bt::comonad_typeclass<std::pair<int, int>>;
    std::pair<int, int> wp{1, 4};
    REQUIRE(cp.fmap([&cp](const std::pair<int, int> &inner) {
        return cp.extract(inner);
    }, cp.duplicate(wp)) == wp);
}

TEST_CASE("comonad laws: duplicate . duplicate associativity — Identity") {
    const auto &c = bt::comonad_typeclass<bt::Identity<int>>;
    bt::Identity<int> w{4};
    auto lhs = c.fmap(
        [&c](const bt::Identity<int> &inner) { return c.duplicate(inner); },
        c.duplicate(w));
    auto rhs = c.duplicate(c.duplicate(w));
    REQUIRE(lhs == rhs);
}

// -- constexpr --

namespace {
constexpr auto comonad_generic_laws_hold() -> bool {
    const auto &ci = bt::comonad_typeclass<bt::Identity<int>>;
    bt::Identity<int> wi{2};
    bool identity_ok = ci.extend([&ci](const bt::Identity<int> &inner) {
        return ci.extract(inner);
    }, wi) == wi;

    const auto &cp = bt::comonad_typeclass<std::pair<int, int>>;
    std::pair<int, int> wp{0, 2};
    bool pair_ok = cp.extend([&cp](const std::pair<int, int> &inner) {
        return cp.extract(inner);
    }, wp) == wp;

    return identity_ok && pair_ok;
}
} // namespace

static_assert(comonad_generic_laws_hold());
