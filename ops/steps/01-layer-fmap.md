# S01 — layer_fmap and typeclass-lookup scheme overloads

**Goal.** Bridge `smd::fixpoint` schemes to `smd::typeclass` functor
dispatch: a `layer_fmap` helper that looks up
`functor_typeclass<Layer>`, plus overloads of
`fold_fix`/`unfold_fix`/`refold` that no longer take an explicit
`fmap_fn`. Prove it with a functor_typeclass instance for the NatF test
functor.

**Depends on:** S00.
**Design refs:** §3 D2, §4, §6.1, §6.2, §7.1.

## Do
1. **New header `src/smd/fixpoint/fmap.hpp`** with the `layer_fmap`
   helper exactly as design §4 sketches (constexpr, perfect-forwarding,
   keyed on `std::remove_cvref_t` of the layer). Include
   `<smd/typeclass/functor.hpp>`. Note: this makes the fixpoint module
   depend on the typeclass module — both are header FILE_SETs on the
   same `fixpoint.fixpoint` target, so no CMake target changes are
   needed beyond adding `fmap.hpp` to `smd_fixpoint_headers`.
2. **Lookup overloads in `recursion_schemes.hpp`** (design §6.2):
   - `template <class Result, template <class> class F, class Algebra>
     constexpr auto fold_fix(const Algebra&, const Fix<F>&) -> Result;`
   - `template <template <class> class F, class Coalgebra, class Seed>
     constexpr auto unfold_fix(const Coalgebra&, const Seed&) -> Fix<F>;`
   - `template <class Result, template <class> class F, class Algebra,
     class Coalgebra, class Seed> constexpr auto refold(...) -> Result;`
   Implement them directly with `layer_fmap` (do not adapt through the
   explicit-fmap versions — the lambda-wrapping cost in readability
   isn't worth it). Leave the existing explicit-fmap overloads
   untouched. Also mark the existing three `constexpr` (design D10) —
   this is the one sanctioned edit to existing code.
3. **Tests** in a new `src/smd/fixpoint/fmap.t.cpp` (add to
   `smd_fixpoint_test`): define NatF (Zero/Succ, as in
   `recursion_schemes.t.cpp`) *plus* its `functor_typeclass` instance
   following design §6.1 verbatim; then:
   - lookup `fold_fix<int>(count_algebra, two)` == 2 (mirrors the
     existing explicit-fmap test);
   - lookup `unfold_fix<NatF>(coalg, 5)` folds back to 5;
   - lookup `refold` == explicit-fmap `refold` for n in 0..10;
   - a `static_assert` doing the count fold at compile time (D10) —
     wrap the tree construction + fold in a `consteval`-invoked lambda
     or constexpr function so Box allocation stays transient.

## Build
`make TOOLCHAIN=gcc-16 test`

## Verify (gate)
- All prior tests still pass; new tests pass; total > 45.
- The static_assert compiles (it is its own proof).
- `functor_typeclass<NatF<X>>` specialization compiles from *outside*
  namespace smd::typeclass by reopening it (this validates the D2
  pattern every later step relies on).

## Done when
Gate green; committed `[schemes] S01: layer_fmap + lookup overloads`.

## Capture in handoff
The exact `layer_fmap` signature as landed; whether reopening
`namespace smd::typeclass` from a fixpoint header caused any ordering
subtleties; the constexpr status of the classic three (did marking them
constexpr just work under gcc-16?).

## Pitfalls
- The variable-template partial specialization must be declared before
  any use that would instantiate the primary (`std::false_type`)
  — keep instance definitions in the same header as the functor types,
  above any scheme calls in the TU.
- `fold_fix<Result>` lookup overload vs existing 3-arg overload: the
  arity differs (2 vs 3 args), so overload resolution is unambiguous —
  but double-check the deprecated `cata` shim still compiles unchanged.
- Don't try to make the *variant* itself the specialization key
  indirectly via `NatF<A>` in a way GCC can't deduce — the key IS
  `std::variant<Zero, Succ<A>>`; if deduction fails, spell the variant
  out in the specialization (and record a deviation).
