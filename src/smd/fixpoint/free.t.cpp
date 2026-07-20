// src/smd/fixpoint/free.t.cpp                                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/free.hpp> // Re-inclusion check

#include <smd/concrete/functors.hpp>
#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/one_shot.hpp>

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <utility>
#include <variant>

using smd::concrete::Cons;
using smd::concrete::IntListF;
using smd::fixpoint::Box;
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

// ---------------------------------------------------------------------
// FD4: consuming (rvalue) traversal -- move-only payload smoke, and the
// deferred-invocation lifetime test that gates the capture-ownership rule.
// ---------------------------------------------------------------------

namespace {

struct MoveOnlyInt {
    int value;

    explicit MoveOnlyInt(int v) : value(v) {}
    MoveOnlyInt(const MoveOnlyInt &) = delete;
    auto operator=(const MoveOnlyInt &) -> MoveOnlyInt & = delete;
    MoveOnlyInt(MoveOnlyInt &&) = default;
    auto operator=(MoveOnlyInt &&) -> MoveOnlyInt & = default;
};

using MOFree = Free<IntListF, MoveOnlyInt>;

// Consuming analogue of make_run above: `count` Cons layers each holding
// `value`, then a Pure leaf holding `seed` -- built by moving, never
// copying (MoveOnlyInt has no copy constructor).
auto make_run_mo(int value, int count, int seed) -> MOFree {
    if (count <= 0) {
        return pure_free<IntListF>(MoveOnlyInt{seed});
    }
    return roll_free<IntListF>(IntListF<MOFree>{Cons<int, MOFree>{
        value, make_box<MOFree>(make_run_mo(value, count - 1, seed))}});
}

} // namespace

TEST_CASE("free consuming fmap: maps a move-only Pure value and recurses "
          "through a Roll chunk") {
    const auto &functor = smd::typeclass::functor_typeclass<MOFree>;
    // Generic parameter, not a hard-coded `MoveOnlyInt&&`: IntListF is a
    // pure-data F (functors.hpp's ListFFunctorImpl has only a `const&`
    // fmap), so once fmap recurses through a Roll layer it hits FD4's
    // documented fallthrough -- the child is delivered as an lvalue
    // (`*c.tail`, box.hpp) and the recursive continuation below routes
    // to the const-path fmap overload, which in turn invokes this
    // callback with `const MoveOnlyInt&`. Only the direct-Pure case
    // (no intervening Roll layer) invokes it with a genuine
    // `MoveOnlyInt&&`. `x.value` is read either way; no copy of `x`
    // itself is needed.
    auto add_ten = [](auto &&x) -> MoveOnlyInt {
        return MoveOnlyInt{x.value + 10};
    };

    MOFree pure_leaf = pure_free<IntListF>(MoveOnlyInt{4});
    MOFree mapped_pure = functor.fmap(add_ten, std::move(pure_leaf));
    REQUIRE(is_pure(mapped_pure));
    CHECK(std::get<MoveOnlyInt>(mapped_pure.node).value == 14);

    // make_run_mo(9, 2, 4): Roll(Cons(9, Roll(Cons(9, Pure(4))))). The
    // functor's `fmap` maps only the terminal Pure seed, per the const-path
    // test above -- the consuming path must produce the same shape.
    MOFree chunk = make_run_mo(9, 2, 4);
    MOFree mapped_chunk = functor.fmap(add_ten, std::move(chunk));
    REQUIRE_FALSE(is_pure(mapped_chunk));
    auto &outer_cons = std::get<Cons<int, MOFree>>(
        std::get<IntListF<MOFree>>(mapped_chunk.node));
    CHECK(outer_cons.head == 9);
    REQUIRE_FALSE(is_pure(*outer_cons.tail));
    auto &inner_cons = std::get<Cons<int, MOFree>>(
        std::get<IntListF<MOFree>>(outer_cons.tail->node));
    CHECK(inner_cons.head == 9);
    REQUIRE(is_pure(*inner_cons.tail));
    CHECK(std::get<MoveOnlyInt>(inner_cons.tail->node).value == 14);
}

TEST_CASE("free consuming bind: sequences through a Roll chunk of a "
          "move-only payload") {
    const auto &monad = smd::typeclass::monad_typeclass<MOFree>;
    // Generic parameter for the same reason as add_ten above: this m has
    // a Roll layer, so bind falls through IntListF's const-path fmap
    // before reaching the Pure leaf.
    auto k = [](auto &&seed) -> MOFree {
        return make_run_mo(seed.value, 1, seed.value + 100);
    };

    MOFree m = make_run_mo(1, 1, 2); // Roll(Cons(1, Pure(2)))
    MOFree result = monad.bind(std::move(m), k);
    // bind recurses through the single Cons layer and applies k at the
    // Pure(2) leaf: Roll(Cons(1, k(2))) == Roll(Cons(1, make_run_mo(2,1,102))).

    REQUIRE_FALSE(is_pure(result));
    auto &cons =
        std::get<Cons<int, MOFree>>(std::get<IntListF<MOFree>>(result.node));
    CHECK(cons.head == 1);
    REQUIRE_FALSE(is_pure(*cons.tail));
    auto &inner_cons = std::get<Cons<int, MOFree>>(
        std::get<IntListF<MOFree>>(cons.tail->node));
    CHECK(inner_cons.head == 2);
    REQUIRE(is_pure(*inner_cons.tail));
    CHECK(std::get<MoveOnlyInt>(inner_cons.tail->node).value == 102);
}

// --- Deferred-invocation lifetime test (Asan gate for FD4's rule) ------
//
// A miniature of S03's real Coyoneda layer: LazyLayer<X> stores a one-shot
// continuation `k : int -> Box<X>` and nothing else. Its Functor instance
// does NOT invoke `k` -- `fmap` post-composes `fn` onto it and returns a
// new LazyLayer, exactly FD6's "closure post-composition, O(1), no
// traversal" shape. This is the case the capture-ownership rule exists
// for: a pure-data layer applies its mapping function eagerly inside the
// call (safe even with `[&self, &fn]`), but a lazy layer *stores* the
// closure past the call's return -- a reference capture anywhere on the
// path from `bind` through `layer_fmap` into this stored closure dangles
// the moment the enclosing scope exits.

namespace {

// one_shot (one_shot.hpp), not std::move_only_function: libc++ (Linux and
// Apple rows of the CI matrix) does not declare std::move_only_function
// at all, and one_shot is what S03's real Coyoneda layer stores anyway --
// its rvalue-qualified call operator carries the same one-shot-ness the
// &&-qualified move_only_function signature expressed here.
template <class X>
struct LazyLayer {
    smd::fixpoint::one_shot<smd::fixpoint::Box<X>(int)> k;
};

} // namespace

namespace smd::typeclass {

template <class X>
struct LazyLayerFunctorImpl {
    template <class Fn>
    constexpr auto fmap(this auto &&, Fn &&fn, LazyLayer<X> &&layer)
        -> LazyLayer<remove_cvref_t<std::invoke_result_t<Fn, X &&>>> {
        using Y = remove_cvref_t<std::invoke_result_t<Fn, X &&>>;
        return LazyLayer<Y>{smd::fixpoint::one_shot<smd::fixpoint::Box<Y>(int)>{
            [k = std::move(layer.k), fn = std::forward<Fn>(fn)](
                int response) mutable -> smd::fixpoint::Box<Y> {
                return smd::fixpoint::make_box<Y>(
                    std::invoke(fn, *std::move(k)(response)));
            }}};
    }
};

template <class X>
struct LazyLayerFunctorMap : Functor<LazyLayerFunctorImpl<X>> {
    using LazyLayerFunctorImpl<X>::fmap;
};

/** Functor instance for LazyLayer<X>: consuming-only (no const& overload
 * exists, or is needed -- a lazy layer's continuation is inherently
 * one-shot).
 */
template <class X>
inline constexpr auto functor_typeclass<LazyLayer<X>> =
    LazyLayerFunctorMap<X>{};

} // namespace smd::typeclass

namespace {

using LazyFree = Free<LazyLayer, MoveOnlyInt>;

auto make_lazy_program() -> LazyFree {
    return roll_free<LazyLayer>(
        LazyLayer<LazyFree>{smd::fixpoint::one_shot<Box<LazyFree>(int)>{
            [](int response) -> Box<LazyFree> {
                return make_box<LazyFree>(
                    pure_free<LazyLayer>(MoveOnlyInt{response}));
            }}});
}

} // namespace

TEST_CASE("consuming bind: continuation owns its captures (FD4)") {
    // Everything named in this inner scope -- the program, the
    // continuation `k`, the monad dictionary reference, the bound Free
    // value -- is gone by the time the outer scope invokes the extracted
    // continuation below. A `[&self, &fn]` capture anywhere on the
    // consuming path dangles a reference into this dead stack frame; Asan
    // is the detector (the step file's named gate for this rule).
    auto continuation = [] {
        LazyFree program = make_lazy_program();
        // `bump` is real per-call state, not a stateless capture: a
        // dangling `[&fn]` in bind's recursion has to actually read
        // through the dead reference to reach it, so an empty/stateless
        // continuation (nothing to read, nothing for Asan to catch)
        // can't silently make this test pass for the wrong reason.
        int bump = 1;
        auto k = [bump](MoveOnlyInt &&x) -> LazyFree {
            return pure_free<LazyLayer>(MoveOnlyInt{x.value + bump});
        };
        const auto &monad = smd::typeclass::monad_typeclass<LazyFree>;
        LazyFree bound = monad.bind(std::move(program), k);
        auto &layer = std::get<LazyLayer<LazyFree>>(bound.node);
        return std::move(layer.k);
    }();

    Box<LazyFree> result = std::move(continuation)(41);
    REQUIRE(is_pure(*result));
    CHECK(std::get<MoveOnlyInt>(result->node).value == 42);
}
