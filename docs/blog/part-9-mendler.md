<div class="abstract" id="org5aa7562">
<p>
Every scheme so far leaned on one hook: the Functor instance that maps the
recursive call over a layer. Mendler-style recursion removes it. The
algebra receives the recursive call itself as an argument and applies it
where it chooses &#x2014; no fmap, no typeclass lookup, no instance required.
What it costs is a guarantee C++ cannot express.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Part 8 - Dynamorphisms and Chronomorphisms ←](part-8-dyna-chrono.md)

</nav>


# Inverting the Contract

A `fold_fix` algebra is passive: by the time it runs, `layer_fmap` has already folded every child, and the algebra sees only results. A Mendler algebra (Mendler, Nax Paul, 1991) is active: it receives the *unevaluated* layer &#x2014; children still trees &#x2014; plus the recursive call `recurse` as an explicit callable argument, and it decides which children to fold, in what order, or whether to fold them at all.

The scheme shrinks accordingly. From [`src/smd/fixpoint/mendler.hpp`](../../src/smd/fixpoint/mendler.hpp):

```cpp
/** mcata :: (forall y. (y -> c) -> f y -> c) -> Fix f -> c
 * @tparam Result the fold's result type (design D5: explicit)
 * @param phi   called as phi(recurse, layer): `recurse` is a callable
 *              const Fix<F>& -> Result (mcata Φ, partially applied);
 *              `layer` is const F<Fix<F>>&, the tree's outermost layer.
 *              phi decides which children (if any) to fold via `recurse` —
 *              no functor_typeclass<F<...>> instance is consulted here.
 */
template <class Result, template <class> class F, class MAlgebra>
constexpr auto mcata(const MAlgebra &phi, const Fix<F> &tree) -> Result {
    auto recurse = [&phi](const Fix<F> &child) -> Result {
        return mcata<Result, F>(phi, child);
    };
    return phi(recurse, unwrap_fix(tree));
}
```

That is the whole of `mcata`: build the partially-applied recursive call, unwrap one layer, hand both to the algebra. No `layer_fmap` anywhere &#x2014; which means **no `functor_typeclass` instance is ever looked up**. `mcata` compiles and runs over any base functor, instanced or not. For a one-off functor in a test, or a functor whose `fmap` would be awkward to state, that is the entire sales pitch.

Evaluation, Mendler style, from [`src/examples/mendler_eval.cpp`](../../src/examples/mendler_eval.cpp):

```cpp
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
```

Same answer as Part 1's `eval`, different division of labor: the recursion is applied *by the algebra*, at `recurse(*a.left)` and `recurse(*a.right)`, rather than before the algebra runs.


# What the Types No Longer Prove

The literature type of a Mendler algebra is rank-2 polymorphic:

```
mcata :: (forall y. (y -> c) -> f y -> c) -> Fix f -> c
```

That `forall y` is doing real work. The algebra receives an `f y` for an *abstract* `y` it knows nothing about; the only operation available on a child is the `y -> c` it was handed. Termination and abstraction come out as theorems: the algebra *cannot* express anything with a child except folding it (Uustalu, Tarmo and Vene, Varmo, 1999).

C++ has no rank-2 polymorphism, and the header is candid about the consequence: here the layer is concretely `ExprF<Expr>`, children are plain `Expr` values, and a mischievous algebra could ignore `recurse` and walk a child by hand with `unwrap_fix`. Nothing stops it. The guarantee decays into a discipline &#x2014; "`recurse` is the only thing you may do with a child" &#x2014; and the example's every branch observes it. This is the honest shape of the whole series' translation project: Haskell's types make illegal states unrepresentable; the C++ transcription sometimes only makes them unidiomatic, and the difference belongs in the documentation, not under the rug.


# mhisto: History Without the Annotation

The course-of-values variant adds one more callable:

```cpp
/** mhisto :: (forall y. (y -> c) -> (y -> f y) -> f y -> c) -> Fix f -> c
 * @tparam Result the fold's result type (design D5: explicit)
 * @param phi   called as phi(recurse, unroll, layer): `recurse` is
 *              const Fix<F>& -> Result (mhisto Φ, partially applied);
 *              `unroll` is const Fix<F>& -> const F<Fix<F>>& (i.e.
 *              unwrap_fix) — it returns a reference *into* the tree, which
 *              is fine here since the tree the fold started from outlives
 *              the whole call; `layer` is const F<Fix<F>>&. phi may call
 *              `unroll` on any child it holds to look further down without
 *              committing to folding it via `recurse` — no
 *              functor_typeclass<F<...>> instance is consulted here either.
 */
template <class Result, template <class> class F, class MAlgebra>
constexpr auto mhisto(const MAlgebra &phi, const Fix<F> &tree) -> Result {
    auto recurse = [&phi](const Fix<F> &child) -> Result {
        return mhisto<Result, F>(phi, child);
    };
    auto unroll = [](const Fix<F> &child) -> const F<Fix<F>> & {
        return unwrap_fix(child);
    };
    return phi(recurse, unroll, unwrap_fix(tree));
}
```

`unroll` is just `unwrap_fix`, passed in as an argument: the algebra may peel layers off any child *without* committing to fold it. Look back at what Part 7's `histo` had to build &#x2014; a full `Cofree` annotation of every node, deep copies and all &#x2014; to give its algebra history. `mhisto` gets the same expressive power by lending the algebra a flashlight instead of photocopying the archive: the "history" is the tree itself, read on demand. The trade is memoization: histo's `Cofree` head at each node is computed once; an `mhisto` algebra that re-recurses into the same child pays each time. Coin change wants histo; a scheme that peeks one layer ahead wants `mhisto`.


# Where This Sits in the Catalog

Mendler style is the catalog's control group. Every other fold in this series is `gcata` at some comonad &#x2014; that is Part 11's theorem &#x2014; but `mcata` stands outside the theorem, because it never engages the functor machinery the theorem quantifies over. It is what recursion looks like when you strip the framework to bare metal: one knot-tying type, one partially-applied recursive call, and an algebra trusted to use it. Kmett's library ships the same pair under the same names (Kmett, Edward, 2011), and for the same reason: sometimes the instance is the expensive part.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [Next: Part 10 - Elgot Algebras →](part-10-elgot.md)

</nav>


# References

Mendler, Nax Paul (1991). **Inductive Types and Type Constraints in the Second-Order Lambda Calculus**, Annals of Pure and Applied Logic 51.

Uustalu, Tarmo and Vene, Varmo (1999). **Mendler-style Inductive Types, Categorically**, Nordic Journal of Computing 6(3).

Kmett, Edward (2011&#x2013;). **recursion-schemes**, Hackage, <https://hackage.haskell.org/package/recursion-schemes>.
