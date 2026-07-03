# S10 — Mendler-style mcata and mhisto

**Goal.** Mendler-style catamorphism and histomorphism — the algebra
receives the recursive call (and for mhisto, the unroller) explicitly,
so **no Functor instance is needed**.

**Depends on:** S02.
**Design refs:** §7.7, §9, §10.

## Do
1. **`src/smd/fixpoint/mendler.hpp`** — `mcata<Result>` and
   `mhisto<Result>` per design §7.7. Both are three-line direct
   transcriptions; the interest is in the interfaces:
   - mcata's algebra is called as `phi(recurse, layer)` where `recurse`
     is a callable `const Fix<F>& -> Result` (pass a lambda closing
     over phi).
   - mhisto's algebra is called as `phi(recurse, unroll, layer)` where
     `unroll` is `const Fix<F>& -> const F<Fix<F>>&` (i.e.
     `unwrap_fix`).
   Header comment: state plainly that C++ cannot enforce the rank-2
   abstraction (design §7.7's note) and that NO functor_typeclass
   instance is consulted.
2. **Tests** `mendler.t.cpp`:
   - Law: `mcata` with `phi(rec, l) = φ(layer_fmap(rec, l))` ≡
     `fold_fix(φ)` (§9), Nat and ExprF.
   - Behavior: ExprF eval via mcata written *without* fmap — the phi
     visits the layer and calls `recurse` on boxed children directly.
   - **No-instance proof**: define a test-local functor `OpaqueF` with
     NO functor_typeclass instance; evaluate something with mcata over
     it. (This is the motivating property — it must be a real test.)
   - Behavior: Fibonacci via mhisto (φ uses `unroll` to look two
     levels down; no Cofree anywhere), fib(0..10).
   - constexpr static_asserts for both.
3. **Example** `src/examples/mendler_eval.cpp` (§10): expression eval
   via mcata, comments contrasting it with the fmap-based `fold_fix`
   and noting the abstraction discipline (recurse is the *only* thing
   you may do with a child).

## Build
`make TOOLCHAIN=gcc-16 test`; run the example.

## Verify (gate)
Full suite green; example runs/exits 0; the OpaqueF test genuinely has
no instance (grep the test file — no `functor_typeclass` specialization
for it).

## Done when
Gate green; committed `[schemes] S10: mcata + mhisto`.

## Capture in handoff
Landed algebra calling conventions (argument order!); how the recurse
lambda was spelled so overload resolution stays clean; anything about
recursion depth in the constexpr tests.

## Pitfalls
- Do not be tempted to give `recurse`/`unroll` named types — plain
  lambdas keep mcata generic over algebra shapes.
- mhisto's unroll returns a reference into the tree — fine here
  (the tree outlives the fold), but say so in the header comment.
- fib-via-mhisto needs a two-level pattern match (Succ of Succ);
  use `overloaded` + `std::get_if` on the unrolled layer, and pin the
  base cases fib(0)=0, fib(1)=1 explicitly.
