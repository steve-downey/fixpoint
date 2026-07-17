<div class="abstract" id="org5b3dbf8">
<p>
A fold that can still see the tree it is consuming, and an unfold that can
stop expanding and graft a finished subtree in place. Paramorphism and
apomorphism are the first proper duals in the catalog, and their carriers
&mdash; <code>std::pair</code> and a project <code>either</code> &mdash; are the product and coproduct
the rest of the series is built from.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Part 3 - Fold, Unfold, Refold ←](part-3-classical.md)

</nav>


# para: A Fold That Remembers the Original

`fold_fix` has amnesia by design. By the time the algebra sees an `Add`, the children are already strings or ints; what they *were* is gone. Usually that is the point. Sometimes it is the problem: a pretty-printer that wants minimal parentheses needs each child's rendered string **and** its original shape &mdash; parenthesize the rendering only if the original was an `Add` under a `Mul`.

The paramorphism (Meertens, Lambert, 1992) hands the algebra both. From [`src/smd/fixpoint/para.hpp`](../../src/smd/fixpoint/para.hpp):

```cpp
/** para :: (f (t, a) -> a) -> t -> a
 *
 * Equation: para φ = φ ∘ fmapF (λc. (c, para φ c)) ∘ unfix
 *
 * The pair built at each child position is `{original_subtree,
 * fold_result}` — original first, result second (design §7.2 forward
 * note: later steps, e.g. dist_para, assume this order).
 *
 * @tparam Result carrier type (D5: explicit, cannot be deduced through the
 *   recursive call)
 * @param algebra F<std::pair<Fix<F>, Result>> -> Result
 */
template <class Result, template <class> class F, class Algebra>
constexpr auto para(const Algebra &algebra, const Fix<F> &tree) -> Result {
    const auto &layer = unwrap_fix(tree);
    auto evaluated = layer_fmap(
        [&](const Fix<F> &child) -> std::pair<Fix<F>, Result> {
            return std::pair<Fix<F>, Result>{child,
                                             para<Result>(algebra, child)};
        },
        layer);
    return algebra(evaluated);
}
```

The delta against `fold_fix` is one lambda: instead of mapping children to `Result`, it maps them to `std::pair<Fix<F>, Result>` &mdash; the original subtree first, the fold result second. The algebra's argument type `F<pair<Fix<F>, Result>>` says exactly what is available at each child: everything.

The pretty-printer, from [`src/examples/para_pretty_print.cpp`](../../src/examples/para_pretty_print.cpp):

```cpp
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
```

The `Mul` case is the payoff line: `is_add(child.first)` interrogates the original subtree; `child.second` is the already-rendered string it wraps or passes through. The program prints `2 * 3 + 4` for one association and `2 * (3 + 4)` for the other &mdash; parentheses exactly where meaning requires them, nowhere else.

Degeneracy law: ignore `.first` everywhere and `para` *is* `fold_fix`. The test suite states that literally, and every scheme from here on has a law of the same shape &mdash; forget the extra power, recover the simpler scheme.


# apo: An Unfold That Can Stop Early

Now dualize. `para`'s algebra receives, alongside each result, the original subtree &mdash; a product. `apo`'s coalgebra returns, at each child position, *either* a finished subtree to graft *or* a seed to keep unfolding &mdash; a coproduct (Uustalu, Tarmo and Vene, Varmo, 1999) (Vene, Varmo, 2000). From [`src/smd/fixpoint/apo.hpp`](../../src/smd/fixpoint/apo.hpp):

```cpp
/** apo :: (a -> f (Either t a)) -> a -> t
 *
 * Equation: apo ψ = fix ∘ fmapF (either id (apo ψ)) ∘ ψ
 *
 * Written with `match` (design §5.2), the worker never inspects whether
 * Left holds `Fix<F>` or `const Fix<F>&` — so coalgebras may return
 * `F<either<const Fix<F>&, Seed>>` to graft an existing subtree with no
 * intermediate copy (the zero-copy graft, §7.2): the Left branch simply
 * returns its referent, and returning a `const Fix<F>&` from a
 * `Fix<F>`-returning function is an ordinary copy-construction, not a
 * move-from-const&.
 *
 * @param coalgebra Seed -> F<either<Fix<F>, Seed>> (or the reference-Left
 *   variant above)
 */
template <template <class> class F, class Coalgebra, class Seed>
constexpr auto apo(const Coalgebra &coalgebra, const Seed &seed) -> Fix<F> {
    auto layer = coalgebra(seed);
    auto expanded = layer_fmap(
        [&](const auto &step) -> Fix<F> {
            return smd::typeclass::match(
                step, [](const auto &subtree) -> Fix<F> { return subtree; },
                [&](const auto &next_seed) -> Fix<F> {
                    return apo<F>(coalgebra, next_seed);
                });
        },
        layer);
    return wrap_fix<F>(std::move(expanded));
}
```

The worker `match`-es each child: Left returns its subtree as-is &mdash; no recursion &mdash; and Right recurses like `unfold_fix`. Note what the doc comment promises about references: a coalgebra may return `either<const Fix<F>&, Seed>`, and the graft copies exactly once, at the graft point. The zero-copy graft falls out of `match` never asking which instantiation it holds.

The motivating example is ordered insertion into a sorted list. Unfold the output list element by element while the input's head is smaller; the moment the insertion point appears, emit the new value and graft the *entire untouched remainder* with Left. From [`src/examples/apo_sorted_insert.cpp`](../../src/examples/apo_sorted_insert.cpp):

```cpp
/** Coalgebra for inserting @p value into an already-sorted IntList. Seed =
 * the remaining sublist still to be scanned. Left grafts the *current*
 * seed list (untouched) once the insertion point is found or the list
 * runs out; Right keeps unfolding past elements smaller than @p value.
 */
auto make_insert_coalgebra(int value) {
    return [value](
               const IntList &remaining) -> IntListF<either<IntList, IntList>> {
        const auto &layer = unwrap_fix(remaining);
        return std::visit(
            overloaded{
                [&](const Nil<int> &) -> IntListF<either<IntList, IntList>> {
                    // Ran off the end: value is the new largest element.
                    return Cons<int, either<IntList, IntList>>{
                        value, make_box<either<IntList, IntList>>(
                                   make_left<IntList>(remaining))};
                },
                [&](const Cons<int, IntList> &c)
                    -> IntListF<either<IntList, IntList>> {
                    if (value <= c.head) {
                        // Found the insertion point: graft the whole
                        // current list (starting at c.head) as-is.
                        return Cons<int, either<IntList, IntList>>{
                            value, make_box<either<IntList, IntList>>(
                                       make_left<IntList>(remaining))};
                    }
                    // Keep c.head, keep unfolding on the tail.
                    return Cons<int, either<IntList, IntList>>{
                        c.head, make_box<either<IntList, IntList>>(
                                    make_right<IntList>(*c.tail))};
                },
            },
            layer);
    };
}
```

A plain `unfold_fix` cannot express "and the rest is what it already was" &mdash; it must walk to the end, rebuilding. `apo` says it in one `make_left`.


# either: The Sum the Library Actually Wanted

That Left/Right vocabulary is a project type, and it exists because the obvious candidate failed. From [`src/smd/typeclass/either.hpp`](../../src/smd/typeclass/either.hpp):

```cpp
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
```

`std::expected` was considered and rejected, and the decision log is specific. Its sides are asymmetric: the error side needs the `unexpected` wrapper, error-side mapping is spelled differently, and above all `expected<T, T>` &mdash; which arises *naturally* here, an apo whose Seed is itself `Fix<F>`, as in the insert above &mdash; forces tag-only construction and ambushes generic code. `either` makes `Left` and `Right` distinct wrapper types on **both** sides, so `either<T, T>` constructs and compares without ceremony.

The side convention is fixed once, library-wide, matching the Haskell sources: **Left stops, Right continues**. Apo: Left = graft this subtree. Elgot (Part 10): Left = here is the final answer. The monad instance is right-biased, `pure` is `make_right`, and Part 11 will lean on exactly that when `apo` is recovered from the generalized unfold.

One more symmetry worth seeing now: the design treats `std::pair` and `either` as proper duals &mdash; product and coproduct &mdash; with matched vocabulary on each side: `fanout` builds toward a pair as `fanin` matches away from an either, and the value slot sits second in both (`pair`'s env-comonad `extract` is `.second`; `either`'s monadic `pure` is `make_right`). `para` rides the pair; `apo` rides the either. The duality is not decoration &mdash; in Part 11 it becomes the actual mechanism, when `dist_para` (pair side) and `dist_apo` (either side) sit in the same slot of `gcata` and `gana` respectively.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [Next: Part 5 - Zygomorphisms and Mutumorphisms →](part-5-zygo-mutu.md)

</nav>


# References

Meertens, Lambert (1992). **Paramorphisms**, Formal Aspects of Computing 4(5).

Uustalu, Tarmo and Vene, Varmo (1999). **Primitive (Co)Recursion and Course-of-Value (Co)Iteration, Categorically**, Informatica 10(1).

Vene, Varmo (2000). **Categorical Programming with Inductive and Coinductive Types**, PhD thesis, University of Tartu.

Kilpeläinen, Peter et al. (2025). **std::optional<T&>**, WG21 P2988R12, <https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p2988r12.pdf>.
