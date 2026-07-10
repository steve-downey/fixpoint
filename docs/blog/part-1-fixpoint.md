<div class="abstract" id="org3ac7beb">
<p>
A recursive type that names itself is ordinary. A type that is the fixed
point of a <b>non-recursive</b> template is the foundation every recursion scheme
stands on. The type equation <code>Fix&lt;F&gt; ≅ F&lt;Fix&lt;F&gt;&gt;</code> looks like it cannot
possibly compile &mdash; the trick that makes it legal C++ is deciding, once and
for all, whose job the indirection is.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Part 0 - Why a Field Guide? ←](part-0-intro.md)

</nav>


# The Shape of One Layer

I want an arithmetic expression tree: constants, addition, multiplication. The usual C++ move is a class hierarchy, or a `std::variant` that names itself through a `unique_ptr`. Both weld the recursion into the type.

Instead, I describe *one layer* of the tree, with a type parameter `A` standing wherever a child would go. From [`src/smd/concrete/functors.hpp`](../../src/smd/concrete/functors.hpp):

```cpp
template <typename A>
struct Const {
    int val;
};

template <typename A>
struct Add {
    smd::fixpoint::Box<A> left, right;
};

template <typename A>
struct Mul {
    smd::fixpoint::Box<A> left, right;
};

template <typename A>
using ExprF = std::variant<Const<A>, Add<A>, Mul<A>>;

using Expr = smd::fixpoint::Fix<ExprF>;
```

`ExprF` never mentions `ExprF`. When `A` is `int`, an `ExprF<int>` is one layer whose children are already integers. When `A` is the full tree type, the children are subtrees. This parameterization &mdash; the *base functor* &mdash; is the whole idea (Milewski, Bartosz, 2013) (Milewski, Bartosz, 2017). Everything else in this series is machinery for choosing what to put in that `A` slot.


# Fix: The Knot

The tree type is the fixed point of the layer template. From [`src/smd/fixpoint/fix.hpp`](../../src/smd/fixpoint/fix.hpp):

```cpp
template <template <typename> class F>
struct Fix {
    F<Fix<F>> inner;
};
```

`Fix` takes a template-template parameter &mdash; the unary base functor &mdash; and plugs the resulting type back into itself: a `Fix<F>` holds one layer `F<Fix<F>>`, *by value*. The isomorphism boundary is two trivial functions:

```cpp
/** Wrap one layer of @p F into the fixed-point type. */
template <template <typename> class F>
constexpr auto wrap_fix(F<Fix<F>> layer) -> Fix<F> {
    return Fix<F>{std::move(layer)};
}

/** Unwrap one layer from a fixed-point value, exposing F<Fix<F>>. */
template <template <typename> class F>
constexpr auto unwrap_fix(const Fix<F> &fixed) -> const F<Fix<F>> & {
    return fixed.inner;
}
```

`wrap_fix` packs a layer; `unwrap_fix` exposes it. Neither does work. They exist so the type system can watch the recursion cross the boundary, one layer at a time &mdash; this is iso-recursion, not equi-recursion, and the explicitness is a feature. Every scheme in this series is a dance of `unwrap_fix`, `fmap`, and `wrap_fix`.


# The Magic Trick

Stare at that member declaration.

```cpp
template <template <typename> class F>
struct Fix {
    F<Fix<F>> inner;
};
```

At the point `inner` is declared, `Fix<F>` is still an incomplete type &mdash; the compiler is in the middle of defining it. Instantiating `F` at an incomplete type, and holding the result **by value**, should be a hard error for any `F` that stores its argument directly. `Succ<A>` with an `A pred;` member would demand the size of `Fix<NatF>` while computing the size of `Fix<NatF>`. Infinite regress; the compiler gives up at the template instantiation depth limit.

The trick is a division of responsibility, stated once in the design and obeyed everywhere:

> Boxing is the functor's responsibility. Recursive positions inside base functor alternatives hold `Box<A>`. Consequently `F<X>` is a complete type for incomplete `X`.

Look back at `ExprF`: the children are `Box<A> left, right;` &mdash; never a bare `A`. A `Box<A>` contains only an `A*`, and C++ has always been happy to point at an incomplete type. So `ExprF<Fix<ExprF>>` is a complete type &mdash; its size is known, its members are pointers &mdash; even while `Fix<ExprF>` itself is still being defined. The knot ties.

That is the entire magic. Not a compiler extension, not a clever metaprogram &mdash; a convention about who owns the indirection. The functor boxes; `Fix`, `Cofree` (the Interlude), and `Free` (the Interlude) all get to contain their own layers by value because of it.


# Box: The Indirection That Survives constexpr

`Box` could almost be `std::unique_ptr`, except trees are values here &mdash; they copy, they compare, and they fold at compile time. From [`src/smd/fixpoint/box.hpp`](../../src/smd/fixpoint/box.hpp):

```cpp
template <typename A>
struct Box {
    A *ptr = nullptr;

    constexpr Box() = default;

    constexpr explicit Box(A *p) : ptr(p) {}

    constexpr Box(Box const &other)
        : ptr(other.ptr ? new A(*other.ptr) : nullptr) {}

    constexpr auto operator=(Box const &other) -> Box & {
        if (this != &other) {
            delete ptr;
            ptr = other.ptr ? new A(*other.ptr) : nullptr;
        }
        return *this;
    }

    constexpr Box(Box &&other) noexcept
        : ptr(std::exchange(other.ptr, nullptr)) {}

    constexpr auto operator=(Box &&other) noexcept -> Box & {
        if (this != &other) {
            delete ptr;
            ptr = std::exchange(other.ptr, nullptr);
        }
        return *this;
    }

    constexpr ~Box() { delete ptr; }

    constexpr auto operator*() const -> A & { return *ptr; }
    constexpr auto operator->() const -> A * { return ptr; }

    friend constexpr auto operator==(Box const &lhs, Box const &rhs) -> bool {
        if (lhs.ptr == rhs.ptr)
            return true;
        if (!lhs.ptr || !rhs.ptr)
            return false;
        return *lhs.ptr == *rhs.ptr;
    }
};
```

Three properties earn `Box` its place.

**Deep-copy value semantics.** Copying a `Box` copies the pointee. Copying a tree copies the tree. No sharing, no reference counting, no surprises about who mutated what. The library accepts the copies deliberately &mdash; performance tuning is an explicit non-goal of the design, correctness of the transcription is the goal.

**constexpr capability.** The allocation uses raw `new~/~delete`, which are usable in constant evaluation for transient allocations since C++20. This is why every scheme in the library can carry a `static_assert` that folds a small tree at compile time. The fixpoint machinery is a compile-time library that also works at run time, not the other way around.

**Nullability by default.** A default-constructed `Box` is null. That looks like a concession, and it is &mdash; the C++26 alternative `std::indirect` (Bhosale, Jagrut and Catmur, Ed and Berne, Jonathan, 2024) has the right semantics but an explicit default constructor, which blocks aggregate-initialized storage. The header's comment records the trade honestly: `Box` is a workaround the library keeps for consistency and its nullable default.

The maker function completes the vocabulary:

```cpp
/** Construct a Box<A>, forwarding @p args to A's constructor. */
template <typename A, typename... Args>
constexpr auto make_box(Args &&...args) -> Box<A> {
    return Box<A>(new A(std::forward<Args>(args)...));
}
```


# Composition, Not Inheritance

Eric Niebler's classic translation of this pattern (Niebler, Eric, 2013) ties the knot with inheritance: `struct Fix : F<Fix<F>> {}`. That satisfies the same equation, and unwrapping becomes an upcast. But is-a is stronger than the math requires. Inheritance implies substitutability &mdash; any function taking `F<Fix<F>>` would accept a `Fix<F>` &mdash; and that uninvited conversion can ambush overload resolution. The single-member composition here says exactly what is true: a `Fix<F>` *contains* one layer. Nothing more is promised, so nothing more can go wrong.


# Smart Constructors, and a Tree at Last

Building nodes through `wrap_fix` and `make_box` by hand is ceremony. The functor header ships constructors that hide it:

```cpp
/** Build a constant leaf holding @p v. */
constexpr auto const_node(int v) -> Expr {
    return smd::fixpoint::wrap_fix<ExprF>(ExprF<Expr>{Const<Expr>{v}});
}

/** Build @p l + @p r. */
constexpr auto add_node(Expr l, Expr r) -> Expr {
    return smd::fixpoint::wrap_fix<ExprF>(
        ExprF<Expr>{Add<Expr>{smd::fixpoint::make_box<Expr>(std::move(l)),
                              smd::fixpoint::make_box<Expr>(std::move(r))}});
}

/** Build @p l * @p r. */
constexpr auto mul_node(Expr l, Expr r) -> Expr {
    return smd::fixpoint::wrap_fix<ExprF>(
        ExprF<Expr>{Mul<Expr>{smd::fixpoint::make_box<Expr>(std::move(l)),
                              smd::fixpoint::make_box<Expr>(std::move(r))}});
}

/** Fold: evaluate an Expr tree (cata). */
constexpr auto eval(const Expr &tree) -> int {
    return smd::fixpoint::fold_fix<int>(
        [](const ExprF<int> &node) -> int {
            return std::visit(
                smd::fixpoint::overloaded{
                    [](const Const<int> &c) { return c.val; },
                    [](const Add<int> &a) { return *a.left + *a.right; },
                    [](const Mul<int> &m) { return *m.left * *m.right; },
                },
                node);
        },
        tree);
}
```

The `eval` at the end is a preview: it consumes the tree with `fold_fix`, the catamorphism, and its per-node logic never mentions recursion. Part 3 takes it apart. The runnable example [`src/examples/fixpoint_tree_example.cpp`](../../src/examples/fixpoint_tree_example.cpp) is the whole program:

```cpp
int main() {
    // Build the tree: (2 * 3) + 4
    Expr tree = add_node(mul_node(const_node(2), const_node(3)), const_node(4));

    std::println("Result: {}", eval(tree));
}
```

No tree-walking code anywhere in it. That absence is the product this library sells.


# The Rest of the Bestiary

The same header defines the other base functors the series uses: `NatF` (Peano naturals &mdash; `Zero` or `Succ`), `ListF<E, ·>` (cons lists carrying payload type `E`), and `TreeF<E, ·>` (leaf-valued binary trees).

```cpp
struct Zero {
    // Hand-written (not = default): Clang 22 eagerly instantiates a
    // defaulted comparison operator's deleted-ness check inside a class
    // template body, which forces completeness of any self-referential
    // functor family wrapping this type (e.g. Cofree<NatF, Cofree<NatF,A>>,
    // "squared" comonad-duplicate results, S13/S15) before that family has
    // finished being defined — GCC defers this check, so it never hits the
    // cycle. A plain friend body is only instantiated when actually
    // ODR-used (both compilers agree on this), which breaks the cycle.
    friend constexpr auto operator==(const Zero &, const Zero &) -> bool {
        return true;
    }
};

template <typename A>
struct Succ {
    smd::fixpoint::Box<A> pred;

    // Hand-written for the same reason as Zero's above. Member-wise
    // comparison of Box<A>: Box's own operator== already null-checks
    // (box.hpp), so comparing the Box objects directly (not dereferencing)
    // is exactly what `= default` would have produced.
    friend constexpr auto operator==(const Succ &lhs, const Succ &rhs) -> bool {
        return lhs.pred == rhs.pred;
    }
};

template <typename A>
using NatF = std::variant<Zero, Succ<A>>;

using Nat = smd::fixpoint::Fix<NatF>;
```

Two things to notice. `ListF` is a *binary* template &mdash; element type and recursion slot &mdash; and `Fix` wants a unary one; an alias template (`template <class A> using IntListF = ListF<int, A>`) binds the payload, and alias templates are valid template-template arguments per P0522 (Spertus, Mike and Vandevoorde, Daveed, 2016). And those hand-written `operator==` bodies where `= default` ought to be: the comment records a real Clang 22 discovery &mdash; a defaulted comparison's deleted-ness check forces completeness of self-referential functor families mid-definition; a plain friend body is only instantiated when used. Shipping code accumulates scars. The field guide keeps them visible.

Next: the `fmap` that every scheme leans on, and the typeclass machinery that finds it.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [Next: Part 2 - Typeclass Objects and layer\_fmap →](part-2-typeclasses.md)

</nav>


# References

Milewski, Bartosz (2013). **Understanding F-Algebras**, <https://bartoszmilewski.com/2013/06/10/understanding-f-algebras/>.

Milewski, Bartosz (2017). **F-Algebras**, Category Theory for Programmers, chapter 24, <https://bartoszmilewski.com/2017/02/28/f-algebras/>.

Niebler, Eric (2013). **F-Algebras and C++**, <http://ericniebler.com/2013/07/16/f-algebras-and-c/>.

Bhosale, Jagrut, Catmur, Ed, and Berne, Jonathan (2024). **Indirect and Polymorphic: Vocabulary Types for Composite Class Design**, WG21 P3019, <https://wg21.link/P3019>.

Spertus, Mike and Vandevoorde, Daveed (2016). **Matching of template template-arguments excludes compatible templates**, WG21 P0522, <https://wg21.link/P0522>.
