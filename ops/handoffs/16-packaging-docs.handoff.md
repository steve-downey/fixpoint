# Handoff — S16 Umbrella header, usage docs, final sweep

- **Status:** DONE (gate passed) — **the plan is complete**
- **Commit:** bf6ec8a — `[schemes] S16: packaging + docs`
- **Date / agent:** 2026-07-05, background execution agent

## What changed

- New `src/smd/fixpoint/schemes.hpp`: umbrella header, `#include`s every
  scheme header plus the supporting typeclass headers (`fmap.hpp`,
  `functors.hpp`, `para.hpp`, `apo.hpp`, `zygo.hpp`, `mutu.hpp`,
  `prepro.hpp`, `cofree.hpp`, `histo.hpp`, `free.hpp`, `futu.hpp`,
  `chrono.hpp`, `mendler.hpp`, `elgot.hpp`, `dist_laws.hpp`,
  `generalized.hpp`, `recursion_schemes.hpp`, plus
  `smd/typeclass/{identity,either,pair,comonad}.hpp`). Doc comment maps
  every scheme/type name to its header, organized by design §7 section
  number, with an explicit note that `generalized.hpp` holds
  gcata/gana/ghylo/gprepro/gpostpro/zygo_histo_prepro together rather than
  the one-header-per-scheme split the design's own §8 table sketch first
  assumed (matches reality, per this step's own instruction; see
  ops/DEVIATIONS.md).
- New `src/smd/fixpoint/schemes.t.cpp`: includes `schemes.hpp` twice
  (idempotency check) plus one `TEST_CASE` per representative scheme —
  classical `fold_fix`, `para`, `prepro`, `histo`, `futu`, `dyna`, `mcata`,
  `elgot`, `dist_cata` (direct shape check), `cata_via_gcata` (gcata),
  `ana_via_gana` (gana) — all against a shared `Nat`/`NatF` fixture
  (`functors.hpp`), each checked against a trivial hand-computed count (5).
  Compile-coverage only, per the step file: the real equivalence-law
  proofs live in each scheme's own `*.t.cpp`, unchanged by this step.
- New `docs/recursion-schemes.md`: the user-facing catalog, one section per
  §7 scheme family (classical recap; para/apo; zygo/mutu; hoist/prepro/
  postpro; histo/futu; dyna/codyna/chrono; mcata/mhisto; elgot/coelgot;
  distributive laws — a table, since they're building blocks not
  standalone schemes; gcata/gana/ghylo; gprepro/gpostpro/
  zygo_histo_prepro), plus a closing "Supporting types" section
  (Identity/either/pair-env-comonad/Cofree/Free/functors.hpp's base
  functors). Every signature was copied verbatim from the actual header
  (not transcribed from the design doc), every usage snippet lifted from
  the matching example file, every example's output re-verified by
  actually running the binary (see Verification evidence).
- `README.md`: new "Recursion schemes" section — what the library
  contains, the umbrella include, how to build/run (`make TOOLCHAIN=gcc-16
  test`, example binary path pattern), links to the new catalog and the
  design doc.
- `src/smd/fixpoint/CMakeLists.txt`: append-only — `schemes.hpp` added to
  the `smd_fixpoint_headers` FILE_SET, `schemes.t.cpp` added to
  `smd_fixpoint_test` sources (plus mechanical gersemi reformatting of the
  whole file from the pre-commit sweep below — one-file-set-entry-per-line
  instead of one long line; no content change beyond the two new entries).

## Verification evidence

- `make TOOLCHAIN=gcc-16 test`: **100% tests passed, 0 tests failed out of
  214** (baseline going into this step was **202**, S15's row; net +12,
  exactly `schemes.t.cpp`'s 12 `TEST_CASE`s: HeaderIsIdempotent + 11
  representative-scheme cases). All 202 pre-existing tests still pass
  unchanged. Ran the full suite twice (once before, once after the
  pre-commit-driven mechanical edits below) — both green at 214/214.
- Explicit rebuild after `touch`ing `schemes.hpp`/`schemes.t.cpp`
  (`make TOOLCHAIN=gcc-16 compile`) with `grep -i warning`/`error` over the
  output (excluding the unrelated `VIRTUAL_ENV` venv notice every prior
  handoff has also seen): empty — no compiler warnings.
- `make TOOLCHAIN=gcc-17 compile`: full rebuild, **zero errors or
  warnings** (`grep -iE "error|warning"` over the whole log, excluding the
  venv notice, matched nothing). This finally closes out the gcc-17
  secondary-smoke item every handoff since S00 has carried as an open
  risk — S16's own step file explicitly asked for this as part of its
  consistency sweep.
- **Every §10 example rebuilt and run fresh**, all exit 0 (captured just
  now, not reused from old handoffs):
  ```
  hello                    -> (prints library greeting) exit 0
  fixpoint_tree_example    -> Result: 10
  para_pretty_print        -> 2 * 3 + 4
                              2 * (3 + 4)
  apo_sorted_insert        -> before: [1, 3, 7, 9]
                              after inserting 5: [1, 3, 5, 7, 9]
  zygo_balanced            -> balanced tree is balanced: true
                              unbalanced tree is balanced: false
  mutu_even_odd            -> 0: even=true odd=false ... 10: even=true odd=false
  prepro_takewhile_sum     -> sum(take_while(>=0, [3, 4, -1, 5])) = 7
                              sum(take_while(>=0, [3, -1, 4])) = 3
                              sum(take_while(>=0, [1, 2, 3])) = 6
  histo_coin_change        -> minCoins(0,...)=0 minCoins(1,...)=1 minCoins(4,...)=1
                              minCoins(5,...)=1 minCoins(8,...)=2 minCoins(12,...)=3
                              minCoins(13,...)=3
  futu_rle_decode          -> input: [(2, 7), (3, 1)] -> decoded: [7, 7, 1, 1, 1]
                              input: [(1, 9), (4, 0), (2, 5)] -> decoded: [9, 0, 0, 0, 0, 5, 5]
  dyna_fibonacci           -> fib(0..10) = 0,1,1,2,3,5,8,13,21,34,55
  mendler_eval             -> mcata eval: 10
  elgot_shortcircuit       -> values: 6 elements
                              product (bails out at the first 0): 0
                              elements examined: 3 / 6
  generalized_tour         -> fold_fix / cata_via_gcata    via dist_cata     : specialized = 5, generalized = 5 (match)
                              histo / histo_via_gcata      via dist_histo    : specialized = 55, generalized = 55 (match)
                              dyna / ghylo-as-dyna         via histo+ana     : specialized = 55, generalized = 55 (match)
                              zygo_histo_prepro capstone   via dist_zygo_histo: specialized = 4, generalized = 4 (match)
  ```
  All 13 binaries (12 named + `hello`) exit 0; ran them a second time after
  the CMakeLists.txt gersemi reformatting to confirm nothing regressed.
- **Header hygiene sweep**: every `.hpp` under `src/smd/fixpoint/` has an
  `#ifndef INCLUDED_SMD_FIXPOINT_*` guard and a matching `#endif`
  (scripted grep, zero misses); the four typeclass headers named in §8
  (`identity.hpp`/`either.hpp`/`pair.hpp`/`comonad.hpp`) likewise; `grep -rn
  iostream src/smd/fixpoint/*.hpp src/examples/*.cpp` — no matches
  (examples already use `<print>` throughout, confirmed, not just assumed).
  All headers in design §8's table are present, in the `smd_fixpoint_headers`
  FILE_SET, and have a `.t.cpp` in the `smd_fixpoint_test` sources list
  (confirmed by reading `CMakeLists.txt` directly, not by trusting prior
  handoffs).
- **Pre-commit sweep** (`uv run pre-commit run --files <the 5 files this
  step touched>`, run twice): first pass found two mechanical issues in
  files this step created — `gersemi` (CMake formatter) reformatted
  `CMakeLists.txt`'s `FILES`/`PRIVATE` lists from one-line-per-target to
  one-entry-per-line (accepted, purely mechanical, no semantic change —
  the pre-existing "unknown command 'catch_discover_tests'" gersemi
  warning is unrelated and predates this step); `codespell` flagged
  "re-declared" (schemes.t.cpp's own comment) as a typo for "redeclared" —
  fixed in the one file this step owns. (Note: `gprepro.t.cpp`, S15's file,
  has the identical "re-declared"/"re-declares" spelling and would fail the
  same codespell check if targeted directly — confirmed by running
  codespell on it alone — but per this step's own scope ("fix mechanical
  findings ... in files this plan created" doesn't mandate touching every
  prior step's file), it was left alone; flagging here as a trivial,
  optional follow-up for whoever next touches `gprepro.t.cpp`.) Second
  pass, after both fixes: `trim trailing whitespace`, `fix end of files`,
  `clang-format`, `CMake linting` (gersemi), `markdownlint`, `codespell`,
  `Detect hardcoded secrets` all **Passed** on all five touched files. Full
  gate re-run green (214/214) after these mechanical edits.

## Deviations from the plan / design

None new. No row added to `ops/DEVIATIONS.md` by this step — S16 is a
packaging/docs step with no scheme logic of its own to deviate. This step
*did* need to document, in both `schemes.hpp`'s own doc comment and
`docs/recursion-schemes.md`, that `generalized.hpp` holds six schemes
(gcata/gana/ghylo/gprepro/gpostpro/zygo_histo_prepro) in one file rather
than the one-header-per-scheme layout the design's own §8 table sketch
first assumed — this is not a new deviation, it is the exact,
already-anticipated situation the task brief and S13/S14/S15's handoffs
flagged; §8's own table (as read today) already lists `generalized.hpp`
against all six scheme names on one row, so the doc catalog and umbrella
header were written to match that reality directly, with no ledger entry
needed.

**Existing deviations (DEV-01 through DEV-04) are still unreconciled into
`docs/recursion-schemes-design.md`** — confirmed by `grep -n "DEV-0"
docs/recursion-schemes-design.md`, which finds zero matches. Per this
step's own instruction, I did **not** edit the design doc's normative
sections myself (§1's process note reserves that for the design author).
Flagging explicitly, as the task asked, for whoever reads this next:

- **DEV-01** (S06, reworded pitfall: use a non-idempotent transformation
  like `decrement_nat`, not a monotone cut like take-while, as the
  discriminating fixture for cumulative-hoist-depth correctness) — not
  folded back into S06's own Pitfalls section or design §7.4's
  cumulative-cost note.
- **DEV-02** (S12, `dist_histo`/`dist_futu`/`dist_para` are variable
  templates on `F`; `dist_apo`/`dist_gapo` need an explicit leading `X`) —
  not folded back into design §4/§7.9's prose (which still reads as if
  every distributive law takes zero explicit template arguments). The new
  `docs/recursion-schemes.md` catalog documents the *actual* call
  convention directly (see its §7.9 table) so user-facing docs are
  already correct even though the design doc's own §4 prose is not.
- **DEV-03** (S14, CTAD's copy-deduction candidate silently collapses
  `Identity{x}` when `x` is already an `Identity<T>`; name the wrapped
  type explicitly instead) — not folded back into design §4/§7.9's
  "distributive laws are polymorphic function objects" prose as a general
  CTAD-hazard warning.
- **DEV-04** (S14, `Monad<Impl>`'s CRTP-derived operations needed
  `constexpr` added explicitly; the gap predates D10) — not folded back
  into design §4/D10's "all new schemes and supporting types are declared
  constexpr" wording as an explicit note that CRTP base classes' *derived*
  operations need the same treatment as the `Impl`-provided primitives.

## Discoveries affecting later steps

None — this is the last step in `ops/PLAN.md`. Nothing here needs to
propagate to a "next" agent inside this plan. For whoever picks up
follow-on work outside this plan (a new plan, or ad hoc), the load-bearing
facts are:

- The umbrella include is `#include <smd/fixpoint/schemes.hpp>` — it pulls
  in all 20 fixpoint-module headers plus the 4 typeclass-module headers
  named in design §8's table, with zero ODR/include-order surprises
  (confirmed by `schemes.t.cpp` combining calls to all 11 scheme families
  in one translation unit).
- `docs/recursion-schemes.md` is now the place to update if any scheme's
  *signature* changes in the future — it was written by copying from the
  headers directly (per this step's own pitfall warning: "the docs catalog
  goes stale the moment it transcribes the design doc instead of the
  headers"), so it should be kept in sync with the headers, not the design
  doc, going forward.
- `ops/DEVIATIONS.md`'s four rows are real, still-open documentation debt
  against `docs/recursion-schemes-design.md` — see the itemized list
  above. None of them affect runtime correctness (all four are either
  compile-time-only concerns or already-fixed code); they are purely
  "the design doc's prose doesn't yet match what the code actually does."

## Forward notes for the NEXT step

**There is no next step.** `ops/PLAN.md`'s checklist (S00 through S16) is
now fully checked, Phase A through Phase F all complete. If this plan is
ever extended, the natural next candidates are the ones design §11 already
named as explicit non-goals (and therefore out of scope for anything
tagged `[schemes]`): performance tuning/fusion of `ghylo` (currently
materializing, not fused — see S14's handoff), stack-safety/iterative
rewrites, `std::indirect` migration off `Box`, and reconciling the four
open `ops/DEVIATIONS.md` rows into `docs/recursion-schemes-design.md`
itself (a design-author task, not an execution-agent one, per §1's process
note).

## Open risks / TODOs

- **The four `ops/DEVIATIONS.md` rows (DEV-01 through DEV-04) remain
  unreconciled into `docs/recursion-schemes-design.md`** — see the
  itemized list under Deviations above. This is the one substantive open
  item from the whole plan.
- `gprepro.t.cpp` (S15's file) has the same "re-declared"/"re-declares"
  codespell finding this step fixed in its own new file — left untouched,
  out of this step's stated scope (see Verification evidence); a trivial,
  optional cleanup for later.
- Design §11's non-goals (performance/fusion, stack safety, `std::indirect`
  migration) remain explicitly out of scope, as they have been since S00;
  nothing in this step changes that.
- gcc-17 is now clean at `compile` level (see Verification evidence) —
  this closes the "gcc-17 secondary smoke" item every prior handoff carried
  as open, but note it is still `compile`-only, not a full `ctest` run
  under gcc-17 (the plan's own gate has only ever required this as
  advisory, per design D9; still true).
