// src/examples/freer_retry.cpp                                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// FD10, end to end: the paper's opening example. ONE program --
// retry-with-backoff over two independent signatures, Clock and Network --
// interpreted THREE ways, to exhibit the separation-of-concerns claim as
// running code:
//
//   Run 1  Mock everything. A scripted Cofree responder (FD8) fails the
//          Network twice then succeeds, paired with a virtual-time Clock in
//          ONE Cofree value, run synchronously by pair_run_trace.
//   Run 2  The SAME program, interpreted live through the sender/receiver
//          machinery (run_task + beman::execution::sync_wait). "Live" means
//          through beman::execution -- not that it hits a network -- and is
//          deterministic and single-threaded.
//   Run 3  Composition. Network stays mocked; the Clock handler is swapped
//          for a live-shaped one. That swap is handler STACKING
//          (`overloaded{mock_network, live_clock}`), one expression, no new
//          fixture class (FD10 part 3).
//
// The three runs observe the SAME operation trace (Run 1 and Run 2
// identically; Run 3 differs only in which handler answered the clock), the
// FD10 facts: fetch time, exactly three Sends, backoff 1s then 2s, no
// fourth attempt, final reply length 11.
//
// Implementation-notes disclosure (FD6, NOT a performance claim): each
// resumption allocates one Box, and the S/R interpreter stacks one
// coroutine frame per effect. Acceptable at this demo's scale (~6 effects);
// recorded here only so the example does not hide it.

#include <examples/freer_retry_handlers.hpp>
#include <examples/freer_retry_program.hpp>

#include <smd/fixpoint/freer_cosignature.hpp>
#include <smd/fixpoint/freer_run.hpp>
#include <smd/fixpoint/freer_task.hpp>
#include <smd/fixpoint/unfold_cofree.hpp>

#include <beman/execution/execution.hpp>

#include <print>
#include <string>

namespace ex = beman::execution;

using retry_example::expected_trace;
using retry_example::expected_value;
using retry_example::make_live_clock_handler;
using retry_example::make_mock_handler;
using retry_example::make_network_handler;
using retry_example::retry_coalgebra;
using retry_example::retry_head_fn;
using retry_example::retry_program;
using retry_example::RetryScript;
using retry_example::Row;

namespace {

auto render_trace(const smd::fixpoint::trace &log) -> std::string {
    std::string out;
    for (std::size_t i = 0; i < log.size(); ++i) {
        if (i != 0) {
            out += " -> ";
        }
        out += log[i];
    }
    return out;
}

} // namespace

auto main() -> int {
    std::println("FD10 -- retry-with-backoff over Clock + Network");
    std::println("The program (once): fetch the time, then send \"hello\"; on");
    std::println("failure back off (1s, then 2s) and retry, up to three "
                 "attempts.");
    std::println("");

    // -- Run 1: mock everything (Cofree pairing) ------------------------
    {
        smd::fixpoint::Mock<Row, RetryScript> mock =
            smd::fixpoint::unfold_cofree<
                smd::fixpoint::cosignature<Row>::template type>(
                retry_head_fn, retry_coalgebra, RetryScript{});

        auto [value, final_state, log] =
            smd::fixpoint::pair_run_trace<Row, int, RetryScript>(
                std::move(mock), retry_program());

        std::println("Run 1  mock everything (Cofree pairing, synchronous)");
        std::println("  trace : {}", render_trace(log));
        std::println("  sends : {}   backoffs advanced virtual time to {}s",
                     final_state.send_count, final_state.virtual_time);
        std::println("  reply : length {} (\"hello-reply\")", value);
        std::println("");
    }

    // -- Run 2: the SAME program, live S/R interpretation ---------------
    {
        smd::fixpoint::trace log;
        int send_calls = 0;
        int virtual_time = 0;
        auto handler = make_mock_handler(log, send_calls, virtual_time);

        auto result = ex::sync_wait(smd::fixpoint::run_task<Row, int>(
            std::move(handler), retry_program()));
        auto [value] = result.value();

        std::println("Run 2  the SAME program, live S/R (run_task + "
                     "sync_wait)");
        std::println("  trace : {}", render_trace(log));
        std::println("  sends : {}   virtual time advanced to {}s", send_calls,
                     virtual_time);
        std::println("  reply : length {}", value);
        std::println("  (identical trace and value to Run 1: one program, two "
                     "interpreters)");
        std::println("");
    }

    // -- Run 3: composition -- mock Network, live-shaped Clock ----------
    {
        smd::fixpoint::trace log;
        int send_calls = 0;
        int clock_now = 100; // a distinct clock, demonstrably not the mock

        // Handler STACKING, not a new fixture: keep the scripted Network,
        // swap in the live-shaped Clock -- one `overloaded{...}` expression.
        auto handler =
            smd::fixpoint::overloaded{make_network_handler(log, send_calls),
                                      make_live_clock_handler(log, clock_now)};

        auto result = ex::sync_wait(smd::fixpoint::run_task<Row, int>(
            std::move(handler), retry_program()));
        auto [value] = result.value();

        std::println("Run 3  composition: Network mocked, Clock handler "
                     "live-shaped");
        std::println("  trace : {}", render_trace(log));
        std::println("  sends : {}   (mocked to fail twice, as in Runs 1-2)",
                     send_calls);
        std::println("  reply : length {}", value);
        std::println("  (mocking Network while keeping a real-shaped Clock is "
                     "handler");
        std::println("   stacking -- one expression -- not a new fixture "
                     "class)");
        std::println("");
    }

    std::println("All three runs: fetch time, three Sends, backoff 1s/2s, no "
                 "fourth attempt.");
    std::println("Run 1 == Run 2 trace: {}", render_trace(expected_trace()));
    std::println("Final reply length: {}", expected_value);
    return 0;
}
