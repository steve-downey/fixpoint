# Handoff — S00 Toolchain pins, baseline capture, feature probe

- **Status:** DONE (gate passed)
- **Commit:** 88072d8 (`[freer] S00: toolchain pins + baseline`)
- **Date / agent:** 2026-07-11, Sonnet worker

## What changed

- `ops/freer/PLAN.md`: filled in the "Build & test" section with the
  actual toolchain pins, baseline test counts, and the
  `__cpp_lib_move_only_function` probe results (D-B input). No library
  code touched — this step is ground-truth capture only.

## Verification evidence

Toolchain pins (confirmed, no substitution needed):

- **Primary GCC: gcc-16.** `g++-16` on PATH; `etc/gcc-16-toolchain.cmake`
  exists.
- **Primary Clang: clang-23.** `clang++-23` on PATH;
  `etc/clang-23-toolchain.cmake` exists. `etc/` has clang toolchain
  files 11..23; binaries on PATH are clang++-17..23. clang-23 is the
  newest with both, so it is the pin (matches the dispatch prompt's
  expectation).

Baseline (untouched tree, i.e. before the S00 commit):

```
make TOOLCHAIN=gcc-16 test
  100% tests passed out of 220 (Total Test time (real) = 7.66 sec)

make TOOLCHAIN=clang-23 test
  100% tests passed out of 220 (Total Test time (real) = 6.00 sec)
```

Both full CTest runs are 220/220 green, no failures, no skips. Raw
logs kept only in the scratch dir (not committed):
`/tmp/claude-1000/-home-sdowney-src-fixpoint-main/577637cb-72a1-4600-855b-c939bd778de4/scratchpad/s00-gcc16-baseline.log`
and `.../s00-clang23-baseline.log`.

`make lint` (pre-commit -a): all hooks passed (trim trailing
whitespace, end-of-file fixer, check-yaml, check-added-large-files,
clang-format, CMake linting/gersemi, markdownlint, codespell, Makefile
linter, mbake validate, gitleaks, shellcheck).

### Feature probe: `__cpp_lib_move_only_function`

Scratch TU (not committed):
`/tmp/claude-1000/-home-sdowney-src-fixpoint-main/577637cb-72a1-4600-855b-c939bd778de4/scratchpad/probe_mof.cpp`
— prints the macro value and, when defined, constructs and calls a
`std::move_only_function<int(int) &&>` (consumed by an rvalue-qualified
call operator).

| Toolchain | stdlib | `__cpp_lib_move_only_function` | `move_only_function<int(int)&&>` well-formed |
|---|---|---|---|
| gcc-16 (`-std=c++23`) | libstdc++ | `202110L` | yes — compiled and ran, result 42 |
| clang-23 (`-std=c++23`) | libstdc++ (toolchain default) | `202110L` | yes — compiled and ran, result 42 |
| clang-23 (`-std=c++23 -stdlib=libc++`) | libc++ (libc++-23-dev installed locally) | **not defined** (0) | **no** — hard compile error |

libc++ detail (also scratch, not committed:
`probe_mof_nomacro.cpp`): even naming
`std::move_only_function<int(int) &&>` directly (no macro guard) fails
under `clang++-23 -stdlib=libc++`:

```
probe_mof_nomacro.cpp:7:22: error: no template named 'move_only_function' in namespace 'std'
    7 | using probe_t = std::move_only_function<int(int) &&>;
      |                 ~~~~~^
```

`/usr/lib/llvm-23/include/c++/v1/version` lists
`__cpp_lib_move_only_function 202110L <functional>` in its
documentation table (line 183) but the actual `# define` for it
(line 519 area) is commented out:
`// # define __cpp_lib_move_only_function 202110L`. So under this
locally installed libc++ (LLVM 23 packaged build), the type is not
just untagged by the feature macro — it is not implemented at all.
libc++-23-dev IS installed locally (`dpkg -l | grep libc++` shows
`libc++-23-dev`, `libc++1`, `libc++abi-23-dev`, `libc++abi1`), so this
is a real, not a "libc++ absent" result.

## Cross-compiler divergences

None on the primary pins (gcc-16 / clang-23, both against libstdc++):
identical macro value, identical well-formedness, identical baseline
pass counts (220/220 each). The only divergence found is
stdlib-driven, not compiler-driven: clang-23 against libc++ diverges
sharply from clang-23 against libstdc++ (feature present vs. type
literally undeclared) on the *same* compiler binary.

## Deviations from the plan / design

FD9's toolchain-floor paragraph states "libc++ shipped
`std::move_only_function` only in LLVM 20" (implying it's present from
LLVM 20 on, modulo feature-test tagging). Reality found at LLVM 23
(the libc++-23-dev package installed here) is stronger than "present
but untagged": the symbol does not exist under `namespace std` at all
when compiling against libc++. Recorded as `DEV-S00-1` in
`ops/freer/DEVIATIONS.md`. This is direct evidence for FD12's decision
rule — if the Freer layer's compiler matrix must include libc++, guard
against believing "add the macro check" alone is sufficient; libc++
may need the bespoke `one_shot<Sig>` type instead, or a version-gated
libc++ exclusion. Since the plan's two *primary* pins (gcc-16,
clang-23-with-libstdc++) both have full support, this does not block
S01 — it is evidence for the orchestrator's D-B decision once S01's
probe (c) results are in.

## Discoveries affecting later steps

- Both primary pins fully support `std::move_only_function` with an
  `&&`-qualified call operator, feature-macro-gated at `202110L`. S01's
  probe (c) should compile cleanly on both primary pins.
- If the orchestrator or a later step ever widens the Clang pin's
  matrix to include `-stdlib=libc++` (e.g. for CI parity or the paper's
  cross-compiler evidence in FD9), that configuration will need either
  a feature-macro guard that *excludes* the header entirely (not just
  tags it) or the bespoke `one_shot<Sig>` fallback from FD12 — the
  macro being merely "0/undefined" behaves correctly here (code can
  `#if defined(__cpp_lib_move_only_function)` and fall back), so the
  gating strategy in FD12's decision rule is directly actionable
  against this libc++ configuration if it's ever probed again.
- Scratch probe sources, if useful as a starting point for S01's probe
  (c), live at (uncommitted, may be gone in a future session; the
  content above is a full transcript so no need to reread them):
  `/tmp/claude-1000/-home-sdowney-src-fixpoint-main/577637cb-72a1-4600-855b-c939bd778de4/scratchpad/probe_mof.cpp`

## Forward notes for the NEXT step (written after reading its step file)

S01 (`ops/freer/steps/01-baseline-gate.md`) builds one new test file
`src/smd/fixpoint/freer_baseline.t.cpp` wired into the existing
fixpoint test target, with test-local `Get`/`Put`/`impure_node`/`KVSig`
types (not the real headers, which don't exist until S03). Notes for
that work:

- Use the pins as-is: `make TOOLCHAIN=gcc-16 test` and
  `make TOOLCHAIN=clang-23 test`; no toolchain surprises expected —
  both primary pins are libstdc++-backed and both have
  `std::move_only_function` with `&&`-qualified call operators fully
  working (see probe table above). Probe (c) in S01 should be
  straightforwardly green on both; do not expect to need a
  compiler-conditional there unless something new surfaces.
- Existing house style confirmed by skimming `free.hpp`/`free.t.cpp`:
  SPDX header comment line 1-2, `-*-C++-*-` marker, include-guard style
  `INCLUDED_SMD_FIXPOINT_<NAME>`, a `// <uuid> ... // <uuid> end`
  transclusion-anchor comment pair wrapping the "real" code block (used
  by the presentation build) is present in headers that need it —
  S01's new file is a test file, so check whether `free.t.cpp` (no
  such anchors, plain Catch2 `TEST_CASE`s) or an anchored header is the
  closer template; `free.t.cpp` looks like the right model since S01
  only adds a test file, not shipped header content. First `TEST_CASE`
  in `free.t.cpp` is a trivial `"free - HeaderIsIdempotent"` smoke
  test via double-`#include`; S01's new file including test-local types
  only (no new header) doesn't need that pattern, but check the
  CMakeLists wiring convention (`target_sources(... FILE_SET ...
  TYPE HEADERS ...)` for headers; a `_test` executable via
  `catch_discover_tests` for `.t.cpp`) in
  `src/smd/fixpoint/CMakeLists.txt` before adding the new file — S01's
  file must be *added to the existing fixpoint test target*, not given
  its own new executable.
- `overloaded.hpp`'s `smd::fixpoint::overloaded` helper (used
  throughout `free.hpp` for `std::visit`) is a ready-made pattern if
  S01's hand-rolled resume/probe code needs to visit the local
  `impure_node`/`Free`-shaped variant.
- D-A probes (a)/(b) in S01 are genuinely new ground (member-template
  deduction against `functor_typeclass`, and a constrained
  variable-template partial specialization keyed on a concept over
  nested typedefs) — S00 did not touch typeclass registration at all,
  so there's no forward evidence from this step beyond "the compilers
  agree on everything tried so far."

## Open risks / TODOs

- The libc++ gap (DEV-S00-1) is unresolved risk for FD9's stated CI
  matrix (which references clang-19/libc++ and appleclang rows) — it
  is out of scope for S00/S01 (primary pins only) but the orchestrator
  should be aware it may need explicit libc++-version gating language
  in FD9/FD12 beyond "check the feature macro," since at least LLVM 23
  the macro is deliberately suppressed by libc++ upstream even though
  the type exists in their documentation table (implementation
  presumably incomplete/unvetted upstream, not merely undocumented).
- Did not probe any non-primary toolchain (older gcc/clang) — out of
  S00's scope per the step file ("Oldest supported of each" is
  explicitly deferred to whatever floor decision the orchestrator makes
  from FD9, not S00's job).
