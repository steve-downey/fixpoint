// Standalone prototype driver for fold_fix_object.hpp (experiment branch).
// Compile directly, no Catch2/CMake needed:
//   g++ -std=c++23 -I src fold_fix_object.proto.cpp -o /tmp/proto && /tmp/proto
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/fold_fix_object.hpp>

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <cassert>
#include <functional>
#include <type_traits>
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

} // namespace proto

// Registered functor instance for NatF (the ordinary, "count by 1" behaviour).
namespace smd::typeclass {
template <typename A>
struct NatFFunctorImpl {
    template <typename Fn>
    constexpr auto fmap(this auto &&, Fn &&fn, const proto::NatF<A> &layer) {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return std::visit(
            smd::fixpoint::overloaded{
                [](const proto::Zero &) -> proto::NatF<B> {
                    return proto::Zero{};
                },
                [&](const proto::Succ<A> &s) -> proto::NatF<B> {
                    return proto::Succ<B>{
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
inline constexpr auto functor_typeclass<proto::NatF<A>> = NatFFunctorMap<A>{};
} // namespace smd::typeclass

namespace proto {

using smd::fixpoint::make_box;
using smd::fixpoint::wrap_fix;

constexpr auto make_zero() -> Nat {
    return wrap_fix<NatF>(NatF<Nat>{Zero{}});
}
constexpr auto make_succ(Nat n) -> Nat {
    return wrap_fix<NatF>(NatF<Nat>{Succ<Nat>{make_box<Nat>(std::move(n))}});
}
constexpr auto nat_of_depth(int depth) -> Nat {
    Nat n = make_zero();
    for (int i = 0; i < depth; ++i)
        n = make_succ(std::move(n));
    return n;
}

constexpr auto count_algebra = [](const NatF<int> &layer) -> int {
    return std::visit(smd::fixpoint::overloaded{
                          [](const Zero &) { return 0; },
                          [](const Succ<int> &s) { return *s.pred + 1; },
                      },
                      layer);
};

// An UNREGISTERED functor instance: same shape, but each mapped child is
// bumped by +10. If it threads through every recursion level, a depth-N fold
// yields 11*N (each level contributes the +10 bump plus the algebra's +1);
// if only the top level used it, the numbers would not compound.
struct BumpTenFunctor {
    template <typename Fn, typename A>
    constexpr auto fmap(Fn &&fn, const NatF<A> &layer) const {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return std::visit(
            smd::fixpoint::overloaded{
                [](const Zero &) -> NatF<B> { return Zero{}; },
                [&](const Succ<A> &s) -> NatF<B> {
                    return Succ<B>{make_box<B>(std::invoke(fn, *s.pred) + 10)};
                },
            },
            layer);
    }
};

} // namespace proto

int main() {
    using smd::fixpoint::experimental::fold_fix;

    // Mode 1 — implicit lookup: identical call site to the free function.
    for (int d = 0; d < 8; ++d) {
        int got = fold_fix<int>(proto::count_algebra, proto::nat_of_depth(d));
        assert(got == d);
    }

    // Mode 3 — explicit unregistered instance, threaded through recursion.
    proto::BumpTenFunctor bump;
    for (int d = 0; d < 8; ++d) {
        int got = fold_fix<int>(bump, proto::count_algebra, proto::nat_of_depth(d));
        assert(got == 11 * d); // compounding proves per-level threading
    }

    // constexpr: both modes usable in constant expressions.
    static_assert(fold_fix<int>(proto::count_algebra, proto::nat_of_depth(5)) == 5);
    static_assert(
        fold_fix<int>(proto::BumpTenFunctor{}, proto::count_algebra,
                      proto::nat_of_depth(3)) == 33);

    // Alternative: thread-by-value variant, same results, no carrier struct.
    using smd::fixpoint::experimental::fold_fix_threaded;
    for (int d = 0; d < 8; ++d) {
        assert(fold_fix_threaded<int>(proto::count_algebra,
                                      proto::nat_of_depth(d)) == d);
        assert(fold_fix_threaded<int>(bump, proto::count_algebra,
                                      proto::nat_of_depth(d)) == 11 * d);
    }
    static_assert(fold_fix_threaded<int>(proto::BumpTenFunctor{},
                                         proto::count_algebra,
                                         proto::nat_of_depth(3)) == 33);

    return 0;
}
