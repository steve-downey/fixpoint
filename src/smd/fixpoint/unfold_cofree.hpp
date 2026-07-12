// src/smd/fixpoint/unfold_cofree.hpp                                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_UNFOLD_COFREE
#define INCLUDED_SMD_FIXPOINT_UNFOLD_COFREE

// unfold_cofree: Cofree's ana half (design doc FD8, 2026-07-09 code-audit
// correction) -- the recursion-schemes catalog never needed this: unfold_fix
// (recursion_schemes.hpp) targets Fix<F> only, and histo's Cofree carrier
// (cofree.hpp/histo.hpp) is built by *folding*, never unfolding. Mirrors
// unfold_fix's lookup-based shape exactly, generalized from one coalgebra
// S -> F<S> to a coalgebra PAIR (S -> B, S -> F<S>): `head_fn` supplies each
// node's annotation, `coalgebra` supplies the next functor layer of child
// seeds.
//
// Works for ANY functor F registered with functor_typeclass -- layer_fmap
// dispatches through the same lookup every other scheme uses -- and this
// header does not depend on the signature layer at all; freer_cosignature.hpp
// (S06) builds the co-signature's Functor instance on top of this. See
// freer_cosignature.t.cpp for a standalone proof against a pure-data functor
// (NatF), independent of the co-signature's own (lazy) use.
//
// Termination is F's Functor instance's responsibility, not this function's
// -- exactly as for unfold_fix. Over an EAGER F (a pure-data functor whose
// fmap invokes its callable immediately -- NatF, IntListF), the coalgebra
// must reach a terminal layer (one contributing no recursive-position seeds)
// or this diverges. Over a LAZY F (freer_cosignature.hpp's co-signature,
// whose fmap post-composes onto a stored responder WITHOUT invoking it),
// unfold_cofree always terminates after building exactly one node, no matter
// what the coalgebra does: layer_fmap never recurses into the next seed
// until a responder is actually invoked at runtime, and that invocation
// happens outside this function entirely. That laziness is the entire
// reason the co-signature's Functor instance is shaped the way it is (FD8).
//
// Capture-ownership rule (FD4) applies to the recursive closure below just
// as it does to every closure handed to layer_fmap: `head_fn`/`coalgebra`
// are captured BY VALUE, never by reference. This is not optional even
// though unfold_cofree's own parameters are local to this call: for a LAZY
// F, layer_fmap does not invoke the closure synchronously -- it stores it,
// unevaluated, inside the returned layer, which this function then returns
// to ITS caller. By the time anything actually invokes that stored closure
// (typically much later, e.g. via a Cofree tail's responder), this
// function's own stack frame is long gone. A `[&]`-capturing version of
// this closure was verified by hand to fail exactly this way: built as a
// standalone probe pairing a lazy CoSigLayer-shaped functor with STATEFUL
// (not stateless) head_fn/coalgebra closures (a stateless closure makes a
// dangling reference invisible to Asan -- the same lesson S02-S05 record),
// `g++-16 -fsanitize=address` reported a `stack-use-after-return` at the
// exact line reading the dangling `head_fn` capture; switching the capture
// to by-value fixed it, both compilers, same probe. For an EAGER F this
// distinction is invisible (the closure runs synchronously, so `[&]` would
// have been safe there too) -- capturing by value costs nothing correctness
// -wise for the eager case and is required for the lazy one, so it is the
// only spelling used here.

#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/fmap.hpp>

#include <functional>
#include <type_traits>
#include <utility>

namespace smd::fixpoint {

/** unfold_cofree<F>(head_fn, coalgebra, seed) -> Cofree<F, B>, where
 * B = the result type of head_fn(seed). Builds one Cofree node per step:
 * `head_fn` (S -> B) annotates the node; `coalgebra` (S -> F<S>) supplies
 * the next functor layer of child seeds, expanded via layer_fmap
 * (functor_typeclass lookup) into a layer of child Cofree trees.
 *
 * Explicit (not deduced) trailing return type: directly self-recursive
 * through the nested lambda handed to layer_fmap -- a deduced `auto` return
 * type cannot be used before its own deduction completes, even one level
 * down inside a lambda (same reasoning as CofreeFunctorImpl::fmap,
 * cofree.hpp, and FreeMonadImpl::bind, free.hpp).
 */
template <template <class> class F, class HeadFn, class Coalgebra, class Seed>
constexpr auto unfold_cofree(HeadFn head_fn, Coalgebra coalgebra,
                             const Seed &seed)
    -> Cofree<F,
              std::remove_cvref_t<std::invoke_result_t<HeadFn, const Seed &>>> {
    using B = std::remove_cvref_t<std::invoke_result_t<HeadFn, const Seed &>>;
    auto layer = coalgebra(seed);
    auto expanded = layer_fmap(
        [head_fn, coalgebra](const Seed &child) -> Cofree<F, B> {
            return unfold_cofree<F>(head_fn, coalgebra, child);
        },
        std::move(layer));
    return Cofree<F, B>{std::invoke(head_fn, seed), std::move(expanded)};
}

} // namespace smd::fixpoint

#endif
