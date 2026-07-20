// src/smd/concrete/functors.hpp                                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_CONCRETE_FUNCTORS
#define INCLUDED_SMD_CONCRETE_FUNCTORS

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
#include <smd/typeclass/sequence.hpp>

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace smd::concrete {

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
    smd::fixpoint::Box<A> pred;

    // Hand-written for the same reason as Zero's above. Member-wise
    // comparison of Box<A>: Box's own operator== already null-checks
    // (box.hpp), so comparing the Box objects directly (not dereferencing)
    // is exactly what `= default` would have produced.
    friend constexpr auto operator==(const Succ &lhs, const Succ &rhs) -> bool {
        return lhs.pred == rhs.pred;
    }
};

template <typename A>
using NatF = std::variant<Zero, Succ<A>>;

using Nat = smd::fixpoint::Fix<NatF>;
// 534bcaa9-4a28-453d-83c3-37d4f479bc8c end

} // namespace smd::concrete

namespace smd::typeclass {

// ed1f1ead-9b55-4e2c-8ce1-de7f1f0a405b
template <typename A>
struct NatFFunctorImpl {
    template <typename Fn>
    constexpr auto fmap(this auto &&, Fn &&fn,
                        const smd::concrete::NatF<A> &layer) {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return std::visit(
            smd::fixpoint::overloaded{
                [](const smd::concrete::Zero &) -> smd::concrete::NatF<B> {
                    return smd::concrete::Zero{};
                },
                [&](const smd::concrete::Succ<A> &s) -> smd::concrete::NatF<B> {
                    return smd::concrete::Succ<B>{
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
inline constexpr auto functor_typeclass<smd::concrete::NatF<A>> =
    NatFFunctorMap<A>{};
// ed1f1ead-9b55-4e2c-8ce1-de7f1f0a405b end

} // namespace smd::typeclass

namespace smd::concrete {

/** Build a Nat by successive Succ from zero. */
constexpr auto make_zero() -> Nat {
    return smd::fixpoint::wrap_fix<NatF>(NatF<Nat>{Zero{}});
}

/** Wrap @p n as the predecessor of a new Succ node. */
constexpr auto make_succ(Nat n) -> Nat {
    return smd::fixpoint::wrap_fix<NatF>(
        NatF<Nat>{Succ<Nat>{smd::fixpoint::make_box<Nat>(std::move(n))}});
}

// 0cc54e00-8ad2-4f77-a3f1-f3562a923b37
/** Unfold: build a Nat counting down from @p n (ana). */
constexpr auto nat_from_int(int n) -> Nat {
    return smd::fixpoint::unfold_fix<NatF>(
        [](int m) -> NatF<int> {
            if (m <= 0) {
                return Zero{};
            }
            return Succ<int>{smd::fixpoint::make_box<int>(m - 1)};
        },
        n);
}

/** Fold: count the Succ layers of @p nat (cata). */
constexpr auto nat_to_int(const Nat &nat) -> int {
    return smd::fixpoint::fold_fix<int>(
        [](const NatF<int> &layer) -> int {
            return std::visit(
                smd::fixpoint::overloaded{
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
    smd::fixpoint::Box<A> tail;

    // Hand-written for the same reason as Nil's above. Member-wise, in
    // declaration order, short-circuiting on the first false — exactly
    // what `= default` would produce.
    friend constexpr auto operator==(const Cons &lhs, const Cons &rhs) -> bool {
        return lhs.head == rhs.head && lhs.tail == rhs.tail;
    }
};

template <typename E, typename A>
using ListF = std::variant<Nil<E>, Cons<E, A>>;

template <typename A>
using IntListF = ListF<int, A>;

using IntList = smd::fixpoint::Fix<IntListF>;
// 9647fdcb-1a8f-4b77-82e6-97b5f8632788 end

} // namespace smd::concrete

namespace smd::typeclass {

template <typename E, typename A>
struct ListFFunctorImpl {
    template <typename Fn>
    constexpr auto fmap(this auto &&, Fn &&fn,
                        const smd::concrete::ListF<E, A> &layer) {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return std::visit(smd::fixpoint::overloaded{
                              [](const smd::concrete::Nil<E> &)
                                  -> smd::concrete::ListF<E, B> {
                                  return smd::concrete::Nil<E>{};
                              },
                              [&](const smd::concrete::Cons<E, A> &c)
                                  -> smd::concrete::ListF<E, B> {
                                  return smd::concrete::Cons<E, B>{
                                      c.head, smd::fixpoint::make_box<B>(
                                                  std::invoke(fn, *c.tail))};
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
inline constexpr auto functor_typeclass<smd::concrete::ListF<E, A>> =
    ListFFunctorMap<E, A>{};

} // namespace smd::typeclass

namespace smd::concrete {

// f0dc904d-7ea2-4a34-b7c8-1779969b5e42
/** Unfold: build an IntList from a std::vector<int>, front to back (ana). */
constexpr auto list_from_vector(const std::vector<int> &v) -> IntList {
    return smd::fixpoint::unfold_fix<IntListF>(
        [&v](std::size_t i) -> IntListF<std::size_t> {
            if (i >= v.size()) {
                return Nil<int>{};
            }
            return Cons<int, std::size_t>{
                v[i], smd::fixpoint::make_box<std::size_t>(i + 1)};
        },
        std::size_t{0});
}

/** Fold: collect an IntList into a std::vector<int> (cata). */
constexpr auto list_to_vector(const IntList &list) -> std::vector<int> {
    return smd::fixpoint::fold_fix<std::vector<int>>(
        [](const IntListF<std::vector<int>> &layer) -> std::vector<int> {
            return std::visit(
                smd::fixpoint::overloaded{
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
    smd::fixpoint::Box<A> left;
    smd::fixpoint::Box<A> right;
};

template <typename E, typename A>
using TreeF = std::variant<Leaf<E>, Node<A>>;

template <typename A>
using IntTreeF = TreeF<int, A>;

using IntTree = smd::fixpoint::Fix<IntTreeF>;

} // namespace smd::concrete

namespace smd::typeclass {

template <typename E, typename A>
struct TreeFFunctorImpl {
    template <typename Fn>
    constexpr auto fmap(this auto &&, Fn &&fn,
                        const smd::concrete::TreeF<E, A> &layer) {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return std::visit(
            smd::fixpoint::overloaded{
                [](const smd::concrete::Leaf<E> &l)
                    -> smd::concrete::TreeF<E, B> {
                    return smd::concrete::Leaf<E>{l.value};
                },
                [&](const smd::concrete::Node<A> &n)
                    -> smd::concrete::TreeF<E, B> {
                    return smd::concrete::Node<B>{
                        smd::fixpoint::make_box<B>(std::invoke(fn, *n.left)),
                        smd::fixpoint::make_box<B>(std::invoke(fn, *n.right))};
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
inline constexpr auto functor_typeclass<smd::concrete::TreeF<E, A>> =
    TreeFFunctorMap<E, A>{};

} // namespace smd::typeclass

namespace smd::concrete {

/** Build a leaf holding @p v. */
constexpr auto make_leaf(int v) -> IntTree {
    return smd::fixpoint::wrap_fix<IntTreeF>(IntTreeF<IntTree>{Leaf<int>{v}});
}

/** Build an (payloadless) internal node joining @p l and @p r. */
constexpr auto make_node(IntTree l, IntTree r) -> IntTree {
    return smd::fixpoint::wrap_fix<IntTreeF>(IntTreeF<IntTree>{
        Node<IntTree>{smd::fixpoint::make_box<IntTree>(std::move(l)),
                      smd::fixpoint::make_box<IntTree>(std::move(r))}});
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
    smd::fixpoint::Box<A> left, right;
};

template <typename A>
struct Mul {
    smd::fixpoint::Box<A> left, right;
};

template <typename A>
using ExprF = std::variant<Const<A>, Add<A>, Mul<A>>;

using Expr = smd::fixpoint::Fix<ExprF>;
// 2c4f33dc-5a80-4afb-8254-b33bab58a0b0 end

} // namespace smd::concrete

namespace smd::typeclass {

template <typename A>
struct ExprFFunctorImpl {
    template <typename Fn>
    constexpr auto fmap(this auto &&, Fn &&fn,
                        const smd::concrete::ExprF<A> &layer) {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return std::visit(
            smd::fixpoint::overloaded{
                [](const smd::concrete::Const<A> &c)
                    -> smd::concrete::ExprF<B> {
                    return smd::concrete::Const<B>{c.val};
                },
                [&](const smd::concrete::Add<A> &a) -> smd::concrete::ExprF<B> {
                    return smd::concrete::Add<B>{
                        smd::fixpoint::make_box<B>(std::invoke(fn, *a.left)),
                        smd::fixpoint::make_box<B>(std::invoke(fn, *a.right))};
                },
                [&](const smd::concrete::Mul<A> &m) -> smd::concrete::ExprF<B> {
                    return smd::concrete::Mul<B>{
                        smd::fixpoint::make_box<B>(std::invoke(fn, *m.left)),
                        smd::fixpoint::make_box<B>(std::invoke(fn, *m.right))};
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
inline constexpr auto functor_typeclass<smd::concrete::ExprF<A>> =
    ExprFFunctorMap<A>{};

} // namespace smd::typeclass

namespace smd::concrete {

// 5f486336-1b03-4f4a-87b9-ad169d481bd6
/** Build a constant leaf holding @p v. */
constexpr auto const_node(int v) -> Expr {
    return smd::fixpoint::wrap_fix<ExprF>(ExprF<Expr>{Const<Expr>{v}});
}

/** Build @p l + @p r. */
constexpr auto add_node(Expr l, Expr r) -> Expr {
    return smd::fixpoint::wrap_fix<ExprF>(
        ExprF<Expr>{Add<Expr>{smd::fixpoint::make_box<Expr>(std::move(l)),
                              smd::fixpoint::make_box<Expr>(std::move(r))}});
}

/** Build @p l * @p r. */
constexpr auto mul_node(Expr l, Expr r) -> Expr {
    return smd::fixpoint::wrap_fix<ExprF>(
        ExprF<Expr>{Mul<Expr>{smd::fixpoint::make_box<Expr>(std::move(l)),
                              smd::fixpoint::make_box<Expr>(std::move(r))}});
}

/** Fold: evaluate an Expr tree (cata). */
constexpr auto eval(const Expr &tree) -> int {
    return smd::fixpoint::fold_fix<int>(
        [](const ExprF<int> &node) -> int {
            return std::visit(
                smd::fixpoint::overloaded{
                    [](const Const<int> &c) { return c.val; },
                    [](const Add<int> &a) { return *a.left + *a.right; },
                    [](const Mul<int> &m) { return *m.left * *m.right; },
                },
                node);
        },
        tree);
}
// 5f486336-1b03-4f4a-87b9-ad169d481bd6 end

// ---------------------------------------------------------------------
// RoseF<E, ·> — labeled rose tree: one E label per node, arbitrarily many
// recursive positions. Not a variant — there is only one node shape — and
// no Box: std::vector supports an incomplete element type, so vector<A>
// with A = Fix<RoseF<E, ·>> is a complete member type on its own.
// ---------------------------------------------------------------------

// 6c2a8614-33ab-4986-ab9d-3d97e461b50e
template <typename E, typename A>
struct RoseF {
    E label;
    std::vector<A> children;

    // No operator== — Fix<F> (the A that matters) has no equality to
    // compare children with; tests observe fields directly instead.
};
// 6c2a8614-33ab-4986-ab9d-3d97e461b50e end

} // namespace smd::concrete

namespace smd::typeclass {

// 44131dae-7fa9-4705-8a5c-0d408da83008
// RoseF's instances delegate the child positions to std::vector's
// registered instances: the rose layer is label ⊗ vector, so mapping,
// folding, and traversing a layer is the vector operation with the label
// carried across unchanged.

template <typename E, typename A>
struct RoseFFunctorImpl {
    template <typename Fn>
    auto fmap(this auto &&, Fn &&fn, const smd::concrete::RoseF<E, A> &layer) {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return smd::concrete::RoseF<E, B>{
            layer.label, functor_typeclass<std::vector<A>>.fmap(
                             std::forward<Fn>(fn), layer.children)};
    }
};

template <typename E, typename A>
struct RoseFFunctorMap : Functor<RoseFFunctorImpl<E, A>> {
    using RoseFFunctorImpl<E, A>::fmap;
};

template <typename E, typename A>
inline constexpr auto functor_typeclass<smd::concrete::RoseF<E, A>> =
    RoseFFunctorMap<E, A>{};

template <typename E, typename A>
struct RoseFFoldableImpl {
    template <typename Fn>
    auto fold_map(this auto &&, Fn &&fn,
                  const smd::concrete::RoseF<E, A> &layer) {
        return foldable_typeclass<std::vector<A>>.fold_map(std::forward<Fn>(fn),
                                                           layer.children);
    }
};

template <typename E, typename A>
struct RoseFFoldableMap : Foldable<RoseFFoldableImpl<E, A>> {
    using RoseFFoldableImpl<E, A>::fold_map;
};

template <typename E, typename A>
inline constexpr auto foldable_typeclass<smd::concrete::RoseF<E, A>> =
    RoseFFoldableMap<E, A>{};

template <typename E, typename A>
struct RoseFTraversableImpl {
    using element_type = A;

    template <typename APPLICATIVE, typename Fn>
    auto traverse(this auto &&, const APPLICATIVE &applicative, Fn &&fn,
                  const smd::concrete::RoseF<E, A> &layer) {
        return applicative.fmap(
            [label = layer.label](const auto &children) {
                using B = typename std::remove_cvref_t<
                    decltype(children)>::value_type;
                return smd::concrete::RoseF<E, B>{label, children};
            },
            traversable_typeclass<std::vector<A>>.traverse(
                applicative, std::forward<Fn>(fn), layer.children));
    }
};

template <typename E, typename A>
struct RoseFTraversableMap : Traversable<RoseFTraversableImpl<E, A>> {
    using RoseFTraversableImpl<E, A>::traverse;
};

template <typename E, typename A>
inline constexpr auto traversable_typeclass<smd::concrete::RoseF<E, A>> =
    RoseFTraversableMap<E, A>{};
// 44131dae-7fa9-4705-8a5c-0d408da83008 end

} // namespace smd::typeclass

namespace smd::concrete {

// e37d750c-8369-4173-82db-2eb648328c6d
/** Build a rose-tree node labeled @p label with @p children subtrees. */
template <template <typename> class F, typename E>
auto rose_node(E label, std::vector<smd::fixpoint::Fix<F>> children = {})
    -> smd::fixpoint::Fix<F> {
    return smd::fixpoint::wrap_fix<F>(
        F<smd::fixpoint::Fix<F>>{std::move(label), std::move(children)});
}
// e37d750c-8369-4173-82db-2eb648328c6d end

} // namespace smd::concrete

#endif
