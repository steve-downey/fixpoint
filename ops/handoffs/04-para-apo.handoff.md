# Handoff — S04 para + apo

- **Status:** DONE (gate passed)
- **Commit:** 7ed6eb6 — `[schemes] S04: para + apo`
- **Date / agent:** 2026-07-03 (session date rolled to 2026-07-04 mid-task; no bearing on the work), background execution agent

## What changed

- New `src/smd/fixpoint/para.hpp` (namespace `smd::fixpoint`):
  `template <class Result, template <class> class F, class Algebra>
  constexpr auto para(const Algebra &algebra, const Fix<F> &tree) -> Result`.
  Direct recursion, one `layer_fmap` per node building
  `std::pair<Fix<F>, Result>{child, para<Result>(algebra, child)}` at every
  child position — **order: original subtree first, fold result second**,
  exactly as the step file specifies (S12's `dist_para` and later steps
  must match this).
- New `src/smd/fixpoint/apo.hpp` (namespace `smd::fixpoint`):
  `template <template <class> class F, class Coalgebra, class Seed>
  constexpr auto apo(const Coalgebra &coalgebra, const Seed &seed) -> Fix<F>`.
  Coalgebra returns `F<either<Fix<F>, Seed>>` (D4: Left = finished
  subtree to graft, Right = keep unfolding). The worker is written with
  `smd::typeclass::match` over a **generic lambda** (`const auto &step`
  for the layer element, `const auto &subtree` / `const auto &next_seed`
  for the match branches) — this is what makes it equally accept
  `F<either<const Fix<F>&, Seed>>` coalgebras (zero-copy graft) with zero
  special-casing: `return subtree;` from a function returning `Fix<F>` by
  value triggers an ordinary copy-construction whether `subtree`'s
  deduced type came from a `Fix<F>` or a `const Fix<F>&` referent.
  Includes `<smd/typeclass/either.hpp>` per the step file.
- `src/smd/fixpoint/CMakeLists.txt`: added `para.hpp apo.hpp` to
  `smd_fixpoint_headers` FILE_SET; `para.t.cpp apo.t.cpp` to
  `smd_fixpoint_test` sources.
- New `src/smd/fixpoint/para.t.cpp` (5 tests): fold_fix-degeneracy law
  over Nat 0..10 and three IntLists (`{}`, `{1,2,3}`, `{5,4,3,2,1}}`);
  "tails" behavior test (algebra reifies each node's *original* tail back
  to a vector via `list_to_vector`, builds the full suffix list, asserted
  against the hand-built `{{1,2,3},{2,3},{3},{}}`); one `HeaderIsIdempotent`;
  one constexpr `static_assert` (Nat, small).
- New `src/smd/fixpoint/apo.t.cpp` (7 tests, incl. `HeaderIsIdempotent`):
  always-Right law ≡ `unfold_fix`/`nat_from_int` over int seeds 0..10;
  `Seed = Fix<F>` compile+behavior smoke using `either<Nat, Nat>` (the
  same-type-sides case D4 exists to support); sorted-insert behavior test
  (insert 5 into `[1,3,7,9]` → `[1,3,5,7,9]`, by-value graft coalgebra);
  a second test proving the **zero-copy graft** coalgebra
  (`either<const IntList&, IntList>`, `Left` bound via
  `make_left<IntList, const IntList &>(l)`) produces byte-for-byte the
  same `list_to_vector` result as the by-value one, plus front/back
  insertion edge cases; one constexpr `static_assert` (Nat, small, no
  graft branch — kept simple to avoid off-by-one arithmetic risk in a
  static_assert; graft/Left-branch behavior is fully covered by the
  runtime tests above per D10's "at least one static_assert" bar).
- `src/examples/para_pretty_print.cpp` (new): `para<std::string>` over
  `ExprF` (from `functors.hpp`) — Mul's algebra branch inspects each
  child's *original* subtree (`is_add(child.first)`, a local helper
  checking `std::holds_alternative<Add<Expr>>(unwrap_fix(e))`) to decide
  parenthesization; Add's branch never parenthesizes either child (lowest
  precedence here). Prints `(2*3)+4` as `2 * 3 + 4` and `2*(3+4)` as
  `2 * (3 + 4)`.
- `src/examples/apo_sorted_insert.cpp` (new): by-value-graft insert
  coalgebra (same shape as the test's `make_insert_coalgebra`), prints
  before/after vectors: `before: [1, 3, 7, 9]` /
  `after inserting 5: [1, 3, 5, 7, 9]`.
- `src/examples/CMakeLists.txt`: added executable+install blocks for
  `para_pretty_print` and `apo_sorted_insert`, copied verbatim from the
  `fixpoint_tree_example` pattern.

## Verification evidence

- `make TOOLCHAIN=gcc-16 test`: configure+build+ctest clean — **100%
  tests passed, 0 tests failed out of 107** (up from S03's 98; net +9).
  Breakdown from `ctest -N`: 4 para tests (`#7` tails, `#9` law-IntList,
  `#16` HeaderIsIdempotent, `#27` law-Nat — the constexpr static_assert
  is a compile-time check, not a discovered ctest case) and 5 apo tests
  (`#10` Seed=Fix<F>, `#11` HeaderIsIdempotent, `#26` law-always-Right,
  `#34` zero-copy-graft, `#38` sorted-insert-behavior), for 9 new
  discovered tests total — matching 107 − 98 = 9 exactly.
- Explicit rebuild of just the two new test files and both new example
  binaries with `grep -i "warning\|error"` over the build output: empty
  (no compiler warnings).
- `.build/build-gcc-16/src/examples/Asan/para_pretty_print` prints:
  ```
  2 * 3 + 4
  2 * (3 + 4)
  ```
  and exits 0.
- `.build/build-gcc-16/src/examples/Asan/apo_sorted_insert` prints:
  ```
  before: [1, 3, 7, 9]
  after inserting 5: [1, 3, 5, 7, 9]
  ```
  and exits 0.
- Both `static_assert`s (`para_constexpr_smoke`, `apo_constexpr_smoke`)
  compile clean under gcc-16/C++26 — no `either`/constexpr friction
  beyond what S03 already proved; `match`'s generic-lambda branches and
  `apo`'s recursive `constexpr` call nest fine at compile time.

## Deviations from the plan / design

None. No row added to `ops/DEVIATIONS.md`. The step file, S02's handoff,
and S03's handoff were all followed as authoritative with no conflicts
encountered; S03's worked zero-copy-graft guidance (exact `match`
signature, `left(e)`/referent copy-on-return behavior, `make_left<R, L>`
explicit-argument order) transferred directly into `apo.hpp` and
`apo.t.cpp` with no adjustment needed.

## Discoveries affecting later steps

- **`match`'s branch lambdas can be fully generic (`const auto &`) and
  still let the compiler pick the right overload per instantiation** —
  `apo`'s worker never names `Fix<F>` or `const Fix<F>&` inside the
  lambda bodies; it only declares the *return* type (`-> Fix<F>`), which
  is what forces the copy-construction on the Left/graft path regardless
  of which `either` instantiation the coalgebra used. This generalizes:
  any later scheme worker that wants to be graft-agnostic (gana in S14
  is the obvious next candidate, per design §7.10/§7.14's dist_gapo
  reference to this same mechanism) should reach for the same pattern —
  generic lambda body, concrete return type — rather than templating the
  worker itself over the `either`'s exact type parameters.
- **`Box::operator->()` composes cleanly through nested carriers.** Both
  `para.t.cpp`'s tails test and `para_pretty_print.cpp`'s algebra do
  `a.left->second` / `(*m.left)` directly on a `Box<std::pair<Fix<F>,
  Result>>` — no need to dereference-then-dot, confirming S02's
  functors.hpp discovery extends unchanged to the `std::pair`-carrying
  case S05's zygo/mutu will also use (S05's carrier is
  `std::pair<Helper, Result>`, same boxed-pair shape).
  Concretely, `layer_fmap`'s lambda in `para` returns
  `std::pair<Fix<F>, Result>` **by value**, and the surrounding
  `functor_typeclass::fmap` implementation boxes it
  (`make_box<B>(std::invoke(fn, *child))` in each `NatF`/`ListF`/etc.
  instance) — so the algebra sees `Box<std::pair<Fix<F>, Result>>` at
  each recursive position and reaches in with `->first`/`->second` (or
  `(*box).first`/`.second`), exactly the access pattern S05's zygo
  algebra (`std::pair<Helper, Result>` carrier, "helper is `.first`, main
  is `.second`") will need too.
- **Building a `std::vector<std::vector<int>>` (or any nested
  standard container) as a fold carrier is unproblematic in
  `constexpr`/runtime alike** — no workaround needed for the "tails"
  test's `std::vector<std::vector<int>>` algebra carrier, consistent with
  S02's earlier confirmation that `constexpr std::vector` + `Box`
  transient allocation is fine under gcc-16.
- Confirmed (again) that explicit-template-argument order matters and
  is exactly as S03 documented: `make_left<R, L>(L v)` (result side
  named first) and its reference-side use
  `make_left<IntList, const IntList &>(l)` — used identically in both
  `apo.t.cpp` and `apo_sorted_insert.cpp`'s coalgebras, always compiling
  on first try once the S03 forward note was followed literally.

## Forward notes for the NEXT step (S05 — zygo + mutu)

- **S05 depends only on S02**, not S04 — no file overlap with
  `para.hpp`/`apo.hpp`/their tests. `smd/fixpoint/CMakeLists.txt` and
  `src/examples/CMakeLists.txt` are the only two files S04 touched that
  S05 will also touch (adding its own new entries) — expect a purely
  additive diff to both, same pattern used here (append to the `FILES`/
  `PRIVATE` lists, append executable+install blocks).
- **The `std::pair<Helper, Result>` carrier S05's `zygo` step asks for
  is structurally the same "boxed pair inside a layer" shape `para`
  already exercises** (see Discoveries above): expect
  `Box<std::pair<Helper, Result>>` at each recursive position, accessed
  via `->first`/`->second` or `(*box).first`/`.second` — the exact same
  idiom, no new technique required. The *convention* differs though:
  S05's step file says **helper is `.first`, main is `.second`** for
  zygo's carrier — the opposite slot assignment from D11's general
  "non-value slot first" framing read literally, but consistent with
  D11 itself (helper is the "extra" accumulator, main is the "value" —
  matches pair's env-comonad convention `pair<B, ValueSlot>`). Do not
  confuse this with para's `std::pair<Fix<F>, Result>` (original-subtree
  first, fold-result second) — the two pairs serve different purposes
  and the step file is explicit that zygo's order is helper-first,
  main-second; follow the step file, not an analogy to para's pair.
- **`layer_fmap` composes without issue for a "double fmap"
  construction** (zygo's step-file pitfall: `layer_fmap(.first, x)` runs
  a second fmap over an already-`layer_fmap`-produced layer). Nothing in
  S04 exercised a double-fmap directly, but `apo`'s worker does chain two
  `layer_fmap`-adjacent operations per recursive step (the coalgebra call
  producing a layer, then `layer_fmap` over it) with no aliasing/lifetime
  surprises — the general pattern of "call `layer_fmap` on a value
  produced by another `layer_fmap`-shaped computation in the same
  expression" is unproblematic under gcc-16/C++26 constexpr.
- **Even/odd on Nat via `mutu`** (S05's second example) can reuse
  `functors.hpp`'s existing `Nat`/`NatF`/`Zero`/`Succ`/`nat_from_int`
  fixture directly, the same way `para.t.cpp`/`apo.t.cpp` did — no new
  Nat-shaped scaffolding needed.
- Gate remains `make TOOLCHAIN=gcc-16 test` fully green; S05 additionally
  requires two new example binaries (`zygo_balanced`, `mutu_even_odd`) to
  run and exit 0 with visibly-correct output.

## Open risks / TODOs

- gcc-17 secondary smoke was not re-run this step (advisory only per
  design D9/plan); still outstanding since S00, not blocking.
- None specific to S04's own scope were left open — gate is green, every
  bullet in the step file's "Tests"/"Examples" sections has a
  corresponding test or example, and both example binaries were run and
  their output pasted above (not just claimed).
