// prepro_takewhile_sum.cpp                                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Demonstrates prepro (design §7.4): take-while fused into a fold. The
// natural transformation rewrites `Cons(x, rest)` to `Nil` once `x < 0`;
// prepro applies it cumulatively on the way down an IntList (from
// smd/concrete/functors.hpp) before the summing algebra ever sees a node,
// so the negative element (and everything past it) is dropped without a
// separate take-while pass over the list.

#include <smd/concrete/functors.hpp>
#include <smd/fixpoint/prepro.hpp>

#include <print>
#include <variant>
#include <vector>

using smd::concrete::Cons;
using smd::concrete::IntList;
using smd::concrete::IntListF;
using smd::concrete::list_from_vector;
using smd::concrete::Nil;
using smd::fixpoint::overloaded;
using smd::fixpoint::prepro;

namespace {

// b46e1d07-0c11-4acb-ae54-673564092c5b
// The natural transformation: IntListF<A> -> IntListF<A> for every A. Must
// have a templated call operator (design §4) — it runs at F<Fix<F>> during
// prepro's cumulative hoisting, not at the algebra's concrete carrier.
struct take_while_positive {
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

// The algebra: sum every remaining head.
auto sum_algebra(const IntListF<int> &layer) -> int {
    return std::visit(
        overloaded{
            [](const Nil<int> &) { return 0; },
            [](const Cons<int, int> &c) { return c.head + *c.tail; },
        },
        layer);
}
// b46e1d07-0c11-4acb-ae54-673564092c5b end

void print_sum(const std::vector<int> &v) {
    IntList list = list_from_vector(v);
    int sum = prepro<int>(take_while_positive{}, sum_algebra, list);
    std::print("sum(take_while(>=0, [");
    for (std::size_t i = 0; i < v.size(); ++i) {
        std::print("{}{}", i == 0 ? "" : ", ", v[i]);
    }
    std::println("])) = {}", sum);
}

} // namespace

int main() {
    print_sum({3, 4, -1, 5});
    print_sum({3, -1, 4});
    print_sum({1, 2, 3});
}
