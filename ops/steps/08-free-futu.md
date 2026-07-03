# S08 — Free monad and futu

**Goal.** The Free monad type with its monad_typeclass instance, and the
futumorphism built on it, with the run-length-decode example.

**Depends on:** S02, S03.
**Design refs:** §5.4, §6.4, §7.5 (futu half), §9, §10.

## Do
1. **`src/smd/fixpoint/free.hpp`** — `Free<F, A>` per design §5.4:
   `std::variant<A, F<Free<F,A>>> node` with `pure_free`/`roll_free`
   constructors, `is_pure`, and visitation left to std::visit +
   `overloaded`. Instances (reopening `namespace smd::typeclass`):
   - `functor_typeclass<Free<F, A>>` — maps the Pure value; recurses
     through Roll layers.
   - `monad_typeclass<Free<F, A>>` — pure = `pure_free`; bind: Pure a →
     `k(a)`; Roll layer → `roll_free(layer_fmap(recursive-bind, layer))`.
2. **`src/smd/fixpoint/futu.hpp`** — `futu<F>` per design §7.5 with the
   worker: `worker(Free): Pure s → futu(ψ, s); Roll layer →
   wrap_fix(layer_fmap(worker, layer))`. constexpr.
3. **Tests** `free.t.cpp`, `futu.t.cpp`:
   - Free: monad laws by example (left/right identity, associativity
     spot-check) on `Free<IntListF, int>`; bind through a Roll chunk.
   - Law: futu whose coalgebra emits exactly one layer
     (`layer_fmap(pure_free, ψ(s))`) ≡ `unfold_fix(ψ)` (§9), 0..10.
   - Behavior: run-length decode via futu on IntList — seed is an index
     into `{{2,7},{3,1}}`-style (count, value) pairs; one step emits
     `count` Cons layers as a Free chunk with the next-seed Pure at the
     bottom. Decoded [7,7,1,1,1].
   - Behavior: pairwise-swap stream head — futu emitting two layers per
     step to swap adjacent elements of a generated list.
   - constexpr static_assert (small decode).
4. **Example** `src/examples/futu_rle_decode.cpp` (§10): decode a
   couple of RLE inputs, printing input pairs and decoded vectors, the
   multi-layer emission commented.

## Build
`make TOOLCHAIN=gcc-16 test`; run the example.

## Verify (gate)
Full suite green; example runs/exits 0 with correct decodes.

## Done when
Gate green; committed `[schemes] S08: Free + futu`.

## Capture in handoff
Landed Free shape + constructor names (S09/S12 build the `unroll`
worker and `dist_futu` on them); how you built multi-layer chunks
ergonomically in tests (helper?) — S09's codyna/chrono tests will want
the same trick.

## Pitfalls
- Building an n-deep Free chunk is a loop of `roll_free(Cons{value,
  box(inner)})` — write a small test-local helper; keep it out of the
  public header.
- `monad_typeclass<Free<F,A>>` bind's result type changes the parameter
  (`Free<F,B>`); make the recursive lambda a named function template or
  the deduction cycle bites (same trick as everywhere: explicit return
  type).
- futu's worker and Free's bind look similar; do not implement futu
  *via* bind — the direct worker is simpler and constexpr-friendlier.
