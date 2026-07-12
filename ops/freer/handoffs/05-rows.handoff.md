# Handoff — S05 Rows: variant-of-signatures, Member injection, handler adaptors

- **Status:** DONE (gate passed)
- **Commit:** d2ef54e (`[freer] S05: rows + discharge`)
- **Date / agent:** 2026-07-11, Sonnet worker

## What changed

- `src/smd/fixpoint/freer_row.hpp` (new): `row<Sigs...>`, `row_type<T>`,
  `member<Op, SigOrRow>`, a row-aware `send<Row>(op)` overload, and
  `discharge<Sig, Row, A>(handler, prog)`. A `detail` sub-namespace holds
  the alias-level pack machinery (`ops_pack<Ops...>`, `signature_ops_pack`,
  `concat_ops_pack`, `row_ops_pack_t`, `all_distinct`/`pack_all_distinct`,
  `ops_pack_variant_t`, `row_prepend`/`row_filter_out`/`row_without_t`).
- `src/smd/fixpoint/freer_row.t.cpp` (new): FD10's `Clock`/`Network`
  signatures (test-local `time_point`/`duration`/`request`/`reply`/
  `net_error` stand-ins), a retry-shaped program over `row<Clock,Network>`,
  and 4 `TEST_CASE`s plus a `static_assert` block — see "Exact new test
  names" below.
- `src/smd/fixpoint/CMakeLists.txt`: added `freer_row.hpp` to
  `smd_fixpoint_headers` and `freer_row.t.cpp` to `smd_fixpoint_test`'s
  sources.
- Did not touch `freer.hpp`, `freer_run.hpp`, `free.hpp`, `fmap.hpp`,
  `box.hpp`, `one_shot.hpp`, `monad.hpp`, or any pre-existing test — `git
  show --stat d2ef54e`: 3 files changed (CMakeLists.txt, plus the two new
  files).

## Verification evidence

```
make TOOLCHAIN=gcc-16 test    -> 100% tests passed, 254/254
make TOOLCHAIN=clang-23 test  -> 100% tests passed, 254/254
make lint                     -> all hooks passed (clang-format required
                                  one auto-fix pass on both new files, then
                                  clean on the immediate re-run before
                                  committing, per the established process)
```

254 = S04's landed baseline (250) + 4 new (`ctest -N`, identical names on
both pins):

- `row<Clock> alone: send/run interop matches the bare signature`
- `discharge [FD10]: Network mocked (fail-then-succeed), Clock flows
  through untouched`
- `discharge [FD5]: Clock-first-then-Network yields the same observation
  as Network-first-then-Clock`
- `discharge: the re-emitted continuation owns its captures (FD4)`

Plus a `static_assert` block (not separate `ctest` entries) for
`member<Op, SigOrRow>` accept/reject over `Clock`, `Network`, and
`row<Clock, Network>` (positive and negative cases, including a
`NotInRow` op that belongs to neither signature nor the row).

**Asan lifetime-test teeth, verified by hand** (per the dispatch prompt's
explicit requirement): temporarily changed discharge's re-emit closure in
`freer_row.hpp` from owning `node.k`/`handler` by move
(`[k = std::move(node.k), handler = std::move(handler)]`) to named locals
captured by reference (`auto k_local = std::move(node.k); auto
handler_local = std::move(handler); ... [&k_local, &handler_local]`),
rebuilt under `gcc-16`, and ran `"discharge: the re-emitted continuation
owns its captures (FD4)"` in isolation:

```
SUMMARY: AddressSanitizer: stack-use-after-scope
  src/smd/fixpoint/one_shot.hpp:98 in operator()
```

— the exact line where the outlived continuation's `k_local`/`handler_local`
(now dangling, since they lived in the discharge call's own stack frame,
long gone by the time the extracted continuation is invoked in the test's
outer scope) would be read. Reverted to the move-capture and reran both
pinned gates: 254/254 on both. The test is not decorative.

## Cross-compiler divergences

None in the landed code — every test, the `send<Row>` overload-resolution
behavior, and the Asan-teeth check behaved identically on gcc-16 and
clang-23. One divergence surfaced only in **diagnostic text** while
capturing evidence for this handoff (not a behavioral difference, both
reject the same programs): clang's error for the duplicate-operation
`static_assert` prints the fully-substituted `ops_pack<...>` type,
literally naming the colliding operation twice; GCC's does not expand it.
See "How the diagnostics read" below.

## Deviations from the plan / design

None requiring a `DEVIATIONS.md` row against the *design* doc (FD1/§6),
but one implementation deviation from my own initial approach worth
recording as a discovery (below): `discharge`'s non-matching branch does
**not** use `smd::typeclass::mbind(send<SmallRow>(op), continuation)` as
the step file's "prefer mbind" guidance suggested for program composition
— it builds the re-emitted `impure_node` directly (mirroring `send`'s own
construction). This is an implementation-technique choice, not a change to
what `discharge` does or its observable behavior; see Discoveries for why.

## Discoveries affecting later steps

- **Row's landed encoding is FLATTENED, confirmed, exactly as FD6/FD11
  predicted.** `row<Sigs...>::type<X>` exposes the identical nested-typedef
  surface `signature<Ops...>::type<X>` does — `coyoneda_signature` (=
  `row<Sigs...>` itself, not any one member signature),
  `coyoneda_value_type` (= `X`), `node_variant` (= ONE
  `std::variant<impure_node<Op,X>...>` over the concatenation of every
  member signature's `Ops...`, never a variant-of-variants), and a `node`
  data member of that type. `row<Sigs...>` has its own `template<class X>
  struct type` (mirroring `signature`) precisely because S03's generic
  `CoyonedaFunctorImpl`/`functor_typeclass<Layer>` instance names
  `Layer::coyoneda_signature::template type<Y>` to build the mapped result
  layer — **zero new `fmap` was written for rows**; the existing
  `functor_typeclass<Layer>` constrained partial specialization
  (`coyoneda_layer<Layer>`, freer.hpp) picks up `row<Sigs...>::type<X>`
  automatically, confirmed by every test in `freer_row.t.cpp` running
  end-to-end.
- **`run`/`run_trace`/`tracing`/`is_pure`/`pure_free`/`roll_free` from
  S04/S02 needed ZERO changes to work over a row** — confirmed, not just
  assumed. `"row<Clock> alone: send/run interop matches the bare
  signature"` and both `discharge` tests call `run_trace<row<Clock>,
  int>(...)`/`run<row<Network>, int>(...)` directly; the generic
  `std::visit(overloaded{...}, std::move(layer.node))` dispatch in `run`
  doesn't care how many signatures contributed alternatives.
- **The exact `member`/`row_type`/`send<Row>`/`discharge` signatures, as
  landed** (`freer_row.hpp`):
  ```c++
  template <class... Sigs> struct row { /* row_member_signatures, type<X> */ };

  template <class T>
  concept row_type = requires { typename T::row_member_signatures; };

  template <class Op, class SigOrRow>
  concept member = operation<Op> &&
      detail::op_in_pack_v<Op, detail::member_ops_pack_t<SigOrRow>>;
  // SigOrRow may be a bare signature<Ops...> OR a row<Sigs...>.

  template <row_type Row, operation Op>
  auto send(Op op) -> Freer<Row, typename Op::response>;
  // static_assert(member<Op, Row>, "...") inside the body -- see below
  // for why the constraint deliberately does NOT check membership.

  template <class Sig, class Row, class A, class Handler>
  auto discharge(Handler &&handler, Freer<Row, A> &&prog)
      -> Freer<detail::row_without_t<Sig, Row>, A>;
  ```
  `Sig`, `Row`, `A` must all be given explicitly at every `discharge` call
  site (S04's deduction-limitation discovery applies identically: `Sig`
  never appears in a function parameter at all, and `Row`/`A` are hidden
  inside the `Freer<Row,A>` alias, which deduction cannot invert). `Handler`
  deduces normally.
- **`send<Row>`'s overload is deliberately constrained on `row_type<Row>`
  alone, NOT on `member<Op, Row>`.** This was a genuine design decision
  forced by a subtlety in how the readable diagnostic actually gets
  selected, worth spelling out for whoever touches this again:
  freer.hpp's `send<Sig,Op>` has NO constraint tying `Op` to `Sig` at all
  (any `Sig`/`Op` pair is a viable candidate for it). If the row overload
  were instead constrained on `member<Op,Row>`, an INVALID `Op` would
  SFINAE the row overload away, leaving freer.hpp's unconstrained one as
  the *only* viable candidate — which then instantiates and fails deep
  inside `node_variant`'s converting constructor (the "variant vomit" the
  design explicitly warns against), never reaching a `member`-flavored
  message at all. Constraining on `row_type<Row>` instead (a condition
  independent of `Op`) keeps the row overload viable — and, by C++20
  constraint-subsumption partial ordering, *strictly more specialized*
  than freer.hpp's unconstrained template — for every row call regardless
  of whether `Op` is actually a member. So the row overload is always the
  one selected for a row argument, and `static_assert(member<Op,Row>,
  ...)` inside its body is what actually fires for a non-member `Op`.
  Verified empirically, not just reasoned through (see "How the
  diagnostics read" below); confirmed the SAME overload is selected for
  valid `Op` too, since the constraint doesn't depend on `Op`'s validity at
  overload-resolution time.
- **`smd::typeclass::mbind`/`FreeMonadImpl::bind`'s generic recursive
  traversal (free.hpp, pre-existing, untouched) structurally cannot carry
  a move-only-capturing continuation (one that owns a `one_shot`) across
  more than one level.** This is the one genuinely new piece of evidence
  this step surfaces for FD4/FD6, and it bit hard enough to change the
  implementation: `.bind`'s consuming overload, when the right-hand side
  is impure (a Roll — which `send<SmallRow>(op)` always is), builds
  ```c++
  auto mapped = smd::fixpoint::layer_fmap(
      [self, fn = std::forward<Fn>(fn)](auto &&child) -> ResultFree {
          return self.bind(std::forward<decltype(child)>(child), fn);
      },
      std::move(layer));
  ```
  This closure is **not** `mutable`, so inside its body `fn` is accessed as
  `const Fn&`. The *template* still compiles/instantiates the RECURSIVE
  `self.bind(child, fn)` call for `child`'s generic `auto&&` type — which,
  because `.bind` is a single generic template covering both the Pure-leaf
  and Roll branches, ALSO instantiates `.bind`'s own impure-branch code at
  that recursive level, where it tries `fn = std::forward<Fn>(fn)` again —
  and since `Fn` was forced to `const T&` by the non-mutable closure one
  level up, this needs `T`'s COPY constructor, not its move constructor.
  Any `T` holding a `one_shot` (move-only) has no copy constructor, so this
  instantiation hard-errors (`error: use of deleted function
  one_shot::one_shot(const one_shot&)`) — REGARDLESS of whether that
  recursive branch is ever reached at runtime; templates compile both
  branches structurally. I first tried making the continuation
  const-callable via `mutable` DATA MEMBERS (the C++ keyword on class
  members, distinct from a lambda's `mutable` qualifier) instead of a
  `mutable` lambda — that fixed the FIRST-level error but the SAME failure
  recurred one level deeper (the recursive `.bind` instantiation's own
  impure branch), confirming this is structural, not a capture-syntax
  workaround away. **Fix landed: `discharge`'s re-emit branch builds the
  `impure_node<Op,SmallX>`/`roll_free` DIRECTLY (the same literal
  aggregate-init shape `send` itself uses), never routing the move-only
  continuation through `mbind`/`.bind` at all.** This sidesteps `bind`'s
  multi-child-fanout generality entirely, which a single suspended
  Coyoneda node never needed (FD6: `layer_fmap` for the impure layer is
  O(1) closure post-composition, not a traversal, so there is no actual
  "second child" to recurse into). **Forward-looking note: `mbind` remains
  perfectly fine for composing PROGRAMS (ordinary user-facing `send`/`pure`
  chains, as `retry_program()` in this step's tests does throughout) —
  the limitation is specific to continuations that themselves capture a
  move-only `one_shot`, which only infrastructure code like `discharge`
  builds. Any later step writing its own "reinterpret one op, re-emit the
  rest" style adaptor (S06's co-signature pairing conceivably could) should
  expect the same constraint and build the node directly rather than via
  `mbind`.**
- **Duplicate-operation detection**: `row<Sigs...>`'s own class body
  `static_assert`s `pack_all_distinct_v` over the FLATTENED op pack (not
  per-signature-pair comparison) — this also catches the "same signature
  type repeated" case as a special case of "same op type appearing twice",
  without a separate check.
- **How the diagnostics read** (verbatim, captured via standalone
  `g++-16`/`clang++-23` probes outside the CMake build, then discarded —
  not committed):
  - `send<Row>(NotInRowOp{})`:
    ```
    freer_row.hpp:258:19: error: static assertion failed: send<Row>(op): Op is not an operation of any member signature of Row
        static_assert(member<Op, Row>,
                      ^~~~~~~~~~~~~~~
      • constraints not satisfied
        • required by the constraints of 'template<class Op, class SigOrRow> concept smd::fixpoint::member'
    ```
    (clang additionally prints, as notes: `in instantiation of function
    template specialization 'send<row<signature<Get>, signature<Other>>,
    NotInRow>'`, then `because 'member<NotInRow, row<...>>' evaluated to
    false`, then `because 'detail::op_in_pack_v<NotInRow,
    detail::member_ops_pack_t<row<...>>>' evaluated to false` — a full,
    readable causal chain.)
  - Duplicate operation across two distinct member signatures
    (`row<signature<Get,Put>, signature<Get,Foo>>`, `Get` shared):
    ```
    freer_row.hpp:160:17: error: static assertion failed: row<Sigs...>: two (or more) of the row's member signatures share an operation type -- every operation type must be unique across all member signatures of a row (a shared operation type would collide as a duplicate std::variant alternative and make injection ambiguous)
    ```
    clang's version literally names the collision in the requirement text:
    `static assertion failed due to requirement
    'detail::pack_all_distinct_v<smd::fixpoint::detail::ops_pack<Get, Put,
    Get, Foo>>'` — the duplicate `Get` is visible directly in the printed
    pack.
  - `discharge<Sig>` where `Sig` is not one of `Row`'s own member
    signatures:
    ```
    freer_row.hpp:305:...: error: static assertion failed: discharge<Sig>: Sig is not one of Row's own member signatures -- Sig must be exactly one of the signature<...> types Row was instantiated with
    ```
    clang's requirement text spells out the actual comparison attempted:
    `'std::is_same_v<signature<NotASig>, signature<Get>> ||
    std::is_same_v<signature<NotASig>, signature<Other>>'`.
- **Discharge's re-emission does visibly stack closures, but only across
  DEFERRED resumptions, not eagerly at construction time (FD6 cost note).**
  Each consecutive non-matching op adds exactly one closure layer to the
  chain that will eventually run when that op's response arrives — the
  same O(1)-per-resumption shape `CoyonedaFunctorImpl::fmap` already has,
  not a new cost class. What IS new: `discharge`'s own recursion for
  CONSECUTIVE MATCHING ops happens on the **native C++ call stack**
  (`return discharge<Sig,Row,A>(std::move(handler), std::move(next));` is
  a direct, synchronous recursive call, not deferred through a stored
  continuation) — unlike `run`'s while-loop, which is O(1) native stack
  regardless of program length. This step's tests only exercise short
  chains (2-4 ops), so this was not stress-tested at depth; flagging for
  whichever later step first builds a program with many (dozens+)
  consecutive same-signature ops through `discharge` — S08's retry loop is
  a plausible candidate if it grows beyond a handful of attempts.

## Forward notes for the NEXT step (written after reading its step file)

S06 (`ops/freer/steps/06-cofree-pairing.md`) adds `unfold_cofree.hpp` and
`freer_cosignature.hpp` (the lazy co-signature product + `pair_run`).
Notes for that work:

- **If S06 builds `co<row<...>>` (the step file's "mirroring S05" aside),
  mirror the FLATTENED encoding, not a product-of-products**: the
  co-signature's `type<X>` should be one product/struct with a responder
  field per operation across ALL member signatures, recovered the same
  alias-level way `row`'s `node_variant` is (this step's `detail::ops_pack`
  machinery — `signature_ops_pack`/`concat_ops_pack`/`row_ops_pack_t` — is
  reusable verbatim for pulling a flattened `Ops...` pack out of a row;
  only the final "turn the pack into X" step differs, a struct-of-fields
  instead of a variant).
- **Any adaptor that builds a continuation capturing a move-only value
  (a `one_shot`, or anything holding one) must NOT be threaded through
  `smd::typeclass::mbind`/`.bind`.** This step's central discovery (see
  above) — `FreeMonadImpl::bind`'s generic recursive traversal requires
  its `Fn` to be copyable at every level it might structurally recurse
  through, which a move-only continuation can never satisfy, and the
  failure only surfaces as a deep "use of deleted copy constructor"
  several template-instantiation layers down. `pair_run`'s loop
  (S06 §3) sounds like it will be a `run`-shaped explicit while-loop, not
  a `bind`-composed one, so it likely never hits this — but if S06's
  co-signature responders or the pairing logic ever compose a suspended
  program via `mbind`/`bind` where the continuation owns a `one_shot`
  (D-B's one-shot responder option), expect the same wall and build the
  node directly instead (this step's `discharge` non-matching branch is a
  worked example to copy from).
- **The deduction limitation is universal across this whole layer**: `Row`
  and `A` (and, for `discharge`, `Sig`) must be named explicitly at every
  `send<Row>`/`run<Row,A>`/`run_trace<Row,A>`/`discharge<Sig,Row,A>` call
  site. `pair_run<Sig,A,...>` (or whatever S06 names it) should expect the
  same and not spend time trying to make it deducible.
- **`traceable_operation`/`trace`/`render_operation`/`tracing`/`run_trace`
  (S04) are reused verbatim by this step's tests with zero changes for a
  row** — `Now`/`SleepFor`/`Send` each just need their own `operator<<`,
  same as any bare-signature op. S06's `pair_run_trace` sharing the same
  `Trace` type (the step file's own plan) should compose cleanly with
  these for programs over a row too, once S06 exists.
- **`member<Op, SigOrRow>` and `row_without_t<Sig, Row>` are available now
  if S06's pairing needs to check/split by signature** (e.g. if the
  co-signature side ever needs to know which of a row's signatures a given
  op belongs to) — no new membership machinery should be needed there.

## Open risks / TODOs

- **`discharge`'s own recursion depth for consecutive matching ops is
  native-call-stack-bound, not O(1) like `run`'s loop** (see Discoveries
  above) — untested at any real depth in this step. Flagging for S08 (the
  most likely place a long same-signature chain would first appear) or
  any step that stress-tests `discharge` the way S04 stress-tested `run`
  (its 500-round test).
- **`row<>` (a fully empty row, e.g. what discharging every member
  signature out of a row one at a time would eventually produce) was
  deliberately never exercised.** I designed this step's order-independence
  test to discharge exactly one signature and then finish with plain
  `run`/`run_trace` on the single remaining signature (per the step file's
  own "the all-at-once run may fall out for free" suggestion), specifically
  to avoid needing `Freer<row<>, A>` to be constructed or run — `row<>`'s
  `node_variant` would be `std::variant<>` (zero alternatives), which is a
  legal type but can never hold a value; I did not verify whether `run`
  over `row<>` compiles/behaves sensibly for an always-Pure program. If a
  later step (S08's retry-to-completion, most likely) wants to cascade
  `discharge` all the way down to zero remaining signatures, this is
  untested territory — check it explicitly rather than assuming it falls
  out.
- No other open risks specific to this step; the row encoding, `member`,
  `send<Row>`, and `discharge` are all exercised end-to-end against FD10's
  actual two-signature example, not a toy stand-in.
