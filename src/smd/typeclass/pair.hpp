// src/smd/typeclass/pair.hpp                                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_TYPECLASS_PAIR
#define INCLUDED_SMD_TYPECLASS_PAIR

#include <smd/typeclass/comonad.hpp>
#include <smd/typeclass/detail/typeclass_base.hpp>
#include <smd/typeclass/functor.hpp>

#include <functional>
#include <type_traits>
#include <utility>

// std::pair<B, A> is the D11 dual of either.hpp's either<L, R>: pair is the
// product (both components always present), either is the coproduct (exactly
// one alternative is present). Slot convention matches throughout: the
// non-value slot comes first, the value slot second — pair's env comonad has
// extract = .second, either's monad has pure = make_right; zygo's
// `pair<Helper, Result>` and either's `either<Stop, Continue>` follow the
// same convention. Dual vocabulary:
//   introduction   pair{a, b}            <-> make_left / make_right
//   elimination    .first / .second      <-> match(e, on_left, on_right)
//   pairing        fanout(f, g)          <-> fanin(f, g)
//   one-sided map  map_first(f, p)       <-> map_left(f, e)
//   functor        maps .second           <-> maps Right
//
// Reference-assignment semantics differ between the two by construction, and
// that is correct, not an inconsistency to fix: std::pair<T&, U> assigns
// *through* the referent (both components always exist, so assignment can
// always forward into them — this is standard std::pair behavior since
// P2321R2/C++23, no extra work needed here), while either<L&, R> *rebinds*
// (see either.hpp's Left<L&>/Right<R&>) because the active alternative can
// change under assignment, so there may be no referent left to assign
// through. Rebinding is the only coherent model for a sum; assigning through
// is the only coherent model for a product.

namespace smd::typeclass {

// -- Functor (maps .second, matching either's "maps Right") --

template <class B, class A>
struct PairFunctorImpl {
    template <class Fn>
    constexpr auto fmap(this auto &&, Fn &&fn, const std::pair<B, A> &p) {
        using C = remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return std::pair<B, C>{p.first,
                               std::invoke(std::forward<Fn>(fn), p.second)};
    }
};

template <class B, class A>
struct PairFunctorMap : Functor<PairFunctorImpl<B, A>> {
    using PairFunctorImpl<B, A>::fmap;
};

/** Functor instance for `std::pair<B, A>`: maps `.second`, leaving `.first`
 * untouched — the dual of `either<L, R>`'s "maps Right".
 */
template <class B, class A>
inline constexpr auto functor_typeclass<std::pair<B, A>> =
    PairFunctorMap<B, A>{};

// -- Comonad (the "env" / reader comonad: B is the fixed environment) --

// `extract`/`duplicate`/`fmap` are generic over the value-slot type (their
// own template parameter `X`), not fixed to the instance's own `A`; only the
// environment `B` stays fixed (the env comonad never changes its
// environment). This is needed so the Comonad CRTP's derived
// `extend = fmap f . duplicate` can invoke `fmap` on the doubled structure
// `std::pair<B, std::pair<B, X>>` that `duplicate` produces — mirroring how
// OptionalMonadImpl's `bind` is generic in its own template parameter rather
// than the instance-keying `VALUE_TYPE`. `A` only serves to key the
// `comonad_typeclass<std::pair<B, A>>` lookup below.
template <class B, class A>
struct PairComonadImpl {
    template <class X>
    constexpr auto extract(this auto &&, const std::pair<B, X> &p)
        -> const X & {
        return p.second;
    }

    template <class X>
    constexpr auto duplicate(this auto &&, const std::pair<B, X> &p)
        -> std::pair<B, std::pair<B, X>> {
        return std::pair<B, std::pair<B, X>>{p.first, p};
    }

    template <class Fn, class X>
    constexpr auto fmap(this auto &&, Fn &&fn, const std::pair<B, X> &p) {
        using C = remove_cvref_t<std::invoke_result_t<Fn, const X &>>;
        return std::pair<B, C>{p.first,
                               std::invoke(std::forward<Fn>(fn), p.second)};
    }
};

template <class B, class A>
struct PairComonadMap : Comonad<PairComonadImpl<B, A>> {
    using PairComonadImpl<B, A>::duplicate;
    using PairComonadImpl<B, A>::extract;
    using PairComonadImpl<B, A>::fmap;
};

/** The env/reader comonad instance for `std::pair<B, A>`: `extract` reads
 * the value slot (`.second`), `duplicate` nests the whole pair back into the
 * value slot alongside the unchanged environment (`.first`).
 */
template <class B, class A>
inline constexpr auto comonad_typeclass<std::pair<B, A>> =
    PairComonadMap<B, A>{};

// -- D11 dual vocabulary --

/** Maps `.first` only, leaving `.second` untouched; the dual of
 * `map_left` (either.hpp).
 */
template <class Fn, class B, class A>
constexpr auto map_first(Fn &&fn, const std::pair<B, A> &p) {
    using C = remove_cvref_t<std::invoke_result_t<Fn, const B &>>;
    return std::pair<C, A>{std::invoke(std::forward<Fn>(fn), p.first),
                          p.second};
}

/** The product introducer `<f, g>` (the pairing): returns a callable
 * `x -> pair{f(x), g(x)}`; the dual of `fanin(f, g)` (either.hpp).
 */
template <class F, class G>
constexpr auto fanout(F f, G g) {
    return [f = std::move(f), g = std::move(g)](const auto &x) {
        return std::pair{std::invoke(f, x), std::invoke(g, x)};
    };
}

} // namespace smd::typeclass

#endif // INCLUDED_SMD_TYPECLASS_PAIR
