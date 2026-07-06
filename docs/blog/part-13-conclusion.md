<div class="abstract" id="org9b72b19">
<p>
The field guide is complete: twenty-odd schemes, one fixpoint type, one
fmap hook, and a handful of distributive laws &#x2014; the full Kmett catalog,
implemented, tested against its laws, and running at compile time in
C++26. What we told you, what C++ pushed back on, and where the trail
continues.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Part 12 - The Capstone: zygoHistoPrepro ←](part-12-capstone.md)

</nav>


# The Field Guide in Review

**Part 1 &#x2014; Fix and Box.** One non-recursive layer template, one knot-tying `Fix`, and the trick that makes it compile: boxing is the functor's responsibility, so `F<X>` is complete even for incomplete `X`. `Box` is the constexpr-capable deep-copying owner that keeps trees values.

**Part 2 &#x2014; Typeclass Objects.** `functor_typeclass<T>` variable templates holding instance objects; CRTP bases deriving what can be derived; `layer_fmap` as the single bridge, with implicit lookup, NTTP pinning, and explicit-object passing as the three doors in.

**Part 3 &#x2014; The Classical Trio.** `fold_fix`, `unfold_fix`, `refold` &#x2014; catamorphism, anamorphism, hylomorphism under descriptive names, three-line bodies, carriers explicit because C++ will not infer them through recursion.

**Parts 4-6 &#x2014; The Fokkinga Extensions.** para (fold that sees originals), apo (unfold that grafts), zygo (fold with a helper), mutu (mutually recursive folds), hoist/prepro/postpro (natural transformations fused into the walk). Every one: the classical body plus one move, and a degeneracy law pinning it to the scheme it extends.

**Parts 7-8 &#x2014; Course-of-Values.** Cofree annotates history for histo; Free chunks the future for futu; dyna, codyna, and chrono fuse them with refold so Fibonacci's memo table is a comonad and no intermediate tree ever exists.

**Parts 9-10 &#x2014; Off the Main Sequence.** Mendler style hands the algebra the recursive call and drops the Functor requirement &#x2014; and loses, honestly, the rank-2 guarantee C++ cannot state. Elgot algebras give the coalgebra a veto (Left = answer, stop) and the algebra the seed.

**Parts 11-12 &#x2014; The Reveal.** Distributive laws; gcata subsuming fold\_fix/histo/zygo/para; gana subsuming unfold\_fix/apo/futu; ghylo fusing both; gprepro/gpostpro splicing in natural transformations; and zygoHistoPrepro composing the env comonad onto Cofree &#x2014; with the one dedicated comonad instance the generic machinery could not supply, and the executable proof in `generalized_tour`.


# What C++ Pushed Back On

The transcription project kept a ledger, and the entries repay reading.

**Carriers must be spelled.** Haskell infers a fold's carrier through the recursive call; C++ never will. Every scheme takes `Result` (and `Helper`, `WResult`, `MSeed`&#x2026;) as leading explicit template parameters. Once accepted, this became convention rather than friction.

**Template-template parameters do not deduce backward.** `dist_histo`, `dist_para`, `dist_futu`, `dist_zygo_histo` all need `F` named at the call site, because an elaborated alias application cannot give its template back. Logged as a deviation with the failed alternative preserved in the comments.

**Deduced return types cannot self-recurse.** Every self-recursive worker &#x2014; Cofree's fmap, dist\_histo, the gcata/gana workers &#x2014; had to compute its return type up front and name it. The discipline was discovered once and then applied five times.

**CTAD copies when you meant to nest.** `Identity{x}` for an `x` already an Identity collapses instead of wrapping &#x2014; the same trap as `vector{v}`. Explicit template arguments in exactly the places generic code wraps generically.

**Defaulted comparisons force completeness.** Clang 22's eager deleted-ness check on `= default` comparisons cycles on self-referential functor families; hand-written friend bodies, which instantiate only when called, break the cycle. The scars are documented at every site.

**Some guarantees decay into disciplines.** Mendler's rank-2 abstraction, prepro's naturality, the "Left stops" orientation &#x2014; C++ can state none of them in types. The library's answer is tests aimed at the gap: invocation-counting for short-circuits, equivalence laws for every scheme, `static_assert` s folding trees at compile time. Tests are the spec; where an equation and its law disagreed, the law won and the deviation was recorded.


# What Was Deliberately Not Done

No stack-safety &#x2014; deep structures overflow, as the naive Haskell does. No sharing or move-optimization &#x2014; `Box` deep-copies, values are values. No fused `ghylo` &#x2014; the materializing version passed every law, so fusion waits for a reason to exist. Performance is a non-goal stated in the design, not an oversight; the goal was a faithful, lawful, readable transcription, and each of these would have traded readability for speed the examples do not need.


# Where the Trail Continues

The threads that would extend the guide: element-generic instance objects, so a single threaded functor object can serve multi-site schemes; `std::indirect` as `Box`'s successor when aggregate initialization allows; stack-safe or heap-driven evaluation for deep structures; and the Foldable/Traversable side of the typeclass library, which this series only waved at. The design document and its decision log remain the map of record &#x2014; this series is the guided tour, and the tests are the territory.

One layer's worth of logic; the scheme supplies the walk. Twenty schemes later, that sentence is the whole book. Once you see it here, you see it everywhere.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md)

</nav>


# References

Meijer, Erik, Fokkinga, Maarten, and Paterson, Ross (1991). **Functional Programming with Bananas, Lenses, Envelopes and Barbed Wire**, FPCA '91.

Fokkinga, Maarten M. (1992). **Law and Order in Algorithmics**, PhD thesis, University of Twente.

Uustalu, Tarmo, Vene, Varmo, and Pardo, Alberto (2001). **Recursion Schemes from Comonads**, Nordic Journal of Computing 8(3).

Hinze, Ralf, Wu, Nicolas, and Gibbons, Jeremy (2013). **Unifying Structured Recursion Schemes**, ICFP '13.

Kmett, Edward (2009). **Recursion Schemes: A Field Guide (Redux)**, The Comonad.Reader, <http://comonad.com/reader/2009/recursion-schemes/>.

Kmett, Edward (2011&#x2013;). **recursion-schemes**, Hackage, <https://hackage.haskell.org/package/recursion-schemes>.
