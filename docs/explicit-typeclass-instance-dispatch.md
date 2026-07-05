# Explicit typeclass instance dispatch — design-surface record

*A harvestable record of the thread-by-value experiment. Captures the design
choice, the two prototypes that informed it, and the findings worth lifting
into the typeclass-object papers. Companion to `recursion-schemes-design.md`
§6.2 (lookup-based overloads) and decision D12.*

## The choice in one line

Every generic algorithm built on a typeclass object provides a **`_with`
form** that accepts the instance **by value as an explicit first argument**,
so a caller is never stuck when the global lookup is wrong or absent. Uniform
distinct `_with` name — not an overload of the lookup form.

## Three lookup modes

A typeclass instance is a *value* (`functor_typeclass<T>` is an object, not a
tag), which gives an algorithm three ways to reach an operation:

1. **Implicit lookup** — `fold_fix<int>(alg, tree)`; the instance is found by
   type via the registry.
2. **NTTP pinning** — bind the instance as a `const auto&` non-type template
   parameter defaulted to the lookup. Reference NTTP, not by-value: the
   `Functor`/`Monad` CRTP instances derive a `protected` (non-structural) base,
   so a by-value class NTTP is ill-formed; a reference NTTP binds the `inline
   constexpr` variable's static storage with no structural requirement. Put the
   data/layer parameter *before* the NTTP so the callable trails as a deduced
   parameter — then an inline lambda never has to be spelled to reach the pin.
3. **Explicit object (`_with`)** — `fold_fix_with(instance, alg, tree)`; the
   instance is passed by value and threaded unchanged down the recursion. It
   need not be registered anywhere.

This record is about mode 3.

## Why thread-by-value is not optional

Mode 3 is a *contract*, not a convenience, because two situations leave the
registry unable to answer:

- **No canonical default.** Some typeclasses have several equally-valid
  instances with no basis to prefer one. Monoid over `int` is the textbook
  case: sum, product, min, max are all monoids. A global `monoid_v<int>`
  merely imposes one choice (additive) on the whole program; `combine_all`
  can then only ever sum. Haskell disambiguates with newtypes (`Sum`,
  `Product`, `Min`, `Max`), but **C++ has no transparent, zero-overhead
  monomorphizing newtype** — wrapping means wrapper-type explosion and
  conversions at every boundary. Passing the instance by value folds one
  `vector<int>` four ways with four empty value objects, no wrappers, no
  global mutation. *(The monoid example motivates the contract only; the
  authoritative Monoid design lives in the finger-tree companion.)*
- **Unregistered / test-local / ODR-sensitive types.** A test wants to
  exercise the machinery without owning — or risking an ODR clash on — the
  global `functor_typeclass<T>` specialization. Mode 3 lets it hand in a
  local instance and register nothing.

## The experiment: `_with` vs overloading the name

Two branches off the same base (`experiment-fold-fix-object` @ `4c58e6b`),
identical behavior, full fixpoint suite green on gcc-17:

- **`experiment-schemes-with`** — distinct `_with` names
  (`fold_fix_with`/`unfold_fix_with`/`refold_with`/`zygo_with`), each
  validating its instance with a `static_assert` reusing `functor_instance_for`
  from `fmap.hpp`.
- **`experiment-schemes-overload`** — the instance-first form reuses the
  existing scheme name (`fold_fix(instance, alg, tree)`, etc.).

### Finding 1 — the tension is narrow

Reusing the name is only *contested* where the added instance-first argument
**collides in arity** with an existing overload — here the 3-arg `fmap_fn`
schemes (`fold_fix(alg, fmap_fn, tree)` vs `fold_fix(instance, alg, tree)`).
Where arities differ, overloading is free: `zygo`'s instance form is 4-arg
against a 3-arg base, so it overloads cleanly and even keeps a `static_assert`.

### Finding 2 — the colliding schemes force a `requires`-clause

To tell the two 3-arg overloads apart you must constrain the instance overload
(`requires functor_instance_for<...>`). Constraint partial ordering then
prefers the constrained instance overload over the unconstrained `fmap_fn`
one; a non-instance first argument fails the constraint and routes to
`fmap_fn`. Only the *new* overload needs constraining — the existing overloads
are untouched. (Verified on gcc and clang.) But note this drags a
`requires`-clause into what is otherwise not a dispatch problem: threading an
instance is the *same* operation regardless of where the instance came from.

### Finding 3 (decisive) — diagnostics

A bad instance is where the two diverge sharply. Passing a non-functor as the
instance:

**Overload variant** — the instance overload's constraint fails, so the call
**silently falls through to the unconstrained `fmap_fn` overload**, which then
tries to *call* the bad object as an fmap function inside its body:

```
recursion_schemes.hpp:16: error: no match for call to
  '(const <lambda>)(...fold_fix<...NotAFunctor...>..., variant<...>&)'
```

The error points deep inside `fold_fix`'s body, mentions nothing about a bad
functor instance, and buries the real type in template soup. The mistyped
instance was reinterpreted as an fmap function.

**`_with` variant** — leads with the named assertion:

```
recursion_schemes.hpp:121: error: static assertion failed:
  fold_fix_with: the functor instance has no fmap(fn, F<Fix<F>>) for this
  layer -- pass a functor typeclass object for F.
```

This is the crux. Because `_with` is not overload selection, validation is a
`static_assert` — an assertion of a requirement, with a direct actionable
message — instead of a SFINAE removal that lets a worse candidate produce a
misleading error. Overload resolution here does not merely fail to help; on
the colliding schemes it actively hurts.

### Decision (D12)

`_with` uniformly. The diagnostic win on the colliding schemes is decisive,
and one convention beats the split rule "reuse the name where arity is free,
else `_with`." If both are ever wanted, `_with` is the contract and name-reuse
(where arity-free) is at most additive sugar, never a substitute.

## Finding 4 — multi-site schemes need element-generic instances

Threading a *single* instance object constrains what that object must be.

- A **single-site** scheme (`fold_fix`) maps one fixed input layer type at
  every recursion level (`unwrap_fix` returns `F<Fix<F>>` throughout), so any
  instance — even one fixed to that element type — threads fine.
- A **multi-site** scheme like `zygo` maps *different* element types at its
  two fmap sites: `F<Fix<F>>` at the fold, `F<pair<Helper,Result>>` at the
  helper projection. A single threaded object must therefore be
  **element-generic** — its `fmap` templated over the element type. The
  library's per-element-type registered instances (`FunctorMap<A>` fixed to
  `A`) do *not* fit a single threaded object; `zygo_with`'s `static_assert`
  catches exactly this.

Registry lookup (`layer_fmap`) sidesteps the issue by re-resolving per site.
So the price of thread-by-value at multi-site schemes is an element-generic
instance. This pairs naturally with the register-nothing/test case (users
write element-generic instances anyway). **Open question:** whether to also
make the *library* functor instances element-generic (one object per functor
`F`, `fmap` templated on the element type — cheap for plain functors like
`NatF`/`ListF`, but `Free`/`Cofree` fix their `A` deliberately) so registered
instances too can thread through multi-site schemes.

## Artifacts

- Prototypes (frozen base `experiment-fold-fix-object` @ `4c58e6b`):
  `fold_fix_object.hpp` (object+inherit vs thread-by-value; established that
  inherit-from-instance is for multi-*operation* bodies, not single-op
  schemes), `zygo_object.hpp`, `monoid_explicit.hpp` (the contract via
  Monoid-over-int).
- `experiment-schemes-with` @ `d63b10f` — `_with` on real schemes (chosen).
- `experiment-schemes-overload` @ `0059131` — overload variant (comparison).

## Open follow-ons

1. Roll `_with` out to the remaining single-op schemes (para, apo, prepro,
   histo, futu, mutu, mendler, elgot, generalized family).
2. Decide the element-generic-instance question (Finding 4) for the library
   functor instances.
3. Reconcile the Monoid contract example with the finger-tree companion's
   Monoid before treating any monoid shape here as settled.
