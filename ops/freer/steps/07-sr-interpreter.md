# S07 — The S/R interpreter: mcata_free + task carrier

**Goal.** Interpret Freer programs into senders/receivers: a NEW
consuming Mendler-style fold over `Free` (`mcata_free` — the audit
established the existing `mcata` is Fix-only, const&) with
`beman::task<A>` as the monomorphic carrier, handlers mapping each
operation to a sender. Gate: the same program that ran synchronously
now runs live under Beman::execution only.

**Depends on:** S05, S07d. (Parallel with S06 — branch
`freer/sr-interpreter`.)
**Design refs:** §5 forward reference, FD4 (consuming recursor +
recorded one-shot/S-R correspondence), FD5 (same assertion
vocabulary), mendler.hpp's header comment (house Mendler
conventions).

## Do

1. **`src/smd/fixpoint/mcata_free.hpp`** (new): the Mendler fold over
   Free, consuming:
   ```
   mcata_free<Result>(pure_alg, phi, Free<F, A>&& prog) -> Result
   //   pure_alg : A&& -> Result
   //   phi      : (recurse, F<Free<F,A>>&&) -> Result
   //   recurse  : Free<F,A>&& -> Result   (mcata_free, partially applied)
   ```
   Mirroring mcata's documentation stance (mendler.hpp): no
   functor_typeclass lookup for F — phi holds the recursive call and
   applies it to whichever children it likes; the rank-2 discipline
   is by convention. Also test it standalone over a pure-data
   functor's Free (futu's chunks are ready-made values) so the
   scheme is independent of Freer.
2. **`src/smd/fixpoint/freer_task.hpp`** (new): the interpreter.
   - Handler concept: per operation, `handler(op) -> sender-of
     (typename Op::response)` (beman::execution sender whose value
     completion is the response type).
   - `run_task(Handler&&, Freer<Sig, A>&&) -> beman::task<A>`
     via mcata_free with `Result = beman::task<A>`: Pure a →
     co_return a; impure layer → co_await the op's sender, resume
     the continuation with the response (one-shot, rvalue invoke),
     recurse on the unboxed rest. The coroutine frame owns the
     moved-in node across the co_await — the FD4 capture-ownership
     rule in coroutine clothing: nothing borrowed from the caller's
     frame.
   - Row programs: accept a handler set covering the row (S05's
     shapes), so mocked-Network/live-Clock composition is expressible
     here too.
3. **Tests** `freer_task.t.cpp` (all single-threaded, deterministic —
   run on a manual/inline scheduler from beman.execution; no wall-
   clock sleeps, no threads in the gate):
   - mcata_free standalone (pure-data Free, e.g. sum a futu chunk).
   - KV program from S04 against op→sender handlers
     (`just(response)`-level), `sync_wait(run_task(...))`; value
     matches S04's synchronous run — FD5's observational equality
     across interpreters, asserted with the same helpers.
   - The retry program (S05/S06's shape) with a Network handler whose
     sender fails twice then succeeds and a Clock handler on virtual
     time; same trace/value assertions as S06's pairing run.
   - One deliberately-async smoke: an op whose sender completes via
     a scheduler hop, proving suspension actually crosses a
     scheduling point (still deterministic).

## Build

`make TOOLCHAIN=<gcc-pin> test` and `make TOOLCHAIN=<clang-pin> test`.

## Verify (gate)

Full suite green both pins; the live-interpretation assertions match
the synchronous ones; `make lint` clean.

## Done when

Gate green; committed `[freer] S07: S/R interpreter` + handoff.

## Capture in handoff

mcata_free's landed signature (design-doc §5 needs it recorded —
flag for the orchestrator's doc sync); the handler-concept spelling;
where the Box-per-resumption and coroutine-frame allocations sit
(FD6's cost paragraph, paper implementation notes); scheduler
choices that kept tests deterministic (S08 reuses); any place the
one-shot invariant fought the sender/receiver contract (FD4's
recorded correspondence — evidence either way is paper material).

## Pitfalls

- The continuation must be invoked as an rvalue exactly once, on the
  coroutine's resumption path; storing it in a local that outlives
  the co_await is fine (the frame owns it) — storing a reference to
  the *node* is not.
- Don't `co_await run_task(recurse(...))` naively if it stacks a
  coroutine frame per effect: prefer resuming into a loop inside one
  coroutine (mirror S04's loop) if frame-per-bind shows up in Asan
  or blows the demo scale. Record which shape landed and why.
- beman::task/sync_wait spellings per S07d's handoff, not memory.
- Keep Beman::execution usage inside freer_task.hpp/its test; the
  core headers must not grow the dependency.
