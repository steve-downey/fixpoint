// src/smd/fixpoint/freer_run.hpp                                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_FREER_RUN
#define INCLUDED_SMD_FIXPOINT_FREER_RUN

// The synchronous, handler-driven interpreter for Freer<Sig, A> (design doc
// FD5, FD6, FD7), plus the trace-collecting reference handler that is FD5's
// observational-testing vocabulary: Freer programs are not comparable
// (FD5), so tests interpret a program against a handler and assert on the
// resulting value and the sequence of operations visited, never on the
// program's structure.

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/freer.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <concepts>
#include <functional>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace smd::fixpoint {

/** run(handler, prog) -> A: interpret a Freer<Sig, A> program to completion
 * against `handler`.
 *
 * `handler` is any callable set covering every operation in `Sig` -- an
 * `overloaded` of one lambda per Op is the intended shape (FD1: handlers
 * pattern-match on requests, so `std::visit` over the node variant is the
 * dispatch). A missing case is a compile error at this call: the
 * static_assert below fires naming the uncovered `impure_node<Op, X>` in
 * the instantiation backtrace, rather than a generic "no matching
 * `std::visit` alternative" failure.
 *
 * A LOOP, not recursion (FD6, pitfall list): each iteration handles
 * exactly one suspended operation and rebinds `current` to the resumed
 * result, so the C++ call stack never grows with the number of effects a
 * program performs -- a hand-recursive `run` would stack one frame per
 * effect and Asan would not tolerate a program of any real depth.
 * `X = Freer<Sig, A>` is fixed for the whole loop: `Free` is
 * self-referential through `F<Free<F,A>>`, so every layer's recursive
 * position already is `Freer<Sig, A>` itself, and resuming a continuation
 * always hands back exactly that type -- no re-templating between
 * iterations.
 */
template <class Sig, class A, class Handler>
auto run(Handler &&handler, Freer<Sig, A> &&prog) -> A {
    using X = Freer<Sig, A>;
    using Layer = typename Sig::template type<X>;

    Freer<Sig, A> current = std::move(prog);
    while (!is_pure(current)) {
        Layer layer = std::get<Layer>(std::move(current.node));
        Box<X> resumed = std::visit(
            overloaded{
                [&handler]<operation Op>(impure_node<Op, X> &&node) -> Box<X> {
                    static_assert(
                        std::invocable<Handler &, Op>,
                        "run: handler does not cover every operation in "
                        "Sig -- see the impure_node<Op, X> named above in "
                        "the instantiation backtrace for which Op");
                    typename Op::response response =
                        std::invoke(handler, std::move(node.op));
                    return std::move(node.k)(std::move(response));
                }},
            std::move(layer.node));
        current = *std::move(resumed);
    }
    return std::get<A>(std::move(current.node));
}

/** traceable_operation: an `operation` (freer.hpp) that additionally
 * renders itself via the ordinary ADL-found `operator<<` idiom -- the same
 * mechanism any other streamable type uses, not a bespoke customization
 * point. Deliberately a separate concept, not an extra requirement folded
 * into `operation` itself: `operation` is S03's landed contract and
 * already has passing tests (`Get`/`Put` in freer.t.cpp) that do not
 * define `operator<<`; broadening it here would break them.
 */
template <class Op>
concept traceable_operation =
    operation<Op> && requires(const Op &op, std::ostream &out) {
        { out << op } -> std::same_as<std::ostream &>;
    };

/** trace: FD5's assertion vocabulary. One rendered string per visited
 * operation, in visitation order. Tests assert "exactly these ops, in this
 * order" as a plain `std::vector<std::string>` comparison -- terse, and
 * reused verbatim by every later interpreter (S05-S08).
 */
using trace = std::vector<std::string>;

/** render_operation(op) -> one trace entry: `op`'s own `operator<<`,
 * captured as a `std::string` *before* the operation is moved into the
 * handler -- never a pointer or reference surviving past the node it came
 * from (the pitfall list's warning).
 */
template <traceable_operation Op>
auto render_operation(const Op &op) -> std::string {
    std::ostringstream out;
    out << op;
    return std::move(out).str();
}

/** tracing_handler<Handler> / tracing(handler, log): wrap `handler` (an
 * `overloaded` per-op callable set, or any callable set covering `Sig`) so
 * every operation passed through it is first rendered into `log` (FD5),
 * then delegated to `handler` for the response. `log` is a caller-owned
 * `trace&`; the adaptor itself is a thin pass-through, cheap to build
 * fresh per `run_trace` call. This is FD5's "first handler, first mock":
 * the reference interpreter for observational testing.
 */
template <class Handler>
struct tracing_handler {
    Handler handler;
    trace *log;

    template <traceable_operation Op>
    constexpr auto operator()(Op &&op) ->
        typename std::remove_cvref_t<Op>::response {
        log->push_back(render_operation(op));
        return std::invoke(handler, std::forward<Op>(op));
    }
};

/** tracing(handler, log) -> tracing_handler<Handler>: see above. */
template <class Handler>
constexpr auto tracing(Handler &&handler, trace &log)
    -> tracing_handler<std::remove_cvref_t<Handler>> {
    return tracing_handler<std::remove_cvref_t<Handler>>{
        std::forward<Handler>(handler), &log};
}

/** run_trace(handler, prog) -> {value, trace}: `run` through `tracing`'s
 * reference handler (FD5) -- the vocabulary every later interpreter test
 * (S05-S08) reuses to assert "exactly these ops, in this order" without
 * inspecting the program's structure.
 */
template <class Sig, class A, class Handler>
auto run_trace(Handler &&handler, Freer<Sig, A> &&prog) -> std::pair<A, trace> {
    trace log;
    A result = run<Sig, A>(tracing(std::forward<Handler>(handler), log),
                           std::move(prog));
    return {std::move(result), std::move(log)};
}

} // namespace smd::fixpoint

#endif
