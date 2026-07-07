// src/smd/typeclass/detail/typeclass_base.hpp                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_TYPECLASS_DETAIL_TYPECLASS_BASE
#define INCLUDED_SMD_TYPECLASS_DETAIL_TYPECLASS_BASE

#include <optional>
#include <type_traits>

namespace smd::typeclass {

// ===========================================================================
// The typeclass contract (canonical; the per-concept headers refer here)
// ===========================================================================
//
// A typeclass in this library is a compile-time *dictionary of operations*
// selected per concrete type -- a shape, not a type. The whole contract that
// every concept (Functor, Applicative, Monad, Foldable, Traversable, Comonad,
// ...) and every instance must honor lives here so it is stated exactly once.
//
// 1. Shape, not associated type. A typeclass is identified by the set of
//    named operations its dictionary exposes, not by any reified associated
//    type. There is no `Monad::type`; there is only an object that answers
//    `bind`, `pure`, etc. (The only type-level helper is applicative_value_t
//    below, an element-type projection -- not a nominal conformance.)
//
// 2. Duck-typed dispatch through a lookup variable. Each concept owns a
//    variable template, e.g. `template<class T> inline constexpr auto
//    functor_typeclass = std::false_type{};`. A concrete type T "has" the
//    concept iff that variable is specialized for T to a dictionary object
//    exposing the required members. The default `std::false_type{}` is the
//    unregistered sentinel; every CRTP base static_asserts against it to give
//    a readable "no *_typeclass<T> specialization found" diagnostic. Generic
//    algorithms call through the looked-up object; membership is structural
//    (the members are present), never nominal.
//
// 3. Dictionaries are empty, copyable at zero cost. An instance is an empty,
//    stateless value with static storage: `inline constexpr auto
//    xxx_typeclass<T> = XxxMap<...>{};`. It carries no data members, so it is
//    trivially copyable at no cost -- algorithms may take it by value, by
//    reference, or bind it as a reference NTTP without overhead. Do not give
//    a dictionary state.
//
// 4. Names mean the same thing everywhere. An operation name denotes one
//    meaning across every concept that exposes it: `pure` (Applicative,
//    Monad), `apply` (Applicative primitive; Monad-synthesized), `fmap`
//    (Functor; derived on Applicative and Monad) all coincide. This is what
//    lets the concepts share a single object without conflict: a richer
//    concept's dictionary is a *superset* of the weaker ones' surfaces.
//    Never bind a shared name to divergent semantics -- introduce a distinct
//    name or a distinct dictionary/type instead.
//
// 5. Object enrichment (the flattened hierarchy). Because names coincide,
//    each concept's object also exposes the surfaces below it, derived once
//    from its primitives: Applicative is-a Functor, Monad is-a Applicative
//    (see the structural chain in apply.hpp / monad.hpp). So a single Monad
//    registration yields a dictionary answering the full Functor +
//    Applicative + Monad surface. Enrichment is at the *object* level only;
//    implicit `*_typeclass<T>` dispatch stays per-concept, so a hand-written
//    per-concept instance remains authoritative for implicit lookup while the
//    monad object always carries the canonical, law-abiding derivations.
//
// 6. Lookup is explicit and static; never ADL. Algorithms reach operations by
//    consulting the looked-up dictionary object, not by argument-dependent
//    lookup. Do not add a parallel ADL-only customization path for a concept.
//    Three lookup modes are supported -- implicit lookup, NTTP pinning, and
//    explicit-object -- realized canonically by `layer_fmap` in
//    smd/fixpoint/fmap.hpp; new algorithms should follow that shape.
//
// 7. Bases are non-structural; mind deducing-this access. Instances are built
//    as `struct XxxMap : Xxx<XxxImpl<...>> { using XxxImpl::prim; ... };` over
//    a CRTP base `Xxx<Impl> : protected Impl`. The `protected` base makes Map
//    a non-structural type, so NTTP pinning must use a *reference* NTTP
//    (`const auto&`) binding the inline-constexpr variable's static storage
//    (a by-value `auto` NTTP would demand a structural type). A corollary of
//    deducing-this: a `self.member(...)` call is access-checked against
//    `self`'s reified type, so any class-internal helper reached that way must
//    be public (re-exposed via `using` up the chain) or a `detail::` free
//    function -- a private/protected helper becomes inaccessible across a
//    `protected` inheritance link two-plus layers down.
//
// Registration recipe: define `XxxImpl` (the primitive operations), define
// `XxxMap : Xxx<XxxImpl>` re-exposing the primitives via `using`, then
// `inline constexpr auto xxx_typeclass<ConcreteType> = XxxMap{};`.

template <class T>
using remove_cvref_t = std::remove_cvref_t<T>;

/** Trait that extracts the element type from an applicative container.
 * Primary template uses the nested `value_type` alias when present.
 */
template <class T, class = void>
struct applicative_value;

template <class T>
struct applicative_value<T,
                         std::void_t<typename remove_cvref_t<T>::value_type>> {
    using type = typename remove_cvref_t<T>::value_type;
};

template <class T>
struct applicative_value<std::optional<T>, void> {
    using type = T;
};

/** Convenience alias for `applicative_value<T>::type`. */
template <class T>
using applicative_value_t = typename applicative_value<remove_cvref_t<T>>::type;

} // namespace smd::typeclass

#endif // INCLUDED_SMD_TYPECLASS_DETAIL_TYPECLASS_BASE
