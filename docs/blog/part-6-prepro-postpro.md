<div class="abstract" id="org744c66e">
<p>
So far the algebras have decided what to compute; the <i>shape</i> of the tree
was taken as given. A natural transformation rewrites the shape itself,
one layer at a time, uniformly at every type &mdash; and Fokkinga's
prepromorphism and postpromorphism fuse that rewriting into a fold on the
way down, or an unfold on the way out. Take-while disappears into the sum
that consumes it.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Part 5 - Zygomorphisms and Mutumorphisms ←](part-5-zygo-mutu.md)

</nav>


# What "Natural" Buys You

A natural transformation between functors is a function `e : F<X> -> G<X>` that works *for every X* &mdash; it may rearrange the layer's structure, but it cannot look at the `X` s, so it cannot care whether the children are subtrees, seeds, or finished results. In C++ that "for every X" is a polymorphic function object with a **templated** call operator. The header is emphatic about this, because the failure mode is nasty: a lambda with a concrete layer parameter type-checks at one call site and then `prepro` applies the transformation at a *different* instantiation &mdash; whole subtrees, not carriers &mdash; and it fails to compile, or worse, silently picks another overload. The example fixture shows the required shape:

```cpp
// The natural transformation: IntListF<A> -> IntListF<A> for every A. Must
// have a templated call operator (design §4) — it runs at F<Fix<F>> during
// prepro's cumulative hoisting, not at the algebra's concrete carrier.
struct take_while_positive {
    template <class A>
    constexpr auto operator()(const IntListF<A> &layer) const -> IntListF<A> {
        return std::visit(
            overloaded{
                [](const Nil<int> &n) -> IntListF<A> { return n; },
                [](const Cons<int, A> &c) -> IntListF<A> {
                    if (c.head < 0) {
                        return Nil<int>{};
                    }
                    return c;
                },
            },
            layer);
    }
};

// The algebra: sum every remaining head.
auto sum_algebra(const IntListF<int> &layer) -> int {
    return std::visit(
        overloaded{
            [](const Nil<int> &) { return 0; },
            [](const Cons<int, int> &c) { return c.head + *c.tail; },
        },
        layer);
}
```

`take_while_positive` rewrites one list layer: a `Cons` with a negative head becomes `Nil`; everything else passes through. Templated over `A`, so it applies at any element type &mdash; which is exactly the license `prepro` needs.


# hoist: Retag Every Layer

The simplest consumer of a natural transformation rewrites a whole tree. From [`src/smd/fixpoint/prepro.hpp`](../../src/smd/fixpoint/prepro.hpp):

```cpp
/** hoist :: (forall x. f x -> g x) -> Fix f -> Fix g
 *
 * Equation: hoist e = fold_fix(wrap_fix<G> ∘ e)
 *
 * Retags every layer of the tree from functor @p F to functor @p G via the
 * natural transformation @p e, applied bottom-up (a fold whose algebra is
 * "transform this layer, then wrap it"). For the common endo case (@p G ==
 * @p F, i.e. rewriting a tree's layers without changing its functor), the
 * target functor still has to be given explicitly since it cannot be
 * deduced from @p e alone: call as `hoist<F>(e, t)`.
 *
 * @tparam G target functor (D5: explicit, cannot be deduced from @p e)
 * @tparam F source functor (deduced from @p tree)
 * @param e F<X> -> G<X> for every X
 */
template <template <class> class G, template <class> class F, class Nat>
constexpr auto hoist(const Nat &e, const Fix<F> &tree) -> Fix<G> {
    return fold_fix<Fix<G>>(
        [&](const F<Fix<G>> &layer) -> Fix<G> { return wrap_fix<G>(e(layer)); },
        tree);
}
```

`hoist` is a `fold_fix` whose algebra is "transform the layer, wrap it again" &mdash; so it can even change the functor, `Fix<F>` to `Fix<G>`. The endo case `hoist<F>(e, t)` &mdash; same functor, rewritten layers &mdash; is the workhorse; the target functor is named explicitly because nothing about `e` (a polymorphic object, remember) lets C++ deduce it.


# prepro: The Rewrite Fused Into the Fold

Now fuse. `prepro` folds a tree, but on the way down it applies `e` to each child's *entire subtree* before recursing:

```cpp
/** prepro (Fokkinga) :: (forall x. f x -> f x) -> (f a -> a) -> Fix f -> a
 *
 * Equation: prepro e φ = φ ∘ fmapF (prepro e φ ∘ hoist<F>(e)) ∘ unfix
 *
 * A fold whose algebra sees, at each child, the result of first
 * rewriting the *entire child subtree* via `hoist<F>(e)` and then
 * recursing prepro on the rewritten subtree. This is the "transform on
 * the way down" dual to postpro's "transform on the way out".
 *
 * Cumulative-cost note: because each recursive step re-hoists the whole
 * remaining subtree before descending into it, a node at depth k has had
 * @p e applied to it k times by the time prepro's algebra sees its
 * layer (once per ancestor, each hoist itself walking every layer below
 * it). This is the classical (and expensive) reading of prepro — Fokkinga's
 * equation is transcribed literally here, not fused/memoized. At the small
 * tree sizes exercised by this module's tests it is not observable, but it
 * is worth knowing before reaching for prepro over anything but small or
 * shallow trees.
 *
 * @tparam Result carrier type (D5: explicit)
 * @tparam F functor (deduced from @p tree)
 * @param e F<X> -> F<X> for every X (a natural transformation, endo case)
 * @param algebra F<Result> -> Result
 */
template <class Result, template <class> class F, class Nat, class Algebra>
constexpr auto prepro(const Nat &e, const Algebra &algebra, const Fix<F> &tree)
    -> Result {
    const auto &layer = unwrap_fix(tree);
    auto evaluated = layer_fmap(
        [&](const Fix<F> &child) -> Result {
            return prepro<Result>(e, algebra, hoist<F>(e, child));
        },
        layer);
    return algebra(evaluated);
}
```

The body is `fold_fix` with one insertion: `hoist<F>(e, child)` ahead of the recursive call. The effect is **cumulative**: a node at depth `k` has passed through `e` once per ancestor by the time the algebra sees it. For take-while, cumulativity is the semantics you want &mdash; once any ancestor's rewrite truncated the list, everything below the cut is already gone, and the summing algebra never sees a single dropped element:

```
sum(take_while(>=0, [3, 4, -1, 5])) = 7
```

No separate filtering pass, no intermediate list. The full program is [`src/examples/prepro_takewhile_sum.cpp`](../../src/examples/prepro_takewhile_sum.cpp).

The header's cumulative-cost note deserves its moment: each step re-hoists the whole remaining subtree, so this literal transcription of Fokkinga's equation (Fokkinga, Maarten M., 1992) does O(depth) passes over deep nodes. The library documents the cost instead of optimizing it away &mdash; the transcription *is* the product; fusion belongs to a different layer of the story (and Part 11's `gprepro` shows where it would live).


# postpro: The Mirror

The dual rewrites on the way *out* of an unfold:

```cpp
/** postpro :: (forall x. f x -> f x) -> (a -> f a) -> a -> Fix f
 *
 * Equation: postpro e ψ = fix ∘ fmapF (hoist<F>(e) ∘ postpro e ψ) ∘ ψ
 *
 * Dual to prepro: an unfold whose coalgebra's children are first unfolded
 * as usual, then the *entire resulting subtree* is rewritten via
 * `hoist<F>(e)` before being grafted in. Same cumulative-cost shape as
 * prepro, mirrored: a node at depth k has been hoisted k times by the time
 * it is grafted into the final tree.
 *
 * @tparam F functor (D5: explicit, cannot be deduced from @p seed alone)
 * @param e F<X> -> F<X> for every X (a natural transformation, endo case)
 * @param coalgebra Seed -> F<Seed>
 */
template <template <class> class F, class Nat, class Coalgebra, class Seed>
constexpr auto postpro(const Nat &e, const Coalgebra &coalgebra,
                       const Seed &seed) -> Fix<F> {
    auto layer = coalgebra(seed);
    auto expanded = layer_fmap(
        [&](const Seed &child) -> Fix<F> {
            return hoist<F>(e, postpro<F>(e, coalgebra, child));
        },
        layer);
    return wrap_fix<F>(std::move(expanded));
}
```

`unfold_fix` with the same one-line insertion, mirrored: `hoist<F>(e, ...)` wraps the *result* of the recursive unfold, so each freshly built subtree is rewritten before being grafted into its parent. Where `prepro`'s transformation gates what the algebra ever sees, `postpro`'s gates what the final tree ever contains &mdash; an unfold that generates candidates and a transformation that prunes or normalizes them, running interleaved rather than as passes.

Degeneracy, as always, pins the pair down: with the identity transformation, `prepro` is `fold_fix` and `postpro` is `unfold_fix`, and the test suite says so verbatim.


# The Shape of the Delta

Worth pausing on how small these were. `para` was `fold_fix` with a pair in the lambda. `zygo` was `fold_fix` with a pair carrier and a projection. `prepro` is `fold_fix` with a `hoist` before the recursive call; `postpro` is `unfold_fix` with a `hoist` after it. Every extension so far is the classical scheme's three-line body plus one move. The next part breaks that streak: when the algebra needs *all* of history, the carrier stops being a pair and becomes a data structure of its own.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [Next: Interlude - Free, Cofree, and Their Free-er Relatives →](part-6.5-free-cofree.md)

</nav>


# References

Fokkinga, Maarten M. (1992). **Law and Order in Algorithmics**, PhD thesis, University of Twente.

Kmett, Edward (2009). **Recursion Schemes: A Field Guide (Redux)**, The Comonad.Reader, <http://comonad.com/reader/2009/recursion-schemes/>.
