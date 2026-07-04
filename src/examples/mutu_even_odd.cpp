// mutu_even_odd.cpp                                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Demonstrates mutu (design §7.3): is-even and is-odd on Nat (from
// smd/fixpoint/functors.hpp), each defined in terms of the other via a
// single mutually recursive fold (Fokkinga's banana-split construction) —
// no auxiliary modulo arithmetic, just the base case (Zero is even, not
// odd) and each Succ deferring to the *other* predicate on its predecessor.

#include <smd/fixpoint/functors.hpp>
#include <smd/fixpoint/mutu.hpp>

#include <print>
#include <utility>
#include <variant>

using smd::fixpoint::mutu;
using smd::fixpoint::Nat;
using smd::fixpoint::nat_from_int;
using smd::fixpoint::NatF;
using smd::fixpoint::overloaded;
using smd::fixpoint::Succ;
using smd::fixpoint::Zero;

namespace {

// alg_a: is-even. A Succ is even iff its predecessor is odd (.second).
auto alg_even(const NatF<std::pair<bool, bool>> &layer) -> bool {
    return std::visit(
        overloaded{
            [](const Zero &) { return true; },
            [](const Succ<std::pair<bool, bool>> &s) {
                return s.pred->second;
            },
        },
        layer);
}

// alg_b: is-odd. A Succ is odd iff its predecessor is even (.first).
auto alg_odd(const NatF<std::pair<bool, bool>> &layer) -> bool {
    return std::visit(
        overloaded{
            [](const Zero &) { return false; },
            [](const Succ<std::pair<bool, bool>> &s) {
                return s.pred->first;
            },
        },
        layer);
}

} // namespace

int main() {
    for (int n = 0; n <= 10; ++n) {
        Nat nat = nat_from_int(n);
        auto [is_even, is_odd] = mutu<bool, bool>(alg_even, alg_odd, nat);
        std::println("{}: even={} odd={}", n, is_even, is_odd);
    }
}
