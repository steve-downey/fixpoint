// free_cofree_interlude.cpp                                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Companion to the blog interlude "Free, Cofree, and Their Free-er
// Relatives". Fix is the degenerate case of both carriers, in the library's
// own types:
//
//   * Cofree<F, Unit>  is a Fix<F> with a trivial annotation at each node
//     (the Haskell iso  Cofree f () ~= Fix f);
//   * Free<F, Empty>   is a Fix<F> with no Pure leaves ever used
//     (the Haskell iso  Free f Void ~= Fix f).
//
// Both conversions round-trip, checked at compile time by static_assert.
// Haskell twins of these isomorphisms live in docs/blog/code/FixRel.hs.

#include <smd/concrete/functors.hpp>
#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/free.hpp>

#include <print>
#include <variant>

using smd::concrete::make_succ;
using smd::concrete::make_zero;
using smd::concrete::Nat;
using smd::concrete::nat_to_int;
using smd::concrete::NatF;
using smd::fixpoint::Cofree;
using smd::fixpoint::Free;
using smd::fixpoint::layer_fmap;
using smd::fixpoint::roll_free;
using smd::fixpoint::unwrap_cofree;
using smd::fixpoint::unwrap_fix;
using smd::fixpoint::wrap_fix;

namespace {

// 2ecd0000-0000-4000-8000-000000000001
// The two "trivial" annotation/leaf types. Unit is the trivial annotation
// (the () of Cofree f ()); Empty stands in for Haskell's uninhabited Void:
// the Pure alternative of Free<NatF, Empty> is simply never constructed.
// Haskell's Void makes "no Pure leaf" a theorem; here it is a discipline --
// the same iso-recursion honesty as Part 1's wrap_fix / unwrap_fix.
struct Unit {
    friend constexpr auto operator==(Unit, Unit) -> bool { return true; }
};
struct Empty {
    friend constexpr auto operator==(Empty, Empty) -> bool { return true; }
};

// Fix<NatF>  <->  Cofree<NatF, Unit>   (Cofree f () ~= Fix f)
constexpr auto fix_to_cofree(const Nat &n) -> Cofree<NatF, Unit> {
    auto tail = layer_fmap([](const Nat &child) { return fix_to_cofree(child); },
                           unwrap_fix(n));
    return Cofree<NatF, Unit>{Unit{}, std::move(tail)};
}

constexpr auto cofree_to_fix(const Cofree<NatF, Unit> &c) -> Nat {
    auto layer = layer_fmap(
        [](const Cofree<NatF, Unit> &child) { return cofree_to_fix(child); },
        unwrap_cofree(c));
    return wrap_fix<NatF>(std::move(layer));
}

// Fix<NatF>  <->  Free<NatF, Empty>    (Free f Void ~= Fix f)
constexpr auto fix_to_free(const Nat &n) -> Free<NatF, Empty> {
    auto layer = layer_fmap(
        [](const Nat &child) { return fix_to_free(child); }, unwrap_fix(n));
    return roll_free<NatF>(std::move(layer));
}

constexpr auto free_to_fix(const Free<NatF, Empty> &f) -> Nat {
    // The Pure alternative is never used, so the Roll layer is always present.
    const auto &roll = std::get<1>(f.node);
    auto layer = layer_fmap(
        [](const Free<NatF, Empty> &child) { return free_to_fix(child); }, roll);
    return wrap_fix<NatF>(std::move(layer));
}
// 2ecd0000-0000-4000-8000-000000000001 end

// Peano 3. A function, not a `constexpr` variable: a Nat owns a Box (raw
// new/delete), so it may only live transiently inside a single constant
// evaluation -- the same reason the library folds trees inside static_assert
// rather than storing them.
constexpr auto three() -> Nat {
    return make_succ(make_succ(make_succ(make_zero())));
}

// Both round-trips, folded at compile time.
static_assert(nat_to_int(cofree_to_fix(fix_to_cofree(three()))) == 3);
static_assert(nat_to_int(free_to_fix(fix_to_free(three()))) == 3);

} // namespace

auto main() -> int {
    std::println("Fix -> Cofree<Unit> -> Fix : {}",
                 nat_to_int(cofree_to_fix(fix_to_cofree(three()))));
    std::println("Fix -> Free<Empty>  -> Fix : {}",
                 nat_to_int(free_to_fix(fix_to_free(three()))));
    return 0;
}
