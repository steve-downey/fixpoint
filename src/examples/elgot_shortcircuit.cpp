// elgot_shortcircuit.cpp                                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Demonstrates elgot (design §7.8): product of a vector<int> that bails out
// with 0 the instant a 0 is seen. The coalgebra counts how many elements it
// actually examines; once it returns Left(0) (D4: Left = stop, Right =
// continue), elgot never calls it again for the remainder of the vector.
// Printing the examined count against the vector's length is what makes the
// short-circuit visible — the product alone (0) would look the same whether
// or not the rest of the vector was ever touched, since multiplying by zero
// swallows the difference.

#include <smd/concrete/functors.hpp>
#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/elgot.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <smd/typeclass/either.hpp>

#include <cstddef>
#include <print>
#include <variant>
#include <vector>

using smd::concrete::Cons;
using smd::concrete::IntListF;
using smd::concrete::Nil;
using smd::fixpoint::elgot;
using smd::fixpoint::make_box;
using smd::fixpoint::overloaded;
using smd::typeclass::either;
using smd::typeclass::make_left;
using smd::typeclass::make_right;

namespace {

// F<Result> -> Result: the product algebra. Nil is the multiplicative
// identity (reached only if the coalgebra runs off the end without ever
// seeing a 0); Cons multiplies its head by the already-folded tail.
auto product_algebra(const IntListF<int> &layer) -> int {
    return std::visit(
        overloaded{
            [](const Nil<int> &) { return 1; },
            [](const Cons<int, int> &c) { return c.head * *c.tail; },
        },
        layer);
}

} // namespace

// 5e2768c7-a257-4616-aef1-6d7634d818a2
int main() {
    std::vector<int> values{4, 3, 0, 5, 9, 2};

    std::size_t examined = 0;
    auto coalgebra = [&](std::size_t i) -> either<int, IntListF<std::size_t>> {
        ++examined;
        if (i >= values.size()) {
            return make_right<int>(IntListF<std::size_t>{Nil<int>{}});
        }
        if (values[i] == 0) {
            return make_left<IntListF<std::size_t>>(0);
        }
        return make_right<int>(IntListF<std::size_t>{
            Cons<int, std::size_t>{values[i], make_box<std::size_t>(i + 1)}});
    };

    int product =
        elgot<int, IntListF>(product_algebra, coalgebra, std::size_t{0});

    std::println("values: {} elements", values.size());
    std::println("product (bails out at the first 0): {}", product);
    std::println("elements examined: {} / {}", examined, values.size());
}
// 5e2768c7-a257-4616-aef1-6d7634d818a2 end
