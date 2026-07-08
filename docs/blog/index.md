- [Overview](#org8f5481a)
- [Blog Posts](#orge203e0c)
  - [[Part 0 — Why a Field Guide?](part-0-intro.md)](#org4cdd374)
  - [[Part 1 — Fix and Box: Tying the Knot](part-1-fixpoint.md)](#org1bcaf5f)
  - [[Part 2 — Typeclass Objects and layer\_fmap](part-2-typeclasses.md)](#orgcb219f0)
  - [[Part 3 — Fold, Unfold, Refold](part-3-classical.md)](#org5ec5981)
  - [[Part 4 — Paramorphisms and Apomorphisms](part-4-para-apo.md)](#org9e171eb)
  - [[Part 5 — Zygomorphisms and Mutumorphisms](part-5-zygo-mutu.md)](#org4eb53a1)
  - [[Part 6 — Natural Transformations: hoist, prepro, postpro](part-6-prepro-postpro.md)](#org672fc37)
  - [[Interlude — Free, Cofree, and Their Free-er Relatives](part-6.5-free-cofree.md)](#org02fa99e)
  - [[Part 7 — Histomorphisms and Futumorphisms](part-7-histo-futu.md)](#orgc487774)
  - [[Part 8 — Dynamorphisms and Chronomorphisms](part-8-dyna-chrono.md)](#orgc572354)
  - [[Part 9 — Mendler Style](part-9-mendler.md)](#org860eff1)
  - [[Part 10 — Elgot Algebras](part-10-elgot.md)](#org6454e59)
  - [[Part 11 — Distributive Laws and the Generalized Schemes](part-11-generalized.md)](#orgf1500bd)
  - [[Part 12 — The Capstone: zygoHistoPrepro](part-12-capstone.md)](#org38d82d9)
  - [[Part 13 — Conclusion](part-13-conclusion.md)](#orgbb54949)
- [Table of Contents](#org7325ce9)



<a id="org8f5481a"></a>

# Overview

A field guide to the recursion-schemes menagerie, implemented in C++26 as the `smd::fixpoint` library: the full Kmett catalog &mdash; folds, unfolds, refolds, course-of-values recursion, Mendler style, Elgot algebras, and the comonadic/monadic generalizations &mdash; built on one fixpoint type, a constexpr `Box`, and typeclass objects. Every post transcludes the shipping code and a runnable example; every scheme is pinned by the degeneracy laws in its test file.


<a id="orge203e0c"></a>

# Blog Posts


<a id="org4cdd374"></a>

## [Part 0 — Why a Field Guide?](part-0-intro.md)

What recursion schemes separate, where they come from, and the roadmap for the series.


<a id="org1bcaf5f"></a>

## [Part 1 — Fix and Box: Tying the Knot](part-1-fixpoint.md)

The fixpoint type, and the trick that makes `Fix<F> ≅ F<Fix<F>>` legal C++: boxing is the functor's responsibility.


<a id="orgcb219f0"></a>

## [Part 2 — Typeclass Objects and layer\_fmap](part-2-typeclasses.md)

Functor as a variable-template registry of instance objects, and the three lookup modes: implicit, NTTP-pinned, explicit-object.


<a id="org5ec5981"></a>

## [Part 3 — Fold, Unfold, Refold](part-3-classical.md)

Catamorphism, anamorphism, hylomorphism under descriptive names, and why `cata` is deprecated.


<a id="org9e171eb"></a>

## [Part 4 — Paramorphisms and Apomorphisms](part-4-para-apo.md)

Folds that see the original subtree; unfolds that graft finished subtrees; the `either` type and Left-stops/Right-continues.


<a id="org4eb53a1"></a>

## [Part 5 — Zygomorphisms and Mutumorphisms](part-5-zygo-mutu.md)

A fold consulting a helper fold, two folds as peers, and the banana-split pair carrier underneath both.


<a id="org672fc37"></a>

## [Part 6 — Natural Transformations: hoist, prepro, postpro](part-6-prepro-postpro.md)

Rewriting every layer, and fusing the rewrite into folds and unfolds.


<a id="org02fa99e"></a>

## [Interlude — Free, Cofree, and Their Free-er Relatives](part-6.5-free-cofree.md)

Where Free and Cofree come from (the adjunctions), how they are `Fix` with variables or annotations, the naive vs Church encodings, and Kiselyov's Freer — plus the dual the title asks about, all with compiled code.


<a id="orgc487774"></a>

## [Part 7 — Histomorphisms and Futumorphisms](part-7-histo-futu.md)

Course-of-values recursion: Cofree histories for folds, Free chunks for unfolds, coin change and run-length decoding.


<a id="orgc572354"></a>

## [Part 8 — Dynamorphisms and Chronomorphisms](part-8-dyna-chrono.md)

The fused forms: dynamic programming from a seed, Fibonacci with the memo table as a comonad.


<a id="org860eff1"></a>

## [Part 9 — Mendler Style](part-9-mendler.md)

Folds with no Functor instance: the algebra receives the recursive call, and a rank-2 guarantee becomes a discipline.


<a id="org6454e59"></a>

## [Part 10 — Elgot Algebras](part-10-elgot.md)

Refolds where the coalgebra can stop with an answer, the dual where the algebra sees the seed, and testing the work instead of the answer.


<a id="orgf1500bd"></a>

## [Part 11 — Distributive Laws and the Generalized Schemes](part-11-generalized.md)

The reveal: gcata and gana subsume the catalog, one distributive law per scheme, with the executable proof.


<a id="org38d82d9"></a>

## [Part 12 — The Capstone: zygoHistoPrepro](part-12-capstone.md)

Composing comonads: zygo + histo + prepro in one pass, and the dedicated EnvT-style instance the generic pair could not provide.


<a id="orgbb54949"></a>

## [Part 13 — Conclusion](part-13-conclusion.md)

The catalog in review, the ledger of what C++ pushed back on, and where the trail continues.


<a id="org7325ce9"></a>

# Table of Contents


# Table of Contents

1.  [Overview](#org8f5481a)
2.  [Blog Posts](#orge203e0c)
3.  [Table of Contents](#org7325ce9)
