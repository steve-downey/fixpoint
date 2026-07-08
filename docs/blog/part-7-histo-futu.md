<div class="abstract" id="org9068d2f">
<p>
Coin-change dynamic programming needs <code>minCoins(n-1)</code>, <code>minCoins(n-4)</code>, and
<code>minCoins(n-5)</code> at once; a plain fold sees only its immediate child.
Run-length decoding wants to emit a whole run of list cells in one step; a
plain unfold emits exactly one layer per call. Course-of-values recursion
fixes both, and the carriers that do it &mdash; Cofree and Free &mdash; are the
most interesting data structures in the library.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Interlude - Free, Cofree, and Their Free-er Relatives ←](part-6.5-free-cofree.md)

</nav>


# Cofree: A Tree Annotated With Its Own Past

The pair carrier of Part 5 gave a fold one extra value per child. Course-of-values recursion (Uustalu, Tarmo and Vene, Varmo, 1999) needs *every* value ever computed below a child. The carrier for that is the Cofree comonad: the same shape as the tree being folded, but every node also holds an annotation. From [`src/smd/fixpoint/cofree.hpp`](../../src/smd/fixpoint/cofree.hpp):

```cpp
/** Cofree<F, A>: an F-tree where every node is annotated with an A.
 * @tparam F unary template functor (the base functor being annotated)
 * @tparam A the annotation/history type at each node
 */
template <template <class> class F, class A>
struct Cofree {
    A head;               // the annotation at this node
    F<Cofree<F, A>> tail; // one functor layer of annotated children

    // Hand-written (not = default): this is the exact type the clang-22
    // "incomplete type" cascade (dist_laws.t.cpp/gprepro.t.cpp) traces
    // through — forming F<WWR> for WWR = Cofree<F,X> (generalized.hpp's
    // gcata_worker_t/gprepro_worker_t) requires std::variant to ask
    // is_trivially_destructible_v<Succ<WWR>>, which (per Clang, more
    // eagerly than GCC) forces this defaulted friend's deleted-ness check,
    // which requires Cofree<F,X> itself complete — a cycle. A plain friend
    // body sidesteps this: it is only instantiated when actually called.
    friend constexpr auto operator==(const Cofree &lhs, const Cofree &rhs)
        -> bool {
        return lhs.head == rhs.head && lhs.tail == rhs.tail;
    }
};

/** extract(c) -> const A& : the annotation at this node. */
template <template <class> class F, class A>
constexpr auto extract(const Cofree<F, A> &c) -> const A & {
    return c.head;
}

/** unwrap_cofree(c) -> const F<Cofree<F,A>>& : one layer of annotated
 * children.
 */
template <template <class> class F, class A>
constexpr auto unwrap_cofree(const Cofree<F, A> &c) -> const F<Cofree<F, A>> & {
    return c.tail;
}
```

Notice that this compiles for exactly the reason `Fix` did. `F`'s recursive positions are boxed, so `F<Cofree<F, A>>` is a complete type while `Cofree<F, A>` is still being defined &mdash; the Part 1 trick, paying its second dividend. (And the hand-written `operator==` is the Part 1 war story again, at the type where Clang's eager completeness check actually fired.)


# histo: Fold With Total Recall

From [`src/smd/fixpoint/histo.hpp`](../../src/smd/fixpoint/histo.hpp):

```cpp
/** histo :: (f (Cofree f a) -> a) -> t -> a
 * @tparam Result the fold's result/annotation type (design D5: explicit)
 * @param algebra F<Cofree<F, Result>> -> Result
 */
template <class Result, template <class> class F, class Algebra>
constexpr auto histo(const Algebra &algebra, const Fix<F> &tree) -> Result {
    using Carrier = Cofree<F, Result>;
    auto combined = [&](const F<Carrier> &layer) -> Carrier {
        return Carrier{algebra(layer), layer};
    };
    return extract(fold_fix<Carrier>(combined, tree));
}
```

Four lines, and the same construction as zygo: a `fold_fix` at a richer carrier. The combined algebra runs the user's algebra, then stores the layer *itself* as the new node's tail &mdash; so each child position accumulates its entire annotated history as the fold climbs. The user's algebra receives `F<Cofree<F, Result>>`: at every child, the head is that child's result, and the tail goes all the way down. `extract` at the root throws the scaffolding away.

The classic use is dynamic programming where the recurrence skips levels. From [`src/examples/histo_coin_change.cpp`](../../src/examples/histo_coin_change.cpp):

```cpp
using History = Cofree<NatF, int>; // annotates each Nat node with minCoins

// Step `steps` layers further back through an already-computed history.
// `c` is the history at some amount m; look_back(c, k) is minCoins(m - k),
// or nullopt if m - k < 0 (ran off the front of the Nat).
constexpr auto look_back(const History &c, int steps) -> std::optional<int> {
    if (steps == 0) {
        return c.head;
    }
    if (std::holds_alternative<Zero>(c.tail)) {
        return std::nullopt;
    }
    return look_back(*std::get<Succ<History>>(c.tail).pred, steps - 1);
}

// The histo algebra: F<Cofree<F, int>> -> int, i.e.
// NatF<History> -> int.
auto min_coins_algebra(const NatF<History> &layer) -> int {
    return std::visit(overloaded{
                          [](const Zero &) { return 0; }, // minCoins(0) = 0
                          [](const Succ<History> &s) -> int {
                              // s.pred is the history for amount n-1: its own
                              // head is minCoins(n-1), and its tail lets us
                              // walk further back.
                              const History &pred = *s.pred;
                              int best = pred.head; // coin 1: 1 + minCoins(n-1)

                              // coin 4: 1 + minCoins(n-4) == 1 + minCoins((n-1)
                              // - 3)
                              if (auto m4 = look_back(pred, 3)) {
                                  if (*m4 < best) {
                                      best = *m4;
                                  }
                              }
                              // coin 5: 1 + minCoins(n-5) == 1 + minCoins((n-1)
                              // - 4)
                              if (auto m5 = look_back(pred, 4)) {
                                  if (*m5 < best) {
                                      best = *m5;
                                  }
                              }
                              return best + 1;
                          },
                      },
                      layer);
}
```

`minCoins(n)` needs `minCoins(n-1)`, `minCoins(n-4)`, `minCoins(n-5)`. The predecessor's head is the first; `look_back` walks the history 3 and 4 more steps for the others. Nothing is ever recomputed &mdash; the history *is* the memo table, built by the fold itself, no map, no mutable array. The program prints `minCoins(8) = 2` (4+4) and `minCoins(12) = 3` (4+4+4), the cases where a greedy or naive-recursive formulation goes wrong or goes exponential.


# Free: Computation Chunks With Seeds at the Leaves

Dualize the annotation idea. Cofree is *always* a node, *plus* an annotation at every level. Its mirror is *either* a value *or* a layer of more structure &mdash; the Free monad. From [`src/smd/fixpoint/free.hpp`](../../src/smd/fixpoint/free.hpp):

```cpp
/** Free<F, A>: a Pure value of type A, or one F-layer of further Free
 * computations (a Roll).
 * @tparam F unary template functor (the base functor being sequenced)
 * @tparam A the seed/value type at Pure leaves
 */
template <template <class> class F, class A>
struct Free {
    std::variant<A, F<Free<F, A>>> node; // Pure a | Roll layer

    // Hand-written (not = default): see cofree.hpp's/functors.hpp's own
    // comment — Free<F,A> is self-referential through F<Free<F,A>>, exactly
    // the shape that triggers Clang's eager defaulted-comparison
    // completeness check inside a self-embedding class template.
    friend constexpr auto operator==(const Free &lhs, const Free &rhs) -> bool {
        return lhs.node == rhs.node;
    }
};

/** pure_free(a) -> Free<F, A>: a Pure leaf holding @p a. */
template <template <class> class F, class A>
constexpr auto pure_free(A a) -> Free<F, A> {
    return Free<F, A>{std::variant<A, F<Free<F, A>>>{std::move(a)}};
}

/** roll_free(layer) -> Free<F, A>: one F-layer of further Free
 * computations.
 */
template <template <class> class F, class A>
constexpr auto roll_free(F<Free<F, A>> layer) -> Free<F, A> {
    return Free<F, A>{std::variant<A, F<Free<F, A>>>{std::move(layer)}};
}

/** is_pure(f) -> bool: true iff @p f is a Pure leaf (not a Roll layer). */
template <template <class> class F, class A>
constexpr auto is_pure(const Free<F, A> &f) -> bool {
    return std::holds_alternative<A>(f.node);
}
```

Read `Free<F, Seed>` as "a chunk of tree, with fresh seeds where I stopped building." `pure_free` is a seed; `roll_free` is one more layer of chunk. Cofree is the cofree *comonad* over F and Free the free *monad* &mdash; the library registers exactly those instances, and Part 11 spends them.


# futu: Unfold Several Layers per Step

An `unfold_fix` coalgebra returns `F<Seed>`: one layer, seeds under it. A `futu` coalgebra returns `F<Free<F, Seed>>`: one layer whose children are whole *chunks* (Uustalu, Tarmo and Vene, Varmo, 1999). From [`src/smd/fixpoint/futu.hpp`](../../src/smd/fixpoint/futu.hpp):

```cpp
/** Implementation detail of futu: unrolls one Free<F, Seed> chunk (as
 * emitted by a single coalgebra step) into a Fix<F> subtree. Pure s
 * resumes futu at s; a Roll layer is unrolled one F-layer at a time,
 * recursing into every child chunk.
 */
template <template <class> class F, class Coalgebra, class Seed>
constexpr auto futu_worker(const Coalgebra &coalgebra,
                           const Free<F, Seed> &chunk) -> Fix<F> {
    return std::visit(
        overloaded{
            [&](const Seed &s) -> Fix<F> { return futu<F>(coalgebra, s); },
            [&](const F<Free<F, Seed>> &layer) -> Fix<F> {
                return wrap_fix<F>(layer_fmap(
                    [&coalgebra](const Free<F, Seed> &child) -> Fix<F> {
                        return futu_worker<F>(coalgebra, child);
                    },
                    layer));
            },
        },
        chunk.node);
}

/** futu :: (a -> f (Free f a)) -> a -> t
 * @tparam F unary template functor (D5: explicit, cannot be deduced from
 *   @p seed alone)
 * @param coalgebra Seed -> F<Free<F, Seed>>
 */
template <template <class> class F, class Coalgebra, class Seed>
constexpr auto futu(const Coalgebra &coalgebra, const Seed &seed) -> Fix<F> {
    auto layer = coalgebra(seed); // F<Free<F, Seed>>
    auto expanded = layer_fmap(
        [&coalgebra](const Free<F, Seed> &child) -> Fix<F> {
            return futu_worker<F>(coalgebra, child);
        },
        layer);
    return wrap_fix<F>(std::move(expanded));
}
```

The worker walks a chunk: a `Pure` leaf resumes the coalgebra at its seed; a `Roll` layer becomes real tree, one `wrap_fix` per layer, recursing into child chunks. `futu` itself is `unfold_fix` with the worker spliced in where the plain recursive call was.

Run-length decoding is the shape made obvious, from [`src/examples/futu_rle_decode.cpp`](../../src/examples/futu_rle_decode.cpp):

```cpp
using IndexFree = Free<IntListF, std::size_t>;

// Builds `count` further Cons(value) layers, then a Pure leaf holding
// `seed` — the tail of the chunk a single RLE coalgebra step emits.
auto build_run_tail(int value, int count, std::size_t seed) -> IndexFree {
    if (count <= 0) {
        return pure_free<IntListF>(seed);
    }
    return roll_free<IntListF>(IntListF<IndexFree>{Cons<int, IndexFree>{
        value, make_box<IndexFree>(build_run_tail(value, count - 1, seed))}});
}

// The futu coalgebra: seed = index into `pairs`. Each call emits `count`
// Cons(value) layers as one Free chunk (this call's own Cons is the first
// layer, build_run_tail supplies the remaining count-1), resuming at the
// next pair's index.
auto make_rle_coalgebra(const std::vector<std::pair<int, int>> &pairs) {
    return [&pairs](std::size_t i) -> IntListF<IndexFree> {
        if (i >= pairs.size()) {
            return Nil<int>{};
        }
        auto [count, value] = pairs[i];
        return Cons<int, IndexFree>{value, make_box<IndexFree>(build_run_tail(
                                               value, count - 1, i + 1))};
    };
}
```

One coalgebra call sees `(count, value)` and emits `count` `Cons` layers as a single nested-`roll_free` chunk, with the `Pure` leaf carrying the index of the next pair. `[(2,7), (3,1)]` decodes to `[7, 7, 1, 1, 1]` with three coalgebra calls, not five. A plain unfold would need seed bookkeeping &mdash; "which pair, how many emitted so far" &mdash; to fake what the chunk states directly.


# The Symmetry Ledger

The catalog is now symmetric three levels deep, and it is worth writing the ledger down:

| fold side                               | unfold side                            |
|--------------------------------------- |-------------------------------------- |
| `fold_fix` &mdash; child's result       | `unfold_fix` &mdash; one seed          |
| `para` &mdash; result + original (pair) | `apo` &mdash; seed or subtree (either) |
| `histo` &mdash; full history (Cofree)   | `futu` &mdash; full chunk (Free)       |

Products and annotations on the left; sums and chunks on the right. Pair generalizes to Cofree, either generalizes to Free &mdash; and each right column entry is the categorical dual of its left neighbor. This table is not a mnemonic; it is a theorem, and Part 11 proves it in code: every left entry is `gcata` at a comonad, every right entry `gana` at a monad. But first, the fused forms &mdash; what happens when a histo runs directly on a seed &mdash; and two schemes that step outside the functor framework entirely.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [Next: Part 8 - Dynamorphisms and Chronomorphisms →](part-8-dyna-chrono.md)

</nav>


# References

Uustalu, Tarmo and Vene, Varmo (1999). **Primitive (Co)Recursion and Course-of-Value (Co)Iteration, Categorically**, Informatica 10(1).

Vene, Varmo (2000). **Categorical Programming with Inductive and Coinductive Types**, PhD thesis, University of Tartu.

Kmett, Edward (2009). **Recursion Schemes: A Field Guide (Redux)**, The Comonad.Reader, <http://comonad.com/reader/2009/recursion-schemes/>.
