# Handoff — S06 Cofree pairing: unfold_cofree, the lazy co-signature, the scripted mock

- **Status:** DONE (gate passed)
- **Commit:** 1056370 (`[freer] S06: cofree pairing mock`)
- **Date / agent:** 2026-07-12, Sonnet worker

## What changed

- `src/smd/fixpoint/unfold_cofree.hpp` (new): `unfold_cofree<F>(head_fn,
  coalgebra, seed) -> Cofree<F, B>` — Cofree's `ana` half (the 2026-07-09
  FD8 audit found it missing). Mirrors `unfold_fix`'s lookup-based shape
  (recursion_schemes.hpp) exactly, generalized to a coalgebra PAIR. Depends
  only on `cofree.hpp`/`fmap.hpp` — no dependency on the signature layer, so
  it stands alone as a recursion scheme.
- `src/smd/fixpoint/freer_cosignature.hpp` (new): `responder<Op, X>`,
  `cosignature<Sig>` (both `signature<Ops...>` and `row<Sigs...>` cases —
  the co-row LANDED), the generic LAZY co-signature Functor instance
  (registered via the same constrained-`functor_typeclass`-partial-spec
  route as S03, D-A), `Mock<Sig, B>`, and the pairing interpreters
  `pair_run` / `pair_run_trace`.
- `src/smd/fixpoint/freer_cosignature.t.cpp` (new): 6 `TEST_CASE`s (names
  below).
- `src/smd/fixpoint/CMakeLists.txt`: added `unfold_cofree.hpp` and
  `freer_cosignature.hpp` to `smd_fixpoint_headers`, and
  `freer_cosignature.t.cpp` to `smd_fixpoint_test`'s sources.
- Did NOT touch `free.hpp`/`box.hpp`/`fmap.hpp`/`one_shot.hpp`/`monad.hpp`/
  `cofree.hpp`/`freer.hpp`/`freer_row.hpp`/`freer_run.hpp` or any
  pre-existing test — `git show --stat 1056370`: 4 files changed
  (CMakeLists.txt + 3 new files). `pair_run`/`pair_run_trace` live in
  `freer_cosignature.hpp`, so `freer_run.hpp` was not edited at all.

## Verification evidence

```
make TOOLCHAIN=gcc-16 test    -> 100% tests passed, 260/260
make TOOLCHAIN=clang-23 test  -> 100% tests passed, 260/260
make lint                     -> all hooks passed (clang-format auto-fixed
                                 the 3 new files on the first pass, then
                                 clean on the immediate re-run before
                                 committing, per the established process)
```

260 = S05's landed baseline (254) + 6 new (`ctest -N`, identical names on
both pins):

- `unfold_cofree [FD8]: builds a Cofree tree over a pure-data functor (NatF)
  by unfolding a coalgebra pair` — the standalone scheme test (no
  co-signature involved).
- `unfold_cofree: a terminal seed (n=0) yields a single Zero-tailed node`
- `cosignature layer fmap [FD8]: post-composes onto the stored responder
  without invoking it; the new responder owns its captures (FD4)` — the
  laziness assertion (side-effect flag, `CHECK_FALSE(invoked)` before
  invocation) AND the FD4 Asan capture-ownership test, in one.
- `pair_run [FD8]: a scripted KV mock drives the same program run/run_trace
  would, without a trace`
- `pair_run_trace [FD8]: a scripted KV mock (built with unfold_cofree)
  yields the fetched value and the exact op sequence, same form as S04's
  run_trace` — value + trace assertions IDENTICAL in form to S04's KV test.
- `pair_run_trace [FD10]: a scripted Network (failing twice, then
  succeeding) paired with a virtual-time Clock drives the retry program
  synchronously` — the gate. 4 assertions: `value == 11`,
  `log == {"Send(hello)", "SleepFor(1)", "Send(hello)", "SleepFor(2)",
  "Send(hello)"}` (exactly three Sends, backoff 1s/2s, no fourth attempt),
  `send_count == 3`, `virtual_time == 3`.

**Asan capture-ownership teeth, verified by hand** (per the protocol's
FD4 requirement, S03/S05 precedent): temporarily changed
`detail::cosignature_layer<...>::fmap_one`'s new-responder capture in
`freer_cosignature.hpp` from `[old = std::move(r.respond), fn]` to
`[old = std::move(r.respond), &fn]`, rebuilt under gcc-16, and ran the
`cosignature layer fmap [FD8]` test in isolation:

```
==...==ERROR: AddressSanitizer: stack-use-after-scope ...
SUMMARY: AddressSanitizer: stack-use-after-scope
  freer_cosignature.t.cpp:170 in operator()
```

— the exact line where the mapped responder reads the (now-dangling) `fn`
captured by reference, `fn` itself owning the test's heap-backed `salt`
`std::string`. Reverted to the value capture and reran both pinned gates:
260/260 on both. **Non-obvious detail worth recording:** the FIRST version
of this test used a bare `int salt` captured state (as S03/S05 did), and
the `&fn` break did NOT trip Asan — the all-`inline`/templated fmap
machinery collapses every stack frame, so a dangling `int` read from
reclaimed-but-unpoisoned stack reads clean. Switching the captured state
to a HEAP-backed `std::string` made the teeth bite reliably (the dangling
reference now traces to a real heap event ASan always tracks). Any later
step writing an FD4-teeth test against this all-inline layer should use a
heap-backed captured value, not a bare scalar.

## Cross-compiler divergences

None. Every new test, the co-signature's variadic-base product construction,
the lazy fmap's pack-expansion, and the Asan-teeth check behaved
identically on gcc-16 and clang-23. No compiler-conditional code in any
new file. The whole S06 surface compiled first-try on both pins (no
`in_place_type`-vs-aggregate-init surprises like S03 hit — the
struct-of-responders is a plain aggregate over variadic responder bases,
no variant converting-constructor involved on the co-signature side).

## Deviations from the plan / design

None requiring a `DEVIATIONS.md` row against the design doc. The one
delegated choice (responder representation) was resolved within FD8's
explicit latitude — see below. No `DEVIATIONS.md` rows added this step.

## Discoveries affecting later steps

- **Responder representation: `std::function`, NOT `one_shot`** (the
  step's explicitly-delegated choice). Rationale: FD8's own text says the
  mock is "conceptually re-askable," and Cofree's tail is a by-value/shared
  structure (unlike Freer's move-only one-shot continuation), so a
  copyable, multi-shot `std::function`-shaped responder is both sufficient
  and the natural fit — and it composes cleanly through `unfold_cofree`'s
  layer_fmap without any of the move-only-continuation friction. **Because
  the responder is copyable, DEV-S05-1's `mbind`/`bind` wall never bites
  here**: nothing on the co-signature side threads a move-only continuation
  through `mbind`. `pair_run`/`pair_run_trace` are `run`-shaped explicit
  while-loops (never `bind`-composed), so they sidestep it structurally too
  — no node-building-by-hand workaround was needed anywhere in S06. (If a
  future variant wants a one-shot/scripted-consumable responder, THEN
  DEV-S05-1 reapplies; this step deliberately did not go there.)
- **`unfold_cofree`'s landed signature** (recursion_schemes-style, in
  `smd::fixpoint`):
  ```c++
  template <template <class> class F, class HeadFn, class Coalgebra, class Seed>
  constexpr auto unfold_cofree(HeadFn head_fn, Coalgebra coalgebra,
                               const Seed &seed)
      -> Cofree<F, std::remove_cvref_t<std::invoke_result_t<HeadFn, const Seed &>>>;
  ```
  `F` (the functor template) must be named explicitly at every call site
  (`unfold_cofree<NatF>(...)`, `unfold_cofree<cosignature<KV>::template
  type>(...)`); `HeadFn`/`Coalgebra`/`Seed` deduce. Explicit trailing
  return type (not `auto`) for the same deduction-cycle reason as
  `CofreeFunctorImpl::fmap`/`FreeMonadImpl::bind`. The recursive closure
  captures `head_fn`/`coalgebra` **by value** (FD4) — load-bearing for a
  lazy F, invisible-but-harmless for an eager one; header comment explains,
  and a standalone `[&]`-vs-value probe confirmed the lazy case
  stack-use-after-returns under Asan with `[&]`.
- **The co-signature product's shape + per-op responder selection** (what
  S08 shows in the paper): `cosignature<Sig>::type<X>` is a struct that
  **inherits one `responder<Op, X>` base per operation** (`struct type :
  responder<Ops, X>...`), flattened over all of a row's member signatures
  (reusing `freer_row.hpp`'s `detail::row_ops_pack_t` verbatim — the
  co-row LANDED, not a follow-up). A responder is selected per op by a
  direct base-class upcast: `static_cast<responder<Op, M>&>(state.tail)` —
  O(1), no scanning/variant-visit, because each op's responder is a
  distinct base subobject. Marker typedefs `co_signature_ops` /
  `co_signature_value_type` (deliberately co-signature-specific names,
  disjoint from S03's `coyoneda_signature`/`coyoneda_value_type`) key the
  generic Functor instance's constrained partial spec, so a layer can never
  satisfy both `coyoneda_layer` and `co_signature_layer` at once.
- **The lazy fmap builds one NEW responder per op, `fn` copied into each**
  (`detail::cosignature_layer<Pack>::fmap_layer` → `fmap_one<Op>`): unlike
  `CoyonedaFunctorImpl::fmap` (one `std::visit`-selected live alternative,
  `fn` moved once), the co-signature is a PRODUCT — every responder
  coexists — so `fn` must be **copyable** and is duplicated once per op.
  This is the structural reason the responder side stays `std::function`-
  friendly and never wants a move-only `fn`.
- **`pair_run` / `pair_run_trace` landed signatures** (in
  `freer_cosignature.hpp`, `smd::fixpoint`):
  ```c++
  template <class Sig, class A, class B>
  auto pair_run(Mock<Sig, B> &&mock, Freer<Sig, A> &&prog) -> std::pair<A, B>;
  template <class Sig, class A, class B>
  auto pair_run_trace(Mock<Sig, B> &&mock, Freer<Sig, A> &&prog)
      -> std::tuple<A, B, trace>;
  // Mock<Sig, B> = Cofree<cosignature<Sig>::template type, B>
  ```
  Both are `run`-shaped while-loops consuming BOTH structures (no copies of
  the script): each iteration selects the matching responder from the
  current Mock state's tail, invokes it to get `(response, next_state)`,
  REPLACES the Mock state with `next_state`, resumes the program's
  continuation with `response`. O(1) native stack regardless of program
  length, exactly like `run`. **`Sig`, `A`, AND `B` must all be named
  explicitly at every call site** — the deduction-limitation is universal
  (S04/S05): `Sig` can't be recovered from `Freer<Sig,A>`, and `B` can't
  be recovered from `Mock<Sig,B>` (= `Cofree<...,B>`), both for the same
  alias-transparency reason.
- **`pair_run_trace` reuses S04's `trace` vocabulary VERBATIM** (same
  `trace = std::vector<std::string>` type, same `render_operation`,
  `traceable_operation` gating): FD8's "tests read identically against
  either interpreter" is literally true — the KV `pair_run_trace` test's
  assertions are the same shape as S04's `run_trace` KV test, and the FD10
  test's trace assertion is the same shape as S05's discharge trace. Each
  op (`Now`/`SleepFor`/`Send`/`Get`/`Put`) needs only its own `operator<<`,
  same as any bare-signature op.
- **`pair_run_trace` is a second small loop, NOT a `tracing`-wrapper over
  `pair_run`** (contrast `run_trace`, which wraps `run`'s handler via
  `tracing`): `pair_run` has no handler argument to wrap — the mock's
  responders play that role — so a tracing adaptor would have to rebuild an
  equivalent "tracing Cofree" for no benefit over duplicating a short loop.
  If S08 wants a single canonical driver it can refactor, but the
  duplication is deliberate and small.
- **The retry test-program for S08 to lift** lives in
  `freer_cosignature.t.cpp`'s anonymous namespace as `retry_program()`
  (returns `RowFree = Freer<row<Clock, Network>, int>`), grown to N=3
  attempts (S05's was N=2). Kept a test-local function exactly so S08 can
  lift it. Its Clock/Network domain types (`time_point`, `duration`,
  `request`, `reply`, `net_error`, and the `Now`/`SleepFor`/`Send` ops with
  their `operator<<`) are duplicated locally from S05's — anonymous-
  namespace types can't cross TUs — so S08's "consolidate into one
  canonical spelling" (its step 1) has TWO copies to unify (S05's
  `freer_row.t.cpp` and this file's).
- **The scripted mock's coalgebra shape** (what S08 lifts as the mock
  script): a plain-data script-state struct (`RetryScript{int send_count;
  int virtual_time;}` for FD10; a `std::map` for KV) + a
  `coalgebra: State -> cosignature<Sig>::type<State>` returning one
  responder lambda per op (each capturing the state by value, returning
  `(response, next_state)`) + a `head_fn: State -> B`. `unfold_cofree`
  over that is the whole mock. The coalgebra NEVER reaches a terminal
  layer (there's always a next state) — fine, because the co-signature's
  Functor instance is lazy: `unfold_cofree` builds exactly one node
  eagerly and defers all recursion to responder-invocation time.
- **Did NOT need to Box the Cofree tail in the responder.** FD8's text
  doesn't box there, and completeness never bit: `responder<Op, X>` stores
  a `std::function<std::pair<Op::response, X>(Op)>`, and `std::function`'s
  target type is type-erased behind a pointer, so naming
  `responder<Op, Cofree<...>>` never requires the Cofree complete at the
  declaration point (same firewall reasoning as `one_shot`/`Box`, but
  `std::function` already provides it). Recorded per the step's request.

## Forward notes for the NEXT step (written after reading S08's file)

S08 (`ops/freer/steps/08-integration.md`, depends on S06 **and** S07,
branch `freer/integration`) builds the FD10 example end-to-end. Notes:

- **The retry program to consolidate is `retry_program()` in
  `freer_cosignature.t.cpp`** (this step) and its twin in
  `freer_row.t.cpp` (S05). They differ only in N (3 here, 2 there) and in
  whether they open with a `Now` — unify to ONE spelling. The domain types
  (`request`/`reply`/`net_error`/`time_point`/`duration` + the three ops
  and their `operator<<`) must move to S08's shared example/test-support
  header (NOT into a library header — the step is explicit: keep example
  vocabulary out of `src/smd/fixpoint/`).
- **Run 1 (mock everything) is already written** — it's this step's FD10
  `pair_run_trace` test. S08 lifts the mock (`RetryScript` +
  `retry_coalgebra` + `retry_head_fn` + the `unfold_cofree<cosignature<Row>
  ::template type>(...)` call) essentially verbatim into the example
  binary; the trace facts to print are exactly this test's CHECKs.
- **"Same helper, both interpreters" (FD8/S08 step 4) works today**:
  `pair_run_trace` and S07's S/R `run_trace` both yield the same `trace`
  type; assert both runs of the canonical program with the same
  `trace{...}` literal. This step already demonstrates the pairing half of
  that identity against S04/S05's `run_trace` shape.
- **The co-row is done** — `cosignature<row<Clock, Network>>` is real and
  exercised end-to-end in the FD10 test; S08 does not need to build any new
  co-signature machinery, only reuse `Mock<Row, State>` /
  `pair_run_trace<Row, int, State>`.
- **Deduction is explicit everywhere**: `pair_run<Sig,A,B>`,
  `pair_run_trace<Sig,A,B>`, `unfold_cofree<F>`, `send<Row>`,
  `run<Sig,A>` — name every type parameter at every call site.

## Open risks / TODOs

- **`std::function`'s small-buffer/heap allocation per responder is an
  FD6-class cost, undisclosed-as-a-claim** (FD10 non-goal: no performance
  claims). Each `fmap_one` allocates a new `std::function` per op per
  unfold step; at the demo's depth (≤3 retries, 2 signatures) this is
  trivial, but it IS a per-step allocation on top of FD6's `Box`-per-
  resumption. S08 should keep it in the implementation-notes bucket, never
  the example's output — same discipline as FD6's other recorded costs.
- **`unfold_cofree` over an EAGER functor must reach a terminal layer or it
  diverges** — documented in the header comment, tested only in the
  terminating direction (NatF from a finite seed). No guard against an
  eager non-terminating coalgebra (there can't be one — it's the caller's
  contract, same as `unfold_fix`). Flagged so nobody mistakes the lazy
  co-signature's always-terminating behavior for a general property of
  `unfold_cofree`.
- **`cosignature<T>` for a `T` that is neither `signature<...>` nor
  `row<...>` fails with an incomplete-type diagnostic** (primary template
  left undefined, mirroring `freer_row.hpp`'s `signature_ops_pack`). Not a
  readable `static_assert` — if S08 or a user hits it, the message is
  "incomplete type `cosignature<...>`", not a sentence. Left as-is to match
  the existing house pattern; flag if a friendlier diagnostic is wanted.
