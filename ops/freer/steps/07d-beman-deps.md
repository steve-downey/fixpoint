# S07d — Provision beman.execution + beman.task

**Goal.** Make `beman::execution` (std::execution reference
implementation) and `beman::task` available to the build in both
dependency-provisioning modes (vcpkg and FetchContent fallback), per
the repo's dual-mode convention, without linking them into anything
yet. Pure build machinery; S07 consumes it.

**Depends on:** S00. (Parallel with anything; branch
`freer/sr-interpreter`, where S07 continues.)
**Design refs:** §5 forward reference; CLAUDE.md's dependency-
provisioning description.

## Do

1. **Investigate availability first, adapt second.**
   - vcpkg: check whether beman-execution / beman-task ports exist in
     the registry the repo's vcpkg resolves against. If yes, add to
     `vcpkg.json`. If no (likely), skip the vcpkg side and record it —
     FetchContent becomes the only path and the Makefile's vcpkg mode
     must still configure (the fallback include must fire for these
     deps even when vcpkg provides catch2; look at how
     `infra/cmake/use-fetch-content.cmake` is keyed before assuming).
   - FetchContent: extend the fallback (or add a freer-specific
     include alongside it — do NOT hand-edit `infra/` itself, it is a
     vendored subtree; if the hook must live inside infra, STOP,
     BLOCKED handoff, the orchestrator decides) to fetch pinned tags
     of `bemanproject/execution` and `bemanproject/task`. Pin exact
     tags/commits, not branches.
2. **Wire minimally.** The library target does NOT link them. Add a
   probe test target (or a compile-only test in the module test
   CMake) `freer_beman_probe.t.cpp`: includes
   `<beman/execution/execution.hpp>` (spelling per the fetched
   version — verify, don't guess) and `<beman/task/task.hpp>`,
   `sync_wait(just(42))`-level smoke, co_return-level task smoke.
   Link only that target against the beman targets.
3. **Both compilers.** beman.execution is heavy modern C++; if a
   pinned toolchain can't compile it, capture diagnostics verbatim
   and go BLOCKED — dropping a pin is an orchestrator/Steve call
   (see ORCHESTRATOR_PROTOCOL escalation list).
4. **Document** in the step commit: how a clean checkout gets the
   deps in each mode (README-level note can wait for S08; a comment
   at the FetchContent site suffices now).

## Build

`make TOOLCHAIN=<gcc-pin> test` and `make TOOLCHAIN=<clang-pin> test`
— and additionally `make reconf` first on at least one toolchain to
prove a from-scratch configure fetches cleanly.

## Verify (gate)

Fresh configure + full suite green on both pins with the probe test
passing; if vcpkg is installed locally, the vcpkg mode also
configures; `make lint` clean.

## Done when

Gate green; committed `[freer] S07d: beman deps` + handoff.

## Capture in handoff

Exact tags pinned and include spellings/target names (S07 writes
against them); which provisioning modes actually work; compile-time
cost observations (beman.execution can be slow — S07's iteration
loop wants to know); any toolchain trouble short of BLOCKED.

## Pitfalls

- Do not modify `infra/` (vendored subtree). Extension points only.
- beman projects rename targets/headers between tags; trust the
  tag's own README/CMake, not memory.
- The Asan default CONFIG applies to the probe too — beman under
  Asan is the configuration S07 will actually run, so a green probe
  here de-risks the real step.
