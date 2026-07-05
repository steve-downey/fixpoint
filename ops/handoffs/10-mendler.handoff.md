# Handoff — S10 Mendler mcata and mhisto

- **Status:** DONE (gate passed)
- **Commit:** 0893792 — `[schemes] S10: mcata + mhisto`
- **Date / agent:** 2026-07-04, background execution agent

## What changed

- New `src/smd/fixpoint/mendler.hpp` (namespace `smd::fixpoint`, no
  `smd::typeclass` reopening — no typeclass instance is defined or
  consulted anywhere in this header, which is the whole point of the
  step): direct three-line transcriptions of design §7.7's equations.
  - `mcata<Result, F>(phi, tree) -> Result`: `phi` is called as
    `phi(recurse, layer)` — `recurse` is a plain lambda closing over
    `phi` (`const Fix<F>& -> Result`, `mcata Φ` partially applied),
    `layer` is `const F<Fix<F>>&` from `unwrap_fix(tree)`. Body is
    exactly `phi(recurse, unwrap_fix(tree))`.
  - `mhisto<Result, F>(phi, tree) -> Result`: `phi` is called as
    `phi(recurse, unroll, layer)` — `unroll` is a second plain lambda,
    `const Fix<F>& -> const F<Fix<F>>&`, literally `unwrap_fix` wrapped
    (documented as returning a reference *into* the tree, valid because
    the tree the fold started from outlives the whole call — same
    caveat mhisto's own step file flagged). Body is
    `phi(recurse, unroll, unwrap_fix(tree))`.
  - Header comment states plainly (design §7.7's note): C++ has no
    rank-2 polymorphism, so nothing stops an algebra from reaching into
    a child `Fix<F>` directly instead of going through `recurse`/
    `unroll` — the "recurse/unroll is the only thing you may do with a
    child" discipline is convention only, not enforced by the type
    system. That's the point `mendler_eval.cpp`'s comments make
    concrete.
- New `src/smd/fixpoint/mendler.t.cpp` (6 tests): `HeaderIsIdempotent`;
  the §9 degeneracy law (`mcata(λ(rec,l). φ(layer_fmap(rec,l))) ≡
  fold_fix(φ)`) checked on **both** Nat (0..10) and ExprF
  (`(2*3)+4`), per the step file's explicit ask for both fixtures;
  ExprF eval via mcata written *without* fmap (the algebra
  pattern-matches `Add`/`Mul` and calls `recurse` directly on each
  boxed child); the **no-instance proof** — a test-local `OpaqueF`
  functor family (`OpaqueZero<A>`/`OpaqueSucc<A>`, both with a
  defaulted `friend operator==` for consistency with the rest of the
  tree, though `==` is never exercised here) with **no
  `functor_typeclass<OpaqueF<...>>` specialization anywhere in the
  file** — a `Fix<OpaqueF>` tree is built by hand with `wrap_fix`/
  `make_box` (deliberately *not* `unfold_fix`, which would itself
  require a functor instance to fmap through and would silently defeat
  the proof), then folded with `mcata` to count its depth; Fibonacci
  via mhisto on Nat (0..10, fib(0)=0/fib(1)=1 pinned explicitly as the
  step file's pitfall asked, two-level `Succ`-of-`Succ` pattern match
  via nested `std::visit`/`overloaded`, no `Cofree` anywhere); one
  `static_assert` per scheme (`mcata_constexpr_smoke`,
  `mhisto_constexpr_smoke`), each with its own fully self-contained
  local lambda algebra (house pattern from `histo.t.cpp`, required here
  too — see Discoveries).
- `src/smd/fixpoint/CMakeLists.txt` / `src/examples/CMakeLists.txt`:
  append-only additions (`mendler.hpp` / `mendler.t.cpp` to the
  FILE_SET/test sources; one executable+install block for
  `mendler_eval`), same two-file pattern every prior step touched.
- New `src/examples/mendler_eval.cpp`: `(2*3)+4` evaluated via `mcata`,
  header comment contrasting it with `functors.hpp`'s fmap-based
  `eval` and stating the recurse-only abstraction discipline
  explicitly.

## Verification evidence

- `make TOOLCHAIN=gcc-16 test`: **100% tests passed, 0 tests failed
  out of 157** (baseline going into this step was **151**, confirmed
  via `ops/PLAN.md`'s S09 row and `git log` — no commits landed
  between S09's handoff and this step's start; net for S10 is +6,
  matching the 6 new `TEST_CASE`s in `mendler.t.cpp` exactly —
  HeaderIsIdempotent + 2 law variants + 1 no-fmap behavior + 1
  no-instance proof + 1 mhisto-fib behavior).
- `.build/build-gcc-16/src/examples/Asan/mendler_eval` output (exit 0):
  `mcata eval: 10` — matches `(2*3)+4`.
- Both `static_assert`s (`mcata_constexpr_smoke`,
  `mhisto_constexpr_smoke`) compile clean under gcc-16/C++26.
- **DEV-01 mutation check, run and reverted before committing**
  (following the S09 handoff's technique, applied here to the property
  this whole step is built around): temporarily added
  `#include <smd/fixpoint/fmap.hpp>` to `mendler.hpp` and inserted a
  spurious `(void)layer_fmap([](const Fix<F>& child){ return child; },
  unwrap_fix(tree));` at the top of `mcata`'s body — simulating a
  plausible regression back to fmap-based dispatch while leaving the
  algebra-calling convention untouched. Rebuilding
  (`make TOOLCHAIN=gcc-16 compile`) **failed exactly at the
  `mcata<int, OpaqueF>` call in the no-instance-proof test**:
  `fmap.hpp:24: error: 'const struct std::integral_constant<bool,
  false>' has no member named 'fmap'`, i.e. the `functor_typeclass`
  static_assert-style diagnostic firing because `OpaqueF` genuinely has
  no instance. This confirms the OpaqueF test is a real discriminator
  for "mcata stopped needing a functor instance" — it is not passing
  for an accidental reason (e.g. some other header supplying an
  instance for `OpaqueF` nobody noticed). Reverted via
  `cp /tmp/mendler.hpp.bak mendler.hpp`, diffed against the original to
  confirm an exact match, then re-ran the full gate
  (`make TOOLCHAIN=gcc-16 test`) green at 157/157 before committing.

## Deviations from the plan / design

None. No `ops/DEVIATIONS.md` row filed — the design §7.7 equations
transcribed directly with no equation-vs-law conflict.

## Discoveries affecting later steps

- **A function *template* (declared via an abbreviated-template
  parameter, e.g. `auto f(auto recurse, const Layer&) -> int`) cannot
  be passed as a scheme's algebra argument — template argument
  deduction for the scheme's own `MAlgebra`/`Algebra` parameter fails
  with "couldn't deduce template parameter ... unresolved overloaded
  function type"**, because the free function's name denotes an
  overload set (of one, but still unresolved) rather than a single
  callable type. This bit `mcata`/`mhisto`'s algebras in this step
  three times before being caught (`mendler_eval_algebra`,
  `opaque_count_algebra`, `mhisto_fib_algebra` — anywhere the algebra
  itself needed a generic/`auto` `recurse`/`unroll` parameter). **Fix:
  declare the algebra as a lambda bound to a variable**
  (`auto f = [](auto recurse, const Layer&) -> int {...};`), never as
  a free function with an `auto` parameter — a lambda is a single
  concrete callable type even though its `operator()` is itself
  templated, so it deduces cleanly. This is a new discovery, distinct
  from S07/S08's deduced-return-type quirk (that one was about
  self-recursive CRTP methods; this one is about passing a callable by
  name into a template parameter at all). **Any future step whose
  algebra needs a generic parameter (elgot/coelgot's `phi`/`psi` won't,
  per their step file's plain `Result`/`Seed` signatures, but a later
  Mendler-flavored or higher-order scheme might) should bind the
  algebra to a lambda variable from the start, not a free function.**
- **A free function (even a non-template one) cannot be called from a
  `static_assert`/constexpr context unless it is itself declared
  `constexpr`.** `mcata_constexpr_smoke`/`mhisto_constexpr_smoke`
  initially tried to reuse the file's own free-function algebras
  (`count_algebra`, `mhisto_fib_algebra`) the way the runtime behavior/
  law tests do, and GCC 16 rejected it ("call to non-'constexpr'
  function"). **Fix: exactly the `histo.t.cpp` house pattern already
  established (S07) — each `*_constexpr_smoke` helper builds its own
  fully self-contained local lambda algebra from scratch**, never
  reusing a `.t.cpp`'s free-function algebra even if the free function
  looks constexpr-capable at a glance. Confirmed twice in this step
  (both mcata's and mhisto's smoke helpers needed the fix).
- **The no-instance proof only requires the scheme header itself to
  avoid `#include`-ing `fmap.hpp`/never calling `layer_fmap`** — there
  is no way to statically forbid a functor_typeclass lookup other than
  "don't write the call". The mutation test above is the way to gain
  confidence the property is real rather than assumed: temporarily
  reintroduce a `layer_fmap` call, confirm the OpaqueF test stops
  compiling, revert. Worth reusing this exact technique (mutate the
  scheme header to add a spurious functor-typeclass-forcing call, not
  the test) for any future step whose central claim is "does not need
  X" rather than "computes the right answer" — DEV-01's discriminator
  logic is about numeric answers, but the same falsifiability
  discipline applies to compile-time non-dependency claims.
- **`OpaqueF`'s `Fix<OpaqueF>` tree must be built with `wrap_fix`/
  `make_box` directly, never `unfold_fix`** — `unfold_fix` (both the
  explicit-fmap and lookup overloads in `recursion_schemes.hpp`) itself
  calls `layer_fmap`/`functor_typeclass` internally, so building the
  no-instance fixture via `unfold_fix` would silently require exactly
  the instance the test is trying to prove unnecessary. A small
  hand-written recursive `make_opaque(int) -> OpaqueNat` constexpr
  function (mirroring `nat_from_int`'s shape but via direct
  `wrap_fix`/`make_box` calls instead of `unfold_fix`) is the pattern
  to reach for if a later step ever needs another instance-free fixture.

## Forward notes for the NEXT step (S11 — elgot + coelgot)

- **S11 depends on S02 and S03 (either), not S10** — no file overlap
  with `mendler.hpp`/`mendler.t.cpp`. `src/smd/fixpoint/CMakeLists.txt`
  and `src/examples/CMakeLists.txt` are again the only two files this
  step touched that S11 will also touch (append `elgot.hpp` /
  `elgot.t.cpp` to the FILE_SET/test sources, add one
  executable+install block for `elgot_shortcircuit`).
- **elgot/coelgot are back to ordinary `layer_fmap`/functor-typeclass-
  based schemes** (design §7.8's own transcriptions use
  `layer_fmap(elgot..., right(e))` / `layer_fmap(coelgot..., ψ(seed))`)
  — this step's "no functor instance" property and its
  abbreviated-template-parameter pitfall are specific to Mendler-style
  algebras (`recurse`/`unroll` as explicit callable arguments) and do
  **not** apply to S11. `elgot`'s/`coelgot`'s own algebra/coalgebra
  parameters (`Algebra`, `Coalgebra`) are plain, concrete-typed
  template parameters exactly like every S01–S09 scheme (`para`,
  `zygo`, `dyna`, etc.) — deduction from a lambda variable or a
  ordinary (non-generic-parameter) free function both work fine there,
  no special handling needed.
- **The GCC 16 deduced-auto self-recursion quirk (S07/S08) still only
  bites CRTP `Impl` typeclass-instance methods with a return type
  deduced from the algebra's `invoke_result_t`** — `elgot`/`coelgot`
  are free function templates with an explicit `Result` return type
  (per their step file's own signatures), so per every step since S08
  that confirmed this, expect no workaround needed here either.
- **D4's either orientation (Left = stop/short-circuit, Right =
  continue) is already implemented and tested** in
  `src/smd/typeclass/either.hpp` (S03) — `elgot`'s coalgebra returns
  `either<Result, F<Seed>>` per the design snippet already in
  §7.8/the step file; nothing in this step (S10) touched `either.hpp`
  or its instances, so it's exactly as S03 left it. Use `match`/
  `fanin` over hand-rolled `is_left` branching per §5.2's own guidance
  (not exercised by S10, but confirmed still current by inspection).
- **The step file's own Pitfall about not swapping the either
  orientation** ("the compiler won't catch a flip when Seed and Result
  are both int; the invocation-counting test will") is exactly the
  same falsifiability discipline this step's mutation-test technique
  demonstrates — worth actually running that mutation (flip
  `make_left`/`make_right` in the short-circuit coalgebra, confirm the
  invocation-counting test fails, revert) rather than trusting the
  test's power by inspection, the same way this handoff did for the
  OpaqueF no-instance proof above.
- Gate remains `make TOOLCHAIN=gcc-16 test` fully green; S11
  additionally requires the new `elgot_shortcircuit` example binary to
  run, exit 0, and demonstrably print an examined-count strictly less
  than the input list's length (per its own step file's Verify gate).

## Open risks / TODOs

- gcc-17 secondary smoke was not re-run this step (advisory only per
  design D9/plan); still outstanding since S00, not blocking.
- None specific to S10's own scope were left open — gate is green,
  every bullet in the step file's "Tests"/"Example" sections has a
  corresponding test or example, the example binary's output was run
  and pasted above (not just claimed), and the no-instance proof's
  discriminating power was empirically mutation-tested (spurious
  `layer_fmap` call introduced, compile failure observed at the exact
  OpaqueF call site, revert diffed to confirm exact restoration)
  rather than just asserted in prose.
