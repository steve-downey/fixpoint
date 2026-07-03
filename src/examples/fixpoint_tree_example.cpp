// fixpoint_tree_example.cpp                                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Demonstrates the smd::fixpoint library: build a small arithmetic
// expression tree and evaluate it via a catamorphism (fold_fix). There is
// no hand-written tree-walking code — only the shape of the data (ExprF)
// and the per-node evaluation logic (eval_algebra).

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/overloaded.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <functional>
#include <print>
#include <utility>
#include <variant>

using smd::fixpoint::Box;
using smd::fixpoint::Fix;
using smd::fixpoint::fold_fix;
using smd::fixpoint::make_box;
using smd::fixpoint::overloaded;
using smd::fixpoint::wrap_fix;

namespace {

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

auto const_node(int v) -> Expr {
    return wrap_fix<ExprF>(ExprF<Expr>{Const<Expr>{v}});
}

auto add_node(Expr l, Expr r) -> Expr {
    return wrap_fix<ExprF>(ExprF<Expr>{
        Add<Expr>{make_box<Expr>(std::move(l)), make_box<Expr>(std::move(r))}});
}

auto mul_node(Expr l, Expr r) -> Expr {
    return wrap_fix<ExprF>(ExprF<Expr>{
        Mul<Expr>{make_box<Expr>(std::move(l)), make_box<Expr>(std::move(r))}});
}

template <typename A, typename F>
auto fmap_expr(F &&f, const ExprF<A> &node) {
    using B = std::invoke_result_t<F, const A &>;
    return std::visit(overloaded{
                           [](const Const<A> &c) -> ExprF<B> {
                               return Const<B>{c.val};
                           },
                           [&f](const Add<A> &a) -> ExprF<B> {
                               return Add<B>{make_box<B>(std::invoke(f, *a.left)),
                                             make_box<B>(std::invoke(f, *a.right))};
                           },
                           [&f](const Mul<A> &m) -> ExprF<B> {
                               return Mul<B>{make_box<B>(std::invoke(f, *m.left)),
                                             make_box<B>(std::invoke(f, *m.right))};
                           },
                       },
                       node);
}

auto fmap_expr_fn = [](auto &&f, const auto &node) {
    return fmap_expr(std::forward<decltype(f)>(f), node);
};

} // namespace

int main() {
    // Build the tree: (2 * 3) + 4
    Expr tree = add_node(mul_node(const_node(2), const_node(3)), const_node(4));

    auto eval_algebra = [](const ExprF<int> &node) -> int {
        return std::visit(overloaded{
                               [](const Const<int> &c) { return c.val; },
                               [](const Add<int> &a) { return *a.left + *a.right; },
                               [](const Mul<int> &m) { return *m.left * *m.right; },
                           },
                           node);
    };

    std::println("Result: {}", fold_fix<int>(eval_algebra, fmap_expr_fn, tree));
}
