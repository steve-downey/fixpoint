# S04 — para and apo

**Goal.** Paramorphism and apomorphism, with their equivalence laws and
two runnable examples.

**Depends on:** S02, S03 (either).
**Design refs:** §5.2, §7.2, §3 D4/D5/D10, §9, §10.

## Do
1. **`src/smd/fixpoint/para.hpp`** — `para<Result>` per design §7.2:
   direct recursion, one `layer_fmap` per node mapping each child to
   `std::pair<Fix<F>, Result>{child, para(child)}`. constexpr.
2. **`src/smd/fixpoint/apo.hpp`** — `apo<F>` per design §7.2: coalgebra
   returns `F<either<Fix<F>, Seed>>` (D4: Left = finished subtree,
   Right = keep unfolding); write the worker with `match` (§5.2): Left
   → return the embedded subtree by copy (D6), Right → recurse. Written
   that way the worker also accepts `F<either<const Fix<F>&, Seed>>`
   coalgebras (zero-copy graft, §7.2) — don't special-case it, just
   don't preclude it. Include `<smd/typeclass/either.hpp>`. constexpr.
3. **Tests** `para.t.cpp`, `apo.t.cpp` (wire into `smd_fixpoint_test`):
   - Law: `para` with an algebra that projects `.second` at children ≡
     `fold_fix` with the plain algebra, over Nat 0..10 and a couple of
     IntLists (§9).
   - Law: `apo` whose coalgebra always returns Right ≡
     `unfold_fix`, over int seeds 0..10.
   - An `apo` with `Seed = Fix<F>` compiles and behaves (the
     `either<Fix<F>, Fix<F>>` same-type case — the scenario that ruled
     out `std::expected`; even a trivial identity-ish unfold suffices).
   - The zero-copy graft variant: the sorted-insert coalgebra rewritten
     to return `F<either<const Fix<F>&, Seed>>` (reference Left into
     the original list), same observable result as the by-value
     version.
   - Behavior: para "tails" on IntList — algebra collects, at each Cons,
     the *original* tail as a vector; assert against hand-built answer.
   - Behavior: apo sorted-insert on IntList (insert 5 into
     [1,3,7,9]; the coalgebra short-circuits with the untouched
     original tail once 5 < head) — assert `list_to_vector` result and
     assert (indirectly, by construction) that positions after the
     insertion point came from the graft: e.g. insert into a list built
     once and compare full contents.
   - constexpr static_assert for each (small Nat/list).
4. **Examples** `src/examples/para_pretty_print.cpp` and
   `src/examples/apo_sorted_insert.cpp` (§10): pretty-printer uses
   `para<std::string>` over ExprF, consulting the original child to
   decide whether it needs parentheses (Add under Mul does; Mul under
   Add doesn't); prints the minimal-parens rendering of a couple of
   expressions with `std::println`. Sorted-insert prints before/after
   vectors. Both ≤ ~120 lines, comment-heavy, exit 0. Add
   executable+install blocks to `src/examples/CMakeLists.txt` following
   the `fixpoint_tree_example` pattern.

## Build
`make TOOLCHAIN=gcc-16 test`; run both example binaries from
`.build/build-gcc-16/src/examples/Asan/`.

## Verify (gate)
- Full suite green; both examples run, exit 0, output visibly correct
  (paste it into the handoff).

## Done when
Gate green; committed `[schemes] S04: para + apo`.

## Capture in handoff
Landed signatures; how the pair is built (order: original first,
result second — later steps and dist_para assume this); any either +
constexpr friction under gcc-16.

## Pitfalls
- Algebra argument type is `F<std::pair<Fix<F>, Result>>` — the pair is
  *inside* the functor (inside `Box` for boxed positions); tests should
  destructure via `overloaded` visitation like the existing fixtures.
- For apo's Left case return the grafted subtree *by value*
  (copy out of `left(e)`) — do not try to move out of a `const&`
  coalgebra result.
- Keep the example printers free of `<format>` pitfalls: `std::println`
  with plain strings is enough.
