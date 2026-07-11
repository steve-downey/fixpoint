# S00 — Toolchain pins, baseline capture, feature probe

**Goal.** Pin the two primary toolchains for the whole plan, capture
the pre-existing test baseline under both, and probe
`__cpp_lib_move_only_function` per toolchain (input to decision D-B).
No library changes.

**Depends on:** nothing.
**Design refs:** FD9 (toolchain floor paragraph), FD12.

## Do

1. **Pin GCC.** Confirm `g++-16` is on PATH and
   `etc/gcc-16-toolchain.cmake` exists (both expected — the
   recursion-schemes plan used them). If gcc-16 is absent, use the
   newest gcc-NN that has both, and record the substitution.
2. **Pin Clang.** Find the newest `clang++-NN` on PATH that has an
   `etc/clang-NN-toolchain.cmake`. Expected: clang-22 or newer
   (`etc/` has files up to clang-23).
3. **Baseline.** Run `make TOOLCHAIN=<gcc-pin> test` and
   `make TOOLCHAIN=<clang-pin> test` on an untouched tree. Record
   total/passed per toolchain. If the *pre-existing* suite fails
   under either pin: do not fix anything — record the failures
   verbatim in the handoff with Status BLOCKED and stop; the
   orchestrator decides (repin vs. investigate).
4. **Feature probe.** For each pinned toolchain, compile a scratch TU
   (scratch dir, not committed) printing/`static_assert`ing
   `__cpp_lib_move_only_function` and, separately, whether
   `std::move_only_function<int(int) &&>` is well-formed. Record the
   macro value (or absence) per toolchain, plus the same for the
   libc++ configuration if the Clang pin can build against libc++
   locally (`-stdlib=libc++` probe compile; skip if libc++ not
   installed, and say so).
5. **Record.** Fill the "expected pins" in `ops/freer/PLAN.md`'s
   Build & test section with the actuals, and add the S00 Status-log
   row with both baseline counts.

## Build

No library build changes. Gate commands above.

## Verify (gate)

Both pinned toolchains: pre-existing suite 100% green. Probe results
recorded per toolchain in the handoff.

## Done when

Gate green; PLAN.md pins + Status row landed; committed
`[freer] S00: toolchain pins + baseline` and the handoff commit.

## Capture in handoff

The exact pin names and baseline counts (every later step's gate
cites them); the macro values per toolchain/stdlib (D-B input); any
substitution from the expected pins.

## Pitfalls

- `make TOOLCHAIN=...` expects the versioned compiler on PATH; check
  `etc/<name>-toolchain.cmake` exists before concluding a compiler is
  unusable.
- Build trees are per-toolchain under `.build/`; a red run on one
  toolchain doesn't touch the other's tree.
- Do not "fix" a red baseline. Recording it is the deliverable.
