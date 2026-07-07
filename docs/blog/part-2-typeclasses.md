<div class="abstract" id="orgd0d9cfa">
<p>
Every recursion scheme needs exactly one thing from a base functor: <code>fmap</code>,
apply a function at each child position of one layer. Haskell finds it by
typeclass resolution. C++ has no such resolver, so I built a small one:
variable templates holding instance objects, one bridge function, and three
ways for an algorithm to reach an operation &#x2014; implicit lookup, an NTTP
pin, and an explicit object argument.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Part 1 - Fix and Box: Tying the Knot ←](part-1-fixpoint.md)

</nav>


# What the Schemes Need

`fold_fix` recurses by saying "apply me at every child position of this layer." For `ExprF` that means both sides of an `Add`; for `NatF` the one predecessor of a `Succ`; for a `Const` or `Zero`, nowhere at all. Only the functor knows its own shape. The operation is `fmap : (A -> B) -> F<A> -> F<B>`, and every scheme in this series &#x2014; all of them, until Mendler style in Part 9 opts out &#x2014; consumes precisely this one hook.

The question is dispatch. How does generic scheme code, holding a layer of unknown type, find the right `fmap`? Not virtual functions: the layers are `std::variant` values, and constexpr. Not ADL customization points: the invariants comment in the functor header is blunt about keeping lookup explicit rather than growing parallel ADL paths. The answer is the typeclass-object pattern I described in (Downey, Steve, 2023) and (Downey, Steve, 2024): a concept's operations live in an ordinary object, and a variable template maps types to instances.


# The Instance Registry

From [`src/smd/typeclass/functor.hpp`](../../src/smd/typeclass/functor.hpp):

```cpp
/** CRTP base for Functor instances.
 * `Impl` must provide `fmap(f, container)`; `replace` is derived from it.
 */
template <class Impl>
struct Functor : protected Impl {
    using Impl::fmap;

    /** Replaces every element of `value` with `replacement`, ignoring the
     * original element values.
     */
    template <class T, class U>
    auto replace(this auto &&self, T &&value, U &&replacement) {
        return self.fmap([replacement = std::forward<U>(replacement)](
                             const auto &) { return replacement; },
                         std::forward<T>(value));
    }
};

/** Typeclass lookup variable for Functor; specialize for each container type.
 */
template <class T>
inline constexpr auto functor_typeclass = std::false_type{};
```

Two pieces. `Functor<Impl>` is a CRTP base: the `Impl` supplies the primitive `fmap`, and the base derives what can be derived (`replace` &#x2014; map ignoring the element). `functor_typeclass<T>` is the registry: default `std::false_type`, meaning *no instance*; a type opts in by specializing the variable template to hold an instance object.

Registration for a base functor is a partial specialization keyed on the concrete **layer** type. Here is the whole of `NatF`'s membership, from [`src/smd/concrete/functors.hpp`](../../src/smd/concrete/functors.hpp):

```cpp
template <typename A>
struct NatFFunctorImpl {
    template <typename Fn>
    constexpr auto fmap(this auto &&, Fn &&fn,
                        const smd::concrete::NatF<A> &layer) {
        using B = std::remove_cvref_t<std::invoke_result_t<Fn, const A &>>;
        return std::visit(
            smd::fixpoint::overloaded{
                [](const smd::concrete::Zero &) -> smd::concrete::NatF<B> {
                    return smd::concrete::Zero{};
                },
                [&](const smd::concrete::Succ<A> &s) -> smd::concrete::NatF<B> {
                    return smd::concrete::Succ<B>{
                        smd::fixpoint::make_box<B>(std::invoke(fn, *s.pred))};
                },
            },
            layer);
    }
};

template <typename A>
struct NatFFunctorMap : Functor<NatFFunctorImpl<A>> {
    using NatFFunctorImpl<A>::fmap;
};

template <typename A>
inline constexpr auto functor_typeclass<smd::concrete::NatF<A>> =
    NatFFunctorMap<A>{};
```

The `Impl` visits the variant: `Zero` crosses unchanged, `Succ` applies the function through the `Box` and reboxes at the new element type. The `Map` layers the CRTP base over it, and one `inline constexpr` variable per element type `A` registers it for the whole family.

The keying choice matters and the design log defends it. Instances key on the *concrete layer instantiation* (`NatF<A>` for all `A`), not on the template-template parameter `NatF` itself. Deduced call sites always have a concrete layer in hand, and partial specialization over `<E, A>` lets one instance cover an element-parameterized family like `ListF<E, A>` &#x2014; something keying on the template name cannot express.


# layer\_fmap: One Bridge, Three Doors

Scheme bodies do not touch `functor_typeclass` directly. They call `layer_fmap(fn, layer)`, the single point where the fixpoint library bridges into typeclass dispatch. From [`src/smd/fixpoint/fmap.hpp`](../../src/smd/fixpoint/fmap.hpp):

```cpp
/** Concept satisfied by a functor typeclass *object* — anything exposing an
 * `fmap(fn, layer)` member. Disambiguates the explicit-object overload
 * (mode 3, instance first) from the implicit/pinned lookup form. */
template <class Typeclass, class Fn, class Layer>
concept functor_instance_for =
    requires(const Typeclass &tc, Fn &&fn, const Layer &layer) {
        tc.fmap(std::forward<Fn>(fn), layer);
    };

/** Apply the functor instance for @p layer's concrete type to @p fn.
 *
 * Modes 1 (implicit lookup) and 2 (NTTP pinning): the @p Typeclass reference
 * NTTP defaults to `functor_typeclass<remove_cvref_t<Layer>>` but may be
 * pinned at the call site as `layer_fmap<Layer, instance>(fn, layer)`.
 *
 * The NTTP is `const auto&` — a *reference* NTTP binding the `inline
 * constexpr` typeclass variable's static storage. A by-value `auto` NTTP
 * would instead require the instance to be a *structural* type, which the
 * `Functor`/`Monad` CRTP instances are not (their `protected` base is
 * non-structural); the reference form sidesteps that entirely. @p Layer
 * precedes the NTTP so @p Fn can trail as a deduced parameter and an inline
 * lambda never has to be spelled to reach the pin.
 */
template <class Layer,
          const auto &Typeclass =
              smd::typeclass::functor_typeclass<std::remove_cvref_t<Layer>>,
          class Fn>
constexpr auto layer_fmap(Fn &&fn, const Layer &layer) {
    return Typeclass.fmap(std::forward<Fn>(fn), layer);
}
```

That one template is two of the three lookup modes.

**Mode 1 &#x2014; implicit lookup.** `layer_fmap(fn, layer)`. Both arguments deduce; the `Typeclass` NTTP defaults to `functor_typeclass<remove_cvref_t<Layer>>`. This is what every scheme in the library writes, and it reads like a plain function call. A layer type with no registered instance hits the `false_type` default and fails loudly at the lookup, not mysteriously downstream.

**Mode 2 &#x2014; NTTP pinning.** `layer_fmap<Layer, my_functor>(fn, layer)`. The caller binds a specific instance as the non-type template parameter, overriding the registry for this call. Resolved once, at instantiation.

The mechanics of that NTTP repay attention, because getting them wrong makes the whole mode unusable. The parameter is `const auto&` &#x2014; a **reference** NTTP that binds the `inline constexpr` instance variable's static storage. A by-value `auto` NTTP would require the instance to be a *structural* type, and the CRTP instances are not: their `protected Impl` base disqualifies them. The reference form sidesteps structurality entirely. And the parameter order &#x2014; `Layer` first, NTTP second, callable last &#x2014; is deliberate: the callable stays a trailing *deduced* parameter, so an inline lambda never has to be spelled to reach the pin. Put the callable's type first and the mode dies: you cannot write the type of a lambda you are in the middle of writing.

**Mode 3 &#x2014; explicit object.** Pass the instance as an ordinary argument:

```cpp
/** Mode 3 (explicit object): apply the functor object @p tc directly instead
 * of consulting the global registry. The typeclass object is the first
 * argument — it threads ahead of the data it maps over, so a scheme can
 * carry an unregistered instance down through its recursion. Constrained on
 * `functor_instance_for` so it never competes with the two-argument
 * implicit/pinned form above.
 */
template <class Typeclass, class Fn, class Layer>
    requires functor_instance_for<Typeclass, Fn, Layer>
constexpr auto layer_fmap(const Typeclass &tc, Fn &&fn, const Layer &layer) {
    return tc.fmap(std::forward<Fn>(fn), layer);
}
```

The object need not be registered in `functor_typeclass` at all. This is the escape hatch &#x2014; above all for tests, which want the machinery without owning, or risking an ODR clash on, a global specialization. It is also the door through which a scheme can thread an instance by value down its own recursion. The `functor_instance_for` concept keeps this overload from ever competing with the two-argument form.


# Why an Object, Not a Trait

The three modes are the payoff of making the instance a **value** rather than a type-level fact. A trait can answer "does `NatF<int>` have an `fmap`, and what is it?" &#x2014; mode 1, and nothing else. An object can be defaulted from the registry, pinned as an NTTP, passed as an argument, built locally in a test, or composed with another instance. Registration (writing the instance) and declaration-for-the-program (specializing the variable) become separate, optional steps. The full theory is in the posts linked above; the fixpoint library is the consumer that keeps the design honest, because recursion schemes stress exactly the awkward corner &#x2014; an operation reached from deep inside recursive generic code, where nothing can be spelled explicitly.

One more consequence worth naming: the whole apparatus is ordinary C++. Variable templates, partial specialization, a concept, a reference NTTP. No macros, no code generation, and instances are about a dozen lines per functor.

The machinery is on the table. Time to fold something with it.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [Next: Part 3 - Fold, Unfold, Refold →](part-3-classical.md)

</nav>


# References

Downey, Steve (2023). **Some Informal Remarks Towards a New Theory of Trait Customization**, <https://sdowney.org/posts/index.php/2023/12/24/some-informal-remarks-towards-a-new-theory-of-trait-customization/>.

Downey, Steve (2024). **Concept Maps Using C++23 Library Tech**, <https://sdowney.org/posts/index.php/2024/05/19/concept-maps-using-c23-library-tech/>.
