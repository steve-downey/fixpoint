- [Overview](#org4bb6943)
- [Blog Posts](#org569d110)
  - [[Part 0 — Why a Field Guide?](part-0-intro.md)](#org8fe4adf)
  - [[Part 1 — Fix and Box: Tying the Knot](part-1-fixpoint.md)](#orgb7992a0)
  - [[Part 2 — Typeclass Objects and layer\_fmap](part-2-typeclasses.md)](#org586faf0)
  - [[Part 3 — Fold, Unfold, Refold](part-3-classical.md)](#org8076fb3)
  - [[Part 4 — Paramorphisms and Apomorphisms](part-4-para-apo.md)](#orgbe409fd)
  - [[Part 5 — Zygomorphisms and Mutumorphisms](part-5-zygo-mutu.md)](#org0dec399)
  - [[Part 6 — Natural Transformations: hoist, prepro, postpro](part-6-prepro-postpro.md)](#orgba7b0e0)
  - [[Interlude — Free, Cofree, and Their Free-er Relatives](part-6.5-free-cofree.md)](#org0abb844)
  - [[Part 7 — Histomorphisms and Futumorphisms](part-7-histo-futu.md)](#orgb38489a)
  - [[Part 8 — Dynamorphisms and Chronomorphisms](part-8-dyna-chrono.md)](#orgaf18bba)
  - [[Part 9 — Mendler Style](part-9-mendler.md)](#org5ccf130)
  - [[Part 10 — Elgot Algebras](part-10-elgot.md)](#org927cab0)
  - [[Part 11 — Distributive Laws and the Generalized Schemes](part-11-generalized.md)](#org8f86f32)
  - [[Part 12 — The Capstone: zygoHistoPrepro](part-12-capstone.md)](#orgd5e3bdd)
  - [[Part 13 — Conclusion](part-13-conclusion.md)](#org5a5cb72)
- [Table of Contents](#orgb9ec11f)



<a id="org4bb6943"></a>

# Overview

A field guide to the recursion-schemes menagerie, implemented in C++26 as the `smd::fixpoint` library: the full Kmett catalog &mdash; folds, unfolds, refolds, course-of-values recursion, Mendler style, Elgot algebras, and the comonadic/monadic generalizations &mdash; built on one fixpoint type, a constexpr `Box`, and typeclass objects. Every post transcludes the shipping code and a runnable example; every scheme is pinned by the degeneracy laws in its test file.


<a id="org569d110"></a>

# Blog Posts


<a id="org8fe4adf"></a>

## [Part 0 — Why a Field Guide?](part-0-intro.md)

What recursion schemes separate, where they come from, and the roadmap for the series.


<a id="orgb7992a0"></a>

## [Part 1 — Fix and Box: Tying the Knot](part-1-fixpoint.md)

The fixpoint type, and the trick that makes `Fix<F> ≅ F<Fix<F>>` legal C++: boxing is the functor's responsibility.


<a id="org586faf0"></a>

## [Part 2 — Typeclass Objects and layer\_fmap](part-2-typeclasses.md)

Functor as a variable-template registry of instance objects, and the three lookup modes: implicit, NTTP-pinned, explicit-object.


<a id="org8076fb3"></a>

## [Part 3 — Fold, Unfold, Refold](part-3-classical.md)

Catamorphism, anamorphism, hylomorphism under descriptive names, and why `cata` is deprecated.


<a id="orgbe409fd"></a>

## [Part 4 — Paramorphisms and Apomorphisms](part-4-para-apo.md)

Folds that see the original subtree; unfolds that graft finished subtrees; the `either` type and Left-stops/Right-continues.


<a id="org0dec399"></a>

## [Part 5 — Zygomorphisms and Mutumorphisms](part-5-zygo-mutu.md)

A fold consulting a helper fold, two folds as peers, and the banana-split pair carrier underneath both.


<a id="orgba7b0e0"></a>

## [Part 6 — Natural Transformations: hoist, prepro, postpro](part-6-prepro-postpro.md)

Rewriting every layer, and fusing the rewrite into folds and unfolds.


<a id="org0abb844"></a>

## [Interlude — Free, Cofree, and Their Free-er Relatives](part-6.5-free-cofree.md)

Where Free and Cofree come from (the adjunctions), how they are `Fix` with variables or annotations, the naive vs Church encodings, and Kiselyov's Freer — plus the dual the title asks about, all with compiled code.


<a id="orgb38489a"></a>

## [Part 7 — Histomorphisms and Futumorphisms](part-7-histo-futu.md)

Course-of-values recursion: Cofree histories for folds, Free chunks for unfolds, coin change and run-length decoding.


<a id="orgaf18bba"></a>

## [Part 8 — Dynamorphisms and Chronomorphisms](part-8-dyna-chrono.md)

The fused forms: dynamic programming from a seed, Fibonacci with the memo table as a comonad.


<a id="org5ccf130"></a>

## [Part 9 — Mendler Style](part-9-mendler.md)

Folds with no Functor instance: the algebra receives the recursive call, and a rank-2 guarantee becomes a discipline.


<a id="org927cab0"></a>

## [Part 10 — Elgot Algebras](part-10-elgot.md)

Refolds where the coalgebra can stop with an answer, the dual where the algebra sees the seed, and testing the work instead of the answer.


<a id="org8f86f32"></a>

## [Part 11 — Distributive Laws and the Generalized Schemes](part-11-generalized.md)

The reveal: gcata and gana subsume the catalog, one distributive law per scheme, with the executable proof.


<a id="orgd5e3bdd"></a>

## [Part 12 — The Capstone: zygoHistoPrepro](part-12-capstone.md)

Composing comonads: zygo + histo + prepro in one pass, and the dedicated EnvT-style instance the generic pair could not provide.


<a id="org5a5cb72"></a>

## [Part 13 — Conclusion](part-13-conclusion.md)

The catalog in review, the ledger of what C++ pushed back on, and where the trail continues.


<a id="orgb9ec11f"></a>

# Table of Contents


# Table of Contents

1.  [Overview](#org4bb6943)
2.  [Blog Posts](#org569d110)
3.  [Table of Contents](#orgb9ec11f)
