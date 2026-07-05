// Standalone prototype driver for zygo_object.hpp (experiment branch).
//   g++ -std=c++23 -I src zygo_object.proto.cpp -o /tmp/zproto && /tmp/zproto
// Define ZYGO_BAD_INSTANCE to see the projection static_assert fire when a
// per-element-type instance (not element-generic) is threaded.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/zygo_object.hpp>

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <cassert>
#include <functional>
#include <type_traits>
#include <utility>
#include <variant>

namespace proto {

struct Zero {
    friend constexpr auto operator==(const Zero &, const Zero &) -> bool {
        return true;
    }
};
template <typename A>
struct Succ {
    smd::fixpoint::Box<A> pred;
};
template <typename A>
using NatF = std::variant<Zero, Succ<A>>;
using Nat = smd::fixpoint::Fix<NatF>;

using smd::fixpoint::make_box;
using smd::fixpoint::wrap_fix;

constexpr auto make_zero() -> Nat { return wrap_fix<NatF>(NatF<Nat>{Zero{}}); }
constexpr auto make_succ(Nat n) -> Nat {
    return wrap_fix<NatF>(NatF<Nat>{Succ<Nat>{make_box<Nat>(std::move(n))}});
}
constexpr auto nat_of_depth(int depth) -> Nat {
    Nat n = make_zero();
    for (int i = 0; i < depth; ++i)
        n = make_succ(std::move(n));
    return n;
}

// --- Functor instances, ELEMENT-GENERIC (one object maps NatF<A> for any A) ---
// This shape is what a threaded instance must have to serve a multi-site scheme
// like zygo, whose two fmap sites map different element types (NatF<Nat> at the
// fold site, NatF<pair<Helper,Result>> at the projection site).

struct GenericNatFunctor {
    template <typename Fn, typename A>
    constexpr auto fmap(Fn &&fn, const NatF<A> &layer) const {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return std::visit(
            smd::fixpoint::overloaded{
                [](const Zero &) -> NatF<B> { return Zero{}; },
                [&](const Succ<A> &s) -> NatF<B> {
                    return Succ<B>{make_box<B>(std::invoke(fn, *s.pred))};
                },
            },
            layer);
    }
};

// Same, but bumps every *arithmetic* mapped value by +10. At zygo's fold site
// the mapped type is the pair carrier (not arithmetic) so it is untouched; at
// the projection site the mapped type is Helper=int, so helper values are
// perturbed -- an observable proof that the threaded object drives the
// projection site, not a fallback to some registry.
struct BumpHelperFunctor {
    template <typename Fn, typename A>
    constexpr auto fmap(Fn &&fn, const NatF<A> &layer) const {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return std::visit(
            smd::fixpoint::overloaded{
                [](const Zero &) -> NatF<B> { return Zero{}; },
                [&](const Succ<A> &s) -> NatF<B> {
                    B mapped = std::invoke(fn, *s.pred);
                    if constexpr (std::is_arithmetic_v<B>)
                        mapped = mapped + B(10);
                    return Succ<B>{make_box<B>(std::move(mapped))};
                },
            },
            layer);
    }
};

// zygo algebras.
//   helper : NatF<Helper> -> Helper     (depth: Zero->0, Succ(h)->h+1)
//   main   : NatF<pair<Helper,Result>> -> Result  (Zero->0, Succ{h,r}->h+r)
using Helper = int;
using Result = int;
using Carrier = std::pair<Helper, Result>;

constexpr auto helper_alg = [](const NatF<Helper> &layer) -> Helper {
    return std::visit(smd::fixpoint::overloaded{
                          [](const Zero &) { return 0; },
                          [](const Succ<Helper> &s) { return *s.pred + 1; },
                      },
                      layer);
};
constexpr auto main_alg = [](const NatF<Carrier> &layer) -> Result {
    return std::visit(
        smd::fixpoint::overloaded{
            [](const Zero &) { return 0; },
            [](const Succ<Carrier> &s) { return s.pred->first + s.pred->second; },
        },
        layer);
};

// Reference for the un-perturbed zygo: result at depth N is 0+1+...+(N-1).
constexpr auto reference(int depth) -> int { return depth * (depth - 1) / 2; }

#ifdef ZYGO_BAD_INSTANCE
// A per-element-type instance: fmap fixed to one A (NatF<Nat>), not
// element-generic. It cannot serve both of zygo's fmap sites.
struct PerTypeNatFunctor {
    template <typename Fn>
    constexpr auto fmap(Fn &&fn, const NatF<Nat> &layer) const {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const Nat &>>;
        return std::visit(
            smd::fixpoint::overloaded{
                [](const Zero &) -> NatF<B> { return Zero{}; },
                [&](const Succ<Nat> &s) -> NatF<B> {
                    return Succ<B>{make_box<B>(std::invoke(fn, *s.pred))};
                },
            },
            layer);
    }
};
#endif

} // namespace proto

int main() {
    using proto::helper_alg;
    using proto::main_alg;
    using proto::nat_of_depth;
    using proto::reference;
    using smd::fixpoint::experimental::zygo_with;

    proto::GenericNatFunctor generic;
    proto::BumpHelperFunctor bump;

    for (int d = 0; d < 8; ++d) {
        auto tree = nat_of_depth(d);

        // Correct element-generic instance, never registered anywhere:
        // reproduces the textbook zygo result. This is the register-nothing
        // path working end to end at a two-site scheme.
        int got = zygo_with<int, int>(generic, helper_alg, main_alg, tree);
        assert(got == reference(d));

        // Perturbed instance: differs from the reference for d >= 2, proving
        // the threaded object is what drives the projection fmap site.
        int bumped = zygo_with<int, int>(bump, helper_alg, main_alg, tree);
        if (d >= 2)
            assert(bumped != reference(d));
    }

    // Exact spot checks of the perturbed run (hand-traced): depth 3 -> 33,
    // depth 4 -> 66.
    assert((zygo_with<int, int>(bump, helper_alg, main_alg, nat_of_depth(3)) == 33));
    assert((zygo_with<int, int>(bump, helper_alg, main_alg, nat_of_depth(4)) == 66));

    // constexpr in both.
    static_assert(zygo_with<int, int>(proto::GenericNatFunctor{}, helper_alg,
                                      main_alg, nat_of_depth(5)) == reference(5));
    static_assert(zygo_with<int, int>(proto::BumpHelperFunctor{}, helper_alg,
                                      main_alg, nat_of_depth(3)) == 33);

#ifdef ZYGO_BAD_INSTANCE
    // A per-element-type instance (fmap fixed to one A) cannot serve both of
    // zygo's fmap sites; threading it must fail with the projection
    // static_assert, not a wall of "no matching function" notes.
    (void)zygo_with<int, int>(proto::PerTypeNatFunctor{}, helper_alg, main_alg,
                              nat_of_depth(3));
#endif

    return 0;
}
