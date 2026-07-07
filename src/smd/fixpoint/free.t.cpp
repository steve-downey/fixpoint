// src/smd/fixpoint/free.t.cpp                                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/free.hpp> // Re-inclusion check

#include <smd/concrete/functors.hpp>
#include <smd/fixpoint/box.hpp>

#include <catch2/catch_test_macros.hpp>

#include <variant>

using smd::concrete::Cons;
using smd::concrete::IntListF;
using smd::fixpoint::Free;
using smd::fixpoint::is_pure;
using smd::fixpoint::make_box;
using smd::fixpoint::pure_free;
using smd::fixpoint::roll_free;

TEST_CASE("free - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

using IntFree = Free<IntListF, int>;

// Builds an n-deep Free chunk: `count` Cons layers each holding `value`,
// then a Pure leaf holding `seed`. This is the step file's own suggested
// helper for building multi-layer chunks ergonomically in tests — kept
// test-local (not shipped in free.hpp), per its Pitfalls section.
auto make_run(int value, int count, int seed) -> IntFree {
    if (count <= 0) {
        return pure_free<IntListF>(seed);
    }
    return roll_free<IntListF>(IntListF<IntFree>{Cons<int, IntFree>{
        value, make_box<IntFree>(make_run(value, count - 1, seed))}});
}

} // namespace

// ---------------------------------------------------------------------
// Shape smoke: pure_free/roll_free/is_pure directly.
// ---------------------------------------------------------------------

TEST_CASE("free: pure_free/roll_free/is_pure expose Pure vs Roll") {
    IntFree pure_leaf = pure_free<IntListF>(5);
    CHECK(is_pure(pure_leaf));
    CHECK(std::get<int>(pure_leaf.node) == 5);

    IntFree chunk = make_run(9, 2, 4);
    CHECK_FALSE(is_pure(chunk));
    CHECK(std::holds_alternative<IntListF<IntFree>>(chunk.node));
}

TEST_CASE("free: equality is structural") {
    CHECK(make_run(1, 2, 3) == make_run(1, 2, 3));
    CHECK_FALSE(make_run(1, 2, 3) == make_run(1, 2, 4));
    CHECK_FALSE(make_run(1, 2, 3) == make_run(1, 1, 3));
}

// ---------------------------------------------------------------------
// Functor instance: maps the Pure value; recurses through Roll layers
// (design §5.4/step S08 — the Cons heads carry IntListF's own payload
// type, untouched; only the terminal seed is `fn`'s target).
// ---------------------------------------------------------------------

TEST_CASE("free functor: fmap maps the Pure value, recurses through Roll") {
    auto &functor = smd::typeclass::functor_typeclass<IntFree>;

    IntFree pure_leaf = pure_free<IntListF>(4);
    IntFree mapped_pure = functor.fmap([](int x) { return x + 10; }, pure_leaf);
    CHECK(is_pure(mapped_pure));
    CHECK(std::get<int>(mapped_pure.node) == 14);

    IntFree chunk = make_run(9, 2, 4);
    IntFree mapped_chunk = functor.fmap([](int x) { return x + 10; }, chunk);
    CHECK(mapped_chunk == make_run(9, 2, 14));
}

// ---------------------------------------------------------------------
// Monad laws by example (design §9): left/right identity, associativity
// spot-check, on Free<IntListF, int>.
// ---------------------------------------------------------------------

TEST_CASE("free monad law: left identity - bind(pure(a), k) == k(a)") {
    const auto &monad = smd::typeclass::monad_typeclass<IntFree>;
    auto k = [](int x) -> IntFree { return make_run(x, 2, x + 1); };
    for (int a = 0; a <= 5; ++a) {
        CHECK(monad.bind(monad.pure(a), k) == k(a));
    }
}

TEST_CASE("free monad law: right identity - bind(m, pure) == m") {
    const auto &monad = smd::typeclass::monad_typeclass<IntFree>;
    auto pure_k = [](int x) -> IntFree { return monad.pure(x); };
    for (int count = 0; count <= 3; ++count) {
        IntFree m = make_run(7, count, count * 10);
        CHECK(monad.bind(m, pure_k) == m);
    }
}

TEST_CASE("free monad law: associativity spot-check") {
    const auto &monad = smd::typeclass::monad_typeclass<IntFree>;
    auto k = [](int x) -> IntFree { return make_run(x, 1, x + 1); };
    auto h = [](int x) -> IntFree { return make_run(x * 2, 1, x + 2); };
    for (int count = 0; count <= 3; ++count) {
        IntFree m = make_run(3, count, count);
        IntFree lhs = monad.bind(monad.bind(m, k), h);
        IntFree rhs =
            monad.bind(m, [&k, &h](int x) { return monad.bind(k(x), h); });
        CHECK(lhs == rhs);
    }
}

// ---------------------------------------------------------------------
// Behavior: bind sequences through a Roll chunk (design §5.4's own
// worked shape: Roll layer -> roll_free(layer_fmap(bind-with-k, layer))).
// ---------------------------------------------------------------------

TEST_CASE("free monad: bind sequences through a Roll chunk") {
    const auto &monad = smd::typeclass::monad_typeclass<IntFree>;
    IntFree m = make_run(1, 1, 2); // Roll(Cons(1, Pure(2)))
    auto k = [](int seed) -> IntFree { return make_run(seed, 2, seed + 100); };

    IntFree result = monad.bind(m, k);
    // bind recurses through the single Cons layer and applies k at the
    // Pure(2) leaf: Roll(Cons(1, k(2))) == Roll(Cons(1, make_run(2,2,102))).
    IntFree expected = roll_free<IntListF>(IntListF<IntFree>{
        Cons<int, IntFree>{1, make_box<IntFree>(make_run(2, 2, 102))}});
    CHECK(result == expected);
}

// ---------------------------------------------------------------------
// constexpr (design D10).
// ---------------------------------------------------------------------

namespace {

constexpr auto free_constexpr_smoke() -> bool {
    const auto &monad = smd::typeclass::monad_typeclass<IntFree>;
    auto k = [](int x) -> IntFree { return pure_free<IntListF>(x + 1); };
    IntFree m = monad.pure(5);
    IntFree bound = monad.bind(m, k);
    return is_pure(bound) && std::get<int>(bound.node) == 6;
}

} // namespace

static_assert(free_constexpr_smoke());
