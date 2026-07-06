// src/smd/fixpoint/unregistered_functor.t.cpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// The full unregistered-functor contract (design D12): a base functor whose
// functor_typeclass is NEVER specialized, driven entirely by threading an
// element-generic functor object. This exercises the schemes whose *carrier's*
// comonad/monad also consults the functor -- histo (Cofree) and futu (Free) --
// which is why cofree_comonad_with / free_monad_with exist. If any scheme fell
// back to functor_typeclass lookup for UNat, it would be std::false_type and
// fail to compile: green here proves the threading reaches all the way down.

#include <smd/fixpoint/futu.hpp>
#include <smd/fixpoint/generalized.hpp>
#include <smd/fixpoint/histo.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <type_traits>
#include <variant>

namespace {

// A Peano-style base functor deliberately WITHOUT any functor_typeclass<UNat>
// specialization anywhere in the program.
struct UZero {
    friend constexpr auto operator==(const UZero &, const UZero &)
        -> bool = default;
};
template <class A>
struct USucc {
    smd::fixpoint::Box<A> pred;
    friend constexpr auto operator==(const USucc &lhs, const USucc &rhs)
        -> bool {
        return lhs.pred == rhs.pred;
    }
};
template <class A>
using UNat = std::variant<UZero, USucc<A>>;
using UFix = smd::fixpoint::Fix<UNat>;

// The only functor instance for UNat: a local, element-generic object, never
// registered.
struct UNatFunctor {
    template <class Fn, class A>
    constexpr auto fmap(Fn &&fn, const UNat<A> &layer) const {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return std::visit(
            smd::fixpoint::overloaded{
                [](const UZero &) -> UNat<B> { return UZero{}; },
                [&](const USucc<A> &s) -> UNat<B> {
                    return USucc<B>{
                        smd::fixpoint::make_box<B>(std::invoke(fn, *s.pred))};
                },
            },
            layer);
    }
};
inline constexpr UNatFunctor u_functor{};

using smd::fixpoint::make_box;
using smd::fixpoint::wrap_fix;

constexpr auto u_zero() -> UFix { return wrap_fix<UNat>(UNat<UFix>{UZero{}}); }
constexpr auto u_succ(UFix n) -> UFix {
    return wrap_fix<UNat>(UNat<UFix>{USucc<UFix>{make_box<UFix>(std::move(n))}});
}
constexpr auto u_of_depth(int depth) -> UFix {
    UFix n = u_zero();
    for (int i = 0; i < depth; ++i) {
        n = u_succ(std::move(n));
    }
    return n;
}

// F<int> -> int Succ-counter (plain fold algebra).
constexpr auto count_alg(const UNat<int> &layer) -> int {
    return std::visit(smd::fixpoint::overloaded{
                          [](const UZero &) { return 0; },
                          [](const USucc<int> &s) { return *s.pred + 1; },
                      },
                      layer);
}

} // namespace

TEST_CASE("unregistered functor: fold_fix_with counts UNat") {
    using smd::fixpoint::fold_fix_with;
    for (int d = 0; d < 8; ++d) {
        CHECK(fold_fix_with<int>(u_functor, count_alg, u_of_depth(d)) == d);
    }
}

TEST_CASE("unregistered functor: histo_via_gcata_with (Cofree carrier)") {
    using smd::fixpoint::Cofree;
    using smd::fixpoint::extract;
    using smd::fixpoint::histo_via_gcata_with;
    // History-consuming algebra that still just counts: reads extract of the
    // predecessor's Cofree history. Exercises the Cofree comonad's threaded
    // duplicate/fmap over the unregistered UNat.
    auto histo_count = [](const UNat<Cofree<UNat, int>> &layer) -> int {
        return std::visit(
            smd::fixpoint::overloaded{
                [](const UZero &) { return 0; },
                [](const USucc<Cofree<UNat, int>> &s) -> int {
                    return extract(*s.pred) + 1;
                },
            },
            layer);
    };
    for (int d = 0; d < 8; ++d) {
        CHECK(histo_via_gcata_with<int>(u_functor, histo_count,
                                        u_of_depth(d)) == d);
    }
}

TEST_CASE("unregistered functor: futu_via_gana_with (Free carrier)") {
    using smd::fixpoint::fold_fix_with;
    using smd::fixpoint::Free;
    using smd::fixpoint::futu_via_gana_with;
    using smd::fixpoint::layer_fmap;
    using smd::fixpoint::pure_free;
    // One-layer-per-step coalgebra (the futu degeneracy shape) over UNat.
    auto countdown = [](int m) -> UNat<int> {
        if (m <= 0) {
            return UZero{};
        }
        return USucc<int>{make_box<int>(m - 1)};
    };
    auto one_layer = [&](int m) -> UNat<Free<UNat, int>> {
        return layer_fmap(
            u_functor,
            [](int child) -> Free<UNat, int> { return pure_free<UNat>(child); },
            countdown(m));
    };
    for (int d = 0; d < 8; ++d) {
        UFix built = futu_via_gana_with<UNat>(u_functor, one_layer, d);
        CHECK(fold_fix_with<int>(u_functor, count_alg, built) == d);
    }
}
