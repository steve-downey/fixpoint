<div class="abstract" id="org27c68dd">
<p>
Part 3's refold fused fold-after-unfold so the tree never exists. Part 7's
histo and futu gave folds history and unfolds chunks. Compose the two ideas
and you get dynamic programming straight from a seed: dyna, codyna, and
chrono, where Fibonacci's memo table is a Cofree that no one ever declared.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Part 7 - Histomorphisms and Futumorphisms ←](part-7-histo-futu.md)

</nav>


# Three Fusions, One Header

The compositions worth naming:

```
dyna    = histo . ana     -- fold-with-history over a generated structure
codyna  = cata  . futu    -- plain fold over a chunked unfold
chrono  = histo . futu    -- history on the way up, chunks on the way down
```

Each could be written as exactly that composition &#x2014; unfold, then fold &#x2014; and the test suite does write them that way, as the laws that pin the fused versions. But the composition builds the whole intermediate `Fix<F>` and walks it twice. The header fuses each through `refold`, so the intermediate tree *never exists*; the coalgebra's layers flow straight into the algebra's carrier.


# dyna: The Dynamorphism

From [`src/smd/fixpoint/chrono.hpp`](../../src/smd/fixpoint/chrono.hpp):

```cpp
/** dyna :: (f (Cofree f b) -> b) -> (a -> f a) -> a -> b
 * histo . ana, fused: never builds the intermediate Fix<F>.
 * @param algebra F<Cofree<F, Result>> -> Result
 * @param coalgebra Seed -> F<Seed>
 */
template <class Result, template <class> class F, class Algebra,
          class Coalgebra, class Seed>
constexpr auto dyna(const Algebra &algebra, const Coalgebra &coalgebra,
                    const Seed &seed) -> Result {
    using Carrier = Cofree<F, Result>;
    auto combined = [&](const F<Carrier> &layer) -> Carrier {
        return Carrier{algebra(layer), layer};
    };
    return extract(refold<Carrier, F>(combined, coalgebra, seed));
}
```

Compare with `histo` from Part 7: identical `combined` algebra, identical `Cofree` carrier, identical `extract` at the end &#x2014; but `refold` where `fold_fix` was, so the layers come from a coalgebra instead of an existing tree. That is the entire diff. The library's schemes keep composing because they are all the same three lines wearing different carriers.

Fibonacci is the canonical demonstration. From [`src/examples/dyna_fibonacci.cpp`](../../src/examples/dyna_fibonacci.cpp):

```cpp
// The ana half: counts down from n to 0. Seed and layer element type are
// both plain int — no Free chunk, unlike futu/codyna/chrono's coalgebras.
auto countdown(int m) -> NatF<int> {
    if (m <= 0) {
        return Zero{};
    }
    return Succ<int>{smd::fixpoint::make_box<int>(m - 1)};
}

// The histo half: Succ c has c.head == fib(n-1) (already computed by an
// earlier refold step, stored in the Cofree history); if c's own tail is
// Zero, n-1 == 0 so fib(n) == 1 (the fib(1) base case); otherwise c's tail
// is Succ(cc) and cc.head == fib(n-2), so fib(n) == c.head + cc.head. No
// Nat node is ever inspected here — only Cofree<NatF, int> history.
auto fib_algebra(const NatF<Cofree<NatF, int>> &layer) -> int {
    return std::visit(
        overloaded{
            [](const Zero &) { return 0; },
            [](const Succ<Cofree<NatF, int>> &s) -> int {
                const Cofree<NatF, int> &c = *s.pred;
                return std::visit(
                    overloaded{
                        [&](const Zero &) { return 1; },
                        [&](const Succ<Cofree<NatF, int>> &cc) -> int {
                            return c.head + cc.pred->head;
                        },
                    },
                    c.tail);
            },
        },
        layer);
}

auto fib(int n) -> int { return dyna<int, NatF>(fib_algebra, countdown, n); }
```

Naive recursive `fib` is exponential because it recomputes; memoized `fib` drags a table through the code. Here the countdown coalgebra generates `n, n-1, ..., 0` and the algebra reads `fib(n-1)` from its child's head and `fib(n-2)` from one step deeper in the history. The `Cofree<NatF, int>` **is** the DP table &#x2014; built by the scheme, indexed by structure, never named in user code. Linear time falls out; nobody wrote a cache.

The example's header comment makes the fusion point precise: the algebra cannot tell `dyna` from `histo`-after-`unfold_fix`. What it sees is identical. The fusion changes only whether the intermediate `Nat` ever occupies memory.


# codyna and chrono: Chunks on the Way In

The other two fusions take a *futu*-style coalgebra &#x2014; one that emits `Free` chunks &#x2014; and the interesting engineering is the impedance match. `refold` wants a one-layer-per-call coalgebra; a chunk is many layers. The bridge is `unroll`:

```cpp
/** codyna :: (f b -> b) -> (a -> f (Free f a)) -> a -> b
 * cata . futu, fused: never builds the intermediate Fix<F>.
 * @param algebra F<Result> -> Result
 * @param coalgebra Seed -> F<Free<F, Seed>>
 */
template <class Result, template <class> class F, class Algebra,
          class Coalgebra, class Seed>
constexpr auto codyna(const Algebra &algebra, const Coalgebra &coalgebra,
                      const Seed &seed) -> Result {
    auto unroll_step = [&coalgebra](const Free<F, Seed> &chunk)
        -> F<Free<F, Seed>> { return unroll<F>(coalgebra, chunk); };
    return refold<Result, F>(algebra, unroll_step, pure_free<F>(seed));
}

/** chrono :: (f (Cofree f b) -> b) -> (a -> f (Free f a)) -> a -> b
 * histo . futu, fused: never builds the intermediate Fix<F>.
 * @param algebra F<Cofree<F, Result>> -> Result
 * @param coalgebra Seed -> F<Free<F, Seed>>
 */
template <class Result, template <class> class F, class Algebra,
          class Coalgebra, class Seed>
constexpr auto chrono(const Algebra &algebra, const Coalgebra &coalgebra,
                      const Seed &seed) -> Result {
    using Carrier = Cofree<F, Result>;
    auto combined = [&](const F<Carrier> &layer) -> Carrier {
        return Carrier{algebra(layer), layer};
    };
    auto unroll_step = [&coalgebra](const Free<F, Seed> &chunk)
        -> F<Free<F, Seed>> { return unroll<F>(coalgebra, chunk); };
    return extract(
        refold<Carrier, F>(combined, unroll_step, pure_free<F>(seed)));
}
```

The seed type becomes `Free<F, Seed>` itself, starting at `pure_free(seed)`. Each `refold` step peels exactly one layer: a `Pure` leaf calls the real coalgebra for a fresh chunk; a `Roll` layer yields its top, leaving the children &#x2014; still chunks &#x2014; for the next steps. The chunk is consumed lazily, one layer per demand, which is precisely what a fused pipeline needs and precisely what `futu`'s own worker (which resolves whole chunks eagerly) does not do. `chrono` (Kmett, Edward, 2009) then stacks both extensions &#x2014; Cofree carrier up, Free chunks down &#x2014; and its body is, once more, the same three lines.


# Where the Streak Stands

Every scheme so far obeys the same recipe: pick a carrier (plain, pair, either, Cofree, Free), adjust the three-line worker, prove the degeneracy law. The recipe has been so uniform that the next two parts are each a deliberate break from it. Part 9 removes the one ingredient every scheme has depended on &#x2014; the Functor instance itself. Part 10 moves the short-circuit power from the carrier into the (co)algebra's return type. After that, Part 11 explains *why* the recipe kept working: the carriers were comonads and monads all along, and the recipe is one theorem.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [Next: Part 9 - Mendler Style →](part-9-mendler.md)

</nav>


# References

Kmett, Edward (2009). **Recursion Schemes: A Field Guide (Redux)**, The Comonad.Reader, <http://comonad.com/reader/2009/recursion-schemes/> &#x2014; including the dynamorphism and chronomorphism entries.

Uustalu, Tarmo and Vene, Varmo (1999). **Primitive (Co)Recursion and Course-of-Value (Co)Iteration, Categorically**, Informatica 10(1).
