// src/smd/fixpoint/mutu.hpp                                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_MUTU
#define INCLUDED_SMD_FIXPOINT_MUTU

// Mutumorphism (design §7.3, Fokkinga): two mutually recursive folds,
// packaged as a single fold_fix over a pair carrier (the banana-split
// theorem) — alg_a and alg_b each see the *other's* result (and their own)
// at every child position via the shared std::pair<A, B> carrier.

#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <utility>

namespace smd::fixpoint {

/** mutu :: (f (a, b) -> a) -> (f (a, b) -> b) -> t -> (a, b)
 *
 * Equation: mutu f g = fold_fix(λx. pair(f(x), g(x)))
 *
 * @tparam A first fold's carrier (D5: explicit)
 * @tparam B second fold's carrier (D5: explicit)
 * @param alg_a F<std::pair<A, B>> -> A
 * @param alg_b F<std::pair<A, B>> -> B
 */
template <class A, class B, template <class> class F, class AlgA, class AlgB>
constexpr auto mutu(const AlgA &alg_a, const AlgB &alg_b,
                    const Fix<F> &tree) -> std::pair<A, B> {
    using Carrier = std::pair<A, B>;
    auto combined = [&](const F<Carrier> &layer) -> Carrier {
        return Carrier{alg_a(layer), alg_b(layer)};
    };
    return fold_fix<Carrier>(combined, tree);
}

/** mutu threading an explicit functor instance (the `_with` form, design
 * D12); delegates to fold_fix_with, which validates the instance. */
template <class A, class B, template <class> class F, class Functor,
          class AlgA, class AlgB>
constexpr auto mutu_with(const Functor &functor, const AlgA &alg_a,
                         const AlgB &alg_b, const Fix<F> &tree)
    -> std::pair<A, B> {
    using Carrier = std::pair<A, B>;
    auto combined = [&](const F<Carrier> &layer) -> Carrier {
        return Carrier{alg_a(layer), alg_b(layer)};
    };
    return fold_fix_with<Carrier>(functor, combined, tree);
}

} // namespace smd::fixpoint

#endif
