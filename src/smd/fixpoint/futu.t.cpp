// src/smd/fixpoint/futu.t.cpp                                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/futu.hpp>
#include <smd/fixpoint/futu.hpp> // Re-inclusion check

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/functors.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <utility>
#include <vector>

using smd::fixpoint::Cons;
using smd::fixpoint::Free;
using smd::fixpoint::futu;
using smd::fixpoint::IntList;
using smd::fixpoint::IntListF;
using smd::fixpoint::layer_fmap;
using smd::fixpoint::make_box;
using smd::fixpoint::Nat;
using smd::fixpoint::nat_to_int;
using smd::fixpoint::NatF;
using smd::fixpoint::Nil;
using smd::fixpoint::pure_free;
using smd::fixpoint::roll_free;
using smd::fixpoint::Succ;
using smd::fixpoint::unfold_fix;
using smd::fixpoint::Zero;
using smd::fixpoint::list_to_vector;

TEST_CASE("futu - HeaderIsIdempotent") { REQUIRE(true); }

// ---------------------------------------------------------------------
// Law (design §9): a futu whose coalgebra emits exactly one layer per step
// (layer_fmap(pure_free, psi(s))) degenerates to unfold_fix(psi), 0..10.
// ---------------------------------------------------------------------

TEST_CASE("futu law: one-layer-per-step futu degenerates to unfold_fix (Nat)") {
    auto psi = [](int m) -> NatF<int> {
        if (m <= 0) {
            return Zero{};
        }
        return Succ<int>{make_box<int>(m - 1)};
    };
    auto one_layer_coalgebra = [&psi](int m) -> NatF<Free<NatF, int>> {
        return layer_fmap(
            [](int child) -> Free<NatF, int> {
                return pure_free<NatF>(child);
            },
            psi(m));
    };

    for (int n = 0; n <= 10; ++n) {
        Nat via_futu = futu<NatF>(one_layer_coalgebra, n);
        Nat via_unfold = unfold_fix<NatF>(psi, n);
        CHECK(nat_to_int(via_futu) == nat_to_int(via_unfold));
    }
}

// ---------------------------------------------------------------------
// Behavior: run-length decode via futu on IntList (design §7.5/§10). Seed
// is an index into `{count, value}` pairs; one coalgebra step emits
// `count` Cons layers as a single Free chunk, with the next-seed Pure at
// the bottom — the futu-defining multi-layer-per-step move.
// ---------------------------------------------------------------------

namespace {

using IndexFree = Free<IntListF, std::size_t>;

// Builds `count` Cons(value) layers, then a Pure leaf holding `seed` (same
// n-deep-chunk shape as free.t.cpp's make_run, kept test-local per the
// step's own pitfalls section — not shipped in futu.hpp).
auto make_run(int value, int count, std::size_t seed) -> IndexFree {
    if (count <= 0) {
        return pure_free<IntListF>(seed);
    }
    return roll_free<IntListF>(IntListF<IndexFree>{Cons<int, IndexFree>{
        value, make_box<IndexFree>(make_run(value, count - 1, seed))}});
}

auto make_rle_coalgebra(const std::vector<std::pair<int, int>> &pairs) {
    return [&pairs](std::size_t i) -> IntListF<IndexFree> {
        if (i >= pairs.size()) {
            return Nil<int>{};
        }
        auto [count, value] = pairs[i];
        // This call's own Cons is the first of `count` layers; the
        // remaining count-1 layers are built as the chunk in the tail.
        return Cons<int, IndexFree>{
            value, make_box<IndexFree>(make_run(value, count - 1, i + 1))};
    };
}

} // namespace

TEST_CASE("futu behavior: run-length decode {{2,7},{3,1}} -> [7,7,1,1,1]") {
    std::vector<std::pair<int, int>> rle{
        {2, 7},
        {3, 1},
    };
    IntList decoded = futu<IntListF>(make_rle_coalgebra(rle), std::size_t{0});
    CHECK(list_to_vector(decoded) == std::vector<int>{7, 7, 1, 1, 1});
}

// ---------------------------------------------------------------------
// Behavior: pairwise-swap stream head — futu emitting two layers per step
// to swap adjacent elements of a generated list.
//
// DEV-01 sanity check (ops/DEVIATIONS.md): a fixture only discriminates if
// a plausible wrong answer actually differs from the right one. Here the
// "plausible bug" the step file names is a coalgebra (or engine) that only
// ever emits/consumes ONE layer per step instead of a genuine multi-layer
// chunk. Concretely: `make_one_layer_coalgebra` below is exactly the
// `layer_fmap(pure_free, ...)` single-layer degeneracy shape (the same
// shape as the law test above, and as `list_from_vector`'s ana) — it can
// only stream elements in original order, since it never looks ahead past
// the current index. Run through the very same `futu` engine, it
// reproduces the *unswapped* list, verifiably different from the
// genuinely-swapped result the two-layer coalgebra produces. That
// confirms this fixture does discriminate multi-layer emission from
// single-layer emission, rather than the two coincidentally landing on
// the same answer (the DEV-01 failure mode).
// ---------------------------------------------------------------------

namespace {

auto make_pairwise_swap_coalgebra(const std::vector<int> &v) {
    return [&v](std::size_t i) -> IntListF<IndexFree> {
        if (i + 1 >= v.size()) {
            return Nil<int>{};
        }
        // One coalgebra call emits TWO Cons layers (v[i+1] then v[i]) as a
        // single Free chunk, then resumes at seed i+2.
        return Cons<int, IndexFree>{
            v[i + 1],
            make_box<IndexFree>(roll_free<IntListF>(IntListF<IndexFree>{
                Cons<int, IndexFree>{
                    v[i], make_box<IndexFree>(pure_free<IntListF>(i + 2))}}))};
    };
}

auto make_one_layer_coalgebra(const std::vector<int> &v) {
    return [&v](std::size_t i) -> IntListF<IndexFree> {
        if (i >= v.size()) {
            return Nil<int>{};
        }
        return Cons<int, IndexFree>{
            v[i], make_box<IndexFree>(pure_free<IntListF>(i + 1))};
    };
}

} // namespace

TEST_CASE("futu behavior: pairwise-swap emits two layers per step") {
    std::vector<int> input{1, 2, 3, 4, 5, 6};

    IntList swapped =
        futu<IntListF>(make_pairwise_swap_coalgebra(input), std::size_t{0});
    CHECK(list_to_vector(swapped) == std::vector<int>{2, 1, 4, 3, 6, 5});

    IntList unswapped =
        futu<IntListF>(make_one_layer_coalgebra(input), std::size_t{0});
    CHECK(list_to_vector(unswapped) == input);
    CHECK(list_to_vector(swapped) != list_to_vector(unswapped));
}

// ---------------------------------------------------------------------
// constexpr (design D10): a small decode ({{2, 7}}) at compile time.
// ---------------------------------------------------------------------

namespace {

constexpr auto futu_constexpr_smoke() -> bool {
    auto coalgebra = [](std::size_t i) -> IntListF<IndexFree> {
        if (i != 0) {
            return Nil<int>{};
        }
        return Cons<int, IndexFree>{
            7, make_box<IndexFree>(roll_free<IntListF>(IntListF<IndexFree>{
                   Cons<int, IndexFree>{
                       7, make_box<IndexFree>(
                              pure_free<IntListF>(std::size_t{1}))}}))};
    };
    IntList decoded = futu<IntListF>(coalgebra, std::size_t{0});
    return list_to_vector(decoded) == std::vector<int>{7, 7};
}

} // namespace

static_assert(futu_constexpr_smoke());
