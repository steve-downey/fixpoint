# S08 — FD10 integration: the paper's opening example, end to end

**Goal.** The retry-with-backoff example as the paper will show it:
one program, two interpreters (Cofree pairing mock + live S/R), the
side-by-side virtual-interface comparison, packaged as a runnable
example. This is the separation-of-concerns claim as running code.

**Depends on:** S06, S07. (Branch `freer/integration`, cut after
both E-phase branches merged.)
**Design refs:** FD10 (all of it — the example must exhibit all
three parts of the preempted objection's answer), FD5, FD6
(non-goal: no performance claims).

## Do

1. **Consolidate the retry program.** Lift the test-local retry
   program (S06/S07 each carry a copy) into ONE canonical spelling —
   a small header under `src/examples/` or a shared test-support
   header (mirror how existing examples share; do not put example
   vocabulary types into the library headers). Domain types
   (`request`, `reply`, `net_error`, virtual `time_point`/`duration`)
   get their one real definition here.
2. **`src/examples/freer_retry.cpp`** (new example executable, own
   install block per house convention). Output tells FD10's story in
   order, printing what the paper narrates:
   - the program definition (once);
   - Run 1 — mock everything: Cofree script fails twice then
     succeeds, virtual clock; print the trace: exactly three Sends,
     backoffs 1s/2s, no fourth attempt, final reply.
   - Run 2 — same program, live S/R interpretation (deterministic
     inline/manual scheduler; "live" means through beman::execution,
     not that it hits a network): print result.
   - Run 3 — composition: Network mocked, Clock handler live-shaped —
     handler stacking, not a new fixture (FD10 part 3).
3. **The comparison section (paper artifact).** A markdown fragment
   `docs/freer-vs-interface.md` (or an org fragment beside the design
   doc — match the design doc's format) containing the side-by-side:
   the virtual-interface version of the same test (written out,
   compiling if cheap to keep, in a fenced block if not) against the
   Freer version, annotated with FD10's three points: (1) inheritance
   + reference semantics imposed; (2) "suspended on THIS request" is
   inexpressible so interaction tests devolve to side-channel logs;
   (3) handlers compose, fixtures don't. Keep it excerpt-ready: the
   paper lifts from here.
4. **Tests** `freer_retry.t.cpp` (or grow S06/S07's): both
   interpreters' runs of the canonical program asserted with the
   SAME helper — FD8's "tests read identically" claim, demonstrated
   in one file. Update S06/S07 tests to use the consolidated program
   (this is the one sanctioned edit of existing tests).
5. **Sweep.** Every new public header reachable, FILE_SET complete,
   `make testinstall` green (installed package still works),
   examples run and exit 0 under both pins.

## Build

`make TOOLCHAIN=<gcc-pin> test` and `make TOOLCHAIN=<clang-pin> test`;
run the example binary from both build trees; `make testinstall`.

## Verify (gate)

Full suite + testinstall green both pins; `freer_retry` runs, exits
0, output shows the three runs with the FD10 trace facts; `make lint`
clean.

## Done when

Gate green; committed `[freer] S08: FD10 integration` + handoff. The
handoff's Forward notes address the ORCHESTRATOR: what the paper
narrative can now claim as demonstrated, what remains sketch (e.g.
co-rows if S06 deferred them, Reflection-without-Remorse as recorded
future work), and a candidate list of FD-series updates for the
final design-doc sync.

## Capture in handoff

Where every paper-gating artifact lives (example, comparison doc,
trace assertions); measured demo-scale observations if any fell out
(disclosed as implementation notes, never as claims — FD10 non-goal);
the full list of DEVIATIONS rows accepted during the plan, for the
orchestrator's closing design-doc reconciliation.

## Pitfalls

- No performance claims anywhere in example output or the comparison
  doc (FD10 non-goal); costs are FD6's recorded notes.
- The interface-comparison code must be honest: write the virtual-
  interface version as a competent C++ programmer would, not a straw
  man — the three points must survive a fair fight.
- Keep the example single-threaded and deterministic; a flaky demo
  of "deterministic testing of async workflows" is a self-refuting
  artifact.
