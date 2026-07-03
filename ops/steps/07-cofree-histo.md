# S07 — Cofree comonad and histo

**Goal.** The Cofree comonad type with its comonad_typeclass instance,
and the histomorphism built on it, with the coin-change example.

**Depends on:** S02, S03.
**Design refs:** §5.3, §6.3, §6.4, §7.5 (histo half), §9, §10.

## Do
1. **`src/smd/fixpoint/cofree.hpp`** — `Cofree<F, A>` per design §5.3:
   aggregate `{A head; F<Cofree<F,A>> tail;}`, helpers
   `extract`/`unwrap_cofree`, defaulted `==` if the members support it.
   Then the typeclass instances (reopening `namespace smd::typeclass`):
   - `functor_typeclass<Cofree<F, A>>` — maps head, recurses through
     tail via `layer_fmap`.
   - `comonad_typeclass<Cofree<F, A>>` — extract = head;
     `duplicate(c) -> Cofree<F, Cofree<F,A>>` = `{c,
     layer_fmap(duplicate, c.tail)}`; fmap as above. Partial
     specialization over `<template <class> class F, class A>`.
2. **`src/smd/fixpoint/histo.hpp`** — `histo<Result>` per design §7.5:
   `extract(fold_fix<Cofree<F, Result>>(λx. Cofree{φ(x), x}, tree))`.
   Note the algebra's layer `x` is used twice (once through φ, once
   stored) — copy is fine (D6). constexpr.
3. **Tests** `cofree.t.cpp`, `histo.t.cpp`:
   - Cofree: build a small annotated Nat by hand; extract/duplicate
     laws (`extract(duplicate(c)) == c`,
     `fmap(extract, duplicate(c)) == c` — needs Cofree ==), fmap
     behavior.
   - Law: histo with an algebra that only looks at children's `head`s ≡
     `fold_fix` (§9), Nat 0..10.
   - Behavior: Fibonacci via histo on Nat — algebra: Zero → 0; Succ c →
     if c's tail is Zero then 1 else c.head + (head of the Succ inside
     c.tail). fib(0..10) == 0,1,1,2,3,5,....
   - Behavior: coin-change minimal count for amount n with coins
     {1,4,5} via histo on Nat (the classic: at Succ, consult histories
     1, 4 and 5 layers back); spot-check n=8 → 2 (4+4), n=12 → 3.
   - constexpr static_assert: histo-fib(6) == 8 at compile time.
4. **Example** `src/examples/histo_coin_change.cpp` (§10): prints the
   minimal coin counts for a few amounts, with the algebra's
   history-walk commented step by step.

## Build
`make TOOLCHAIN=gcc-16 test`; run the example.

## Verify (gate)
Full suite green; example runs/exits 0, printed counts match
hand-checked values.

## Done when
Gate green; committed `[schemes] S07: Cofree + histo`.

## Capture in handoff
Landed Cofree shape and helper names (S09/S12/S15 build on them —
especially how to walk "k levels back" ergonomically); whether the
comonad partial specialization over a template-template parameter
worked as §6.3 assumed; Cofree `==` availability.

## Pitfalls
- `F<Cofree<F,A>>` as a *member* requires the layer alternatives to be
  complete for incomplete Cofree — true because recursive positions are
  Box'd (§4). If GCC balks, the functor definition is wrong, not
  Cofree.
- duplicate's return type nests the template: `Cofree<F, Cofree<F,A>>`;
  write the return type explicitly, deduction won't carry through the
  recursion.
- The history in the coin-change algebra is indexed from the *current*
  node: `c.head` is n-1's answer, one step into `c.tail` is n-2's, etc.
  Off-by-ones here are the classic bug; pin with n=8 → 2.
