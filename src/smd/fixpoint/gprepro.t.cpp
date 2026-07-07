// src/smd/fixpoint/gprepro.t.cpp                                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/generalized.hpp>
#include <smd/fixpoint/generalized.hpp> // Re-inclusion check

#include <smd/concrete/functors.hpp>
#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/dist_laws.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/overloaded.hpp>
#include <smd/fixpoint/prepro.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <smd/typeclass/identity.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <utility>
#include <variant>
#include <vector>

using smd::concrete::Cons;
using smd::concrete::IntList;
using smd::concrete::IntListF;
using smd::concrete::list_from_vector;
using smd::concrete::list_to_vector;
using smd::concrete::Nat;
using smd::concrete::nat_from_int;
using smd::concrete::nat_to_int;
using smd::concrete::NatF;
using smd::concrete::Nil;
using smd::concrete::Succ;
using smd::concrete::Zero;
using smd::fixpoint::Cofree;
using smd::fixpoint::dist_ana;
using smd::fixpoint::dist_cata;
using smd::fixpoint::dist_histo;
using smd::fixpoint::extract;
using smd::fixpoint::fold_fix;
using smd::fixpoint::gcata;
using smd::fixpoint::gpostpro;
using smd::fixpoint::gprepro;
using smd::fixpoint::hoist;
using smd::fixpoint::layer_fmap;
using smd::fixpoint::make_box;
using smd::fixpoint::overloaded;
using smd::fixpoint::postpro;
using smd::fixpoint::prepro;
using smd::fixpoint::unfold_fix;
using smd::fixpoint::zygo_histo_prepro;

using smd::typeclass::Identity;

TEST_CASE("gprepro - HeaderIsIdempotent") { REQUIRE(true); }

// ---------------------------------------------------------------------
// Fixture natural transformations (design §4, redeclared locally per
// gana.t.cpp's own precedent: these fixtures live in prepro.t.cpp's own
// anonymous namespace, one translation unit away, so this file redeclares
// the exact shapes rather than reaching across translation units).
// ---------------------------------------------------------------------

namespace {

/** The endo identity natural transformation, generic over *any* functor
 * layer (not fixed to one base functor family like prepro.t.cpp's own
 * NatF-specific `identity_nat`) -- this file exercises identity on both
 * NatF and IntListF, so one fully generic struct covers both instead of
 * two near-duplicate ones. */
struct identity_nat {
    template <class Layer>
    constexpr auto operator()(const Layer &layer) const -> Layer {
        return layer;
    }
};

/** IntListF endo transformation: rewrite Cons(x, rest) to Nil once x < 0
 * (prepro.t.cpp's own fixture, S06/S15 shared shape). */
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

/** IntListF endo transformation: cap every head at 3 (prepro.t.cpp's own
 * postpro fixture). */
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
// gprepro laws (design §9/§7.11): three degeneracies.
// ---------------------------------------------------------------------

namespace {

constexpr auto nat_count_algebra(const NatF<int> &layer) -> int {
    return std::visit(overloaded{
                          [](const Zero &) { return 0; },
                          [](const Succ<int> &s) { return *s.pred + 1; },
                      },
                      layer);
}

constexpr auto nat_count_algebra_prime(const NatF<Identity<int>> &layer)
    -> int {
    return nat_count_algebra(
        layer_fmap([](const Identity<int> &i) { return i.value; }, layer));
}

auto sum_algebra(const IntListF<int> &layer) -> int {
    return std::visit(
        overloaded{
            [](const Nil<int> &) { return 0; },
            [](const Cons<int, int> &c) { return c.head + *c.tail; },
        },
        layer);
}

auto sum_algebra_prime(const IntListF<Identity<int>> &layer) -> int {
    return sum_algebra(
        layer_fmap([](const Identity<int> &i) { return i.value; }, layer));
}

auto fib_algebra(const NatF<Cofree<NatF, int>> &layer) -> int {
    return std::visit(
        overloaded{
            [](const Zero &) { return 0; },
            [](const Succ<Cofree<NatF, int>> &s) -> int {
                const Cofree<NatF, int> &c = *s.pred;
                return std::visit(
                    overloaded{
                        [&](const Zero &) { return 1; },
                        [&](const Succ<Cofree<NatF, int>> &cc) -> int {
                            return c.head + cc.pred->head;
                        },
                    },
                    c.tail);
            },
        },
        layer);
}

} // namespace

TEST_CASE("gprepro law: gprepro(dist_cata, identity_nat, phi') equals "
          "fold_fix(phi) (Nat, 0..10)") {
    for (int n = 0; n <= 10; ++n) {
        Nat nat = nat_from_int(n);
        CHECK((gprepro<int, Identity<int>>(dist_cata, identity_nat{},
                                           nat_count_algebra_prime, nat)) ==
              fold_fix<int>(nat_count_algebra, nat));
    }
}

TEST_CASE("gprepro law: gprepro(dist_cata, e, phi') equals prepro(e, phi) "
          "(take-while-positive fixture, [3,4,-1,5])") {
    IntList list = list_from_vector({3, 4, -1, 5});
    int via_gprepro = gprepro<int, Identity<int>>(
        dist_cata, take_while_positive_nat{}, sum_algebra_prime, list);
    int via_prepro = prepro<int>(take_while_positive_nat{}, sum_algebra, list);
    CHECK(via_gprepro == via_prepro);
    CHECK(via_gprepro == 7);
}

TEST_CASE("gprepro law: gprepro(k, identity_nat, phi) equals gcata(k, phi) "
          "(dist_histo, Fibonacci, 0..10)") {
    for (int n = 0; n <= 10; ++n) {
        Nat nat = nat_from_int(n);
        CHECK((gprepro<int, Cofree<NatF, int>>(dist_histo<NatF>, identity_nat{},
                                               fib_algebra, nat)) ==
              (gcata<int, Cofree<NatF, int>>(dist_histo<NatF>, fib_algebra,
                                             nat)));
    }
}

// ---------------------------------------------------------------------
// gpostpro laws (design §9/§7.11): two degeneracies.
// ---------------------------------------------------------------------

namespace {

constexpr auto countdown(int m) -> NatF<int> {
    if (m <= 0) {
        return Zero{};
    }
    return Succ<int>{make_box<int>(m - 1)};
}

constexpr auto countdown_prime(int m) -> NatF<Identity<int>> {
    return layer_fmap([](int x) { return Identity<int>{x}; }, countdown(m));
}

} // namespace

TEST_CASE("gpostpro law: gpostpro(dist_ana, identity_nat, psi') equals "
          "unfold_fix(psi) (Nat, 0..10)") {
    for (int n = 0; n <= 10; ++n) {
        Nat via_gpostpro = gpostpro<NatF, Identity<int>>(
            dist_ana, identity_nat{}, countdown_prime, n);
        Nat via_unfold = unfold_fix<NatF>(countdown, n);
        CHECK(nat_to_int(via_gpostpro) == nat_to_int(via_unfold));
    }
}

TEST_CASE("gpostpro law: gpostpro(dist_ana, e, psi') equals postpro(e, psi) "
          "(cap-at-three fixture)") {
    std::vector<int> source{1, 5, 2, 9, 3};
    auto coalgebra = [&source](std::size_t i) -> IntListF<std::size_t> {
        if (i >= source.size()) {
            return Nil<int>{};
        }
        return Cons<int, std::size_t>{source[i], make_box<std::size_t>(i + 1)};
    };
    auto coalgebra_prime =
        [&coalgebra](std::size_t i) -> IntListF<Identity<std::size_t>> {
        return layer_fmap(
            [](std::size_t x) { return Identity<std::size_t>{x}; },
            coalgebra(i));
    };

    IntList via_gpostpro = gpostpro<IntListF, Identity<std::size_t>>(
        dist_ana, cap_at_three_nat{}, coalgebra_prime, std::size_t{0});
    IntList via_postpro =
        postpro<IntListF>(cap_at_three_nat{}, coalgebra, std::size_t{0});

    std::vector<int> expected{1, 3, 2, 3, 3};
    CHECK(list_to_vector(via_gpostpro) == expected);
    CHECK(list_to_vector(via_gpostpro) == list_to_vector(via_postpro));
}

// ---------------------------------------------------------------------
// zygo_histo_prepro (design §7.11 capstone).
// ---------------------------------------------------------------------

namespace {

// Helper folds ignored by the degeneracy test's main algebra below.
auto length_helper(const IntListF<int> &layer) -> int {
    return std::visit(overloaded{
                          [](const Nil<int> &) { return 0; },
                          [](const Cons<int, int> &c) { return *c.tail + 1; },
                      },
                      layer);
}

// Degeneracy main algebra: ignores the Helper component entirely, reads
// only `extract` of the Cofree component -- design §7.11's own degeneracy
// gate, "helper ignored ... Cofree used only via extract".
auto degenerate_main(
    const IntListF<std::pair<int, Cofree<IntListF, int>>> &layer) -> int {
    return std::visit(
        overloaded{
            [](const Nil<int> &) { return 0; },
            [](const Cons<int, std::pair<int, Cofree<IntListF, int>>> &c)
                -> int { return c.head + extract(c.tail->second); },
        },
        layer);
}

} // namespace

TEST_CASE("zygo_histo_prepro law: identity transformation + helper-ignored "
          "+ Cofree-extract-only degenerates to fold_fix") {
    for (const auto &v :
         std::vector<std::vector<int>>{{}, {1, 2, 3}, {5, 4, 3, 2, 1}}) {
        IntList list = list_from_vector(v);
        int via_zhp = zygo_histo_prepro<int, int>(length_helper, identity_nat{},
                                                  degenerate_main, list);
        int via_fold = fold_fix<int>(sum_algebra, list);
        CHECK(via_zhp == via_fold);
    }
}

namespace {

// Real behavior: helper = remaining-length-from-here (a length fold, S05's
// own convention), transformation = take-while-positive (S06), main
// algebra sums the current head only where the *tail's* remaining length is
// even (equivalently: elements at an even distance from the end of the
// positive prefix are kept), consulting one step of Cofree history (the
// previous node's already-computed sum, via `extract`) to accumulate.
auto even_tail_length_main(
    const IntListF<std::pair<int, Cofree<IntListF, int>>> &layer) -> int {
    return std::visit(
        overloaded{
            [](const Nil<int> &) { return 0; },
            [](const Cons<int, std::pair<int, Cofree<IntListF, int>>> &c)
                -> int {
                int tail_length = c.tail->first;
                int rest_sum = extract(c.tail->second);
                if (tail_length % 2 == 0) {
                    return c.head + rest_sum;
                }
                return rest_sum;
            },
        },
        layer);
}

} // namespace

TEST_CASE("zygo_histo_prepro behavior: helper+history parity sum over "
          "positive prefix ([3,4,-1,5] -> 4)") {
    // take_while_positive_nat truncates [3,4,-1,5] to [3,4] (S06's own
    // [3,4,-1,5]->7-via-sum fixture, reused here). Remaining lengths in the
    // truncated list: at node "4" (last), tail is Nil -> tail_length = 0
    // (even) -> "4" is kept. At node "3", tail is the "4"-node, whose own
    // helper value (length including "4") is 1 -> tail_length at "3" = 1
    // (odd) -> "3" is dropped. Total = 4.
    IntList list = list_from_vector({3, 4, -1, 5});
    int result = zygo_histo_prepro<int, int>(
        length_helper, take_while_positive_nat{}, even_tail_length_main, list);
    CHECK(result == 4);
}

// ---------------------------------------------------------------------
// constexpr (design D10).
// ---------------------------------------------------------------------

namespace {

constexpr auto gprepro_constexpr_smoke() -> bool {
    auto nat = nat_from_int(4);
    return (gprepro<int, Identity<int>>(dist_cata, identity_nat{},
                                        nat_count_algebra_prime, nat)) == 4;
}

constexpr auto gpostpro_constexpr_smoke() -> bool {
    auto nat = gpostpro<NatF, Identity<int>>(dist_ana, identity_nat{},
                                             countdown_prime, 4);
    return nat_to_int(nat) == 4;
}

constexpr auto zygo_histo_prepro_degenerate_main(
    const NatF<std::pair<int, Cofree<NatF, int>>> &layer) -> int {
    return std::visit(
        overloaded{
            [](const Zero &) { return 0; },
            [](const Succ<std::pair<int, Cofree<NatF, int>>> &s) -> int {
                return extract(s.pred->second) + 1;
            },
        },
        layer);
}

constexpr auto zygo_histo_prepro_helper_ignored(const NatF<int> &) -> int {
    return 0;
}

constexpr auto zygo_histo_prepro_constexpr_smoke() -> bool {
    auto nat = nat_from_int(4);
    return (zygo_histo_prepro<int, int>(
               zygo_histo_prepro_helper_ignored, identity_nat{},
               zygo_histo_prepro_degenerate_main, nat)) == 4;
}

} // namespace

static_assert(gprepro_constexpr_smoke());
static_assert(gpostpro_constexpr_smoke());
static_assert(zygo_histo_prepro_constexpr_smoke());
