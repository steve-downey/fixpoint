// src/smd/fixpoint/histo.hpp                                         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_HISTO
#define INCLUDED_SMD_FIXPOINT_HISTO

// Histomorphism (design §7.5): course-of-values fold. At every child
// position the algebra sees not just that child's folded Result but its
// *entire* annotated history (a Cofree<F, Result>) — so the algebra can
// look arbitrarily far back into already-computed history (the classic
// example, coin-change DP, consults history 1/4/5 layers back).
//
// Equation: histo phi = extract . fold_fix(\x -> Cofree{phi(x), x})
// with fold carrier Cofree<F, Result>. The layer `x : F<Cofree<F,Result>>`
// is used twice — once as the algebra's own argument, once stored verbatim
// as the new node's tail (design D6: copies are fine, no sharing).

#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

namespace smd::fixpoint {

/** histo :: (f (Cofree f a) -> a) -> t -> a
 * @tparam Result the fold's result/annotation type (design D5: explicit)
 * @param algebra F<Cofree<F, Result>> -> Result
 */
template <class Result, template <class> class F, class Algebra>
constexpr auto histo(const Algebra &algebra, const Fix<F> &tree) -> Result {
    using Carrier = Cofree<F, Result>;
    auto combined = [&](const F<Carrier> &layer) -> Carrier {
        return Carrier{algebra(layer), layer};
    };
    return extract(fold_fix<Carrier>(combined, tree));
}

} // namespace smd::fixpoint

#endif
