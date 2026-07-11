# Deviations from docs/freer-signature-functor-design.org

Reality wins; the design doc catches up. One row per contradiction
between what a step had to do and what the FD-series says. The
orchestrator reviews each row and folds accepted deviations back into
the design doc (updating the affected FD's status/rationale).

| ID | Step | FD affected | What the design said | What reality required | Handoff |
|----|------|-------------|----------------------|-----------------------|---------|
| DEV-S00-1 | S00 | FD9, FD12 | "libc++ shipped `std::move_only_function` only in LLVM 20" (FD9 toolchain-floor paragraph); FD12 frames libc++ availability as a version-gate problem. | Under the locally installed libc++-23-dev (LLVM 23 packaged build) `std::move_only_function` is not merely untagged by the feature macro — `clang++-23 -stdlib=libc++` rejects `std::move_only_function<int(int)&&>` outright ("no template named 'move_only_function' in namespace 'std'"); `<version>`'s `# define __cpp_lib_move_only_function` line is present but commented out. So at least as of this LLVM 23 snapshot, libc++'s gap is non-implementation, not just a missing feature-test macro. | `ops/freer/handoffs/00-ground.handoff.md` |
