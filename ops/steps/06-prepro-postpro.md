# S06 — hoist, prepro, postpro

**Goal.** Natural-transformation plumbing: `hoist` (retag every layer),
Fokkinga's prepromorphism (transform layers on the way down a fold) and
postpromorphism (transform layers on the way out of an unfold).

**Depends on:** S02.
**Design refs:** §4 (natural transformations), §7.4, §9, §10.

## Do
1. **`src/smd/fixpoint/prepro.hpp`** containing all three:
   - `hoist<G>(e, tree)` per design §7.4 — `fold_fix` with algebra
     `wrap_fix<G> ∘ e`. The endo case is `hoist<F>(e, t)`.
   - `prepro<Result>(e, algebra, tree)` — the cumulative equation from
     §7.4: `φ(layer_fmap(λc. prepro(e, φ, hoist<F>(e, c)), unfix(t)))`.
     Document the cumulative-application cost in the header comment.
   - `postpro<F>(e, coalgebra, seed)` — dual:
     `wrap_fix(layer_fmap(λs. hoist<F>(e, postpro<F>(e, ψ, s)), ψ(seed)))`.
   All constexpr. A natural transformation here is any object callable
   as `e(F<X>) -> F<X>` (or `-> G<X>` for hoist) for every X — document
   with a code-comment example (a struct with a templated call
   operator, design §4).
2. **Tests** `prepro.t.cpp`:
   - hoist with the identity transformation ≡ original tree (compare
     via a fold, or `==` if Fix layers compare — they do for the
     fixture functors since variant/Box provide ==).
   - hoist to a *different* functor: NatF → IntListF (Succ→Cons(1,·),
     Zero→Nil) then sum ≡ nat_to_int.
   - Law: `prepro(identity_nat, φ)` ≡ `fold_fix(φ)`;
     `postpro(identity_nat, ψ)` ≡ `unfold_fix(ψ)` (§9), 0..10.
   - Behavior: take-while-positive sum via prepro on IntList — the
     transformation rewrites `Cons(x, rest)` with `x < 0` to `Nil`;
     sum [3,4,-1,5] == 7.
   - Behavior: postpro that caps values (`Cons(x,·)` → `Cons(min(x,3),·)`)
     applied below an unfold; assert resulting vector.
   - constexpr static_asserts for hoist and prepro.
3. **Example** `src/examples/prepro_takewhile_sum.cpp` (§10), standard
   wiring.

## Build
`make TOOLCHAIN=gcc-16 test`; run the example.

## Verify (gate)
Full suite green; example runs/exits 0 with correct printed sums.

## Done when
Gate green; committed `[schemes] S06: hoist + prepro + postpro`.

## Capture in handoff
Landed signatures (S15's gprepro references hoist and the natural
transformation shape); the identity_nat spelling used in tests (S15
reuses it); measured/observed cost note if the cumulative hoisting was
noticeable (it shouldn't be at test sizes).

## Pitfalls
- `hoist<G>` — G is the *first* explicit template parameter so the endo
  case reads `hoist<F>(e, t)`; keep F deducible from the tree.
- prepro applies `e` to the *children before recursing*, via
  `hoist` (whole-subtree), not just to the top layer of each child —
  transcribe the equation exactly; the identity law won't catch
  a wrong-depth version, but the take-while test will (a wrong
  version stops too early or too late on [3,-1,4]-shaped inputs; add
  that case).
- The natural transformation must be applied to layers of *any* carrier
  X (during prepro it runs at `F<Fix<F>>`), so fixture transformations
  in tests must have templated call operators — no lambdas with
  concrete layer parameter types.
