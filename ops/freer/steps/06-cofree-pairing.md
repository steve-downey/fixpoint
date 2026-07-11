# S06 — Cofree pairing: unfold_cofree, the lazy co-signature, the scripted mock

**Goal.** The paper's central demonstration (FD8): a deterministic
scripted mock as a Cofree value over the co-signature, built with a
new Cofree-targeted unfold, run against a Freer program by executing
the Free/Cofree pairing. Gate: FD10's failing-twice Network script
drives the retry program synchronously.

**Depends on:** S05. (Parallel with S07d/S07 — separate worktree,
branch `freer/cofree-pairing`.)
**Design refs:** FD8 (incl. the 2026-07-09 correction and the
strict-Cofree/lazy-co-signature paragraphs), FD5, FD10, FD4
(capture-ownership — it binds here too).

## Do

1. **`src/smd/fixpoint/unfold_cofree.hpp`** (new, small): the Cofree
   anamorphism the audit found missing —
   `unfold_cofree<F>(head_fn, coalgebra, seed) -> Cofree<F, B>`
   (`head_fn: S -> B`, `coalgebra: S -> F<S>`), via layer_fmap over
   the coalgebra's layer, mirroring `unfold_fix`'s shape
   (recursion_schemes.hpp). Works for ANY registered functor — test
   it once on a pure-data functor (NatF or IntListF) so the scheme
   stands alone; note in the header comment that over an EAGER
   functor it must reach a terminal layer or it diverges (FD8).
2. **`src/smd/fixpoint/freer_cosignature.hpp`** (new):
   - `cosignature<Sig>` (and mirroring S05, `co<row<...>>` if cheap;
     otherwise record as follow-up): layer functor `type<X>` = a
     product, one responder per op, each a stored callable
     `Op -> std::pair<typename Op::response, X>` — function-space
     shaped per FD8.
   - Its Functor instance is LAZY: fmap post-composes onto each
     stored responder without invoking — the dual of the S03
     instance, same registration route (D-A) and the same
     capture-ownership rule. This laziness is what makes
     unfold_cofree over it terminate (FD8): assert it in a test
     (side-effect flag, as in S03).
   - Responders here can be plain `std::function`-shaped (copyable,
     multi-shot) OR match D-B's one-shot type — the mock side is
     conceptually re-askable, but a one-shot script is fine for
     paper one. Pick the simpler spelling that composes with
     unfold_cofree, and record the choice + rationale (DEVIATIONS
     row only if it contradicts FD8's text).
3. **Pairing** (in freer_cosignature.hpp or freer_run.hpp):
   `pair_run(Cofree<CoSig::type, B>&&, Freer<Sig, A>&&) ->
   std::pair<A, B>`: loop — Pure a → (a, current head); layer →
   select the impure_node's matching responder from the Cofree tail,
   invoke it with the op to get (response, next Cofree), resume the
   continuation with the response, continue. Share the S04 trace
   vocabulary: a `pair_run_trace` variant records the same Trace
   type so FD8's "tests read identically against either interpreter"
   claim is literally true — same assertion helpers.
4. **Tests** `freer_cosignature.t.cpp`:
   - unfold_cofree on a pure-data functor (standalone scheme test).
   - Laziness assertions for the co-signature instance.
   - A scripted KV responder built with unfold_cofree (seed = script
     state); pair_run against the S04 KV program; value + trace
     assertions IDENTICAL in form to S04's.
   - The FD10 shape: retry-with-backoff program over
     row<Clock, Network> (S05's test program grown to N=3 retries),
     Network script failing twice then succeeding, virtual-time
     Clock responder; assert exactly three Sends, backoff 1s/2s, no
     fourth attempt, correct final value — FD10's bullet list as
     CHECKs. (S08 promotes this to the example/paper artifact; keep
     the program a test-local function S08 can lift.)

## Build

`make TOOLCHAIN=<gcc-pin> test` and `make TOOLCHAIN=<clang-pin> test`.

## Verify (gate)

Full suite green both pins; the failing-twice script drives the
retry program synchronously with the FD10 assertions; `make lint`
clean.

## Done when

Gate green; committed `[freer] S06: cofree pairing mock` + handoff.

## Capture in handoff

unfold_cofree's landed signature; the co-signature product's shape
and how a responder is selected per op (S08 shows this in the
paper); the retry test-program's location/shape for S08 to lift;
whether the co-row (mock over a row) landed or is follow-up.

## Pitfalls

- If unfold_cofree recurses eagerly into the co-signature you will
  know immediately (hang/OOM): the recursion must be *inside* the
  post-composed closure, forced only when a responder is invoked.
- Cofree's tail is by-value (cofree.hpp:37): the "next" Cofree your
  responder returns is materialized per step — that's the strict
  representation working as designed (FD8), not a bug; but build it
  in the closure, not ahead of time.
- Box up the Cofree in the responder's return if completeness bites
  (same firewall reasoning as everywhere); record if needed —
  FD8's text doesn't currently box there.
- pair_run's loop must consume BOTH structures; no copies of the
  script (it may hold one-shot responders per D-B).
- Keep the co-signature's laziness test honest: fmap must not invoke
  responders even once (a single eager invocation = diverging
  unfold at depth 2, easy to miss with a short script).
