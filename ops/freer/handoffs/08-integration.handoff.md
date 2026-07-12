# Handoff — S08 FD10 integration: the paper's opening example, end to end

- **Status:** DONE (gate passed)
- **Commit:** 91f82af (`[freer] S08: FD10 integration`)
- **Date / agent:** 2026-07-12, Sonnet worker
- **This is the FINAL step of the plan.** Forward notes address the
  ORCHESTRATOR (design-doc sync), not a next worker.

## What changed

- `src/examples/freer_retry_program.hpp` (new, EXAMPLE/test-support header —
  NOT in the `smd_fixpoint_headers` FILE_SET, deliberately beman-free):
  the ONE canonical retry program. Domain types (`time_point`, `duration`,
  `request`, `reply`, `net_error`), the two signatures (`Clock` =
  `signature<Now, SleepFor>`, `Network` = `signature<Send>`), `Row` =
  `row<Clock, Network>`, the recursive `retry_attempt(int)` + `retry_program()`
  (fetch time, then retry), the Cofree-pairing mock (`RetryScript`,
  `retry_coalgebra`, `retry_head_fn`), and shared assertion vocabulary
  (`expected_trace()`, `expected_value == 11`). All in NAMED namespace
  `retry_example`, `inline` functions, so every TU shares one definition.
- `src/examples/freer_retry_handlers.hpp` (new, EXAMPLE header, beman-linked):
  the sender/receiver handlers (`make_network_handler`,
  `make_virtual_clock_handler`, `make_mock_handler`, `make_live_clock_handler`),
  kept separate from the program header so the program header stays beman-free.
- `src/examples/freer_retry.cpp` (new example executable `freer_retry`): the
  three-run FD10 story — Run 1 (mock everything, Cofree pairing), Run 2 (same
  program, live S/R via `run_task` + `sync_wait`), Run 3 (composition: mock
  Network + live-shaped Clock, handler stacking). Own install block; built
  only `if(TARGET beman::task AND TARGET beman::execution_headers)`.
- `src/smd/fixpoint/freer_retry.t.cpp` (new test, new executable
  `freer_retry_test`, beman-linked): the item-4 "tests read identically"
  demonstration — the canonical program through BOTH interpreters
  (`pair_run_trace` and `run_task`) asserted with the SAME helper
  `expect_retry(value, log)`.
- `docs/freer-vs-interface.org` (new, paper artifact): the side-by-side
  virtual-interface-vs-Freer comparison, FD10's three points annotated,
  excerpt-ready. Org format to match the design doc. Virtual-interface code
  in fenced blocks (not compiled — the step permits this).
- `src/smd/fixpoint/freer_cosignature.t.cpp` (S06, sanctioned edit): removed
  the test-local FD10 vocabulary/program/mock copy; now `#include
  <examples/freer_retry_program.hpp>` + `using` decls. FD10 test's trace
  literal updated to the canonical 6-op trace (leading `Now()` added).
- `src/smd/fixpoint/freer_task.t.cpp` (S07, sanctioned edit): removed the
  test-local FD10 vocabulary/program copy; now includes the shared header +
  `using` decls. Its FD10 trace assertion was ALREADY the 6-op canonical
  trace, so it is unchanged; only the program source moved to the header.
- `src/smd/fixpoint/CMakeLists.txt`: added `freer_retry_test` executable
  (links `fixpoint.fixpoint` + `beman::execution_headers` + `beman::task`,
  mirrors `freer_task_test`).
- `src/examples/CMakeLists.txt`: added the `freer_retry` example (guarded on
  beman target availability).

## The S06/S07 divergence, resolved

The two copies differed structurally: S06 was fully unrolled (3 nested
mbinds, no leading `Now`, 5-op trace); S07 used a recursive `retry_attempt`
AND fetched `Now{}` first (6-op trace). **Resolved to: S07's recursive
`retry_attempt` shape, WITH the leading `Now{}` fetch kept.** Deliberate
reasons: (a) the recursive spelling is the cleaner canonical form; (b)
keeping the `Now{}` fetch means the program exercises BOTH Clock operations,
so "Clock fully mocked" is a real claim rather than half the signature going
untouched; (c) FD10's narrative is "fetch time, retry with backoff"; (d) it
lets both interpreters assert the identical 6-op trace. **The canonical
trace both interpreters assert:**
`{Now(), Send(hello), SleepFor(1), Send(hello), SleepFor(2), Send(hello)}`,
value `11`, send_count/send_calls `3`, virtual_time `3`. S06's test was the
one adjusted (added the leading `Now()`); S07's was already this.

## Verification evidence

```
make TOOLCHAIN=gcc-16 test    -> 100% tests passed, 271/271
make TOOLCHAIN=clang-23 test  -> 100% tests passed, 271/271
make TOOLCHAIN=gcc-16 testinstall    -> RunInstalledTest passed (1/1)
make TOOLCHAIN=clang-23 testinstall  -> RunInstalledTest passed (1/1)
make lint                     -> all hooks passed (clang-format auto-fixed the
                                 new files on the first pass; re-run clean)
freer_retry example (gcc-16)  -> runs, EXIT 0, three runs printed with the
                                 FD10 trace facts
freer_retry example (clang-23)-> runs, EXIT 0
```

271 = 270 branch baseline (264 S07 + 6 S06, both merged into
`freer/integration` base 481d8cd) + 1 net new (`freer_retry_test`'s single
`TEST_CASE`). S06/S07 FD10 tests were modified in place (count unchanged).

New/affected test names (identical on both pins):
- `freer_retry [FD8/FD10]: the Cofree pairing mock and the live S/R
  interpreter run the SAME program to the SAME value and trace` (NEW,
  `freer_retry_test`) — the cross-interpreter, same-helper demonstration.
- `pair_run_trace [FD10]: ...` (`smd_fixpoint_test`, S06) — now on the
  canonical program, 6-op trace.
- `run_task [FD10]: retry-with-backoff ...` (`freer_task_test`, S07) — now on
  the canonical program (unchanged assertions).

**Asan deferred-invocation path:** all beman-linked tests run under the
default `CONFIG=Asan` (`-fsanitize=address,undefined,leak`); the S06 laziness
FD4-teeth test (`cosignature layer fmap [FD8]`) and the S07 coroutine-lifetime
rules are exercised as before — S08 added no new capture-ownership surface
(the shared header's program/mock is the same code S06/S07 already
Asan-verified; no new hand-written continuation or responder).

## Cross-compiler divergences

None observed. The consolidated header, both new executables, the example's
three runs, and all 271 tests behaved identically on gcc-16 and clang-23. No
compiler-conditional code. One PRE-EXISTING, untouched warning remains in
`freer_row.t.cpp` (`operator<<(..., const Send&)` defined-but-not-used under
gcc) — it is S05's file, not touched by S08, and not `-Werror`.

## Deviations from the plan / design

None requiring a new `DEVIATIONS.md` row. The canonical-shape choice (keep
the leading `Now{}`) is within the step's explicitly-delegated latitude, and
it makes the example MORE faithful to FD10's "fetch time" narrative, not less.
The comparison doc is `.org` (not the step's nominally-named `.md`) to match
the design doc's format — the step explicitly permits this.

## Discoveries affecting later steps

(There is no next step; these are for the orchestrator's records.)

- **`src/` is on the include path for every target linking
  `fixpoint.fixpoint`** (via the top-level `example_fixpoint_headers` FILE_SET
  `BASE_DIRS src`), so `<examples/freer_retry_program.hpp>` resolves from
  library tests AND from `src/examples/*.cpp`. That is how one shared header
  reaches both `src/smd/fixpoint/*.t.cpp` and `src/examples/` without any new
  include-directory wiring.
- **Beman wiring for the example was pure build wiring, no design choice.**
  `FetchContent_MakeAvailable(execution task)` runs in
  `src/smd/fixpoint/CMakeLists.txt` under `FIXPOINT_ENABLE_TESTING`;
  `src/smd` is processed before `src/examples` (see `src/CMakeLists.txt`), so
  the beman targets exist by the time the example is configured. The example
  block is guarded `if(TARGET beman::task AND TARGET beman::execution_headers)`
  and links `beman::execution_headers` + `beman::task` — same two targets as
  `freer_task_test`. `make testinstall` is unaffected: `installtest/test.cpp`
  only includes `smd/example/fixpoint.hpp`, never `freer_task.hpp`, so no
  installed consumer pulls beman (S07's flagged risk did not materialize).
- **The shared program header must stay beman-free.** It is included by the
  beman-free `smd_fixpoint_test`; the S/R handlers (which need beman) are in
  the separate `freer_retry_handlers.hpp`. Keep that split if the header is
  ever extended.

## Forward notes to the ORCHESTRATOR

### What the paper narrative can now claim as DEMONSTRATED (running code)

- **FD10, all three parts, as one running artifact.** One program value,
  three interpretations, in `freer_retry.cpp`; the separation-of-concerns
  claim executes and exits 0 on both pins.
- **FD8 "tests read identically across interpreters."** `freer_retry.t.cpp`
  runs the canonical program through the Cofree pairing interpreter AND the
  live S/R interpreter and asserts BOTH with the same `expect_retry(value,
  log)` helper against the same trace/value. Literally one helper, two
  interpreters, one file.
- **FD5 observational testing across interpreters.** Both interpreters agree
  on value (11) and the 6-op trace; no structural equality on any
  Freer/Cofree value anywhere.
- **FD6 closure-post-composition Freer** interpreted both synchronously
  (pairing) and via the coroutine S/R carrier, at demo scale, Asan-clean.
- **The preempted "why not a virtual interface?" objection** has an
  excerpt-ready, fair-fight side-by-side in `docs/freer-vs-interface.org`,
  with FD10's three points annotated against competent (not straw-man)
  interface code.

### What remains SKETCH / future work

- **The virtual-interface comparison code is NOT compiled** — fenced blocks
  in the doc only. If the paper wants it machine-checked, add a small TU;
  the step judged it not worth the wiring and permitted the fenced form.
- **Reflection without Remorse (type-aligned deque)** — still the recorded
  FD6 future-work item; left-nested `mbind` chains keep a linear closure
  residue. Not needed at demo scale; unimplemented.
- **`std::move_only_function` fast path** for `one_shot` (FD6/FD12
  optimization) — deferred, bespoke `one_shot` shipped instead (D-B).
- **Frame-per-effect S/R interpreter is O(N) live coroutine frames** (S07);
  the resume-into-a-loop rewrite is the recorded FD6 optimization if the
  paper ever scales the retry to many attempts. Demo uses ~6 effects.
- **`std::function`-per-responder allocation** in the pairing mock (S06) and
  **Box-per-resumption + coroutine-frame-per-effect** (S07) are FD6
  implementation-note costs — disclosed in the example header comment and the
  comparison doc's non-goal section, NEVER claimed as an advantage (FD10
  non-goal honored: no performance claim anywhere in output or doc).

### Full list of DEVIATIONS rows accepted across the whole plan

(For the closing design-doc reconciliation. S08 added none.)

- **DEV-S00-1** (S00; FD9, FD12): libc++ at LLVM 23 lacks
  `std::move_only_function` entirely (non-implementation, not just a missing
  feature-test macro) — fed the D-B decision toward bespoke `one_shot`.
- **DEV-S01-1** (S01; FD3, FD5, FD9): `std::equality_comparable<Free<...>>`
  reports `true` (declaration visible, body never instantiated), so FD9 item
  2's literal `static_assert(!std::equality_comparable<...>)` does not hold;
  the operationally-relevant "never names/calls `==`" property does.
- **DEV-S05-1** (S05; FD4, FD6): `FreeMonadImpl::bind` structurally cannot
  carry a move-only (`one_shot`-owning) continuation — its recursive
  instantiation needs a copy of `Fn`. `mbind` stays safe for ordinary
  copyable-continuation programs (all retry programs qualify); only
  infrastructure (`discharge`, and any one-shot-carrying responder) must
  build nodes directly. Confirmed still true in S08: the canonical
  `retry_program` composes with `mbind` + `pure` and copyable continuations
  only, so the wall never bites the example.

### Candidate FD-series updates for the final design-doc sync

1. **FD10:** fold in that the example FETCHES time first (leading `Now{}`)
   and that the canonical trace is the 6-op sequence — the design's FD10
   prose lists "three Sends, backoff 1s/2s, no fourth attempt" but does not
   mention the `Now` fetch; the shipped example exercises both Clock ops.
2. **FD8 / §5:** record `mcata_free`'s landed signature (from the S07
   handoff — still flagged as not yet in §5) and note the two interpreters
   share the `trace`/`render_operation` vocabulary verbatim, now proven in
   one file (`freer_retry.t.cpp`).
3. **FD5:** the "not comparable" caveat (DEV-S01-1) should be folded into
   FD5's predicate-caveat paragraph as decided reality (it already is,
   partially — confirm it matches the committed test).
4. **FD6:** move the recorded costs (Box-per-resumption, closure chains,
   frame-per-effect, `std::function`-per-responder) into a single
   implementation-notes subsection; S08's example and comparison doc both
   reference them as notes, so the doc should have one canonical home.
5. **FD11/FD12:** already marked decided; no change needed from S08.
6. **DEVIATIONS reconciliation:** DEV-S00-1 and DEV-S05-1 are the two that
   change design claims (FD9 toolchain floor; FD4/FD6 `mbind` generality) —
   fold both into the affected FD sections' rationale.

## Open risks / TODOs

- **No compiled virtual-interface counterpart** (see Sketch above) — the only
  unproven surface in the comparison doc; acceptable per the step.
- **`freer_task.t.cpp` and `freer_retry.t.cpp` both run an FD10 run_task
  test** — deliberate (one is the S/R interpreter's own test, one is the
  cross-interpreter demonstration), minor redundancy, each with distinct
  intent. Not consolidated to keep S07's file as the S/R unit test.
- **The `freer_retry` example builds only when beman targets exist** (i.e.
  under `FIXPOINT_ENABLE_TESTING`, which is on for every `make` build and the
  gate). A hypothetical consumer building examples with testing OFF would not
  get this one example; that matches the confinement of beman to the
  test/interpreter surface and is intentional.
