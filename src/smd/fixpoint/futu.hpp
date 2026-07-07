// src/smd/fixpoint/futu.hpp                                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_FUTU
#define INCLUDED_SMD_FIXPOINT_FUTU

// Futumorphism (design §7.5): course-of-values unfold. A single coalgebra
// step may emit several layers at once — a Free<F, Seed> *chunk*, built
// with nested roll_free (free.hpp) — with fresh seeds sitting at the
// chunk's Pure leaves for futu to keep unfolding from.
//
// Equation: futu psi = fix . fmapF(worker) . psi, where:
//   worker(Pure s)      = futu psi s
//   worker(Roll layer)  = fix(fmapF(worker, layer))
//
// Unlike Cofree/Free's own recursive typeclass methods (cofree.hpp,
// free.hpp), futu's worker is an ordinary free function template whose
// return type is always the concrete `Fix<F>` — never a type deduced from
// the coalgebra — so it does not hit the GCC 16 "use of ... before
// deduction of 'auto'" self-recursion quirk those handoffs flagged: the
// trailing return type is written out explicitly here too, but that's
// just following house style (every existing scheme in this module does
// the same), not a workaround for a cycle that doesn't exist in this
// shape.

#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <utility>
#include <variant>

namespace smd::fixpoint {

template <template <class> class F, class Coalgebra, class Seed>
constexpr auto futu(const Coalgebra &coalgebra, const Seed &seed) -> Fix<F>;

// e0f553ab-ab72-44c9-b21c-ab1aff17751a
/** Implementation detail of futu: unrolls one Free<F, Seed> chunk (as
 * emitted by a single coalgebra step) into a Fix<F> subtree. Pure s
 * resumes futu at s; a Roll layer is unrolled one F-layer at a time,
 * recursing into every child chunk.
 */
template <template <class> class F, class Coalgebra, class Seed>
constexpr auto futu_worker(const Coalgebra &coalgebra,
                           const Free<F, Seed> &chunk) -> Fix<F> {
    return std::visit(
        overloaded{
            [&](const Seed &s) -> Fix<F> { return futu<F>(coalgebra, s); },
            [&](const F<Free<F, Seed>> &layer) -> Fix<F> {
                return wrap_fix<F>(layer_fmap(
                    [&coalgebra](const Free<F, Seed> &child) -> Fix<F> {
                        return futu_worker<F>(coalgebra, child);
                    },
                    layer));
            },
        },
        chunk.node);
}

/** futu :: (a -> f (Free f a)) -> a -> t
 * @tparam F unary template functor (D5: explicit, cannot be deduced from
 *   @p seed alone)
 * @param coalgebra Seed -> F<Free<F, Seed>>
 */
template <template <class> class F, class Coalgebra, class Seed>
constexpr auto futu(const Coalgebra &coalgebra, const Seed &seed) -> Fix<F> {
    auto layer = coalgebra(seed); // F<Free<F, Seed>>
    auto expanded = layer_fmap(
        [&coalgebra](const Free<F, Seed> &child) -> Fix<F> {
            return futu_worker<F>(coalgebra, child);
        },
        layer);
    return wrap_fix<F>(std::move(expanded));
}
// e0f553ab-ab72-44c9-b21c-ab1aff17751a end

} // namespace smd::fixpoint

#endif
