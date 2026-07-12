// src/examples/freer_retry_program.hpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_EXAMPLES_FREER_RETRY_PROGRAM
#define INCLUDED_EXAMPLES_FREER_RETRY_PROGRAM

// FD10's motivating example, consolidated (S08). This is EXAMPLE / test-
// support vocabulary, deliberately NOT a library header (it is not in the
// `smd_fixpoint_headers` FILE_SET): the retry-with-backoff program, its two
// domain signatures (Clock + Network), the domain data types, and the
// Cofree-pairing mock (RetryScript coalgebra) that Run 1 / S06 drive.
//
// Kept beman-free on purpose: it is included by the beman-free
// `smd_fixpoint_test` (freer_cosignature.t.cpp, the pairing interpreter) as
// well as by the beman-linked S/R consumers. The sender/receiver handlers
// (which DO need beman) live in the companion `freer_retry_handlers.hpp`.
//
// Everything here lives in the NAMED namespace `retry_example` (not an
// anonymous one) so all translation units that include it share ONE set of
// types and ONE program definition -- that is the whole point of S08 item 1
// ("one canonical spelling"). Free functions are `inline` for the same
// reason (ODR across the several TUs that include this).
//
// Canonical shape decision (S08): the recursive `retry_attempt` spelling
// (S07's, cleaner than S06's fully-unrolled triple-nested mbind), and the
// program FETCHES the time first (`send<Row>(Now{})`) before retrying --
// FD10's narrative is "fetch time, retry with backoff", and keeping the
// Now{} fetch means the program exercises BOTH Clock operations (Now and
// SleepFor), not just SleepFor, so "Clock fully mocked" is a real claim
// rather than half the signature going untouched. Both interpreters (Cofree
// pairing and live S/R) therefore assert the SAME six-op trace:
//   Now(), Send(hello), SleepFor(1), Send(hello), SleepFor(2), Send(hello)

#include <smd/fixpoint/freer.hpp>
#include <smd/fixpoint/freer_cosignature.hpp>
#include <smd/fixpoint/freer_row.hpp>
#include <smd/fixpoint/freer_run.hpp> // trace / render_operation vocabulary

#include <smd/typeclass/detail/typeclass_base.hpp>
#include <smd/typeclass/monad.hpp>

#include <expected>
#include <ostream>
#include <string>
#include <utility>

namespace retry_example {

// -- Domain data (the vocabulary a caller's own types would be) ---------

struct time_point {
    int t = 0;
};

using duration = int;

struct request {
    std::string body;
};

struct reply {
    std::string body;
};

struct net_error {
    std::string message;
};

// -- Clock signature ----------------------------------------------------

struct Now {
    using response = time_point;
};

inline auto operator<<(std::ostream &out, const Now &) -> std::ostream & {
    return out << "Now()";
}

struct SleepFor {
    duration d;
    using response = smd::fixpoint::unit;
};

inline auto operator<<(std::ostream &out, const SleepFor &op)
    -> std::ostream & {
    return out << "SleepFor(" << op.d << ")";
}

using Clock = smd::fixpoint::signature<Now, SleepFor>;

// -- Network signature --------------------------------------------------

struct Send {
    request req;
    using response = std::expected<reply, net_error>;
};

inline auto operator<<(std::ostream &out, const Send &op) -> std::ostream & {
    return out << "Send(" << op.req.body << ")";
}

using Network = smd::fixpoint::signature<Send>;

// -- The composed row and the program's carrier -------------------------

using Row = smd::fixpoint::row<Clock, Network>;
using RowFree = smd::fixpoint::Freer<Row, int>;

/** One attempt: send the request; on success, the value is the reply's
 * length; on failure, back off `attempt` seconds (SleepFor) and retry, up
 * to three attempts total (1s after attempt 1, 2s after attempt 2). The
 * value on total failure is -1; there is no fourth attempt. Composed with
 * `mbind` + `pure` only (FD4): every continuation captures copyable state,
 * so DEV-S05-1's move-only-continuation wall never bites here.
 */
inline auto retry_attempt(int attempt) -> RowFree {
    return smd::typeclass::mbind(
        smd::fixpoint::send<Row>(Send{request{"hello"}}),
        [attempt](std::expected<reply, net_error> res) -> RowFree {
            if (res) {
                return smd::fixpoint::pure_free<Row::template type>(
                    static_cast<int>(res->body.size()));
            }
            if (attempt >= 3) {
                return smd::fixpoint::pure_free<Row::template type>(
                    -1); // no fourth attempt
            }
            return smd::typeclass::mbind(
                smd::fixpoint::send<Row>(SleepFor{attempt}),
                [attempt](smd::fixpoint::unit) -> RowFree {
                    return retry_attempt(attempt + 1);
                });
        });
}

/** FD10: fetch the current time, then retry-with-backoff. The fetched time
 * is a real (if here unused) start timestamp -- its point is that the Clock
 * signature is exercised in full, and that both Clock operations flow
 * through whichever handler is stacked underneath.
 */
inline auto retry_program() -> RowFree {
    return smd::typeclass::mbind(
        smd::fixpoint::send<Row>(Now{}),
        [](time_point) -> RowFree { return retry_attempt(1); });
}

// -- The Cofree-pairing mock (Run 1 / S06): a scripted Network failing ---
// -- twice then succeeding, paired with a virtual-time Clock, in ONE -----
// -- Cofree value (FD8). ------------------------------------------------

/** Script state: how many Sends have been answered (fail the first two,
 * succeed on the third -- FD10's "failing twice then succeeding") and the
 * virtual clock's current time (advanced only by SleepFor -- virtual time,
 * no sleep_for, no threads).
 */
struct RetryScript {
    int send_count = 0;
    int virtual_time = 0;
};

inline auto retry_coalgebra(const RetryScript &state)
    -> smd::fixpoint::cosignature<Row>::type<RetryScript> {
    using smd::fixpoint::responder;
    return smd::fixpoint::cosignature<Row>::type<RetryScript>{
        responder<Now, RetryScript>{
            [state](Now) -> std::pair<Now::response, RetryScript> {
                return {time_point{state.virtual_time}, state};
            }},
        responder<SleepFor, RetryScript>{
            [state](SleepFor op) -> std::pair<SleepFor::response, RetryScript> {
                RetryScript next = state;
                next.virtual_time += op.d;
                return {smd::fixpoint::unit{}, next};
            }},
        responder<Send, RetryScript>{
            [state](Send) -> std::pair<Send::response, RetryScript> {
                RetryScript next = state;
                ++next.send_count;
                if (next.send_count <= 2) {
                    return {std::unexpected(net_error{"boom"}), next};
                }
                return {reply{"hello-reply"}, next}; // length 11
            }}};
}

inline auto retry_head_fn(const RetryScript &state) -> RetryScript {
    return state;
}

/** The canonical trace both interpreters must observe: fetch time, three
 * Sends, backoff 1s then 2s, no fourth attempt.
 */
inline auto expected_trace() -> smd::fixpoint::trace {
    return smd::fixpoint::trace{"Now()",       "Send(hello)", "SleepFor(1)",
                                "Send(hello)", "SleepFor(2)", "Send(hello)"};
}

/** The successful reply's length -- the program's final value. */
inline constexpr int expected_value = 11; // "hello-reply".size()

} // namespace retry_example

#endif
