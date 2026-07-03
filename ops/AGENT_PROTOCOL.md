# Agent protocol — read this first, every time

You are an independent agent with no prior context. You will do **exactly
one step** and stop. Follow this without improvising on process.

1. **Orient.** Read `ops/PLAN.md` in full. Pick the first unchecked step
   whose dependencies (listed beside it) are all checked. That is *your*
   step. If none qualifies, write nothing and report that the plan is
   blocked or complete.
2. **Load context.**
   - Read your step file `ops/steps/NN-*.md` completely.
   - Read the handoffs in `ops/handoffs/` for every dependency of your
     step (at minimum the most recent one). Treat their "Forward notes"
     and "Discoveries" as authoritative — they override the step file
     where they conflict.
   - Read the design-doc sections your step file lists from
     `docs/recursion-schemes-design.md`. §3 (decisions) and §4
     (conventions) apply to every step.
   - Skim one existing header + its `.t.cpp` in `src/smd/fixpoint/`
     (e.g. `fix.hpp` / `fix.t.cpp`) to absorb house style before writing
     any code.
3. **Execute** the step's "Do" exactly. Keep the diff minimal. New
   headers go into the module's FILE_SET; new tests into the module's
   test target; new examples get their own executable + install block in
   `src/examples/CMakeLists.txt`. If you must deviate from the step or
   the design, do the smallest thing that works and record why (step 6d).
4. **Gate.** Run `make TOOLCHAIN=gcc-16 test` and any step-specific
   checks (example binaries run and exit 0; static_asserts compile). Do
   **not** proceed unless everything passes.
   - If the gate cannot pass: leave the checkbox unchecked, write a
     handoff with Status **BLOCKED** describing exactly where you stopped
     and what you tried, and STOP.
5. **Record green.** Tick your step's box in `ops/PLAN.md`, append one
   row to its Status log (include the new total test count), and commit
   everything as one commit: `[schemes] SNN: <title>`.
6. **Hand off.** This is the part that makes the chain work:
   a. **Read the *next* step's file** `ops/steps/<next>.md` now.
   b. Copy `ops/HANDOFF_TEMPLATE.md` to `ops/handoffs/NN-<slug>.handoff.md`.
   c. Fill it in: what changed, verification evidence (test counts,
      example output), deviations, discoveries — and, having just read
      the next step, write **specific forward notes** for the next agent
      (exact signatures you settled on, files to look at, gotchas).
      Vague handoffs break the chain; be concrete.
   d. If anything contradicted the design doc, also append a row to
      `ops/DEVIATIONS.md`.
   e. Amend the handoff into your step's commit or make it a second
      commit `[schemes] SNN: handoff` — either is fine.
7. **Stop.** Do not begin the next step.
