# S05 — zygo and mutu

**Goal.** Fokkinga's zygomorphism (fold with a helper fold) and
mutumorphism (mutually recursive folds), via the pairing/banana-split
implementations, with laws and examples.

**Depends on:** S02.
**Design refs:** §7.3, §3 D5/D10, §9, §10.

## Do
1. **`src/smd/fixpoint/zygo.hpp`** — `zygo<Result, Helper>` per design
   §7.3: implemented as a single `fold_fix` with carrier
   `std::pair<Helper, Result>` and algebra
   `λx. {helper(layer_fmap(.first, x)), main(x)}`, then `.second` of the
   result. Convention: **helper value is `.first`, main is `.second`**
   (matches §7.3 and dist_zygo in S12 — do not flip it).
2. **`src/smd/fixpoint/mutu.hpp`** — `mutu<A, B>` returning
   `std::pair<A, B>` per design §7.3 (one fold, paired algebra). Add
   convenience projections `mutu_fst`/`mutu_snd` only if the tests want
   them; otherwise skip (minimal diff).
3. **Tests** `zygo.t.cpp`, `mutu.t.cpp`:
   - Law: zygo with main algebra ignoring helper components ≡
     `fold_fix` (§9), Nat 0..10.
   - Behavior: zygo_balanced logic on IntTree — helper computes height,
     main computes "is height-balanced (subtree heights differ ≤ 1)";
     assert on a balanced and an unbalanced tree.
   - Law: `mutu(f, g)` equals the pair computed by `fold_fix` with the
     hand-paired algebra (they're the same construction — the test
     guards against drift).
   - Behavior: even/odd on Nat via mutu — `alg_even` says a Succ is
     even iff pred is odd (`.second`), `alg_odd` dually; check n=0..10
     against `n % 2`.
   - constexpr static_asserts (small Nat).
4. **Examples** `src/examples/zygo_balanced.cpp`,
   `src/examples/mutu_even_odd.cpp` (§10) — same wiring pattern as S04.

## Build
`make TOOLCHAIN=gcc-16 test`; run both examples.

## Verify (gate)
Full suite green; examples run/exit 0 with visibly correct output.

## Done when
Gate green; committed `[schemes] S05: zygo + mutu`.

## Capture in handoff
Landed signatures; the pair-order convention as implemented (S12's
`dist_zygo` and S15's capstone must match it); anything about carrier
pair copies worth knowing.

## Pitfalls
- In zygo's algebra, `layer_fmap(.first, x)` runs a *second* fmap over
  the already-mapped layer — that's inherent to the pairing
  construction; don't try to fuse it.
- Even/odd base case: Zero is even → `alg_even(Zero) = true`,
  `alg_odd(Zero) = false`. Get this right or the whole example
  inverts.
