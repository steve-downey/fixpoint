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

// 039f26eb-8473-4ebb-bb47-36de2dfe9a1a
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
// 039f26eb-8473-4ebb-bb47-36de2dfe9a1a end

} // namespace smd::fixpoint

#endif
