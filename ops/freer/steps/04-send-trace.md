# S04 — send, the trace handler, and the observational test vocabulary

**Goal.** `send<Sig>(op)` (the unit of the adjunction) plus the first
interpreter: a synchronous handler-driven runner whose reference
implementation is the trace-collecting handler. This step creates the
assertion vocabulary (FD5) every later test speaks.

**Depends on:** S03.
**Design refs:** FD5, FD7, FD1 (the KV example).

## Do

1. **`src/smd/fixpoint/freer.hpp`** — `send<Sig>(op)` exactly per
   FD7's listing: identity-into-Pure continuation via
   `roll_free<Sig::template type>`, adjusted only to S03's landed
   spellings (D-B's continuation type, in_place variant construction
   if S01/S03 found it necessary).
2. **`src/smd/fixpoint/freer_run.hpp`** (new) — the synchronous
   runner:
   - `run(Handler&&, Freer<Sig, A>&&) -> A`: loop (not recursion —
     left-nested binds make deep chains; FD6 notes the closure chain
     is linear): visit the node; Pure → return the value; layer →
     visit the impure_node, call
     `handler(op)` to get the operation's `response`, resume
     `std::move(k)(response)`, unbox, continue. Handler is any
     callable set covering every Op in Sig (an `overloaded` of
     lambdas per op is the intended spelling; constrain so a missing
     op is a concept error naming the op, not a visit error).
   - The trace handler as the reference mock (FD5): a small adaptor
     `tracing(handler)` (or equivalent) that records each visited
     operation (type + payload rendering) into a `std::vector` trace
     alongside delegating for the response; `run_trace(handler,
     prog) -> std::pair<A, Trace>`. Design the Trace type so tests
     can assert "exactly these ops, in this order" tersely — this
     vocabulary is FD5's deliverable and S06/S07/S08 reuse it
     verbatim.
3. **Tests** `freer_run.t.cpp` — the KV example from FD1/FD7:
   - `Get`/`Put` ops over a `std::map` handler; the FD7 usage-sketch
     program (get → put f(v) → pure v) written with
     `monad_typeclass<...>.bind` (per FD4, only pure/bind — no
     derived ops).
   - Trace assertions: run_trace yields the expected value AND the
     expected op sequence [Get k, Put k v'].
   - One-shot-ness observable: after run, the program has been
     consumed (compile-time: run takes Freer&&; a second run of the
     same named value is ill-formed — negative compile check via
     static_assert on invocability, not a runtime test).
   - Non-comparability restated: tests assert on run results only
     (FD5) — grep yourself honest: no `==` on Freer values anywhere.

## Build

`make TOOLCHAIN=<gcc-pin> test` and `make TOOLCHAIN=<clang-pin> test`.

## Verify (gate)

Full suite green both pins; KV trace assertions pass; `make lint`
clean.

## Done when

Gate green; committed `[freer] S04: send + trace handler` + handoff.

## Capture in handoff

The runner's and trace vocabulary's exact shapes (S05 threads rows
through them; S06 mirrors the vocabulary for the pairing; S08's
paper example is written in it); how deep a bind chain the loop
comfortably handles (S08's retry loop wants N≈3 but the paper may
show more); ergonomics notes on writing programs with raw
`monad_typeclass.bind` (candidate helper ideas go in the handoff,
NOT in the header — adding sugar is a design decision).

## Pitfalls

- The runner must be a loop over `Free` values, rebinding
  `prog = *std::move(box_from_continuation)` each iteration —
  recursion here stacks a frame per effect and Asan won't love the
  demo programs.
- `std::visit` on the moved node: same pattern as S03.
- Don't let the trace type capture pointers into consumed nodes; ops
  are moved out of the node before the handler sees them — the trace
  stores copies/renderings, not references.
- Resist building `>>=`-style operator sugar now; the paper's
  narrative may want it, but that's an FD-level (naming/API) call —
  handoff note, orchestrator decides.
