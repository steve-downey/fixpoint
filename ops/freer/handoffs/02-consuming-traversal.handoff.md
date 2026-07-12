# Handoff — S02 Consuming traversal: fmap.hpp + free.hpp + box.hpp

- **Status:** DONE (gate passed)
- **Commit:** e8751c3 (`[freer] S02: consuming traversal`)
- **Date / agent:** 2026-07-11, Sonnet worker

## What changed

- `src/smd/fixpoint/box.hpp`: added `constexpr auto operator*() &&
  -> A&& { return std::move(*ptr); }` alongside the untouched
  `operator*() const -> A&`.
- `src/smd/fixpoint/fmap.hpp`: added two rvalue-constrained overloads
  of `layer_fmap` (modes 1/2, and mode 3) plus a
  `functor_instance_for_rvalue` concept mirroring the existing
  `functor_instance_for` for mode 3's constraint.
- `src/smd/fixpoint/free.hpp`: added consuming overloads of
  `FreeFunctorImpl::fmap` and `FreeMonadImpl::bind`, taking
  `smd::fixpoint::Free<F, A>&&` / `Free<F, X>&&`.
- Tests only, no shipped-header additions: `box.t.cpp`, `fmap.t.cpp`,
  `free.t.cpp`.
- `functors.hpp`/`concrete/functors.hpp` (IntListF's Functor
  instance) were **not** touched — confirmed by `git diff --stat`
  against this step's commit; the step's own const-path pure-data
  instances are exactly as FD4 said they'd stay: untouched, const-only.

## Exact new signatures (S03 compiles against these)

```c++
// box.hpp
constexpr auto operator*() && -> A&&;   // alongside operator*() const -> A&

// fmap.hpp -- modes 1/2 (implicit lookup / NTTP pin), consuming
template <class Layer,
          const auto &Typeclass =
              smd::typeclass::functor_typeclass<std::remove_cvref_t<Layer>>,
          class Fn>
    requires (!std::is_lvalue_reference_v<Layer>)
constexpr auto layer_fmap(Fn &&fn, Layer &&layer);

// fmap.hpp -- mode 3 (explicit object), consuming
template <class Typeclass, class Fn, class Layer>
concept functor_instance_for_rvalue =
    (!std::is_lvalue_reference_v<Layer>) &&
    requires(const Typeclass &tc, Fn &&fn, Layer &&layer) {
        tc.fmap(std::forward<Fn>(fn), std::forward<Layer>(layer));
    };

template <class Typeclass, class Fn, class Layer>
    requires functor_instance_for_rvalue<Typeclass, Fn, Layer>
constexpr auto layer_fmap(const Typeclass &tc, Fn &&fn, Layer &&layer);

// free.hpp -- FreeFunctorImpl (member of the CRTP Impl, deducing-this)
template <class Fn>
constexpr auto fmap(this auto &&self, Fn &&fn,
                    smd::fixpoint::Free<F, A> &&fr)
    -> smd::fixpoint::Free<F, remove_cvref_t<std::invoke_result_t<Fn, A &&>>>;

// free.hpp -- FreeMonadImpl
template <class X, class Fn>
constexpr auto bind(this auto &&self, smd::fixpoint::Free<F, X> &&m, Fn &&fn)
    -> remove_cvref_t<std::invoke_result_t<Fn, X &&>>;
```

Both `fmap`/`bind` consuming overloads are exposed identically to the
existing const overloads (`using FreeFunctorImpl<F,A>::fmap;` /
`using FreeMonadImpl<F,A>::bind;` in the `*Map` wrappers already cover
all overloads of the name), so `functor_typeclass<Free<F,A>>.fmap(...)`
and `monad_typeclass<Free<F,A>>.bind(...)` pick whichever overload
matches the argument's value category automatically — no new lookup
plumbing needed downstream.

`pure` and `layer_fmap`'s pure-data `const&` overloads are unchanged.

## How rvalue-vs-lvalue overload selection is constrained

- `layer_fmap`'s two new overloads use a *forwarding-reference* `Layer
  &&` template parameter (not a fixed rvalue-ref), constrained with
  `requires (!std::is_lvalue_reference_v<Layer>)` (mode 1/2) or baked
  into the `functor_instance_for_rvalue` concept (mode 3). For an
  lvalue argument, `Layer` deduces to `T&`, the constraint fails, and
  only the pre-existing `const Layer&` overload is viable — no
  ambiguity. For an rvalue argument, `Layer` deduces to `T` (no ref),
  the constraint holds, and **both** the const-ref and the new
  rvalue-ref overload become viable candidates; ordinary overload
  resolution then prefers `T&&` binding an rvalue over `const T&`
  binding the same rvalue (identical rule to move-ctor-vs-copy-ctor
  selection) — verified directly by the two "lvalue keeps const&,
  rvalue selects &&" tests in `fmap.t.cpp` using a
  `DualPathFunctorImpl` that tags each path differently (+1000 vs
  +2000) so the selected overload is observable, and by a compile
  test that an unregistered pure-data-only instance (const&-only
  `fmap`) is still reachable when called with an rvalue Layer (rvalue
  binds to const&, "fallthrough").
- `free.hpp`'s `fmap`/`bind` consuming overloads are plain non-template
  overloads on `Free<F,A>&&` vs. the existing `const Free<F,A>&` — same
  reasoning, no forwarding-reference subtlety needed there since the
  class template parameters `F`/`A` are already fixed by the enclosing
  class; only the member function's own value-category constraint on
  its second parameter matters, and that's ordinary `&&` vs `const&`
  overload resolution.

## A load-bearing discovery: `auto&&`, not hard-coded `&&`, in the recursive continuation

The first implementation hard-coded the recursive lambda's `child`
parameter as `smd::fixpoint::Free<F, X> &&child`, reasoning "this is
the consuming path, the child must be an rvalue." That broke this
file's own **pre-existing** tests (e.g. `"free monad law: associativity
spot-check"`, `monad.bind(monad.bind(m, k), h)`): the *outer* `bind`
call receives the *inner* `bind`'s return value as a prvalue, so it now
picks the new consuming overload -- even though `F = IntListF` is a
pure-data functor whose only Functor instance
(`concrete/functors.hpp`'s `ListFFunctorImpl`) has just a `const&`
`fmap` that hands children out via `*c.tail` (an **lvalue**, `A&`, per
`box.hpp`'s const-qualified `operator*`). A hard-coded `Free<F,X>&&`
parameter can't bind that lvalue at all -- hard compile error deep in
`functors.hpp`.

Fix: the recursive closure takes `auto &&child` and forwards it
(`self.bind(std::forward<decltype(child)>(child), fn)` /
`self.fmap(fn, std::forward<decltype(child)>(child))`). Ordinary
overload resolution then routes to whichever `bind`/`fmap` overload
matches what the underlying `F`'s Functor instance actually delivered:
an lvalue child (pure-data `F`, FD4's fallthrough) routes back to the
**pre-existing const-path** `bind`/`fmap`; a genuine rvalue child (a
lazy/Coyoneda-shaped `F` -- S03's real layer) routes to the new
consuming overload. **This is required, not optional, for S03**:
`Freer<Sig,A> = Free<Sig::template type, A>`'s layer is
consuming-only (FD6/FD3 -- no const `fmap` at all, by design), so
every child delivered to `bind`'s recursive continuation through it
*will* be a genuine rvalue (materialized via `*std::move(box)`, S02's
new `Box::operator*() &&`, inside the deferred continuation the layer
stores) -- the `auto&&`+forward pattern is what makes that rvalue
actually reach the consuming `bind` overload rather than silently (or
not-so-silently) trying to bind it as `const&`.

One corollary this creates: any test-local *lazy* layer (like this
step's `LazyLayer<X>` mock, and S03's real layer) whose continuation
type accepts only an rvalue-qualified payload (`X&&`) will invoke the
top-level user-supplied `fn` with a genuine `X&&` when reached directly
from `Pure`, but a callback used against a **pure-data** `F`'s Roll
chunk (e.g. this step's `Free<IntListF, MoveOnlyInt>` move-only smoke
test) must be written generically (`auto&&x`, not `X&&x`) if it's ever
invoked while recursing through such an `F`'s Roll layer, since it'll
receive `const X&` there. `free.t.cpp`'s two move-only smoke tests
(`add_ten`, `k`) both use `auto&&` for exactly this reason, with an
inline comment.

## The lazy-layer test pattern (S03 promotes this to the real thing)

`free.t.cpp`'s `"consuming bind: continuation owns its captures
(FD4)"` builds a minimal, test-local mock of S03's real Coyoneda
layer:

```c++
template <class X>
struct LazyLayer { std::move_only_function<smd::fixpoint::Box<X>(int) &&> k; };
```

registered as `functor_typeclass<LazyLayer<X>>` with **only** a
consuming `fmap` (no const overload, matching S03's Pitfalls: "the
const path must simply never instantiate"). Its `fmap` does **not**
invoke the incoming closure -- it post-composes: `[k = std::move(layer.k),
fn = std::forward<Fn>(fn)](int response) mutable -> Box<Y> { return
make_box<Y>(std::invoke(fn, *std::move(k)(response))); }` (exactly
FD6's sketch, using S02's new `Box::operator*() &&`). `Free<LazyLayer,
MoveOnlyInt>` is the "layer" instantiation.

The test then: builds a one-layer `LazyFree` program, calls the
**consuming** `bind` on it with a continuation `k` that captures real
state (`int bump`, not stateless!), extracts the resulting `LazyLayer`'s
stored continuation *out of* an IIFE whose scope (program, k, the
`monad_typeclass` reference, the bound `Free`) then ends, and only
*then*, from the outer scope, invokes the extracted continuation and
checks the result.

**Load-bearing gotcha found empirically, worth repeating to S03
verbatim**: the first version of this test used a **stateless**
(non-capturing) `k` lambda and a `self` that (through `this auto&&`)
resolves to a reference into the `monad_typeclass<...>` *global static*
object. I deliberately re-broke the fix (reverted to `[&self, &fn]` in
`FreeMonadImpl::bind`'s recursive closure) to check the test would
catch it -- **it silently passed**. Root cause: `self` never dangles
(it aliases a `inline constexpr` static-storage object, not a stack
frame), and a stateless lambda's invocation touches no captured data,
so a dangling reference to it is never actually *read* -- UB in the
strict sense, but Asan has nothing to observe. Only after giving the
continuation genuine captured state (`int bump`) did the same broken
`[&self, &fn]` reliably fail under Asan (`stack-use-after-scope`,
caught under the **default** Asan config, no `ASAN_OPTIONS` needed) --
and the fix made it pass again. **S03's promoted version of this test
must give its continuation real captured state**, or the Asan gate is
decorative. I re-verified the final, committed version (with `self`
captured by value, not by reference) both fails-when-reintroduced and
passes-when-correct before committing.

## Verification evidence

```
make TOOLCHAIN=gcc-16 test    -> 100% tests passed, 229/229 (220 baseline + 9 new)
make TOOLCHAIN=clang-23 test  -> 100% tests passed, 229/229 (220 baseline + 9 new)
make lint                     -> all hooks passed (clang-format required one
                                  auto-fix pass, then clean)
```

New tests (9): `box.t.cpp` — `"Box - RvalueDerefMovesOut"`, `"Box -
RvalueDerefRoundTripsMoveOnlyType"`; `fmap.t.cpp` — `"fmap - mode 1
rvalue call falls through to a const&-only registered instance"`,
`"fmap - mode 3 rvalue call falls through to a const&-only explicit
instance"`, `"fmap - mode 2 NTTP pin: lvalue keeps const&, rvalue
selects &&"`, `"fmap - mode 3 explicit object: lvalue keeps const&,
rvalue selects &&"`; `free.t.cpp` — `"free consuming fmap: maps a
move-only Pure value and recurses through a Roll chunk"`, `"free
consuming bind: sequences through a Roll chunk of a move-only
payload"`, `"consuming bind: continuation owns its captures (FD4)"`.

No pre-existing test was edited (`git diff` on this commit is
additive-only per file; confirmed by re-running the full pre-existing
suite unchanged in content).

## Cross-compiler divergences

None on the primary pins (gcc-16 / clang-23): identical 229/229 pass
counts, identical overload-resolution behavior (verified via the
dual-path lvalue/rvalue selection tests, which assert on which branch
ran, not just that something compiled).

One **non-pin** finding worth recording: the *system* `c++` resolved
to GCC 15.2.0 (Ubuntu default, not a pin) fails to compile
`box.hpp` with:

```
box.hpp:55:20: error: 'constexpr A&& smd::fixpoint::Box<A>::operator*() &&'
  cannot be overloaded with 'constexpr A& smd::fixpoint::Box<A>::operator*() const'
  [-Wtemplate-body]
```

This is very likely a GCC-15-specific `-Wtemplate-body` false positive
(that diagnostic pass is new-ish, prone to early false positives) on a
ref-qualifier-plus-cv-qualifier overload set inside a class template:
both gcc-16 and clang-23 accept and correctly run this exact code (the
dual-path overload-selection tests specifically prove the two
overloads coexist and resolve correctly), and mixing `const` (no ref
qualifier) with `&&` (differing in both cv- and ref-qualification) is
the same pattern the standard library itself uses (cf.
`optional<T>::value()`'s four overloads). Not investigated further --
out of scope (system `c++` is not a pin; S00 pinned gcc-16/clang-23).
Flagging in case FD9's eventual toolchain-floor language widens to
touch gcc-15 or the system-default toolchain path.

## Deviations from the plan / design

None requiring a `DEVIATIONS.md` row: the discoveries above (the
`auto&&`-forwarding fix, the stateless-continuation Asan blind spot)
are implementation-correctness fixes needed to make FD4's own stated
requirements ("pure-data instance ... still reachable through the new
overload" and "the S02 gate includes a deferred-invocation test ... to
police exactly this class of bug") actually hold — not contradictions
of what FD4 says, just non-obvious mechanics in *how* to satisfy it
that are worth recording for S03. No design question was resolved
unilaterally; both were compile/runtime correctness bugs surfaced by
the pre-existing test suite and by deliberately re-breaking my own fix
to check the gate's teeth.

## Discoveries affecting later steps

- See "A load-bearing discovery" above (`auto&&` in the recursive
  continuation) — S03's real layer is consuming-only, so this is the
  mechanism that lets a genuine rvalue reach the consuming `bind`/`fmap`
  overload from inside a stored, later-invoked continuation.
- See "The lazy-layer test pattern" above (stateless-continuation Asan
  blind spot) — S03's promoted version of this test must use a
  continuation with real captured state, verified by deliberately
  re-breaking the capture and confirming the test then fails.
- `layer_fmap`'s two new overloads are additive; nothing about mode 1/2
  vs. mode 3 dispatch changed for the const-lvalue path. D-A's eventual
  registration mechanism (still open, orchestrator-owned) plugs into
  the *same* `functor_typeclass<...>` lookup point either overload
  uses — S02 didn't have to anticipate D-A's specific resolution.
- `Box::operator*() &&` returns `A&&`; a caller must still route through
  it explicitly (e.g. `*std::move(k)(r)`, as `LazyLayerFunctorImpl::fmap`
  does in this step's test, and as FD6's own sketch does) — it is not
  automatically selected just because the `Box` itself was constructed
  as a temporary; the deref expression's own value category is what's
  overload-resolved.
- `FreeFunctorImpl`/`FreeMonadImpl`'s consuming overloads visit by
  `std::holds_alternative` + `std::get<...>(std::move(...))` rather than
  `std::visit(overloaded{...}, std::move(...))`: the latter would
  construct **both** visitor alternatives' lambdas eagerly before
  dispatch, and since both need to move-capture the same `fn`, one
  would silently move from an already-moved-from `fn` for the other.
  Branching first sidesteps this. S03's Pitfalls note already
  recommends `std::visit(std::move(layer.node))` with `overloaded` for
  the *layer's own* fmap — that's fine there because each `std::visit`
  alternative in FD6's sketch captures a *different* per-node `k` (not
  one shared external `fn`), so the double-move hazard doesn't arise in
  that shape. Worth having this distinction in mind if S03's actual
  code ends up sharing a captured value across alternatives anywhere.

## Forward notes for the NEXT step (written after reading its step file)

S03 (`ops/freer/steps/03-signature-layer.md`) builds the real
`freer.hpp`: `unit`, `operation` concept, `impure_node<Op,X>`,
`signature<Ops...>`, `Freer<Sig,A> = Free<Sig::template type, A>`, and
the generic Coyoneda `Functor` instance. Notes for that work:

- The instance's `fmap` must be **consuming-only** (no `const&`
  overload at all) — this step's `layer_fmap` overloads and
  `FreeFunctorImpl`/`FreeMonadImpl`'s consuming overloads are exactly
  what make that legal: `layer_fmap`'s new rvalue overloads dispatch to
  whatever `functor_typeclass<Layer>` provides, and since S03's layer
  will *only* register a consuming `fmap`, the const-lvalue `layer_fmap`
  overload (mode 1/2/3) simply won't find a matching `Typeclass.fmap`
  overload if ever reached with an lvalue — which is the intended "no
  const path" compile error FD4/S03's own Pitfalls call for, not
  something S02 needs to special-case.
- Use `smd::fixpoint::Box<X>::operator*() &&` (this step) to move out of
  the boxed continuation result inside the instance's `fmap` — exactly
  as `LazyLayerFunctorImpl::fmap` does in `free.t.cpp` (this step's
  test file), which is a close structural preview of what S03's real
  instance should look like (only the `Op op;` member and
  `Op::response`-typed continuation parameter differ from the test
  mock's bare `int`).
- When `FreeMonadImpl::bind` (or `FreeFunctorImpl::fmap`) recurses
  through `Sig::template type<X>` via `layer_fmap`, the recursive
  closure's `child` parameter is `auto&&` (this step's fix, see
  "load-bearing discovery" above) — for S03's real layer this will
  always deduce as a genuine rvalue when the continuation is actually
  invoked (materialized via `Box::operator*() &&`), so `bind`'s
  consuming overload is what actually runs end-to-end. No action
  needed in `freer.hpp` itself for this — it's already true of the
  `free.hpp` machinery S03 builds on — but it explains *why* S01's
  hand-rolled probe re-expressed against the real library should
  "just work" once the instance is registered.
- `std::visit(smd::fixpoint::overloaded{...}, std::move(layer.node))`
  taking alternatives by `&&` (per S03's own Pitfalls) is fine and
  preferred over this step's `holds_alternative`+`get` pattern *for the
  layer's own fmap specifically*, because each signature alternative in
  `signature<Ops...>` moves its *own* per-node continuation, not one
  externally shared `fn` — the double-move hazard this step hit in
  `free.hpp` (shared `fn` across two eagerly-constructed alternatives)
  doesn't apply there. Use `std::visit` freely for the layer's own
  instance; only be wary of it in `free.hpp`-shaped code that shares one
  outer capture across multiple `std::visit` alternatives.
- `remove_cvref_t` (used throughout this step's new overloads' trailing
  return types) lives in `namespace smd::typeclass`
  (`detail/typeclass_base.hpp`), already `#include`d transitively via
  `smd/typeclass/functor.hpp`/`monad.hpp` — no new include needed if
  `freer.hpp` also pulls those in, which it will need to for
  `functor_typeclass`/`monad_typeclass` anyway.

## Open risks / TODOs

- The gcc-15/system-`c++` `-Wtemplate-body` finding above (not a pin,
  not investigated further) — flag to the orchestrator if FD9's
  toolchain-floor language is ever widened to include gcc-15 or an
  unpinned system default.
- D-A/D-B are still unresolved (orchestrator-owned, dep on S01) — S02
  didn't need either decision (it only touches `fmap.hpp`/`free.hpp`/
  `box.hpp`, not signature/continuation representation), but S03 does
  and must not proceed without them per its own step file.
