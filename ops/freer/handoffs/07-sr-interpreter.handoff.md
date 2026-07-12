# Handoff — S07 The S/R interpreter: mcata_free + task carrier

- **Status:** DONE (gate passed)
- **Commit:** 5298658 (`[freer] S07: S/R interpreter`)
- **Date / agent:** 2026-07-12, Sonnet worker

## What changed

- `src/smd/fixpoint/mcata_free.hpp` (new): `mcata_free<Result, F, A>` -- a
  NEW consuming Mendler-style fold over `Free<F,A>`, distinct from
  `mendler.hpp`'s Fix-only const& `mcata`. Two-callable algebra (Pure leaf
  + Roll layer), no `functor_typeclass<F>` lookup (mendler.hpp's documented
  stance). Consuming and BY VALUE (pure_alg/phi taken by value, `recurse`
  closes over its own owned copies) so the fold is safe when `Result` is a
  coroutine type. No beman dependency.
- `src/smd/fixpoint/mcata_free.t.cpp` (new): 4 `TEST_CASE`s in
  `smd_fixpoint_test` (beman-free), folding a pure-data futu chunk
  (`Free<IntListF,int>`, built the way `futu.t.cpp`'s `make_run` does) --
  sum, zero-Cons edge case, and a `std::string` Result proving Result is a
  free choice. Standalone: scheme proven independent of Freer/beman.
- `src/smd/fixpoint/freer_task.hpp` (new): the S/R interpreter. `run_task<Sig,
  A>(handler, prog) -> beman::task<A>` = `mcata_free` at `Result =
  beman::task<A>`. `task_handler_for<Handler,Op>` concept. Detail:
  `resume_one` (co_await sender, resume one-shot, unbox), `run_task_step`
  (impure-layer coroutine), `task_phi` (non-coroutine thin forwarder).
  The ONLY module header that includes beman.
- `src/smd/fixpoint/freer_task.t.cpp` (new): 4 `TEST_CASE`s in a NEW
  executable `freer_task_test` (links `fixpoint.fixpoint` +
  `beman::execution_headers` + `beman::task`, mirrors `freer_beman_probe_test`).
- `src/smd/fixpoint/CMakeLists.txt`: `mcata_free.hpp`/`freer_task.hpp` into
  FILE_SET `smd_fixpoint_headers`; `mcata_free.t.cpp` into `smd_fixpoint_test`;
  new `freer_task_test` executable.
- Did NOT touch `free.hpp`/`box.hpp`/`fmap.hpp`/`one_shot.hpp`/`monad.hpp`/
  `mendler.hpp`/`freer.hpp`/`freer_run.hpp`/`freer_row.hpp`/`cofree.hpp`, the
  beman provisioning (`cmake/beman-deps.cmake`, Makefile), or any pre-existing
  test. `git show --stat 5298658`: 5 files (CMakeLists.txt + 4 new).

## mcata_free's LANDED signature (design-doc §5 needs this recorded)

```c++
// smd::fixpoint, mcata_free.hpp
template <class Result, template <class> class F, class A, class PureAlg,
          class MAlgebra>
constexpr auto mcata_free(PureAlg pure_alg, MAlgebra phi, Free<F, A> &&prog)
    -> Result;
//   pure_alg : A&& -> Result                         (Pure-leaf case)
//   phi      : (recurse, F<Free<F,A>>&&) -> Result   (Roll-layer case)
//   recurse  : Free<F,A>&& -> Result   (mcata_free<Result,F,A> partially
//                                       applied; owns its own pure_alg/phi
//                                       copies BY VALUE)
```
Called explicitly as `mcata_free<Result, F, A>(pure_alg, phi, std::move(prog))`
-- `Result`, `F`, `A` are all non-deducible (F/A buried in the `Free<F,A>&&`
parameter). `pure_alg`/`phi` deduce. **Flag for orchestrator: fold this exact
signature into design-doc §5** (the forward reference currently only names
`mcata_free` + carrier `beman::task<A>`).

## Handler-concept spelling (as landed)

```c++
// smd::fixpoint, freer_task.hpp
template <class Handler, class Op>
concept task_handler_for =
    operation<Op> && std::invocable<Handler &, Op> &&
    beman::execution::sender<std::invoke_result_t<Handler &, Op>>;
```
Keyed on "is a beman sender". The tighter half of the contract -- the
sender's value completion is exactly `Op::response` -- is enforced
STRUCTURALLY at the resumption site (`typename Op::response response =
co_await handler(op);` in `resume_one`), deliberately keeping the concept
free of beman's completion-signature/env detail machinery. `static_assert`ed
inside `resume_one`, so a handler missing an op names the uncovered
`impure_node<Op,X>` in the backtrace (mirrors `run`'s static_assert).

`run_task` signature: `run_task<Sig, A>(Handler handler, Freer<Sig,A> prog)
-> beman::execution::task<A>` -- BY VALUE (see coroutine-ownership below);
Sig/A explicit (S04 deduction limitation), Handler deduces.

## Which coroutine shape landed, and why

**Frame-per-effect via `mcata_free` (NOT the resume-into-a-loop shape).**
`run_task` IS `mcata_free` at `Result = beman::task<A>` -- the design §5
claim as running code, not a hand-written loop. The impure-layer algebra
`run_task_step` is a coroutine that `co_await`s the op's sender (via a
per-Op `resume_one` coroutine), then `co_return co_await recurse(rest)` --
so interpreting an N-effect program stacks N `run_task_step` frames, each
suspended awaiting the next.

Chose this over the loop because the step's Goal + design §5 make
`run_task == mcata_free @ task<A>` the theoretical claim under test; a
hand loop would test a different artifact. Frame-per-effect is FD6's
disclosed demo-scale cost and is acceptable for the retry example (~6
effects). The single-coroutine resume-into-a-loop rewrite (mirror `run`'s
while loop; hoist the co_await out via a per-Op `resume_one` returning
`task<X>` as done here) is the available FD6 optimization if a later step
needs O(1) live frames -- **S08's example is small, so it is not needed there.**

## Where Box-per-resumption / coroutine-frame allocations sit (FD6 cost)

- **One `Box<X>` per resumption**, unchanged from `run`: `resume_one` does
  `*std::move(node.k)(response)` -- the one-shot returns `Box<X>`, deref'd
  and moved out (`box.hpp`'s `operator*() &&`). One heap Box alloc/free per
  effect.
- **One `beman::task` coroutine frame per effect** (`run_task_step`), plus
  **one transient `resume_one` frame per effect** (created, awaited, and
  destroyed before the recursion). So ~N `run_task_step` frames live
  simultaneously (frame-per-effect) + at most one live `resume_one` at a
  time. `just()`-level senders add no extra frames; the scheduler-hop op
  adds beman's own op-state for the `schedule`/`then`/`let_value` chain.
- No leaks under Asan on either pin (default `CONFIG=Asan`,
  `-fsanitize=address,undefined,leak`).

## The one-shot invariant vs. the sender/receiver contract (FD4 evidence)

The one-shot fit S/R cleanly -- no fight. `resume_one` invokes the move-only
`one_shot` continuation as an rvalue exactly once, on the coroutine's
resumption path (after `co_await` yields the response), matching FD4's
recorded correspondence (P2300 receivers are one-shot rvalues; the CPS
translation of handlers wants exactly one-shot continuations). DEV-S05-1
(bind can't carry a move-only continuation) **did not arise**: `run_task` is
`mcata_free`/coroutine-based, never `mbind`-based, so nothing routes the
`one_shot` through `FreeMonadImpl::bind`. The programs compose with
`mbind`+`pure` (copyable continuations only), exactly the sanctioned use.

**Coroutine capture-ownership (FD4 in coroutine clothing) -- two load-bearing
rules, documented in `freer_task.hpp`'s header:**
1. Every coroutine takes state it touches after a suspension BY VALUE
   (handler, recurse, layer, node) -- a by-value coroutine parameter is
   owned by the frame; a reference parameter dangles past the first suspend
   (beman task's `initial_suspend` is `suspend_always`, so the body does not
   begin until the producing full-expression's temporaries are gone).
2. The algebra handed to `mcata_free` (`task_phi`) is a NON-coroutine thin
   forwarder. A coroutine *lambda* would hold `handler` in a closure that is
   a temporary destroyed at the end of the task-producing full-expression,
   while the task stays suspended referencing it. `task_phi` forwards
   synchronously into `run_task_step`, which owns `handler` by value.

   Honest caveat on the teeth: because the landed shape is frame-per-effect,
   every parent frame stays suspended (alive) while its children run, so a
   naive reference-capture does NOT always surface as a live Asan UAF in
   these particular test flows (the referent up-chain survives). The
   by-value rules are therefore correct-by-construction / robust-to-refactor
   rather than currently exploitable -- but they become strictly necessary
   the moment anyone rewrites to the loop shape or reorders resumption, so
   they are kept and documented. (Verified by patching experiments: node
   by-rvalue-ref and pure_alg by-const-ref both still passed here, precisely
   because the parent frame outlives the child -- recorded so S08 does not
   mistake "passes" for "reference-capture is safe in general".)

## Scheduler choices that kept tests deterministic (S08 reuses)

- **KV / retry**: handlers return `beman::execution::just(response)` --
  already-ready, no scheduling. `sync_wait` drives its own internal
  `run_loop`; the whole interpretation runs synchronously, single-threaded.
- **Scripted Network + virtual Clock**: pure `just()` senders; the "fails
  twice then succeeds" is a *value* (`std::expected` holding `unexpected`),
  not a sender error-completion -- identical to S05's synchronous mock.
  Virtual time is an `int` the SleepFor handler advances; no `sleep_for`,
  no threads.
- **Async smoke**: the op's sender is
  `read_env(get_scheduler) | let_value([](auto sched){ return schedule(sched)
  | then([]{ return value; }); })` -- it reads the AMBIENT scheduler (which,
  inside a task under `sync_wait`, is `sync_wait`'s own `run_loop` scheduler,
  provided via the task promise's env) and defers onto it. A genuine deferred
  scheduling hop, driven deterministically by `sync_wait`'s loop -- no extra
  loop to hand-drive, single-threaded. A `hopped` bool set on the far side of
  the hop, checked after the value returns, witnesses the hop executed.
  (`beman::execution::inline_scheduler` is the trivial fallback if a later
  step wants a scheduling point with no ambient-scheduler dependency.)

## Verification evidence

```
make TOOLCHAIN=gcc-16 test    -> 100% tests passed, 264/264
make TOOLCHAIN=clang-23 test  -> 100% tests passed, 264/264
make lint                     -> all hooks passed (clang-format auto-fixed
                                 the 4 new files on the first pass, then
                                 clean on the immediate re-run before commit)
```
264 = this branch's pre-S07 baseline (256, post main-merge) + 8 new
(`ctest -N`, identical names on both pins):
- `smd_fixpoint_test` (+4, beman-free): `mcata_free - HeaderIsIdempotent`;
  `mcata_free: sums a futu chunk's Cons values plus its Pure seed`;
  `mcata_free: a chunk with zero Cons layers reduces to the Pure seed alone`;
  `mcata_free: renders a chunk's values into a string, Result independent of
  arithmetic accumulation`.
- `freer_task_test` (+4, beman-linked): `run_task [FD5]: KV get-bump-put
  matches the synchronous run's value`; `run_task [FD5]: KV Get on an absent
  key matches the synchronous run's value`; `run_task [FD10]:
  retry-with-backoff -- three Sends, backoff 1s/2s, no fourth attempt`;
  `run_task: an op whose sender completes via a scheduler hop suspends across
  a scheduling point`.

All 8 run under default `CONFIG=Asan` (`-fsanitize=address,undefined,leak`)
on both toolchains -- the coroutine interpreter and every resumption path are
sanitizer-policed. The KV values (10, 0) MATCH freer_run.t.cpp's synchronous
`run` for the identical program/store (FD5 observational equality across
interpreters). Retry: value 11, send_calls 3, virtual_time 3, trace exactly
`{Now(), Send(hello), SleepFor(1), Send(hello), SleepFor(2), Send(hello)}`.
Async smoke: value 42, `hopped == true`.

## Cross-compiler divergences

None observed. mcata_free, run_task, the beman `co_await`/`sync_wait`/
`read_env`/`let_value`/`schedule`/`then` usage, and all 8 tests behaved
identically on gcc-16 and clang-23. No compiler-conditional code. (Both pins
auto-enable `BEMAN_USE_MODULES=ON` per S07d, but only the header-only
`beman::execution_headers` + compiled `beman::task` targets are linked, so
the modules build is never triggered -- same as S07d.)

## Deviations from the plan / design

None requiring a `DEVIATIONS.md` row against the design doc. `run_task`
implements §5's forward reference exactly (mcata_free carrier `beman::task<A>`,
handler = op->sender). Two step-file-wording notes, both because reality
required it, recorded here:

1. The step says "same trace/value assertions as S06's pairing run." S06 is
   the parallel E1 branch and is NOT visible in this worktree, so YOURS
   asserts against the **FD10 facts directly** (exactly three Sends, backoff
   1s/2s, no fourth attempt, final reply length) using the S04
   `trace`/`render_operation` vocabulary the handlers record into. S08
   consolidates the two copies and unifies the assertion helper -- the retry
   program here is deliberately test-local, as the dispatch prompt directed.
2. `run_task` takes `Handler`/`prog` BY VALUE (not `Handler&&`/`Freer&&` as
   the step's prose sketches): the whole interpretation is a coroutine that
   must own everything it touches after a suspension (FD4). The `&&` sketch
   would dangle. Callers pass rvalues/lvalues freely; the by-value parameter
   moves/copies in.

## Discoveries affecting later steps

- **`run_task == mcata_free @ task<A>` is real and Asan-clean.** The design's
  central §5 claim -- the interpreter is the Mendler fold with the coroutine
  carrier -- holds end to end on both pins. Paper material.
- **The one-shot ⟷ P2300-receiver correspondence (FD4) is exhibited, not
  fought.** Rvalue one-shot invoke exactly once on the resumption path maps
  directly onto `co_await sender`. No `mbind`/DEV-S05-1 wall (run_task never
  uses bind).
- **Handlers must be copyable with shared mutable state by ref/pointer.**
  mcata_free copies phi (hence handler) per recursion; scripted mocks
  (call counters, virtual clock) MUST capture their state by reference so
  every copy observes it -- exactly how `run`'s handlers already work.
- **`sync_wait` provides its `run_loop` scheduler to the task's env**, so
  `read_env(get_scheduler)` inside a co_awaited sender yields a real,
  sync_wait-driven scheduler -- the deterministic way to get a genuine
  deferred scheduling hop without hand-driving a second loop. S08's "Run 2 --
  live S/R (deterministic inline/manual scheduler)" can use exactly this, or
  `beman::execution::inline_scheduler` for a scheduling point with no ambient
  dependency.
- **beman confinement holds**: `grep beman src/**/CMakeLists.txt` hits only
  `freer_beman_probe_test` and `freer_task_test`; `fixpoint.fixpoint` and
  `smd_fixpoint_test` link no beman; `mcata_free.hpp` has no beman include.

## Forward notes for the NEXT step (written after reading its step file)

S08 (`ops/freer/steps/08-integration.md`) consolidates the retry program,
adds `src/examples/freer_retry.cpp`, the `docs/freer-vs-interface.md`
comparison, and unified tests. Notes:

- **The canonical retry program to lift**: S07's is
  `retry_program()`/`retry_attempt(int)` in `freer_task.t.cpp` (lines ~163-191
  pre-format) over `Row = row<Clock, Network>`, with test-local `Now`/`SleepFor`/
  `Send` ops + `time_point`/`duration`/`request`/`reply`/`net_error` domain
  types (byte-identical to S05's in `freer_row.t.cpp`). Backoff = attempt
  number (1s after attempt 1, 2s after attempt 2); succeeds on the 3rd Send.
  S06 carries its own copy; consolidate BOTH into one header per the step's
  item 1 (do NOT put these vocabulary types in library headers).
- **Run 2 (live S/R)** is exactly `sync_wait(run_task<Row,int>(handler,
  retry_program()))` with the scripted-Network + virtual-Clock handlers from
  `freer_task.t.cpp`'s FD10 test. To print the trace, keep the handler-side
  `trace`-recording shape (handlers `push_back(render_operation(op))`) --
  run_task itself has no trace; the trace is a handler side-effect, reused
  verbatim from freer_run.hpp.
- **FD5 "tests read identically"**: assert Run-1 (Cofree pairing, S06) and
  Run-2 (S/R, this step) with the SAME helper on `{value, trace}`; both should
  yield value 11 and the 6-entry trace above. This step's KV tests already
  demonstrate cross-interpreter value equality against `run` -- the pattern
  extends to the retry program.
- **run_task takes handler/prog by value** -- an example that runs the same
  program twice must rebuild it (`retry_program()` per run) since the first
  `run_task` consumes it; `retry_program()` is a cheap factory, call it twice.
- **No performance claims** (FD10 non-goal): the frame-per-effect / Box-per-
  resumption costs recorded above are FD6 implementation notes, NOT example
  output. Keep the example single-threaded/deterministic (just()/inline/
  read_env-scheduler; no threads, no sleeps).
- **testinstall**: S08's item 5 sweep runs `make testinstall`. The new
  headers `mcata_free.hpp`/`freer_task.hpp` are in FILE_SET
  `smd_fixpoint_headers`, so they install; but `freer_task.hpp` includes
  beman headers, so any *installed* consumer of it needs beman on its include
  path. The example/tests here link beman explicitly; if `installtest/`
  transitively pulls `freer_task.hpp`, it will need the beman targets too --
  flag to check, since S07d's provisioning is FetchContent-in-build-tree, not
  an installed/exported dependency of `fixpoint.fixpoint`.

## Open risks / TODOs

- **Frame-per-effect at scale is unstressed.** The retry (~6 effects) and KV
  (2-4) are small. A program with dozens+ of effects would allocate dozens of
  simultaneously-suspended `run_task_step` frames. `run`'s loop is O(1) native
  stack; run_task is O(N) live coroutine frames. If S08's example or the paper
  ever scales the retry to many attempts, switch to the resume-into-a-loop
  shape (recorded above as the available FD6 optimization). Not a correctness
  risk at demo scale.
- **`read_env(get_scheduler)` relies on the ambient scheduler existing in the
  task env.** Under `sync_wait` it does (its run_loop). Under a different
  driver that provides no scheduler, the async-hop handler's sender would fail
  to connect. S08's "deterministic inline/manual scheduler" wording is
  satisfied by either the sync_wait run_loop (used here) or `inline_scheduler`
  (no env dependency) -- pick per how S08 drives the example.
- **Installed-consumer beman path** (see Forward notes, testinstall) is the
  one unproven integration surface for S08's sweep.
