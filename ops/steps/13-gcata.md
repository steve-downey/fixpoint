# S13 — gcata and its recovery laws

**Goal.** The comonadic generalized catamorphism, proven by recovering
fold_fix, histo, zygo and para from the S12 distributive laws.

**Depends on:** S12, S05.
**Design refs:** §7.10 (gcata half), §3 D5/D8, §6.3, §9.

## Do
1. **`src/smd/fixpoint/generalized.hpp`** (new; S14/S15 extend it) —
   `gcata<Result, WResult>` per design §7.10:
   - Compute the worker's return type up front exactly as §7.10
     prescribes: `WWR = decltype(comonad.duplicate(declval<WResult>()))`,
     `C = decltype(dist(declval<F<WWR>>()))` — then the worker is a
     function template with explicit return type `C`:
     `c(t) = dist(layer_fmap(λchild -> WWR { return
     w.duplicate(w.fmap(algebra, c(child))); }, unwrap_fix(t)))`
     with `w = comonad_typeclass<...>` looked up where needed
     (the lookup key for `fmap`/`duplicate` at the inner point is
     `decltype(c(child))`'s value type — in practice look up once from
     `WResult` and once from `C`; let the code drive which lookups are
     actually required and record it).
   - Result: `algebra(extract(c(tree)))` — wait, transcribe §7.10
     *exactly*: `g(extract(c t))` where extract peels `C = W<F<W<A>>>`
     to `F<W<A>>`. Follow the design; if the types refuse, trust the
     recovery laws (D8) and record the corrected composition as a
     deviation.
2. **Recovery aliases** (same header, thin wrappers used by tests and
   §10's tour): `cata_via_gcata<Result>` (dist_cata + Identity),
   `histo_via_gcata<Result>` (dist_histo + Cofree),
   `zygo_via_gcata<Result, Helper>` (dist_zygo(f) + pair),
   `para_via_gcata<Result>` (dist_para + pair<Fix<F>, ·>). Each maps
   the plain algebra to the wrapped-carrier algebra (e.g. for
   dist_cata: `φ' = φ ∘ layer_fmap(.value)`).
3. **Tests** `gcata.t.cpp` — the §9 recovery table, gcata half:
   - `cata_via_gcata` ≡ `fold_fix` (Nat count, list sum; 0..10),
   - `histo_via_gcata` ≡ `histo` (fib 0..10),
   - `zygo_via_gcata` ≡ `zygo` (the S05 balanced fixture),
   - `para_via_gcata` ≡ `para` (the S04 tails fixture),
   - one direct gcata call with explicit template args to pin the
     public spelling: `gcata<int, Identity<int>>(dist_cata, φ', tree)`,
   - constexpr static_assert on cata_via_gcata.

## Build
`make TOOLCHAIN=gcc-16 test`

## Verify (gate)
Full suite green — all four recoveries exact on every tested input.

## Done when
Gate green; committed `[schemes] S13: gcata + recovery laws`.

## Capture in handoff
The exact worker signature and where each comonad lookup happens (S14
mirrors the structure for gana with monad lookups; S15 extends it for
gprepro — spell out the pattern so they can copy it); which §7.10
composition finally type-checked, and whether it matched the design
(deviation if not).

## Pitfalls
- This is the type-heaviest step in the plan. Work bottom-up: get
  `cata_via_gcata` (W = Identity, everything degenerate) compiling
  first; the other three are then mechanical.
- `algebra` inside the worker is fmapped over W — that's the comonad's
  fmap, not the base functor's. Two fmaps of different typeclasses in
  one expression; name intermediates.
- If `WResult`-driven lookup produces "no comonad instance" for
  `pair<Fix<F>, X>`, S03's pair-env instance covers it — check the
  include chain before debugging deduction.
