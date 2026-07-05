// src/smd/fixpoint/fmap.t.cpp                                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/fmap.hpp> // Re-inclusion check

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/overloaded.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <type_traits>
#include <variant>

using smd::fixpoint::Box;
using smd::fixpoint::Fix;
using smd::fixpoint::fold_fix;
using smd::fixpoint::make_box;
using smd::fixpoint::overloaded;
using smd::fixpoint::refold;
using smd::fixpoint::unfold_fix;
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

} // namespace

// design §6.1: functor_typeclass instance for the NatF base functor,
// following the OptionalFunctorImpl pattern but keyed on the layer
// instantiation (D2). Reopening namespace smd::typeclass from a fixpoint
// test TU validates the cross-module pattern every later step relies on.
//
// This specialization must be visible before any code below instantiates
// fold_fix/unfold_fix/refold's lookup overloads for NatF: the
// functor_typeclass variable template's partial specialization has to be
// declared before any use that would instantiate the std::false_type
// primary (see step pitfalls) — hence this block sits directly after the
// layer types and above every scheme call in this TU.
namespace smd::typeclass {

template <typename A>
struct NatFFunctorImpl {
    template <typename Fn>
    constexpr auto fmap(this auto &&, Fn &&fn, const NatF<A> &layer) {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return std::visit(
            smd::fixpoint::overloaded{
                [](const Zero &) -> NatF<B> { return Zero{}; },
                [&](const Succ<A> &s) -> NatF<B> {
                    return Succ<B>{
                        smd::fixpoint::make_box<B>(std::invoke(fn, *s.pred))};
                },
            },
            layer);
    }
};

template <typename A>
struct NatFFunctorMap : Functor<NatFFunctorImpl<A>> {
    using NatFFunctorImpl<A>::fmap;
};

template <typename A>
inline constexpr auto functor_typeclass<NatF<A>> = NatFFunctorMap<A>{};

} // namespace smd::typeclass

namespace {

constexpr auto make_zero() -> Nat { return wrap_fix<NatF>(NatF<Nat>{Zero{}}); }

constexpr auto make_succ(Nat n) -> Nat {
    return wrap_fix<NatF>(NatF<Nat>{Succ<Nat>{make_box<Nat>(std::move(n))}});
}

// Legacy explicit-fmap helper (mirrors recursion_schemes.t.cpp) — used only
// to check the lookup-based overloads against it.
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

constexpr auto count_algebra = [](const NatF<int> &n) -> int {
    return std::visit(overloaded{
                          [](const Zero &) { return 0; },
                          [](const Succ<int> &s) { return *s.pred + 1; },
                      },
                      n);
};

constexpr auto nat_coalgebra = [](int n) -> NatF<int> {
    if (n <= 0)
        return Zero{};
    return Succ<int>{make_box<int>(n - 1)};
};

constexpr auto count_via_lookup(int depth) -> int {
    Nat n = make_zero();
    for (int i = 0; i < depth; ++i) {
        n = make_succ(std::move(n));
    }
    return fold_fix<int>(count_algebra, n);
}

} // namespace

TEST_CASE("fmap - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("fmap - LookupFoldFixNatTwo") {
    auto two = make_succ(make_succ(make_zero()));
    CHECK(fold_fix<int>(count_algebra, two) == 2);
}

TEST_CASE("fmap - LookupUnfoldFixNatFive") {
    auto nat = unfold_fix<NatF>(nat_coalgebra, 5);
    CHECK(fold_fix<int>(count_algebra, nat) == 5);
}

TEST_CASE("fmap - LookupRefoldMatchesExplicitFmapRefold") {
    for (int n = 0; n < 10; ++n) {
        auto via_lookup = refold<int, NatF>(count_algebra, nat_coalgebra, n);
        auto via_explicit =
            refold<int, NatF>(count_algebra, nat_coalgebra, fmap_nat_fn, n);
        CHECK(via_lookup == via_explicit);
    }
}

TEST_CASE("fmap - LookupFoldFixIsConstexpr") {
    static_assert(count_via_lookup(2) == 2);
    CHECK(count_via_lookup(2) == 2);
}
