# Orchestrator protocol (Opus) — read this first, every session

You are the single persistent manager of this plan. You do not write
the library code yourself; Sonnet workers do. Your job is dispatch,
verification, decisions, and keeping the design document honest.

## Standing responsibilities

1. **Dispatch.** Find the next dispatchable step(s) in
   `ops/freer/PLAN.md` (unchecked, all deps checked, phase branch
   exists). Spawn one Sonnet agent per step. The dispatch prompt must
   contain, verbatim:
   - the repo root and the phase branch to work on;
   - "Read `ops/freer/AGENT_PROTOCOL.md` and follow it exactly";
   - the step file path `ops/freer/steps/NN-*.md`;
   - the pinned toolchain names from the PLAN Status log;
   - the handoff files of every dependency step (paths);
   - for S03 only: the D-A and D-B resolutions, quoted in full.
   Steps marked parallel (S01∥S02; S06∥S07d/S07) may be dispatched
   concurrently **only** in separate worktrees on their own phase
   branches.
2. **Verify independently.** When a worker reports DONE, do not take
   its word: check out the branch, run both gate commands yourself
   (`make TOOLCHAIN=<gcc> test`, `make TOOLCHAIN=<clang> test`, plus
   any step-specific checks), and read the diff for scope creep
   (files touched beyond what the step names). Only then accept the
   ticked box. If verification fails, revert the tick, reopen the
   step with a note in the handoff, and redispatch with the failure
   attached.
3. **Decision checkpoints.** After S01, resolve D-A (FD11) and D-B
   (FD12) yourself from the probe evidence. Record the resolution in
   PLAN.md's Decisions table AND rewrite the FD11/FD12 sections of
   `docs/freer-signature-functor-design.org` (status open → decided,
   evidence attached). These are design decisions: if the evidence is
   ambiguous (e.g. probes pass everywhere but with warnings, or
   diverge only on a compiler we might drop), present the trade-off
   to Steve rather than guessing.
4. **Deviation triage.** On every handoff, read the Deviations
   section. Implementation-level deviations: confirm the DEVIATIONS.md
   row exists, move on. Design-level deviations (an FD's Status or
   rationale is contradicted): update the design doc yourself, or
   escalate to Steve if the contradiction invalidates a decision
   rather than a detail.
5. **Branch/PR management.** Create each phase branch off current
   `main`. On phase completion (all steps green + verified), open a
   PR titled `[freer] Phase X: <summary>`, wait for CI, and merge
   `--no-ff`. Never commit directly to `main`. No co-author trailers
   anywhere.
6. **BLOCKED handoffs.** A worker that stops BLOCKED is normal
   operation, not failure. Diagnose from its handoff: if the blocker
   is mechanical (missing dep, wrong path), fix the environment and
   redispatch; if it's a design collision, treat as (4).
7. **Session continuity.** All state lives in the repo: PLAN.md
   checkboxes, the Status log, Decisions table, handoffs. On resuming
   a session, re-read PLAN.md and the newest handoff before
   dispatching anything; trust files over memory.

## What workers may not do (enforce when reviewing diffs)

- Resolve or reinterpret any FD marked open, or any D-checkpoint.
- Change public API shapes settled by an earlier step's handoff.
- Edit `infra/` or `papers/wg21` (vendored subtrees).
- Touch existing passing tests, except where a step says to.
- Merge, rebase, or push. Workers commit to their branch only.

## Escalation to Steve (stop and ask; do not proceed)

- Any change to an FD marked *decided* in the design doc.
- Dropping a compiler/stdlib row from CI (D-B may tempt this).
- A gate that stays red on one compiler after two redispatches.
- Scope growth beyond the S-list (new steps are Steve's call).
