# S09 — dyna, codyna, chrono (fused refolds)

**Goal.** The three course-of-values refolds, each implemented as a
`refold` fusion that never materializes an intermediate `Fix<F>`, with
the Fibonacci dynamorphism example.

**Depends on:** S07, S08.
**Design refs:** §7.6, §9, §10.

## Do
1. **`src/smd/fixpoint/chrono.hpp`** containing all three, per design
   §7.6 exactly:
   - `dyna<Result>(φ, ψ, seed)` =
     `extract(refold<Cofree<F,Result>>(cofree-building algebra, ψ, seed))`.
   - a shared internal `unroll` coalgebra
     `Free<F,S> -> F<Free<F,S>>`: `Pure s → ψ(s); Roll layer → layer`
     (function template, header-internal namespace `detail`).
   - `codyna<Result>(φ, ψ, seed)` = `refold<Result>(φ, unroll,
     pure_free<F>(seed))`.
   - `chrono<Result>(φ, ψ, seed)` = `extract(refold<Cofree<F,Result>>(
     cofree-building algebra, unroll, pure_free<F>(seed)))`.
   All constexpr.
2. **Tests** `chrono.t.cpp`:
   - Law: `dyna(φ, ψ, s)` ≡ `histo(φ, unfold_fix<F>(ψ, s))` for s in
     0..10 (§9).
   - Law: `codyna(φ, ψ, s)` ≡ `fold_fix(φ, futu<F>(ψ, s))`.
   - Law: `chrono(φ, ψ, s)` ≡ `histo(φ, futu<F>(ψ, s))`.
   - Behavior: `dyna_fibonacci` — fib(n) from an int seed with the S07
     histo-fib algebra and the countdown coalgebra; fib(0..10).
   - Behavior: coin-change via dyna directly from the amount (no
     pre-built Nat).
   - constexpr static_assert: dyna-fib(6) == 8.
3. **Example** `src/examples/dyna_fibonacci.cpp` (§10): fib table,
   emphasizing in comments that no Nat tree exists at any point and the
   Cofree history is the DP table.

## Build
`make TOOLCHAIN=gcc-16 test`; run the example.

## Verify (gate)
Full suite green; example runs/exits 0, fib values correct.

## Done when
Gate green; committed `[schemes] S09: dyna + codyna + chrono`.

## Capture in handoff
Landed signatures; where `unroll` lives and its exact spelling (S14's
ghylo equivalence tests reuse the same fixtures); any interplay between
`refold`'s lookup overload and the `Free`-seeded coalgebra worth
noting.

## Pitfalls
- The refold *seed type* differs per scheme: plain seed for dyna,
  `Free<F, Seed>` for codyna/chrono. The lookup `refold<Result, F>`
  deduces the seed — just make sure the coalgebra parameter types
  line up (`const Free<F,S>&`).
- `unroll`'s Roll branch returns the layer *by value* (it holds
  `Free`s, which keep unrolling later) — do not return a reference into
  the variant.
- The three laws make transcription errors in §7.6 impossible to miss —
  run them before polishing the example.
