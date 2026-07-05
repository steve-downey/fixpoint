// src/smd/fixpoint/recursion_schemes.t.cpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/recursion_schemes.hpp>
#include <smd/fixpoint/recursion_schemes.hpp> // Re-inclusion check

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <type_traits>
#include <variant>

using smd::fixpoint::Box;
using smd::fixpoint::Fix;
using smd::fixpoint::fold_fix;
using smd::fixpoint::fold_fix_with;
using smd::fixpoint::make_box;
using smd::fixpoint::overloaded;
using smd::fixpoint::refold;
using smd::fixpoint::refold_with;
using smd::fixpoint::unfold_fix;
using smd::fixpoint::unfold_fix_with;
using smd::fixpoint::wrap_fix;

namespace {

struct Zero {};

template <typename A>
struct Succ {
    Box<A> pred;
};

template <typename A>
using NatF = std::variant<Zero, Succ<A>>;

using Nat = Fix<NatF>;

auto make_zero() -> Nat { return wrap_fix<NatF>(NatF<Nat>{Zero{}}); }

auto make_succ(Nat n) -> Nat {
    return wrap_fix<NatF>(NatF<Nat>{Succ<Nat>{make_box<Nat>(std::move(n))}});
}

template <typename A, typename F>
auto fmap_nat(F &&f, const NatF<A> &nat) {
    using B = std::invoke_result_t<F, const A &>;
    return std::visit(overloaded{
                          [](const Zero &) -> NatF<B> { return Zero{}; },
                          [&f](const Succ<A> &s) -> NatF<B> {
                              return Succ<B>{
                                  make_box<B>(std::invoke(f, *s.pred))};
                          },
                      },
                      nat);
}

auto fmap_nat_fn = [](auto &&f, const auto &nat) {
    return fmap_nat(std::forward<decltype(f)>(f), nat);
};

// An element-generic functor *object* for NatF, deliberately NOT registered in
// functor_typeclass. The `_with` schemes accept it by value, so it drives the
// recursion with nothing registered anywhere -- the thread-by-value contract.
struct NatFunctor {
    template <typename Fn, typename A>
    auto fmap(Fn &&fn, const NatF<A> &nat) const {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return std::visit(overloaded{
                              [](const Zero &) -> NatF<B> { return Zero{}; },
                              [&](const Succ<A> &s) -> NatF<B> {
                                  return Succ<B>{
                                      make_box<B>(std::invoke(fn, *s.pred))};
                              },
                          },
                          nat);
    }
};
inline constexpr NatFunctor nat_functor{};

auto count_algebra = [](const NatF<int> &n) -> int {
    return std::visit(overloaded{
                          [](const Zero &) { return 0; },
                          [](const Succ<int> &s) { return *s.pred + 1; },
                      },
                      n);
};

auto nat_coalgebra = [](int n) -> NatF<int> {
    if (n <= 0)
        return Zero{};
    return Succ<int>{make_box<int>(n - 1)};
};

} // namespace

TEST_CASE("Cata - NatZero") {
    auto zero = make_zero();
    CHECK(fold_fix<int>(count_algebra, fmap_nat_fn, zero) == 0);
}

TEST_CASE("Cata - NatTwo") {
    auto two = make_succ(make_succ(make_zero()));
    CHECK(fold_fix<int>(count_algebra, fmap_nat_fn, two) == 2);
}

TEST_CASE("Cata - NatFive") {
    auto n = make_zero();
    for (int i = 0; i < 5; ++i) {
        n = make_succ(std::move(n));
    }
    CHECK(fold_fix<int>(count_algebra, fmap_nat_fn, n) == 5);
}

TEST_CASE("Cata - NatCustomAlgebra") {
    auto three = make_succ(make_succ(make_succ(make_zero())));

    auto bool_algebra = [](const NatF<bool> &n) -> bool {
        return std::visit(overloaded{
                              [](const Zero &) { return true; },
                              [](const Succ<bool> &s) { return !*s.pred; },
                          },
                          n);
    };

    CHECK(fold_fix<bool>(bool_algebra, fmap_nat_fn, three) == false);
}

TEST_CASE("UnfoldFix - NatFromZero") {
    auto nat = unfold_fix<NatF>(nat_coalgebra, fmap_nat_fn, 0);
    CHECK(fold_fix<int>(count_algebra, fmap_nat_fn, nat) == 0);
}

TEST_CASE("UnfoldFix - NatFromFive") {
    auto nat = unfold_fix<NatF>(nat_coalgebra, fmap_nat_fn, 5);
    CHECK(fold_fix<int>(count_algebra, fmap_nat_fn, nat) == 5);
}

TEST_CASE("Refold - NatZero") {
    CHECK(refold<int, NatF>(count_algebra, nat_coalgebra, fmap_nat_fn, 0) == 0);
}

TEST_CASE("Refold - NatFive") {
    CHECK(refold<int, NatF>(count_algebra, nat_coalgebra, fmap_nat_fn, 5) == 5);
}

TEST_CASE("Refold - EquivalentToFoldOfUnfold") {
    for (int n = 0; n < 10; ++n) {
        auto via_tree =
            fold_fix<int>(count_algebra, fmap_nat_fn,
                          unfold_fix<NatF>(nat_coalgebra, fmap_nat_fn, n));
        auto via_refold =
            refold<int, NatF>(count_algebra, nat_coalgebra, fmap_nat_fn, n);
        CHECK(via_tree == via_refold);
    }
}

// --- _with forms: thread an unregistered functor instance by value ---------

TEST_CASE("FoldFixWith - threads an unregistered instance") {
    auto n = make_zero();
    for (int i = 0; i < 5; ++i) {
        n = make_succ(std::move(n));
    }
    CHECK(fold_fix_with<int>(nat_functor, count_algebra, n) == 5);
}

TEST_CASE("UnfoldFixWith - NatFromFive") {
    auto nat = unfold_fix_with<NatF>(nat_functor, nat_coalgebra, 5);
    CHECK(fold_fix_with<int>(nat_functor, count_algebra, nat) == 5);
}

TEST_CASE("RefoldWith - MatchesFmapFnRefold") {
    for (int n = 0; n < 10; ++n) {
        auto via_with =
            refold_with<int, NatF>(nat_functor, count_algebra, nat_coalgebra, n);
        auto via_fmap_fn =
            refold<int, NatF>(count_algebra, nat_coalgebra, fmap_nat_fn, n);
        CHECK(via_with == via_fmap_fn);
    }
}

TEST_CASE("FoldFixWith - MatchesLookupForm") {
    // nat_functor behaves identically to the registered instance would; the
    // _with result matches the fmap_fn-threaded reference at every depth.
    auto n = make_zero();
    for (int depth = 0; depth < 8; ++depth) {
        CHECK(fold_fix_with<int>(nat_functor, count_algebra, n) ==
              fold_fix<int>(count_algebra, fmap_nat_fn, n));
        n = make_succ(std::move(n));
    }
}
