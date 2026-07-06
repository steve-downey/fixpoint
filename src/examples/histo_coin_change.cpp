// histo_coin_change.cpp                                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Demonstrates histo (design §7.5): the classic course-of-values-fold coin
// -change example. For coins {1, 4, 5}, the minimal number of coins to make
// amount n is:
//
//   minCoins(0) = 0
//   minCoins(n) = 1 + min( minCoins(n-1),
//                          minCoins(n-4)  [only if n >= 4],
//                          minCoins(n-5)  [only if n >= 5] )
//
// A plain fold_fix can only see its *own* child's folded Result — it has no
// way to reach minCoins(n-4) or minCoins(n-5) from the node for n, since
// those live several layers further down the Nat. histo's algebra instead
// receives a whole Cofree<NatF, int> at each child position: not just
// minCoins(n-1), but the *entire annotated history* leading up to it. The
// algebra below walks that history 3 and 4 steps further back (from the
// n-1 node) to reach minCoins(n-4) and minCoins(n-5).

#include <smd/concrete/functors.hpp>
#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/histo.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <optional>
#include <print>
#include <variant>

using smd::concrete::Nat;
using smd::concrete::nat_from_int;
using smd::concrete::NatF;
using smd::concrete::Succ;
using smd::concrete::Zero;
using smd::fixpoint::Cofree;
using smd::fixpoint::histo;
using smd::fixpoint::overloaded;

namespace {

// 7a78616c-c66c-4ffd-ab18-e192aac2d913
using History = Cofree<NatF, int>; // annotates each Nat node with minCoins

// Step `steps` layers further back through an already-computed history.
// `c` is the history at some amount m; look_back(c, k) is minCoins(m - k),
// or nullopt if m - k < 0 (ran off the front of the Nat).
constexpr auto look_back(const History &c, int steps) -> std::optional<int> {
    if (steps == 0) {
        return c.head;
    }
    if (std::holds_alternative<Zero>(c.tail)) {
        return std::nullopt;
    }
    return look_back(*std::get<Succ<History>>(c.tail).pred, steps - 1);
}

// The histo algebra: F<Cofree<F, int>> -> int, i.e.
// NatF<History> -> int.
auto min_coins_algebra(const NatF<History> &layer) -> int {
    return std::visit(overloaded{
                          [](const Zero &) { return 0; }, // minCoins(0) = 0
                          [](const Succ<History> &s) -> int {
                              // s.pred is the history for amount n-1: its own
                              // head is minCoins(n-1), and its tail lets us
                              // walk further back.
                              const History &pred = *s.pred;
                              int best = pred.head; // coin 1: 1 + minCoins(n-1)

                              // coin 4: 1 + minCoins(n-4) == 1 + minCoins((n-1)
                              // - 3)
                              if (auto m4 = look_back(pred, 3)) {
                                  if (*m4 < best) {
                                      best = *m4;
                                  }
                              }
                              // coin 5: 1 + minCoins(n-5) == 1 + minCoins((n-1)
                              // - 4)
                              if (auto m5 = look_back(pred, 4)) {
                                  if (*m5 < best) {
                                      best = *m5;
                                  }
                              }
                              return best + 1;
                          },
                      },
                      layer);
}
// 7a78616c-c66c-4ffd-ab18-e192aac2d913 end

auto min_coins(int n) -> int {
    Nat nat = nat_from_int(n);
    return histo<int>(min_coins_algebra, nat);
}

} // namespace

int main() {
    for (int n : {0, 1, 4, 5, 8, 12, 13}) {
        std::println("minCoins({}, coins={{1,4,5}}) = {}", n, min_coins(n));
    }
}
