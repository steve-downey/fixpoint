# S05 — Rows: variant-of-signatures, Member injection, handler adaptors

**Goal.** Recover openness (§6): programs over several independent
signatures, with concept-checked injection (the C++ image of
`Member`) and handlers that discharge one signature of a row leaving
the rest. Gate is FD10's composition shape: Clock + Network in one
program, Network mocked, Clock real-shaped.

**Depends on:** S04.
**Design refs:** FD1 (closing paragraph: openness recovered), §6
forward reference, FD10 (the two signatures).

## Do

1. **`src/smd/fixpoint/freer_row.hpp`** (new):
   - `row<Sigs...>`: a composed signature whose layer
     `type<X>` is the variant over ALL member signatures'
     impure_nodes (flattened — one variant, not variant-of-variants;
     record if reality forces the nested shape instead). It must
     satisfy whatever D-A's registration keys on, so the S03 generic
     instance covers rows with no new fmap.
   - `member<Op, SigOrRow>` concept: Op is an operation of the
     row/signature. `send` extended (or overloaded) so
     `send<Row>(op)` injects any member op — the concept produces the
     readable error when it isn't one.
   - Handler adaptor discharging one signature: given a Row program
     and a complete handler for ONE member signature, produce a
     program over the remaining row (`discharge<Sig>(handler,
     prog)`), by reinterpreting: walk the program (consuming), handle
     matching ops inline, re-emit non-matching ops via send into the
     smaller row. `run` from S04 then closes over a fully-discharged
     program (or accept a handler-set covering the whole row in one
     run call — implement discharge; the all-at-once run may fall
     out for free; record what you did).
   - Modeled on completion_signatures-style set manipulation
     (design §6): membership/subset as concepts, not SFINAE soup.
2. **Tests** `freer_row.t.cpp` — FD10's signatures verbatim (test-
   local `time_point`/`duration`/`request`/`reply`/`net_error`
   stand-ins are fine at this step):
   - `Clock = signature<Now, SleepFor>`, `Network = signature<Send>`
     per FD10; a small program over `row<Clock, Network>` using ops
     from both.
   - member<> accepts/rejects: static_asserts.
   - Discharge Network with a scripted synchronous handler (S04
     vocabulary) while Clock ops flow through untouched; then
     discharge Clock; final value + trace assertions.
   - Order-independence: discharging Clock first then Network yields
     the same observation (FD5: observational equality).
   - A one-signature row behaves exactly like the bare signature
     (send/run interop).

## Build

`make TOOLCHAIN=<gcc-pin> test` and `make TOOLCHAIN=<clang-pin> test`.

## Verify (gate)

Full suite green both pins; the composed Clock+Network program runs
with Network mocked and Clock real-shaped; `make lint` clean.

## Done when

Gate green; committed `[freer] S05: rows + discharge` + handoff.

## Capture in handoff

Row's landed encoding (flattened vs nested — S06's co-signature
mirrors it; S07's handler-per-signature stacks on discharge); the
member/discharge signatures exactly; how errors read when an op
isn't in the row (paper evidence); performance note if discharge's
re-emission visibly stacks closures (FD6's recorded cost, fine, but
measure enough to say so).

## Pitfalls

- Discharge is CONSUMING and self-recursive through stored
  continuations: the capture-ownership rule applies to the re-emitted
  continuation wrappers. The S02 Asan lifetime test pattern should
  get a discharge-shaped sibling here.
- Flattening packs: `signature<Ops...>` needs to expose its op pack
  (S03's nested typedefs likely already do); avoid instantiating
  every impure_node just to compute the row type — alias-level pack
  concatenation only.
- Two signatures sharing an op type is ill-formed (duplicate variant
  alternative) — static_assert a readable diagnostic rather than
  letting variant produce its own.
- Don't build the open-union index machinery of the 2015 paper
  (Typeable tags); the closed variant + concepts IS the design (FD1).
