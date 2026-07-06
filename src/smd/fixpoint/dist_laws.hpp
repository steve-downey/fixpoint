// src/smd/fixpoint/dist_laws.hpp                                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_DIST_LAWS
#define INCLUDED_SMD_FIXPOINT_DIST_LAWS

// Distributive laws (design §7.9): the polymorphic function objects that
// parameterize the generalized schemes gcata/gana/ghylo (S13/S14). No new
// scheme lives here; these are only proven by S13/S14's recovery laws (D8)
// — this header's own tests gate shape/naturality/one-off spot-checks only,
// per the step file.
//
// Every object below is callable on the layer type at hand for *every* X it
// might be instantiated at (design §4's "polymorphic function object"
// convention) except where noted, for two independent reasons discovered
// while writing this header's tests (both are C++ deduction limits, not
// design choices — see each object's own doc comment for the specific
// case):
//   1. **dist_para/dist_histo/dist_futu need their functor F named
//      explicitly**, called as `dist_para<F>(...)`/`dist_histo<F>(...)`/
//      `dist_futu<F>(...)`: whenever a law's argument type contains a
//      *bare* `F<X>` application (not wrapped inside a class template like
//      Identity/either/Cofree/Free that records F as one of its own,
//      already-named template arguments), GCC 16 cannot deduce the
//      template-template parameter F back out of an already-elaborated
//      alias-template application (NatF, IntListF, ... are alias templates,
//      D3) — empirically confirmed for all three (see their own comments).
//      This is exactly why wrap_fix (fix.hpp) always takes F explicit at
//      every existing call site in this codebase, while unwrap_fix never
//      needs to (its parameter type is `Fix<F>`, a *real* class template
//      recording F directly, not a bare `F<X>`).
//   2. **dist_apo/dist_gapo need their *result* element type X named
//      explicitly**, called as `dist_apo.operator()<X>(...)` /
//      `dist_gapo(coalg).operator()<X>(...)`: `make_left`'s result-side
//      type parameter is never deducible from its argument, by the same
//      design as either.hpp's make_left/make_right (S03) — see this
//      header's own dist_apo doc comment.

#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <smd/typeclass/either.hpp>
#include <smd/typeclass/identity.hpp>

#include <type_traits>
#include <utility>
#include <variant>

namespace smd::fixpoint {

// ---------------------------------------------------------------------
// dist_cata :: f (Identity a) -> Identity (f a)
// ---------------------------------------------------------------------

// 610b65ff-c051-4654-be28-2300b099c197
/** distCata :: f (Identity a) -> Identity (f a)
 * Design §7.9's own worked transcription, verbatim.
 */
struct dist_cata_t {
    template <class Layer> // Layer = F<Identity<A>>
    constexpr auto operator()(const Layer &l) const {
        return smd::typeclass::Identity{
            layer_fmap([](const auto &i) { return i.value; }, l)};
    }
};
inline constexpr dist_cata_t dist_cata{};

// ---------------------------------------------------------------------
// dist_ana :: Identity (f a) -> f (Identity a)
// ---------------------------------------------------------------------

/** distAna :: Identity (f a) -> f (Identity a)
 * fmapF(wrap-in-Identity) over the layer inside.
 *
 * The wrapping lambda names its result type explicitly
 * (`Identity<remove_cvref_t<decltype(x)>>{x}`) rather than writing
 * `Identity{x}` and relying on CTAD (DEV-03, S14): when `A` (the layer's
 * own element type) is itself already an `Identity<...>` -- which is
 * exactly what happens when `dist_ana` is used as `gana`'s distributive
 * law with `MSeed = Identity<Seed>` (generalized.hpp's `ana_via_gana`) --
 * `Identity{x}` hits CTAD's implicit *copy* deduction candidate
 * ([over.match.class.deduct]) and silently collapses to `Identity<T>`
 * (a copy of `x`) instead of wrapping it one level deeper as
 * `Identity<Identity<T>>`, exactly as `vector{v}` for `v : vector<int>`
 * copies rather than nesting. Naming the template argument explicitly
 * sidesteps CTAD entirely, so the lambda always wraps regardless of what
 * `x`'s own type happens to be.
 */
struct dist_ana_t {
    template <class Layer> // Layer = F<A>; argument is Identity<Layer>
    constexpr auto operator()(const smd::typeclass::Identity<Layer> &ident)
        const {
        return layer_fmap(
            [](const auto &x) {
                return smd::typeclass::Identity<
                    std::remove_cvref_t<decltype(x)>>{x};
            },
            ident.value);
    }
};
inline constexpr dist_ana_t dist_ana{};
// 610b65ff-c051-4654-be28-2300b099c197 end

// ---------------------------------------------------------------------
// dist_histo :: f (Cofree f a) -> Cofree f (f a)
// ---------------------------------------------------------------------

/** distHisto :: f (Cofree f a) -> Cofree f (f a)
 *
 * head = fmapF(extract, l); tail = fmapF(λc. dist_histo(c.tail), l).
 *
 * Recurses through Cofree tails at the *same* instantiation (F, A) every
 * level down (structural descent on the tree, not on the types) — exactly
 * the CofreeComonadImpl::duplicate shape (S07). Its own return type is
 * written out explicitly (`-> Cofree<F, F<A>>`, computed from F/A alone,
 * never from the call's own deduced result) for the same reason S07/S08's
 * handoffs flagged for every self-recursive-through-a-lambda case in this
 * codebase: GCC 16 rejects a deduced `auto` return type used before its own
 * deduction completes, even one level down inside a lambda. Recurses via
 * `(*this)(...)`, not by name, so no forward-declaration/ADL timing to
 * reason about.
 *
 * Like dist_para below (and unlike dist_cata/dist_ana, whose argument type
 * is a single opaque `Layer` or wraps `Layer` inside a *real* class template
 * that names it directly), this object's argument type is a *bare*
 * `F<Cofree<F,A>>` application — an alias-template functor family (NatF,
 * IntListF, ...) applied directly, with no surrounding named struct to
 * anchor `F`. GCC 16 cannot deduce a template-template parameter `F` back
 * out of an already-elaborated alias application (confirmed empirically:
 * attempting `template <template<class> class F, class A> operator()(const
 * F<Cofree<F,A>>&)` fails deduction outright — "is not derived from const
 * F<Cofree<F,A>>", the same failure wrap_fix's callers avoid by always
 * naming `F` explicitly, fix.hpp). So `dist_histo` is a variable *template*
 * parameterized on `F` (called as `dist_histo<F>(layer)`): once `F` is
 * bound at the struct level, `A` deduces normally from the argument (same
 * fix as dist_para_t below).
 */
// f96a7eef-2e16-4dce-b8b4-f507cbd743ca
template <template <class> class F>
struct dist_histo_t {
    template <class A>
    constexpr auto operator()(const F<Cofree<F, A>> &l) const
        -> Cofree<F, F<A>> {
        auto head = layer_fmap(
            [](const Cofree<F, A> &c) { return extract(c); }, l);
        auto tail = layer_fmap(
            [this](const Cofree<F, A> &c) -> Cofree<F, F<A>> {
                return (*this)(c.tail);
            },
            l);
        return Cofree<F, F<A>>{std::move(head), std::move(tail)};
    }
};

template <template <class> class F>
inline constexpr dist_histo_t<F> dist_histo{};
// f96a7eef-2e16-4dce-b8b4-f507cbd743ca end

// ---------------------------------------------------------------------
// dist_futu :: Free f (f a) -> f (Free f a)
// ---------------------------------------------------------------------

/** distFutu :: Free f (f a) -> f (Free f a)
 *
 * Pure layer (layer : F<A>)          -> fmapF(pure_free, layer)
 * Roll layer  (layer : F<Free<F,A>>) -> fmapF(roll_free . dist_futu, layer)
 *
 * Mind which functor each fmap runs over: the *outer* layer_fmap in both
 * branches walks the outer F; the Roll branch's inner `dist_futu(child)`
 * recurses into Free's own variant one more level (same instantiation
 * F, A every level, same self-recursion shape/fix as dist_histo above —
 * explicit trailing return type, `(*this)(...)` recursion).
 *
 * Same F-must-be-explicit reasoning as dist_histo above: the argument type
 * `Free<F, F<A>>` still contains a *bare* `F<A>` (Free's own second
 * template argument), which the same alias-deduction limit bites just as
 * hard as a top-level `F<X>` would — empirically confirmed alongside
 * dist_histo's failure. `dist_futu` is therefore also a variable template
 * on `F` (called as `dist_futu<F>(chunk)`); `A` deduces normally from the
 * argument once `F` is bound.
 */
template <template <class> class F>
struct dist_futu_t {
    template <class A>
    constexpr auto operator()(const Free<F, F<A>> &chunk) const
        -> F<Free<F, A>> {
        return std::visit(
            overloaded{
                [](const F<A> &layer) -> F<Free<F, A>> {
                    return layer_fmap(
                        [](const A &x) { return pure_free<F>(x); }, layer);
                },
                [this](const F<Free<F, F<A>>> &layer) -> F<Free<F, A>> {
                    return layer_fmap(
                        [this](const Free<F, F<A>> &child) -> Free<F, A> {
                            return roll_free<F>((*this)(child));
                        },
                        layer);
                },
            },
            chunk.node);
    }
};

template <template <class> class F>
inline constexpr dist_futu_t<F> dist_futu{};

// ---------------------------------------------------------------------
// dist_zygo(helper) :: (f b -> b) -> f (b, a) -> (b, f a)
// ---------------------------------------------------------------------

/** The object returned by dist_zygo(helper) (factory, see below): captures
 * @p helper by value (S12 pitfall: keep the returned type a named struct so
 * S13/S14 can spell it in explicit template arguments if needed).
 */
template <class HelperAlg>
struct dist_zygo_t {
    HelperAlg helper;

    // Layer = F<std::pair<B, X>>; helper : F<B> -> B (S05's convention:
    // helper first, main/value second).
    template <class Layer>
    constexpr auto operator()(const Layer &l) const {
        auto helper_layer =
            layer_fmap([](const auto &p) { return p.first; }, l);
        auto x_layer = layer_fmap([](const auto &p) { return p.second; }, l);
        return std::pair{helper(helper_layer), std::move(x_layer)};
    }
};

/** distZygo :: (f b -> b) -> f (b, a) -> (b, f a)
 *
 * A *factory*: takes the helper algebra `F<B> -> B` and returns the law
 * `F<std::pair<B, X>> -> std::pair<B, F<X>>` for every X. `B`/`X` are
 * extracted via std::pair's own named members (`.first`/`.second`), so
 * unlike dist_apo/dist_gapo below, no explicit template argument is ever
 * needed at the call site.
 */
template <class HelperAlg>
constexpr auto dist_zygo(HelperAlg helper) -> dist_zygo_t<HelperAlg> {
    return dist_zygo_t<HelperAlg>{std::move(helper)};
}

// ---------------------------------------------------------------------
// dist_para :: f (Fix f, a) -> (Fix f, f a)
// ---------------------------------------------------------------------

/** distPara = dist_zygo(wrap_fix), with B = Fix<F> — but implemented as its
 * own small object (per the step file), not literally as a call to
 * dist_zygo(wrap_fix<F>): wrap_fix<F> needs F named explicitly everywhere
 * it is used in this codebase (fix.hpp: unlike unwrap_fix, whose parameter
 * type `Fix<F>` lets F deduce trivially from the argument, wrap_fix's
 * parameter type is `F<Fix<F>>` — an already-elaborated functor
 * application, which C++ cannot invert back to "F" for an alias-template
 * functor family like NatF/IntListF). Making dist_para itself a variable
 * *template* parameterized on F (called as `dist_para<F>(layer)`) gives
 * `wrap_fix<F>` a concrete, already-bound F to use, and X then deduces
 * normally from the argument (the same reason dist_para<F>'s F must be
 * bound before X can deduce, one level up from dist_zygo_t's Layer-only
 * deduction above).
 */
template <template <class> class F>
struct dist_para_t {
    template <class X>
    constexpr auto operator()(const F<std::pair<Fix<F>, X>> &l) const
        -> std::pair<Fix<F>, F<X>> {
        auto b_layer = layer_fmap(
            [](const std::pair<Fix<F>, X> &p) -> Fix<F> { return p.first; },
            l);
        auto x_layer = layer_fmap(
            [](const std::pair<Fix<F>, X> &p) -> X { return p.second; }, l);
        return std::pair<Fix<F>, F<X>>{wrap_fix<F>(std::move(b_layer)),
                                       std::move(x_layer)};
    }
};

template <template <class> class F>
inline constexpr dist_para_t<F> dist_para{};

// ---------------------------------------------------------------------
// dist_apo :: Either (Fix f) (f a) -> f (Either (Fix f) a)
// ---------------------------------------------------------------------

/** distApo :: Either t (f a) -> f (Either t a)   (either, D4)
 *
 * Right(layer) -> fmapF(make_right, layer)
 * Left(t)      -> fmapF(make_left,  unfix(t))
 *
 * The Right branch needs no explicit type argument: `make_right<L>` only
 * needs its fixed `L` (the either's own Left type, already known from the
 * argument), and deduces its `R` per element automatically. The Left branch
 * is different: `make_left<R, L>` can *never* deduce its result-side `R`
 * (design §5.2/S03 handoff — this is the whole reason `make_left`'s result
 * side is named first) — and here the "R" we need is the *other* branch's
 * element type, which does not appear anywhere in the Left branch's own
 * data (`unfix(t) : F<Fix<F>>` only ever mentions `Fix<F>`, never the
 * unrelated `X` that the Right branch's `F<X>` carries). So this object's
 * call operator takes that element type `X` as an **explicit** leading
 * template parameter (D5's "carrier explicit" convention) — `L`/`R` (the
 * either's own two type parameters) still deduce from the argument as
 * usual. Called as `dist_apo.operator()<X>(e)` (or, inside a dependent
 * context, `dist_apo.template operator()<X>(e)`).
 */
struct dist_apo_t {
    template <class X, class L, class R>
    constexpr auto operator()(const smd::typeclass::either<L, R> &e) const {
        return smd::typeclass::match(
            e,
            [](const L &t) {
                return layer_fmap(
                    [](const auto &child) {
                        return smd::typeclass::make_left<X>(child);
                    },
                    unwrap_fix(t));
            },
            [](const R &layer) {
                return layer_fmap(
                    [](const auto &x) {
                        return smd::typeclass::make_right<L>(x);
                    },
                    layer);
            });
    }
};
inline constexpr dist_apo_t dist_apo{};

// ---------------------------------------------------------------------
// dist_gapo(coalg) :: (b -> f b) -> Either b (f a) -> f (Either b a)
// ---------------------------------------------------------------------

/** The object returned by dist_gapo(coalg) (factory, see below): captures
 * @p coalg by value, mirroring dist_zygo_t.
 */
template <class Coalg>
struct dist_gapo_t {
    Coalg coalg;

    // Same explicit-X reasoning as dist_apo_t above: coalg(b) : F<B> only
    // ever mentions B, never the unrelated X the Right branch's F<X> holds.
    template <class X, class B, class R>
    constexpr auto operator()(const smd::typeclass::either<B, R> &e) const {
        return smd::typeclass::match(
            e,
            [this](const B &b) {
                return layer_fmap(
                    [](const auto &child) {
                        return smd::typeclass::make_left<X>(child);
                    },
                    coalg(b));
            },
            [](const R &layer) {
                return layer_fmap(
                    [](const auto &x) {
                        return smd::typeclass::make_right<B>(x);
                    },
                    layer);
            });
    }
};

/** distGApo :: (b -> f b) -> Either b (f a) -> f (Either b a)
 *
 * Generalizes dist_apo with a coalgebra @p coalg : b -> F<b> for the Left
 * side (dist_apo is the special case coalg = unwrap_fix, b = Fix<F>) — this
 * is where either's symmetric construction pays off (design §7.9): with
 * std::expected it would have been transform_error gymnastics.
 */
template <class Coalg>
constexpr auto dist_gapo(Coalg coalg) -> dist_gapo_t<Coalg> {
    return dist_gapo_t<Coalg>{std::move(coalg)};
}

} // namespace smd::fixpoint

#endif
