# Handoff — S03 The signature layer: unit, operation, impure_node, signature, Freer

- **Status:** DONE (gate passed)
- **Commit:** 9dc11fe (`[freer] S03: signature layer`)
- **Date / agent:** 2026-07-11, Sonnet worker

## What changed

- `src/smd/fixpoint/one_shot.hpp` (new): `one_shot<R(Args...)>` — FD12/D-B's
  bespoke type-erased, move-only, call-consuming callable. Class-template
  partial specialization on a function type `R(Args...)`; type-erased
  `concept_t`/`model_t<F>` pair behind a raw owning pointer (mirrors
  `Box`'s completeness-firewall shape, manual `new`/`delete`, no
  `constexpr` — unlike `Box`, nothing here needs to be constexpr-capable).
  Move-only (copy deleted). `operator()(Args...) && -> R` invokes the
  target and deletes it as part of the same call (RAII scope-delete guard,
  so the target is destroyed even if invoking it throws); a violated
  precondition (empty target — default-constructed, moved-from, or
  already-called) is an `assert`, not a silent null-deref — the default
  `Asan` `CONFIG` does **not** define `NDEBUG` (confirmed by reading
  `etc/gcc-flags.cmake`/`etc/clang-flags.cmake`), so the assert is live
  under the pinned gate. `explicit operator bool() const` observes whether
  a target is still held.
- `src/smd/fixpoint/one_shot.t.cpp` (new): 9 `TEST_CASE`s — empty by
  default, holds-a-target after construction, invoke returns the result,
  invoke consumes (`operator bool()` flips false), move ctor/assignment
  transfer the target (and moved-from source reports empty), a move-only
  capture with two arguments, a stateful capture shaped like an
  `impure_node` continuation, and exception-safety (target still destroyed
  and box emptied when the invoked callable throws). Plus `static_assert`s
  for `movable`/`!copyable`.
- `src/smd/fixpoint/freer.hpp` (new): `unit`, `operation` concept,
  `impure_node<Op, X>` (using `one_shot`, not `move_only_function`),
  `signature<Ops...>` with nested `type<X>` exposing
  `coyoneda_signature`/`coyoneda_value_type`/`node_variant`, `Freer<Sig,A>
  = Free<Sig::template type, A>`, the `coyoneda_layer` concept, and the
  generic `CoyonedaFunctorImpl`/`CoyonedaFunctorMap`/`functor_typeclass<Layer>`
  constrained partial specialization (FD11/D-A option (a)). No
  `monad_typeclass` specialization written for the layer, per D-A's
  explicit instruction — the Monad comes from the pre-existing
  `Free<F,A>` windfall.
- `src/smd/fixpoint/freer.t.cpp` (new): 4 `TEST_CASE`s plus a
  `static_assert` cluster — see "Exact new test names" below.
- `src/smd/fixpoint/CMakeLists.txt`: added `one_shot.hpp`/`freer.hpp` to
  `smd_fixpoint_headers` (the FILE_SET actually named in this repo —
  **not** `example_fixpoint_headers`, which is what my dispatch prompt
  said; that name does not exist anywhere in this tree. Flagging loudly
  per the dispatch prompt's own request, but this is a prompt/repo naming
  mismatch, not a design deviation, so no `DEVIATIONS.md` row), and
  `one_shot.t.cpp`/`freer.t.cpp` to `smd_fixpoint_test`'s sources (existing
  test target, no new executable).
- Did not touch `free.hpp`, `fmap.hpp`, `box.hpp`, `monad.hpp`,
  `functor.hpp`, or any pre-existing test — confirmed by `git show
  --stat 9dc11fe` (5 files changed, all new files plus the CMakeLists.txt
  addition).

## Verification evidence

```
make TOOLCHAIN=gcc-16 test    -> 100% tests passed, 245/245
make TOOLCHAIN=clang-23 test  -> 100% tests passed, 245/245
make lint                     -> all hooks passed (clang-format required
                                  one auto-fix pass on the new files, then
                                  clean; re-ran make lint immediately
                                  before committing per S01's process note)
```

245 = this worktree's pre-S03 baseline (232: the 229 from the S02 handoff's
Status-log entry, plus 3 more already merged into this branch from a
parallel `S07d` step's history — visible via `git log --oneline`, not
something this step added) + 13 new (`ctest -N`, `gcc-16`):

- `one_shot - default-constructed is empty`
- `one_shot - holds a target after construction from a callable`
- `one_shot - invoking returns the target's result`
- `one_shot - invoking consumes the target`
- `one_shot - move construction transfers the target`
- `one_shot - move assignment transfers the target and releases the destination's own`
- `one_shot - carries a move-only capture and multiple arguments`
- `one_shot - real captured state (not stateless), matching the impure_node continuation shape`
- `one_shot - the target is destroyed (and the box emptied) even when invoking it throws`
- `Freer [FD3][FD9]: hand-built impure_node/Freer constructs and resumes`
- `Freer layer fmap [FD6]: post-composes onto the continuation without invoking it`
- `Freer consuming bind [FD3 windfall]: monad_typeclass<Freer<KV, int>> sequences through a suspended Get`
- `Freer consuming bind: continuation owns its captures (FD4)`

Identical 245/245 and identical new-test names on both pins.

**Asan lifetime-test teeth, verified by hand** (per the dispatch prompt's
explicit requirement): temporarily changed
`CoyonedaFunctorImpl::fmap`'s inner continuation capture in `freer.hpp`
from `[k = std::move(node.k), fn = std::move(fn)]` to
`[k = std::move(node.k), &fn]`, rebuilt under `gcc-16`, and ran the
`"Freer consuming bind: continuation owns its captures (FD4)"` test in
isolation:

```
SUMMARY: AddressSanitizer: stack-use-after-scope
  src/smd/fixpoint/freer.t.cpp:209 in operator()
```

— the exact line where the extracted, outlived continuation is invoked
and its (now-dangling) captured `fn` would be read. Reverted the capture
to `fn = std::move(fn)` and reran both pinned gates: 245/245 on both. The
test is not decorative.

## Cross-compiler divergences

None. Every probe, every new test, and the deliberate Asan-teeth check
above behaved identically on gcc-16 and clang-23 — no compiler-conditional
code anywhere in this step's files.

## Deviations from the plan / design

None requiring a `DEVIATIONS.md` row. D-A and D-B were implemented
exactly as quoted in the dispatch prompt:

- The Coyoneda-specific marker is `coyoneda_signature` (naming
  `signature<Ops...>` itself), not a generic name — binding constraint
  honored. A second nested typedef, `coyoneda_value_type` (naming `X`),
  is also Coyoneda-specific in name (not the probe's illustrative
  `recursive_position`) and is required by the `coyoneda_layer` concept
  alongside `coyoneda_signature`; a third, `node_variant` (naming the
  layer's own `std::variant<impure_node<Ops,X>...>` type), is exposed so
  the instance can name the *result* layer's variant type without
  re-deriving the `Ops...` pack. None of these overlap or could be
  confused with `Free<F,A>`'s own specialization or any pure-data functor
  instance's surface.
- `one_shot<Sig>` was delivered exactly per D-B's placement note: its own
  small header (`one_shot.hpp`), with its own `.t.cpp`, wired into the
  FILE_SET/test target. No macro-gated `std::move_only_function` fast path
  was added.
- The one (harmless) surprise: `functor_typeclass`'s constrained partial
  specialization needed no special handling to coexist with the
  pre-existing `Free<F,A>` partial specialization or any of the
  pure-data instances (`Optional`, `Vector`, ...) — overload/partial-
  ordering between the unconstrained primary (`std::false_type`) and this
  step's constrained specialization Just Worked, on both compilers, first
  try. Not a deviation, just recording that FD11's probe (b) evidence
  (S01) generalized cleanly to the real `functor_typeclass` variable
  template, not only the mock registry S01 tested it against.

## Discoveries affecting later steps

- **The FD3 windfall holds at instantiation, on both pins** — this is the
  key new evidence beyond S01 (which only checked that
  `functor_typeclass<Free<F,A>>`/`monad_typeclass<Free<F,A>>` *select*
  the right specialization, deliberately never calling through it). This
  step's `"Freer consuming bind [FD3 windfall]"` test and the Asan
  lifetime test both call `smd::typeclass::monad_typeclass<KVFree>.bind(...)`
  on a real `Freer<KV,int>`, and it actually runs end-to-end — through
  `FreeMonadImpl`'s consuming `bind` (S02), through `layer_fmap`'s
  consuming overloads (S02), into this step's new
  `CoyonedaFunctorImpl<KVLayer>::fmap` — resuming to the correct value on
  both gcc-16 and clang-23. Zero new registration was needed at the
  `Free<F,A>` level, exactly as FD3/FD11 predicted; only the layer-level
  instance (this step's deliverable) was missing, and now it is not.
- **`std::in_place_type` is still the right way to build a layer's
  variant**, confirmed again here (as S01 first found): direct aggregate
  init of `Sig::type<X>{ impure_node<Op,X>{...} }` was not attempted in
  the committed code (I used `ResultVariant{std::in_place_type<...>, ...}`
  and `KVLayer::node_variant{std::in_place_type<...>, ...}` throughout,
  defensively, matching S01's recorded caution about the variant's
  converting constructor and move-only alternatives). **S04 should be
  aware**: FD7's `send()` code sketch in the design doc writes
  `typename Sig::template type<X>{ impure_node<Op, X>{...} }` directly
  (relying on the variant's converting constructor) — that spelling has
  *not* been verified to compile in this codebase; every committed test
  in both S01's and this step's files uses `std::in_place_type` instead.
  S04 should try the sketch's literal spelling first (it may well be
  fine — S01's caution was "used this defensively... did not attempt the
  converting constructor to compare"), but should not be surprised if it
  needs the `in_place_type` form and should say so in its own handoff
  either way.
- **The layer's own Functor instance's `fmap` signature, exactly as
  landed** (S04 builds `send`/`run` against this; later steps building
  other signatures rely on it needing zero per-signature code):
  ```c++
  // freer.hpp, namespace smd::typeclass
  template <class Layer>
      requires smd::fixpoint::coyoneda_layer<Layer>
  struct CoyonedaFunctorImpl {
      template <class Fn>
      constexpr auto fmap(this auto &&, Fn &&fn, Layer &&layer) -> /* Sig::type<Y> */;
  };
  // registered as functor_typeclass<Layer> for any Layer satisfying coyoneda_layer
  ```
  Reached automatically via `layer_fmap(fn, std::move(layer))` (mode 1,
  implicit lookup) — no call-site changes anywhere, including inside
  `free.hpp`'s pre-existing consuming `bind`/`fmap`, which S02 already
  wrote generically enough (the `auto&&`-forwarded recursive `child`) to
  route straight into it.
- **`signature<Ops...>::type<X>`'s full nested-typedef surface, as
  landed**: `coyoneda_signature` (= `signature<Ops...>`),
  `coyoneda_value_type` (= `X`), `node_variant` (=
  `std::variant<impure_node<Ops,X>...>`), plus the `node` data member of
  that variant type (unchanged from FD3's sketch). `Freer<Sig,A>` is
  exactly `Free<Sig::template type, A>` — no `typename` before
  `Sig::template type` in that alias (matches S01's confirmed-working
  spelling; `typename` there is actually **ill-formed**, since a bare
  template-name used as a template-template argument is not a type-id —
  worth remembering if anyone "fixes" this alias by adding `typename`
  later).
- **`one_shot<Box<X>(typename Op::response)>` compiles with `X`
  incomplete**, same as the `move_only_function` spelling S01 verified —
  `one_shot` only stores a `concept_t*` member, so naming
  `one_shot<Box<X>(...)>` as a type never requires `X` (or even `Box<X>`)
  complete at the declaration point inside `impure_node`.
- **`std::visit` with a single templated generic lambda, wrapped in
  `overloaded{...}`, is what the layer's own `fmap` uses** — not a
  branch-and-`get` pattern like `free.hpp`'s consuming `bind`/`fmap`. This
  is deliberate and safe here (unlike in `free.hpp`): the outer visitor
  lambda captures `fn` by move and is invoked exactly once (std::visit
  dispatches to exactly one live alternative), so there is no
  double-move race even though the same `fn` is (logically) available to
  every `Op` instantiation of the lambda's template — see the long
  in-code comment in `freer.hpp` contrasting this with `free.hpp`'s
  reason for branching instead.

## Forward notes for the NEXT step (written after reading its step file)

S04 (`ops/freer/steps/04-send-trace.md`) adds `send<Sig>(op)` to
`freer.hpp` and a new `freer_run.hpp` (the synchronous loop-based runner
plus the trace handler). Notes for that work:

- `send`'s continuation is the "identity-into-Pure" shape; per the
  discovery above, try the design doc's literal aggregate-init spelling
  first, but be ready to fall back to `std::in_place_type` on the layer's
  `node_variant` (named exactly that in this step's landed code) if the
  converting constructor gives trouble with the move-only `one_shot`
  member — this codebase's precedent (S01, S03) is 2-for-2 in favor of
  `in_place_type` being the one that's actually used, so don't be
  surprised.
- `send`'s continuation must be built as a `one_shot<Box<X>(typename
  Op::response)>` (not `std::move_only_function`) — construct it the same
  way this step's tests do:
  `one_shot<Box<X>(typename Op::response)>([](typename Op::response r) -> Box<X> { return make_box<X>(pure_free<Sig::template type>(std::move(r))); })`.
  A plain lambda converts implicitly via `one_shot`'s templated
  constructor (SFINAE-constrained on `std::invocable`/`std::convertible_to`,
  see `one_shot.hpp`) — no explicit wrapping needed beyond passing the
  lambda where the `one_shot<...>` is expected, but the *type* named at
  the member/parameter position must say `one_shot<...>`, not
  `move_only_function<...>` or a bare deduced lambda type.
- The runner's loop needs to `std::visit(overloaded{...}, std::move(node))`
  over `Sig::type<X>::node` (the `node_variant` member, per this step's
  landed nested typedef) to reach each `impure_node<Op,X>` by move, call
  the user handler on `std::move(node.op)`, then resume with
  `std::move(node.k)(response)` (this step's `one_shot::operator()(Args...) &&`)
  and unbox with `*std::move(box)` (S02's `Box::operator*() &&`, already
  in `box.hpp`, untouched by this step). `is_pure`/`std::get<A>(prog.node)`
  (both pre-existing, `free.hpp`) close the loop on a `Pure` leaf.
- This step's `CoyonedaFunctorImpl` and `coyoneda_layer` concept are
  usable as-is for anything S04 needs to know "is this a Coyoneda layer" —
  no new concept needed there.
- `functor_typeclass<Sig::type<X>>`/the layer's own `fmap` is not
  something `send`/`run` need to call directly at all (they only touch
  `impure_node`/`Freer` machinery); `layer_fmap`/`bind`/`fmap` via the
  typeclass objects are what S04's own program-composition examples
  (`bind(send<KV>(Get{k}), ...)`, per FD7's usage sketch) will exercise,
  and that path is already proven end-to-end by this step's "FD3
  windfall" test.
- `one_shot`'s `operator bool()` (this step) is a clean way for S04's
  trace/runner code to assert internal invariants if it wants to (e.g.
  "the node's `k` still holds a target before resuming") without risking
  a silent empty-call — not required, just available.

## Open risks / TODOs

- The dispatch prompt's stated FILE_SET name (`example_fixpoint_headers`)
  does not match this repo (`smd_fixpoint_headers`, as already used by
  every other header in `src/smd/fixpoint/CMakeLists.txt`, including S01's
  `freer_baseline.t.cpp` wiring). Used the real name. Flagging for the
  orchestrator in case that stale name appears in other not-yet-dispatched
  step prompts.
- FD7's `send()` code sketch's exact aggregate-init spelling for building
  the layer's variant is untested by this step (out of S03's scope, S03
  never calls `send`) — flagged above as a concrete, low-risk item for S04
  to resolve empirically (either spelling works fine, or `in_place_type`
  is needed; either way is a one-line fix, not a blocker).
- No other open risks specific to this step; D-A and D-B are now both
  fully exercised (lookup selection *and* instantiation/runtime), closing
  the last piece of open evidence FD11/FD12 were waiting on.
