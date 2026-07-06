- [Overview](#org784400b)
- [Blog Posts](#org9fb33ef)
  - [[Part 0 — Why a Field Guide?](part-0-intro.md)](#org79ecfda)
  - [[Part 1 — Fix and Box: Tying the Knot](part-1-fixpoint.md)](#org60d05ae)
  - [[Part 2 — Typeclass Objects and layer\_fmap](part-2-typeclasses.md)](#org0a5169b)
  - [[Part 3 — Fold, Unfold, Refold](part-3-classical.md)](#orge2e5c1c)
  - [[Part 4 — Paramorphisms and Apomorphisms](part-4-para-apo.md)](#org80b01a1)
  - [[Part 5 — Zygomorphisms and Mutumorphisms](part-5-zygo-mutu.md)](#org397d607)
  - [[Part 6 — Natural Transformations: hoist, prepro, postpro](part-6-prepro-postpro.md)](#orgecd3b8a)
  - [[Part 7 — Histomorphisms and Futumorphisms](part-7-histo-futu.md)](#org36cba7d)
  - [[Part 8 — Dynamorphisms and Chronomorphisms](part-8-dyna-chrono.md)](#orgaed40bd)
  - [[Part 9 — Mendler Style](part-9-mendler.md)](#org698b8de)
  - [[Part 10 — Elgot Algebras](part-10-elgot.md)](#org00f7028)
  - [[Part 11 — Distributive Laws and the Generalized Schemes](part-11-generalized.md)](#org98475e3)
  - [[Part 12 — The Capstone: zygoHistoPrepro](part-12-capstone.md)](#org1da31e8)
  - [[Part 13 — Conclusion](part-13-conclusion.md)](#orgea58059)
- [Table of Contents](#orge3ab19c)



<a id="org784400b"></a>

# Overview

A field guide to the recursion-schemes menagerie, implemented in C++26 as the `smd::fixpoint` library: the full Kmett catalog &#x2014; folds, unfolds, refolds, course-of-values recursion, Mendler style, Elgot algebras, and the comonadic/monadic generalizations &#x2014; built on one fixpoint type, a constexpr `Box`, and typeclass objects. Every post transcludes the shipping code and a runnable example; every scheme is pinned by the degeneracy laws in its test file.


<a id="org9fb33ef"></a>

# Blog Posts


<a id="org79ecfda"></a>

## [Part 0 — Why a Field Guide?](part-0-intro.md)

What recursion schemes separate, where they come from, and the roadmap for the series.


<a id="org60d05ae"></a>

## [Part 1 — Fix and Box: Tying the Knot](part-1-fixpoint.md)

The fixpoint type, and the trick that makes `Fix<F> ≅ F<Fix<F>>` legal C++: boxing is the functor's responsibility.


<a id="org0a5169b"></a>

## [Part 2 — Typeclass Objects and layer\_fmap](part-2-typeclasses.md)

Functor as a variable-template registry of instance objects, and the three lookup modes: implicit, NTTP-pinned, explicit-object.


<a id="orge2e5c1c"></a>

## [Part 3 — Fold, Unfold, Refold](part-3-classical.md)

Catamorphism, anamorphism, hylomorphism under descriptive names, and why `cata` is deprecated.


<a id="org80b01a1"></a>

## [Part 4 — Paramorphisms and Apomorphisms](part-4-para-apo.md)

Folds that see the original subtree; unfolds that graft finished subtrees; the `either` type and Left-stops/Right-continues.


<a id="org397d607"></a>

## [Part 5 — Zygomorphisms and Mutumorphisms](part-5-zygo-mutu.md)

A fold consulting a helper fold, two folds as peers, and the banana-split pair carrier underneath both.


<a id="orgecd3b8a"></a>

## [Part 6 — Natural Transformations: hoist, prepro, postpro](part-6-prepro-postpro.md)

Rewriting every layer, and fusing the rewrite into folds and unfolds.


<a id="org36cba7d"></a>

## [Part 7 — Histomorphisms and Futumorphisms](part-7-histo-futu.md)

Course-of-values recursion: Cofree histories for folds, Free chunks for unfolds, coin change and run-length decoding.


<a id="orgaed40bd"></a>

## [Part 8 — Dynamorphisms and Chronomorphisms](part-8-dyna-chrono.md)

The fused forms: dynamic programming from a seed, Fibonacci with the memo table as a comonad.


<a id="org698b8de"></a>

## [Part 9 — Mendler Style](part-9-mendler.md)

Folds with no Functor instance: the algebra receives the recursive call, and a rank-2 guarantee becomes a discipline.


<a id="org00f7028"></a>

## [Part 10 — Elgot Algebras](part-10-elgot.md)

Refolds where the coalgebra can stop with an answer, the dual where the algebra sees the seed, and testing the work instead of the answer.


<a id="org98475e3"></a>

## [Part 11 — Distributive Laws and the Generalized Schemes](part-11-generalized.md)

The reveal: gcata and gana subsume the catalog, one distributive law per scheme, with the executable proof.


<a id="org1da31e8"></a>

## [Part 12 — The Capstone: zygoHistoPrepro](part-12-capstone.md)

Composing comonads: zygo + histo + prepro in one pass, and the dedicated EnvT-style instance the generic pair could not provide.


<a id="orgea58059"></a>

## [Part 13 — Conclusion](part-13-conclusion.md)

The catalog in review, the ledger of what C++ pushed back on, and where the trail continues.


<a id="orge3ab19c"></a>

# Table of Contents


# Table of Contents

1.  [Overview](#org784400b)
2.  [Blog Posts](#org9fb33ef)
3.  [Table of Contents](#orge3ab19c)
