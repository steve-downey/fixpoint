# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A C++ (C++23/26) library project ("Type Level Fixpoint") that serves as Steve Downey's reference
best-practices template. It was generated via [Copier](https://copier.readthedocs.io/) from a
personal template repo (see `.copier-answers.yml`); the code itself is intentionally trivial (a
library returning a name, a test, a `hello` example) so the surrounding build/lint/CI machinery can
be reused quickly for new projects. Don't add real functionality unless asked — the point of this
repo is the scaffolding.

## Build & test

Everything is driven through the top-level `Makefile` (preferred over invoking CMake/CTest
directly). Python tooling (cmake, gcovr, pre-commit, clang-format, mbake, shellcheck) is installed
into a local `.venv` via `uv` automatically — no manual setup needed.

```sh
make               # default target: build + run all tests (same as `make test`)
make compile       # configure + build only
make test          # rebuild and run tests via ctest
make ctest         # run ctest without forcing a rebuild
make lint          # run pre-commit hooks (clang-format, gersemi, markdownlint, codespell, checkmake, gitleaks, shellcheck...)
make lint-manual    # run pre-commit hooks tagged manual-stage
make coverage      # build+test with the Gcov config and produce a coverage report
make view-coverage # open the gcovr HTML report
make install       # install to INSTALL_PREFIX (default .install/)
make testinstall   # install then build/test the installtest/ project against the installed package
make clean         # clean build artifacts (keeps configured build tree)
make reconf        # wipe and reconfigure the build tree
make realclean     # remove build tree, venv, uv.lock, install dir, papers infra, etc.
make help          # list all targets with descriptions
```

Compiler selection and build config are environment/variable driven, not CMake presets (presets
exist mainly to satisfy Beman CI tooling):

```sh
make TOOLCHAIN=gcc-15                       # use etc/gcc-15-toolchain.cmake (versioned compiler must be on PATH, e.g. g++-15)
make TOOLCHAIN=clang-21 CONFIG=RelWithDebInfo
make CONFIG=Tsan                            # thread sanitizer build
```

`CONFIG` defaults to `Asan`. Valid configs (see `_configuration_types` in the Makefile):
`RelWithDebInfo`, `Debug`, `Tsan`, `Asan`, `Gcov`. With no `TOOLCHAIN` set, the system `c++` is
used via `etc/toolchain.cmake`.

### Running a single test

Tests use Catch2 v3, discovered per-file via `catch_discover_tests`. After building, filter with
ctest's regex or invoke the Catch2 binary directly for finer-grained filtering:

```sh
make compile
$(uv run which ctest) --test-dir .build/build-system -C Asan -R fixpoint   # ctest name filter
.build/build-system/src/smd/example/Asan/fixpoint_test "fixpoint returns Steve"  # Catch2 test name
.build/build-system/src/smd/example/Asan/fixpoint_test "[fixpoint]"              # Catch2 tag
```

(Exact binary path depends on `TOOLCHAIN`/`CONFIG` — check `.build/<toolchain-dir>/` after compiling.)

### compile_commands.json

`make compile_commands.json` (a dependency of `compile`) symlinks the active build's
`compile_commands.json` to the repo root for clangd/IDE tooling.

## Architecture

- **Layout**: follows the [Pitchfork Layout](https://api.csswg.org/bikeshed/?force=1&url=https://raw.githubusercontent.com/vector-of-bool/pitchfork/develop/data/spec.bs)
  merged-layout convention. All C++ lives under `src/`, organized as
  `src/<include_prefix>/<library_name>/...` (here `src/smd/example/`), and headers/sources/tests are
  co-located in the same directory rather than split into separate `include/`/`test/` trees.
- **CMake is target- and file-set oriented** (CMake 4.x). The single library target
  `fixpoint.fixpoint` is declared once in the top-level `CMakeLists.txt` and each subdirectory's
  `CMakeLists.txt` appends sources/headers to it via `target_sources(... FILE_SET
  example_fixpoint_headers TYPE HEADERS ...)`. New headers must be added to that file set, not just
  dropped in a directory.
- **Test files** are named `<name>.test.cpp` and live next to the code they test (e.g.
  `src/smd/example/fixpoint.test.cpp` next to `fixpoint.hpp`/`fixpoint.cpp`). Each leaf
  `CMakeLists.txt` builds its own `..._test` executable and registers it with
  `catch_discover_tests`.
- **Dependency provisioning** is dual-mode: if `vcpkg` is on PATH, deps (Catch2, per
  `vcpkg.json`) come from vcpkg using the custom triplet in `cmake/`; otherwise
  `CMAKE_PROJECT_TOP_LEVEL_INCLUDES` falls back to `infra/cmake/use-fetch-content.cmake` to fetch
  Catch2 directly. The Makefile handles selecting the right mode automatically.
- **`infra/`** is vendored from the [Beman Project infra](https://github.com/bemanproject/infra) via
  git subtree (Apache-2.0). It supplies `beman_install_library`, the FetchContent fallback, and is
  used by several GitHub Actions workflows (which call reusable workflows from
  `bemanproject/infra-workflows`). Treat it as third-party/vendored — don't hand-edit unless
  intentionally updating the subtree.
- **`papers/wg21`** is a second vendored git subtree (a generic WG21 paper-writing framework, not
  tied to a specific proposal). Managed via `make papers` / `make clean-papers` /
  `make realclean-papers`, delegating to `papers/wg21`'s own Makefile.
- **Toolchain files** live in `etc/` (`etc/<name>-toolchain.cmake`), one per supported
  compiler/version (gcc-10..16, clang-11..23, llvm variants). Compilers are expected on `PATH` under
  versioned names (e.g. `g++-15`, `clang++-21`); there is no auto-detection.
- **`installtest/`** is a standalone CMake project used only by `make testinstall` to verify the
  installed package (via `find_package`) actually works for a consumer.
- **Presentation build**: `fixpoint.org` + `make presentation` uses Emacs (`org-transclusion` +
  `org-export`, config in `.emacs.d/`) to produce HTML/reveal.js slides that transclude source and
  test code, so the presentation content stays in sync with what actually compiles and passes.
- **CI** (`.github/workflows/ci_tests.yml`) is built on reusable workflows from
  `bemanproject/infra-workflows`, testing a matrix of gcc/clang/appleclang/msvc versions,
  C++23/C++26, libstdc++/libc++, and sanitizer configs. `test_makefile.yaml` separately smoke-tests
  the Makefile-driven workflow itself.

## Linting/formatting

Driven entirely by `pre-commit` (`make lint`): clang-format (C++), gersemi (CMake format/lint),
markdownlint, codespell (ignore list in `.codespell_ignore`), checkmake, mbake (Makefile
validation), gitleaks, shellcheck. Don't hand-format C++/CMake — run `make lint` instead.
