// src/smd/fixpoint/elgot.hpp                                         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_ELGOT
#define INCLUDED_SMD_FIXPOINT_ELGOT

// Elgot (co)algebras (design §7.8): a hylomorphism (refold) whose coalgebra
// (elgot) or algebra (coelgot) gets an extra say over the recursion.
//
// elgot ::  (f a -> a) -> (b -> Either a (f b)) -> b -> a
// Equation: elgot phi psi s = case psi(s) of
//                                Left(a)     -> a
//                                Right(layer) -> phi(fmapF(elgot phi psi, layer))
// The coalgebra can short-circuit *any* seed (including the very first one)
// with a finished answer: D4's convention (Left = stop, Right = continue,
// smd::typeclass::either, §5.2) applies here exactly as it does to apo
// (apo.hpp) — Left = answer, stop; Right = one more layer to expand. Do NOT
// swap this: the whole point of the scheme is that once psi(seed) returns
// Left, psi is never called again for that seed or anything below it. This
// is checked directly (not just by the final numeric answer, which a
// plausible "expand one extra layer before discarding" bug can still get
// right on many inputs — see elgot.t.cpp's invocation-counting test).
//
// coelgot :: ((a, f b) -> b) -> (a -> f a) -> a -> b
// Equation: coelgot phi psi s = phi(s, fmapF(coelgot phi psi, psi(s)))
// The algebra additionally receives the seed that produced the current
// layer (the pair `(a, f b)` of the signature above, flattened into two
// arguments per design §7.8's own C++ snippet: `algebra(const Seed&,
// F<Result>)`). coelgot never short-circuits; psi(seed) is evaluated
// exactly once per seed (bound to a local before fmap-ing it — a careless
// transcription that calls psi twice, once to build the layer and again
// inside the fmap, would silently double the coalgebra's side effects/cost
// without changing the final answer for a pure psi).

#include <smd/fixpoint/fmap.hpp>

#include <smd/typeclass/either.hpp>

namespace smd::fixpoint {

// e780b556-5251-4f23-8b5a-5634e36439a9
/** elgot :: (f a -> a) -> (b -> Either a (f b)) -> b -> a
 *
 * @tparam Result the fold's carrier (design D5: explicit)
 * @param algebra   F<Result> -> Result
 * @param coalgebra Seed -> either<Result, F<Seed>> (D4: Left = answer, stop;
 *                  Right = one more layer, continue)
 */
template <class Result, template <class> class F, class Algebra,
          class Coalgebra, class Seed>
constexpr auto elgot(const Algebra &algebra, const Coalgebra &coalgebra,
                     const Seed &seed) -> Result {
    auto step = coalgebra(seed);
    return smd::typeclass::match(
        step,
        [](const Result &answer) -> Result { return answer; },
        [&](const auto &layer) -> Result {
            auto evaluated = layer_fmap(
                [&](const Seed &child) -> Result {
                    return elgot<Result, F>(algebra, coalgebra, child);
                },
                layer);
            return algebra(evaluated);
        });
}
// e780b556-5251-4f23-8b5a-5634e36439a9 end

// 47505502-2ee0-4088-8753-3bf71df74360
/** coelgot :: ((a, f b) -> b) -> (a -> f a) -> a -> b
 *
 * @tparam Result the fold's carrier (design D5: explicit)
 * @param algebra   (const Seed&, F<Result>) -> Result — the flattened pair;
 *                  the seed that produced `layer` is passed alongside the
 *                  already-folded children.
 * @param coalgebra Seed -> F<Seed> (no short-circuit; always one more layer)
 */
template <class Result, template <class> class F, class Algebra,
          class Coalgebra, class Seed>
constexpr auto coelgot(const Algebra &algebra, const Coalgebra &coalgebra,
                       const Seed &seed) -> Result {
    auto layer = coalgebra(seed); // evaluated exactly once per seed
    auto evaluated = layer_fmap(
        [&](const Seed &child) -> Result {
            return coelgot<Result, F>(algebra, coalgebra, child);
        },
        layer);
    return algebra(seed, evaluated);
}
// 47505502-2ee0-4088-8753-3bf71df74360 end

} // namespace smd::fixpoint

#endif
