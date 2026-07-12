/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "fixpoint", "index.html", [
    [ "replacing_vist", "md_src_smd_fixpoint_replacing_vist.html", null ],
    [ "freer-future-work", "md_docs_blog_freer_future_work.html", [
      [ "The closure chain", "md_docs_blog_freer_future_work.html#autotoc_md3", null ],
      [ "The one-shot that could have been <tt>std::move_only_function</tt>", "md_docs_blog_freer_future_work.html#autotoc_md4", null ],
      [ "Frame per effect", "md_docs_blog_freer_future_work.html#autotoc_md5", null ]
    ] ],
    [ "index", "md_docs_blog_index.html", [
      [ "Overview", "md_docs_blog_index.html#autotoc_md6", null ],
      [ "Blog Posts", "md_docs_blog_index.html#autotoc_md7", [
        [ "@ref /home/runner/work/fixpoint/fixpoint/docs/blog/part-0-intro.md \"Part 0 — Why a Field Guide?\"", "md_docs_blog_index.html#autotoc_md8", null ],
        [ "@ref /home/runner/work/fixpoint/fixpoint/docs/blog/part-1-fixpoint.md \"Part 1 — Fix and Box: Tying the Knot\"", "md_docs_blog_index.html#autotoc_md9", null ],
        [ "@ref /home/runner/work/fixpoint/fixpoint/docs/blog/part-2-typeclasses.md \"Part 2 — Typeclass Objects and layer\\_fmap\"", "md_docs_blog_index.html#autotoc_md10", null ],
        [ "@ref /home/runner/work/fixpoint/fixpoint/docs/blog/part-3-classical.md \"Part 3 — Fold, Unfold, Refold\"", "md_docs_blog_index.html#autotoc_md11", null ],
        [ "@ref /home/runner/work/fixpoint/fixpoint/docs/blog/part-4-para-apo.md \"Part 4 — Paramorphisms and Apomorphisms\"", "md_docs_blog_index.html#autotoc_md12", null ],
        [ "@ref /home/runner/work/fixpoint/fixpoint/docs/blog/part-5-zygo-mutu.md \"Part 5 — Zygomorphisms and Mutumorphisms\"", "md_docs_blog_index.html#autotoc_md13", null ],
        [ "@ref /home/runner/work/fixpoint/fixpoint/docs/blog/part-6-prepro-postpro.md \"Part 6 — Natural Transformations: hoist, prepro, postpro\"", "md_docs_blog_index.html#autotoc_md14", null ],
        [ "@ref /home/runner/work/fixpoint/fixpoint/docs/blog/part-6.5-free-cofree.md \"Interlude — Free, Cofree, and Their Free-er Relatives\"", "md_docs_blog_index.html#autotoc_md15", null ],
        [ "@ref /home/runner/work/fixpoint/fixpoint/docs/blog/part-7-histo-futu.md \"Part 7 — Histomorphisms and Futumorphisms\"", "md_docs_blog_index.html#autotoc_md16", null ],
        [ "@ref /home/runner/work/fixpoint/fixpoint/docs/blog/part-8-dyna-chrono.md \"Part 8 — Dynamorphisms and Chronomorphisms\"", "md_docs_blog_index.html#autotoc_md17", null ],
        [ "@ref /home/runner/work/fixpoint/fixpoint/docs/blog/part-9-mendler.md \"Part 9 — Mendler Style\"", "md_docs_blog_index.html#autotoc_md18", null ],
        [ "@ref /home/runner/work/fixpoint/fixpoint/docs/blog/part-10-elgot.md \"Part 10 — Elgot Algebras\"", "md_docs_blog_index.html#autotoc_md19", null ],
        [ "@ref /home/runner/work/fixpoint/fixpoint/docs/blog/part-11-generalized.md \"Part 11 — Distributive Laws and the Generalized Schemes\"", "md_docs_blog_index.html#autotoc_md20", null ],
        [ "@ref /home/runner/work/fixpoint/fixpoint/docs/blog/part-12-capstone.md \"Part 12 — The Capstone: zygoHistoPrepro\"", "md_docs_blog_index.html#autotoc_md21", null ],
        [ "@ref /home/runner/work/fixpoint/fixpoint/docs/blog/part-13-conclusion.md \"Part 13 — Conclusion\"", "md_docs_blog_index.html#autotoc_md22", null ]
      ] ],
      [ "Table of Contents", "md_docs_blog_index.html#autotoc_md23", null ],
      [ "Table of Contents", "md_docs_blog_index.html#autotoc_md24", null ]
    ] ],
    [ "part-0-intro", "md_docs_blog_part_0_intro.html", [
      [ "The Motivation", "md_docs_blog_part_0_intro.html#autotoc_md25", null ],
      [ "What Makes This Possible in C++", "md_docs_blog_part_0_intro.html#autotoc_md26", null ],
      [ "The Phase-by-Phase Roadmap", "md_docs_blog_part_0_intro.html#autotoc_md27", null ],
      [ "How to Read the Series", "md_docs_blog_part_0_intro.html#autotoc_md28", null ],
      [ "References", "md_docs_blog_part_0_intro.html#autotoc_md29", null ]
    ] ],
    [ "part-1-fixpoint", "md_docs_blog_part_1_fixpoint.html", [
      [ "The Shape of One Layer", "md_docs_blog_part_1_fixpoint.html#autotoc_md30", null ],
      [ "Fix: The Knot", "md_docs_blog_part_1_fixpoint.html#autotoc_md31", null ],
      [ "The Magic Trick", "md_docs_blog_part_1_fixpoint.html#autotoc_md32", null ],
      [ "Box: The Indirection That Survives constexpr", "md_docs_blog_part_1_fixpoint.html#autotoc_md33", null ],
      [ "Composition, Not Inheritance", "md_docs_blog_part_1_fixpoint.html#autotoc_md34", null ],
      [ "Smart Constructors, and a Tree at Last", "md_docs_blog_part_1_fixpoint.html#autotoc_md35", null ],
      [ "The Rest of the Bestiary", "md_docs_blog_part_1_fixpoint.html#autotoc_md36", null ],
      [ "References", "md_docs_blog_part_1_fixpoint.html#autotoc_md37", null ]
    ] ],
    [ "part-10-elgot", "md_docs_blog_part_10_elgot.html", [
      [ "elgot: The Coalgebra's Veto", "md_docs_blog_part_10_elgot.html#autotoc_md38", null ],
      [ "coelgot: The Algebra Gets the Seed", "md_docs_blog_part_10_elgot.html#autotoc_md39", null ],
      [ "The Pattern Completing", "md_docs_blog_part_10_elgot.html#autotoc_md40", null ],
      [ "References", "md_docs_blog_part_10_elgot.html#autotoc_md41", null ]
    ] ],
    [ "part-11-generalized", "md_docs_blog_part_11_generalized.html", [
      [ "The Carriers Were (Co)monads All Along", "md_docs_blog_part_11_generalized.html#autotoc_md42", null ],
      [ "The Laws Themselves", "md_docs_blog_part_11_generalized.html#autotoc_md43", null ],
      [ "gcata: One Fold to Rule Them", "md_docs_blog_part_11_generalized.html#autotoc_md44", null ],
      [ "gana and ghylo: The Mirror and the Fusion", "md_docs_blog_part_11_generalized.html#autotoc_md45", null ],
      [ "The Proof Runs", "md_docs_blog_part_11_generalized.html#autotoc_md46", null ],
      [ "References", "md_docs_blog_part_11_generalized.html#autotoc_md47", null ]
    ] ],
    [ "part-12-capstone", "md_docs_blog_part_12_capstone.html", [
      [ "gprepro: Part 6's Delta, One Level Up", "md_docs_blog_part_12_capstone.html#autotoc_md48", null ],
      [ "The Distributive Law for a Composed Comonad", "md_docs_blog_part_12_capstone.html#autotoc_md49", null ],
      [ "The Thin Ice: Why the Generic Pair Instance Is Wrong", "md_docs_blog_part_12_capstone.html#autotoc_md50", null ],
      [ "The Capstone Computes", "md_docs_blog_part_12_capstone.html#autotoc_md51", null ],
      [ "References", "md_docs_blog_part_12_capstone.html#autotoc_md52", null ]
    ] ],
    [ "part-13-conclusion", "md_docs_blog_part_13_conclusion.html", [
      [ "The Field Guide in Review", "md_docs_blog_part_13_conclusion.html#autotoc_md53", null ],
      [ "What C++ Pushed Back On", "md_docs_blog_part_13_conclusion.html#autotoc_md54", null ],
      [ "What Was Deliberately Not Done", "md_docs_blog_part_13_conclusion.html#autotoc_md55", null ],
      [ "Where the Trail Continues", "md_docs_blog_part_13_conclusion.html#autotoc_md56", null ],
      [ "References", "md_docs_blog_part_13_conclusion.html#autotoc_md57", null ]
    ] ],
    [ "part-2-typeclasses", "md_docs_blog_part_2_typeclasses.html", [
      [ "What the Schemes Need", "md_docs_blog_part_2_typeclasses.html#autotoc_md58", null ],
      [ "The Instance Registry", "md_docs_blog_part_2_typeclasses.html#autotoc_md59", null ],
      [ "layer_fmap: One Bridge, Three Doors", "md_docs_blog_part_2_typeclasses.html#autotoc_md60", null ],
      [ "Why an Object, Not a Trait", "md_docs_blog_part_2_typeclasses.html#autotoc_md61", null ],
      [ "References", "md_docs_blog_part_2_typeclasses.html#autotoc_md62", null ]
    ] ],
    [ "part-3-classical", "md_docs_blog_part_3_classical.html", [
      [ "The Algebra Handles One Layer", "md_docs_blog_part_3_classical.html#autotoc_md63", null ],
      [ "fold_fix: The Catamorphism", "md_docs_blog_part_3_classical.html#autotoc_md64", null ],
      [ "unfold_fix: The Anamorphism", "md_docs_blog_part_3_classical.html#autotoc_md65", null ],
      [ "refold: The Hylomorphism", "md_docs_blog_part_3_classical.html#autotoc_md66", null ],
      [ "The Explicit-fmap Escape Hatch", "md_docs_blog_part_3_classical.html#autotoc_md67", null ],
      [ "Why Not cata?", "md_docs_blog_part_3_classical.html#autotoc_md68", null ],
      [ "References", "md_docs_blog_part_3_classical.html#autotoc_md69", null ]
    ] ],
    [ "part-4-para-apo", "md_docs_blog_part_4_para_apo.html", [
      [ "para: A Fold That Remembers the Original", "md_docs_blog_part_4_para_apo.html#autotoc_md70", null ],
      [ "apo: An Unfold That Can Stop Early", "md_docs_blog_part_4_para_apo.html#autotoc_md71", null ],
      [ "either: The Sum the Library Actually Wanted", "md_docs_blog_part_4_para_apo.html#autotoc_md72", null ],
      [ "References", "md_docs_blog_part_4_para_apo.html#autotoc_md73", null ]
    ] ],
    [ "part-5-zygo-mutu", "md_docs_blog_part_5_zygo_mutu.html", [
      [ "The Problem That Needs a Helper", "md_docs_blog_part_5_zygo_mutu.html#autotoc_md74", null ],
      [ "zygo: A Fold With a Sidecar", "md_docs_blog_part_5_zygo_mutu.html#autotoc_md75", null ],
      [ "mutu: Two Folds as Peers", "md_docs_blog_part_5_zygo_mutu.html#autotoc_md76", null ],
      [ "One Trick, Three Schemes", "md_docs_blog_part_5_zygo_mutu.html#autotoc_md77", null ],
      [ "References", "md_docs_blog_part_5_zygo_mutu.html#autotoc_md78", null ]
    ] ],
    [ "part-6-prepro-postpro", "md_docs_blog_part_6_prepro_postpro.html", [
      [ "What \"Natural\" Buys You", "md_docs_blog_part_6_prepro_postpro.html#autotoc_md79", null ],
      [ "hoist: Retag Every Layer", "md_docs_blog_part_6_prepro_postpro.html#autotoc_md80", null ],
      [ "prepro: The Rewrite Fused Into the Fold", "md_docs_blog_part_6_prepro_postpro.html#autotoc_md81", null ],
      [ "postpro: The Mirror", "md_docs_blog_part_6_prepro_postpro.html#autotoc_md82", null ],
      [ "The Shape of the Delta", "md_docs_blog_part_6_prepro_postpro.html#autotoc_md83", null ],
      [ "References", "md_docs_blog_part_6_prepro_postpro.html#autotoc_md84", null ]
    ] ],
    [ "part-6.5-free-cofree", "md_docs_blog_part_6_5_free_cofree.html", [
      [ "Where \"Free\" and \"Cofree\" Come From", "md_docs_blog_part_6_5_free_cofree.html#autotoc_md85", null ],
      [ "Fix Is the Degenerate Case of Both", "md_docs_blog_part_6_5_free_cofree.html#autotoc_md86", null ],
      [ "The Naive Implementation, in Both Languages", "md_docs_blog_part_6_5_free_cofree.html#autotoc_md87", null ],
      [ "Representing a Free by Its Own Fold: the Church Encoding", "md_docs_blog_part_6_5_free_cofree.html#autotoc_md88", null ],
      [ "Freer: the Monad That Forgot It Needed a Functor", "md_docs_blog_part_6_5_free_cofree.html#autotoc_md89", null ],
      [ "Side-Light: C++26 Already Shipped a Freer", "md_docs_blog_part_6_5_free_cofree.html#autotoc_md90", null ],
      [ "Is There a CoFreer?", "md_docs_blog_part_6_5_free_cofree.html#autotoc_md91", null ],
      [ "The Ledger", "md_docs_blog_part_6_5_free_cofree.html#autotoc_md92", null ],
      [ "References", "md_docs_blog_part_6_5_free_cofree.html#autotoc_md93", null ]
    ] ],
    [ "part-7-histo-futu", "md_docs_blog_part_7_histo_futu.html", [
      [ "Cofree: A Tree Annotated With Its Own Past", "md_docs_blog_part_7_histo_futu.html#autotoc_md94", null ],
      [ "histo: Fold With Total Recall", "md_docs_blog_part_7_histo_futu.html#autotoc_md95", null ],
      [ "Free: Computation Chunks With Seeds at the Leaves", "md_docs_blog_part_7_histo_futu.html#autotoc_md96", null ],
      [ "futu: Unfold Several Layers per Step", "md_docs_blog_part_7_histo_futu.html#autotoc_md97", null ],
      [ "The Symmetry Ledger", "md_docs_blog_part_7_histo_futu.html#autotoc_md98", null ],
      [ "References", "md_docs_blog_part_7_histo_futu.html#autotoc_md99", null ]
    ] ],
    [ "part-8-dyna-chrono", "md_docs_blog_part_8_dyna_chrono.html", [
      [ "Three Fusions, One Header", "md_docs_blog_part_8_dyna_chrono.html#autotoc_md100", null ],
      [ "dyna: The Dynamorphism", "md_docs_blog_part_8_dyna_chrono.html#autotoc_md101", null ],
      [ "codyna and chrono: Chunks on the Way In", "md_docs_blog_part_8_dyna_chrono.html#autotoc_md102", null ],
      [ "Where the Streak Stands", "md_docs_blog_part_8_dyna_chrono.html#autotoc_md103", null ],
      [ "References", "md_docs_blog_part_8_dyna_chrono.html#autotoc_md104", null ]
    ] ],
    [ "part-9-mendler", "md_docs_blog_part_9_mendler.html", [
      [ "Inverting the Contract", "md_docs_blog_part_9_mendler.html#autotoc_md105", null ],
      [ "What the Types No Longer Prove", "md_docs_blog_part_9_mendler.html#autotoc_md106", null ],
      [ "mhisto: History Without the Annotation", "md_docs_blog_part_9_mendler.html#autotoc_md107", null ],
      [ "Where This Sits in the Catalog", "md_docs_blog_part_9_mendler.html#autotoc_md108", null ],
      [ "References", "md_docs_blog_part_9_mendler.html#autotoc_md109", null ]
    ] ],
    [ "Recursion Schemes for smd::fixpoint — Design", "md_docs_recursion_schemes_design.html", [
      [ "§1 Purpose and scope", "md_docs_recursion_schemes_design.html#autotoc_md111", null ],
      [ "§2 Ground: what already exists", "md_docs_recursion_schemes_design.html#autotoc_md112", null ],
      [ "§3 Decisions log", "md_docs_recursion_schemes_design.html#autotoc_md113", null ],
      [ "§4 Core conventions", "md_docs_recursion_schemes_design.html#autotoc_md114", null ],
      [ "§5 Supporting types", "md_docs_recursion_schemes_design.html#autotoc_md115", [
        [ "§5.1 Identity (smd/typeclass/identity.hpp, S03)", "md_docs_recursion_schemes_design.html#autotoc_md116", null ],
        [ "§5.2 either (smd/typeclass/either.hpp, S03)", "md_docs_recursion_schemes_design.html#autotoc_md117", null ],
        [ "§5.3 Cofree comonad (src/smd/fixpoint/cofree.hpp, S07)", "md_docs_recursion_schemes_design.html#autotoc_md118", null ],
        [ "§5.4 Free monad (src/smd/fixpoint/free.hpp, S08)", "md_docs_recursion_schemes_design.html#autotoc_md119", null ]
      ] ],
      [ "§6 Typeclass extensions", "md_docs_recursion_schemes_design.html#autotoc_md120", [
        [ "§6.1 functor_typeclass instances for base functors (S01/S02)", "md_docs_recursion_schemes_design.html#autotoc_md121", null ],
        [ "§6.2 Lookup-based scheme overloads (S01)", "md_docs_recursion_schemes_design.html#autotoc_md122", null ],
        [ "§6.3 Comonad typeclass (smd/typeclass/comonad.hpp, S03)", "md_docs_recursion_schemes_design.html#autotoc_md123", null ],
        [ "§6.4 Instance/spec summary table", "md_docs_recursion_schemes_design.html#autotoc_md124", null ]
      ] ],
      [ "§7 Scheme catalog", "md_docs_recursion_schemes_design.html#autotoc_md125", [
        [ "§7.1 Classical recap (existing; S01 adds lookup overloads)", "md_docs_recursion_schemes_design.html#autotoc_md126", null ],
        [ "§7.2 para and apo (S04) — para.hpp, apo.hpp", "md_docs_recursion_schemes_design.html#autotoc_md127", null ],
        [ "§7.3 zygo and mutu (S05) — zygo.hpp, mutu.hpp", "md_docs_recursion_schemes_design.html#autotoc_md128", null ],
        [ "§7.4 hoist, prepro, postpro (S06) — prepro.hpp", "md_docs_recursion_schemes_design.html#autotoc_md129", null ],
        [ "§7.5 histo and futu (S07, S08) — cofree.hpp+histo.hpp, free.hpp+futu.hpp", "md_docs_recursion_schemes_design.html#autotoc_md130", null ],
        [ "§7.6 dyna, codyna, chrono (S09) — chrono.hpp", "md_docs_recursion_schemes_design.html#autotoc_md131", null ],
        [ "§7.7 Mendler-style: mcata, mhisto (S10) — mendler.hpp", "md_docs_recursion_schemes_design.html#autotoc_md132", null ],
        [ "§7.8 Elgot (co)algebras (S11) — elgot.hpp", "md_docs_recursion_schemes_design.html#autotoc_md133", null ],
        [ "§7.9 Distributive laws (S12) — dist_laws.hpp", "md_docs_recursion_schemes_design.html#autotoc_md134", null ],
        [ "§7.10 gcata, gana, ghylo (S13, S14) — generalized.hpp", "md_docs_recursion_schemes_design.html#autotoc_md135", null ],
        [ "§7.11 gprepro, gpostpro, zygo_histo_prepro (S15)", "md_docs_recursion_schemes_design.html#autotoc_md136", null ]
      ] ],
      [ "§8 File and target layout", "md_docs_recursion_schemes_design.html#autotoc_md137", null ],
      [ "§9 Testing strategy — the equivalence-law suite", "md_docs_recursion_schemes_design.html#autotoc_md138", null ],
      [ "§10 Examples catalog (src/examples/)", "md_docs_recursion_schemes_design.html#autotoc_md139", null ],
      [ "§11 Non-goals", "md_docs_recursion_schemes_design.html#autotoc_md140", null ]
    ] ],
    [ "Recursion schemes — usage catalog", "md_docs_recursion_schemes.html", [
      [ "§7.1 The classical recap — <tt>fold_fix</tt>, <tt>unfold_fix</tt>, <tt>refold</tt>", "md_docs_recursion_schemes.html#autotoc_md143", null ],
      [ "§7.2 <tt>para</tt> and <tt>apo</tt> — Fokkinga's classical extensions", "md_docs_recursion_schemes.html#autotoc_md145", [
        [ "para", "md_docs_recursion_schemes.html#autotoc_md146", null ],
        [ "apo", "md_docs_recursion_schemes.html#autotoc_md147", null ]
      ] ],
      [ "§7.3 <tt>zygo</tt> and <tt>mutu</tt> — auxiliary and mutual folds", "md_docs_recursion_schemes.html#autotoc_md149", [
        [ "zygo", "md_docs_recursion_schemes.html#autotoc_md150", null ],
        [ "mutu", "md_docs_recursion_schemes.html#autotoc_md151", null ]
      ] ],
      [ "§7.4 <tt>hoist</tt>, <tt>prepro</tt>, <tt>postpro</tt> — natural-transformation plumbing", "md_docs_recursion_schemes.html#autotoc_md153", null ],
      [ "§7.5 <tt>histo</tt> and <tt>futu</tt> — course-of-values (co)recursion", "md_docs_recursion_schemes.html#autotoc_md155", [
        [ "histo", "md_docs_recursion_schemes.html#autotoc_md156", null ],
        [ "futu", "md_docs_recursion_schemes.html#autotoc_md157", null ]
      ] ],
      [ "§7.6 <tt>dyna</tt>, <tt>codyna</tt>, <tt>chrono</tt> — fused course-of-values refolds", "md_docs_recursion_schemes.html#autotoc_md159", null ],
      [ "§7.7 Mendler-style <tt>mcata</tt> and <tt>mhisto</tt>", "md_docs_recursion_schemes.html#autotoc_md161", null ],
      [ "§7.8 Elgot (co)algebras — <tt>elgot</tt> and <tt>coelgot</tt>", "md_docs_recursion_schemes.html#autotoc_md163", null ],
      [ "§7.9 Distributive laws", "md_docs_recursion_schemes.html#autotoc_md165", null ],
      [ "§7.10 <tt>gcata</tt>, <tt>gana</tt>, <tt>ghylo</tt> — comonadic/monadic generalizations", "md_docs_recursion_schemes.html#autotoc_md167", null ],
      [ "§7.11 <tt>gprepro</tt>, <tt>gpostpro</tt>, <tt>zygo_histo_prepro</tt> — the capstone", "md_docs_recursion_schemes.html#autotoc_md169", null ],
      [ "Supporting types", "md_docs_recursion_schemes.html#autotoc_md171", null ]
    ] ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", "namespacemembers_dup" ],
        [ "Functions", "namespacemembers_func.html", "namespacemembers_func" ],
        [ "Variables", "namespacemembers_vars.html", null ],
        [ "Typedefs", "namespacemembers_type.html", null ]
      ] ]
    ] ],
    [ "Concepts", "concepts.html", "concepts" ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", null ],
        [ "Variables", "functions_vars.html", null ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Related Functions", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", "globals_dup" ],
        [ "Functions", "globals_func.html", "globals_func" ],
        [ "Typedefs", "globals_type.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"fmap_8t_8cpp.html#a054748e5e03121eb22400ea90da319a7",
"functors_8hpp.html#af87544fd5fbad43b7d6937e445c79dd3",
"md_docs_blog_part_13_conclusion.html#autotoc_md54",
"namespaceanonymous__namespace_02free_8t_8cpp_03.html#a349ab364945e84fc2c4958f8a95aaff8",
"namespacemembers_p.html",
"recursion__schemes_8t_8cpp.html",
"structanonymous__namespace_02freer__run_8t_8cpp_03_1_1Put.html#a8b1d2afc3dd2ae4763f271d9e170ed0b",
"structsmd_1_1fixpoint_1_1dist__gapo__t.html#af087ec23a77d5170175ac7c71b4f99db",
"structsmd_1_1typeclass_1_1Foldable.html#acc0f067180a8d4d9d3a347558c7716e0",
"structsmd_1_1typeclass_1_1Right.html#a8d61d95e3e6fac20a624aa2036bbe200"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';