# S03 — The signature layer: unit, operation, impure_node, signature, Freer

**Goal.** The real header: the closed-world Coyoneda layer over an
effect signature, with its generic Functor instance registered per
decision D-A and its continuation type per decision D-B. After this
step, `Freer<Sig, A>` is a real type and S01's hand-rolled probe
re-expresses against the library.

**Depends on:** S01 + D-A + D-B (quoted in your dispatch prompt — if
they are not, STOP with a BLOCKED handoff), S02.
**Design refs:** FD1, FD2, FD3 (incl. addendum), FD6 (incl.
granularity correction), FD11 (as resolved), FD12 (as resolved).

## Do

1. **`src/smd/fixpoint/freer.hpp`** (new; FILE_SET wiring in the
   module CMakeLists):
   - `unit` per FD2 (bespoke empty type, defaulted friend ==).
   - `concept operation` per FD2 (`std::movable` + `typename
     Op::response`).
   - `impure_node<Op, X>` per FD3: `Op op;` + the one-shot
     continuation returning `Box<X>`, whose concrete type is D-B's
     resolution (either `std::move_only_function<Box<X>(typename
     Op::response) &&>` behind the feature-test macro, or the bespoke
     one-shot type — if bespoke, it lives in this header or its own
     small header per D-B's text).
   - `signature<Ops...>` per FD3: nested `template <class X> struct
     type` holding `std::variant<impure_node<Ops, X>...>`, PLUS
     whatever nested typedefs D-A's resolution requires the layer to
     expose (e.g. the signature type and `X`) so the generic instance
     can recover them.
   - `Freer<Sig, A> = Free<Sig::template type, A>`.
   - The generic Coyoneda Functor instance per FD6's granularity
     correction: an instance object whose `fmap` takes the WHOLE
     layer `typename Sig::template type<X>&&` (consuming only — no
     const overload; the const path must simply never instantiate),
     visits the variant, and per alternative post-composes onto the
     stored continuation, capture-by-move throughout (FD4 rule),
     using `std::move(*box)` / the S02 rvalue deref. Registered per
     D-A: either the constrained variable-template partial
     specialization of `functor_typeclass`, or reachable via the
     mode-2/3 dispatch D-A prescribes — implement exactly what D-A
     says, nothing else.
2. **Tests** `freer.t.cpp`:
   - Re-express S01's probe against the real header: form
     `signature<Get, Put>`, `Freer<KV, int>`, hand-construct one
     suspended node, resume by hand, CHECK the Pure result.
   - `static_assert` the operation concept accepts Get/Put and
     rejects a payload without `response`.
   - Layer fmap is lazy: fmap over a suspended value does NOT invoke
     the continuation (side-effect flag), and resuming afterwards
     yields the mapped result. One fmap = composition, not traversal.
   - Consuming bind through the layer (via
     `monad_typeclass<Freer<KV,int>>` — the FD3-addendum windfall:
     the existing Free instances found by deduction + S02's
     consuming overloads): bind a suspended value, resume, CHECK.
   - Non-comparability: `static_assert(!std::equality_comparable<
     Freer<KV,int>>)` (or the S01-recorded fallback if that check
     proved eager on a pin).
   - The S02 deferred-invocation lifetime pattern, now against the
     real layer (Asan-gated).

## Build

`make TOOLCHAIN=<gcc-pin> test` and `make TOOLCHAIN=<clang-pin> test`.

## Verify (gate)

Full suite green on both pins; `make lint` clean. This is FD9's
"S01 TU compiles against real headers, both compilers" gate.

## Done when

Gate green; committed `[freer] S03: signature layer` + handoff.

## Capture in handoff

The landed spellings (signature's nested typedefs, the instance's
registration) — S04's send and every later step compile against
them; whether the Free-instance deduction windfall held on both pins
once bind actually instantiated (S01 only checked lookup selection);
any divergence between D-A/D-B's expectation and what you had to do
(DEVIATIONS row + tell the orchestrator loudly).

## Pitfalls

- Do not add a const-lvalue fmap for the layer "for completeness" —
  FD4: the impure layer only admits the consuming path. A const
  overload that can't compile is worse than none: it turns a clear
  "no const path" error into template vomit.
- `std::visit` over a moved variant: visit `std::move(layer.node)`
  and take alternatives by `&&` in the visitor (the `overloaded`
  helper works unchanged).
- The doubled-structure rule (see free.hpp's FreeMonadImpl comment):
  keep the instance's operation templates generic over their own
  element type, fixed only on the signature — same reasoning as the
  existing Free/Cofree instances, or join/derived shapes break later.
- Explicit trailing return types on anything self-recursive (house
  rule; GCC's "use before deduction of auto").
- deducing-this + protected access: if a helper on the instance needs
  to call itself through `self`, remember self's reified type is the
  Map type — private/protected helpers break across the typeclass
  chain; use detail:: free functions instead (house gotcha).
