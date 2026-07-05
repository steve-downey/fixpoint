// src/smd/fixpoint/mendler.t.cpp                                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/mendler.hpp>
#include <smd/fixpoint/mendler.hpp> // Re-inclusion check

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/functors.hpp>
#include <smd/fixpoint/overloaded.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <catch2/catch_test_macros.hpp>

#include <variant>

using smd::fixpoint::Add;
using smd::fixpoint::Box;
using smd::fixpoint::Const;
using smd::fixpoint::Expr;
using smd::fixpoint::ExprF;
using smd::fixpoint::Fix;
using smd::fixpoint::fold_fix;
using smd::fixpoint::layer_fmap;
using smd::fixpoint::make_box;
using smd::fixpoint::mcata;
using smd::fixpoint::mhisto;
using smd::fixpoint::Mul;
using smd::fixpoint::Nat;
using smd::fixpoint::nat_from_int;
using smd::fixpoint::NatF;
using smd::fixpoint::overloaded;
using smd::fixpoint::Succ;
using smd::fixpoint::wrap_fix;
using smd::fixpoint::Zero;
using smd::fixpoint::add_node;
using smd::fixpoint::const_node;
using smd::fixpoint::mul_node;

TEST_CASE("mendler - HeaderIsIdempotent") { REQUIRE(true); }

// ---------------------------------------------------------------------
// Law (design §9): mcata(λ(rec,l). φ(fmapF(rec,l))) ≡ fold_fix(φ) — an
// mcata algebra that immediately fmaps its recurse callable over the layer
// and defers to a plain algebra degenerates to a plain fold_fix. Checked on
// both Nat and ExprF (the step file asks for both fixtures).
// ---------------------------------------------------------------------

namespace {

auto count_algebra(const NatF<int> &layer) -> int {
    return std::visit(overloaded{
                           [](const Zero &) { return 0; },
                           [](const Succ<int> &s) { return *s.pred + 1; },
                       },
                       layer);
}

auto eval_plain_algebra(const ExprF<int> &layer) -> int {
    return std::visit(overloaded{
                           [](const Const<int> &c) { return c.val; },
                           [](const Add<int> &a) { return *a.left + *a.right; },
                           [](const Mul<int> &m) { return *m.left * *m.right; },
                       },
                       layer);
}

} // namespace

TEST_CASE("mcata law: mcata degenerates to fold_fix (Nat)") {
    auto mendler_algebra = [](auto recurse, const NatF<Nat> &layer) -> int {
        return count_algebra(layer_fmap(recurse, layer));
    };
    for (int n = 0; n <= 10; ++n) {
        Nat nat = nat_from_int(n);
        CHECK(mcata<int, NatF>(mendler_algebra, nat) ==
              fold_fix<int>(count_algebra, nat));
    }
}

TEST_CASE("mcata law: mcata degenerates to fold_fix (ExprF)") {
    auto mendler_algebra = [](auto recurse, const ExprF<Expr> &layer) -> int {
        return eval_plain_algebra(layer_fmap(recurse, layer));
    };
    Expr e =
        add_node(mul_node(const_node(2), const_node(3)), const_node(4));
    CHECK(mcata<int, ExprF>(mendler_algebra, e) ==
          fold_fix<int>(eval_plain_algebra, e));
}

// ---------------------------------------------------------------------
// Behavior: ExprF eval via mcata, written *without* fmap — the algebra
// pattern-matches the layer and calls `recurse` directly on each boxed
// child, rather than fmap-ing a precomputed layer (design §10).
// ---------------------------------------------------------------------

namespace {

auto mendler_eval_algebra = [](auto recurse, const ExprF<Expr> &layer)
    -> int {
    return std::visit(
        overloaded{
            [](const Const<Expr> &c) { return c.val; },
            [&](const Add<Expr> &a) -> int {
                return recurse(*a.left) + recurse(*a.right);
            },
            [&](const Mul<Expr> &m) -> int {
                return recurse(*m.left) * recurse(*m.right);
            },
        },
        layer);
};

} // namespace

TEST_CASE("mcata behavior: ExprF eval via mcata, no fmap") {
    Expr e =
        add_node(mul_node(const_node(2), const_node(3)), const_node(4));
    CHECK(mcata<int, ExprF>(mendler_eval_algebra, e) == 10);
}

// ---------------------------------------------------------------------
// No-instance proof (the motivating property of this whole step):
// OpaqueF is a test-local base functor with NO functor_typeclass
// specialization anywhere in this translation unit (grep this file for
// "functor_typeclass" — the only hits are #includes/using-declarations
// pulling in *other* functors' instances, never one for OpaqueF). mcata
// still folds a Fix<OpaqueF> tree correctly, proving it never looks the
// instance up. Built by hand with wrap_fix/make_box (not unfold_fix, which
// itself would require a functor_typeclass<OpaqueF<...>> instance to fmap
// through — using it here would silently defeat the whole point).
// ---------------------------------------------------------------------

namespace {

template <class A>
struct OpaqueZero {
    friend constexpr auto operator==(const OpaqueZero &, const OpaqueZero &)
        -> bool = default;
};

template <class A>
struct OpaqueSucc {
    Box<A> pred;

    friend constexpr auto operator==(const OpaqueSucc &, const OpaqueSucc &)
        -> bool = default;
};

template <class A>
using OpaqueF = std::variant<OpaqueZero<A>, OpaqueSucc<A>>;

using OpaqueNat = Fix<OpaqueF>;

constexpr auto make_opaque(int n) -> OpaqueNat {
    if (n <= 0) {
        return wrap_fix<OpaqueF>(OpaqueF<OpaqueNat>{OpaqueZero<OpaqueNat>{}});
    }
    return wrap_fix<OpaqueF>(OpaqueF<OpaqueNat>{
        OpaqueSucc<OpaqueNat>{make_box<OpaqueNat>(make_opaque(n - 1))}});
}

auto opaque_count_algebra = [](auto recurse, const OpaqueF<OpaqueNat> &layer)
    -> int {
    return std::visit(
        overloaded{
            [](const OpaqueZero<OpaqueNat> &) { return 0; },
            [&](const OpaqueSucc<OpaqueNat> &s) -> int {
                return recurse(*s.pred) + 1;
            },
        },
        layer);
};

} // namespace

TEST_CASE(
    "mcata no-instance proof: OpaqueF has no functor_typeclass instance") {
    OpaqueNat five = make_opaque(5);
    CHECK(mcata<int, OpaqueF>(opaque_count_algebra, five) == 5);
}

// ---------------------------------------------------------------------
// Behavior: Fibonacci via mhisto on Nat (design §7.7/§9's mhisto-fib) — no
// Cofree anywhere. At a node for n (Succ pred, pred : Nat for n-1), `unroll`
// peels pred's own layer without committing to folding it: if that layer is
// Zero, n-1 == 0 so n == 1 (fib(1) == 1, pinned explicitly); otherwise it is
// Succ(pred2) with pred2 : Nat for n-2, and fib(n) == recurse(pred) [fib(n-1)]
// + recurse(*pred2.pred) [fib(n-2)]. fib(0) == 0 is the outer Zero case,
// pinned explicitly too.
// ---------------------------------------------------------------------

namespace {

auto mhisto_fib_algebra = [](auto recurse, auto unroll,
                              const NatF<Nat> &layer) -> int {
    return std::visit(
        overloaded{
            [](const Zero &) { return 0; }, // fib(0) == 0
            [&](const Succ<Nat> &s) -> int {
                const Nat &pred = *s.pred;                // n - 1
                const NatF<Nat> &pred_layer = unroll(pred);
                return std::visit(
                    overloaded{
                        [&](const Zero &) { return 1; }, // fib(1) == 1
                        [&](const Succ<Nat> &s2) -> int {
                            return recurse(pred) + recurse(*s2.pred);
                        },
                    },
                    pred_layer);
            },
        },
        layer);
};

auto mhisto_fib(int n) -> int {
    return mhisto<int, NatF>(mhisto_fib_algebra, nat_from_int(n));
}

} // namespace

TEST_CASE("mhisto behavior: Fibonacci via mhisto on Nat, 0..10") {
    int expected[] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55};
    for (int n = 0; n <= 10; ++n) {
        CHECK(mhisto_fib(n) == expected[n]);
    }
}

// ---------------------------------------------------------------------
// constexpr (design D10): one static_assert per scheme, self-contained
// local lambdas (house pattern, e.g. histo.t.cpp's *_constexpr_smoke).
// ---------------------------------------------------------------------

namespace {

constexpr auto mcata_constexpr_smoke() -> bool {
    auto plain_count_algebra = [](const NatF<int> &layer) -> int {
        return std::visit(overloaded{
                               [](const Zero &) { return 0; },
                               [](const Succ<int> &s) { return *s.pred + 1; },
                           },
                           layer);
    };
    auto mendler_algebra = [&](auto recurse, const NatF<Nat> &layer) -> int {
        return plain_count_algebra(layer_fmap(recurse, layer));
    };
    return mcata<int, NatF>(mendler_algebra, nat_from_int(4)) == 4;
}

constexpr auto mhisto_constexpr_smoke() -> bool {
    auto algebra = [](auto recurse, auto unroll, const NatF<Nat> &layer)
        -> int {
        return std::visit(
            overloaded{
                [](const Zero &) { return 0; },
                [&](const Succ<Nat> &s) -> int {
                    const Nat &pred = *s.pred;
                    const NatF<Nat> &pred_layer = unroll(pred);
                    return std::visit(
                        overloaded{
                            [&](const Zero &) { return 1; },
                            [&](const Succ<Nat> &s2) -> int {
                                return recurse(pred) + recurse(*s2.pred);
                            },
                        },
                        pred_layer);
                },
            },
            layer);
    };
    return mhisto<int, NatF>(algebra, nat_from_int(6)) == 8;
}

} // namespace

static_assert(mcata_constexpr_smoke());
static_assert(mhisto_constexpr_smoke());
