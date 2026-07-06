<div class="abstract" id="org69a17e3">
<p>
Is this tree height-balanced? The predicate needs each subtree's height &#x2014;
a different fold &#x2014; at every step. Running two folds over one tree, where
one consults the other or each consults the other, is Fokkinga's territory:
the zygomorphism and the mutumorphism, both delivered by one pair-carrier
trick.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Part 4 - Paramorphisms and Apomorphisms ←](part-4-para-apo.md)

</nav>


# The Problem That Needs a Helper

A tree is height-balanced when, at every node, the children's heights differ by at most one and the children are themselves balanced. Try to write that as a plain `fold_fix` with carrier `bool` and you hit a wall immediately: "balanced" at a node needs the children's *heights*, and a `bool` carrier has already thrown them away.

You could widen the carrier to `pair<int, bool>` by hand and maintain both in one algebra. That works &#x2014; and the zygomorphism (Fokkinga, Maarten M., 1992) is exactly that construction, packaged so the two concerns stay separate: a *helper* algebra that knows only about heights, and a *main* algebra that consults the helper's answer at each child.


# zygo: A Fold With a Sidecar

From [`src/smd/fixpoint/zygo.hpp`](../../src/smd/fixpoint/zygo.hpp):

```cpp
/** zygo :: (f b -> b) -> (f (b, a) -> a) -> t -> a
 *
 * Equation: zygo f g = second ∘ fold_fix(λx. pair(f(fmapF(first, x)), g(x)))
 *
 * Convention (design §7.3, matches dist_zygo in S12): the carrier pair is
 * `{helper value, main value}` — **helper first, main second**. This is the
 * opposite slot order from para's `std::pair<Fix<F>, Result>` (original
 * subtree first, fold result second); the two pairs serve different
 * purposes and are not to be confused.
 *
 * @tparam Result main fold's carrier (D5: explicit)
 * @tparam Helper helper fold's carrier (D5: explicit)
 * @param helper F<Helper> -> Helper
 * @param main   F<std::pair<Helper, Result>> -> Result
 */
template <class Result, class Helper, template <class> class F,
          class HelperAlg, class MainAlg>
constexpr auto zygo(const HelperAlg &helper, const MainAlg &main,
                    const Fix<F> &tree) -> Result {
    using Carrier = std::pair<Helper, Result>;
    auto combined = [&](const F<Carrier> &layer) -> Carrier {
        // A *second* fmap over the already fold_fix-mapped layer, projecting
        // just the helper component at each child position — inherent to
        // the pairing construction, not a fusion opportunity (design §7.3
        // pitfall).
        auto helper_layer = layer_fmap(
            [](const Carrier &c) -> Helper { return c.first; }, layer);
        return Carrier{helper(helper_layer), main(layer)};
    };
    return fold_fix<Carrier>(combined, tree).second;
}
```

The implementation is a single `fold_fix` with carrier `std::pair<Helper, Result>`. At each layer the combined algebra does two jobs: project the helper component out of every child (that inner `layer_fmap`) and run the helper algebra on the projection; run the main algebra on the full pair layer. Then `.second` discards the scaffolding at the root.

Note the comment defending that inner `layer_fmap`: it is a second map over an already-mapped layer, and it is *inherent* &#x2014; the helper algebra's argument type is `F<Helper>`, not `F<pair<Helper, Result>>`, so the projection has to happen. The design log calls this out as a pitfall precisely because it looks like a fusion opportunity and is not one.

Slot convention, fixed by decision: **helper first, main second**. The opposite of `para`'s pair &#x2014; and deliberately so; `para`'s pair is "original, result" while zygo's is "context, value", and Part 11's `dist_zygo` depends on this order. The value slot is `.second` in both, which is the same convention that makes `std::pair`'s env comonad extract `.second`.

The example, from [`src/examples/zygo_balanced.cpp`](../../src/examples/zygo_balanced.cpp):

```cpp
// Helper algebra: F<Helper> -> Helper, Helper = int (subtree height).
auto height(const IntTreeF<int> &layer) -> int {
    return std::visit(overloaded{
                           [](const Leaf<int> &) { return 0; },
                           [](const Node<int> &n) {
                               int l = *n.left;
                               int r = *n.right;
                               return 1 + (l > r ? l : r);
                           },
                       },
                       layer);
}

// Main algebra: F<std::pair<Helper, Result>> -> Result, Result = bool. Each
// child arrives as {its height, whether it's balanced}; a Node is balanced
// iff both children are balanced *and* their heights differ by at most 1.
auto is_balanced(const IntTreeF<std::pair<int, bool>> &layer) -> bool {
    return std::visit(
        overloaded{
            [](const Leaf<int> &) { return true; },
            [](const Node<std::pair<int, bool>> &n) {
                int lh = n.left->first;
                int rh = n.right->first;
                int diff = lh > rh ? lh - rh : rh - lh;
                return diff <= 1 && n.left->second && n.right->second;
            },
        },
        layer);
}
```

`height` never heard of balance; `is_balanced` never computes a height. The call is `zygo<bool, int>(height, is_balanced, tree)` &#x2014; one traversal, both facts, each algebra minding its own business.


# mutu: Two Folds as Peers

Drop the hierarchy. In a mutumorphism (Fokkinga, Maarten M., 1992) neither fold is the helper: each sees *both* results at every child, and either can be the one you keep &#x2014; or keep both. From [`src/smd/fixpoint/mutu.hpp`](../../src/smd/fixpoint/mutu.hpp):

```cpp
/** mutu :: (f (a, b) -> a) -> (f (a, b) -> b) -> t -> (a, b)
 *
 * Equation: mutu f g = fold_fix(λx. pair(f(x), g(x)))
 *
 * @tparam A first fold's carrier (D5: explicit)
 * @tparam B second fold's carrier (D5: explicit)
 * @param alg_a F<std::pair<A, B>> -> A
 * @param alg_b F<std::pair<A, B>> -> B
 */
template <class A, class B, template <class> class F, class AlgA, class AlgB>
constexpr auto mutu(const AlgA &alg_a, const AlgB &alg_b,
                    const Fix<F> &tree) -> std::pair<A, B> {
    using Carrier = std::pair<A, B>;
    auto combined = [&](const F<Carrier> &layer) -> Carrier {
        return Carrier{alg_a(layer), alg_b(layer)};
    };
    return fold_fix<Carrier>(combined, tree);
}
```

Compare with zygo's body. No projection `fmap` &#x2014; both algebras take the same `F<pair<A, B>>` layer directly. This is the *banana-split theorem* made executable: two folds over the same structure are one fold with a pair carrier, `⟨f, g⟩` pairing the algebras. Zygo is what you get when one component additionally forgets the other; mutu is the symmetric original.

The classic demonstration is even/odd on Peano naturals, each defined by deferring to the other, from [`src/examples/mutu_even_odd.cpp`](../../src/examples/mutu_even_odd.cpp):

```cpp
// alg_a: is-even. A Succ is even iff its predecessor is odd (.second).
auto alg_even(const NatF<std::pair<bool, bool>> &layer) -> bool {
    return std::visit(
        overloaded{
            [](const Zero &) { return true; },
            [](const Succ<std::pair<bool, bool>> &s) {
                return s.pred->second;
            },
        },
        layer);
}

// alg_b: is-odd. A Succ is odd iff its predecessor is even (.first).
auto alg_odd(const NatF<std::pair<bool, bool>> &layer) -> bool {
    return std::visit(
        overloaded{
            [](const Zero &) { return false; },
            [](const Succ<std::pair<bool, bool>> &s) {
                return s.pred->first;
            },
        },
        layer);
}
```

`Succ(n)` is even iff `n` is odd; `Succ(n)` is odd iff `n` is even; `Zero` seeds the base cases. No arithmetic anywhere &#x2014; the mutual recursion *is* the definition, and `mutu` runs both in one pass, returning the pair.


# One Trick, Three Schemes

Step back and the pattern of this post is a single idea. `fold_fix` with a product carrier subsumes:

-   **mutu**: carrier `pair<A, B>`, algebras see everything, return the pair.
-   **zygo**: mutu where the helper ignores the main component &#x2014; add one projection, return `.second`.
-   **para** (last part): zygo where the helper is the identity algebra &#x2014; the "helper fold" that rebuilds the original subtree.

Each degeneracy is a law in the test suite: mutu with a second algebra nobody reads is `fold_fix`; zygo consulted only for its own result is `fold_fix`; para ignoring originals is `fold_fix`. The pair is doing all the work, which raises the right suspicion &#x2014; if a *product* carrier buys context on the way up, what does the matching structure on the unfold side buy? Part 4 answered with `either`. And what if the context a fold needs is not one helper value but the entire history of everything below? That is Part 7, and the pair will not be enough.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [Next: Part 6 - Natural Transformations: hoist, prepro, postpro →](part-6-prepro-postpro.md)

</nav>


# References

Fokkinga, Maarten M. (1992). **Law and Order in Algorithmics**, PhD thesis, University of Twente.

Meijer, Erik, Fokkinga, Maarten, and Paterson, Ross (1991). **Functional Programming with Bananas, Lenses, Envelopes and Barbed Wire**, FPCA '91 &#x2014; the banana-split theorem.
