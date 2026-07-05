// src/smd/fixpoint/apo.hpp                                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_APO
#define INCLUDED_SMD_FIXPOINT_APO

// Apomorphism (design §7.2). Like unfold_fix, but the coalgebra may
// short-circuit any branch by grafting an already-built subtree instead of
// continuing to unfold (D4: Left = finished subtree, Right = keep
// unfolding — same side assignment as the Haskell `Either` this is
// transcribed from).

#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/fmap.hpp>

#include <smd/typeclass/either.hpp>

#include <utility>

namespace smd::fixpoint {

/** apo :: (a -> f (Either t a)) -> a -> t
 *
 * Equation: apo ψ = fix ∘ fmapF (either id (apo ψ)) ∘ ψ
 *
 * Written with `match` (design §5.2), the worker never inspects whether
 * Left holds `Fix<F>` or `const Fix<F>&` — so coalgebras may return
 * `F<either<const Fix<F>&, Seed>>` to graft an existing subtree with no
 * intermediate copy (the zero-copy graft, §7.2): the Left branch simply
 * returns its referent, and returning a `const Fix<F>&` from a
 * `Fix<F>`-returning function is an ordinary copy-construction, not a
 * move-from-const&.
 *
 * @param coalgebra Seed -> F<either<Fix<F>, Seed>> (or the reference-Left
 *   variant above)
 */
template <template <class> class F, class Coalgebra, class Seed>
constexpr auto apo(const Coalgebra &coalgebra, const Seed &seed) -> Fix<F> {
    auto layer = coalgebra(seed);
    auto expanded = layer_fmap(
        [&](const auto &step) -> Fix<F> {
            return smd::typeclass::match(
                step,
                [](const auto &subtree) -> Fix<F> { return subtree; },
                [&](const auto &next_seed) -> Fix<F> {
                    return apo<F>(coalgebra, next_seed);
                });
        },
        layer);
    return wrap_fix<F>(std::move(expanded));
}

} // namespace smd::fixpoint

#endif
