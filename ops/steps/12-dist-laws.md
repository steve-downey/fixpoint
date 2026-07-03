# S12 — Distributive laws

**Goal.** The distributive-law function objects that parameterize the
generalized schemes. No new schemes yet — S13/S14 consume these.

**Depends on:** S07, S08 (S03's either/Identity instances arrive
transitively).
**Design refs:** §5.2, §7.9, §3 D4/D8, §6.4.

## Do
1. **`src/smd/fixpoint/dist_laws.hpp`** — polymorphic function objects
   per design §7.9, each a struct with a templated `operator()` and an
   `inline constexpr` instance:
   - `dist_cata` : `F<Identity<X>> -> Identity<F<X>>`
   - `dist_ana`  : `Identity<F<X>> -> F<Identity<X>>`
   - `dist_histo`: `F<Cofree<F,X>> -> Cofree<F, F<X>>`
   - `dist_futu` : `Free<F, F<X>> -> F<Free<F,X>>`
   - `dist_zygo(helper_alg)` — a *factory* returning the law
     `F<std::pair<B,X>> -> std::pair<B, F<X>>` (helper first — S05's
     convention)
   - `dist_para` — `dist_zygo(wrap_fix)`-equivalent; implement as its
     own small object so the layer type works out (`B = Fix<F>`)
   - `dist_apo`  : `either<Fix<F>, F<X>> -> F<either<Fix<F>, X>>`
     (D4: Left = graft; per §7.9, Left(t) fans `unfix(t)`'s children
     out as Lefts)
   - `dist_gapo(coalg)` — factory generalizing dist_apo per §7.9
     (uses either's `map_left` / symmetric construction).
   All constexpr; each carries its Haskell type as a comment.
2. **Tests** `dist_laws.t.cpp` — these objects are only proven by the
   S13/S14 recovery laws, but gate what's checkable now:
   - each law round-trips shapes on NatF/IntListF layers (types and
     values by hand on one- and two-alternative layers);
   - naturality spot-check for dist_cata and dist_histo:
     `dist(layer_fmap(fmap_W(f), l)) == fmap_W(layer_fmap(f))(dist(l))`
     on a concrete layer (write both sides out with the typeclass
     objects);
   - dist_apo on both a Left and a Right input, including one
     `either<Fix<F>, F<Fix<F>>>`-shaped case where Seed = Fix<F>;
   - constexpr static_assert on dist_cata and dist_zygo outputs.

## Build
`make TOOLCHAIN=gcc-16 test`

## Verify (gate)
Full suite green; the naturality checks pass.

## Done when
Gate green; committed `[schemes] S12: distributive laws`.

## Capture in handoff
Exact spellings/instances landed (S13/S14 name them in calls); the
dist_histo recursion pattern (it recurses through Cofree tails — note
the explicit return type trick used); any variance between §7.9's
transcriptions and what the naturality checks forced (deviation rows).

## Pitfalls
- These transcriptions are the likeliest place for silent errors —
  that's why S13/S14 gate on recovering the named schemes (D8). Don't
  over-invest in S12-local testing beyond the listed checks; the
  recovery laws are the real proof.
- `dist_futu`'s Roll branch composes `roll_free ∘ dist_futu` *inside*
  a layer_fmap — mind which functor each fmap runs over (outer F vs
  Free's variant).
- Factories (`dist_zygo`, `dist_gapo`) capture their algebra by value
  into the returned object; keep the returned type a named struct so
  S13 can spell it in explicit template arguments if needed.
