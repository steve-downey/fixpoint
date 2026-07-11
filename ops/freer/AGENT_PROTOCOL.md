# Worker agent protocol (Sonnet) — read this first, every time

You are a worker agent with no prior context, assigned **exactly one
step** by the orchestrator. Do that step and stop. Follow this
without improvising on process.

1. **Orient.** Read `ops/freer/PLAN.md` in full. Your step was named
   in your dispatch prompt; confirm its dependencies are all checked.
   If they aren't, or the prompt names no step, write nothing and
   report the mismatch.
2. **Load context.**
   - Read your step file `ops/freer/steps/NN-*.md` completely.
   - Read the handoffs the dispatch prompt lists (in
     `ops/freer/handoffs/`). Their "Forward notes" and "Discoveries"
     are authoritative — they override the step file where they
     conflict.
   - Read the design-doc sections your step file lists from
     `docs/freer-signature-functor-design.org`. FD4 (consuming
     traversal, capture-ownership rule) and FD5 (observational
     testing) apply to every step from S02 on.
   - Skim one existing header + its `.t.cpp` in `src/smd/fixpoint/`
     (e.g. `free.hpp`/`free.t.cpp`) to absorb house style before
     writing any code.
3. **Execute** the step's "Do" exactly, on the branch named in your
   dispatch prompt. Keep the diff minimal. New headers go into the
   module's FILE_SET; new tests into the module's test target; new
   examples get their own executable in `src/examples/CMakeLists.txt`.
   If you must deviate from the step or the design, do the smallest
   thing that works and record why (step 6d). **Never resolve a
   design question yourself** — if the step can't proceed without a
   decision that isn't in your inputs, that is a BLOCKED handoff.
4. **Gate.** Run both pinned-toolchain gates
   (`make TOOLCHAIN=<gcc-primary> test` and
   `make TOOLCHAIN=<clang-primary> test`; names in the PLAN Status
   log) plus any step-specific checks, and `make lint`. Do **not**
   proceed unless everything passes.
   - If the gate cannot pass: leave the checkbox unchecked, write a
     handoff with Status **BLOCKED** describing exactly where you
     stopped, what you tried, and verbatim diagnostics, and STOP.
5. **Record green — two commits, in this order.**
   a. **Step commit**: the step's code/doc changes only, message
      `[freer] SNN: <title>`. No co-author trailers. Note its hash.
   b. **Bookkeeping commit** (after writing the handoff, step 6): the
      ticked box + Status-log row in `ops/freer/PLAN.md` (citing the
      step commit's hash and per-toolchain test counts), the handoff
      file, and any `ops/freer/DEVIATIONS.md` rows, message
      `[freer] SNN: handoff`.
6. **Hand off.**
   a. Read the *next* step's file now (the PLAN checklist order).
   b. Copy `ops/freer/HANDOFF_TEMPLATE.md` to
      `ops/freer/handoffs/NN-<slug>.handoff.md`.
   c. Fill it in: what changed, verification evidence (per-toolchain
      counts, new test names, example output), deviations,
      discoveries — and, having just read the next step, specific
      forward notes (exact signatures you settled on, gotchas).
      Vague handoffs break the chain; be concrete.
   d. If anything contradicted the design doc, also append a row to
      `ops/freer/DEVIATIONS.md`.
   e. Land it all as the bookkeeping commit (5b).
7. **Stop.** Do not begin the next step. Do not merge, rebase, or
   push; the orchestrator owns branch management.
