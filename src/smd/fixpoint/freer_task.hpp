// src/smd/fixpoint/freer_task.hpp                                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_FREER_TASK
#define INCLUDED_SMD_FIXPOINT_FREER_TASK

// The sender/receiver interpreter for Freer<Sig, A> (design doc Section 5
// forward reference; FD4, FD5). Where freer_run.hpp's `run` drives a
// program to completion synchronously, `run_task` interprets it into a
// beman::execution coroutine -- `beman::task<A>` is the Mendler algebra's
// monomorphic carrier, the C++ image of the interpreter's quantified answer
// type. Each operation is mapped, by a handler, to a sender whose value
// completion is that operation's response; run_task co_awaits the sender,
// resumes the suspended one-shot continuation with the response, and
// recurses on the rest of the program.
//
// This is the ONE header in the module that depends on beman::execution /
// beman::task; the core fold it drives (mcata_free.hpp) has no beman in it,
// and its test links the beman targets in a dedicated executable
// (freer_task.t.cpp), never smd_fixpoint_test or fixpoint.fixpoint (S07d).
//
// SHAPE THAT LANDED: run_task IS mcata_free at Result = beman::task<A> --
// the design's Section 5 claim as running code, not a hand-written loop.
// The impure-layer algebra (run_task_step) is a coroutine, so interpreting
// an N-effect program stacks N task frames, each suspended awaiting the
// next (frame-per-effect). That is FD6's disclosed demo-scale cost (Box per
// resumption + one coroutine frame per effect), acceptable for the retry
// example; a single-coroutine resume-into-a-loop rewrite (mirroring `run`)
// is the available FD6 optimization if a later step needs O(1) live frames.
//
// COROUTINE CAPTURE-OWNERSHIP (FD4, in coroutine clothing). The one-shot
// continuation is move-only and consumed on resumption (FD4); a coroutine
// that resumes it must OWN it across the suspension, never borrow it from an
// enclosing native stack frame that may already be gone. Two rules make
// that hold, and both are load-bearing (violating either is an Asan
// use-after-free, not a compile error):
//   1. Every coroutine here takes the state it touches after a suspension
//      point BY VALUE (handler, recurse, the layer, the node) -- a
//      by-value coroutine parameter is copied/moved into the frame before
//      the body runs; a reference parameter is NOT, and dangles past the
//      first suspend (beman::task's initial_suspend is suspend_always, so
//      the body does not even begin until after the producing full-
//      expression has ended and its temporaries are gone).
//   2. The Mendler algebra handed to mcata_free (task_phi) is a NON-
//      coroutine thin forwarder. A coroutine *lambda* would store the
//      handler in its closure, and that closure is a temporary destroyed
//      at the end of the full-expression that created the task -- while the
//      task stays suspended referencing it. task_phi forwards synchronously
//      into run_task_step, which owns handler by value; the closure's job
//      is done before the first suspension.

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/freer.hpp>
#include <smd/fixpoint/mcata_free.hpp>
#include <smd/fixpoint/one_shot.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <beman/execution/execution.hpp>
#include <beman/task/task.hpp>

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>
#include <variant>

namespace smd::fixpoint {

/** task_handler_for<Handler, Op>: `Handler` maps operation `Op` to a
 * beman::execution sender. The handler-as-adaptor of the S/R interpreter:
 * `handler(op)` is a sender whose *value completion carries `Op::response`*
 * -- the contract the async model replaces `run`'s synchronous
 * `handler(op) -> Op::response` with. The value-type half of the contract
 * (the completion is exactly `Op::response`) is enforced structurally at
 * the resumption site in `resume_one` -- `typename Op::response response =
 * co_await handler(op);` -- so this concept keys only on "is a sender",
 * keeping it free of beman's completion-signature/env detail machinery
 * while still rejecting a plainly non-sender handler with a readable
 * message at the `run_task` call.
 */
template <class Handler, class Op>
concept task_handler_for =
    operation<Op> && std::invocable<Handler &, Op> &&
    beman::execution::sender<std::invoke_result_t<Handler &, Op>>;

namespace detail {

/** resume_one: the resumption path for one suspended operation. co_await the
 * handler's sender for the response, then invoke the move-only one-shot
 * continuation on that response -- as an rvalue, exactly once (one_shot's
 * `operator()(...) &&`), on this (the coroutine's) resumption path -- and
 * unbox its Box<X> result.
 *
 * `node` is taken BY VALUE so the frame owns it -- and therefore owns
 * `node.k` (the move-only one_shot) -- across the co_await suspension (FD4).
 * `handler` is a reference to the caller frame (run_task_step), which is
 * itself suspended awaiting this task, so it outlives every suspension here.
 * Templated per Op so std::visit's alternatives all yield the SAME task<X>.
 */
template <class X, class Handler, operation Op>
auto resume_one(Handler &handler, impure_node<Op, X> node)
    -> beman::execution::task<X> {
    static_assert(task_handler_for<Handler, Op>,
                  "run_task: handler does not map every operation in Sig to "
                  "a sender -- see the impure_node<Op, X> named in the "
                  "instantiation backtrace for which Op");
    typename Op::response response = co_await std::invoke(handler, node.op);
    co_return *std::move(node.k)(std::move(response));
}

/** run_task_step: the Mendler algebra's impure-layer (Roll) case, as a
 * coroutine. Visits the single suspended node, resumes it (resume_one), then
 * recurses on the unboxed rest via `recurse` (mcata_free partially applied).
 *
 * `handler` and `recurse` are taken BY VALUE -- owned by this frame across
 * every suspension (FD4 capture-ownership rule in coroutine clothing:
 * nothing this coroutine touches after a suspend is borrowed from an
 * enclosing native frame). `layer` is likewise by value; the visited node is
 * moved out of it into resume_one.
 */
template <class Sig, class A, class Handler, class Recurse>
auto run_task_step(Handler handler, Recurse recurse,
                   typename Sig::template type<Freer<Sig, A>> layer)
    -> beman::execution::task<A> {
    using X = Freer<Sig, A>;
    X next = co_await std::visit(
        [&handler]<operation Op>(
            impure_node<Op, X> &&node) -> beman::execution::task<X> {
            return detail::resume_one<X, Handler, Op>(handler, std::move(node));
        },
        std::move(layer.node));
    co_return co_await std::move(recurse)(std::move(next));
}

/** task_phi: the Mendler algebra handed to mcata_free for the Roll case. A
 * NON-coroutine thin forwarder (see this header's top comment, rule 2): its
 * closure holds `handler` only long enough to copy it, synchronously, into
 * run_task_step's frame. Making phi a coroutine lambda would dangle the
 * captured handler past the first suspension.
 */
template <class Sig, class A, class Handler>
struct task_phi {
    Handler handler;

    template <class Recurse>
    auto operator()(Recurse recurse,
                    typename Sig::template type<Freer<Sig, A>> layer) const
        -> beman::execution::task<A> {
        return detail::run_task_step<Sig, A, Handler, Recurse>(
            handler, std::move(recurse), std::move(layer));
    }
};

} // namespace detail

/** run_task<Sig, A>(handler, prog) -> beman::task<A>: interpret a
 * Freer<Sig, A> program into a beman::execution coroutine against `handler`
 * (a callable set covering every operation in `Sig` -- an `overloaded` of
 * one lambda per Op, each returning a sender-of-response, is the intended
 * shape; a row is covered by a handler set spanning all its member
 * signatures, exactly like `run`). This IS mcata_free at Result =
 * beman::task<A>: Pure a -> co_return a; a Roll layer -> co_await the op's
 * sender, resume the one-shot continuation with the response, recurse on the
 * unboxed rest.
 *
 * `handler` and `prog` are taken BY VALUE: the whole interpretation is a
 * coroutine that must own everything it touches after a suspension point
 * (FD4). The handler must therefore be copyable, and any mutable state it
 * scripts (a call counter, virtual-clock cursor) must be shared by
 * reference/pointer capture so every copy observes it -- the ordinary way to
 * write a scripted mock, and what `run`'s handlers already do.
 *
 * Sig and A must be given explicitly (the S04 deduction limitation:
 * Freer<Sig,A> is an alias `Free<Sig::type, A>`, which deduction cannot
 * invert). Handler deduces from the argument.
 */
template <class Sig, class A, class Handler>
auto run_task(Handler handler, Freer<Sig, A> prog)
    -> beman::execution::task<A> {
    return mcata_free<beman::execution::task<A>, Sig::template type, A>(
        [](A a) -> beman::execution::task<A> { co_return std::move(a); },
        detail::task_phi<Sig, A, std::remove_cvref_t<Handler>>{
            std::move(handler)},
        std::move(prog));
}

} // namespace smd::fixpoint

#endif
