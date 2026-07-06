// src/smd/fixpoint/para.hpp                                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_PARA
#define INCLUDED_SMD_FIXPOINT_PARA

// Paramorphism (design §7.2). Like fold_fix, but the algebra sees, at every
// child position, both the *original* subtree and its fold result — the
// classical extension that lets an algebra "look back" at un-folded
// structure (e.g. a pretty-printer inspecting a child's original shape to
// decide whether it needs parentheses).

#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/fmap.hpp>

#include <utility>

namespace smd::fixpoint {

// 5cdab85f-88b0-477e-9da4-6766f7615a74
/** para :: (f (t, a) -> a) -> t -> a
 *
 * Equation: para φ = φ ∘ fmapF (λc. (c, para φ c)) ∘ unfix
 *
 * The pair built at each child position is `{original_subtree,
 * fold_result}` — original first, result second (design §7.2 forward
 * note: later steps, e.g. dist_para, assume this order).
 *
 * @tparam Result carrier type (D5: explicit, cannot be deduced through the
 *   recursive call)
 * @param algebra F<std::pair<Fix<F>, Result>> -> Result
 */
template <class Result, template <class> class F, class Algebra>
constexpr auto para(const Algebra &algebra, const Fix<F> &tree) -> Result {
    const auto &layer = unwrap_fix(tree);
    auto evaluated = layer_fmap(
        [&](const Fix<F> &child) -> std::pair<Fix<F>, Result> {
            return std::pair<Fix<F>, Result>{child,
                                              para<Result>(algebra, child)};
        },
        layer);
    return algebra(evaluated);
}
// 5cdab85f-88b0-477e-9da4-6766f7615a74 end

} // namespace smd::fixpoint

#endif
