// src/examples/freer_retry_handlers.hpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_EXAMPLES_FREER_RETRY_HANDLERS
#define INCLUDED_EXAMPLES_FREER_RETRY_HANDLERS

// The sender/receiver handlers for FD10's retry example (S08). Companion to
// `freer_retry_program.hpp`; kept SEPARATE from it because these DO depend
// on beman::execution (every case returns a sender), so only the
// beman-linked consumers (freer_retry.t.cpp, the freer_retry example, and
// -- for the live-S/R run -- freer_task.t.cpp) include this. The program
// header stays beman-free so the pure-pairing test can include it.
//
// Each handler records into a caller-owned `trace& log` (FD5 vocabulary,
// render_operation) and answers with `just(response)` -- an already-ready
// sender, so run_task interprets the whole program synchronously under
// sync_wait, single-threaded, no scheduler hop. Mutable script state
// (send_calls, virtual_time) is held BY REFERENCE so every copy run_task
// makes of the handler observes the same state (S07 discovery: mcata_free
// copies the algebra, hence the handler, per recursion).

#include <examples/freer_retry_program.hpp>

#include <smd/fixpoint/freer_run.hpp> // trace / render_operation
#include <smd/fixpoint/overloaded.hpp>

#include <beman/execution/execution.hpp>

#include <expected>

namespace retry_example {

/** The scripted Network mock: fails the first two Sends (a value-level
 * `std::unexpected`, not a sender error-completion), succeeds on the third.
 * Records each Send into `log`; counts attempts in `send_calls`.
 */
inline auto make_network_handler(smd::fixpoint::trace &log, int &send_calls) {
    return [&log, &send_calls](Send op) {
        log.push_back(smd::fixpoint::render_operation(op));
        ++send_calls;
        Send::response r =
            (send_calls < 3)
                ? Send::response{std::unexpected(net_error{"boom"})}
                : Send::response{reply{"hello-reply"}};
        return beman::execution::just(r);
    };
}

/** The virtual-time Clock mock (Run 2): Now reports the current virtual
 * time; SleepFor advances it. No wall clock, no threads -- deterministic.
 */
inline auto make_virtual_clock_handler(smd::fixpoint::trace &log,
                                       int &virtual_time) {
    return smd::fixpoint::overloaded{
        [&log, &virtual_time](Now op) {
            log.push_back(smd::fixpoint::render_operation(op));
            return beman::execution::just(time_point{virtual_time});
        },
        [&log, &virtual_time](SleepFor op) {
            log.push_back(smd::fixpoint::render_operation(op));
            virtual_time += op.d; // advance virtual time deterministically
            return beman::execution::just(smd::fixpoint::unit{});
        }};
}

/** Fully-mocked handler (Run 2, and the cross-interpreter test): scripted
 * Network stacked over the virtual Clock -- `overloaded` composition, one
 * expression.
 */
inline auto make_mock_handler(smd::fixpoint::trace &log, int &send_calls,
                              int &virtual_time) {
    return smd::fixpoint::overloaded{
        make_network_handler(log, send_calls),
        make_virtual_clock_handler(log, virtual_time)};
}

/** A "live-shaped" Clock handler (Run 3): the same handler SHAPE a
 * deployed Clock adapter has -- Now answers a real (here still
 * deterministic, monotonically advancing) time, SleepFor is where a real
 * adapter would schedule a timer. It advances `now` on every read so it is
 * demonstrably NOT the virtual-time mock; the point of Run 3 is that
 * swapping it in is handler STACKING (`overloaded{mock_network,
 * live_clock}`), not a new fixture class (FD10 part 3).
 */
inline auto make_live_clock_handler(smd::fixpoint::trace &log, int &now) {
    return smd::fixpoint::overloaded{
        [&log, &now](Now op) {
            log.push_back(smd::fixpoint::render_operation(op));
            return beman::execution::just(time_point{now++});
        },
        [&log, &now](SleepFor op) {
            log.push_back(smd::fixpoint::render_operation(op));
            // A real adapter schedules a timer here; the demo stays
            // deterministic and does not touch the wall clock.
            now += op.d;
            return beman::execution::just(smd::fixpoint::unit{});
        }};
}

} // namespace retry_example

#endif
