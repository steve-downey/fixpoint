// src/smd/fixpoint/functors.hpp                                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_FUNCTORS
#define INCLUDED_SMD_FIXPOINT_FUNCTORS

// Reusable base functors (design §10 table): NatF, ListF<E,·>, TreeF<E,·>,
// ExprF. Each functor family below is laid out types -> functor_typeclass
// instance -> smart constructors/converters, in that order: the
// functor_typeclass partial specialization must be visible before any
// function in this header that calls fold_fix/unfold_fix/refold for that
// layer (see S01 handoff "Discoveries" — the point of instantiation for a
// template used inside a non-template function is essentially immediate).

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/overloaded.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <smd/typeclass/functor.hpp>

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace smd::fixpoint {

// ---------------------------------------------------------------------
// NatF — unary naturals.
// ---------------------------------------------------------------------

// 534bcaa9-4a28-453d-83c3-37d4f479bc8c
struct Zero {
    // Hand-written (not = default): Clang 22 eagerly instantiates a
    // defaulted comparison operator's deleted-ness check inside a class
    // template body, which forces completeness of any self-referential
    // functor family wrapping this type (e.g. Cofree<NatF, Cofree<NatF,A>>,
    // "squared" comonad-duplicate results, S13/S15) before that family has
    // finished being defined — GCC defers this check, so it never hits the
    // cycle. A plain friend body is only instantiated when actually
    // ODR-used (both compilers agree on this), which breaks the cycle.
    friend constexpr auto operator==(const Zero &, const Zero &) -> bool {
        return true;
    }
};

template <typename A>
struct Succ {
    Box<A> pred;

    // Hand-written for the same reason as Zero's above. Member-wise
    // comparison of Box<A>: Box's own operator== already null-checks
    // (box.hpp), so comparing the Box objects directly (not dereferencing)
    // is exactly what `= default` would have produced.
    friend constexpr auto operator==(const Succ &lhs, const Succ &rhs)
        -> bool {
        return lhs.pred == rhs.pred;
    }
};

template <typename A>
using NatF = std::variant<Zero, Succ<A>>;

using Nat = Fix<NatF>;
// 534bcaa9-4a28-453d-83c3-37d4f479bc8c end

} // namespace smd::fixpoint

namespace smd::typeclass {

// ed1f1ead-9b55-4e2c-8ce1-de7f1f0a405b
template <typename A>
struct NatFFunctorImpl {
    template <typename Fn>
    constexpr auto fmap(this auto &&, Fn &&fn,
                         const smd::fixpoint::NatF<A> &layer) {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return std::visit(
            smd::fixpoint::overloaded{
                [](const smd::fixpoint::Zero &) -> smd::fixpoint::NatF<B> {
                    return smd::fixpoint::Zero{};
                },
                [&](const smd::fixpoint::Succ<A> &s)
                    -> smd::fixpoint::NatF<B> {
                    return smd::fixpoint::Succ<B>{
                        smd::fixpoint::make_box<B>(std::invoke(fn, *s.pred))};
                },
            },
            layer);
    }
};

template <typename A>
struct NatFFunctorMap : Functor<NatFFunctorImpl<A>> {
    using NatFFunctorImpl<A>::fmap;
};

template <typename A>
inline constexpr auto functor_typeclass<smd::fixpoint::NatF<A>> =
    NatFFunctorMap<A>{};
// ed1f1ead-9b55-4e2c-8ce1-de7f1f0a405b end

} // namespace smd::typeclass

namespace smd::fixpoint {

/** Build a Nat by successive Succ from zero. */
constexpr auto make_zero() -> Nat { return wrap_fix<NatF>(NatF<Nat>{Zero{}}); }

/** Wrap @p n as the predecessor of a new Succ node. */
constexpr auto make_succ(Nat n) -> Nat {
    return wrap_fix<NatF>(NatF<Nat>{Succ<Nat>{make_box<Nat>(std::move(n))}});
}

// 0cc54e00-8ad2-4f77-a3f1-f3562a923b37
/** Unfold: build a Nat counting down from @p n (ana). */
constexpr auto nat_from_int(int n) -> Nat {
    return unfold_fix<NatF>(
        [](int m) -> NatF<int> {
            if (m <= 0) {
                return Zero{};
            }
            return Succ<int>{make_box<int>(m - 1)};
        },
        n);
}

/** Fold: count the Succ layers of @p nat (cata). */
constexpr auto nat_to_int(const Nat &nat) -> int {
    return fold_fix<int>(
        [](const NatF<int> &layer) -> int {
            return std::visit(overloaded{
                                   [](const Zero &) { return 0; },
                                   [](const Succ<int> &s) { return *s.pred + 1; },
                               },
                               layer);
        },
        nat);
}
// 0cc54e00-8ad2-4f77-a3f1-f3562a923b37 end

// ---------------------------------------------------------------------
// ListF<E, ·> — element-parameterized cons-lists (design D3).
// ---------------------------------------------------------------------

// 9647fdcb-1a8f-4b77-82e6-97b5f8632788
template <typename E>
struct Nil {
    // Hand-written for the same reason as Zero's above (S07).
    friend constexpr auto operator==(const Nil &, const Nil &) -> bool {
        return true;
    }
};

template <typename E, typename A>
struct Cons {
    E head;
    Box<A> tail;

    // Hand-written for the same reason as Nil's above. Member-wise, in
    // declaration order, short-circuiting on the first false — exactly
    // what `= default` would produce.
    friend constexpr auto operator==(const Cons &lhs, const Cons &rhs)
        -> bool {
        return lhs.head == rhs.head && lhs.tail == rhs.tail;
    }
};

template <typename E, typename A>
using ListF = std::variant<Nil<E>, Cons<E, A>>;

template <typename A>
using IntListF = ListF<int, A>;

using IntList = Fix<IntListF>;
// 9647fdcb-1a8f-4b77-82e6-97b5f8632788 end

} // namespace smd::fixpoint

namespace smd::typeclass {

template <typename E, typename A>
struct ListFFunctorImpl {
    template <typename Fn>
    constexpr auto fmap(this auto &&, Fn &&fn,
                         const smd::fixpoint::ListF<E, A> &layer) {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return std::visit(
            smd::fixpoint::overloaded{
                [](const smd::fixpoint::Nil<E> &)
                    -> smd::fixpoint::ListF<E, B> {
                    return smd::fixpoint::Nil<E>{};
                },
                [&](const smd::fixpoint::Cons<E, A> &c)
                    -> smd::fixpoint::ListF<E, B> {
                    return smd::fixpoint::Cons<E, B>{
                        c.head,
                        smd::fixpoint::make_box<B>(std::invoke(fn, *c.tail))};
                },
            },
            layer);
    }
};

template <typename E, typename A>
struct ListFFunctorMap : Functor<ListFFunctorImpl<E, A>> {
    using ListFFunctorImpl<E, A>::fmap;
};

template <typename E, typename A>
inline constexpr auto functor_typeclass<smd::fixpoint::ListF<E, A>> =
    ListFFunctorMap<E, A>{};

} // namespace smd::typeclass

namespace smd::fixpoint {

// f0dc904d-7ea2-4a34-b7c8-1779969b5e42
/** Unfold: build an IntList from a std::vector<int>, front to back (ana). */
constexpr auto list_from_vector(const std::vector<int> &v) -> IntList {
    return unfold_fix<IntListF>(
        [&v](std::size_t i) -> IntListF<std::size_t> {
            if (i >= v.size()) {
                return Nil<int>{};
            }
            return Cons<int, std::size_t>{v[i], make_box<std::size_t>(i + 1)};
        },
        std::size_t{0});
}

/** Fold: collect an IntList into a std::vector<int> (cata). */
constexpr auto list_to_vector(const IntList &list) -> std::vector<int> {
    return fold_fix<std::vector<int>>(
        [](const IntListF<std::vector<int>> &layer) -> std::vector<int> {
            return std::visit(
                overloaded{
                    [](const Nil<int> &) -> std::vector<int> { return {}; },
                    [](const Cons<int, std::vector<int>> &c)
                        -> std::vector<int> {
                        std::vector<int> result{c.head};
                        result.insert(result.end(), c.tail->begin(),
                                      c.tail->end());
                        return result;
                    },
                },
                layer);
        },
        list);
}
// f0dc904d-7ea2-4a34-b7c8-1779969b5e42 end

// ---------------------------------------------------------------------
// TreeF<E, ·> — external binary tree, payload at leaves only.
// ---------------------------------------------------------------------

template <typename E>
struct Leaf {
    E value;
};

template <typename A>
struct Node {
    Box<A> left;
    Box<A> right;
};

template <typename E, typename A>
using TreeF = std::variant<Leaf<E>, Node<A>>;

template <typename A>
using IntTreeF = TreeF<int, A>;

using IntTree = Fix<IntTreeF>;

} // namespace smd::fixpoint

namespace smd::typeclass {

template <typename E, typename A>
struct TreeFFunctorImpl {
    template <typename Fn>
    constexpr auto fmap(this auto &&, Fn &&fn,
                         const smd::fixpoint::TreeF<E, A> &layer) {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return std::visit(
            smd::fixpoint::overloaded{
                [](const smd::fixpoint::Leaf<E> &l)
                    -> smd::fixpoint::TreeF<E, B> {
                    return smd::fixpoint::Leaf<E>{l.value};
                },
                [&](const smd::fixpoint::Node<A> &n)
                    -> smd::fixpoint::TreeF<E, B> {
                    return smd::fixpoint::Node<B>{
                        smd::fixpoint::make_box<B>(std::invoke(fn, *n.left)),
                        smd::fixpoint::make_box<B>(
                            std::invoke(fn, *n.right))};
                },
            },
            layer);
    }
};

template <typename E, typename A>
struct TreeFFunctorMap : Functor<TreeFFunctorImpl<E, A>> {
    using TreeFFunctorImpl<E, A>::fmap;
};

template <typename E, typename A>
inline constexpr auto functor_typeclass<smd::fixpoint::TreeF<E, A>> =
    TreeFFunctorMap<E, A>{};

} // namespace smd::typeclass

namespace smd::fixpoint {

/** Build a leaf holding @p v. */
constexpr auto make_leaf(int v) -> IntTree {
    return wrap_fix<IntTreeF>(IntTreeF<IntTree>{Leaf<int>{v}});
}

/** Build an (payloadless) internal node joining @p l and @p r. */
constexpr auto make_node(IntTree l, IntTree r) -> IntTree {
    return wrap_fix<IntTreeF>(IntTreeF<IntTree>{
        Node<IntTree>{make_box<IntTree>(std::move(l)),
                      make_box<IntTree>(std::move(r))}});
}

// ---------------------------------------------------------------------
// ExprF — small arithmetic expression language (design §10, matches
// src/examples/fixpoint_tree_example.cpp).
// ---------------------------------------------------------------------

// 2c4f33dc-5a80-4afb-8254-b33bab58a0b0
template <typename A>
struct Const {
    int val;
};

template <typename A>
struct Add {
    Box<A> left, right;
};

template <typename A>
struct Mul {
    Box<A> left, right;
};

template <typename A>
using ExprF = std::variant<Const<A>, Add<A>, Mul<A>>;

using Expr = Fix<ExprF>;
// 2c4f33dc-5a80-4afb-8254-b33bab58a0b0 end

} // namespace smd::fixpoint

namespace smd::typeclass {

template <typename A>
struct ExprFFunctorImpl {
    template <typename Fn>
    constexpr auto fmap(this auto &&, Fn &&fn,
                         const smd::fixpoint::ExprF<A> &layer) {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return std::visit(
            smd::fixpoint::overloaded{
                [](const smd::fixpoint::Const<A> &c)
                    -> smd::fixpoint::ExprF<B> {
                    return smd::fixpoint::Const<B>{c.val};
                },
                [&](const smd::fixpoint::Add<A> &a)
                    -> smd::fixpoint::ExprF<B> {
                    return smd::fixpoint::Add<B>{
                        smd::fixpoint::make_box<B>(std::invoke(fn, *a.left)),
                        smd::fixpoint::make_box<B>(
                            std::invoke(fn, *a.right))};
                },
                [&](const smd::fixpoint::Mul<A> &m)
                    -> smd::fixpoint::ExprF<B> {
                    return smd::fixpoint::Mul<B>{
                        smd::fixpoint::make_box<B>(std::invoke(fn, *m.left)),
                        smd::fixpoint::make_box<B>(
                            std::invoke(fn, *m.right))};
                },
            },
            layer);
    }
};

template <typename A>
struct ExprFFunctorMap : Functor<ExprFFunctorImpl<A>> {
    using ExprFFunctorImpl<A>::fmap;
};

template <typename A>
inline constexpr auto functor_typeclass<smd::fixpoint::ExprF<A>> =
    ExprFFunctorMap<A>{};

} // namespace smd::typeclass

namespace smd::fixpoint {

// 5f486336-1b03-4f4a-87b9-ad169d481bd6
/** Build a constant leaf holding @p v. */
constexpr auto const_node(int v) -> Expr {
    return wrap_fix<ExprF>(ExprF<Expr>{Const<Expr>{v}});
}

/** Build @p l + @p r. */
constexpr auto add_node(Expr l, Expr r) -> Expr {
    return wrap_fix<ExprF>(ExprF<Expr>{
        Add<Expr>{make_box<Expr>(std::move(l)), make_box<Expr>(std::move(r))}});
}

/** Build @p l * @p r. */
constexpr auto mul_node(Expr l, Expr r) -> Expr {
    return wrap_fix<ExprF>(ExprF<Expr>{
        Mul<Expr>{make_box<Expr>(std::move(l)), make_box<Expr>(std::move(r))}});
}

/** Fold: evaluate an Expr tree (cata). */
constexpr auto eval(const Expr &tree) -> int {
    return fold_fix<int>(
        [](const ExprF<int> &node) -> int {
            return std::visit(overloaded{
                                   [](const Const<int> &c) { return c.val; },
                                   [](const Add<int> &a) {
                                       return *a.left + *a.right;
                                   },
                                   [](const Mul<int> &m) {
                                       return *m.left * *m.right;
                                   },
                               },
                               node);
        },
        tree);
}
// 5f486336-1b03-4f4a-87b9-ad169d481bd6 end

} // namespace smd::fixpoint

#endif
