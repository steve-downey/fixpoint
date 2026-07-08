<div class="abstract" id="orgda1f969">
<p>
Every recursive function over a tree mixes two concerns: what to do at each
node, and how to walk the structure. Recursion schemes separate them. This
series walks the full catalog &mdash; from the classical fold and unfold through
paramorphisms, histomorphisms, and Elgot algebras, up to the generalized
schemes that reveal them all as one construction &mdash; implemented in C++26,
with the machinery that makes it work: a fixpoint type, a constexpr Box, and
typeclass objects.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md)

</nav>


# The Motivation

Every recursive data structure in C++ ships with a private zoo of recursive functions. An expression tree has an evaluator, a pretty-printer, a simplifier, a depth-counter. Each one re-implements the same walk: visit the children, then combine. The walking code and the combining code are tangled together, and the walking code is the part that harbors the bugs &mdash; the missed case, the unmanaged lifetime, the stack that only overflows in production.

Functional programmers factored this zoo thirty-five years ago. Meijer, Fokkinga, and Paterson's "Functional Programming with Bananas, Lenses, Envelopes and Barbed Wire" (Meijer, Erik and Fokkinga, Maarten and Paterson, Ross, 1991) named the pattern: a *catamorphism* tears a structure down, an *anamorphism* builds one up, and a *hylomorphism* does both without materializing the middle. Fokkinga's thesis (Fokkinga, Maarten M., 1992) added schemes that carry extra context. Uustalu and Vene added course-of-values recursion &mdash; folds that remember history (Uustalu, Tarmo and Vene, Varmo, 1999). Edward Kmett's Haskell `recursion-schemes` package (Kmett, Edward, 2011) collected the whole menagerie behind one interface, and his "Field Guide" (Kmett, Edward, 2009) is still the best map of the territory.

This series is that field guide, rewritten for C++26. Not a sketch &mdash; a shipping library, `smd::fixpoint`, with every scheme implemented, tested against its defining laws, and exercised by a runnable example. The recursion is written once, in the scheme. What you write is one layer's worth of logic, and the scheme supplies the walk.


# What Makes This Possible in C++

Three pieces of machinery carry the whole series.

First, a **fixpoint type**. `Fix<F>` takes a non-recursive template `F` &mdash; one layer of structure with a type parameter where the children go &mdash; and ties the recursive knot. The type equation `Fix<F> ≅ F<Fix<F>>` looks like it should not compile. Part 1 shows the trick that makes it compile, and the constexpr-capable `Box` that breaks the infinite regress.

Second, **typeclass objects**. Every scheme needs `fmap` &mdash; apply a function at each child position of one layer. Rather than virtual functions, traits, or ADL, the library uses variable templates holding instance objects, looked up by the concrete layer type. Part 2 shows the machinery and its three lookup modes.

Third, the **schemes themselves**, each a short constexpr function template. The recursive equations transcribe almost line-for-line from the Haskell sources. Where C++ cannot follow &mdash; it cannot infer a fold's carrier type through a recursive call, and it cannot deduce a template-template parameter from an alias &mdash; the deviations are explicit, named, and small.


# The Phase-by-Phase Roadmap

**Part 1 &mdash; Fix and Box: Tying the Knot.** The fixpoint type itself, and the "magic" that makes a self-referential type legal C++: boxing is the functor's responsibility, so `F<X>` is a complete type even when `X` is not.

**Part 2 &mdash; Typeclass Objects and layer\_fmap.** How Functor is modeled without inheritance: `functor_typeclass<T>` variable templates, CRTP instance maps, and the single bridge function `layer_fmap` with its three lookup modes &mdash; implicit, NTTP-pinned, and explicit-object.

**Part 3 &mdash; Fold, Unfold, Refold.** The classical trio &mdash; catamorphism, anamorphism, hylomorphism &mdash; under their descriptive names `fold_fix`, `unfold_fix`, and `refold`, and why the library deprecated `cata`.

**Part 4 &mdash; Paramorphisms and Apomorphisms.** Folds that also see the original subtree, and unfolds that can graft a finished subtree and stop. Introduces the project's `either` type and its Left-stops/Right-continues convention.

**Part 5 &mdash; Zygomorphisms and Mutumorphisms.** Two folds running together: a main fold consulting a helper, and two mutually recursive folds as peers.

**Part 6 &mdash; Natural Transformations: hoist, prepro, postpro.** Rewriting every layer of a tree, and fusing that rewrite into a fold on the way down or an unfold on the way out.

**Interlude &mdash; Free, Cofree, and Their Free-er Relatives.** A detour before Part 7 uses them: where Free and Cofree come from (left and right adjoints to forgetful functors), why each is `Fix` with one extra thing per node, how the naive types compare to the Church encoding and to Kiselyov's Freer monad, and whether there is a CoFreer &mdash; every excerpt compiled.

**Part 7 &mdash; Histomorphisms and Futumorphisms.** Course-of-values recursion: folds that can look arbitrarily far back into computed history via the Cofree comonad, and unfolds that emit several layers at once via the Free monad.

**Part 8 &mdash; Dynamorphisms and Chronomorphisms.** The fused forms: dynamic programming from a seed, with no intermediate tree. Fibonacci where the memo table is the comonad.

**Part 9 &mdash; Mendler Style.** Folds with no Functor instance at all: the algebra receives the recursive call as an argument and applies it where it chooses.

**Part 10 &mdash; Elgot Algebras.** Refolds where the coalgebra can short-circuit with a finished answer, and the dual where the algebra also sees the seed.

**Part 11 &mdash; Distributive Laws and the Generalized Schemes.** The big reveal: `fold_fix`, `histo`, `zygo`, and `para` are one scheme (`gcata`) instantiated at different comonads; `unfold_fix`, `apo`, and `futu` are one scheme (`gana`) at different monads. A distributive law is the only moving part.

**Part 12 &mdash; The Capstone: zygoHistoPrepro.** Kmett's famous compositional answer to "can I have a zygomorphism, a histomorphism, and a prepromorphism at the same time?" Yes &mdash; by composing comonads.

**Part 13 &mdash; Conclusion.** What we told you, what C++ pushed back on, and where the field guide points next.


# How to Read the Series

Each part follows the same shape. What the scheme does, in one sentence. The recursive equation it implements, and where it comes from in the literature. The C++ implementation, transcluded directly from the library headers &mdash; the code in these posts *is* the shipping code, not a simplification. A worked example, transcluded from a runnable program in `src/examples/`. And the law that pins it down: every scheme in the library degenerates to a simpler one when you hand it trivial inputs, and the test suite holds every scheme to its degeneracy law.

The library is C++26, built with GCC 16. Everything is `constexpr`-capable; the test suite folds trees at compile time. Nothing here needs inheritance, virtual dispatch, or RTTI.

The source is the `smd::fixpoint` library; the design rationale and the decision log live in the repository's [design document](../recursion-schemes-design.md), and the per-scheme reference in the [usage catalog](../recursion-schemes.md).

Let's meet the type that makes the whole thing go.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [Next: Part 1 - Fix and Box: Tying the Knot →](part-1-fixpoint.md)

</nav>


# References

Meijer, Erik, Fokkinga, Maarten, and Paterson, Ross (1991). **Functional Programming with Bananas, Lenses, Envelopes and Barbed Wire**, FPCA '91, Springer LNCS 523.

Fokkinga, Maarten M. (1992). **Law and Order in Algorithmics**, PhD thesis, University of Twente.

Uustalu, Tarmo and Vene, Varmo (1999). **Primitive (Co)Recursion and Course-of-Value (Co)Iteration, Categorically**, Informatica 10(1).

Kmett, Edward (2009). **Recursion Schemes: A Field Guide (Redux)**, The Comonad.Reader, <http://comonad.com/reader/2009/recursion-schemes/>.

Kmett, Edward (2011&ndash;). **recursion-schemes**, Hackage, <https://hackage.haskell.org/package/recursion-schemes>.

Hinze, Ralf, Wu, Nicolas, and Gibbons, Jeremy (2013). **Unifying Structured Recursion Schemes**, ICFP '13.
