<div class="abstract" id="orgb115f84">
<p>
Part 7 pulls two new types out of a hat &mdash; Cofree and Free &mdash; calls them
"the carriers," and moves on. They deserve better than a hat. Both are older
and more general than the schemes that use them, both stand on the exact Part
1 fixpoint trick, and both sit at the head of a small family &mdash; the
Church-encoded free monad, Kiselyov's Freer, and the dual the title asks
about. Fair warning: this is the interlude where I am writing at the edge of
what I understand. Where the category theory outruns me &mdash; adjunctions,
mostly &mdash; I quote the definitions and say so, rather than pretend; where the
code compiles, I vouch for it, and every excerpt here compiles: Haskell by
GHC 9.6.7, C++ by the library's own build. There is also one genuine surprise
on the trail: C++26 has already shipped something shaped exactly like a Freer
monad, and it is called sender/receiver.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Part 6 - Natural Transformations: hoist, prepro, postpro ←](part-6-prepro-postpro.md)

</nav>


# Where "Free" and "Cofree" Come From

The words are not decoration &mdash; but I owe you a warning label before I try to explain them. This is the section where I am writing to learn, not writing from mastery. The standard etymology of "free" and "cofree" runs through a piece of category theory called an *adjunction*, and I do not understand adjunctions yet &mdash; not well enough to explain one to you, which is the only standard that counts. So here is the deal for the next few paragraphs: I quote the definitions, unpack the individual words as far as I honestly can, and then show what the definitions *predict* &mdash; because the predictions are type equations, and type equations can be checked by a compiler. The predictions check out. That is the level of confidence on offer.

Start with the punch line every functional programmer has had recited at them, me included:

> A monad is just a monoid in the category of endofunctors.

The words in that, I can unpack. A monoid is a set of things, an associative way to combine two of them, and a unit that combines as a no-op: integers with `+` and `0`, strings with `++` and `""`. An endofunctor is what this series has called `F` all along: a type constructor with an `fmap`. The slogan says a monad is the monoid pattern one level up, where the things being combined are functor layers themselves: the combining operation is `join`, which flattens `M<M<X>>` into `M<X>`, and the unit is `return`, which wraps a plain `X` into `M<X>` without adding anything. The monoid laws &mdash; associativity, unit on either side &mdash; become the monad laws. Turn every arrow around and you get the dual, which &mdash; as I would happily tell you at a party, and which is even true &mdash; makes a comonad a *comonoid* in the same category: `duplicate` un-flattens, `extract` unwraps.

Now the part I hold at arm's length. The textbook story continues: there is a "forgetful" functor from monads to plain endofunctors &mdash; forgetful because it throws away `return` and `join` and keeps only the underlying `F` &mdash; and the free monad construction is its *left adjoint*, while dually the cofree comonad construction is the *right adjoint* of the matching forgetful functor for comonads (Kmett, Edward, 2009). "Free" means left-adjoint-to-forgetful; "cofree" means right-adjoint-to-forgetful. I can report that. I cannot yet explain what makes one functor the adjoint of another, and I will not fake it here.

What I can do is say what the definition is supposed to *buy*, because that part has checkable consequences. "Free" promises that `Free F` is a genuine monad built from `F` with nothing added beyond what the monad laws force &mdash; no extra equations, no collapsing, every value just a finite pile of constructors you could have written by hand. "Cofree" promises the mirror image: `Cofree F` is a genuine comonad over `F` that throws nothing away &mdash; it records, up front, every observation you could ever make of an `F`-shaped unfolding. That is why the shapes come out the way they do: a free thing is *generated* &mdash; finite, built up from constructors &mdash; and a cofree thing is *observed* &mdash; potentially infinite, taken apart by projections. Those shapes I do understand, because they are the type equations of the next section, and the type equations compile.


# Fix Is the Degenerate Case of Both

Here the ground firms up: everything from this point on is checked by a compiler, and I can vouch for all of it. Part 1 built `Fix<F> ≅ F<Fix<F>>`: pure recursion, nothing extra at the nodes. Free and Cofree are each `Fix` with *one extra thing* at every position, and the type equations say exactly which thing.

The free monad is the least fixed point of `X ↦ a + F X`:

    Free F a  ≅  Fix (λX. a + F X)

Read it as a tree of `F`-layers whose leaves are not dead ends but *variables* of type `a` &mdash; a term over the signature `F` with holes you can still fill. Kill the holes (set `a` to the empty type `Void`) and there is nothing left but the recursion: `Free F Void ≅ Fix F`.

The cofree comonad is the greatest fixed point of `X ↦ a × F X`:

    Cofree F a  ≅  Fix (λX. a × F X)

Every node still has its `F`-shape, but now it also carries an *annotation* of type `a`. Make the annotation trivial (set `a` to the unit type) and it vanishes: `Cofree F () ≅ Fix F`.

So `Fix` is not a third thing beside Free and Cofree. It is their shared degenerate core: Free with no variables, Cofree with no labels. These are honest, total isomorphisms, not hand-waving &mdash; here they are in Haskell, compiled, with `Void` and `()` doing the collapsing. From [`docs/blog/code/FixRel.hs`](code/FixRel.hs):

```haskell
-- The plain fixpoint -- the Haskell twin of C++ smd::fixpoint::Fix<F>.
newtype Fix f = Fix (f (Fix f))

-- Free f a is the least fixpoint of  X |-> a + f X : Fix with variables of
-- type a at the leaves. Kill the variables (a = Void) and Free collapses
-- to Fix. `absurd` witnesses that a Pure leaf can never occur.
freeToFix :: Functor f => Free f Void -> Fix f
freeToFix (Pure v)  = absurd v
freeToFix (Free fa) = Fix (fmap freeToFix fa)

fixToFree :: Functor f => Fix f -> Free f Void
fixToFree (Fix fa) = Free (fmap fixToFree fa)

-- Cofree f a is the greatest fixpoint of  X |-> a * f X : Fix with an
-- annotation of type a at every node. Make the annotation trivial (a = ())
-- and Cofree collapses to Fix.
cofreeToFix :: Functor f => Cofree f () -> Fix f
cofreeToFix (() :< fa) = Fix (fmap cofreeToFix fa)

fixToCofree :: Functor f => Fix f -> Cofree f ()
fixToCofree (Fix fa) = () :< fmap fixToCofree fa
```

And &mdash; the reason this interlude belongs to a C++ series &mdash; the same collapse happens in the library's *own* types. `Cofree<F, Unit>` round-trips through `Fix<F>`, and so does `Free<F, Empty>` (with the `Pure` leaf simply never constructed, `Empty` standing in for `Void`). Both directions fold at compile time; the `static_assert` in the example is the proof. From [`src/examples/free_cofree_interlude.cpp`](../../src/examples/free_cofree_interlude.cpp):

```cpp
// The two "trivial" annotation/leaf types. Unit is the trivial annotation
// (the () of Cofree f ()); Empty stands in for Haskell's uninhabited Void:
// the Pure alternative of Free<NatF, Empty> is simply never constructed.
// Haskell's Void makes "no Pure leaf" a theorem; here it is a discipline --
// the same iso-recursion honesty as Part 1's wrap_fix / unwrap_fix.
struct Unit {
    friend constexpr auto operator==(Unit, Unit) -> bool { return true; }
};
struct Empty {
    friend constexpr auto operator==(Empty, Empty) -> bool { return true; }
};

// Both hidden friends above exist so Unit/Empty are Regular (equality is
// part of the discipline, even for types this trivial); exercise them here
// so that stays true rather than aspirational.
static_assert(Unit{} == Unit{});
static_assert(Empty{} == Empty{});

// Fix<NatF>  <->  Cofree<NatF, Unit>   (Cofree f () ~= Fix f)
constexpr auto fix_to_cofree(const Nat &n) -> Cofree<NatF, Unit> {
    auto tail = layer_fmap(
        [](const Nat &child) { return fix_to_cofree(child); }, unwrap_fix(n));
    return Cofree<NatF, Unit>{Unit{}, std::move(tail)};
}

constexpr auto cofree_to_fix(const Cofree<NatF, Unit> &c) -> Nat {
    auto layer = layer_fmap(
        [](const Cofree<NatF, Unit> &child) { return cofree_to_fix(child); },
        unwrap_cofree(c));
    return wrap_fix<NatF>(std::move(layer));
}

// Fix<NatF>  <->  Free<NatF, Empty>    (Free f Void ~= Fix f)
constexpr auto fix_to_free(const Nat &n) -> Free<NatF, Empty> {
    auto layer = layer_fmap([](const Nat &child) { return fix_to_free(child); },
                            unwrap_fix(n));
    return roll_free<NatF>(std::move(layer));
}

constexpr auto free_to_fix(const Free<NatF, Empty> &f) -> Nat {
    // The Pure alternative is never used, so the Roll layer is always present.
    const auto &roll = std::get<1>(f.node);
    auto layer = layer_fmap(
        [](const Free<NatF, Empty> &child) { return free_to_fix(child); },
        roll);
    return wrap_fix<NatF>(std::move(layer));
}
```

That both conversions compile is Part 1's magic trick paying its second and third dividends. `Free<F, A>` and `Cofree<F, A>` contain `F<Free<F, A>>` and `F<Cofree<F, A>>` *by value*, mid-definition, for the identical reason `Fix<F>` could: boxing is the functor's responsibility, so `F<X>` is complete even when `X` is not. One convention about who owns the indirection, three self-referential types.


# The Naive Implementation, in Both Languages

The direct Haskell models are four lines each, and they are precisely what the library's headers transcribe. Here is the naive free monad &mdash; a `Pure` value or one `F`-layer of more Free &mdash; and the naive cofree comonad &mdash; an annotation at the head and one `F`-layer of annotated children at the tail. From [`docs/blog/code/Naive.hs`](code/Naive.hs):

```haskell
-- Free f a: a Pure value of type a, or one F-layer of further Free
-- computations. This is exactly smd::fixpoint::Free<F, A>'s
-- std::variant<A, F<Free<F,A>>>.
data Free f a
  = Pure a
  | Free (f (Free f a))

instance Functor f => Functor (Free f) where
  fmap g (Pure a)  = Pure (g a)
  fmap g (Free fa) = Free (fmap (fmap g) fa)

instance Functor f => Applicative (Free f) where
  pure = Pure
  Pure g  <*> x = fmap g x
  Free fg <*> x = Free (fmap (<*> x) fg)

instance Functor f => Monad (Free f) where
  Pure a  >>= k = k a
  Free fa >>= k = Free (fmap (>>= k) fa)
```

```haskell
-- Cofree f a: an F-shaped tree where every node carries an annotation of
-- type a at its head and one F-layer of annotated children as its tail.
-- This is exactly smd::fixpoint::Cofree<F, A>'s { A head; F<Cofree<F,A>> }.
data Cofree f a = a :< f (Cofree f a)
infixr 5 :<

instance Functor f => Functor (Cofree f) where
  fmap g (a :< fa) = g a :< fmap (fmap g) fa

-- A base-only Comonad class, so these modules need no external packages.
class Functor w => Comonad w where
  extract   :: w a -> a
  duplicate :: w a -> w (w a)
  duplicate = extend id
  extend    :: (w a -> b) -> w a -> w b
  extend f = fmap f . duplicate
  {-# MINIMAL extract, (duplicate | extend) #-}

instance Functor f => Comonad (Cofree f) where
  extract (a :< _)          = a
  duplicate w@(_ :< fa)     = w :< fmap duplicate fa
```

Set that beside the C++. `Free<F, A>` is a `std::variant<A, F<Free<F, A>>>` &mdash; the `Pure a | Free (f ...)` sum, spelled as a variant. From [`src/smd/fixpoint/free.hpp`](../../src/smd/fixpoint/free.hpp):

```cpp
/** Free<F, A>: a Pure value of type A, or one F-layer of further Free
 * computations (a Roll).
 * @tparam F unary template functor (the base functor being sequenced)
 * @tparam A the seed/value type at Pure leaves
 */
template <template <class> class F, class A>
struct Free {
    std::variant<A, F<Free<F, A>>> node; // Pure a | Roll layer

    // Hand-written (not = default): see cofree.hpp's/functors.hpp's own
    // comment — Free<F,A> is self-referential through F<Free<F,A>>, exactly
    // the shape that triggers Clang's eager defaulted-comparison
    // completeness check inside a self-embedding class template.
    friend constexpr auto operator==(const Free &lhs, const Free &rhs) -> bool {
        return lhs.node == rhs.node;
    }
};

/** pure_free(a) -> Free<F, A>: a Pure leaf holding @p a. */
template <template <class> class F, class A>
constexpr auto pure_free(A a) -> Free<F, A> {
    return Free<F, A>{std::variant<A, F<Free<F, A>>>{std::move(a)}};
}

/** roll_free(layer) -> Free<F, A>: one F-layer of further Free
 * computations.
 */
template <template <class> class F, class A>
constexpr auto roll_free(F<Free<F, A>> layer) -> Free<F, A> {
    return Free<F, A>{std::variant<A, F<Free<F, A>>>{std::move(layer)}};
}

/** is_pure(f) -> bool: true iff @p f is a Pure leaf (not a Roll layer). */
template <template <class> class F, class A>
constexpr auto is_pure(const Free<F, A> &f) -> bool {
    return std::holds_alternative<A>(f.node);
}
```

`Cofree<F, A>` is a head and a tail &mdash; the `a :< f (Cofree f a)` product, spelled as two members. From [`src/smd/fixpoint/cofree.hpp`](../../src/smd/fixpoint/cofree.hpp):

```cpp
/** Cofree<F, A>: an F-tree where every node is annotated with an A.
 * @tparam F unary template functor (the base functor being annotated)
 * @tparam A the annotation/history type at each node
 */
template <template <class> class F, class A>
struct Cofree {
    A head;               // the annotation at this node
    F<Cofree<F, A>> tail; // one functor layer of annotated children

    // Hand-written (not = default): this is the exact type the clang-22
    // "incomplete type" cascade (dist_laws.t.cpp/gprepro.t.cpp) traces
    // through — forming F<WWR> for WWR = Cofree<F,X> (generalized.hpp's
    // gcata_worker_t/gprepro_worker_t) requires std::variant to ask
    // is_trivially_destructible_v<Succ<WWR>>, which (per Clang, more
    // eagerly than GCC) forces this defaulted friend's deleted-ness check,
    // which requires Cofree<F,X> itself complete — a cycle. A plain friend
    // body sidesteps this: it is only instantiated when actually called.
    friend constexpr auto operator==(const Cofree &lhs, const Cofree &rhs)
        -> bool {
        return lhs.head == rhs.head && lhs.tail == rhs.tail;
    }
};

/** extract(c) -> const A& : the annotation at this node. */
template <template <class> class F, class A>
constexpr auto extract(const Cofree<F, A> &c) -> const A & {
    return c.head;
}

/** unwrap_cofree(c) -> const F<Cofree<F,A>>& : one layer of annotated
 * children.
 */
template <template <class> class F, class A>
constexpr auto unwrap_cofree(const Cofree<F, A> &c) -> const F<Cofree<F, A>> & {
    return c.tail;
}
```

Both are a near-mechanical transliteration &mdash; sum becomes `variant`, product becomes a struct, the recursive position gets boxed &mdash; and both instances (Monad for Free, Comonad for Cofree; see Part 2's `layer_fmap`) demand exactly one thing of `F`: a Functor instance. That requirement is the thread the rest of this interlude pulls on.


# Representing a Free by Its Own Fold: the Church Encoding

The naive Free is data. There is a rival representation that is *function*: encode a value by the one thing you will ever do to it &mdash; fold it. This is the Church, or Böhm&ndash;Berarducci, encoding (B{\\"o}hm, Corrado and Berarducci, Alessandro, 1985), and it is the exact dual of the library's design choice, so it is worth seeing.

A Church-encoded free value is a function that, given what to do with a `Pure` leaf (`a → r`) and an `F`-algebra (`f r → r`), returns the `r`. From [`docs/blog/code/Church.hs`](code/Church.hs):

```haskell
-- A Free value *is* the function that folds it: give it what to do with a
-- Pure leaf (a -> r) and an F-algebra (f r -> r) and it hands back the r.
-- Note there is no (Functor f) anywhere in this type or its Monad instance
-- -- the fmap that naive Free needs is pushed into the algebra `kf`.
newtype CFree f a = CFree { runCFree :: forall r. (a -> r) -> (f r -> r) -> r }

instance Functor (CFree f) where
  fmap g (CFree c) = CFree (\kp kf -> c (kp . g) kf)

instance Applicative (CFree f) where
  pure a = CFree (\kp _ -> kp a)
  CFree cf <*> CFree cx = CFree (\kp kf -> cf (\g -> cx (kp . g) kf) kf)

instance Monad (CFree f) where
  CFree c >>= k = CFree (\kp kf -> c (\a -> runCFree (k a) kp kf) kf)

-- Lifting a single layer is the one place a Functor is still needed -- but
-- only to build a value, never to sequence (>>=). That asymmetry is the
-- whole idea Freer pushes on (see Freer.hs).
liftF :: Functor f => f a -> CFree f a
liftF fa = CFree (\kp kf -> kf (fmap kp fa))
```

Two things stand out. First, there is no `Functor f` in the type or the `Monad` instance at all: the `fmap` that the naive Free needs inside `>>=` has been pushed out into the caller-supplied algebra `kf`. Second, this is Part 3's `cata` turned inside out &mdash; a value *is* its own catamorphism, a function sitting there waiting to be handed an algebra &mdash; which is why it is isomorphic to the naive form for any Functor, via a fold one way and a pair of constructors the other.

```haskell
-- The encoding is isomorphic to naive Free (for a Functor f): fromNaive
-- runs the fold that rebuilds the CPS form; toNaive instantiates the
-- answer type r at Free f a itself, with Pure and Free as the two folders.
fromNaive :: Functor f => Free f a -> CFree f a
fromNaive m = CFree (\kp kf ->
  let go (Pure a)  = kp a
      go (Free fa) = kf (fmap go fa)
  in go m)

toNaive :: CFree f a -> Free f a
toNaive (CFree c) = c Pure Free
```

The Church form is not just a curiosity; it is a performance fix. The naive `>>=` walks to the leaves of its left argument and grafts there, so a *left-nested* chain `((m >>= f) >>= g) >>= h` re-traverses the growing left spine at every step &mdash; classic quadratic blowup. The CPS shape re-associates that work for free. The definitive treatment, and the type-aligned-sequence remedy that the extensible-effects libraries actually ship, is van der Ploeg and Kiselyov's "Reflection without Remorse" (van der Ploeg, Atze and Kiselyov, Oleg, 2014).

So why does `smd::fixpoint` use the naive `Fix`-of-a-variant and not the Church encoding? Because this library's product is a *materialized, `constexpr` tree* &mdash; values that you pattern-match, compare with `==`, and fold at compile time. A `forall r` rank-2 newtype is precisely the thing you cannot pattern-match, cannot compare structurally, and cannot easily fold in a constant expression. The encoding that wins for asymptotic bind performance in a lazy language is the one that loses for a value-semantics compile-time library. The trade is real, and the library takes the other side of it deliberately.


# Freer: the Monad That Forgot It Needed a Functor

Both naive and Church Free still, at some point, want an `F`-algebra or an `fmap`. Kiselyov and Ishii's **Freer** monad &mdash; from "Freer Monads, More Extensible Effects" (Kiselyov, Oleg and Ishii, Hiromi, 2015) &mdash; removes even that. The idea is to replace the nested-functor layer with an explicit *continuation*. From [`docs/blog/code/Freer.hs`](code/Freer.hs):

```haskell
-- Freer f a: a Pure value, or a command `f x` paired with a continuation
-- (x -> Freer f a). The continuation -- not a nested functor -- is what
-- carries the rest of the computation, so *no `Functor f` is ever needed*.
data Freer f a where
  RPure   :: a -> Freer f a
  RImpure :: f x -> (x -> Freer f a) -> Freer f a

instance Functor (Freer f) where
  fmap g (RPure a)      = RPure (g a)
  fmap g (RImpure fx k) = RImpure fx (fmap g . k)

instance Applicative (Freer f) where
  pure = RPure
  RPure g      <*> x = fmap g x
  RImpure fx k <*> x = RImpure fx ((<*> x) . k)

instance Monad (Freer f) where
  RPure a      >>= k2 = k2 a
  RImpure fx k >>= k2 = RImpure fx ((>>= k2) . k)

-- lift a single command into the monad -- again, no Functor constraint.
liftFreer :: f a -> Freer f a
liftFreer c = RImpure c RPure
```

Look at the instance heads: `instance Functor (Freer f)`, no `Functor f =>` in sight. The `>>=` never maps over `f`; it only *composes continuations*. That is the whole trick, and it has a precise name: `Freer f` is exactly the free monad over the **Coyoneda** of `f`. Coyoneda sounds worse than it is. `Coyoneda f a` is just a pair: some `f x` you already have, and a function `x → a` you promise to apply later. Its `fmap` composes the new function onto the stored one and never touches the `f x` at all &mdash; which is how it makes a lawful Functor out of *any* `f`, asking nothing of it. These witnesses compile, so the isomorphism `Freer f ≅ Free (Coyoneda f)` is a fact, not a slogan:

```haskell
-- Coyoneda f -- the free functor on any f. It is a Functor for *every* f,
-- with no constraint, because fmap just accumulates in the (x -> a) slot.
data Coyoneda f a where
  Coyoneda :: (x -> a) -> f x -> Coyoneda f a

instance Functor (Coyoneda f) where
  fmap g (Coyoneda h fx) = Coyoneda (g . h) fx

-- Freer is exactly Free (Coyoneda f): the continuation in RImpure is the
-- (x -> a) of a Coyoneda, and Free supplies the recursion. These two
-- witnesses compile, so the isomorphism is real, not a slogan.
toFree :: Freer f a -> Free (Coyoneda f) a
toFree (RPure a)      = Pure a
toFree (RImpure fx k) = Free (Coyoneda (toFree . k) fx)

fromFree :: Free (Coyoneda f) a -> Freer f a
fromFree (Pure a)                = RPure a
fromFree (Free (Coyoneda h fx)) = RImpure fx (fromFree . h)
```

The payoff is that your signature need not be a functor &mdash; it can be a plain GADT of commands indexed by their result type, which has no `Functor` instance at all. Freer still hands you a monad, and a pure handler interprets it:

```haskell
-- An effect signature with NO Functor instance (its result type is an
-- index, not a covariant slot). Freer still gives a monad over it.
data Console a where
  PutLine :: String -> Console ()
  GetLine :: Console String

putLine :: String -> Freer Console ()
putLine s = liftFreer (PutLine s)

getLine_ :: Freer Console String
getLine_ = liftFreer GetLine

greet :: Freer Console ()
greet = do
  putLine "what is your name?"
  name <- getLine_
  putLine ("hello, " ++ name)

-- A pure handler: thread a list of inputs, accumulate outputs.
runConsole :: [String] -> Freer Console a -> ([String], a)
runConsole _        (RPure a)              = ([], a)
runConsole ins      (RImpure cmd k) = case cmd of
  PutLine s -> let (out, a) = runConsole ins (k ())
               in (s : out, a)
  GetLine   -> case ins of
    (i : rest) -> runConsole rest (k i)
    []         -> runConsole [] (k "")
```

There is a straight line from here back into the series. Part 9 (Mendler style) removes the Functor requirement from *folds* by the same spirit of move: instead of `fmap`-ing the recursive call over a layer, it *hands the algebra the recursive call itself*. Freer is to Free as Mendler-cata is to cata &mdash; "recursion without a Functor instance," reached from the monadic and the fold sides respectively. The library commits to the Functor instance everywhere (Part 2), which buys the uniform `layer_fmap` bridge; Freer and Mendler are the two escape hatches when you would rather not pay it.


# Side-Light: C++26 Already Shipped a Freer

Look at `RImpure` once more, because if you have been following WG21 you have seen this shape very recently.

    RImpure :: f x -> (x -> Freer f a) -> Freer f a

A Freer computation is a command paired with a continuation: run the command, feed whatever it produces to the rest of the program. Now set that beside the core of C++26's `std::execution`, the sender/receiver framework adopted from P2300 (Dominiak, Michał and Niebler, Eric and others, 2024):

    connect : (sender, receiver) -> operation-state

A *sender* describes work that will eventually complete with some values; by itself it does nothing. A *receiver* is the continuation, with a channel for each way the work can end: `set_value`, `set_error`, `set_stopped`. `connect` pairs the two into an operation state, and `start` runs it. Command plus continuation on one side; work-description plus what-happens-next on the other. It is the same decomposition.

The correspondence runs deeper than one constructor. `Console` had no Functor instance because its result type is an index &mdash; `GetLine` *is* a String-producing command, not a container of Strings &mdash; and a sender advertises its completion signatures the same way: a compile-time list of the types it will deliver, not a covariant slot you can `fmap` over. The adaptor `then(sndr, g)` never touches the work inside `sndr`; it wraps the receiver, so that `g` runs on the value on its way through to the original continuation. That is `Freer`'s `fmap`, and it is exactly the Coyoneda move &mdash; defer the map into the continuation &mdash; played out on operation states instead of closures. `let_value(sndr, g)`, where `g` gets the first result and returns a new sender to run, is `>>=`. And a sender means nothing until something interprets it &mdash; a scheduler, by way of `connect` and `start` &mdash; just as `greet` meant nothing until `runConsole` walked it. Description, continuation, interpreter: the whole Freer kit, renamed.

Two honest caveats. The signatures above are sketches, and the only uncompiled code in this interlude: real senders carry three completion channels rather than one, and compose at compile time through operation-state types, so the alignment is morphological rather than literal. And I do not claim P2300's authors derived the design from Kiselyov and Ishii; it descends from Facebook's futures and libunifex through a decade of engineering. But the convergence is not a coincidence either. Both start from the same constraint &mdash; represent work without running it, and never require the command vocabulary to be a functor &mdash; and continuation-passing is the shape that constraint forces. When you write `then(read(sock), parse)`, you are building an `RImpure` node, and the scheduler is your handler.


# Is There a CoFreer?

Yes &mdash; and by now you can predict its shape. Dualize `Freer f = Free (Coyoneda f)` arrow-for-arrow and you get `Cofreer f = Cofree (Coyoneda f)`: a cofree comonad that needs no Functor instance for its signature, because `Coyoneda f` supplies one for every `f`. The proof is that the comonad operations, specialized to this carrier, have *no* `Functor f` constraint and still typecheck. From [`docs/blog/code/Cofreer.hs`](code/Cofreer.hs):

```haskell
-- Cofreer f = Cofree (Coyoneda f). Because Coyoneda f is a Functor for
-- *every* f, Naive's `instance Functor f => Comonad (Cofree f)` fires with
-- no constraint on f. The proof is that these comonad operations have no
-- `Functor f` in their signatures and still typecheck:
type Cofreer f = Cofree (Coyoneda f)

cofreerExtract :: Cofreer f a -> a
cofreerExtract = extract

cofreerDuplicate :: Cofreer f a -> Cofreer f (Cofreer f a)
cofreerDuplicate = duplicate

-- Coyoneda's lift needs no Functor (you build the comonad for free);
-- its lower does (you only pay a Functor to interpret one layer out).
liftCoyoneda :: f a -> Coyoneda f a
liftCoyoneda = Coyoneda id

lowerCoyoneda :: Functor f => Coyoneda f a -> f a
lowerCoyoneda (Coyoneda h fx) = fmap h fx
```

There is one genuine subtlety, and it is the variance. The literature files `Coyoneda` and `Yoneda` under "Kan extensions" &mdash; a drawer of category theory I have not opened yet, so I will not lean on the name. What I can lean on is the quantifier, because it is right there in the two data declarations. `Coyoneda f a` is an *exists* &mdash; "there is some `x`, an `f x`, and a function out of `x`" &mdash; so it is free to *build* (`liftCoyoneda` needs no Functor) but you pay a Functor to interpret one layer back out. `Yoneda f a` is a *forall* &mdash; "hand me any function out of `a` and I will produce the mapped `f`" &mdash; so it is the reverse: free to *observe* out of, but you pay a Functor to get in. Both make any `f` a functor, so `Cofree (Yoneda f)` is *also* a lawful comonad for every `f`; which one is "the" CoFreer depends on whether you are constructing or consuming. Since a comonad is a thing you *observe*, `Yoneda` is arguably the more honest dual &mdash; and it costs nothing to write both down:

```haskell
-- The variance-dual choice. Yoneda is the other free-functor completion --
-- the forall-dual of Coyoneda's exists -- and Cofree (Yoneda f) is *also*
-- a Comonad for every f. The asymmetry that decides which to use:
--   liftCoyoneda  :: f a -> Coyoneda f a          -- free to *build*
--   lowerCoyoneda :: Functor f => Coyoneda f a -> f a
--   liftYoneda    :: Functor f => f a -> Yoneda f a
--   lowerYoneda   :: Yoneda f a -> f a            -- free to *observe*
-- Coyoneda pairs with Free/Freer (structures you construct); Yoneda pairs
-- with Cofree (structures you observe out of).
newtype Yoneda f a = Yoneda { runYoneda :: forall b. (a -> b) -> f b }

instance Functor (Yoneda f) where
  fmap g (Yoneda y) = Yoneda (\k -> y (k . g))

liftYoneda :: Functor f => f a -> Yoneda f a
liftYoneda fa = Yoneda (\k -> fmap k fa)

lowerYoneda :: Yoneda f a -> f a
lowerYoneda (Yoneda y) = y id

type CofreerY f = Cofree (Yoneda f)
```

So the construction is real, it is lawful, and it compiles. What it is *not* is famous. Freer earned its name because dropping the Functor constraint unlocked composable, efficient extensible *effects* &mdash; a problem thousands of people had. The comonadic dual would unlock composable *co-effects*: context-dependent, streaming, incremental computations. That is a real and interesting corner, but it never acquired a celebrated library or a catchy name, so "CoFreer" remains mostly folklore and a good answer to an interview question. The honest summary: yes, there is a CoFreer; no, you have probably never needed one; and writing it down is a two-line `type` synonym once you have Coyoneda.


# The Ledger

The family, one row per member, and the question that separates them &mdash; does the construction demand a Functor instance for `F`?

| construction         | what it is                      | needs `Functor f`?    |
|-------------------- |------------------------------- |--------------------- |
| `Fix f`              | bare recursion, no extras       | only for its schemes  |
| `Free f a`           | `Fix` with variables at leaves  | yes                   |
| `Cofree f a`         | `Fix` with annotations at nodes | yes                   |
| `CFree f a` (Church) | a Free *as its own fold*        | no (deferred to `kf`) |
| `Freer f a`          | `Free (Coyoneda f)`             | no                    |
| `Cofreer f a`        | `Cofree (Coyoneda f)`           | no                    |

Read down the middle column and the theme of the whole series reappears: it is all `Fix` with something bolted to the nodes. Read down the right column and you see the two ways to stop paying the Functor tax &mdash; push it into the fold (Church) or defer it into a stored continuation (Freer/Cofreer). The `smd::fixpoint` library sits firmly in the top three rows, and on purpose: materialized values that compare and fold at compile time are worth a Functor instance per layer. And if the `Freer` row rings familiar from the day job, that is the side-light: C++26's sender/receiver sits in that row too.

With the carriers demystified, Part 7 can hand you Cofree and Free and you will know precisely what is in your hand: `Fix`, annotated on the left and seeded on the right.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [Next: Part 7 - Histomorphisms and Futumorphisms →](part-7-histo-futu.md)

</nav>


# References

Kmett, Edward (2009). **Recursion Schemes: A Field Guide (Redux)**, The Comonad.Reader, <http://comonad.com/reader/2009/recursion-schemes/>.

Böhm, Corrado and Berarducci, Alessandro (1985). **Automatic Synthesis of Typed Λ-Programs on Term Algebras**, Theoretical Computer Science 39.

van der Ploeg, Atze and Kiselyov, Oleg (2014). **Reflection without Remorse: Revealing a Hidden Sequence to Speed up Monadic Reflection**, Haskell Symposium 2014.

Kiselyov, Oleg and Ishii, Hiromi (2015). **Freer Monads, More Extensible Effects**, Haskell Symposium 2015.

Dominiak, Michał, Evtushenko, Georgy, Baker, Lewis, Teodorescu, Lucian Radu, Howes, Lee, Shoop, Kirk, Garland, Michael, Niebler, Eric, and Adelstein Lelbach, Bryce (2024). **std::execution**, WG21 paper P2300R10, <https://wg21.link/P2300>.
