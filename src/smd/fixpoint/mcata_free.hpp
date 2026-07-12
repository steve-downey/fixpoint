// src/smd/fixpoint/mcata_free.hpp                                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_MCATA_FREE
#define INCLUDED_SMD_FIXPOINT_MCATA_FREE

// Mendler-style catamorphism over Free (design doc's Section 5 forward
// reference; FD4): a NEW consuming fold, distinct from mendler.hpp's mcata
// (Fix-only, const&). Free<F,A> has two constructors -- Pure a | Roll layer
// -- so its Mendler algebra is two callables, not mcata's one: pure_alg
// handles the Pure leaf, phi handles a Roll layer exactly the way mcata's
// own phi does (it receives the recursive call and the layer, and decides
// which children -- if any -- to fold via that call). Exactly as
// mendler.hpp documents for mcata/mhisto: **no functor_typeclass<F>
// instance is ever looked up here** -- phi applies `recurse` to whichever
// children it likes, by convention, not by any enforced rank-2 discipline
// (C++ cannot express that any more here than mendler.hpp's own mcata can).
//
// Equations:
//   mcata_free (p, phi) (Pure a)   = p a
//   mcata_free (p, phi) (Roll l)   = phi (mcata_free (p, phi)) l
//
// Consuming, and BY VALUE (not const&, unlike mcata): this fold exists for
// the S07 interpreter, where Result may be a coroutine type (e.g.
// beman::task<A> -- this header itself has no beman dependency and knows
// nothing about that). A coroutine's frame only safely owns values that
// arrive as genuine, direct parameters -- moved/copied into the frame at
// the point of the call -- never a value merely *referenced* from an
// enclosing native stack frame, which may already be gone by the time the
// coroutine resumes past a suspension point (the classic "coroutine +
// capturing lambda" lifetime hazard: a capturing lambda's closure is a
// temporary, destroyed at the end of the full-expression that created it,
// while the coroutine it produced may still be suspended). mcata's Fix
// traversal never hits this: it is synchronous top-to-bottom, so a const&
// algebra never outlives the single native call stack holding it. Free's
// traversal makes no such promise once Result's own callables may suspend,
// so pure_alg/phi are taken BY VALUE here, and `recurse` -- the callable
// phi is handed -- closes over its OWN owned copies of them, by value, not
// by reference. That makes every value phi can hand to a coroutine
// (`recurse` itself included) fully self-contained, regardless of when or
// where it is actually invoked -- FD4's capture-ownership rule, in
// coroutine clothing.

#include <smd/fixpoint/free.hpp>

#include <functional>
#include <utility>
#include <variant>

namespace smd::fixpoint {

/** mcata_free :: (a -> c) -> (forall y. (y -> c) -> f y -> c) -> Free f a -> c
 * @tparam Result the fold's result type (design D5: explicit)
 * @param pure_alg  called as pure_alg(a): the Pure-leaf case, `a` an A&&.
 * @param phi       called as phi(recurse, layer): `recurse` is a callable
 *                  Free<F,A>&& -> Result (mcata_free, partially applied);
 *                  `layer` is F<Free<F,A>>&&, the Roll's outermost layer.
 *                  phi decides which children (if any) to fold via
 *                  `recurse` -- no functor_typeclass<F<...>> instance is
 *                  consulted here.
 */
template <class Result, template <class> class F, class A, class PureAlg,
          class MAlgebra>
constexpr auto mcata_free(PureAlg pure_alg, MAlgebra phi, Free<F, A> &&prog)
    -> Result {
    if (is_pure(prog)) {
        return std::invoke(pure_alg, std::get<A>(std::move(prog.node)));
    }
    auto layer = std::get<F<Free<F, A>>>(std::move(prog.node));
    auto recurse = [pure_alg, phi](Free<F, A> &&child) -> Result {
        return mcata_free<Result, F, A>(pure_alg, phi, std::move(child));
    };
    return std::invoke(phi, recurse, std::move(layer));
}

} // namespace smd::fixpoint

#endif
