// src/smd/fixpoint/freer_task.t.cpp                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// S07 (ops/freer/steps/07-sr-interpreter.md): run_task, the sender/receiver
// interpreter -- mcata_free at Result = beman::task<A>. Exercised against:
//   * the S04 KV program, through op->sender handlers -- value MATCHES the
//     synchronous `run` (FD5 observational equality across interpreters);
//   * FD10's retry-with-backoff over row<Clock, Network>, a scripted
//     Network sender failing twice then succeeding on virtual time --
//     exactly three Sends, backoff 1s/2s, no fourth attempt (FD10 facts,
//     asserted through the S04 trace vocabulary the handlers record into);
//   * a deliberately-async op whose sender defers via a scheduler hop,
//     proving suspension crosses a real scheduling point (deterministic:
//     the hop lands on sync_wait's own run_loop, which sync_wait drives --
//     single-threaded, no sleeps, no threads).
//
// The one test in the module that links beman::execution / beman::task
// (own executable, per S07d) -- run under the default Asan config on both
// pinned toolchains, so every coroutine-lifetime rule in freer_task.hpp is
// policed by the sanitizer.

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/freer.hpp>
#include <smd/fixpoint/freer_row.hpp>
#include <smd/fixpoint/freer_run.hpp> // trace / render_operation vocabulary
#include <smd/fixpoint/freer_task.hpp>
#include <smd/fixpoint/freer_task.hpp> // Re-inclusion check
#include <smd/fixpoint/one_shot.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <examples/freer_retry_program.hpp>

#include <smd/typeclass/detail/typeclass_base.hpp>
#include <smd/typeclass/monad.hpp>

#include <beman/execution/execution.hpp>
#include <beman/task/task.hpp>

#include <catch2/catch_test_macros.hpp>

#include <expected>
#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <utility>

namespace ex = beman::execution;

using smd::fixpoint::Freer;
using smd::fixpoint::overloaded;
using smd::fixpoint::pure_free;
using smd::fixpoint::render_operation;
using smd::fixpoint::row;
using smd::fixpoint::run_task;
using smd::fixpoint::send;
using smd::fixpoint::signature;
using smd::fixpoint::trace;
using smd::fixpoint::unit;
using smd::typeclass::mbind;

namespace {

// -- KV signature (S04's, verbatim) ------------------------------------

struct Get {
    int key;
    using response = std::optional<int>;
};

struct Put {
    int key;
    int value;
    using response = unit;
};

using KV = signature<Get, Put>;
using KVFree = Freer<KV, int>;

/** The KV handler as an op->sender adaptor: the S04 store-backed handler,
 * but each case returns `just(response)` instead of the bare response. The
 * store is shared by reference so every copy run_task makes of the handler
 * (mcata_free copies it per recursion) observes the same map.
 */
auto kv_sender_handler(std::map<int, int> &store) {
    return overloaded{[&store](Get op) {
                          Get::response r =
                              store.contains(op.key)
                                  ? Get::response{store.at(op.key)}
                                  : std::nullopt;
                          return ex::just(r);
                      },
                      [&store](Put op) {
                          store[op.key] = op.value;
                          return ex::just(unit{});
                      }};
}

/** FD7's usage sketch, byte-for-byte the S04 program: get k >>= \v -> put k
 * (f v) >>= \_ -> pure v. Composed with mbind + pure only (FD4).
 */
auto get_bump_put_program(int key, int (*f)(int)) -> KVFree {
    return mbind(send<KV>(Get{key}), [key, f](std::optional<int> v) -> KVFree {
        int value = v.value_or(0);
        return mbind(send<KV>(Put{key, f(value)}), [value](unit) -> KVFree {
            return pure_free<KV::template type>(value);
        });
    });
}

// -- FD10 Clock + Network, for the retry example -----------------------
//
// The retry program, its Clock/Network vocabulary, and the domain data
// types now live in ONE canonical place, examples/freer_retry_program.hpp
// (S08 consolidation); this test drives that canonical program through the
// live S/R interpreter. The Cofree pairing interpreter drives the SAME
// program in freer_cosignature.t.cpp, and both meet on one program in
// freer_retry.t.cpp.

using retry_example::net_error;
using retry_example::Now;
using retry_example::reply;
using retry_example::retry_program;
using retry_example::Row;
using retry_example::Send;
using retry_example::SleepFor;
using retry_example::time_point;

// -- The deliberately-async signature ----------------------------------

struct AsyncGet {
    int key;
    using response = int;
};

using Async = signature<AsyncGet>;
using AsyncFree = Freer<Async, int>;

} // namespace

// =======================================================================
// FD5 observational equality ACROSS interpreters: run_task interpreting the
// S04 KV program against just()-level senders yields the SAME value the
// synchronous `run` does (freer_run.t.cpp asserts value == 10 for this
// exact program/store), asserted with sync_wait's optional-unwrap shape
// (S07d handoff).
// =======================================================================

TEST_CASE("run_task [FD5]: KV get-bump-put matches the synchronous run's "
          "value") {
    std::map<int, int> store{{7, 10}};
    auto handler = kv_sender_handler(store);

    auto result = ex::sync_wait(run_task<KV, int>(
        handler, get_bump_put_program(7, [](int v) { return v + 1; })));

    REQUIRE(result.has_value());
    auto [value] = result.value();
    CHECK(value == 10); // identical to freer_run.t.cpp's synchronous run
    CHECK(store.at(7) == 11);
}

TEST_CASE("run_task [FD5]: KV Get on an absent key matches the synchronous "
          "run's value") {
    std::map<int, int> store; // key 3 absent
    auto handler = kv_sender_handler(store);

    auto result = ex::sync_wait(run_task<KV, int>(
        handler, get_bump_put_program(3, [](int v) { return v + 5; })));

    REQUIRE(result.has_value());
    auto [value] = result.value();
    CHECK(value == 0); // Get -> nullopt -> value_or(0), same as `run`
    CHECK(store.at(3) == 5);
}

// =======================================================================
// FD10, through the S/R interpreter: a scripted Network sender fails twice
// then succeeds; the real-shaped Clock mock advances virtual time. Exactly
// three Sends, backoff 1s/2s, no fourth attempt, correct final value. The
// handlers record into an S04 `trace` so the op-sequence assertion reads
// identically to the synchronous/pairing interpreters' (FD5 vocabulary).
// =======================================================================

TEST_CASE("run_task [FD10]: retry-with-backoff -- three Sends, backoff "
          "1s/2s, no fourth attempt") {
    trace log;
    int send_calls = 0;
    int virtual_time = 0; // Clock mock: virtual time, no sleep, no threads

    auto network_handler = [&log, &send_calls](Send op) {
        log.push_back(render_operation(op));
        ++send_calls;
        Send::response r =
            (send_calls < 3)
                ? Send::response{std::unexpected(net_error{"boom"})}
                : Send::response{reply{"hello-reply"}};
        return ex::just(r);
    };
    auto clock_handler =
        overloaded{[&log](Now op) {
                       log.push_back(render_operation(op));
                       return ex::just(time_point{42});
                   },
                   [&log, &virtual_time](SleepFor op) {
                       log.push_back(render_operation(op));
                       virtual_time +=
                           op.d; // advance virtual time deterministically
                       return ex::just(unit{});
                   }};
    auto handler = overloaded{network_handler, clock_handler};

    auto result = ex::sync_wait(run_task<Row, int>(handler, retry_program()));

    REQUIRE(result.has_value());
    auto [value] = result.value();

    CHECK(value == 11); // "hello-reply".size()
    CHECK(send_calls == 3);
    CHECK(virtual_time == 3); // 1s + 2s of virtual backoff, no wall clock
    CHECK(log == trace{"Now()", "Send(hello)", "SleepFor(1)", "Send(hello)",
                       "SleepFor(2)", "Send(hello)"});
}

// =======================================================================
// Deliberately-async smoke: the op's sender defers onto sync_wait's own
// run_loop scheduler (read_env(get_scheduler) -> schedule -> then), so the
// coroutine genuinely suspends and resumes ACROSS a scheduling point before
// the value is produced. Deterministic and single-threaded: sync_wait drives
// exactly that run_loop to completion. A `hopped` flag set on the far side
// of the scheduling point, observed after the value returns, witnesses the
// hop actually ran.
// =======================================================================

TEST_CASE("run_task: an op whose sender completes via a scheduler hop "
          "suspends across a scheduling point") {
    bool hopped = false;

    auto handler = overloaded{[&hopped](AsyncGet op) {
        int key = op.key;
        return ex::read_env(ex::get_scheduler) |
               ex::let_value([key, &hopped](auto sched) {
                   return ex::schedule(sched) | ex::then([key, &hopped] {
                              hopped = true;
                              return key * 2;
                          });
               });
    }};

    auto program = mbind(send<Async>(AsyncGet{21}), [](int v) -> AsyncFree {
        return pure_free<Async::template type>(v);
    });

    auto result =
        ex::sync_wait(run_task<Async, int>(handler, std::move(program)));

    REQUIRE(result.has_value());
    auto [value] = result.value();
    CHECK(value == 42);
    CHECK(hopped); // the scheduled far-side of the hop actually executed
}
