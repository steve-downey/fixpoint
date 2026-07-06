// src/smd/fixpoint/chrono.hpp                                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_CHRONO
#define INCLUDED_SMD_FIXPOINT_CHRONO

// dyna, codyna, chrono (design §7.6): the three course-of-values *refold*
// fusions. Each is histo/futu fused with refold (recursion_schemes.hpp) so
// that no intermediate Fix<F> tree is ever materialized — unlike
// "histo(unfold_fix(psi, seed))" or "fold_fix(futu(psi, seed))", which would
// build the whole Fix<F> tree first and then walk it a second time, these
// three interleave the coalgebra and algebra in a single refold pass, one
// F-layer at a time.
//
//   dyna    = histo   . ana,  fused via refold
//   codyna  = cata    . futu, fused via refold
//   chrono  = histo   . futu, fused via refold
//
// codyna/chrono share a coalgebra shape: refold needs a single-step
// Seed -> F<Seed> coalgebra, but futu's own coalgebra is Seed -> F<Free<F,
// Seed>> (it may emit several F-layers per step, as a Free chunk). `unroll`
// bridges the two: given a Free<F, Seed> chunk, it peels exactly one
// F-layer — resuming the original coalgebra at a Pure leaf, or handing back
// an already-built Roll layer unchanged — and refold itself supplies the
// recursion over that layer's children (each still a Free<F, Seed>).
// `unroll` never fully resolves a chunk to Fix<F> itself (unlike futu's own
// futu_worker, futu.hpp) — it only ever peels one layer per call, exactly
// what refold's coalgebra parameter needs.

#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/overloaded.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <variant>

namespace smd::fixpoint {

/** Implementation detail of codyna/chrono: peels one F-layer off a
 * Free<F, Seed> chunk (as futu's coalgebra would emit). A Pure leaf resumes
 * the original coalgebra at its seed; a Roll layer is handed back verbatim
 * (by value — it holds further Free<F,Seed> children for refold to keep
 * unrolling on its own next call).
 */
template <template <class> class F, class Coalgebra, class Seed>
constexpr auto unroll(const Coalgebra &coalgebra, const Free<F, Seed> &chunk)
    -> F<Free<F, Seed>> {
    return std::visit(
        overloaded{
            [&](const Seed &s) -> F<Free<F, Seed>> { return coalgebra(s); },
            [&](const F<Free<F, Seed>> &layer) -> F<Free<F, Seed>> {
                return layer;
            },
        },
        chunk.node);
}

// d06b483b-3a00-4884-ab03-e4e882156ba2
/** dyna :: (f (Cofree f b) -> b) -> (a -> f a) -> a -> b
 * histo . ana, fused: never builds the intermediate Fix<F>.
 * @param algebra F<Cofree<F, Result>> -> Result
 * @param coalgebra Seed -> F<Seed>
 */
template <class Result, template <class> class F, class Algebra,
          class Coalgebra, class Seed>
constexpr auto dyna(const Algebra &algebra, const Coalgebra &coalgebra,
                    const Seed &seed) -> Result {
    using Carrier = Cofree<F, Result>;
    auto combined = [&](const F<Carrier> &layer) -> Carrier {
        return Carrier{algebra(layer), layer};
    };
    return extract(refold<Carrier, F>(combined, coalgebra, seed));
}
// d06b483b-3a00-4884-ab03-e4e882156ba2 end

// 65e204e3-867d-4932-a9bb-83b665f8cdbb
/** codyna :: (f b -> b) -> (a -> f (Free f a)) -> a -> b
 * cata . futu, fused: never builds the intermediate Fix<F>.
 * @param algebra F<Result> -> Result
 * @param coalgebra Seed -> F<Free<F, Seed>>
 */
template <class Result, template <class> class F, class Algebra,
          class Coalgebra, class Seed>
constexpr auto codyna(const Algebra &algebra, const Coalgebra &coalgebra,
                      const Seed &seed) -> Result {
    auto unroll_step = [&coalgebra](const Free<F, Seed> &chunk)
        -> F<Free<F, Seed>> { return unroll<F>(coalgebra, chunk); };
    return refold<Result, F>(algebra, unroll_step, pure_free<F>(seed));
}

/** chrono :: (f (Cofree f b) -> b) -> (a -> f (Free f a)) -> a -> b
 * histo . futu, fused: never builds the intermediate Fix<F>.
 * @param algebra F<Cofree<F, Result>> -> Result
 * @param coalgebra Seed -> F<Free<F, Seed>>
 */
template <class Result, template <class> class F, class Algebra,
          class Coalgebra, class Seed>
constexpr auto chrono(const Algebra &algebra, const Coalgebra &coalgebra,
                      const Seed &seed) -> Result {
    using Carrier = Cofree<F, Result>;
    auto combined = [&](const F<Carrier> &layer) -> Carrier {
        return Carrier{algebra(layer), layer};
    };
    auto unroll_step = [&coalgebra](const Free<F, Seed> &chunk)
        -> F<Free<F, Seed>> { return unroll<F>(coalgebra, chunk); };
    return extract(
        refold<Carrier, F>(combined, unroll_step, pure_free<F>(seed)));
}
// 65e204e3-867d-4932-a9bb-83b665f8cdbb end

} // namespace smd::fixpoint

#endif
