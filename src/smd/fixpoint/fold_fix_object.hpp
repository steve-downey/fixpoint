// src/smd/fixpoint/fold_fix_object.hpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// EXPERIMENT (branch experiment-fold-fix-object): fold_fix reframed as an
// algorithm *object* that inherits from its functor instance, per the
// algorithm-authoring pattern in "Writing Algorithms with Typeclass Objects".
//
// Compare against the free-function fold_fix in recursion_schemes.hpp:
//
//   free function                          this experiment
//   -------------------------------------  ------------------------------------
//   layer_fmap(fn, layer)  (registry       this->fmap(fn, layer)  (inherited
//     lookup on every recursive call)        member; instance pinned once)
//   fold_fix<int>(alg, tree)               fold_fix<int>(alg, tree)   (same)
//   -- no way to thread an unregistered    fold_fix<int>(inst, alg, tree)
//      instance through the recursion         (impl inherits inst's type;
//                                              recursion reuses it by
//                                              construction)
#ifndef INCLUDED_SMD_FIXPOINT_FOLD_FIX_OBJECT
#define INCLUDED_SMD_FIXPOINT_FOLD_FIX_OBJECT

#include <smd/fixpoint/fix.hpp>
#include <smd/typeclass/functor.hpp>

#include <type_traits>
#include <utility>

namespace smd::fixpoint::experimental {

namespace detail {

/** The functor instance *type* for a fixpoint's layer `F<Fix<F>>`.
 *
 * Every level of a fold over `Fix<F>` maps a layer of exactly this type
 * (`unwrap_fix` returns `const F<Fix<F>>&` at every depth), so a single
 * instance serves the entire recursion — which is what lets the algorithm
 * inherit *one* instance rather than re-looking-it-up per call. */
template <template <typename> class F>
using fold_functor_t =
    std::remove_cvref_t<decltype(smd::typeclass::functor_typeclass<F<Fix<F>>>)>;

/** Catamorphism carrier. Inherits from the functor instance so the body
 * names `fmap` directly; @p Functor defaults to the registered instance's
 * type but may be pinned to an unregistered one. The same `*this` — hence
 * the same instance — flows through every recursive `call`. */
template <class Result, template <typename> class F,
          class Functor = fold_functor_t<F>>
struct fold_fix_impl : Functor {
    template <class Algebra>
    constexpr auto call(const Algebra &algebra, const Fix<F> &tree) const
        -> Result {
        auto evaluated = this->fmap(
            [&](const Fix<F> &child) -> Result {
                return this->call(algebra, child);
            },
            unwrap_fix(tree));
        return algebra(evaluated);
    }
};

} // namespace detail

/** Function object for `fold_fix`, parameterized on the (explicit) @p Result
 * — the algebra's return type, which a catamorphism cannot deduce. `F` is
 * deduced from the tree, so the call site matches the free function exactly:
 * `fold_fix<int>(algebra, tree)`.
 *
 * It is an object, not a function, so the call is not subject to ADL. */
template <class Result>
struct fold_fix_fn {
    /** Implicit lookup: functor instance defaulted from `F<Fix<F>>`. */
    template <template <typename> class F, class Algebra>
    constexpr auto operator()(const Algebra &algebra, const Fix<F> &tree) const
        -> Result {
        return detail::fold_fix_impl<Result, F>{}.call(algebra, tree);
    }

    /** Explicit object: thread a (possibly unregistered) instance through the
     * whole fold. The impl inherits from the instance's type, so every
     * recursive level reuses it by construction — no per-level registry
     * lookup, and no dependence on a global specialization existing. The
     * instance is used for its type only (the design's instances are empty);
     * a stateful instance would want the object copied into the base. */
    template <class Functor, template <typename> class F, class Algebra>
    constexpr auto operator()(const Functor &, const Algebra &algebra,
                              const Fix<F> &tree) const -> Result {
        return detail::fold_fix_impl<Result, F, std::remove_cvref_t<Functor>>{}
            .call(algebra, tree);
    }
};

template <class Result>
inline constexpr fold_fix_fn<Result> fold_fix{};

// --- Alternative: thread the instance by value, no inheritance ----------
//
// fold_fix uses exactly one typeclass operation (fmap) exactly once per
// level, so the "inherit the instance to bring names into scope" move buys
// almost nothing over simply threading the instance value and calling
// `functor.fmap(...)`. This variant achieves the same mode-3 threading with
// one function and no carrier struct / variable template. Inheritance earns
// its keep when an algorithm body composes *several* named operations
// (possibly from two typeclasses); a single-operation scheme is not that.

/** In-context check that @p Functor can `fmap` a callable `Fn` over a `Layer`.
 *
 * A plain compile-time bool, meant for `static_assert` — not a requires-clause
 * on the algorithm templates. Threading the instance is *the same operation*
 * regardless of where the instance comes from, so there is no overload to
 * select between: a bad instance should produce one direct diagnostic naming
 * the problem, not SFINAE-remove a candidate and bury the real cause under
 * "no matching function" spew. We cannot validate a typeclass object fully
 * generically, but in the context of concrete layer and function types (which
 * each scheme has) the check is straightforward. */
template <class Functor, class Fn, class Layer>
constexpr bool functor_maps = requires(const Functor &functor, Fn fn,
                                        const Layer &layer) {
    functor.fmap(fn, layer);
};

/** Explicit-instance catamorphism: `functor` is threaded down the recursion
 * as an ordinary value; no registry lookup, no global specialization needed. */
template <class Result, class Functor, template <typename> class F,
          class Algebra>
constexpr auto fold_fix_threaded(const Functor &functor,
                                 const Algebra &algebra, const Fix<F> &tree)
    -> Result {
    static_assert(
        functor_maps<Functor, Result (*)(const Fix<F> &), F<Fix<F>>>,
        "fold_fix_threaded: the functor instance has no fmap(fn, F<Fix<F>>) "
        "for this layer -- pass a functor typeclass object for F "
        "(e.g. smd::typeclass::functor_typeclass<F<Fix<F>>>).");
    auto evaluated = functor.fmap(
        [&](const Fix<F> &child) -> Result {
            return fold_fix_threaded<Result>(functor, algebra, child);
        },
        unwrap_fix(tree));
    return algebra(evaluated);
}

/** Implicit-lookup catamorphism, instance threaded by value. */
template <class Result, template <typename> class F, class Algebra>
constexpr auto fold_fix_threaded(const Algebra &algebra, const Fix<F> &tree)
    -> Result {
    return fold_fix_threaded<Result>(
        smd::typeclass::functor_typeclass<F<Fix<F>>>, algebra, tree);
}

} // namespace smd::fixpoint::experimental

#endif
