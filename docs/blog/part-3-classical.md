<div class="abstract" id="org88ed452">
<p>
Catamorphism, anamorphism, hylomorphism: tear a structure down, build one
up, do both at once without ever holding the structure. The classical trio
from the bananas paper, each a ten-line function template, and the naming
argument for calling them <code>fold_fix</code>, <code>unfold_fix</code>, and <code>refold</code> instead.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Part 2 - Typeclass Objects and layer\_fmap ←](part-2-typeclasses.md)

</nav>


# The Algebra Handles One Layer

An *F-algebra* is a function `F<Result> -> Result`: it consumes one layer whose child positions already hold finished results (Meijer, Erik and Fokkinga, Maarten and Paterson, Ross, 1991). The `eval` from Part 1 is built on exactly such an algebra &#x2014; an `ExprF<int> -> int` where an `Add`'s children are already `int` s. The algebra adds; it never recurses; it cannot even express recursion, because its argument type has no subtrees in it. That typing discipline is the design's whole safety story: the shape of the algebra's argument *proves* the recursion has already happened.

The fold supplies that recursion, once, for every algebra anyone will ever write.


# fold\_fix: The Catamorphism

From [`src/smd/fixpoint/recursion_schemes.hpp`](../../src/smd/fixpoint/recursion_schemes.hpp), the typeclass-lookup forms of all three classical schemes:

```cpp
template <typename Result, template <typename> class F, typename Algebra>
constexpr auto fold_fix(const Algebra &algebra, const Fix<F> &tree)
    -> Result {
    const auto &layer = unwrap_fix(tree);
    auto evaluated = layer_fmap(
        [&](const Fix<F> &child) -> Result {
            return fold_fix<Result>(algebra, child);
        },
        layer);
    return algebra(evaluated);
}

template <template <typename> class F, typename Coalgebra, typename Seed>
constexpr auto unfold_fix(const Coalgebra &coalgebra, const Seed &seed)
    -> Fix<F> {
    auto layer = coalgebra(seed);
    auto expanded = layer_fmap(
        [&](const Seed &child) -> Fix<F> {
            return unfold_fix<F>(coalgebra, child);
        },
        layer);
    return wrap_fix<F>(std::move(expanded));
}

template <typename Result, template <typename> class F, typename Algebra,
          typename Coalgebra, typename Seed>
constexpr auto refold(const Algebra &algebra, const Coalgebra &coalgebra,
                      const Seed &seed) -> Result {
    auto layer = coalgebra(seed);
    auto evaluated = layer_fmap(
        [&](const Seed &child) -> Result {
            return refold<Result, F>(algebra, coalgebra, child);
        },
        layer);
    return algebra(evaluated);
}
```

Read `fold_fix` inside out. `unwrap_fix` exposes the root layer, `F<Fix<F>>` &#x2014; children are subtrees. `layer_fmap` applies the bracketed lambda at every child position, and the lambda is the recursive call; the result is `F<Result>` &#x2014; children are now finished values. The algebra combines them. Three lines of body, and it is the equation `cata φ = φ ∘ fmap (cata φ) ∘ unfix` transcribed with the names this library uses.

One C++ concession, recorded as a design decision: the carrier `Result` is a leading, *explicit* template parameter. Haskell infers the carrier from the algebra; C++ cannot infer a type through a recursive call that returns it. You write `fold_fix<int>(algebra, tree)` and everything else deduces. Every scheme in the series inherits this convention, so I will not mention it again.


# unfold\_fix: The Anamorphism

The dual runs from a seed. A *coalgebra* `Seed -> F<Seed>` produces one layer with fresh seeds in the child positions; `unfold_fix` expands them until the coalgebra stops producing children. Same three-line shape, arrows reversed: coalgebra first, then `layer_fmap` recursing into seeds, then `wrap_fix` instead of an algebra.

The nat and list fixtures the rest of the series leans on are anamorphisms from ordinary C++ values. From [`src/smd/fixpoint/functors.hpp`](../../src/smd/fixpoint/functors.hpp):

```cpp
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
```

`nat_from_int` unfolds a countdown; `nat_to_int` folds it back. The list versions are the same pair at a different functor:

```cpp
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
```

Termination is the programmer's promise, not the type system's. A coalgebra that always returns a `Succ` unfolds forever. The Haskell original has the same property; corecursion is productive, not guaranteed finite.


# refold: The Hylomorphism

Unfold a tree, immediately fold it: the composition is so common it earns a fusion. `refold` interleaves the two so the intermediate `Fix<F>` *never exists* &#x2014; where `fold_fix` unwraps an existing layer, `refold` asks the coalgebra to produce one; where `unfold_fix` wraps, `refold` hands the layer straight to the algebra. Look back at the transclusion above: `refold`'s body is `fold_fix`'s body with `unwrap_fix(tree)` replaced by `coalgebra(seed)`. The defining law, which the test suite pins:

```
refold<R, F>(alg, coalg, seed) == fold_fix<R>(alg, unfold_fix<F>(coalg, seed))
```

Same answer, no tree. Everything in Part 8 &#x2014; dynamic programming straight from a seed &#x2014; is this idea wearing a comonad.


# The Explicit-fmap Escape Hatch

The same header keeps an older overload family that takes `fmap` as an ordinary function argument instead of consulting the typeclass registry:

```cpp
template <typename Result, template <typename> class F, typename Algebra,
          typename FMap>
constexpr auto fold_fix(const Algebra &algebra, const FMap &fmap_fn,
                        const Fix<F> &tree) -> Result {
    const auto &layer = unwrap_fix(tree);
    auto evaluated = fmap_fn(
        [&](const Fix<F> &child) -> Result {
            return fold_fix<Result>(algebra, fmap_fn, child);
        },
        layer);
    return algebra(evaluated);
}

template <template <typename> class F, typename Coalgebra, typename FMap,
          typename Seed>
constexpr auto unfold_fix(const Coalgebra &coalgebra, const FMap &fmap_fn,
                          const Seed &seed) -> Fix<F> {
    auto layer = coalgebra(seed);
    auto expanded = fmap_fn(
        [&](const Seed &child) -> Fix<F> {
            return unfold_fix<F>(coalgebra, fmap_fn, child);
        },
        layer);
    return wrap_fix<F>(std::move(expanded));
}

template <typename Result, template <typename> class F, typename Algebra,
          typename Coalgebra, typename FMap, typename Seed>
constexpr auto refold(const Algebra &algebra, const Coalgebra &coalgebra,
                      const FMap &fmap_fn, const Seed &seed) -> Result {
    auto layer = coalgebra(seed);
    auto evaluated = fmap_fn(
        [&](const Seed &child) -> Result {
            return refold<Result, F>(algebra, coalgebra, fmap_fn, child);
        },
        layer);
    return algebra(evaluated);
}
```

These predate `layer_fmap` and stay for two reasons. The existing tests depend on them. And they remain the zero-machinery path for a one-off functor that will never earn a `functor_typeclass` instance &#x2014; mode 3 of Part 2, before mode 3 existed.


# Why Not cata?

The library deliberately renamed the trio, and deprecated `cata` rather than keep it as a synonym. The reasoning, from the decision log: *catamorphism*, *anamorphism*, and *hylomorphism* are precise terms, but they are jargon most C++ programmers have no reason to know, and these three schemes are the ones ordinary code meets first. Haskell's `data-fix` (Kholomiov, Anton and Kmett, Edward and others, ????) reached the same conclusion &#x2014; `foldFix`, `unfoldFix`, `refold` &#x2014; and this library follows it exactly. The schemes still to come keep their literature names (`para`, `zygo`, `histo`, &#x2026;) because they are terms of art with no adequate descriptive equivalents; a "helper-consulting fold" tells you less than *zygomorphism* once you know the word. Descriptive names for the common; precise names for the precise.

The classical trio is the baseline. Every scheme from here on is one of these three with a more interesting carrier &#x2014; and the series ends, in Parts 11 and 12, by making that sentence a theorem.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [Next: Part 4 - Paramorphisms and Apomorphisms →](part-4-para-apo.md)

</nav>


# References

Meijer, Erik, Fokkinga, Maarten, and Paterson, Ross (1991). **Functional Programming with Bananas, Lenses, Envelopes and Barbed Wire**, FPCA '91, Springer LNCS 523.

Kholomiov, Anton and Kmett, Edward. **data-fix: Fixpoint data types**, Hackage, <https://hackage.haskell.org/package/data-fix>.
