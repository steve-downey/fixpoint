// dyna_fibonacci.cpp                                                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Demonstrates dyna (design §7.6): the canonical dynamorphism. fib(n) is
// computed directly from an int seed, with dyna's refold *fusion*
// interleaving the ana half (countdown: n -> n-1 -> ... -> 0) and the histo
// half (the same course-of-values Fibonacci algebra as histo_coin_change.cpp
// /histo.t.cpp) in a single pass.
//
// The crucial point: at no stage does a Nat tree (Fix<NatF>) get built and
// then walked a second time. `histo(fib_algebra, unfold_fix<NatF>(countdown,
// n))` would build that intermediate tree; `dyna` never does — it calls
// refold, which produces the Cofree<NatF, int> "history" carrier directly
// from the countdown coalgebra, one layer at a time. The Cofree history
// *is* the DP table: fib_algebra reaches one and two layers back into it
// (c.head, cc.pred->head) exactly the way histo's algebra reaches back into
// a Cofree built from an already-materialized tree — the fusion changes
// nothing about what the algebra sees, only how (and whether) the
// intermediate tree exists in memory.

#include <smd/concrete/functors.hpp>
#include <smd/fixpoint/chrono.hpp>
#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <print>
#include <variant>

using smd::concrete::NatF;
using smd::concrete::Succ;
using smd::concrete::Zero;
using smd::fixpoint::Cofree;
using smd::fixpoint::dyna;
using smd::fixpoint::overloaded;

namespace {

// d89d79e8-f8eb-47a9-866f-3b9e424ea333
// The ana half: counts down from n to 0. Seed and layer element type are
// both plain int — no Free chunk, unlike futu/codyna/chrono's coalgebras.
auto countdown(int m) -> NatF<int> {
    if (m <= 0) {
        return Zero{};
    }
    return Succ<int>{smd::fixpoint::make_box<int>(m - 1)};
}

// The histo half: Succ c has c.head == fib(n-1) (already computed by an
// earlier refold step, stored in the Cofree history); if c's own tail is
// Zero, n-1 == 0 so fib(n) == 1 (the fib(1) base case); otherwise c's tail
// is Succ(cc) and cc.head == fib(n-2), so fib(n) == c.head + cc.head. No
// Nat node is ever inspected here — only Cofree<NatF, int> history.
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

auto fib(int n) -> int { return dyna<int, NatF>(fib_algebra, countdown, n); }
// d89d79e8-f8eb-47a9-866f-3b9e424ea333 end

} // namespace

int main() {
    for (int n = 0; n <= 10; ++n) {
        std::println("fib({}) = {}", n, fib(n));
    }
}
