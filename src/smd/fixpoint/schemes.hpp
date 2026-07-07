// src/smd/fixpoint/schemes.hpp                                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_SCHEMES
#define INCLUDED_SMD_FIXPOINT_SCHEMES

// Umbrella header (design §8): one #include for the whole recursion-schemes
// catalog built out by ops/PLAN.md (S00-S16). Pulls in every scheme header
// plus the supporting typeclass types they are built on. See
// docs/recursion-schemes.md for the user-facing catalog (signatures as
// landed, equations, usage snippets, example pointers) and
// docs/recursion-schemes-design.md for the design rationale/decisions log.
//
// Scheme/type -> header map (design §8's table; the "§7.N" tags are where
// each scheme's equation is documented in the design doc):
//
//   Supporting types and dispatch:
//     layer_fmap                                    fmap.hpp
//     NatF, ListF<E,.>, TreeF<E,.>, ExprF + ctors    functors.hpp
//     Identity                                       smd/typeclass/identity.hpp
//     either (+ P2988 reference sides)                smd/typeclass/either.hpp
//     pair functor + env comonad                      smd/typeclass/pair.hpp
//     Comonad (CRTP base + lookup) smd/typeclass/comonad.hpp
//
//   §7.1  fold_fix, unfold_fix, refold (classical)    recursion_schemes.hpp
//   §7.2  para, apo                                    para.hpp, apo.hpp
//   §7.3  zygo, mutu                                    zygo.hpp, mutu.hpp
//   §7.4  hoist, prepro, postpro                         prepro.hpp
//   §7.5  Cofree, histo                                   cofree.hpp, histo.hpp
//         Free, futu                                       free.hpp, futu.hpp
//   §7.6  dyna, codyna, chrono                              chrono.hpp
//   §7.7  mcata, mhisto                                      mendler.hpp
//   §7.8  elgot, coelgot                                      elgot.hpp
//   §7.9  dist_cata, dist_ana, dist_histo, dist_futu,          dist_laws.hpp
//         dist_zygo, dist_para, dist_apo, dist_gapo
//   §7.10 gcata, gana, ghylo (+ recovery aliases: generalized.hpp
//         cata_via_gcata, histo_via_gcata, zygo_via_gcata,
//         para_via_gcata, ana_via_gana, apo_via_gana,
//         futu_via_gana)
//   §7.11 gprepro, gpostpro, zygo_histo_prepro generalized.hpp
//         (+ the one-off dist_zygo_histo law and the composed
//         pair<Helper, Cofree<F,X>> comonad instance)
//
// generalized.hpp holds gcata/gana/ghylo/gprepro/gpostpro/zygo_histo_prepro
// together in a single shared file, grown incrementally across S13-S15,
// rather than the one-header-per-scheme split the design's own §8 table
// sketch first assumed -- this map reflects that landed reality, not the
// original per-scheme-file assumption (see ops/DEVIATIONS.md and the
// S13/S14/S15 handoffs).

#include <smd/fixpoint/apo.hpp>
#include <smd/fixpoint/chrono.hpp>
#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/dist_laws.hpp>
#include <smd/fixpoint/elgot.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/futu.hpp>
#include <smd/fixpoint/generalized.hpp>
#include <smd/fixpoint/histo.hpp>
#include <smd/fixpoint/mendler.hpp>
#include <smd/fixpoint/mutu.hpp>
#include <smd/fixpoint/para.hpp>
#include <smd/fixpoint/prepro.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>
#include <smd/fixpoint/zygo.hpp>

#include <smd/typeclass/comonad.hpp>
#include <smd/typeclass/either.hpp>
#include <smd/typeclass/identity.hpp>
#include <smd/typeclass/pair.hpp>

#endif
