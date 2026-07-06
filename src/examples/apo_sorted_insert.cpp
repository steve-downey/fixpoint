// apo_sorted_insert.cpp                                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Demonstrates apo (design §7.2): insert a value into an already-sorted
// IntList (from smd/fixpoint/functors.hpp). The coalgebra walks the list
// looking for the insertion point; once found, instead of continuing to
// unfold element-by-element it short-circuits with `Left`, grafting the
// *untouched* remainder of the original list back in as-is — no fold_fix
// needed to rebuild everything after the insertion point.

#include <smd/fixpoint/apo.hpp>
#include <smd/fixpoint/functors.hpp>

#include <smd/typeclass/either.hpp>

#include <print>
#include <variant>
#include <vector>

using smd::fixpoint::apo;
using smd::fixpoint::Cons;
using smd::fixpoint::IntList;
using smd::fixpoint::IntListF;
using smd::fixpoint::list_from_vector;
using smd::fixpoint::list_to_vector;
using smd::fixpoint::make_box;
using smd::fixpoint::Nil;
using smd::fixpoint::overloaded;
using smd::fixpoint::unwrap_fix;
using smd::typeclass::either;
using smd::typeclass::make_left;
using smd::typeclass::make_right;

namespace {

// e0098ed1-a5bf-4681-8dfb-8cd06ee73e5d
/** Coalgebra for inserting @p value into an already-sorted IntList. Seed =
 * the remaining sublist still to be scanned. Left grafts the *current*
 * seed list (untouched) once the insertion point is found or the list
 * runs out; Right keeps unfolding past elements smaller than @p value.
 */
auto make_insert_coalgebra(int value) {
    return [value](const IntList &remaining)
               -> IntListF<either<IntList, IntList>> {
        const auto &layer = unwrap_fix(remaining);
        return std::visit(
            overloaded{
                [&](const Nil<int> &) -> IntListF<either<IntList, IntList>> {
                    // Ran off the end: value is the new largest element.
                    return Cons<int, either<IntList, IntList>>{
                        value, make_box<either<IntList, IntList>>(
                                   make_left<IntList>(remaining))};
                },
                [&](const Cons<int, IntList> &c)
                    -> IntListF<either<IntList, IntList>> {
                    if (value <= c.head) {
                        // Found the insertion point: graft the whole
                        // current list (starting at c.head) as-is.
                        return Cons<int, either<IntList, IntList>>{
                            value, make_box<either<IntList, IntList>>(
                                       make_left<IntList>(remaining))};
                    }
                    // Keep c.head, keep unfolding on the tail.
                    return Cons<int, either<IntList, IntList>>{
                        c.head, make_box<either<IntList, IntList>>(
                                    make_right<IntList>(*c.tail))};
                },
            },
            layer);
    };
}
// e0098ed1-a5bf-4681-8dfb-8cd06ee73e5d end

void print_vector(const char *label, const std::vector<int> &v) {
    std::print("{}: [", label);
    for (std::size_t i = 0; i < v.size(); ++i) {
        std::print("{}{}", i == 0 ? "" : ", ", v[i]);
    }
    std::println("]");
}

} // namespace

int main() {
    IntList sorted = list_from_vector({1, 3, 7, 9});
    print_vector("before", list_to_vector(sorted));

    IntList inserted = apo<IntListF>(make_insert_coalgebra(5), sorted);
    print_vector("after inserting 5", list_to_vector(inserted));
}
