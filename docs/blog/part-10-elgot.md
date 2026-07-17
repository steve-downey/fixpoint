<div class="abstract" id="orga6ca192">
<p>
Multiply a list of integers. The instant you see a zero, the answer is
zero &mdash; so why examine the rest? A hylomorphism has no way to say "stop
generating"; an Elgot algebra gives the coalgebra exactly that veto, and
its dual gives the algebra the seed that produced each layer.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Part 9 - Mendler Style ←](part-9-mendler.md)

</nav>


# elgot: The Coalgebra's Veto

`refold`'s coalgebra must always answer with a layer; the recursion stops only when a layer has no children. An Elgot coalgebra (Elgot, Calvin C., 1975) (Ad{\\'a}mek, Ji{\\v r}{\\'\\i} and Milius, Stefan and Velebil, Ji{\\v r}{\\'\\i}, 2006) answers with an `either`: **Left, a finished result &mdash; stop here, fold nothing below**; **Right, one more layer &mdash; continue**. Part 4's convention, doing hylomorphism duty. From [`src/smd/fixpoint/elgot.hpp`](../../src/smd/fixpoint/elgot.hpp):

```cpp
/** elgot :: (f a -> a) -> (b -> Either a (f b)) -> b -> a
 *
 * @tparam Result the fold's carrier (design D5: explicit)
 * @param algebra   F<Result> -> Result
 * @param coalgebra Seed -> either<Result, F<Seed>> (D4: Left = answer, stop;
 *                  Right = one more layer, continue)
 */
template <class Result, template <class> class F, class Algebra,
          class Coalgebra, class Seed>
constexpr auto elgot(const Algebra &algebra, const Coalgebra &coalgebra,
                     const Seed &seed) -> Result {
    auto step = coalgebra(seed);
    return smd::typeclass::match(
        step, [](const Result &answer) -> Result { return answer; },
        [&](const auto &layer) -> Result {
            auto evaluated = layer_fmap(
                [&](const Seed &child) -> Result {
                    return elgot<Result, F>(algebra, coalgebra, child);
                },
                layer);
            return algebra(evaluated);
        });
}
```

`refold` with a `match` wrapped around the step: Left returns the answer directly &mdash; note that the algebra is never called, not even once, if the *first* seed short-circuits; Right proceeds exactly as `refold` would.

The example is the zero-in-a-product bailout, from [`src/examples/elgot_shortcircuit.cpp`](../../src/examples/elgot_shortcircuit.cpp):

```cpp
int main() {
    std::vector<int> values{4, 3, 0, 5, 9, 2};

    std::size_t examined = 0;
    auto coalgebra = [&](std::size_t i) -> either<int, IntListF<std::size_t>> {
        ++examined;
        if (i >= values.size()) {
            return make_right<int>(IntListF<std::size_t>{Nil<int>{}});
        }
        if (values[i] == 0) {
            return make_left<IntListF<std::size_t>>(0);
        }
        return make_right<int>(IntListF<std::size_t>{
            Cons<int, std::size_t>{values[i], make_box<std::size_t>(i + 1)}});
    };

    int product =
        elgot<int, IntListF>(product_algebra, coalgebra, std::size_t{0});

    std::println("values: {} elements", values.size());
    std::println("product (bails out at the first 0): {}", product);
    std::println("elements examined: {} / {}", examined, values.size());
}
```

The output is the interesting part:

```
values: 6 elements
product (bails out at the first 0): 0
elements examined: 3 / 6
```

The examined-count is not decoration &mdash; it is the *only* observable evidence of the short-circuit. The product alone proves nothing: multiplying by zero swallows whether the tail was visited. The header makes the same point about testing, and it generalizes: a plausible off-by-one &mdash; expand one extra layer, then discard it &mdash; still gets every numeric answer right, so the test suite counts coalgebra invocations instead of trusting outputs. When a scheme's contract is about *work avoided*, test the work, not the answer.


# coelgot: The Algebra Gets the Seed

Dualize once more. Where `elgot` strengthens the coalgebra, `coelgot` strengthens the algebra: alongside the already-folded children, it receives *the seed that produced the current layer*:

```cpp
/** coelgot :: ((a, f b) -> b) -> (a -> f a) -> a -> b
 *
 * @tparam Result the fold's carrier (design D5: explicit)
 * @param algebra   (const Seed&, F<Result>) -> Result — the flattened pair;
 *                  the seed that produced `layer` is passed alongside the
 *                  already-folded children.
 * @param coalgebra Seed -> F<Seed> (no short-circuit; always one more layer)
 */
template <class Result, template <class> class F, class Algebra,
          class Coalgebra, class Seed>
constexpr auto coelgot(const Algebra &algebra, const Coalgebra &coalgebra,
                       const Seed &seed) -> Result {
    auto layer = coalgebra(seed); // evaluated exactly once per seed
    auto evaluated = layer_fmap(
        [&](const Seed &child) -> Result {
            return coelgot<Result, F>(algebra, coalgebra, child);
        },
        layer);
    return algebra(seed, evaluated);
}
```

No short-circuit here &mdash; the extra power is context, not control. It is `para`'s move transplanted to hylomorphisms: `para`'s algebra saw the original subtree next to each result; `coelgot`'s sees the original *seed* next to the folded layer, in a refold where no subtree ever exists to show.

One implementation note earns its comment: `coalgebra(seed)` is bound to a local before the `fmap`. A careless transcription that calls `psi` twice &mdash; once to build the layer, once inside the fold &mdash; would be invisible to every test with a pure coalgebra and would silently double the cost, and the side effects, of an impure one. The elgot/coelgot pair is small; the discipline in the comments is most of its value.


# The Pattern Completing

Count the extra-power moves the catalog has made:

-   give the **algebra** more input: para (subtree), zygo (helper), histo (history), coelgot (seed);
-   give the **coalgebra** more output: apo (graft-or-seed), futu (chunk), elgot (answer-or-layer).

Every one is "same three-line worker, richer type at one position", and each side's riches line up with the other's &mdash; products and annotations feeding algebras, sums and chunks flowing from coalgebras. The catalog is complete except for the reveal: Part 11 shows the one construction all the algebra-side schemes are instances of, the one all the coalgebra-side schemes are instances of, and the single moving part &mdash; a distributive law &mdash; that selects among them.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [Next: Part 11 - Distributive Laws and the Generalized Schemes →](part-11-generalized.md)

</nav>


# References

Elgot, Calvin C. (1975). **Monadic Computation and Iterative Algebraic Theories**, Logic Colloquium '73.

Adámek, Jiří, Milius, Stefan, and Velebil, Jiří (2006). **Elgot Algebras**, Logical Methods in Computer Science 2(5:4).

Kmett, Edward (2009). **Recursion Schemes: A Field Guide (Redux)**, The Comonad.Reader, <http://comonad.com/reader/2009/recursion-schemes/>.
