# Handoff — S07 Cofree comonad and histo

- **Status:** DONE (gate passed)
- **Commit:** 6c25a86 — `[schemes] S07: Cofree + histo`
- **Date / agent:** 2026-07-04, background execution agent

## What changed

- New `src/smd/fixpoint/cofree.hpp` (namespace `smd::fixpoint` for the
  type + free helpers, reopens `namespace smd::typeclass` for the
  instances, exactly the house layout): `Cofree<F, A>` per design §5.3 —
  aggregate `{A head; F<Cofree<F,A>> tail;}` plus a defaulted `friend
  operator==`. Free helpers `extract(c) -> const A&` and `unwrap_cofree(c)
  -> const F<Cofree<F,A>>&`. Instances:
  - `functor_typeclass<Cofree<F,A>>` (`CofreeFunctorImpl<F,A>`/`Map`) —
    `fmap` maps every head throughout the whole tree (not just the root),
    recursing through `tail` via `layer_fmap`. Fixed to the class-level
    `A` (its recursion is always same-depth, unlike duplicate's doubled
    structure), but **its trailing return type is written out explicitly**
    (`-> Cofree<F, remove_cvref_t<invoke_result_t<Fn, const A&>>>`) rather
    than left as deduced `auto` — see Discoveries, this is load-bearing.
  - `comonad_typeclass<Cofree<F,A>>` (`CofreeComonadImpl<F,A>`/`Map`) —
    `extract`/`duplicate`/`fmap` are all templated over their own element
    parameter `X` (not the class-level `A`), per the S03 handoff's
    Comonad/Monad genericity discovery: `duplicate` produces the doubled
    `Cofree<F, Cofree<F,A>>`, and `extend`'s derived `fmap` must consume
    that doubled type. `duplicate(c) = Cofree<F, Cofree<F,X>>{c,
    layer_fmap(recursive-duplicate, c.tail)}`. `fmap` also has an explicit
    trailing return type for the same self-recursion reason as the
    Functor instance.
- New `src/smd/fixpoint/histo.hpp`: `histo<Result>(algebra, tree) ->
  Result` = `extract(fold_fix<Cofree<F,Result>>(λx. Cofree{algebra(x), x},
  tree))` — a direct, unfused transcription of Fokkinga's equation (design
  §7.5).
- `src/smd/fixpoint/functors.hpp`: **`Zero` and `Succ<A>` both gained a
  defaulted `friend operator==`.** Neither had one before this step (see
  Discoveries) — required for `Cofree<NatF, A>`'s own defaulted `==` to
  compile, since its `tail` member is `NatF<Cofree<NatF,A>> =
  std::variant<Zero, Succ<Cofree<NatF,A>>>`, and `std::variant`'s `==`
  needs every alternative comparable. Not filed as an `ops/DEVIATIONS.md`
  row — this is a bugfix to an existing type filling in a gap the design
  never called out (design §5.3 already assumed Cofree's `==` "if the
  members support it"; they didn't, so this makes that true), not a place
  reality diverged from a documented decision.
- `src/smd/fixpoint/CMakeLists.txt` / `src/examples/CMakeLists.txt`:
  append-only additions (`cofree.hpp histo.hpp` / `cofree.t.cpp
  histo.t.cpp` to the FILE_SET/test sources; one executable+install block
  for `histo_coin_change`), same two-file pattern every prior step
  touched.
- New `src/smd/fixpoint/cofree.t.cpp` (7 tests): hand-built annotated Nat
  (0←1←2, each node's head = its own value) exercising
  `extract`/`unwrap_cofree` directly; structural-equality smoke; fmap
  incrementing every head all the way down (not just the root); the two
  comonad laws (`extract . duplicate == id`, `fmap(extract) . duplicate ==
  id`) and `extend == fmap . duplicate`, all via `comonad_typeclass`;
  `HeaderIsIdempotent`; one `static_assert` exercising both the comonad
  law and fmap.
- New `src/smd/fixpoint/histo.t.cpp` (4 tests): the §9 law (a heads-only
  algebra — i.e. one that discards history and only reads each child's
  `extract`ed Result — degenerates to `fold_fix` with the corresponding
  plain algebra, Nat 0..10); Fibonacci via histo (0..10, matches naive
  fib); coin-change {1,4,5} minimal counts (n=8→2, n=12→3, plus 0/1/4/5
  spot-checks); `HeaderIsIdempotent`; a `histo_fib_constexpr_smoke()`
  helper (own local lambda algebra, not the runtime test's free function —
  see Discoveries) backing `static_assert(histo-fib(6) == 8)`.
- New `src/examples/histo_coin_change.cpp`: prints `minCoins(n,
  coins={1,4,5})` for n in {0,1,4,5,8,12,13}, the algebra's history-walk
  (3 steps back from the n-1 node for n-4, 4 steps back for n-5) commented
  inline.

## Verification evidence

- `make TOOLCHAIN=gcc-16 test`: **100% tests passed, 0 tests failed out
  of 133** (baseline going into this step was **122**, not the 121 that
  `ops/PLAN.md`'s S06 row cites — a pre-existing, already-committed
  follow-up commit `94ae92f [schemes] S06 follow-up: fix weak prepro
  depth-discriminator test (DEV-01)` sits between S06's handoff commit
  and this step's start and adds one test of its own; net for S07 is
  +11, matching the 11 new TEST_CASEs across `cofree.t.cpp`/`histo.t.cpp`
  exactly). All pre-existing tests still pass unchanged.
- Explicit rebuild of the touched/new files (`touch` + `make
  TOOLCHAIN=gcc-16 compile`) with `grep -i warning` over the output:
  empty (no compiler warnings; the one "warning" `grep` matches is an
  unrelated `uv`/venv notice on stderr, not from the compiler).
- `.build/build-gcc-16/src/examples/Asan/histo_coin_change` output
  (exit 0):
  ```
  minCoins(0, coins={1,4,5}) = 0
  minCoins(1, coins={1,4,5}) = 1
  minCoins(4, coins={1,4,5}) = 1
  minCoins(5, coins={1,4,5}) = 1
  minCoins(8, coins={1,4,5}) = 2
  minCoins(12, coins={1,4,5}) = 3
  minCoins(13, coins={1,4,5}) = 3
  ```
  matches hand-computed values (8 = 4+4; 12 = 4+4+4).
- Both `static_assert`s (`cofree_constexpr_smoke`,
  `histo_fib_constexpr_smoke`) compile clean under gcc-16/C++26.

## Deviations from the plan / design

No `ops/DEVIATIONS.md` row filed. The `Zero`/`Succ<A>` defaulted `==`
addition (see above) is a bugfix filling a gap the design already assumed
away, not a place reality contradicted a documented decision — nothing to
reconcile in the design doc, it already says Cofree gets `==` "if the
members support it" and now they do.

## Discoveries affecting later steps

- **A member function with a *deduced* (`auto`) return type cannot call
  itself recursively, even one level down inside a lambda passed to
  another function.** Both `CofreeFunctorImpl::fmap` and
  `CofreeComonadImpl::fmap` recurse via `self.fmap(fn, child)` inside the
  lambda passed to `layer_fmap`; with `constexpr auto fmap(...)` (no
  trailing return type), GCC 16 rejects this as "use of ... before
  deduction of 'auto'" — the return type of the *same* specialization
  being defined isn't known yet when the recursive call is type-checked,
  even though the recursion is structurally sound (same `F`, same
  element-type parameter, so it isn't actually an infinite type
  regression). **Fix: write the trailing return type out explicitly**
  (`-> Cofree<F, remove_cvref_t<invoke_result_t<Fn, const A&>>>` /
  `-> Cofree<F, remove_cvref_t<invoke_result_t<Fn, const X&>>>`), computed
  from `Fn`/the element type alone (never referencing the function's own
  return type) — this breaks the chicken-and-egg cycle. `duplicate` never
  hit this because its trailing return type was already written out
  explicitly from the start (nothing in its type depends on the
  recursive call's *result*, just on `X` and `F`). **This is exactly the
  situation S08's `monad_typeclass<Free<F,A>>::bind` will be in** (its
  step file's own Pitfalls section already flags "make the recursive
  lambda a named function template or the deduction cycle bites (same
  trick as everywhere: explicit return type)") — do it from the start
  there, don't discover it via a compile error like this step did.
- **`Zero` and `Succ<A>` (functors.hpp, S02) had no `operator==` before
  this step, and nothing before S07 needed one** (S06's handoff already
  flagged "`Fix<F>` has no `operator==`... compare via a fold instead" —
  but that's a different, deeper gap: even the *base functor layer types*
  themselves weren't comparable, independent of `Fix`). Cofree is the
  first type in the tree whose own defaulted `==` needs a base functor's
  alternatives to be comparable (its `tail : F<Cofree<F,A>>` is a
  `std::variant`, and `std::variant::operator==` requires every
  alternative comparable). Fixed by adding defaulted `friend operator==`
  to both `Zero` and `Succ<A>` directly (both trivially comparable: empty
  struct / a single `Box<A>` field, and `Box` already provides `==`).
  **`IntListF`'s `Nil<E>` has the exact same latent gap** (also an empty
  aggregate with no `==`) — nobody has hit it yet because nothing has
  asked for `Cofree<IntListF, A>`'s `==`, but S08's `Free<IntListF, int>`
  test fixture (the step file's own suggested type for Free's monad-law
  tests) will hit it immediately if any test tries to compare two
  `Free<IntListF, int>` values with `==`. If you need that, add the same
  one-line defaulted `friend operator==` to `Nil<E>` in `functors.hpp`
  (same fix, same justification, not a new pattern to invent).
  `IntTreeF`'s `Leaf<E>{E value;}`/`Node<A>{Box<A> left,right;}` and
  `ExprF`'s `Const<A>{int val;}`/`Add`/`Mul` are all already comparable
  (every field is either a plain comparable value or a `Box`), so only
  the *empty* alternatives (`Zero`, `Nil<E>`) are actually at risk.
- **`layer_fmap` composes cleanly as the recursive step inside a
  hand-written CRTP `Impl` method**, not just inside `fold_fix`/
  `unfold_fix` or another free-function scheme — `duplicate`'s
  `layer_fmap(recursive-duplicate-lambda, c.tail)` and both `fmap`s'
  analogous calls confirm this yet again (S04/S05/S06 all made the same
  observation for free functions; this step confirms it holds equally
  well for `this auto&&`-based member functions).
- **The recursive `self.fmap(...)` / `self.duplicate(...)` pattern
  works exactly like `Functor<Impl>::replace`'s `self.fmap(...)` call**
  (comonad.hpp/functor.hpp's own derived operations) — no new deducing-
  this technique was needed, just remembering that a *recursive* use of
  it inside the same `Impl` needs the explicit-return-type fix above.
- **Two of Fokkinga's degeneracy laws share the same shape as zygo's**:
  `heads_only_algebra` in `histo.t.cpp` builds its `F<Result>` layer via
  `layer_fmap(extract, layer)` then defers to the plain algebra — this
  is structurally identical to zygo's "ignore the helper" law test
  (`zygo.t.cpp`), just with `extract` standing in for `.second`/`first`
  projection. Worth reusing this shape if S08/S09's own degeneracy laws
  need the same "project down to the plain carrier" move.

## Forward notes for the NEXT step (S08 — Free monad and futu)

- **S08 depends on S02 and S03, not S07** — no file overlap with
  `cofree.hpp`/`histo.hpp`/their tests. `src/smd/fixpoint/CMakeLists.txt`
  and `src/examples/CMakeLists.txt` are again the only two files this
  step touched that S08 will also touch (append `free.hpp futu.hpp` /
  `free.t.cpp futu.t.cpp` to the FILE_SET/test sources, add one
  executable+install block for `futu_rle_decode`).
- **Apply the explicit-trailing-return-type fix to
  `monad_typeclass<Free<F,A>>::bind` from the start**, per this step's
  own Discoveries above: `bind`'s recursive case (`Roll layer ->
  roll_free(layer_fmap(recursive-bind-with-k, layer))`) is exactly the
  same self-recursive-through-a-lambda shape that bit `Cofree`'s `fmap`
  here. Write `bind`'s trailing return type out explicitly as `->
  Free<F, B>` where `B` is computed from `A`/`k` alone (e.g. `B =
  remove_cvref_t<decltype(k(std::declval<const A&>()))>`'s element type,
  whatever the exact extraction looks like for your `k`'s return type),
  never referencing `bind`'s own deduced return type. The step file's
  own Pitfalls section already anticipated this in the abstract ("make
  the recursive lambda a named function template... same trick as
  everywhere: explicit return type") — this handoff confirms it's a
  real, not hypothetical, GCC 16 diagnostic (`use of ... before
  deduction of 'auto'`), not just style advice.
- **Watch for the same missing-`operator==` gap** if any Free-monad test
  wants to compare two `Free<F, A>` values (e.g. for a law spot-check)
  where `F` is `IntListF`: `Nil<int>` has no `operator==` yet (see this
  step's Discoveries) — add one the same one-line way this step did for
  `Zero`/`Succ<A>`, don't invent a different mechanism. If the step's
  monad-law tests only ever compare `int`/scalar results (not whole
  `Free<F,A>` trees) rather than reaching for `Free`'s own `==`, this
  may not come up at all — check what the actual law assertions compare
  before assuming you need it.
- **`layer_fmap` is still the right tool for `bind`'s Roll-layer
  recursion** (`layer_fmap(recursive-bind, layer)`, mirroring
  `duplicate`'s `layer_fmap(recursive-duplicate, c.tail)` in this step) —
  no new composition technique needed.
- **`histo`'s `Cofree<F, Result>` carrier and `futu`'s `Free<F, Seed>`
  carrier are exact duals** (design §5.3/§5.4, §7.5) — if it helps to
  cross-check `futu`'s worker against something concrete, `histo.hpp`'s
  four-line body (`extract(fold_fix<Cofree<F,Result>>(combined, tree))`)
  is the fold-side mirror of what `futu`'s unfold-side equation should
  look like.
- Gate remains `make TOOLCHAIN=gcc-16 test` fully green; S08 additionally
  requires one new example binary (`futu_rle_decode`) to run and exit 0
  with correct decodes (the step file's own worked example: RLE
  `{{2,7},{3,1}}`-style input decodes to `[7,7,1,1,1]`).

## Open risks / TODOs

- gcc-17 secondary smoke was not re-run this step (advisory only per
  design D9/plan); still outstanding since S00, not blocking.
- `ops/PLAN.md`'s S06 status-log row cites commit `b10a095` and total
  `121`; the true state of `main`/this branch at S06's "done" point (per
  the follow-up commit `94ae92f`) is 122 tests. Not this step's row to
  fix (it's S06's row, and the follow-up commit already exists and is
  already filed under DEV-01), but flagging it so nobody is confused
  when total counts don't telescope cleanly from the table alone — see
  git log, not just the table, if a future step's arithmetic looks off
  by one.
- **Mid-task correction**: while investigating this exact test-count
  discrepancy, I accidentally ran `git stash` with uncommitted S07 work
  present, then immediately `git stash pop`'d it back before anything
  else touched the tree — verified via `git status`/file line counts
  that all files were restored intact before committing. No lasting
  effect, but noting it for the record since it's the kind of near-miss
  worth a future agent double-checking (`git status`) before commit if
  they also go spelunking through `git log`/`git stash` mid-step.
- None specific to S07's own scope were left open — gate is green, every
  bullet in the step file's "Tests"/"Example" sections has a
  corresponding test or example, and the example binary's output was run
  and pasted above (not just claimed).
