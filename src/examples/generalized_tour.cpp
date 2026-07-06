// generalized_tour.cpp                                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Demonstrates the generalized schemes (design §7.10/§7.11): gcata, gana,
// ghylo, and the zygo_histo_prepro capstone all recover the "classical" zoo
// (fold_fix, histo, dyna) from nothing but a distributive law plus a
// comonad/monad -- one worker (gcata_worker_t/gana_worker_t, generalized.hpp)
// underneath every one of them. Each section below computes the same answer
// two ways -- the classical scheme directly, and the generalized scheme
// driven by the matching distributive law -- and prints both, naming which
// distributive law produced the generalized side.

#include <smd/concrete/functors.hpp>
#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/dist_laws.hpp>
#include <smd/fixpoint/generalized.hpp>
#include <smd/fixpoint/histo.hpp>
#include <smd/fixpoint/overloaded.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <smd/fixpoint/chrono.hpp>

#include <print>
#include <utility>
#include <variant>
#include <vector>

using smd::concrete::Cons;
using smd::concrete::IntList;
using smd::concrete::IntListF;
using smd::concrete::list_from_vector;
using smd::concrete::Nat;
using smd::concrete::nat_from_int;
using smd::concrete::NatF;
using smd::concrete::Nil;
using smd::concrete::Succ;
using smd::concrete::Zero;
using smd::fixpoint::cata_via_gcata;
using smd::fixpoint::Cofree;
using smd::fixpoint::dist_ana;
using smd::fixpoint::dist_histo;
using smd::fixpoint::dyna;
using smd::fixpoint::extract;
using smd::fixpoint::fold_fix;
using smd::fixpoint::ghylo;
using smd::fixpoint::histo;
using smd::fixpoint::histo_via_gcata;
using smd::fixpoint::layer_fmap;
using smd::fixpoint::make_box;
using smd::fixpoint::overloaded;
using smd::fixpoint::zygo_histo_prepro;

using smd::typeclass::Identity;

namespace {

// ---------------------------------------------------------------------
// Shared fixtures.
// ---------------------------------------------------------------------

constexpr auto nat_count_algebra(const NatF<int> &layer) -> int {
    return std::visit(overloaded{
                          [](const Zero &) { return 0; },
                          [](const Succ<int> &s) { return *s.pred + 1; },
                      },
                      layer);
}

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

constexpr auto countdown(int m) -> NatF<int> {
    if (m <= 0) {
        return Zero{};
    }
    return Succ<int>{make_box<int>(m - 1)};
}

constexpr auto ana_coalgebra_prime(int m) -> NatF<Identity<int>> {
    return layer_fmap([](int x) { return Identity<int>{x}; }, countdown(m));
}

// zygo_histo_prepro's own fixtures (gprepro.t.cpp's behavior test, reused
// verbatim: helper = remaining-length, transformation = take-while-positive,
// main algebra sums the head wherever the tail's remaining length is even).

struct take_while_positive_nat {
    template <class A>
    constexpr auto operator()(const IntListF<A> &layer) const -> IntListF<A> {
        return std::visit(
            overloaded{
                [](const smd::concrete::Nil<int> &n) -> IntListF<A> {
                    return n;
                },
                [](const smd::concrete::Cons<int, A> &c) -> IntListF<A> {
                    if (c.head < 0) {
                        return smd::concrete::Nil<int>{};
                    }
                    return c;
                },
            },
            layer);
    }
};

auto length_helper(const IntListF<int> &layer) -> int {
    return std::visit(
        overloaded{
            [](const smd::concrete::Nil<int> &) { return 0; },
            [](const smd::concrete::Cons<int, int> &c) { return *c.tail + 1; },
        },
        layer);
}

auto even_tail_length_main(
    const IntListF<std::pair<int, Cofree<IntListF, int>>> &layer) -> int {
    return std::visit(
        overloaded{
            [](const smd::concrete::Nil<int> &) { return 0; },
            [](const smd::concrete::Cons<
                int, std::pair<int, Cofree<IntListF, int>>> &c) -> int {
                int tail_length = c.tail->first;
                int rest_sum = extract(c.tail->second);
                if (tail_length % 2 == 0) {
                    return c.head + rest_sum;
                }
                return rest_sum;
            },
        },
        layer);
}

// Prints one "specialized vs generalized" pair and reports whether they
// match; returns false on mismatch so main() can fail loudly instead of
// silently printing wrong output.
auto report(const char *scheme, const char *dist_law, int specialized,
            int generalized) -> bool {
    std::println("{:<28} via {:<14}: specialized = {}, generalized = {}"
                 " ({})",
                 scheme, dist_law, specialized, generalized,
                 specialized == generalized ? "match" : "MISMATCH");
    return specialized == generalized;
}

} // namespace

int main() {
    bool all_ok = true;

    // 29af561b-71b7-4353-9e4b-079584058b77
    // ---------------------------------------------------------------------
    // 1. fold_fix vs cata_via_gcata(dist_cata) -- Nat count, n = 5.
    // ---------------------------------------------------------------------
    {
        Nat nat = nat_from_int(5);
        int specialized = fold_fix<int>(nat_count_algebra, nat);
        int generalized = cata_via_gcata<int>(nat_count_algebra, nat);
        all_ok &= report("fold_fix / cata_via_gcata", "dist_cata", specialized,
                         generalized);
    }

    // ---------------------------------------------------------------------
    // 2. histo vs histo_via_gcata(dist_histo) -- Fibonacci, n = 10.
    // ---------------------------------------------------------------------
    {
        Nat nat = nat_from_int(10);
        int specialized = histo<int>(fib_algebra, nat);
        int generalized = histo_via_gcata<int>(fib_algebra, nat);
        all_ok &= report("histo / histo_via_gcata", "dist_histo", specialized,
                         generalized);
    }

    // ---------------------------------------------------------------------
    // 3. dyna vs ghylo(dist_histo, dist_ana) -- Fibonacci as a fused
    //    refold, n = 10 (design §9's own ghylo/dyna recovery law).
    // ---------------------------------------------------------------------
    {
        int specialized = dyna<int, NatF>(fib_algebra, countdown, 10);
        int generalized = (ghylo<int, Cofree<NatF, int>, NatF, Identity<int>>(
            dist_histo<NatF>, fib_algebra, dist_ana, ana_coalgebra_prime, 10));
        all_ok &= report("dyna / ghylo-as-dyna", "histo+ana", specialized,
                         generalized);
    }
    // 29af561b-71b7-4353-9e4b-079584058b77 end

    // c75ad0ce-3964-4b44-b368-a244f294d623
    // ---------------------------------------------------------------------
    // 4. zygo_histo_prepro capstone -- helper (remaining length) + one-step
    //    Cofree history + a prepro pass (take-while-positive), driven by the
    //    one-off dist_zygo_histo law. [3,4,-1,5] truncates to [3,4]; only "4"
    //    (whose tail's remaining length, 0, is even) is kept: result = 4.
    //    There is no independent "specialized" scheme to compare against
    //    here (this computation only exists because gprepro/dist_zygo_histo
    //    make it expressible at all) -- the pair printed is the capstone's
    //    own result against the hand-checked expectation.
    // ---------------------------------------------------------------------
    {
        IntList list = list_from_vector({3, 4, -1, 5});
        int computed = zygo_histo_prepro<int, int>(length_helper,
                                                   take_while_positive_nat{},
                                                   even_tail_length_main, list);
        all_ok &= report("zygo_histo_prepro capstone", "dist_zygo_histo",
                         /*specialized (hand-checked)=*/4, computed);
    }
    // c75ad0ce-3964-4b44-b368-a244f294d623 end

    return all_ok ? 0 : 1;
}
