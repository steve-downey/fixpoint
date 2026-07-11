# S01 — FD9 baseline gate TU + mechanics probes

**Goal.** Prove (or find the cracks in) the constructs the Freer
layer stacks, before any library code exists: the self-embedding
variant-of-move_only_function shape, the non-instantiation of
comparison, and the three mechanics probes whose results decide D-A
(FD11) and D-B (FD12). This step produces *evidence*, not API.

**Depends on:** S00.
**Design refs:** FD1, FD3 (incl. code-audit addendum), FD9, FD11,
FD12.

## Do

All probes live in ONE new test file
`src/smd/fixpoint/freer_baseline.t.cpp` (wired into the existing
fixpoint test target), using *test-local* struct definitions — the
real `signature`/`impure_node` land in S03; here you hand-roll
minimal local copies so a failure names a language/library construct,
not a design artifact. Keep each probe a separate TEST_CASE (or
static_assert cluster) with a comment naming which FD it gates.

1. **Probe: the FD3 shape compiles.** Local `Get`/`Put` ops (per
   FD1), local `impure_node<Op, X>` holding
   `std::move_only_function<smd::fixpoint::Box<X>(typename
   Op::response) &&>`, local `KVSig` with nested
   `template <class X> struct type { std::variant<...> node; };`,
   then form `smd::fixpoint::Free<KVSig::template type, int>`,
   construct one suspended value by hand (payload + identity-into-
   Pure continuation, FD7's shape inlined), resume it by hand
   (`std::move(k)(response)`), and CHECK the result is the expected
   Pure. This is the whole cascade of FD9 in one TU.
2. **Probe: comparison never instantiates.** Form the type from (1);
   `static_assert(!std::equality_comparable<...>)` on the layer type
   and on the Free type. If the equality_comparable check itself
   hard-errors instead of yielding false (an eager deleted-ness
   check), do not fight it: reduce to "the TU compiles without ever
   naming ==", record the diagnostic verbatim, and note it — that
   verbatim diagnostic *is* the deliverable.
3. **Probe (a): member-template template-template deduction (FD3
   windfall / D-A input).** static_assert that
   `smd::typeclass::functor_typeclass<Free<KVSig::template type,
   int>>` and `monad_typeclass<...>` select the Free partial
   specializations (i.e. their type is not `std::false_type`). Do
   NOT call fmap/bind through them (the const paths would demand
   copies); the lookup selecting is the probe.
4. **Probe (b): constrained variable-template partial specialization
   (D-A input).** A self-contained mock registry:
   `template <class T> inline constexpr auto probe_tc = std::false_type{};`
   plus a constrained partial specialization
   `template <class T> requires probe_concept<T> inline constexpr
   auto probe_tc<T> = /* marker */;` where `probe_concept` keys on a
   nested typedef your local layer exposes. static_assert the
   constrained specialization is selected for the layer and the
   primary for `int`.
5. **Probe (c): the `&&`-qualified move_only_function corner (D-B
   input).** Exercised by (1) already; additionally check move-only
   propagation: `static_assert(std::movable<Layer> &&
   !std::copyable<Layer>)`, and a runtime move + resume.
6. **Dual-compiler capture.** Run the gate under both pins. Wherever
   the compilers disagree — different diagnostics, one accepting and
   one rejecting a probe — capture the diagnostics verbatim in the
   handoff's Cross-compiler divergences section. If a probe fails on
   ONE compiler only: keep the probe in the file under a
   compiler-conditional (`#if defined(__clang__)` etc.) with a
   comment citing the diagnostic, so the surviving assertions still
   gate, and record a DEVIATIONS row.

## Build

`make TOOLCHAIN=<gcc-pin> test` and `make TOOLCHAIN=<clang-pin> test`.

## Verify (gate)

Full suite green on both pins, including the new TEST_CASEs (modulo
compiler-conditionals per 6). `make lint` clean.

## Done when

Gate green; committed `[freer] S01: baseline gate TU + probes` and
the handoff commit. The handoff's probe-results table is complete —
the orchestrator resolves D-A/D-B from it and will not dispatch S03
until it can.

## Capture in handoff

A table: probe × compiler × result (accepted / rejected+diagnostic).
The exact local spellings that worked (nested-template syntax,
variant construction) — S03 reuses them as the real API. Anything
about `Box<X>` with incomplete X that surprised you.

## Pitfalls

- Keep probes independent: one probe's failure must not take the
  others down with it. Separate TEST_CASEs / static_assert clusters;
  compiler-conditional only the failing one.
- `std::variant`'s converting constructor can get confused by
  single-alternative variants of non-copyable types; use in_place
  construction (`std::in_place_type<impure_node<...>>`) if overload
  resolution misbehaves, and note it.
- Do not include a not-yet-existing `freer.hpp` — everything local.
- The continuation lambda must return `Box<X>` by value via
  `make_box<X>(...)`; `X` is incomplete at some points of the layer's
  definition — that's the point (Box is the firewall). If a probe
  needs `Box<X>` complete where you didn't expect, record, don't
  restructure.
