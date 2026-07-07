// src/smd/typeclass/either.t.cpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/typeclass/either.hpp>
#include <smd/typeclass/either.hpp> // Re-inclusion check

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/fix.hpp>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>
#include <variant>

namespace bt = smd::typeclass;

TEST_CASE("either - HeaderIsIdempotent") { REQUIRE(true); }

// -- construction / observers, both sides --

TEST_CASE("either: construction and observers, Left side") {
    auto e = bt::make_left<int>(std::string{"stop"});
    REQUIRE(bt::is_left(e));
    REQUIRE(bt::left(e) == "stop");
}

TEST_CASE("either: construction and observers, Right side") {
    auto e = bt::make_right<std::string>(42);
    REQUIRE_FALSE(bt::is_left(e));
    REQUIRE(bt::right(e) == 42);
}

// -- either<int, int>: same-type sides construct/compare unambiguously --
// This is the exact failure mode that ruled out std::expected<T, T> (D4).

TEST_CASE("either<int, int>: Left and Right with equal payload are distinct") {
    auto l = bt::make_left<int>(5);
    auto r = bt::make_right<int>(5);
    static_assert(std::is_same_v<decltype(l), bt::either<int, int>>);
    static_assert(std::is_same_v<decltype(r), bt::either<int, int>>);

    REQUIRE(bt::is_left(l));
    REQUIRE_FALSE(bt::is_left(r));
    REQUIRE(bt::left(l) == 5);
    REQUIRE(bt::right(r) == 5);
    REQUIRE(l != r);
    REQUIRE(l == bt::make_left<int>(5));
    REQUIRE(r == bt::make_right<int>(5));
}

// -- functor: maps Right only --

TEST_CASE("either functor: fmap maps Right, leaves Left untouched") {
    const auto &f = bt::functor_typeclass<bt::either<int, int>>;

    auto right_side = bt::make_right<int>(10);
    auto mapped_right = f.fmap([](int x) { return x + 1; }, right_side);
    REQUIRE_FALSE(bt::is_left(mapped_right));
    REQUIRE(bt::right(mapped_right) == 11);

    auto left_side = bt::make_left<int>(10);
    auto mapped_left = f.fmap([](int x) { return x + 1; }, left_side);
    REQUIRE(bt::is_left(mapped_left));
    REQUIRE(bt::left(mapped_left) == 10);
}

// -- monad: pure = make_right, bind propagates Left --

TEST_CASE("either monad: bind propagates Left untouched") {
    const auto &m = bt::monad_typeclass<bt::either<int, int>>;
    auto half_if_even = [](int x) {
        return x % 2 == 0 ? bt::make_right<int>(x / 2) : bt::make_left<int>(-1);
    };

    auto ok = m.bind(bt::make_right<int>(8), half_if_even);
    REQUIRE_FALSE(bt::is_left(ok));
    REQUIRE(bt::right(ok) == 4);

    auto bad = m.bind(bt::make_right<int>(7), half_if_even);
    REQUIRE(bt::is_left(bad));
    REQUIRE(bt::left(bad) == -1);

    auto stopped = m.bind(bt::make_left<int>(99), half_if_even);
    REQUIRE(bt::is_left(stopped));
    REQUIRE(bt::left(stopped) == 99);
}

TEST_CASE("either monad: pure wraps as Right") {
    const auto &m = bt::monad_typeclass<bt::either<int, int>>;
    auto pured = m.pure(3);
    REQUIRE_FALSE(bt::is_left(pured));
    REQUIRE(bt::right(pured) == 3);
}

// -- map_left: maps Left only --

TEST_CASE("either: map_left maps Left, leaves Right untouched") {
    auto left_side = bt::make_left<int>(3);
    auto mapped = bt::map_left([](int x) { return x * 10; }, left_side);
    REQUIRE(bt::is_left(mapped));
    REQUIRE(bt::left(mapped) == 30);

    auto right_side = bt::make_right<int>(3);
    auto mapped_right = bt::map_left([](int x) { return x * 10; }, right_side);
    REQUIRE_FALSE(bt::is_left(mapped_right));
    REQUIRE(bt::right(mapped_right) == 3);
}

// -- match / fanin: the copairing eliminator (D11) --

TEST_CASE("either: match applies the matching branch") {
    auto on_left = [](int x) { return x - 1; };
    auto on_right = [](int x) { return x + 1; };

    REQUIRE(bt::match(bt::make_left<int>(10), on_left, on_right) == 9);
    REQUIRE(bt::match(bt::make_right<int>(10), on_left, on_right) == 11);
}

TEST_CASE("either: fanin returns the matcher as a callable") {
    auto on_left = [](int x) { return x - 1; };
    auto on_right = [](int x) { return x + 1; };
    auto matcher = bt::fanin(on_left, on_right);

    REQUIRE(matcher(bt::make_left<int>(10)) == 9);
    REQUIRE(matcher(bt::make_right<int>(10)) == 11);
    // fanin(f, g)(e) == match(e, f, g)
    REQUIRE(matcher(bt::make_left<int>(4)) ==
            bt::match(bt::make_left<int>(4), on_left, on_right));
}

// -- reference sides (P2988 model) --

TEST_CASE("either references: binds an lvalue; left()/right() alias the "
          "original object") {
    int x = 42;
    auto e = bt::make_left<int, int &>(x);
    REQUIRE(&bt::left(e) == &x);
    REQUIRE(bt::left(e) == 42);

    int y = 7;
    auto r = bt::make_right<int, int &>(y);
    REQUIRE(&bt::right(r) == &y);
}

TEST_CASE("either references: assignment rebinds, does not assign through") {
    int x = 1;
    int y = 2;
    auto e1 = bt::make_left<int, int &>(x);
    auto e2 = bt::make_left<int, int &>(y);

    e1 = e2;

    REQUIRE(&bt::left(e1) == &y); // e1 now aliases y ...
    REQUIRE(x == 1);              // ... and x was never written through.
    REQUIRE(bt::left(e1) == 2);
}

TEST_CASE("either references: shallow const — a const wrapper still yields "
          "a mutable referent") {
    int x = 5;
    const auto e = bt::make_left<int, int &>(x);
    // left(e) returns int& (not const int&) even though e is const: the
    // either<int&, int>'s own constness only ever protects the pointer
    // inside Left<int&>, never the pointee.
    static_assert(std::is_same_v<decltype(bt::left(e)), int &>);
    bt::left(e) = 99;
    REQUIRE(x == 99);
}

TEST_CASE("either references: no-temporary constraints on the wrappers") {
    static_assert(!std::is_constructible_v<bt::Left<const int &>, int &&>);
    static_assert(std::is_constructible_v<bt::Left<const int &>, int &>);
    static_assert(!std::is_constructible_v<bt::Right<const int &>, int &&>);
    static_assert(std::is_constructible_v<bt::Right<const int &>, int &>);
    REQUIRE(true);
}

TEST_CASE("either references: no-temporary constraints on make_left/"
          "make_right themselves") {
    static_assert(
        !std::is_invocable_v<decltype(bt::make_left<int, int &>), int>);
    static_assert(
        std::is_invocable_v<decltype(bt::make_left<int, int &>), int &>);
    static_assert(
        !std::is_invocable_v<decltype(bt::make_right<int, int &>), int>);
    static_assert(
        std::is_invocable_v<decltype(bt::make_right<int, int &>), int &>);
    REQUIRE(true);
}

namespace {

struct Zero {};

template <class A>
struct Succ {
    smd::fixpoint::Box<A> pred;
};

template <class A>
using NatF = std::variant<Zero, Succ<A>>;

using Nat = smd::fixpoint::Fix<NatF>;

} // namespace

TEST_CASE("either<const Fix&, Seed>-shaped smoke: zero-copy graft (S04)") {
    // apo's coalgebra returns F<either<Fix<F>, Seed>> — Left embeds an
    // already-finished subtree as-is. This smoke proves the reference-side
    // Left<const Nat&> binds the existing tree node with no copy: the
    // referent's address survives into the either.
    auto zero = smd::fixpoint::wrap_fix<NatF>(NatF<Nat>{Zero{}});
    auto graft = bt::make_left<int, const Nat &>(zero);
    REQUIRE(bt::is_left(graft));
    REQUIRE(&bt::left(graft) == &zero);

    auto seed_side = bt::make_right<const Nat &>(3);
    REQUIRE_FALSE(bt::is_left(seed_side));
    REQUIRE(bt::right(seed_side) == 3);
}

// -- constexpr --

namespace {
constexpr auto either_constexpr_smoke() -> bool {
    auto l = bt::make_left<int>(1);
    auto r = bt::make_right<int>(2);
    bool ok = bt::is_left(l) && !bt::is_left(r) && bt::left(l) == 1 &&
              bt::right(r) == 2;

    const auto &f = bt::functor_typeclass<bt::either<int, int>>;
    auto mapped = f.fmap([](int x) { return x + 1; }, r);
    ok = ok && bt::right(mapped) == 3;

    ok = ok &&
         bt::match(l, [](int x) { return x; }, [](int x) { return -x; }) == 1;
    return ok;
}
} // namespace

static_assert(either_constexpr_smoke());
