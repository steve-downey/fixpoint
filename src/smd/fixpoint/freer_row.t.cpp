// src/smd/fixpoint/freer_row.t.cpp                                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// S05 (ops/freer/steps/05-rows.md): row<Sigs...>, member<Op,SigOrRow>, and
// discharge<Sig>(handler, prog), exercised against FD10's Clock+Network
// example -- two independent signatures composed into one row, Network
// mocked with a scripted synchronous handler, Clock real-shaped (virtual
// time, no actual sleeping).

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/freer.hpp>
#include <smd/fixpoint/freer_row.hpp>
#include <smd/fixpoint/freer_row.hpp> // Re-inclusion check
#include <smd/fixpoint/freer_run.hpp>
#include <smd/fixpoint/one_shot.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <smd/typeclass/detail/typeclass_base.hpp>
#include <smd/typeclass/monad.hpp>

#include <catch2/catch_test_macros.hpp>

#include <expected>
#include <ostream>
#include <string>
#include <utility>

using smd::fixpoint::Box;
using smd::fixpoint::discharge;
using smd::fixpoint::Freer;
using smd::fixpoint::impure_node;
using smd::fixpoint::is_pure;
using smd::fixpoint::member;
using smd::fixpoint::overloaded;
using smd::fixpoint::pure_free;
using smd::fixpoint::row;
using smd::fixpoint::run;
using smd::fixpoint::run_trace;
using smd::fixpoint::send;
using smd::fixpoint::signature;
using smd::fixpoint::trace;
using smd::fixpoint::unit;
using smd::typeclass::mbind;

namespace {

// FD10's motivating example, verbatim: two independent signatures, so
// composition of mocked algebras shows up in the row's own tests, not a
// later generalization. Test-local time_point/duration/request/reply/
// net_error stand-ins, per the step file.

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

struct Now {
    using response = time_point;
};

auto operator<<(std::ostream &out, const Now &) -> std::ostream & {
    return out << "Now()";
}

struct SleepFor {
    duration d;
    using response = unit;
};

auto operator<<(std::ostream &out, const SleepFor &op) -> std::ostream & {
    return out << "SleepFor(" << op.d << ")";
}

using Clock = signature<Now, SleepFor>;

struct Send {
    request req;
    using response = std::expected<reply, net_error>;
};

auto operator<<(std::ostream &out, const Send &op) -> std::ostream & {
    return out << "Send(" << op.req.body << ")";
}

using Network = signature<Send>;

using Row = row<Clock, Network>;
using RowFree = Freer<Row, int>;

// A retry-shaped program (FD10): fetch the current time, send a request; on
// failure, sleep, then retry once; the final value is the successful
// reply's length (or -1 if both attempts fail). Written with
// smd::typeclass::mbind + pure only (FD4: no derived ops), per the S04
// handoff's ergonomics note -- mbind is a direct forward to
// monad_typeclass<...>.bind, not a derived op.
auto retry_program() -> RowFree {
    return mbind(send<Row>(Now{}), [](time_point) -> RowFree {
        return mbind(
            send<Row>(Send{request{"hello"}}),
            [](std::expected<reply, net_error> first) -> RowFree {
                if (first) {
                    return pure_free<Row::template type>(
                        static_cast<int>(first->body.size()));
                }
                return mbind(send<Row>(SleepFor{1}), [](unit) -> RowFree {
                    return mbind(
                        send<Row>(Send{request{"hello"}}),
                        [](std::expected<reply, net_error> second) -> RowFree {
                            if (second) {
                                return pure_free<Row::template type>(
                                    static_cast<int>(second->body.size()));
                            }
                            return pure_free<Row::template type>(-1);
                        });
                });
            });
    });
}

/** A Clock handler: virtual time, no sleep_for, no threads (FD10's
 * "real-shaped Clock mock").
 */
auto clock_handler() {
    return overloaded{[](Now) -> Now::response { return time_point{42}; },
                      [](SleepFor) -> SleepFor::response { return unit{}; }};
}

// An op that belongs to neither Clock nor Network -- the member concept's
// negative case.
struct NotInRow {
    using response = int;
};

} // namespace

// =======================================================================
// member<Op, SigOrRow>: accepts/rejects, over both bare signatures and the
// composed row.
// =======================================================================

static_assert(member<Now, Clock>, "Now is a Clock operation");
static_assert(member<SleepFor, Clock>, "SleepFor is a Clock operation");
static_assert(!member<Send, Clock>, "Send is not a Clock operation");

static_assert(member<Send, Network>, "Send is a Network operation");
static_assert(!member<Now, Network>, "Now is not a Network operation");
static_assert(!member<SleepFor, Network>,
              "SleepFor is not a Network operation");

static_assert(member<Now, Row>, "Now is a member of row<Clock, Network>");
static_assert(member<SleepFor, Row>,
              "SleepFor is a member of row<Clock, Network>");
static_assert(member<Send, Row>, "Send is a member of row<Clock, Network>");

static_assert(!member<NotInRow, Clock>);
static_assert(!member<NotInRow, Network>);
static_assert(!member<NotInRow, Row>, "NotInRow is not a member of any row "
                                      "signature -- the negative case");

// =======================================================================
// A one-signature row behaves exactly like the bare signature: send/run
// interop, same value and trace as using Clock directly would produce.
// =======================================================================

TEST_CASE("row<Clock> alone: send/run interop matches the bare signature") {
    using SoloRow = row<Clock>;
    using SoloFree = Freer<SoloRow, int>;

    auto program = []() -> SoloFree {
        return mbind(send<SoloRow>(Now{}), [](time_point tp) -> SoloFree {
            return mbind(send<SoloRow>(SleepFor{5}), [tp](unit) -> SoloFree {
                return pure_free<SoloRow::template type>(tp.t);
            });
        });
    };

    auto [value, log] = run_trace<SoloRow, int>(clock_handler(), program());

    CHECK(value == 42);
    CHECK(log == trace{"Now()", "SleepFor(5)"});
}

// =======================================================================
// Discharge Network with a scripted synchronous handler (fails once, then
// succeeds -- FD10's scripted mock) while Clock ops flow through
// untouched: discharge<Network> re-emits Now/SleepFor into row<Clock>,
// and run_trace over THAT program shows only the Clock ops -- the Network
// side is fully resolved inline by discharge, invisible to this trace.
// =======================================================================

TEST_CASE("discharge [FD10]: Network mocked (fail-then-succeed), Clock "
          "flows through untouched") {
    int send_calls = 0;
    auto network_handler = overloaded{[&send_calls](Send) -> Send::response {
        ++send_calls;
        if (send_calls == 1) {
            return std::unexpected(net_error{"boom"});
        }
        return reply{"hello-reply"}; // length 11
    }};

    auto discharged =
        discharge<Network, Row, int>(network_handler, retry_program());

    auto [value, log] =
        run_trace<row<Clock>, int>(clock_handler(), std::move(discharged));

    CHECK(value == 11);                          // "hello-reply".size()
    CHECK(log == trace{"Now()", "SleepFor(1)"}); // Send is invisible here
    CHECK(send_calls == 2);
}

// =======================================================================
// Order-independence (FD5, observational equality): discharging Clock
// first, then Network, yields the same final value as the reverse order
// above -- same program, same scripted Network behavior, different
// discharge order.
// =======================================================================

TEST_CASE("discharge [FD5]: Clock-first-then-Network yields the same "
          "observation as Network-first-then-Clock") {
    int send_calls = 0;
    auto network_handler = overloaded{[&send_calls](Send) -> Send::response {
        ++send_calls;
        if (send_calls == 1) {
            return std::unexpected(net_error{"boom"});
        }
        return reply{"hello-reply"};
    }};

    auto discharged =
        discharge<Clock, Row, int>(clock_handler(), retry_program());

    int value = run<row<Network>, int>(network_handler, std::move(discharged));

    CHECK(value == 11); // matches the Network-first path's final value
    CHECK(send_calls == 2);
}

// =======================================================================
// FD4 capture-ownership rule, Asan-gated: discharge's re-emitted
// continuation (the non-matching branch's closure) must own `k` and
// `handler` by move. `bump` is real per-call state (S02/S03's lesson: a
// stateless continuation makes a dangling reference capture invisible to
// Asan because nothing is ever read through it). This test's teeth were
// verified by hand: temporarily rewriting discharge's re-emit closure in
// freer_row.hpp to capture `node.k`/`handler` by reference (via named
// locals) reliably fails this test under the default (Asan) `make test`
// config; reverting to the move-capture makes it pass again -- see the S05
// handoff for the transcript.
// =======================================================================

TEST_CASE("discharge: the re-emitted continuation owns its captures (FD4)") {
    using SmallRow = row<Clock>;
    using SmallFree = Freer<SmallRow, int>;

    auto continuation = [] {
        int bump = 1000; // real captured state -- see comment above
        RowFree program =
            mbind(send<Row>(Now{}), [bump](time_point tp) -> RowFree {
                return pure_free<Row::template type>(tp.t + bump);
            });

        auto network_handler =
            overloaded{[](Send) -> Send::response { return reply{"unused"}; }};

        // Now is not a Network op: discharge re-emits it into row<Clock>,
        // building the closure under test.
        SmallFree discharged =
            discharge<Network, Row, int>(network_handler, std::move(program));

        using SmallLayer = typename SmallRow::template type<SmallFree>;
        auto &layer = std::get<SmallLayer>(discharged.node);
        auto &node = std::get<impure_node<Now, SmallFree>>(layer.node);
        return std::move(node.k);
    }();

    Box<SmallFree> result = std::move(continuation)(time_point{7});
    REQUIRE(is_pure(*result));
    CHECK(std::get<int>((*result).node) == 1007);
}

// =======================================================================
// FD5 restated: no `==` on a Freer/Free/row value anywhere in this file --
// every assertion above is on `int`, `trace` (std::vector<std::string>),
// or a plain `bool`/count, never on a RowFree itself.
// =======================================================================
