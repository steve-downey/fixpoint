# S16 — Umbrella header, usage docs, final sweep

**Goal.** Package the catalog: one umbrella include, a usage-catalog
document, README pointer, and a final consistency pass over the whole
plan's output.

**Depends on:** S01–S15 (S15's capstone may be BLOCKED; package what
shipped).
**Design refs:** §8, §10, §11.

## Do
1. **`src/smd/fixpoint/schemes.hpp`** — umbrella header including every
   scheme header (§8 table), wired into the FILE_SET. A short header
   comment maps scheme names to headers. Test: `schemes.t.cpp` includes
   it twice and calls one scheme from each family (compile-coverage,
   a handful of trivial asserts).
2. **`docs/recursion-schemes.md`** — the user-facing catalog, one
   section per scheme: signature as landed, one-sentence "when you want
   it", the recursive equation, a ~10-line usage snippet lifted from
   the scheme's example file, and a pointer to the example executable.
   Order follows design §7. Source the exact signatures from the
   headers, not from the design doc (they may have drifted via
   deviations — the deviations ledger tells you where).
3. **README.md** — add a short "Recursion schemes" section: what the
   library now contains, the umbrella include, how to build and run the
   examples (`make TOOLCHAIN=gcc-16 test`, path pattern), link to
   `docs/recursion-schemes.md` and the design doc.
4. **Consistency sweep**:
   - every header in §8's table exists, is in a FILE_SET, has a
     `.t.cpp` in a test target; every §10 example builds, runs, exits 0
     (run them all, capture output);
   - `make TOOLCHAIN=gcc-17 compile` — record result (advisory);
   - header hygiene: SPDX lines, include guards, no stray
     `#include <iostream>` (examples use `<print>`);
   - `ops/DEVIATIONS.md` rows each state a recommended doc change —
     flag any that the design author hasn't reconciled (do NOT edit the
     design doc's normative sections yourself; §1's process note says
     the author reconciles).
5. **Optional, only if trivially green:** run the repo's pre-commit
   hooks (`uv run pre-commit run --all-files`) and fix mechanical
   findings (whitespace, codespell) in files this plan created. Skip
   wholesale reformatting of anything else.

## Build
`make TOOLCHAIN=gcc-16 test`; run every example binary.

## Verify (gate)
- Full suite green (record the final test count against S00's 45).
- All examples run and exit 0.
- Umbrella test compiles clean including the double-include check.

## Done when
Gate green; committed `[schemes] S16: packaging + docs`. The plan is
complete — say so in the handoff, with the final tally (schemes
shipped, tests added, deviations filed, anything BLOCKED).

## Capture in handoff
Final test count; example inventory with one output line each; the
open-items list (unreconciled deviations, BLOCKED capstone if any);
suggested follow-ups explicitly out of scope (§11: std::indirect
migration, stack safety, performance).

## Pitfalls
- The docs catalog goes stale the moment it's written if it transcribes
  the *design* doc instead of the *headers* — copy signatures from
  code.
- Don't let the sweep balloon: fix what the checklist names, file the
  rest as handoff notes.
