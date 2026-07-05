# S14 — gana and ghylo with their recovery laws

**Goal.** The monadic generalized anamorphism and the generalized
refold, proven by recovering unfold_fix/apo/futu and
refold/dyna/codyna/chrono.

**Depends on:** S13, S09.
**Design refs:** §7.10 (gana/ghylo halves), §3 D5/D8, §9.

## Do
1. **gana** in `generalized.hpp` — `gana<F, MSeed>` per design §7.10:
   mirror S13's structure with monad lookups
   (`monad_typeclass<...>`: `pure`, `bind`, plus join spelled as
   `bind(m, id)` or the Map's `join`) in place of comonad ops. The
   worker `a : M<F<MSeed>> -> Fix<F>` has a nameable signature (D5),
   so no deduction cycle: transcribe
   `a(m) = wrap_fix(layer_fmap(λmms. a(fmapM(ψ, join(mms))), dist(m)))`
   and entry `gana = a(pure(ψ(seed)))`. As always: if the composition
   fights the types, the recovery laws below are the arbiter (D8).
2. **Recovery aliases**: `ana_via_gana<F>` (dist_ana + Identity),
   `apo_via_gana<F>` (dist_apo + either), `futu_via_gana<F>`
   (dist_futu + Free) — with the seed-wrapping adapters
   (e.g. for dist_ana: `ψ' = layer_fmap(Identity-wrap) ∘ ψ`).
3. **ghylo** — `ghylo<Result, WResult, F, MSeed>`: first cut as design
   §7.10 permits: `gcata` applied to `gana`'s output. Then, if the
   fused version is straightforward under the established patterns,
   fuse; otherwise leave materializing and record which shipped in the
   handoff (the laws don't care).
4. **Tests** `gana.t.cpp`, `ghylo.t.cpp` — §9 recovery table:
   - `ana_via_gana` ≡ `unfold_fix` (0..10),
   - `apo_via_gana` ≡ `apo` (the S04 sorted-insert fixture),
   - `futu_via_gana` ≡ `futu` (the S08 RLE fixture),
   - `ghylo(dist_cata, dist_ana)` ≡ `refold`,
   - `ghylo(dist_histo, dist_ana)` ≡ `dyna` (fib 0..10),
   - `ghylo(dist_cata, dist_futu)` ≡ `codyna`,
   - `ghylo(dist_histo, dist_futu)` ≡ `chrono`,
   - constexpr static_assert on ana_via_gana.

## Build
`make TOOLCHAIN=gcc-16 test`

## Verify (gate)
Full suite green — every recovery exact on every tested input.

## Done when
Gate green; committed `[schemes] S14: gana + ghylo + recovery laws`.

## Capture in handoff
Landed signatures; monad-lookup pattern as implemented (mirror notes
for S15); whether ghylo shipped fused or materializing; any deviation
rows from equation corrections.

## Pitfalls
- `join` on `Free` is structural recursion — make sure S08's monad
  instance `bind` handles the nested case before blaming gana.
- The dist_apo recovery is where a flipped D4 orientation would
  surface — if apo_via_gana grafts where it should recurse, check the
  either orientation (Left = graft) before touching gana.
- Keep the explicit template-argument spelling of one direct
  `gana<IntListF, either<Fix<IntListF>, int>>(...)` call in the
  tests to pin the public interface, as S13 did for gcata.
