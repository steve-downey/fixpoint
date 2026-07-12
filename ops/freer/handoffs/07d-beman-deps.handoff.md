# Handoff — S07d Provision beman.execution + beman.task

- **Status:** DONE (gate passed)
- **Commit:** a5f406c (`[freer] S07d: beman deps`)
- **Date / agent:** 2026-07-11, Sonnet worker

## What changed

- `cmake/beman-deps.cmake` (new): `FetchContent_Declare()`s
  `bemanproject/execution` and `bemanproject/task` at pinned commit SHAs,
  both `EXCLUDE_FROM_ALL`. Deliberately does **not** call
  `FetchContent_MakeAvailable()` — see "Discoveries" below for why.
- `Makefile`: appends `cmake/beman-deps.cmake` to `_cmake_top_level`
  (`CMAKE_PROJECT_TOP_LEVEL_INCLUDES`) in *both* the vcpkg branch and the
  FetchContent-fallback branch, after the existing mode-specific entry.
  Both branches' values are now semicolon-joined and quoted.
- `src/smd/fixpoint/CMakeLists.txt`: inside the existing
  `if(FIXPOINT_ENABLE_TESTING)` block, after the existing
  `smd_fixpoint_test` wiring, calls `FetchContent_MakeAvailable(execution
  task)` and adds a new, separate executable target
  `freer_beman_probe_test` built from `freer_beman_probe.t.cpp`, linked
  against `Catch2::Catch2WithMain`, `beman::execution_headers`, and
  `beman::task`, registered via `catch_discover_tests`.
  `fixpoint.fixpoint` itself links nothing new (verified: `grep beman`
  across `src/**/CMakeLists.txt` only hits the new probe target).
- `src/smd/fixpoint/freer_beman_probe.t.cpp` (new): two `TEST_CASE`s —
  `sync_wait(just(42))` smoke, and a `task<int>` `co_return 42` smoke,
  each via `beman::execution::sync_wait`.

## Verification evidence

Pins used throughout (both gcc-16 and clang-23):

```
make TOOLCHAIN=gcc-16 test
  100% tests passed out of 222 (Total Test time (real) = 11.91 sec)
make TOOLCHAIN=clang-23 test
  100% tests passed out of 222 (Total Test time (real) =  9.74 sec)
```

222 = the S00 baseline of 220 + this step's 2 new tests
(`freer_beman_probe - sync_wait(just(42))`,
`freer_beman_probe - task co_return smoke`), both `Passed` under both
toolchains (confirmed by name in the ctest log, test #146/#147 in both
runs). Both runs used the default `CONFIG=Asan`, i.e. the probe compiled
and ran under `-fsanitize=address,undefined,leak` on both toolchains —
no sanitizer findings.

Fresh-configure proof (`make reconf`, gate requirement): ran
`make TOOLCHAIN=gcc-16 reconf` from a wiped build tree; it re-ran vcpkg
install for Catch2, freshly cloned `execution-src` and `task-src` via
FetchContent, and configured clean (`Configuring done` / `Generating
done`, no errors). `make TOOLCHAIN=gcc-16 test` afterward was the
222/222 run above.

vcpkg mode: `vcpkg` is on this machine's `PATH`
(`/home/sdowney/bld/vcpkg/master/vcpkg`), so the Makefile picked vcpkg
mode by default — the `make TOOLCHAIN=<pin> test` runs above **are** the
vcpkg-mode gate (Catch2 via vcpkg, beman deps via
`cmake/beman-deps.cmake`'s FetchContent, both coexisting; `vcpkg install`
step ran and installed `catch2:x64-linux-custom@3.11.0` cleanly every
configure).

FetchContent-fallback mode (checked separately, outside the real build
tree, since `_build_path` doesn't vary with vcpkg-mode so I couldn't get
the Makefile to pick the fallback with vcpkg still on PATH without
clobbering the real build dir): reproduced the Makefile's exact `cmake`
invocation by hand with `CMAKE_PROJECT_TOP_LEVEL_INCLUDES=
"infra/cmake/use-fetch-content.cmake;<repo>/cmake/beman-deps.cmake"` and
no `VCPKG_*` args, gcc-16 toolchain file, into a scratch build dir.
Configured clean (Catch2 fetched via `infra/cmake/use-fetch-content.cmake`'s
lockfile provider, beman deps declared via `cmake/beman-deps.cmake`).
Built `freer_beman_probe_test` under `CONFIG=Asan`: fully linked, ran,
`All tests passed (4 assertions in 2 test cases)`. Confirms the fallback
include "fires" for these deps even though it's not the mode selected by
default on this machine.

`make lint` (pre-commit -a): all hooks passed (trim trailing whitespace,
end-of-file fixer, check-yaml, check-added-large-files, clang-format,
CMake linting/gersemi, markdownlint, codespell, Makefile linter, mbake
validate, gitleaks, shellcheck).

## Cross-compiler divergences

None observed. Both gcc-16 and clang-23 auto-enable
`beman::execution`'s C++-modules build path (`BEMAN_USE_MODULES=ON`,
logged at configure time) — gcc-16 ≥ 15 and clang-23 ≥ 19 both trip
`bemanproject/execution`'s own `cmake/cxx-modules-rules.cmake` autodetect
under the Ninja generator this repo uses. This did **not** cause any
divergence or failure on either compiler because of the `EXCLUDE_FROM_ALL`
+ header-only-target choice below (same behavior both toolchains).

## Deviations from the plan / design

None against `docs/freer-signature-functor-design.org` — its only claim
about this step (§5 forward reference: "beman.execution/beman.task are
not yet provisioned in this repo (S07d)") is neutral and unaffected. No
`ops/freer/DEVIATIONS.md` row added.

Two deviations from the step file's literal wording, both because reality
required it, recorded here instead (per AGENT_PROTOCOL step 3, "do the
smallest thing that works and record why"):

1. **`FetchContent_MakeAvailable()` cannot live in the top-level-include
   file itself.** `CMAKE_PROJECT_TOP_LEVEL_INCLUDES` runs as the *last
   step inside* the outer `project()` command's own processing.
   `FetchContent_MakeAvailable()` calls `add_subdirectory()` →
   `project()` for each fetched dep (both `bemanproject/execution` and
   `bemanproject/task` declare their own `project()`), and CMake rejects
   that as a recursive `project()` call from this context:
   `CMake Error ... project(): Language 'CXX' is currently being enabled.
   Recursive call not allowed.` (verbatim, reproduced on first attempt).
   Fix: `cmake/beman-deps.cmake` only calls `FetchContent_Declare()`
   (metadata only, safe anywhere); `FetchContent_MakeAvailable()` moved
   to `src/smd/fixpoint/CMakeLists.txt`, which runs well after the
   top-level `project()` call has returned.
2. **beman-execution is pinned to the commit beman.task's own CMakeLists
   fetches, not to execution's own `main` HEAD.** Neither repo tags
   releases (`git ls-remote --tags` on both is empty — confirmed, not a
   lookup failure), so a "tag" pin is not available; the step file
   anticipated commit pins as the fallback ("Pin exact tags/commits, not
   branches"). First attempt pinned `execution` independently to its own
   `main` HEAD (`4ed789050f3ce9b7a7b7ffec23f7dc52a7364227`) alongside
   `task`'s `main` HEAD
   (`167918522e1cc4bc44c04f97b204f46c1735d7a2`) — this **fails to
   compile**: `task`'s `main` HEAD calls
   `beman::execution::affine_on(...)`, but `execution`'s `main` HEAD at
   that commit has renamed it to `affine_t` (verbatim gcc-16 error:
   `error: 'affine_on' is not a member of 'beman::execution'; did you
   mean 'affine_t'?  [-Wtemplate-body]`, in
   `beman/task/detail/promise_type.hpp:79`). The two repos' `main`
   branches drift independently and are not kept in sync with each
   other. Fix: `task`'s own top-level `CMakeLists.txt` already
   `FetchContent_Declare(execution ... GIT_TAG 7df0f75 ...)` as *its*
   dependency — resolved that short hash to the full SHA
   (`7df0f75d493faf2097db4a392b74d2c6be031847`) and pinned our own
   `execution` declare to the exact same commit, guaranteeing the pair is
   mutually consistent. (This also sidesteps any FetchContent
   re-declaration conflict, since our declare and task's internal
   `FIND_PACKAGE_ARGS`-guarded declare now agree.)

## Discoveries affecting later steps

- **Target names / include spellings (S07 writes against these):**
  - `#include <beman/execution/execution.hpp>` — `beman::execution_headers`
    (INTERFACE, header-only, always available regardless of
    `BEMAN_USE_MODULES`). This is the target to link; do **not** link
    `beman::execution` (only exists when `BEMAN_USE_MODULES=ON`, aliases
    the C++-modules STATIC build of the whole library — unnecessary for
    header-mode usage and drags in the modules build if anything actually
    depends on it).
  - `#include <beman/task/task.hpp>` — `beman::task` (STATIC, compiled
    from `task.cpp`, links `beman::task_headers` → `beman::execution_headers`
    transitively; no modules involved at all for task, even though
    `BEMAN_USE_MODULES` is globally `ON` for execution).
  - Namespace: `beman::execution` (aliased `ex` in the probe: `namespace
    ex = beman::execution;`). `ex::sync_wait(ex::just(42))` returns
    something `std::optional`-shaped with `.has_value()`/`.value()`,
    and `.value()` structured-bindings as `auto [v] = ...value();`
    (single-value completion). `ex::task<int>` is a coroutine type;
    `co_return 42;` works as expected; `sync_wait` also accepts a task
    directly.
  - There is also `include/beman/execution26/execution.hpp` in the
    execution repo (an alternate/newer namespace path) — not used here,
    not verified; `beman/execution/execution.hpp` (the one used by
    beman.task itself) is what the probe uses and what S07 should use
    too unless there's a reason to switch.
- **Both provisioning modes work** (see Verification evidence). vcpkg has
  no `beman-execution`/`beman-task` port in the registry this repo
  resolves against as of this check (2026-07-11) — not exhaustively
  proven (no `vcpkg search` run; inferred from: upstream repos don't tag
  releases, which vcpkg ports normally require, and the FetchContent path
  was already mandated by the step file as "likely"). If S07 or a later
  step wants to double check, `vcpkg search beman` against this repo's
  registry config would confirm directly.
- **Compile-time cost (for S07's iteration loop):** thanks to
  `EXCLUDE_FROM_ALL` + linking only `beman::execution_headers`/
  `beman::task` (never the modules-based `beman::execution` alias), the
  ~200-file C++-modules build of beman.execution is **never** built —
  confirmed by ninja step counts staying small (8 build steps total for
  a from-scratch scratch-project probe: dyndep scans + `task.cpp.o` +
  probe `.o` + link, nothing module-related). A single-TU incremental
  rebuild of `freer_beman_probe.t.cpp` (touching only that file, gcc-16,
  Asan) took **~8s wall** (`time cmake --build ... --target
  freer_beman_probe_test`) — almost all of that is the header-only
  `execution.hpp`'s template weight, not linking. Full from-scratch
  `make reconf` (clone + configure both deps) added a handful of seconds
  on top of the existing baseline configure time; not separately
  measured in isolation but did not make `reconf` noticeably slow.
  **If S07 links `beman::execution` (the modules alias) instead of
  `beman::execution_headers`, expect a much larger one-time cost** (the
  ~200 `.cppm` compilation units) — recommend S07 stay on the
  header-only target unless it has a specific reason to need the
  compiled-module form.
- **`BEMAN_USE_MODULES` becomes a persistent CACHE variable** once
  `execution`'s CMakeLists processes (it's an `option()`), set `ON` for
  both this repo's pinned toolchains under the Ninja generator. This is
  harmless for header-only usage but worth knowing if S07 (or a future
  step) adds any target that conditionally branches on
  `BEMAN_USE_MODULES` itself — it will see `ON` on both pins here, not
  `OFF`.
- `cmake/beman-deps.cmake`'s own header comment has the full narrative
  (recursive-`project()` avoidance, the execution/task pin-consistency
  rationale, `EXCLUDE_FROM_ALL` rationale) — read it before touching that
  file.

## Forward notes for the NEXT step (written after reading its step file)

S07 (`ops/freer/steps/07-sr-interpreter.md`) builds `mcata_free.hpp` and
`freer_task.hpp`/`freer_task.t.cpp` on top of this:

- Link `beman::execution_headers` + `beman::task` (same two targets the
  probe uses) into `freer_task.t.cpp`'s test executable — follow the
  `freer_beman_probe_test` wiring in `src/smd/fixpoint/CMakeLists.txt`
  as the template (own executable, not folded into `smd_fixpoint_test`),
  and keep `beman::execution`/`beman::task` **out of** `fixpoint.fixpoint`
  and out of `mcata_free.hpp` itself — the step file already says "Keep
  Beman::execution usage inside freer_task.hpp/its test; the core headers
  must not grow the dependency," which matches exactly how this step left
  things (only the probe target links beman; `fixpoint.fixpoint` is
  untouched).
- `sync_wait` returns an optional-like value; unwrap with
  `.has_value()`/`auto [v] = ...value();` as shown in
  `freer_beman_probe.t.cpp` — S07's "value matches S04's synchronous run"
  assertions will need this unwrapping pattern.
- For the "deliberately-async smoke" test (S07 Do item 3, last bullet):
  the probe here only exercised `just()` (already-ready) and a task
  `co_return` (no suspension across a real scheduling point) — S07 is
  genuinely new ground for scheduler-hop behavior; nothing here de-risks
  that specifically, but the header/target wiring is confirmed solid up
  to and including coroutine (`task<T>`) usage.
- `EXCLUDE_FROM_ALL` on both `FetchContent_Declare`s in
  `cmake/beman-deps.cmake` is load-bearing for build-cost reasons (see
  Discoveries) — if S07 ever needs the modules-based `beman::execution`
  target specifically (unlikely, since header-only works fine per the
  probe), it will need to explicitly reference that target somewhere to
  pull it into the build graph; don't remove `EXCLUDE_FROM_ALL` without
  updating the cost expectations in this handoff.
- Namespace alias convention used in the probe: `namespace ex =
  beman::execution;` — reasonable to reuse in `freer_task.hpp` for
  brevity, though S07 should follow whatever the design doc's §5 section
  or house style prefers if that's more specific.

## Open risks / TODOs

- vcpkg-port absence for beman-execution/beman-task was not confirmed via
  an actual `vcpkg search`/registry query (see Discoveries) — low risk
  (upstream doesn't tag releases, a hard blocker for a normal vcpkg port
  regardless of whether one nominally exists), but worth a quick double
  check if the orchestrator wants it airtight before S08's integration
  writeup.
- Pinned commits are `main`-branch snapshots as of 2026-07-11, not
  tagged releases — both repos are explicitly "under development," so a
  future re-pin is expected; nothing in this step depends on staying
  current, but S07/S08 should not assume these SHAs track upstream
  fixes.
- `beman::execution26/` namespace variant (see Discoveries) is unexplored
  — if S07 or the design doc's §5/§6 work ever wants the newer namespace
  path, that's untested ground.
