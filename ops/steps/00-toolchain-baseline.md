# S00 — Toolchain to C++26/gcc-16, baseline recorded

**Goal.** Move the project standard to C++26, pin gcc-16 as the gate
toolchain, add a gcc-17 toolchain file for secondary smoke builds,
confirm everything still builds green, and record the baseline. Zero
feature code.

**Depends on:** nothing.
**Design refs:** §3 D9, D10.

## Known-good ground truth (maintainer-provided)
- `make test` (no TOOLCHAIN) uses `etc/toolchain.cmake` → system cc/c++
  (gcc 15.2) and currently passes 45 tests in `.build/build-system`.
- `make TOOLCHAIN=<name> test` uses `etc/<name>-toolchain.cmake` and
  builds in `.build/build-<name>`. `etc/gcc-16-toolchain.cmake` exists.
- All gcc toolchain files include `etc/gcc-flags.cmake`, which
  FORCE-caches `CMAKE_CXX_STANDARD 23` and `-std=gnu++23`.
- Compilers: `/usr/bin/g++-16` (Ubuntu 16.0.1 trunk package),
  `~/.local/bin/g++-17` → `~/install/gcc-17/bin/g++-17` (personal trunk
  build, 20260518). Both accept `-std=c++26`; both ship `std::indirect`.
- vcpkg supplies Catch2; each build dir re-resolves it, and the custom
  triplet detects the compiler per toolchain.

## Do
1. **Bump the standard.** In `etc/gcc-flags.cmake`: `CMAKE_CXX_STANDARD`
   23 → 26 and `-std=gnu++23` → `-std=gnu++26`. In `CMakePresets.json`:
   `"CMAKE_CXX_STANDARD": "23"` → `"26"` (keeps presets consistent with
   the Makefile path even though the gate uses the Makefile).
2. **Add `etc/gcc-17-toolchain.cmake`** modeled on
   `etc/gcc-16-toolchain.cmake`: compilers `gcc-17`/`g++-17`, gcov
   `gcov-17`, and set the rpath to the gcc-17 install's lib64
   (`~/install/gcc-17/lib64` — verify with
   `g++-17 -print-search-dirs | head -3`).
3. **Primary gate build:** `make TOOLCHAIN=gcc-16 test`. Everything must
   pass (45 tests). This also proves Catch2 rebuilds cleanly under the
   new compiler/standard.
4. **Secondary smoke:** `make TOOLCHAIN=gcc-17 compile` (compile is
   enough; run ctest too if it just works). Record the result either
   way — if gcc-17 fails for environmental reasons, note it as baseline
   and move on; gcc-17 is advisory.
5. **Sanity-check C++26 library availability in-tree:** add a temporary
   scratch file (do NOT commit it) including `<expected>` and checking
   `__cpp_lib_expected` and `__cpp_lib_indirect` compile under the
   gcc-16 build flags, e.g. via
   `g++-16 -std=gnu++26 -fsyntax-only <file>`.
6. **Confirm the default build still works:** `make test` (gcc-15 with
   gnu++26 flag). Expected to pass since no C++26 *library* feature is
   in use yet. If it fails, record exactly why in the handoff — the
   plan's gate remains gcc-16 regardless (design D9).

## Verify (gate)
- `make TOOLCHAIN=gcc-16 test` → 100% tests passed, 0 failed, total 45.
- `etc/gcc-17-toolchain.cmake` exists and `make TOOLCHAIN=gcc-17
  compile` result is recorded.
- `git grep -n "gnu++23" etc/` comes back empty.

## Done when
Gate green under gcc-16 at C++26; baseline counts and exact commands
recorded; committed as `[schemes] S00: toolchain to C++26/gcc-16`.

## Capture in handoff
Exact test count and the ctest invocation the Makefile ran; the build
dir path (`.build/build-gcc-16`); the example-binary path pattern
(`.build/build-gcc-16/src/examples/Asan/<name>`); gcc-17 status; any
vcpkg/ccache surprises; whether the default gcc-15 build stayed green.

## Pitfalls
- `gcc-flags.cmake` uses `CACHE ... FORCE` — a stale build dir will
  still reconfigure correctly, but if anything looks cached-wrong,
  delete `.build/build-gcc-16` and reconfigure rather than debugging
  cache interactions.
- Do not touch `infra/` (it is shared beman infrastructure) or the
  GitHub workflows — CI is out of scope (design §11).
- The uv/venv warning about `VIRTUAL_ENV` mismatch is pre-existing
  noise; ignore it.
