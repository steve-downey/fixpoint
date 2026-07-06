// src/smd/typeclass/either.hpp                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_TYPECLASS_EITHER
#define INCLUDED_SMD_TYPECLASS_EITHER

#include <smd/typeclass/detail/typeclass_base.hpp>
#include <smd/typeclass/functor.hpp>
#include <smd/typeclass/monad.hpp>

#include <smd/fixpoint/overloaded.hpp>

#include <functional>
#include <type_traits>
#include <utility>
#include <variant>

// either<L, R> — the project's short-circuit sum type (design D4). We use it
// instead of std::expected because std::expected's sides are asymmetric
// (`unexpected` is a distinct wrapper only on the error side, error-side
// mapping is spelled `transform_error`) and `expected<T, T>` — which arises
// naturally, e.g. apo's `either<Fix<F>, Seed>` when a coalgebra's finished
// subtree and its seed share a type — forces tag-only construction and
// invites ambiguity in generic code. either<L, R> instead gives *both* sides
// a distinct wrapper (Left<L>/Right<R>), so either<T, T> always constructs
// and compares unambiguously.
//
// Fixed convention, identical to the Haskell sources this library is
// modeled on: Left = short-circuit/stop, Right = continue recursing.
// Instances below are right-biased (functor maps Right, monad's pure is
// make_right and bind propagates Left), matching Haskell's Either.
//
// either is the D11 dual of std::pair: pair is the product (both components
// always present), either is the coproduct (exactly one alternative is
// present). The library treats them symmetrically:
//   introduction   pair{a, b}            <-> make_left / make_right
//   elimination    .first / .second      <-> match(e, on_left, on_right)
//   pairing        fanout(f, g)          <-> fanin(f, g)
//   one-sided map  map_first(f, p)       <-> map_left(f, e)
//   functor        maps .second           <-> maps Right
// Reference-assignment semantics differ between the two by construction, not
// convention: std::pair<T&, U> assigns *through* the referent (both slots
// always exist, so there is always something to assign into); either<L&, R>
// *rebinds* (the active alternative can change under assignment, so the old
// referent may no longer be the right target) — see the Left<L&>/Right<R&>
// partial specializations below, modeled on the reference-side of
// std::optional<T&> per P2988R12
// (https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p2988r12.pdf).

namespace smd::typeclass {

/** The stop/short-circuit side of `either<L, R>`. Primary template: an
 * aggregate holding `L` by value.
 */
template <class L>
struct Left {
    L value;

    /** Uniform accessor so generic code (including the `Left<L&>`
     * specialization below) never has to know whether the referent is held
     * by value or by pointer. Library and test code should go through this
     * (or the free function `left(e)`), never `.value`, so the two
     * specializations stay interchangeable.
     */
    constexpr auto referent() const -> const L & { return value; }

    friend constexpr bool operator==(const Left &, const Left &) = default;
};

/** Reference-side specialization: `Left<L&>` holds a pointer to the
 * referent (P2988 model), not a real reference member, so that assignment
 * can *rebind* rather than being deleted (a struct with a true reference
 * member has no assignment operator). Binds lvalues only: the `L&&`
 * constructor is deleted so a temporary can never be bound and then
 * dangle. Copy/assign rebind the pointer (shallow: the wrapper's own
 * constness only ever protects the pointer, never the referent — a
 * `const Left<L&>` still yields a mutable `L&` via `referent()`, exactly
 * like a `const` pointer variable still permitting mutation through it).
 */
template <class L>
struct Left<L &> {
    L *ptr;

    constexpr explicit Left(L &referent) : ptr(&referent) {}
    constexpr Left(L &&) = delete; // no binding to temporaries

    constexpr Left(const Left &) = default;            // rebinds
    constexpr auto operator=(const Left &) -> Left & = default; // rebinds

    constexpr auto referent() const -> L & { return *ptr; }

    friend constexpr bool operator==(const Left &lhs, const Left &rhs) {
        return *lhs.ptr == *rhs.ptr;
    }
};

/** The continue side of `either<L, R>`. Primary template: an aggregate
 * holding `R` by value. Mirrors `Left<L>` exactly.
 */
template <class R>
struct Right {
    R value;

    constexpr auto referent() const -> const R & { return value; }

    friend constexpr bool operator==(const Right &, const Right &) = default;
};

/** Reference-side specialization for the continue side; mirrors
 * `Left<L&>`.
 */
template <class R>
struct Right<R &> {
    R *ptr;

    constexpr explicit Right(R &referent) : ptr(&referent) {}
    constexpr Right(R &&) = delete; // no binding to temporaries

    constexpr Right(const Right &) = default;            // rebinds
    constexpr auto operator=(const Right &) -> Right & = default; // rebinds

    constexpr auto referent() const -> R & { return *ptr; }

    friend constexpr bool operator==(const Right &lhs, const Right &rhs) {
        return *lhs.ptr == *rhs.ptr;
    }
};

// 8cd8e50e-209b-4b8b-8979-06952416c53e
/** Symmetric sum type: exactly one of `Left<L>` (stop) or `Right<R>`
 * (continue). `Left`/`Right` are distinct wrapper types on *both* sides, so
 * `either<T, T>` constructs and compares unambiguously — the failure mode
 * that ruled out `std::expected<T, T>`.
 */
template <class L, class R>
struct either {
    using left_type = L;
    using right_type = R;

    std::variant<Left<L>, Right<R>> node;

    friend constexpr bool operator==(const either &, const either &) = default;
};

/** Constructs the stop side. The result-side type `R` is named first (it
 * cannot be deduced from the argument) and `L` is deduced from `v` — or,
 * for the reference-side instantiation, supplied explicitly as `L&` so `v`
 * is forced to be an lvalue-reference parameter (an rvalue argument fails
 * to bind, closing the dangling-temporary hole at the call site, not just
 * inside `Left<L&>`).
 */
template <class R, class L>
constexpr auto make_left(L v) -> either<L, R> {
    return either<L, R>{std::variant<Left<L>, Right<R>>{Left<L>{v}}};
}

/** Constructs the continue side; dual to `make_left`. */
template <class L, class R>
constexpr auto make_right(R v) -> either<L, R> {
    return either<L, R>{std::variant<Left<L>, Right<R>>{Right<R>{v}}};
}
// 8cd8e50e-209b-4b8b-8979-06952416c53e end

/** True when `e` holds the stop (Left) side. */
template <class L, class R>
constexpr auto is_left(const either<L, R> &e) -> bool {
    return std::holds_alternative<Left<L>>(e.node);
}

/** Observes the stop side. Declared to return `const L&`: when `L` is
 * itself a reference type (the P2988 reference-side instantiation), a
 * `const` applied to a reference type is dropped by the language, so this
 * collapses to plain `L&` — the same signature yields value semantics for
 * plain `L` and shallow-const reference semantics for `L&`, with no
 * special-casing needed at the call site. Generic code therefore cannot
 * tell the two instantiations apart, per design D4/§5.2.
 */
template <class L, class R>
constexpr auto left(const either<L, R> &e) -> const L & {
    return std::get<Left<L>>(e.node).referent();
}

/** Observes the continue side; dual to `left`. */
template <class L, class R>
constexpr auto right(const either<L, R> &e) -> const R & {
    return std::get<Right<R>>(e.node).referent();
}

/** Maps the stop side only, leaving Right untouched; the dual of
 * `map_first` (pair.hpp). No left-functor typeclass is needed for this —
 * it is the one free function callers reach for directly (dist_gapo uses
 * it).
 */
template <class Fn, class L, class R>
constexpr auto map_left(Fn &&fn, const either<L, R> &e) {
    using B = remove_cvref_t<std::invoke_result_t<Fn, const L &>>;
    return std::visit(
        smd::fixpoint::overloaded{
            [&fn](const Left<L> &l) -> either<B, R> {
                return either<B, R>{std::variant<Left<B>, Right<R>>{
                    Left<B>{std::invoke(fn, l.referent())}}};
            },
            [](const Right<R> &r) -> either<B, R> {
                return either<B, R>{
                    std::variant<Left<B>, Right<R>>{r}};
            },
        },
        e.node);
}

/** The coproduct eliminator `[f, g]` (the copairing, D11's dual of
 * building a pair): applies `on_left` to the unwrapped stop referent or
 * `on_right` to the unwrapped continue referent, whichever is present.
 * Both branches must return the same type. Scheme workers (apo, elgot,
 * gana) should prefer this over hand-rolled `is_left` branching.
 */
template <class L, class R, class OnLeft, class OnRight>
constexpr auto match(const either<L, R> &e, OnLeft &&on_left,
                     OnRight &&on_right) {
    return std::visit(
        smd::fixpoint::overloaded{
            [&on_left](const Left<L> &l) {
                return std::invoke(on_left, l.referent());
            },
            [&on_right](const Right<R> &r) {
                return std::invoke(on_right, r.referent());
            },
        },
        e.node);
}

/** Returns the matcher `[f, g]` as a callable value; the dual of
 * `fanout(f, g)` (pair.hpp).
 */
template <class F, class G>
constexpr auto fanin(F f, G g) {
    return [f = std::move(f), g = std::move(g)](const auto &e) {
        return match(e, f, g);
    };
}

// -- Functor (maps Right only) --

template <class L, class R>
struct EitherFunctorImpl {
    template <class Fn>
    constexpr auto fmap(this auto &&, Fn &&fn, const either<L, R> &e) {
        using B = remove_cvref_t<std::invoke_result_t<Fn, const R &>>;
        return std::visit(
            smd::fixpoint::overloaded{
                [](const Left<L> &l) -> either<L, B> {
                    return either<L, B>{
                        std::variant<Left<L>, Right<B>>{l}};
                },
                [&fn](const Right<R> &r) -> either<L, B> {
                    return either<L, B>{std::variant<Left<L>, Right<B>>{
                        Right<B>{std::invoke(fn, r.referent())}}};
                },
            },
            e.node);
    }
};

template <class L, class R>
struct EitherFunctorMap : Functor<EitherFunctorImpl<L, R>> {
    using EitherFunctorImpl<L, R>::fmap;
};

/** Functor instance for `either<L, R>`: maps the continue (Right) side. */
template <class L, class R>
inline constexpr auto functor_typeclass<either<L, R>> =
    EitherFunctorMap<L, R>{};

// -- Monad (right-biased: pure = make_right, bind propagates Left) --

template <class L, class R>
struct EitherMonadImpl {
    template <class VALUE>
    constexpr auto pure(this auto &&, VALUE &&value) {
        return make_right<L>(std::forward<VALUE>(value));
    }

    // Generic over the incoming Right type (its own template parameter
    // `R2`), with `L` fixed to the instance's own `L` — matching
    // OptionalMonadImpl's `bind` being generic in its own template
    // parameter rather than the instance-keying `VALUE_TYPE`. This is what
    // lets the Monad CRTP's derived `join`/`apply`/`kleisli` invoke `bind`
    // on the doubled structure `either<L, either<L, R2>>` they produce;
    // `R` above only serves to key the `monad_typeclass<either<L, R>>`
    // lookup below.
    template <class R2, class F>
    constexpr auto bind(this auto &&, const either<L, R2> &ma, F &&f) {
        using Result = remove_cvref_t<std::invoke_result_t<F, const R2 &>>;
        using ResultRight = typename Result::right_type;
        return std::visit(
            smd::fixpoint::overloaded{
                [](const Left<L> &l) -> Result {
                    return Result{
                        std::variant<Left<L>, Right<ResultRight>>{l}};
                },
                [&f](const Right<R2> &r) -> Result {
                    return std::invoke(f, r.referent());
                },
            },
            ma.node);
    }
};

template <class L, class R>
struct EitherMonadMap : Monad<EitherMonadImpl<L, R>> {
    using EitherMonadImpl<L, R>::bind;
    using EitherMonadImpl<L, R>::pure;
};

/** Monad instance for `either<L, R>`: `pure` = `make_right`, `bind`
 * propagates Left untouched — matching Haskell's Either.
 */
template <class L, class R>
inline constexpr auto monad_typeclass<either<L, R>> = EitherMonadMap<L, R>{};

} // namespace smd::typeclass

#endif // INCLUDED_SMD_TYPECLASS_EITHER
