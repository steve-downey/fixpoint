<div class="abstract" id="org9a7cd2b">
<p>
The interlude promised that sender/receiver is shaped like a Freer monad. The
follow-through built it: a closed-world signature, a Coyoneda node over a
one-shot continuation, rows, the Cofree pairing for the mock, and the same
retry-with-backoff program interpreted two ways &#x2014; once against a scripted
Cofree, once through <code>beman::task</code> and a real scheduler. It compiles, both
interpreters agree on the trace, and the whole thing is Asan-clean. It also
left three loose ends, all deferred on purpose. Here they are, so I don't have
to pretend later that I didn't see them.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [← Interlude - Free, Cofree, and Their Free-er Relatives](part-6.5-free-cofree.md)

</nav>

All three are the same shape: a known-better structure, left out because the demo does not need it, and because the version that ships is the one that shows the bones. None of them blocks anything. That is exactly why it is worth writing them down &#x2014; deferred work with no forcing function is the work that quietly never happens.


# The closure chain

`bind` over a Freer program does not walk the tree any more. That was the whole point of the Coyoneda node: mapping the recursive position is post-composition onto the stored continuation, O(1), no traversal. The O(n²) re-walk that made the naïve free monad a punchline is gone.

What is left is linear, and it hides in the left nest. Write your binds left-leaning &#x2014; `((m >>= f) >>= g) >>= h` &#x2014; and each one wraps the last in another closure, so reaching the next `Pure` leaf means unwinding a tower as tall as the number of binds. Not quadratic. Just a stack of frames proportional to how badly you associated.

van der Ploeg and Kiselyov named this and killed it (van der Ploeg, Atze and Kiselyov, Oleg, 2014): hold the continuations in a type-aligned deque instead of a left-leaning stack of closures, and appends are amortized O(1) at the end you actually use. The title is the joke and the fix &#x2014; reflection *without* remorse.

So why is there no deque in the tree? Because the retry program binds a handful of times and the residue never surfaces, and because a type-aligned sequence is a real piece of machinery, not an afternoon. It goes in when someone writes a program that leans on it. Until then it would be untested weight.


# The one-shot that could have been `std::move_only_function`

The stored continuation is a bespoke `one_shot`, roughly thirty lines of type-erased heap box that consumes itself when you call it. Not out of craftsmanship. `std::move_only_function` is the standard type for exactly this, and I would rather have used it.

I couldn't, uniformly. Under `clang++-23 -stdlib=libc++` the name is not merely untagged by its feature-test macro &#x2014; it is not declared at all. `<version>` carries the `#define __cpp_lib_move_only_function` line, commented out, and the type is absent from `namespace std`. A header that names it unconditionally does not mis-detect a feature; it fails to compile on a row the CI matrix claims. So the bespoke box is the uniform answer, and it earns a little of its keep: it carries the one-shot invariant directly, where a second call is a hard error rather than a UB-adjacent call on a moved-from target.

Where the macro *is* defined, `std::move_only_function` brings a small-buffer optimization the hand-rolled box does not, and could sit behind the same interface as a fast path. But it would have to be selected by the macro, and the bespoke fallback has to exist regardless for libc++ and appleclang. A second, macro-gated path buys small-buffer locality at the price of a forked test matrix for a win nobody has measured wanting. Not yet.


# Frame per effect

`run_task` &#x2014; the sender/receiver interpreter &#x2014; is not a hand-written loop. It is `mcata_free` at `Result = beman::task<A>`: the Mendler fold over `Free`, with the coroutine task as its carrier. That equation *is* the claim the interpreter exists to make &#x2014; interpretation is a fold, and the async carrier is just a choice of answer type &#x2014; so I wrote it as the equation and let the compiler turn the crank.

The bill for that honesty is one coroutine frame per effect. The impure-layer algebra is itself a `task`, so an N-effect program suspends N of them, each awaiting the next. The synchronous `run` does not pay this &#x2014; it is a `while`-loop, O(1) live frames no matter how long the program runs. A resume-into-a-loop rewrite of `run_task`, shaped like `run`, would collapse the stack the same way.

I shipped the frame-per-effect version anyway, because it is the fold, visibly, and the loop rewrite trades that transparency for frames I don't need at six effects. When a program shows up that does need them, the rewrite is sitting right there next to `run`, which already proves the shape.

\*

Three structures, all real, all deferred, all disclosed as costs and none of them spun as a feature. A deque, a macro, and a loop. The trail continues.
