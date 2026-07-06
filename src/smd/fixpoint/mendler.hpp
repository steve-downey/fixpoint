// src/smd/fixpoint/mendler.hpp                                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_MENDLER
#define INCLUDED_SMD_FIXPOINT_MENDLER

// Mendler-style catamorphism and histomorphism (design §7.7). Every other
// scheme in this module recurses by handing a plain algebra to fold_fix/
// unfold_fix/refold, which fmap the recursive call over a pre-computed layer
// via smd::typeclass::functor_typeclass (design D2/§4). mcata and mhisto work
// the other way around: the algebra itself receives the recursive call (and,
// for mhisto, the unroller) as an explicit callable argument, and calls it on
// whichever children it likes. Consequently **no functor_typeclass instance
// is ever looked up for F** — mcata/mhisto compile and run over any base
// functor F, instanced or not.
//
// Equations:
//   mcata  Φ t = Φ (mcata Φ) (unfix t)
//   mhisto Φ t = Φ (mhisto Φ) unfix (unfix t)
// mhisto's extra `unfix` argument lets the algebra peel further layers off
// any child on demand — course-of-values history without ever building a
// Cofree annotation (contrast with histo.hpp, S07).
//
// The literature type is rank-2 polymorphic in the recursive-call argument
// (`forall y. (y -> c) -> f y -> c`): well-typed algebras may only ever
// apply that argument to a child, never otherwise inspect it. C++ has no
// rank-2 polymorphism and cannot enforce this: a mischievous algebra could
// still reach into a child Fix<F> directly (e.g. via unwrap_fix) and bypass
// the callable entirely. Nothing here stops that — the discipline "the
// recurse/unroll callable is the *only* thing you may do with a child" is by
// convention only. Demonstrating that discipline (and that it costs nothing
// in expressiveness) is the pedagogical point of mendler_eval.cpp.

#include <smd/fixpoint/fix.hpp>

namespace smd::fixpoint {

// d0e3c395-8b95-4d27-a3f4-c9e324b00e0b
/** mcata :: (forall y. (y -> c) -> f y -> c) -> Fix f -> c
 * @tparam Result the fold's result type (design D5: explicit)
 * @param phi   called as phi(recurse, layer): `recurse` is a callable
 *              const Fix<F>& -> Result (mcata Φ, partially applied);
 *              `layer` is const F<Fix<F>>&, the tree's outermost layer.
 *              phi decides which children (if any) to fold via `recurse` —
 *              no functor_typeclass<F<...>> instance is consulted here.
 */
template <class Result, template <class> class F, class MAlgebra>
constexpr auto mcata(const MAlgebra &phi, const Fix<F> &tree) -> Result {
    auto recurse = [&phi](const Fix<F> &child) -> Result {
        return mcata<Result, F>(phi, child);
    };
    return phi(recurse, unwrap_fix(tree));
}
// d0e3c395-8b95-4d27-a3f4-c9e324b00e0b end

// 24b61364-25c7-43a5-b18b-842bf88b0022
/** mhisto :: (forall y. (y -> c) -> (y -> f y) -> f y -> c) -> Fix f -> c
 * @tparam Result the fold's result type (design D5: explicit)
 * @param phi   called as phi(recurse, unroll, layer): `recurse` is
 *              const Fix<F>& -> Result (mhisto Φ, partially applied);
 *              `unroll` is const Fix<F>& -> const F<Fix<F>>& (i.e.
 *              unwrap_fix) — it returns a reference *into* the tree, which
 *              is fine here since the tree the fold started from outlives
 *              the whole call; `layer` is const F<Fix<F>>&. phi may call
 *              `unroll` on any child it holds to look further down without
 *              committing to folding it via `recurse` — no
 *              functor_typeclass<F<...>> instance is consulted here either.
 */
template <class Result, template <class> class F, class MAlgebra>
constexpr auto mhisto(const MAlgebra &phi, const Fix<F> &tree) -> Result {
    auto recurse = [&phi](const Fix<F> &child) -> Result {
        return mhisto<Result, F>(phi, child);
    };
    auto unroll = [](const Fix<F> &child) -> const F<Fix<F>> & {
        return unwrap_fix(child);
    };
    return phi(recurse, unroll, unwrap_fix(tree));
}
// 24b61364-25c7-43a5-b18b-842bf88b0022 end

} // namespace smd::fixpoint

#endif
