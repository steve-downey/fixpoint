// src/smd/fixpoint/mcata_free.t.cpp                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// S07 (ops/freer/steps/07-sr-interpreter.md): mcata_free, exercised
// standalone over a pure-data functor's Free -- a futu chunk (IntListF),
// exactly as futu.t.cpp's make_run builds one -- so the scheme is proven
// independent of Freer/beman before freer_task.hpp puts it to work with
// Result = beman::task<A>. No beman dependency anywhere in this file.

#include <smd/fixpoint/mcata_free.hpp>
#include <smd/fixpoint/mcata_free.hpp> // Re-inclusion check

#include <smd/concrete/functors.hpp>
#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <variant>

using smd::concrete::Cons;
using smd::concrete::IntListF;
using smd::concrete::Nil;
using smd::fixpoint::Free;
using smd::fixpoint::make_box;
using smd::fixpoint::mcata_free;
using smd::fixpoint::overloaded;
using smd::fixpoint::pure_free;
using smd::fixpoint::roll_free;

namespace {

using Chunk = Free<IntListF, int>;

/** Build `count` Cons(value) layers, then a Pure leaf holding `seed` -- the
 * same n-deep-chunk shape futu.t.cpp's make_run builds (a single futu
 * coalgebra step's worth of output), kept test-local per house precedent
 * (futu.t.cpp's own comment: not shipped in a production header).
 */
auto make_chunk(int value, int count, int seed) -> Chunk {
    if (count <= 0) {
        return pure_free<IntListF>(seed);
    }
    return roll_free<IntListF>(IntListF<Chunk>{Cons<int, Chunk>{
        value, make_box<Chunk>(make_chunk(value, count - 1, seed))}});
}

} // namespace

TEST_CASE("mcata_free - HeaderIsIdempotent") { REQUIRE(true); }

// ---------------------------------------------------------------------
// mcata_free standalone (design doc Section 5 forward reference): sum a
// futu chunk's Cons values plus its trailing Pure seed. pure_alg handles
// the Pure leaf; phi handles one IntListF<Chunk> layer, folding its tail
// via `recurse` when there is one -- no functor_typeclass<IntListF> lookup
// happens anywhere in this fold (mendler.hpp's documented discipline).
// ---------------------------------------------------------------------

TEST_CASE("mcata_free: sums a futu chunk's Cons values plus its Pure seed") {
    auto pure_alg = [](int &&seed) -> int { return seed; };
    auto phi = [](auto recurse, IntListF<Chunk> &&layer) -> int {
        return std::visit(overloaded{
                              [](Nil<int> &&) -> int { return 0; },
                              [&recurse](Cons<int, Chunk> &&c) -> int {
                                  return c.head + recurse(*std::move(c.tail));
                              },
                          },
                          std::move(layer));
    };

    Chunk chunk = make_chunk(3, 4, 100); // 3+3+3+3+100 = 112
    CHECK(mcata_free<int>(pure_alg, phi, std::move(chunk)) == 112);
}

TEST_CASE("mcata_free: a chunk with zero Cons layers reduces to the Pure "
          "seed alone") {
    auto pure_alg = [](int &&seed) -> int { return seed; };
    auto phi = [](auto recurse, IntListF<Chunk> &&layer) -> int {
        return std::visit(overloaded{
                              [](Nil<int> &&) -> int { return 0; },
                              [&recurse](Cons<int, Chunk> &&c) -> int {
                                  return c.head + recurse(*std::move(c.tail));
                              },
                          },
                          std::move(layer));
    };

    Chunk chunk = make_chunk(7, 0, 42);
    CHECK(mcata_free<int>(pure_alg, phi, std::move(chunk)) == 42);
}

// ---------------------------------------------------------------------
// A different Result type (std::string) proves the fold is not secretly
// coupled to arithmetic accumulation -- Result is a free choice (design
// D5), and phi is free to ignore `recurse` on some children (Nil) while
// using it on others (Cons), exactly per mendler.hpp's documented
// discipline for mcata/mhisto.
// ---------------------------------------------------------------------

TEST_CASE("mcata_free: renders a chunk's values into a string, Result "
          "independent of arithmetic accumulation") {
    auto pure_alg = [](int &&seed) -> std::string {
        return "<" + std::to_string(seed) + ">";
    };
    auto phi = [](auto recurse, IntListF<Chunk> &&layer) -> std::string {
        return std::visit(overloaded{
                              [](Nil<int> &&) -> std::string { return "."; },
                              [&recurse](Cons<int, Chunk> &&c) -> std::string {
                                  return std::to_string(c.head) + "," +
                                         recurse(*std::move(c.tail));
                              },
                          },
                          std::move(layer));
    };

    Chunk chunk = make_chunk(1, 2, 9);
    CHECK(mcata_free<std::string>(pure_alg, phi, std::move(chunk)) ==
          "1,1,<9>");
}
