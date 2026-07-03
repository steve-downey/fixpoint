# S11 — Elgot algebras: elgot and coelgot

**Goal.** The short-circuiting refold (elgot) and its dual, the
seed-observing refold (coelgot), with the product-with-zero-bailout
example.

**Depends on:** S02, S03 (either).
**Design refs:** §3 D4, §5.2, §7.8, §9, §10.

## Do
1. **`src/smd/fixpoint/elgot.hpp`** — both schemes per design §7.8:
   - `elgot<Result>(φ, ψ, seed)`: `ψ(seed)` returns
     `either<Result, F<Seed>>` (D4: Left = final answer); Left →
     return it now (no recursion below this point — that's the whole
     feature); Right → `φ(layer_fmap(elgot..., right(e)))`.
   - `coelgot<Result>(φ, ψ, seed)`: `φ(seed,
     layer_fmap(coelgot..., ψ(seed)))` — the algebra receives the seed
     as a separate first argument (flattened pair, §7.8).
   Both constexpr.
2. **Tests** `elgot.t.cpp`:
   - Law: `elgot(φ, λs. make_right(ψ(s)))` ≡ `refold<Result>(φ, ψ, s)` (§9).
   - Law: `coelgot(λ(s, l). φ(l), ψ)` ≡ `refold<Result>(φ, ψ, s)`.
   - Behavior (short-circuit is real): product over IntListF generation
     from a vector, coalgebra returns Left(0) on seeing 0. Drive it
     with a coalgebra that *counts invocations* (mutable counter by
     reference); assert product([2,3,0,5,...]) == 0 AND the counter
     shows generation stopped at the 0 — the elements after it were
     never visited.
   - Behavior: coelgot where the algebra uses the seed — e.g. building
     the list of running indices: fold result at seed n prepends n;
     assert against hand answer.
   - constexpr static_asserts for both.
3. **Example** `src/examples/elgot_shortcircuit.cpp` (§10): the product
   bailout, printing how many elements were examined vs. list length.

## Build
`make TOOLCHAIN=gcc-16 test`; run the example.

## Verify (gate)
Full suite green; example runs/exits 0 and demonstrably short-circuits
(prints examined-count < length).

## Done when
Gate green; committed `[schemes] S11: elgot + coelgot`.

## Capture in handoff
Landed signatures; the D4 orientation as implemented (Left = stop /
Right = continue) with a pointer to the header comment stating it —
S12's dist_apo and any future reader must not have to rediscover it.

## Pitfalls
- Do NOT swap the either orientation between elgot and apo — D4 fixes
  it globally: Left stops, Right continues (same as the Haskell
  sources). The compiler won't catch a flip when Seed and Result are
  both int; the invocation-counting test will.
- coelgot evaluates `ψ(seed)` exactly once — bind it to a local before
  mapping, or a careless transcription calls it twice.
