// src/smd/fixpoint/fmap.hpp                                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_FMAP
#define INCLUDED_SMD_FIXPOINT_FMAP

#include <smd/typeclass/functor.hpp>

#include <type_traits>
#include <utility>

namespace smd::fixpoint {

// layer_fmap is the single point where scheme bodies bridge into
// smd::typeclass functor dispatch: instead of threading an explicit fmap_fn
// through every scheme, they call layer_fmap(fn, layer) and the typeclass
// lookup keyed on std::remove_cvref_t<Layer> finds the right fmap.
//
// It offers the three lookup modes of the typeclass-object design
// ("Writing Algorithms with Typeclass Objects"):
//
//   1. Implicit lookup   layer_fmap(fn, layer)
//      The Typeclass NTTP defaults to functor_typeclass<Layer>; both call
//      arguments are deduced. This is what every scheme uses.
//
//   2. NTTP pinning      layer_fmap<Layer, my_functor>(fn, layer)
//      Bind a specific instance as the non-type template parameter. Only
//      Layer (a spellable type) and the object are named; Fn still deduces,
//      so the callable stays an inline lambda. Layer precedes the NTTP for
//      exactly this reason -- keeping the callable a trailing *deduced*
//      parameter is what lets an unspellable lambda type through.
//
//   3. Explicit object   layer_fmap(my_functor, fn, layer)
//      Pass the instance as an ordinary first argument. The instance need
//      not be registered in functor_typeclass at all -- the escape hatch for
//      code (tests above all) that wants the machinery without owning, or
//      risking an ODR clash on, the global specialization.

// d1f5f4d6-a17a-47c6-b5cb-b97d08a0af6b
/** Concept satisfied by a functor typeclass *object* — anything exposing an
 * `fmap(fn, layer)` member. Disambiguates the explicit-object overload
 * (mode 3, instance first) from the implicit/pinned lookup form. */
template <class Typeclass, class Fn, class Layer>
concept functor_instance_for =
    requires(const Typeclass &tc, Fn &&fn, const Layer &layer) {
        tc.fmap(std::forward<Fn>(fn), layer);
    };

/** Apply the functor instance for @p layer's concrete type to @p fn.
 *
 * Modes 1 (implicit lookup) and 2 (NTTP pinning): the @p Typeclass reference
 * NTTP defaults to `functor_typeclass<remove_cvref_t<Layer>>` but may be
 * pinned at the call site as `layer_fmap<Layer, instance>(fn, layer)`.
 *
 * The NTTP is `const auto&` — a *reference* NTTP binding the `inline
 * constexpr` typeclass variable's static storage. A by-value `auto` NTTP
 * would instead require the instance to be a *structural* type, which the
 * `Functor`/`Monad` CRTP instances are not (their `protected` base is
 * non-structural); the reference form sidesteps that entirely. @p Layer
 * precedes the NTTP so @p Fn can trail as a deduced parameter and an inline
 * lambda never has to be spelled to reach the pin.
 */
template <class Layer,
          const auto &Typeclass =
              smd::typeclass::functor_typeclass<std::remove_cvref_t<Layer>>,
          class Fn>
constexpr auto layer_fmap(Fn &&fn, const Layer &layer) {
    return Typeclass.fmap(std::forward<Fn>(fn), layer);
}
// d1f5f4d6-a17a-47c6-b5cb-b97d08a0af6b end

// FD4: consuming (rvalue) traversal. A move-only, lazily-mapped layer
// (S03's Coyoneda impure_node) cannot compile against the const-lvalue
// overloads above -- they rebuild by copy. These overloads take the layer
// by rvalue and forward std::move(layer) into Typeclass.fmap, so `bind`
// (free.hpp) and later the Coyoneda instance can move through instead of
// copying. `Layer` is a genuine forwarding reference here, constrained to
// `!std::is_lvalue_reference_v<Layer>` so it never wins over the const&
// overloads above for an lvalue argument -- ordinary overload resolution
// then prefers this rvalue-parameter overload over the const&-binds-to-
// rvalue fallback for an actual rvalue call. A pure-data instance that
// only ever wrote a `const Layer&` fmap is still reachable through this
// overload: `Typeclass.fmap(fn, std::move(layer))` binds the moved-from
// rvalue to that `const Layer&` parameter same as any rvalue would.
template <class Layer,
          const auto &Typeclass =
              smd::typeclass::functor_typeclass<std::remove_cvref_t<Layer>>,
          class Fn>
    requires(!std::is_lvalue_reference_v<Layer>)
constexpr auto layer_fmap(Fn &&fn, Layer &&layer) {
    return Typeclass.fmap(std::forward<Fn>(fn), std::forward<Layer>(layer));
}

// c7e66aa1-1de1-4776-97d1-dc831115749a
/** Mode 3 (explicit object): apply the functor object @p tc directly instead
 * of consulting the global registry. The typeclass object is the first
 * argument — it threads ahead of the data it maps over, so a scheme can
 * carry an unregistered instance down through its recursion. Constrained on
 * `functor_instance_for` so it never competes with the two-argument
 * implicit/pinned form above.
 */
template <class Typeclass, class Fn, class Layer>
    requires functor_instance_for<Typeclass, Fn, Layer>
constexpr auto layer_fmap(const Typeclass &tc, Fn &&fn, const Layer &layer) {
    return tc.fmap(std::forward<Fn>(fn), layer);
}
// c7e66aa1-1de1-4776-97d1-dc831115749a end

// FD4: rvalue mirror of `functor_instance_for` for mode 3's consuming
// overload below -- checks that @p tc.fmap accepts @p layer by rvalue
// instead of `const Layer&`. Constrained to `!std::is_lvalue_reference_v`
// for the same reason as the mode 1/2 rvalue overload above: keep the
// existing const-lvalue mode-3 overload winning for lvalue calls.
template <class Typeclass, class Fn, class Layer>
concept functor_instance_for_rvalue =
    (!std::is_lvalue_reference_v<Layer>) &&
    requires(const Typeclass &tc, Fn &&fn, Layer &&layer) {
        tc.fmap(std::forward<Fn>(fn), std::forward<Layer>(layer));
    };

/** Mode 3, consuming: explicit-object overload taking @p layer by rvalue. */
template <class Typeclass, class Fn, class Layer>
    requires functor_instance_for_rvalue<Typeclass, Fn, Layer>
constexpr auto layer_fmap(const Typeclass &tc, Fn &&fn, Layer &&layer) {
    return tc.fmap(std::forward<Fn>(fn), std::forward<Layer>(layer));
}

} // namespace smd::fixpoint

#endif
