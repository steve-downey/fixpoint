// mendler_eval.cpp                                                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Demonstrates mcata (design §7.7): expression evaluation via a Mendler-style
// catamorphism. Contrast with functors.hpp's own `eval`, which folds via
// fold_fix: it fmaps a precomputed-Result layer through
// smd::typeclass::functor_typeclass<ExprF<A>> and the algebra only ever sees
// already-folded ints. Here the algebra sees the *unevaluated* layer
// (ExprF<Expr>, boxed children still trees) plus a `recurse` callable, and
// decides for itself which children to fold and how to combine them — no
// functor_typeclass lookup happens anywhere in mcata (mendler.hpp) itself.
//
// The abstraction discipline (unenforceable in C++, design §7.7): `recurse`
// is the *only* thing the algebra may do with a child. It would compile
// just as well to reach into `*a.left`/`*m.left` directly (they're plain
// Expr values) and bypass `recurse` entirely — but doing so would silently
// break the "this is Φ, not something's own hand-rolled walk" contract that
// makes mcata mcata. Every branch below only ever calls `recurse` on a
// child, never inspects one any other way.

#include <smd/fixpoint/functors.hpp>
#include <smd/fixpoint/mendler.hpp>

#include <print>
#include <variant>

using smd::fixpoint::Add;
using smd::fixpoint::add_node;
using smd::fixpoint::Const;
using smd::fixpoint::const_node;
using smd::fixpoint::Expr;
using smd::fixpoint::ExprF;
using smd::fixpoint::mcata;
using smd::fixpoint::mul_node;
using smd::fixpoint::Mul;
using smd::fixpoint::overloaded;

namespace {

// The Mendler algebra: (Recurse, const ExprF<Expr>&) -> int. `recurse` is
// `mcata Φ` partially applied (const Expr& -> int) — closed over by mcata
// itself, never named as a type (design §7.7's pitfall: keep it a plain
// generic callable).
auto eval_via_mcata = [](auto recurse, const ExprF<Expr> &layer) -> int {
    return std::visit(
        overloaded{
            [](const Const<Expr> &c) { return c.val; },
            [&](const Add<Expr> &a) -> int {
                return recurse(*a.left) + recurse(*a.right);
            },
            [&](const Mul<Expr> &m) -> int {
                return recurse(*m.left) * recurse(*m.right);
            },
        },
        layer);
};

} // namespace

int main() {
    // (2 * 3) + 4
    Expr e = add_node(mul_node(const_node(2), const_node(3)), const_node(4));
    std::println("mcata eval: {}", mcata<int, ExprF>(eval_via_mcata, e));
}
