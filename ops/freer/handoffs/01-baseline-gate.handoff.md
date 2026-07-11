# Handoff — S01 FD9 baseline gate TU + mechanics probes

- **Status:** DONE (gate passed)
- **Commit:** f3f906c (`[freer] S01: baseline gate TU + probes`), plus a
  small follow-up 1ccd967 (`[freer] S01: fix lint (clang-format,
  codespell)`) — see Deviations below for why a third (non-design)
  commit exists.
- **Date / agent:** 2026-07-11, Sonnet worker

## What changed

- `src/smd/fixpoint/freer_baseline.t.cpp` (new): one TU, test-local
  types only (`unit`, `Get`, `Put`, `impure_node<Op, X>`, `KVSig` with
  nested `template <class X> struct type { variant<...> node; }`,
  `KVFree = Free<KVSig::template type, int>`, `KVLayer =
  KVSig::template type<KVFree>`). Three `TEST_CASE`s plus several
  `static_assert` clusters, each independently tagged and gating one
  FD/decision (see below).
- `src/smd/fixpoint/CMakeLists.txt`: added `freer_baseline.t.cpp` to
  `smd_fixpoint_test`'s `target_sources` (existing target, no new
  executable).
- `ops/freer/DEVIATIONS.md`: added `DEV-S01-1` (the
  `equality_comparable` surprise, see below).

No `freer.hpp` and no changes to any other shipped header — this step
is evidence-only, per its own framing.

## Verification evidence

```
make TOOLCHAIN=gcc-16 test
  100% tests passed out of 223 (Total Test time (real) = 7.65 sec)

make TOOLCHAIN=clang-23 test
  100% tests passed out of 223 (Total Test time (real) = 5.12 sec)
```

223 = the S00 baseline of 220 + this step's 3 new `TEST_CASE`s (each
`static_assert` cluster runs at compile time, before any test
executes, and does not add a ctest entry). New test names (confirmed
via `ctest -N`):

- `freer baseline [FD3][FD9]: hand-built impure_node/Free constructs
  and resumes`
- `freer baseline [FD5][FD9]: no == is ever named on KVFree` (anchors
  the two `static_assert`s above it under a findable name; the probe
  itself already ran at compile time)
- `freer baseline [FD12][FD9]: KVFree move-construction propagates and
  the moved-to value still resumes`

No compiler-conditionals were needed anywhere in the file — every
probe (including the surprising one, DEV-S01-1) gave the *same*
result on both pins. `make lint` (pre-commit -a): all hooks passed,
including clang-format with zero reformatting needed on the new file.

## Probe-results table

| Probe | FD/decision | gcc-16 | clang-23 | Result |
|---|---|---|---|---|
| (1) FD3 shape compiles: hand-build `impure_node<Get,X>`/`KVLayer`/`KVFree`, `roll_free`, resume via `std::move(k)(response)` | FD9 item 1 | accepted, runs, correct value (Pure(7)) | accepted, runs, correct value (Pure(7)) | Green, identical |
| (2a) `!std::equality_comparable<KVLayer>` (the raw layer, no `operator==` written/defaulted) | FD5/FD9 item 2 | accepted (assertion holds: false) | accepted (assertion holds: false) | Green, identical |
| (2b) `std::equality_comparable<KVFree>` (the Free type, which HAS a hand-written friend `operator==`) | FD5/FD9 item 2, FD3 | **evaluates true** — the naive `!equality_comparable` static_assert FAILS to compile (verbatim: `static assertion failed: Free type should not be equality_comparable`) | **evaluates true**, same verbatim failure (`static assertion failed due to requirement '!std::equality_comparable<Free<KVSig::type, int>>'`) | Diverges from the step's literal expectation, not from each other — see Discoveries/DEV-S01-1 |
| (2c) actually *calling* `==` on a `KVFree` (uncommitted scratch probe only, to confirm the operational claim) | FD5/FD9 item 2 | hard error instantiating the friend body: `no match for 'operator=='` comparing the inner `std::variant<int, KVSig::type<...>>` | not independently re-run (gcc-16 result plus the shared reasoning is conclusive: `impure_node` has no `operator==` on either compiler) | Confirms FD3's "instantiated only when called" claim; not part of the committed TU |
| (a) `functor_typeclass<KVFree>`/`monad_typeclass<KVFree>` select the `Free` partial specialization (member-template `F` deduction) | D-A input | accepted (`static_assert` holds) | accepted (`static_assert` holds) | Green, identical — FD3 windfall confirmed |
| (b) constrained variable-template partial specialization selected for a type exposing a keyed nested typedef, primary selected for `int` | D-A input | accepted (both assertions hold) | accepted (both assertions hold) | Green, identical — FD11 option (a)'s mechanism works |
| (c) `std::movable<KVFree> && !std::copyable<KVFree>`, runtime move + resume | D-B input | accepted, runs, correct value (Pure(99)) after move | accepted, runs, correct value (Pure(99)) after move | Green, identical — matches S00's macro/well-formedness probe |

**No cross-compiler divergence anywhere** — every probe, including the
one that produced a result different from the step file's literal
expectation (2b), gave the identical result on gcc-16 and clang-23.
There is nothing to compiler-condition in the committed file.

## Cross-compiler divergences

None. gcc-16 and clang-23 agreed on every probe, including verbatim
diagnostic text differing only in phrasing (GCC: "static assertion
failed: <message>"; Clang: "static assertion failed due to requirement
'<expr>': <message>") for the one static_assert that was deliberately
compiled in its "wrong" polarity during exploration (not committed —
see below).

## Process note (not a DEVIATIONS.md row — not a design/FD deviation)

`make lint` passed cleanly right before the step commit, but that run
silently **skipped** `freer_baseline.t.cpp` — `pre-commit run -a`
operates over `git ls-files` (tracked/staged files), and the new file
was still untracked at that point. After `git add` + commit, re-running
`make lint` for the bookkeeping step (as this protocol requires) caught
real findings: clang-format wanted different wrapping in a few spots,
and codespell flagged two prose words as probable misspellings of
other words. Fixed in a small follow-up commit 1ccd967, no semantic
change. Forward
note for every later step: stage (`git add`) new files *before* the
pre-gate `make lint` run, or re-run `make lint` once more immediately
before committing — otherwise a genuinely-dirty new file can slip
through with a false "all hooks passed".

## Deviations from the plan / design

`DEV-S01-1` (added to `ops/freer/DEVIATIONS.md`): FD9 item 2 asks for
`static_assert(!std::equality_comparable<...>)` on the Free type
itself (not just the layer). That assertion, taken literally, **fails
to compile** on both pins — `std::equality_comparable<KVFree>` is
**true**. Root cause (confirmed by direct experiment, not
speculation): `Free`'s hand-written, non-template friend
`operator==`'s *declaration* becomes visible the moment
`Free<KVSig::type, int>` is instantiated, independent of whether its
*body* would compile. `std::equality_comparable`'s requires-expression
only needs the *expression* `a == b` to resolve via overload
resolution to something convertible to `bool` — it does not
instantiate the friend body to check that. So the concept reports
`true`. Separately (scratch-only, uncommitted probe, gcc-16): actually
*writing* `a == b` and odr-using it **does** hard-error, inside the
friend body, exactly as FD3 predicts (`impure_node` has no
`operator==`, so `std::variant<impure_node<Get,X>,
impure_node<Put,X>>` has none either). So the *operational* claim FD3
and FD9 care about — "comparison is never actually instantiated, no
eager deleted-ness check fires" — holds cleanly; the specific
`std::equality_comparable` predicate is just the wrong tool to observe
it with, because it type-checks well-formedness of the *call
expression*, not whether the *called body* would compile. The committed
file's probe therefore asserts what is actually true on both
compilers: the layer is not `equality_comparable` (no `operator==` at
all), and `KVFree` IS `equality_comparable` by that predicate (with a
comment explaining why this isn't the contradiction it looks like) —
and the file, as a whole, never calls `==` on a `KVFree`, so it never
hits the operationally-real failure. This is evidence for the
orchestrator to fold back into FD3/FD5's phrasing (the "consequence
worth recording" paragraph should clarify "declaration visible, body
lazily instantiated" vs. "not equality_comparable" are different
claims), not a blocker for anything downstream.

No other deviations. Probes (a), (b), (c) all landed exactly as FD9/
FD11/FD12 anticipated, with identical results on both pins.

## Discoveries affecting later steps

- **Exact local spellings that worked** (S03 can reuse verbatim):
  - `impure_node<Op, X>` member layout: `Op op; std::move_only_function<smd::fixpoint::Box<X>(typename Op::response) &&> k;` — compiles with `X` incomplete at declaration point on both compilers, no special handling needed.
  - `signature`'s nested member template: `struct KVSig { template <class X> struct type { std::variant<impure_node<Ops,X>...> node; }; };` — used as `Free<KVSig::template type, int>` at the call site; `KVSig::template type<KVFree>` to name the instantiated layer type. Both spellings compile identically on gcc-16/clang-23 — no P0522 friction observed (consistent with FD3's rationale for choosing this shape over an alias template).
  - Constructing a layer value: use `std::in_place_type<impure_node<Op,X>>` when building the inner `std::variant<impure_node<Ops,X>...>` — I used this defensively per the step's pitfall note; did not attempt the converting constructor to compare, so I can't confirm it actually misbehaves here, only that in-place construction works cleanly.
  - Resuming: `std::move(k)(response)` returns `Box<X>` by value; deref via `*resumed` to get the `X` (here `KVFree`), then `is_pure`/`std::get<int>(...)` as usual. No surprises — `Box<X>` behaves exactly like every other place it's used in the library (free.hpp, cofree.hpp).
  - `functor_typeclass<KVFree>` / `monad_typeclass<KVFree>` — reachable with **zero new registration**, confirming FD3's windfall. S03 does not need to (and per FD11, structurally *cannot* via ordinary partial specialization) register anything at the `Free<F,A>` level for a signature; only the *layer*-level instance (`functor_typeclass<Sig::type<X>>`) is FD11's open question.
  - `probe_b`'s constrained variable-template partial specialization mechanics (mock registry, not the real `functor_typeclass`) worked exactly as FD11 option (a) proposes: a `concept probe_concept<T> = requires { typename T::recursive_position; };` plus `template <class T> requires probe_concept<T> inline constexpr auto probe_tc<T> = marker_t{};` is selected over the `std::false_type` primary for a type exposing the nested typedef, and the primary wins for `int`. This is real, positive evidence for FD11 option (a)'s viability — the orchestrator's D-A input.
- **`Box<X>` with incomplete `X`:** nothing surprised me. `Box<X>` as a *named type* (inside the `move_only_function` signature, as a member declaration) never required `X` complete; only calling `Box`'s own member functions (which I only do later, once `X = KVFree` is complete) would need it. This matches the "Box is the completeness firewall" framing exactly — no restructuring needed anywhere in the file.
- **The `equality_comparable` discovery (DEV-S01-1)** is the one genuinely new/non-obvious fact this step surfaced: `std::equality_comparable<T>` is a *declaration-visibility* check for a plain (non-template, non-defaulted) friend `operator==`, not a *body-compilability* check. Anyone reasoning about "is this Free-wrapped-around-a-noncomparable-layer comparable" from `std::equality_comparable` alone will get a misleading `true`. If FD5/FD10's observational-testing test helpers ever want to *statically forbid* calling `==` on a `Freer` value (e.g. a `static_assert` guard in a test-support header), `std::equality_comparable` is not the predicate to use for that — it would need to actually attempt `a == b` in an unevaluated `requires` context that forces body-checking (it can't, short of actually odr-using it) or a different, deliberately-crude approach (e.g. checking `impure_node`/the layer's own comparability directly, which correctly reports false). Not urgent, just worth knowing before FD5-flavored test infrastructure gets built (S04+).
- Every probe agreed identically across gcc-16 and clang-23 — zero
  cross-compiler friction anywhere in this step, including for the
  novel move_only_function/&&-continuation/self-embedding-variant
  combination FD9 was most worried about.

## Forward notes for the NEXT step (written after reading its step file)

Read `ops/freer/steps/02-consuming-traversal.md` in full. It is
independent of this step's file (touches `box.hpp`, `fmap.hpp`,
`free.hpp`, plus their `.t.cpp`s — none of which this step modified
except read-only), and its dependency is S00 (parallel to S01), so it
does not need anything from this handoff to *start*. That said:

- S02 adds a *consuming* (`Free<F,X>&&`) path to `FreeFunctorImpl::fmap`
  and `FreeMonadImpl::bind`, and a matching rvalue `layer_fmap` overload
  set in `fmap.hpp`. This step's probes never exercised `layer_fmap` or
  `fmap`/`bind` through the typeclass objects at all (probe (a)
  deliberately only checked that lookup *selects* the right
  specialization, per its own instruction not to call through it,
  since the const paths would demand copies of the move-only
  `impure_node`). So S02 is genuinely new ground for the *consuming*
  overloads' interaction with a move-only self-embedding
  variant — nothing in this handoff derisks that beyond confirming the
  base shape (`Free<F,A>` over a move-only-layer `F`) compiles and
  moves correctly per se (probe (c)).
- S02's "deferred-invocation lifetime test" pitfall (a test-local lazy
  layer functor whose `fmap` post-composes without invoking, then
  invoked after the caller scope dies, under Asan) is exactly this
  step's `impure_node`/`KVSig` shape one level up — S02's test-local
  lazy layer can very plausibly reuse `impure_node<Op,X>` /
  `KVSig`-shaped types verbatim (or a trivial one-operation variant of
  them) from this file as its "miniature of S03's real layer," since
  they are already known to compile and to propagate moves correctly
  on both pins. I did not build that test myself (out of this step's
  scope), but the vocabulary is sitting right here if S02 wants it.
- `KVSig`'s two-alternative variant construction pattern
  (`std::in_place_type<impure_node<Op,X>>` inside the layer's variant)
  is the one piece of "how do I actually build one of these by hand"
  ergonomics S02's move-only smoke tests will likely need again for
  its own test-local layer.
- Nothing in S02's scope (`box.hpp`, `fmap.hpp`, `free.hpp` consuming
  paths) is touched by DEV-S01-1; the `equality_comparable` discovery
  is orthogonal to S02's work.

## Open risks / TODOs

- `DEV-S01-1` is unresolved language/library-semantics evidence, not a
  blocker — flagged for the orchestrator to fold back into FD3/FD5's
  phrasing when convenient (not urgent; does not block D-A/D-B or any
  S-step).
- D-A and D-B are **not** resolved by this step (not this worker's
  call) — the probe-results table above is the complete evidence the
  orchestrator needs for both:
  - D-A (FD11): probes (a) and (b) both green, identical on both
    compilers — positive evidence for FD11 option (a) (constrained
    variable-template partial specialization) as well as for the FD3
    windfall (no new registration needed at the `Free<F,A>` level).
  - D-B (FD12): probe (c) green and identical on both compilers,
    consistent with S00's macro probe — positive evidence for keeping
    `std::move_only_function` gated on
    `__cpp_lib_move_only_function`, with the caveat (from S00's
    DEV-S00-1) that this is only established for the two *primary*
    pins (both libstdc++-backed); the libc++ gap remains open risk if
    the CI matrix widens.
- Did not probe any non-primary toolchain — out of scope per the
  dispatch prompt and PLAN's pins.
