// src/smd/fixpoint/recursion_schemes.hpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_RECURSION_SCHEMES
#define INCLUDED_SMD_FIXPOINT_RECURSION_SCHEMES

#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/fmap.hpp>

namespace smd::fixpoint {

template <typename Result, template <typename> class F, typename Algebra,
          typename FMap>
constexpr auto fold_fix(const Algebra &algebra, const FMap &fmap_fn,
                        const Fix<F> &tree) -> Result {
    const auto &layer = unwrap_fix(tree);
    auto evaluated = fmap_fn(
        [&](const Fix<F> &child) -> Result {
            return fold_fix<Result>(algebra, fmap_fn, child);
        },
        layer);
    return algebra(evaluated);
}

template <template <typename> class F, typename Coalgebra, typename FMap,
          typename Seed>
constexpr auto unfold_fix(const Coalgebra &coalgebra, const FMap &fmap_fn,
                          const Seed &seed) -> Fix<F> {
    auto layer = coalgebra(seed);
    auto expanded = fmap_fn(
        [&](const Seed &child) -> Fix<F> {
            return unfold_fix<F>(coalgebra, fmap_fn, child);
        },
        layer);
    return wrap_fix<F>(std::move(expanded));
}

template <typename Result, template <typename> class F, typename Algebra,
          typename Coalgebra, typename FMap, typename Seed>
constexpr auto refold(const Algebra &algebra, const Coalgebra &coalgebra,
                      const FMap &fmap_fn, const Seed &seed) -> Result {
    auto layer = coalgebra(seed);
    auto evaluated = fmap_fn(
        [&](const Seed &child) -> Result {
            return refold<Result, F>(algebra, coalgebra, fmap_fn, child);
        },
        layer);
    return algebra(evaluated);
}

template <typename Result, template <typename> class F, typename Algebra,
          typename FMap>
[[deprecated("use fold_fix")]]
auto cata(const Algebra &algebra, const FMap &fmap_fn, const Fix<F> &tree)
    -> Result {
    return fold_fix<Result>(algebra, fmap_fn, tree);
}

// Lookup-based overloads (design §6.2): dispatch fmap through
// smd::typeclass::functor_typeclass via layer_fmap (design §4) instead of
// taking an explicit fmap_fn.

template <typename Result, template <typename> class F, typename Algebra>
constexpr auto fold_fix(const Algebra &algebra, const Fix<F> &tree)
    -> Result {
    const auto &layer = unwrap_fix(tree);
    auto evaluated = layer_fmap(
        [&](const Fix<F> &child) -> Result {
            return fold_fix<Result>(algebra, child);
        },
        layer);
    return algebra(evaluated);
}

template <template <typename> class F, typename Coalgebra, typename Seed>
constexpr auto unfold_fix(const Coalgebra &coalgebra, const Seed &seed)
    -> Fix<F> {
    auto layer = coalgebra(seed);
    auto expanded = layer_fmap(
        [&](const Seed &child) -> Fix<F> {
            return unfold_fix<F>(coalgebra, child);
        },
        layer);
    return wrap_fix<F>(std::move(expanded));
}

template <typename Result, template <typename> class F, typename Algebra,
          typename Coalgebra, typename Seed>
constexpr auto refold(const Algebra &algebra, const Coalgebra &coalgebra,
                      const Seed &seed) -> Result {
    auto layer = coalgebra(seed);
    auto evaluated = layer_fmap(
        [&](const Seed &child) -> Result {
            return refold<Result, F>(algebra, coalgebra, child);
        },
        layer);
    return algebra(evaluated);
}

// Explicit-instance overloads (OVERLOAD variant): thread a functor instance
// by value, reusing the *same* scheme name rather than a distinct `_with`.
// The instance is the first argument. Because it shares its arity with the
// explicit-fmap_fn overloads above (fold_fix(algebra, fmap_fn, tree) is also
// 3-arg), the two must be told apart -- and since this IS overload selection,
// that means a requires-clause, not a static_assert. functor_instance_for
// constrains only this overload; constraint partial ordering then prefers it
// over the unconstrained fmap_fn overload when both deduce (the fmap_fn
// overload survives for a non-instance first argument, which fails this
// constraint). The cost paid for name reuse: a mistyped instance now fails
// with "no matching call to fold_fix" instead of a named static_assert.

template <typename Result, template <typename> class F, class Functor,
          typename Algebra>
    requires functor_instance_for<Functor, Result (*)(const Fix<F> &),
                                  F<Fix<F>>>
constexpr auto fold_fix(const Functor &functor, const Algebra &algebra,
                        const Fix<F> &tree) -> Result {
    auto evaluated = functor.fmap(
        [&](const Fix<F> &child) -> Result {
            return fold_fix<Result>(functor, algebra, child);
        },
        unwrap_fix(tree));
    return algebra(evaluated);
}

template <template <typename> class F, class Functor, typename Coalgebra,
          typename Seed>
    requires functor_instance_for<
        Functor, Fix<F> (*)(const Seed &),
        std::remove_cvref_t<std::invoke_result_t<Coalgebra, const Seed &>>>
constexpr auto unfold_fix(const Functor &functor, const Coalgebra &coalgebra,
                          const Seed &seed) -> Fix<F> {
    auto layer = coalgebra(seed);
    auto expanded = functor.fmap(
        [&](const Seed &child) -> Fix<F> {
            return unfold_fix<F>(functor, coalgebra, child);
        },
        layer);
    return wrap_fix<F>(std::move(expanded));
}

template <typename Result, template <typename> class F, class Functor,
          typename Algebra, typename Coalgebra, typename Seed>
    requires functor_instance_for<
        Functor, Result (*)(const Seed &),
        std::remove_cvref_t<std::invoke_result_t<Coalgebra, const Seed &>>>
constexpr auto refold(const Functor &functor, const Algebra &algebra,
                      const Coalgebra &coalgebra, const Seed &seed) -> Result {
    auto layer = coalgebra(seed);
    auto evaluated = functor.fmap(
        [&](const Seed &child) -> Result {
            return refold<Result, F>(functor, algebra, coalgebra, child);
        },
        layer);
    return algebra(evaluated);
}

} // namespace smd::fixpoint

#endif
