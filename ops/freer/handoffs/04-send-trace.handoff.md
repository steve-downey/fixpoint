# Handoff — S04 send, the trace handler, and the observational test vocabulary

- **Status:** DONE (gate passed)
- **Commit:** 5921690 (`[freer] S04: send + trace handler`)
- **Date / agent:** 2026-07-11, Sonnet worker

## What changed

- `src/smd/fixpoint/freer.hpp`: added `send<Sig, Op>(op) -> Freer<Sig,
  typename Op::response>` (FD7, the unit of the adjunction). No other
  change to this file.
- `src/smd/fixpoint/freer_run.hpp` (new): the synchronous interpreter and
  FD5's trace vocabulary:
  - `run<Sig, A, Handler>(Handler&&, Freer<Sig,A>&&) -> A` — the loop-based
    runner.
  - `traceable_operation<Op>` concept — `operation<Op>` (freer.hpp)
    plus an ADL-found `operator<<(std::ostream&, const Op&) ->
    std::ostream&`.
  - `trace` = `std::vector<std::string>`.
  - `render_operation<Op>(const Op&) -> std::string`.
  - `tracing_handler<Handler>` / `tracing(handler, log) ->
    tracing_handler<...>` — the adaptor the step file asked for.
  - `run_trace<Sig, A, Handler>(Handler&&, Freer<Sig,A>&&) ->
    std::pair<A, trace>`.
- `src/smd/fixpoint/freer_run.t.cpp` (new): 5 `TEST_CASE`s plus a
  `static_assert` pair — see "Exact new test names" below.
- `src/smd/fixpoint/CMakeLists.txt`: added `freer_run.hpp` to
  `smd_fixpoint_headers` and `freer_run.t.cpp` to `smd_fixpoint_test`'s
  sources (both correctly named FILE_SET/test target, per S03's
  correction).
- Did not touch `monad.hpp`, `free.hpp`, `box.hpp`, `fmap.hpp`,
  `one_shot.hpp`, `overloaded.hpp`, or any pre-existing test file. `git
  show --stat 5921690`: 4 files changed (CMakeLists.txt, freer.hpp, plus
  the two new files).

## Verification evidence

```
make TOOLCHAIN=gcc-16 test    -> 100% tests passed, 250/250
make TOOLCHAIN=clang-23 test  -> 100% tests passed, 250/250
make lint                     -> all hooks passed (clang-format
                                  auto-fixed freer.hpp/freer_run.hpp/
                                  freer_run.t.cpp on the first pass, then
                                  clean on the immediate re-run before
                                  committing, per the established process)
```

250 = S03's landed baseline (245) + 5 new (`ctest -N`, identical names on
both pins):

- `run_trace [FD5][FD7]: KV get-bump-put program yields the fetched value and the exact op sequence`
- `run_trace [FD5]: a Get on an absent key hands the handler response through unmodified`
- `run_trace [FD6]: a four-send chain (two get-bump-put rounds) resumes in order`
- `run_trace: a long chain of separately-suspended operations (500 get/put rounds) runs without recursing the native stack`
- `run [FD7]: drives the KV program to completion without a trace`

Plus two file-scope `static_assert`s (not separate `ctest` entries) proving
`run`'s one-shot-ness at compile time — see "Discoveries" below for why
they're written with `std::invocable`, not a bare `requires{...}`.

## Cross-compiler divergences

None. Everything — `send`'s literal aggregate-init variant construction,
the `run` loop, the trace vocabulary, the `std::invocable`-based negative
compile check — behaved identically on gcc-16 and clang-23. No
compiler-conditional code in either new file.

## Deviations from the plan / design

None requiring a `DEVIATIONS.md` row — if anything this step *removes* a
previously-logged caution rather than adding a contradiction. See the
first item under Discoveries.

## Discoveries affecting later steps

- **FD7's literal aggregate-init spelling for the variant construction
  compiles cleanly here, contra S01's/S03's defensive precedent.**
  `send` is written exactly as FD7 sketches it:
  ```c++
  return roll_free<Sig::template type>(typename Sig::template type<X>{
      impure_node<Op, X>{std::move(op), one_shot<Box<X>(R)>([](R r) -> Box<X> {
          return make_box<X>(pure_free<Sig::template type>(std::move(r)));
      })}});
  ```
  Before landing this, I isolated the construction in a standalone probe
  (single-alternative and two-alternative `signature<...>`, matching
  `send`'s exact shape) and compiled it directly with `g++-16` and
  `clang++-23` outside the CMake build — both exit 0, no diagnostics. So
  **`std::in_place_type` was NOT needed for `send`**, unlike
  `CoyonedaFunctorImpl::fmap` (S03), which still needs it. My working
  hypothesis for why the two call sites differ: `send`'s `impure_node<Op,
  X>` prvalue is an *exact type match* for one alternative of
  `node_variant`, so overload resolution for `variant`'s converting
  constructor has only one viable candidate to consider structurally (no
  competing conversion sequences to rank across alternatives) and never
  needs to reason about the *other* signature ops' alternative types at
  all. `CoyonedaFunctorImpl::fmap` builds its result inside a
  `std::visit`-dispatched generic lambda templated over `Op`, which may
  be a different context for the same converting-constructor mechanics —
  I did not chase this further; flagging as unconfirmed. **Net effect for
  S05+: don't assume `in_place_type` is always required — when a new call
  site builds a `Sig::template type<X>` (or a row's variant, S05), try the
  literal spelling first and fall back only if it actually fails to
  compile.**
- **`Sig` cannot be deduced from a `Freer<Sig, A>` function parameter —
  every `run`/`run_trace`/`send` call site must specify `Sig` (and
  usually `A`) explicitly, e.g. `run<KV, int>(handler, prog)`,
  `run_trace<KV, int>(handler, prog)`.** `Freer<Sig, A>` is the alias
  `Free<Sig::template type, A>` (freer.hpp); alias templates are
  transparent to deduction, so the actual parameter type the compiler
  deduces against is `Free<F, A>` with `F` needing to match
  `Sig::template type` — a template parameter buried inside a dependent
  *qualified-id*, not a template-template-parameter slot deduction can
  invert. Confirmed empirically (first draft of `freer_run.t.cpp` used
  unqualified `run_trace(handler, prog)` calls and every one failed with
  gcc-16's "couldn't deduce template parameter 'Sig'"). **This will bite
  S05's row types identically** (`row<Sigs...>` will presumably alias to
  `Free<row<Sigs...>::type, A>` the same way) — plan for explicit
  `run<Row, A>(...)` / `send<Row>(op)` call sites there too, not
  deduction.
- **A bare `requires(Args...) { call-expr; }` at namespace/global scope is
  NOT a safe way to negative-test "this call doesn't compile" once every
  template parameter of the callee is already resolved (explicit or
  trivially deduced) and the only remaining problem is ordinary argument
  binding (e.g., an rvalue-reference parameter can't bind a named
  lvalue).** That binding failure is not a template *substitution*
  failure — nothing is being substituted anymore — so it falls outside a
  requires-expression's SFINAE "immediate context" carve-out and GCC (and
  presumably Clang, not separately isolated but the full build was clean
  on both pins with the `std::invocable` version) hard-errors instead of
  evaluating the requirement to `false`. Reproduced in isolation with a
  two-line repro (`void g(S&&); static_assert(!requires(S s){ g(s); });`
  hard-errors) before rewriting the real check. **The fix, and the
  pattern S05+ should reuse for any "does NOT compile" compile-time
  check once all template arguments are pinned down: `std::invocable<Fn,
  Args...>`**, e.g.
  ```c++
  using RunFn = decltype(run<KV, int, KVHandler &>);
  static_assert(std::invocable<RunFn, KVHandler &, KVFree &&>);
  static_assert(!std::invocable<RunFn, KVHandler &, KVFree &>);
  ```
  `std::invocable` is specified to answer exactly "would this call be
  well-formed" via decltype in an unevaluated context and does not hit
  this pitfall (verified with the same two-line repro pattern).
- **`counter_chain_program`'s base case issues one trailing bare `Get`
  with no matching `Put`** (a minor test-construction detail, not a
  `run`/`freer_run.hpp` behavior): the "500 rounds" test therefore
  produces 1001 trace entries (`2*500 + 1`), not 1000; the assertions
  account for this. Not a bug in `run` — just how that particular test
  program terminates.

## Forward notes for the NEXT step (written after reading its step file)

S05 (`ops/freer/steps/05-rows.md`) adds `freer_row.hpp`
(`row<Sigs...>`, `member<Op, SigOrRow>`, `discharge<Sig>(handler, prog)`).
Notes for that work:

- **`run`/`run_trace`/`tracing`/`send` from this step are already generic
  over any `Sig` shaped like `signature<Ops...>` — they only touch
  `Sig::template type<X>`, its `node_variant` member, and `impure_node`.
  If `row<Sigs...>::type<X>` is built with the same nested-typedef surface
  S03 established (`coyoneda_signature`, `coyoneda_value_type`,
  `node_variant`, and a `node` data member of that variant type), **`run`
  and `run_trace` should need zero changes to work over a row** — the
  generic `std::visit(overloaded{...}, std::move(layer.node))` dispatch
  doesn't care how many signatures contributed alternatives to the
  variant, only that each alternative is some `impure_node<Op, X>`. Worth
  confirming with a quick row-shaped `run` test before assuming it, but
  the design should fall out for free per FD6/FD11's whole point.
- **Remember the deduction limitation above**: `send<Row>(op)` and
  `run<Row, A>(handler, prog)` will need `Row` (and `A`) named explicitly
  at every call site in `freer_row.t.cpp`, exactly as this step's tests
  do for bare signatures. Don't spend time trying to make `Sig`
  deducible from a `Freer<Sig,A>`/`Row`-aliased parameter — it structurally
  can't be, per the discovery above.
- **`traceable_operation`/`trace`/`render_operation`/`tracing`/
  `run_trace` are ready to reuse verbatim** for the row's Clock/Network
  example — each op in FD10's `Clock`/`Network` signatures just needs its
  own `operator<<`, the same as this step's `Get`/`Put`.
- **When building `row<Sigs...>`'s variant construction (in `send` or
  wherever a row-shaped `Sig::template type<X>` literal gets built),
  try the literal aggregate-init spelling first** (this step's discovery
  above) rather than assuming `in_place_type` is required — verify with a
  quick standalone probe the same way this step did if in doubt.
- **`tracing_handler<Handler>::operator()` is templated on
  `traceable_operation Op`, one op at a time** — `discharge<Sig>(handler,
  prog)`'s "handle matching ops inline, re-emit the rest via `send`" shape
  (S05's own design) will likely want to build on `run`'s
  `std::visit(overloaded{...}, ...)` pattern directly (matching on
  `impure_node<Op, X>` per alternative) rather than on `tracing_handler`,
  since discharge needs to conditionally re-emit rather than always
  delegate-and-record.

## Ergonomics notes on writing programs with raw `monad_typeclass.bind`
(handoff-only, per the step file — NOT implemented, an FD-level/orchestrator
call)

- Every nested continuation in `freer_run.t.cpp`'s test programs re-fetches
  `const auto &monad = smd::typeclass::monad_typeclass<KVFree>;` at its own
  nesting level (rather than capturing the outer one), because each
  continuation is a separate lambda and re-deriving the (stateless,
  `inline constexpr`) dictionary object is free — but it reads noisily
  four levels deep (`double_bump_program`). Two candidate simplifications
  for the orchestrator to weigh, neither implemented here:
  1. **`smd::typeclass::mbind(ma, fn)` already exists** (`monad.hpp`) as a
     direct forwarding free function to `monad_typeclass<remove_cvref_t
     <MA>>.bind(ma, fn)` — it is *not* a derived op (no `join`/`kleisli`/
     `apply` involved), so using it instead of manually re-deriving
     `monad_typeclass<KVFree>.bind(...)` at every nesting level would
     arguably still satisfy this step's "`monad_typeclass<...>.bind` +
     `pure` ONLY (FD4)" instruction while being visibly less noisy. I kept
     the explicit dictionary-object form throughout this step's tests to
     match S03's precedent and the letter of the dispatch prompt as
     written, but flagging that `mbind` is already there and FD4-legal if
     the orchestrator wants later steps' tests to prefer it.
  2. A `>>=`-style operator or small `do`-notation-shaped pipeline helper
     over `send`/`bind`/`pure` (the step file's own suggestion) would cut
     the nesting further still, but is explicitly an FD-level naming/API
     decision this worker does not make.

## Open risks / TODOs

- **Untested dimension: closure-chain depth from repeated *eager*
  left-nested binds onto one already-suspended node** (FD6's explicitly
  recorded "residue" cost — "Reflection without Remorse" territory).
  This step's depth evidence covers two different things well
  (`double_bump_program`: 4 separate `send`s composed via nested nodes,
  resumed correctly; `counter_chain_program`: 500 *separately-suspended*
  effects, each constructed lazily one at a time, proving `run`'s
  while-loop is O(1) native-stack per resumed effect) but does NOT
  construct a single node with N nested `fmap`/`bind` closures stacked
  onto its *one* continuation before ever resuming it (e.g., binding a
  `Pure` value through N `.bind` calls up front, which post-composes N
  closures without touching a suspended node at all, then finally
  binding onto one `send`). If S08's retry loop or the paper's example
  ever builds a program that way, resuming that single node's
  continuation invokes N nested C++ closures in one call — bounded by
  N, not by the number of separate effects `run`'s loop sees. Not
  exercised here; flagging for whichever step first writes a genuinely
  eager left-nested-bind-heavy program (S08 is the most likely).
- No other open risks specific to this step. `send`'s and `run`'s exact
  landed signatures are stable inputs for S05.
