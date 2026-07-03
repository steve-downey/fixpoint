// Fixpoint Trees in C++: F-Algebras and Fixpoint Folds
// Self-contained example — paste directly into Compiler Explorer.
// Requires C++26 (GCC 16+).  Uses only standard headers.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <iostream>
#include <memory>
#include <variant>

// Box<A>: value-semantic wrapper for recursive positions.  C++26 std::indirect.
template <typename A>
using Box = std::indirect<A>;

template <typename A, typename... Args>
auto make_box(Args &&...args) -> Box<A> {
    return Box<A>(std::forward<Args>(args)...);
}

// 1. The flat, non-recursive functor.  No inheritance, no CRTP.
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

// 2. The type-level fixed-point combinator.  Pure composition (has-a).
template <template <typename> class F>
struct Fix {
    F<Fix<F>> inner;
};

// 3. Zero-cost phantom operations (isorecursive boundaries)
template <template <typename> class F>
constexpr Fix<F> wrap_fix(F<Fix<F>> f) {
    return Fix<F>{std::move(f)};
}

template <template <typename> class F>
constexpr const F<Fix<F>> &unwrap_fix(const Fix<F> &f) {
    return f.inner;
}

// 4. Visitor helper and fmap
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

template <typename A, typename B, typename Func>
ExprF<B> fmap(Func &&f, const ExprF<A> &fa) {
    return std::visit(
        overloaded{
            [](const Const<A> &c) -> ExprF<B> { return Const<B>{c.val}; },
            [&f](const Add<A> &a) -> ExprF<B> {
                return Add<B>{make_box<B>(f(*a.left)),
                              make_box<B>(f(*a.right))};
            },
            [&f](const Mul<A> &m) -> ExprF<B> {
                return Mul<B>{make_box<B>(f(*m.left)),
                              make_box<B>(f(*m.right))};
            },
        },
        fa);
}

// 5. Fold over a fixpoint type (fold_fix): the universal fold.
//    The trailing return type breaks the recursive deduction cycle.
//    The algebra is applied via std::visit over the evaluated layer.
template <template <typename> class F, typename Algebra>
auto fold_fix(const Algebra &alg, const Fix<F> &tree)
    -> decltype(std::visit(alg, std::declval<F<int>>())) {
    using Carrier = decltype(std::visit(alg, std::declval<F<int>>()));
    return std::visit(alg, fmap<Fix<F>, Carrier>(
                               [&alg](const Fix<F> &child) -> Carrier {
                                   return fold_fix<F>(alg, child);
                               },
                               unwrap_fix(tree)));
}

// 6. Smart constructors hide Fix wrapping and heap allocation.
using ExprTree = Fix<ExprF>;

ExprTree const_node(int v) { return wrap_fix<ExprF>(Const<ExprTree>{v}); }

ExprTree add_node(ExprTree l, ExprTree r) {
    return wrap_fix<ExprF>(Add<ExprTree>{make_box<ExprTree>(std::move(l)),
                                         make_box<ExprTree>(std::move(r))});
}

ExprTree mul_node(ExprTree l, ExprTree r) {
    return wrap_fix<ExprF>(Mul<ExprTree>{make_box<ExprTree>(std::move(l)),
                                         make_box<ExprTree>(std::move(r))});
}

// 7. Usage: building and evaluating an expression tree.
//    The algebra is entirely non-recursive — fold_fix handles the recursion.
int main() {
    // Build the tree: (2 * 3) + 4
    ExprTree tree =
        add_node(mul_node(const_node(2), const_node(3)), const_node(4));

    // Evaluate using a non-recursive algebra
    auto eval_algebra = overloaded{
        [](const Const<int> &c) { return c.val; },
        [](const Add<int> &a) { return *a.left + *a.right; },
        [](const Mul<int> &m) { return *m.left * *m.right; },
    };

    std::cout << "Result: " << fold_fix<ExprF>(eval_algebra, tree) << "\n";
    return 0;
}
