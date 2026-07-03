# S15 — gprepro, gpostpro, and the zygo_histo_prepro capstone

**Goal.** The transformed generalized folds, and Kmett's famous
`zygoHistoPrepro` implemented concretely — plus the generalized-schemes
tour example.

**Depends on:** S14, S06.
**Design refs:** §7.11, §3 D8, §9, §10.

## Do
1. **gprepro / gpostpro** in `generalized.hpp` per design §7.11:
   gprepro is S13's gcata worker with `∘ hoist<F>(e)` inserted before
   the recursive call (compare §7.4's prepro vs fold_fix — same
   delta); gpostpro dually against S14's gana. Laws are the arbiter
   (D8):
   - `gprepro(dist_cata, identity_nat, φ')` ≡ `fold_fix(φ)`,
   - `gprepro(dist_cata, e, φ')` ≡ `prepro(e, φ)` (S06's take-while
     fixture),
   - `gprepro(k, identity_nat, φ)` ≡ `gcata(k, φ)` (use dist_histo),
   - `gpostpro(dist_ana, e, ψ')` ≡ `postpro(e, ψ)`;
     `gpostpro(dist_ana, identity_nat, ψ')` ≡ `unfold_fix(ψ)`.
2. **zygo_histo_prepro** per design §7.11: concrete composed comonad
   `W<X> = std::pair<Helper, Cofree<F, X>>` with a one-off
   `dist_zygo_histo(f)` law (factory, in `generalized.hpp` — NOT in
   dist_laws.hpp; it is capstone-specific), then
   `zygo_histo_prepro<Result, Helper> = gprepro(dist_zygo_histo(f), e, g)`.
   No general comonad-transformer machinery (§11).
   - Comonad instance for `pair<Helper, Cofree<F,X>>`: S03's pair-env
     comonad composed with S07's Cofree comonad — check whether the
     existing `comonad_typeclass<std::pair<B, A>>` instance with
     `A = Cofree<F,X>` already gives the right duplicate for gprepro's
     needs (it duplicates only the pair layer). If gprepro's worker
     needs the *composed* duplicate, write a dedicated instance keyed
     on `pair<Helper, Cofree<F, X>>` — decide by making the
     degeneracy law pass, and record the outcome (likely deviation
     material; §7.11 flags this as the thin ice).
3. **Tests** `gprepro.t.cpp` — the laws above, plus the §7.11
   degeneracy: `zygo_histo_prepro` with `e = identity`, helper ignored,
   Cofree used only via extract ≡ `fold_fix`; and one real behavior
   test: on IntList, helper = length-so-far, transformation = the S06
   take-while, main algebra consults both helper and one-step history
   (construct something checkable by hand, e.g. "sum of elements at
   even positions of the positive prefix").
4. **Example** `src/examples/generalized_tour.cpp` (§10): walk the
   recovery table — one structure, folded via fold_fix, then
   cata_via_gcata, histo_via_gcata, ghylo-as-dyna, closing with the
   zygo_histo_prepro behavior test's computation, printing each result
   and one line on which dist law produced it.

## Build
`make TOOLCHAIN=gcc-16 test`; run the tour example.

## Verify (gate)
Full suite green; tour runs/exits 0 and every printed pair of
"specialized vs generalized" values matches.

## Done when
Gate green; committed `[schemes] S15: gprepro + zygo_histo_prepro`.

## Capture in handoff
Which comonad-instance route step 2 took and why; landed
zygo_histo_prepro signature; deviations filed. S16 packages everything
— list any header that still lacks an umbrella-worthy doc comment.

## Pitfalls
- This step has the highest chance of a genuine design error (§7.11's
  equations were transcribed, not proven). Budget accordingly: get the
  gprepro laws green before touching the capstone.
- The capstone's `g` sees `F<pair<Helper, Cofree<F, Result>>>` — write
  the behavior test's algebra with explicit types, no `auto`
  parameters, so error messages stay readable.
- If the composed-comonad question (step 2) blocks for more than a
  reasonable effort, ship gprepro/gpostpro, mark the capstone BLOCKED
  in the handoff with the exact failing law, and stop — S16 does not
  depend on the capstone.
