// src/smd/fixpoint/freer_retry.t.cpp                                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// S08 (ops/freer/steps/08-integration.md), item 4: FD8's "tests read
// identically across interpreters" claim, demonstrated in ONE file. The
// canonical FD10 retry program (consolidated in
// examples/freer_retry_program.hpp) is run through BOTH interpreters --
// the Cofree pairing mock (pair_run_trace, S06) and the live sender/
// receiver interpreter (run_task, S07) -- and each result is asserted with
// the SAME helper against the SAME trace and value. FD5: the assertions are
// on {value, trace}, never on the structure of a Freer/Cofree value.
//
// This is the one test in the module beyond freer_task's that links beman
// (it needs run_task); it also drives the beman-free pairing interpreter,
// so it is the single place both interpreters meet on one program.

#include <examples/freer_retry_handlers.hpp>
#include <examples/freer_retry_program.hpp>

#include <smd/fixpoint/freer_cosignature.hpp>
#include <smd/fixpoint/freer_run.hpp>
#include <smd/fixpoint/freer_task.hpp>
#include <smd/fixpoint/unfold_cofree.hpp>

#include <beman/execution/execution.hpp>
#include <beman/task/task.hpp>

#include <catch2/catch_test_macros.hpp>

#include <utility>

namespace ex = beman::execution;

using retry_example::expected_trace;
using retry_example::expected_value;
using retry_example::make_mock_handler;
using retry_example::retry_coalgebra;
using retry_example::retry_head_fn;
using retry_example::retry_program;
using retry_example::RetryScript;
using retry_example::Row;

namespace {

/** The SAME assertion helper both interpreters are checked with (S08 item
 * 4): the fetched-time-then-three-Sends trace, and the reply length. FD5:
 * value + trace only.
 */
void expect_retry(int value, const smd::fixpoint::trace &log) {
    CHECK(value == expected_value); // "hello-reply".size() == 11
    CHECK(log == expected_trace()); // Now(), Send, SleepFor(1), Send,
                                    // SleepFor(2), Send -- three Sends,
                                    // backoff 1s/2s, no fourth attempt
}

} // namespace

TEST_CASE("freer_retry [FD8/FD10]: the Cofree pairing mock and the live S/R "
          "interpreter run the SAME program to the SAME value and trace") {
    // -- Interpreter A: Cofree pairing (S06), synchronous ---------------
    smd::fixpoint::Mock<Row, RetryScript> mock = smd::fixpoint::unfold_cofree<
        smd::fixpoint::cosignature<Row>::template type>(
        retry_head_fn, retry_coalgebra, RetryScript{});

    auto [pair_value, pair_state, pair_log] =
        smd::fixpoint::pair_run_trace<Row, int, RetryScript>(std::move(mock),
                                                             retry_program());

    expect_retry(pair_value, pair_log);
    CHECK(pair_state.send_count == 3);
    CHECK(pair_state.virtual_time == 3); // 1s + 2s of virtual backoff

    // -- Interpreter B: live sender/receiver (S07) ----------------------
    smd::fixpoint::trace task_log;
    int send_calls = 0;
    int virtual_time = 0;
    auto handler = make_mock_handler(task_log, send_calls, virtual_time);

    auto result = ex::sync_wait(
        smd::fixpoint::run_task<Row, int>(std::move(handler), retry_program()));
    REQUIRE(result.has_value());
    auto [task_value] = result.value();

    expect_retry(task_value, task_log); // SAME helper, SAME expectations
    CHECK(send_calls == 3);
    CHECK(virtual_time == 3);
}
