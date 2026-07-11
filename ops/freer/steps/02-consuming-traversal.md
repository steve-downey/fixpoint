# S02 — Consuming traversal: fmap.hpp + free.hpp + box.hpp

**Goal.** Give the existing machinery a consuming (rvalue) path that
a move-only, lazily-mapped layer can compile against, without
touching the const paths pure-data functors use. This is FD4 made
real, including the capture-ownership rule.

**Depends on:** S00 (parallel with S01 — different files).
**Design refs:** FD4 (all of it, especially the capture-ownership
rule and the code-audit addenda), FD6 (for why laziness changes the
rules).

## Do

1. **`src/smd/fixpoint/box.hpp`** — add
   `constexpr auto operator*() && -> A&& { return std::move(*ptr); }`
   (keep the existing const overload untouched). Test: move out of a
   Box rvalue; Box of move-only type round-trips.
2. **`src/smd/fixpoint/fmap.hpp`** — consuming overloads of
   `layer_fmap` for all three lookup modes, taking the layer by
   rvalue (`Layer&&` constrained to rvalues, e.g.
   `requires (!std::is_lvalue_reference_v<Layer>)`, so the existing
   `const Layer&` overloads keep winning for lvalues) and forwarding
   `std::move(layer)` into `Typeclass.fmap`. Mirror
   `functor_instance_for` with an rvalue variant for mode 3's
   constraint. A pure-data instance with only a `const&` fmap must
   still be reachable through the new overload (rvalue binds to
   const&) — that fallthrough is a test.
3. **`src/smd/fixpoint/free.hpp`** — consuming overloads in
   `FreeFunctorImpl::fmap` and `FreeMonadImpl::bind` taking
   `smd::fixpoint::Free<F, X>&&`:
   - visit `std::move(m.node)`;
   - result types computed from `std::invoke_result_t<Fn, X&&>`
     (explicit trailing return types — same deduction-cycle reasoning
     as the existing const overloads' comments);
   - **capture-ownership rule (FD4): the closures handed to
     layer_fmap own their captures** — `fn` by value/move, `self` by
     value (the Impl objects are stateless). No `[&self, &fn]` on the
     consuming path, ever, even though the pure-data instances would
     tolerate it: the Coyoneda layer in S03 will *store* these
     closures.
   - `pure` needs no new overload (already by-value).
4. **Tests** (`free.t.cpp` additions + `box.t.cpp` addition +
   `fmap.t.cpp` additions):
   - No-regression: the const-path suite untouched and green.
   - Move-only smoke: `Free<IntListF, MoveOnlyInt>` (test-local
     move-only wrapper) — consuming fmap and bind through a Roll
     chunk produce the expected values (assert via is_pure +
     std::get, not ==, if the payload kills comparability).
   - **Deferred-invocation lifetime test (Asan gate for the rule):** a
     test-local *lazy* layer functor — one stored
     `std::move_only_function<Box<X>(int) &&>`-style callable whose
     instance's fmap post-composes WITHOUT invoking (a miniature of
     S03's real layer, kept local to the test) — then: call consuming
     bind, let every caller-scope object die, THEN invoke the stored
     continuation and CHECK the result. Under the default Asan
     config this test is what catches a reference capture that slips
     back in. Name it so it's findable:
     `"consuming bind: continuation owns its captures (FD4)"`.
   - Rvalue layer_fmap mode-3 (explicit object) and mode-2 (NTTP pin)
     smoke tests, mirroring the existing const-mode tests in
     fmap.t.cpp.

## Build

`make TOOLCHAIN=<gcc-pin> test` and `make TOOLCHAIN=<clang-pin> test`.

## Verify (gate)

Full suite green on both pins (no pre-existing test edited); the
lifetime test passes under Asan (the default CONFIG — just `make
test`); `make lint` clean.

## Done when

Gate green; committed `[freer] S02: consuming traversal` + handoff.

## Capture in handoff

Exact signatures of every new overload as landed (S03 compiles
against them); how you constrained rvalue-vs-lvalue overload
selection (S03's instance must present matching fmap shapes); the
lazy-layer test pattern (S03 promotes it to the real thing).

## Pitfalls

- A forwarding-reference `Layer&&` overload UNconstrained would
  swallow lvalue calls that today pick `const Layer&`; the rvalue
  constraint is load-bearing. Verify with a compile test that lvalue
  calls still select the const overload.
- Do NOT change `monad.hpp`'s derived ops (apply/kleisli): their
  by-reference captures are a *recorded boundary* (FD4: derived
  Monad ops are off-limits for Freer), not this step's bug to fix.
  If you find yourself editing monad.hpp, stop and reread FD4.
- `std::invoke_result_t<Fn, X&&>` vs `Fn, const X&`: the consuming
  and const overloads may deduce different result types for the same
  Fn; that is fine and expected — don't unify them.
- Implicit move on return of forwarding-reference-typed locals is
  C++23 (P2266) — this repo is C++26, rely on it, but if a pinned
  compiler complains, `std::move` explicitly and record.
