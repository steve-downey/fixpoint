// futu_rle_decode.cpp                                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Demonstrates futu (design §7.5): run-length decoding onto an IntList.
// The seed is an index into a vector of {count, value} pairs; a single
// coalgebra step doesn't just emit one Cons layer the way an ordinary
// unfold_fix coalgebra would — it emits a whole *chunk* of `count` Cons
// layers at once, built as a Free<IntListF, size_t> (free.hpp) via nested
// roll_free, with the Pure leaf at the bottom holding the next pair's
// index. That multi-layer-per-step move is exactly what distinguishes futu
// from a plain ana/unfold_fix: unfold_fix's coalgebra can only ever
// contribute one layer per call.

#include <smd/concrete/functors.hpp>
#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/futu.hpp>

#include <cstddef>
#include <print>
#include <utility>
#include <vector>

using smd::concrete::Cons;
using smd::concrete::IntList;
using smd::concrete::IntListF;
using smd::concrete::list_to_vector;
using smd::concrete::Nil;
using smd::fixpoint::Free;
using smd::fixpoint::futu;
using smd::fixpoint::make_box;
using smd::fixpoint::pure_free;
using smd::fixpoint::roll_free;

namespace {

// 5108825f-15af-4371-a949-3ecb9e370b8d
using IndexFree = Free<IntListF, std::size_t>;

// Builds `count` further Cons(value) layers, then a Pure leaf holding
// `seed` — the tail of the chunk a single RLE coalgebra step emits.
auto build_run_tail(int value, int count, std::size_t seed) -> IndexFree {
    if (count <= 0) {
        return pure_free<IntListF>(seed);
    }
    return roll_free<IntListF>(IntListF<IndexFree>{Cons<int, IndexFree>{
        value, make_box<IndexFree>(build_run_tail(value, count - 1, seed))}});
}

// The futu coalgebra: seed = index into `pairs`. Each call emits `count`
// Cons(value) layers as one Free chunk (this call's own Cons is the first
// layer, build_run_tail supplies the remaining count-1), resuming at the
// next pair's index.
auto make_rle_coalgebra(const std::vector<std::pair<int, int>> &pairs) {
    return [&pairs](std::size_t i) -> IntListF<IndexFree> {
        if (i >= pairs.size()) {
            return Nil<int>{};
        }
        auto [count, value] = pairs[i];
        return Cons<int, IndexFree>{value, make_box<IndexFree>(build_run_tail(
                                               value, count - 1, i + 1))};
    };
}
// 5108825f-15af-4371-a949-3ecb9e370b8d end

void print_pairs(const std::vector<std::pair<int, int>> &pairs) {
    std::print("input: [");
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        std::print("{}({}, {})", i == 0 ? "" : ", ", pairs[i].first,
                   pairs[i].second);
    }
    std::println("]");
}

void print_vector(const char *label, const std::vector<int> &v) {
    std::print("{}: [", label);
    for (std::size_t i = 0; i < v.size(); ++i) {
        std::print("{}{}", i == 0 ? "" : ", ", v[i]);
    }
    std::println("]");
}

void decode_and_print(const std::vector<std::pair<int, int>> &pairs) {
    print_pairs(pairs);
    IntList decoded = futu<IntListF>(make_rle_coalgebra(pairs), std::size_t{0});
    print_vector("decoded", list_to_vector(decoded));
}

} // namespace

int main() {
    decode_and_print({{2, 7}, {3, 1}});
    decode_and_print({{1, 9}, {4, 0}, {2, 5}});
}
