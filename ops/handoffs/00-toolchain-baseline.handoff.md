# Handoff — S00 Toolchain to C++26/gcc-16, baseline recorded

- **Status:** DONE (gate passed)
- **Commit:** e96f02e — `[schemes] S00: toolchain to C++26/gcc-16`
- **Date / agent:** 2026-07-03, background execution agent

## What changed
- `etc/gcc-flags.cmake`: `CMAKE_CXX_STANDARD` 23 → 26; `CMAKE_CXX_FLAGS`
  `-std=gnu++23` → `-std=gnu++26`.
- `CMakePresets.json`: `_root-config.cacheVariables.CMAKE_CXX_STANDARD`
  `"23"` → `"26"`.
- New `etc/gcc-17-toolchain.cmake`, modeled 1:1 on
  `etc/gcc-16-toolchain.cmake`: `CMAKE_C_COMPILER gcc-17`,
  `CMAKE_CXX_COMPILER g++-17`, `GCOV_EXECUTABLE gcov-17`, rpath set to
  `~/install/gcc-17/lib64` (resolved via
  `g++-17 -print-search-dirs | head -3` → `libraries: =/home/sdowney/install/gcc-17/lib/gcc/...`
  and the sibling `lib64` directory), and the same
  `-Wno-maybe-uninitialized` Asan-flags addition as gcc-16. No feature
  code touched.

## Verification evidence
- `make TOOLCHAIN=gcc-16 test` → configured+built+ctest clean:
  **100% tests passed, 0 tests failed out of 45** (ctest invoked via
  `uv run ctest --test-dir .build/build-gcc-16 --output-on-failure -C
  Asan`). Build dir: `.build/build-gcc-16`. Example binaries live at
  `.build/build-gcc-16/src/examples/Asan/<name>` (confirmed `hello` and
  `fixpoint_tree_example` both link and are present there).
- `make TOOLCHAIN=gcc-17 compile` → configure + full Ninja build
  (46/46 targets in the `[N/47]` progress log — one step is metrics
  collection, not a build target) completed with no compiler
  errors/warnings-as-errors under `-std=gnu++26`. ctest was not
  separately re-run for gcc-17 (advisory per plan); compile-only
  satisfies the step's minimum bar. Build dir: `.build/build-gcc-17`.
- `make test` (no TOOLCHAIN, default gcc-15 `c++`/`cc`, still under the
  now-bumped `gnu++26` flag) → **100% tests passed, 0 tests failed out
  of 45**. The default build stayed green; no C++26 library feature is
  in use yet, as expected.
- C++26 library sanity check: scratch file (not committed, deleted
  after use) with `#include <expected>`, `#include <memory>`, and
  `static_assert(__cpp_lib_expected >= 202202L)`,
  `static_assert(__cpp_lib_indirect >= 202405L)`, compiled clean via
  `g++-16 -std=gnu++26 -fsyntax-only <file>`. Both feature-test macros
  are satisfied under gcc-16's C++26 mode.
- `git grep -n "gnu++23" etc/` → empty (confirmed after edits).

## Deviations from the plan / design
None. Everything in the step's "Do" list executed as written; no design
contradiction encountered. No row added to `ops/DEVIATIONS.md`.

## Discoveries affecting later steps
- gcc-17's install layout: `~/install/gcc-17/lib64` exists as a sibling
  of `~/install/gcc-17/lib/gcc/x86_64-pc-linux-gnu/17.0.0/`; the rpath
  in `etc/gcc-17-toolchain.cmake` points there and the gcc-17 compile
  build linked successfully with it.
- `gcc-16` and default `c++` (gcc-15.2 Ubuntu) both accept
  `-std=gnu++26` without complaint and both currently pass the full
  suite — no C++26-only syntax has been exercised yet, so this doesn't
  yet prove gcc-15 compatibility for future steps, just that the flag
  bump alone is harmless.
- vcpkg/Catch2 rebuilt cleanly from scratch for the gcc-17 build dir
  (fresh `.build/build-gcc-17`, ~2.2 min for the vcpkg catch2 build);
  no ccache or vcpkg surprises for either toolchain.
- The `uv`/`VIRTUAL_ENV` mismatch warning appears on every `make`
  invocation as documented in the step's pitfalls — cosmetic, ignore.

## Forward notes for the NEXT step (S01 — layer_fmap + lookup overloads)
- S01 depends only on S00, which is now satisfied.
- S01 adds `src/smd/fixpoint/fmap.hpp` and a new
  `src/smd/fixpoint/fmap.t.cpp`; both need wiring into the
  `smd_fixpoint_headers`/`smd_fixpoint_test` FILE_SETs in
  `src/smd/fixpoint/CMakeLists.txt` (confirmed this file exists and is
  where `fix.hpp`/`fix.t.cpp`, `box.hpp`/`box.t.cpp`,
  `recursion_schemes.hpp`/`.t.cpp`, `overloaded.hpp`/`.t.cpp` are
  listed — follow that exact pattern for the new files).
- Existing style reference confirmed in `src/smd/fixpoint/fix.hpp` /
  `fix.t.cpp`: SPDX header line
  `// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception`, include
  guards, and `.t.cpp` files include a re-inclusion/header-idempotency
  Catch2 test (see `overloaded.t.cpp`'s "overloaded -
  HeaderIsIdempotent" test at line ~12 of the ctest list above) — S01's
  `fmap.t.cpp` should include the same idempotency pattern per house
  style even though the step file doesn't call it out explicitly.
- `src/smd/typeclass/functor.hpp` and its `functor_typeclass<...>`
  variable-template pattern already exist and pass tests (see
  `smd_typeclass_test` target, tests like "functor: fmap over
  optional"/"functor: fmap over vector") — S01 reopens
  `namespace smd::typeclass` from the fixpoint module to add a
  `NatF`-keyed specialization; this is a new cross-module pattern not
  yet exercised anywhere in-tree, so budget extra care/time for it per
  the step's own pitfalls section.
- Both `fold_fix`/`unfold_fix`/`refold` (3 existing explicit-fmap
  overloads in `src/smd/fixpoint/recursion_schemes.hpp`) need to be
  marked `constexpr` as part of S01 (the one sanctioned edit to
  existing code) — confirm this compiles clean under gcc-16 C++26 and
  record the result in the S01 handoff as the step file requests.
- The toolchain is now C++26/gcc-16 end-to-end (`.build/build-gcc-16`),
  so S01's `static_assert`-based constexpr test can rely on
  `std::indirect`/`std::expected` being available if ever needed later,
  though S01 itself doesn't require them (it uses `Box`, per D10).

## Open risks / TODOs
- gcc-17 was only smoke-tested with `compile`, not `ctest`; if a later
  step's handoff needs actual gcc-17 *test* results, that's still
  outstanding (the plan treats gcc-17 as advisory only, so this is not
  blocking).
