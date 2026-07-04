// src/smd/fixpoint/prepro.t.cpp                                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/prepro.hpp>
#include <smd/fixpoint/prepro.hpp> // Re-inclusion check

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/functors.hpp>
#include <smd/fixpoint/overloaded.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <numeric>
#include <variant>
#include <vector>

using smd::fixpoint::Cons;
using smd::fixpoint::fold_fix;
using smd::fixpoint::hoist;
using smd::fixpoint::IntList;
using smd::fixpoint::IntListF;
using smd::fixpoint::list_from_vector;
using smd::fixpoint::list_to_vector;
using smd::fixpoint::make_box;
using smd::fixpoint::Nat;
using smd::fixpoint::nat_from_int;
using smd::fixpoint::nat_to_int;
using smd::fixpoint::NatF;
using smd::fixpoint::Nil;
using smd::fixpoint::overloaded;
using smd::fixpoint::postpro;
using smd::fixpoint::prepro;
using smd::fixpoint::Succ;
using smd::fixpoint::unfold_fix;
using smd::fixpoint::Zero;

TEST_CASE("prepro - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

// ---------------------------------------------------------------------
// Fixture natural transformations. Each has a *templated* call operator
// (design §4): callable on the layer type for every carrier A, not just
// whichever concrete A a particular test happens to instantiate with.
// ---------------------------------------------------------------------

/** The endo identity natural transformation on NatF. */
struct identity_nat {
    template <class A>
    constexpr auto operator()(const NatF<A> &layer) const -> NatF<A> {
        return layer;
    }
};

/** NatF -> IntListF: Zero -> Nil, Succ(pred) -> Cons(1, pred). Used to
 * exercise hoist's non-endo (G != F) case: retagging every layer of a Nat
 * into an IntList layer while reusing the same recursive position. */
struct nat_to_intlist_nat {
    template <class A>
    constexpr auto operator()(const NatF<A> &layer) const -> IntListF<A> {
        return std::visit(
            overloaded{
                [](const Zero &) -> IntListF<A> { return Nil<int>{}; },
                [](const Succ<A> &s) -> IntListF<A> {
                    return Cons<int, A>{1, s.pred};
                },
            },
            layer);
    }
};

/** IntListF endo transformation: rewrite Cons(x, rest) to Nil once x < 0.
 * This is the "take while positive" natural transformation prepro fuses
 * into a fold (design §7.4/§10 prepro_takewhile_sum). */
struct take_while_positive_nat {
    template <class A>
    constexpr auto operator()(const IntListF<A> &layer) const -> IntListF<A> {
        return std::visit(
            overloaded{
                [](const Nil<int> &n) -> IntListF<A> { return n; },
                [](const Cons<int, A> &c) -> IntListF<A> {
                    if (c.head < 0) {
                        return Nil<int>{};
                    }
                    return c;
                },
            },
            layer);
    }
};

/** IntListF endo transformation: cap every head at 3. Used below an
 * unfold via postpro (design §10). */
struct cap_at_three_nat {
    template <class A>
    constexpr auto operator()(const IntListF<A> &layer) const -> IntListF<A> {
        return std::visit(
            overloaded{
                [](const Nil<int> &n) -> IntListF<A> { return n; },
                [](const Cons<int, A> &c) -> IntListF<A> {
                    return Cons<int, A>{c.head > 3 ? 3 : c.head, c.tail};
                },
            },
            layer);
    }
};

} // namespace

// ---------------------------------------------------------------------
// hoist.
// ---------------------------------------------------------------------

TEST_CASE("hoist with identity transformation reproduces the original tree "
          "(Nat, 0..10)") {
    for (int n = 0; n <= 10; ++n) {
        Nat original = nat_from_int(n);
        Nat hoisted = hoist<NatF>(identity_nat{}, original);
        CHECK(nat_to_int(hoisted) == nat_to_int(original));
    }
}

TEST_CASE("hoist to a different functor: NatF -> IntListF, sum equals "
          "nat_to_int (0..10)") {
    for (int n = 0; n <= 10; ++n) {
        Nat nat = nat_from_int(n);
        IntList as_list = hoist<IntListF>(nat_to_intlist_nat{}, nat);
        std::vector<int> v = list_to_vector(as_list);
        int sum = std::accumulate(v.begin(), v.end(), 0);
        CHECK(sum == nat_to_int(nat));
    }
}

// ---------------------------------------------------------------------
// Laws (design §9): with the identity natural transformation, prepro and
// postpro degenerate to fold_fix and unfold_fix respectively.
// ---------------------------------------------------------------------

TEST_CASE("prepro law: identity_nat degenerates to fold_fix (Nat, 0..10)") {
    auto algebra = [](const NatF<int> &layer) -> int {
        return std::visit(overloaded{
                               [](const Zero &) { return 0; },
                               [](const Succ<int> &s) { return *s.pred + 1; },
                           },
                           layer);
    };

    for (int n = 0; n <= 10; ++n) {
        Nat nat = nat_from_int(n);
        CHECK(prepro<int>(identity_nat{}, algebra, nat) ==
              fold_fix<int>(algebra, nat));
    }
}

TEST_CASE("postpro law: identity_nat degenerates to unfold_fix (Nat, "
          "0..10)") {
    auto coalgebra = [](int m) -> NatF<int> {
        if (m <= 0) {
            return Zero{};
        }
        return Succ<int>{make_box<int>(m - 1)};
    };

    for (int n = 0; n <= 10; ++n) {
        Nat via_postpro = postpro<NatF>(identity_nat{}, coalgebra, n);
        Nat via_unfold = unfold_fix<NatF>(coalgebra, n);
        CHECK(nat_to_int(via_postpro) == nat_to_int(via_unfold));
    }
}

// ---------------------------------------------------------------------
// Behavior: prepro take-while-positive sum (design §7.4/§10
// prepro_takewhile_sum). The transformation rewrites Cons(x, rest) to Nil
// once x < 0, fused into the summing fold.
// ---------------------------------------------------------------------

namespace {

auto sum_algebra(const IntListF<int> &layer) -> int {
    return std::visit(overloaded{
                           [](const Nil<int> &) { return 0; },
                           [](const Cons<int, int> &c) { return c.head + *c.tail; },
                       },
                       layer);
}

} // namespace

TEST_CASE("prepro behavior: take-while-positive sum ([3,4,-1,5] -> 7)") {
    IntList list = list_from_vector({3, 4, -1, 5});
    CHECK(prepro<int>(take_while_positive_nat{}, sum_algebra, list) == 7);
}

TEST_CASE("prepro behavior: take-while-positive on [3,-1,4]-shaped input "
          "catches wrong-depth transcriptions (-> 3)") {
    // A whole-subtree hoist (correct) must cut the list to just [3] here:
    // the -1 collapses everything from its own position down to Nil
    // *before* prepro recurses into it, so the 4 past the -1 never
    // contributes. A transcription that applies the natural
    // transformation at the wrong depth (e.g. only to a child's top layer
    // without recursively hoisting the whole remaining subtree first)
    // stops too early or too late on inputs shaped like this.
    IntList list = list_from_vector({3, -1, 4});
    CHECK(prepro<int>(take_while_positive_nat{}, sum_algebra, list) == 3);
}

// ---------------------------------------------------------------------
// Behavior: postpro caps values below an unfold (design §10).
// ---------------------------------------------------------------------

TEST_CASE("postpro behavior: cap_at_three applied below an unfold") {
    std::vector<int> source{1, 5, 2, 9, 3};
    auto coalgebra = [&source](std::size_t i) -> IntListF<std::size_t> {
        if (i >= source.size()) {
            return Nil<int>{};
        }
        return Cons<int, std::size_t>{source[i], make_box<std::size_t>(i + 1)};
    };

    IntList capped =
        postpro<IntListF>(cap_at_three_nat{}, coalgebra, std::size_t{0});
    std::vector<int> expected{1, 3, 2, 3, 3};
    CHECK(list_to_vector(capped) == expected);
}

// ---------------------------------------------------------------------
// constexpr (design D10).
// ---------------------------------------------------------------------

namespace {

constexpr auto hoist_constexpr_smoke() -> bool {
    auto nat = nat_from_int(3);
    return nat_to_int(hoist<NatF>(identity_nat{}, nat)) == 3;
}

constexpr auto prepro_constexpr_smoke() -> bool {
    auto algebra = [](const NatF<int> &layer) -> int {
        return std::visit(overloaded{
                               [](const Zero &) { return 0; },
                               [](const Succ<int> &s) { return *s.pred + 1; },
                           },
                           layer);
    };
    auto nat = nat_from_int(3);
    return prepro<int>(identity_nat{}, algebra, nat) == 3;
}

constexpr auto postpro_constexpr_smoke() -> bool {
    auto coalgebra = [](int m) -> NatF<int> {
        if (m <= 0) {
            return Zero{};
        }
        return Succ<int>{make_box<int>(m - 1)};
    };
    auto nat = postpro<NatF>(identity_nat{}, coalgebra, 3);
    return nat_to_int(nat) == 3;
}

} // namespace

static_assert(hoist_constexpr_smoke());
static_assert(prepro_constexpr_smoke());
static_assert(postpro_constexpr_smoke());
