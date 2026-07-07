// para_pretty_print.cpp                                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Demonstrates para (design §7.2): a minimal-parens pretty-printer for
// ExprF (from smd/concrete/functors.hpp). Unlike a plain fold_fix, the
// algebra sees both the *original* subtree and its rendered string at
// every child position, so it can inspect the child's shape (is it an
// Add?) to decide whether it needs parenthesizing under the parent
// operator — printing "2 * 3 + 4" but "2 * (3 + 4)".

#include <smd/concrete/functors.hpp>
#include <smd/fixpoint/para.hpp>

#include <print>
#include <string>
#include <utility>
#include <variant>

using smd::concrete::Add;
using smd::concrete::add_node;
using smd::concrete::Const;
using smd::concrete::const_node;
using smd::concrete::Expr;
using smd::concrete::ExprF;
using smd::concrete::Mul;
using smd::concrete::mul_node;
using smd::fixpoint::overloaded;
using smd::fixpoint::para;
using smd::fixpoint::unwrap_fix;

namespace {

// bcdeef00-3342-4245-aca3-f4a63d366637
/** True when @p e's top-level node is Add — the only shape that needs
 * parenthesizing, and only when it appears directly under a Mul.
 */
auto is_add(const Expr &e) -> bool {
    return std::holds_alternative<Add<Expr>>(unwrap_fix(e));
}

// The para algebra: F<std::pair<Expr, std::string>> -> std::string. Each
// child arrives as {original subtree, its rendered string}; the Mul case
// consults `.first` (the original subtree) to decide whether `.second`
// (the rendered string) needs wrapping in parentheses.
auto pretty =
    [](const ExprF<std::pair<Expr, std::string>> &layer) -> std::string {
    return std::visit(overloaded{
                          [](const Const<std::pair<Expr, std::string>> &c) {
                              return std::to_string(c.val);
                          },
                          [](const Add<std::pair<Expr, std::string>> &a) {
                              // Addition is the lowest-precedence operator
                              // here, so neither child (Const, Add, or Mul)
                              // ever needs parens.
                              return a.left->second + " + " + a.right->second;
                          },
                          [](const Mul<std::pair<Expr, std::string>> &m) {
                              auto render_child =
                                  [](const std::pair<Expr, std::string> &child)
                                  -> std::string {
                                  if (is_add(child.first)) {
                                      return "(" + child.second + ")";
                                  }
                                  return child.second;
                              };
                              return render_child(*m.left) + " * " +
                                     render_child(*m.right);
                          },
                      },
                      layer);
};
// bcdeef00-3342-4245-aca3-f4a63d366637 end

} // namespace

int main() {
    // (2 * 3) + 4 — Mul under Add: no parens needed.
    Expr no_parens_needed =
        add_node(mul_node(const_node(2), const_node(3)), const_node(4));
    std::println("{}", para<std::string>(pretty, no_parens_needed));

    // 2 * (3 + 4) — Add under Mul: parens required to preserve meaning.
    Expr parens_needed =
        mul_node(const_node(2), add_node(const_node(3), const_node(4)));
    std::println("{}", para<std::string>(pretty, parens_needed));
}
