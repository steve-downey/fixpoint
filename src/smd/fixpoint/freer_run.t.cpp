// src/smd/fixpoint/freer_run.t.cpp                                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// S04 (ops/freer/steps/04-send-trace.md): send<Sig>(op), the synchronous
// `run` loop, and FD5's trace vocabulary (tracing/run_trace), exercised
// against the KV example from FD1/FD7 -- a store-backed handler and the
// `get k >>= \v -> put k (f v) >>= \_ -> pure v` usage sketch, written with
// `monad_typeclass<...>.bind` + `pure` only (FD4: no derived ops).

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/freer.hpp>
#include <smd/fixpoint/freer_run.hpp>
#include <smd/fixpoint/freer_run.hpp> // Re-inclusion check
#include <smd/fixpoint/one_shot.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <smd/typeclass/detail/typeclass_base.hpp>
#include <smd/typeclass/functor.hpp>
#include <smd/typeclass/monad.hpp>

#include <catch2/catch_test_macros.hpp>

#include <concepts>
#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

using smd::fixpoint::overloaded;
using smd::fixpoint::pure_free;
using smd::fixpoint::run;
using smd::fixpoint::run_trace;
using smd::fixpoint::send;
using smd::fixpoint::signature;
using smd::fixpoint::trace;
using smd::fixpoint::unit;

namespace {

// FD1/FD7 shape: each operation names its own response type and renders
// itself via operator<< for the trace vocabulary (freer_run.hpp's
// traceable_operation, NOT part of freer.hpp's `operation` concept).

struct Get {
    int key;
    using response = std::optional<int>;
};

auto operator<<(std::ostream &out, const Get &op) -> std::ostream & {
    return out << "Get(" << op.key << ")";
}

struct Put {
    int key;
    int value;
    using response = unit;
};

auto operator<<(std::ostream &out, const Put &op) -> std::ostream & {
    return out << "Put(" << op.key << ", " << op.value << ")";
}

using KV = signature<Get, Put>;
using KVFree = smd::fixpoint::Freer<KV, int>;

/** A std::map-backed handler for KV, per FD1's motivating example. */
auto store_handler(std::map<int, int> &store) {
    return overloaded{[&store](Get op) -> Get::response {
                          auto it = store.find(op.key);
                          if (it == store.end())
                              return std::nullopt;
                          return it->second;
                      },
                      [&store](Put op) -> Put::response {
                          store[op.key] = op.value;
                          return unit{};
                      }};
}

/** FD7's usage sketch, written with monad_typeclass<KVFree>.bind + pure
 * only (FD4 -- no derived ops):
 *   get k >>= \v -> put k (f v) >>= \_ -> pure v
 */
auto get_bump_put_program(int key, int (*f)(int)) -> KVFree {
    const auto &monad = smd::typeclass::monad_typeclass<KVFree>;
    return monad.bind(
        send<KV>(Get{key}), [key, f](std::optional<int> v) -> KVFree {
            int value = v.value_or(0);
            const auto &inner = smd::typeclass::monad_typeclass<KVFree>;
            return inner.bind(send<KV>(Put{key, f(value)}),
                              [value](unit) -> KVFree {
                                  return pure_free<KV::template type>(value);
                              });
        });
}

/** A deeper chain: two full get-bump-put rounds, back to back -- four
 * `send`s (FD4/FD6's "left-nested binds" residue) written with the same
 * monad_typeclass<KVFree>.bind + pure vocabulary.
 */
auto double_bump_program(int key) -> KVFree {
    const auto &monad = smd::typeclass::monad_typeclass<KVFree>;
    return monad.bind(
        send<KV>(Get{key}), [key](std::optional<int> v) -> KVFree {
            int v1 = v.value_or(0) + 1;
            const auto &m2 = smd::typeclass::monad_typeclass<KVFree>;
            return m2.bind(send<KV>(Put{key, v1}), [key](unit) -> KVFree {
                const auto &m3 = smd::typeclass::monad_typeclass<KVFree>;
                return m3.bind(
                    send<KV>(Get{key}), [key](std::optional<int> v2) -> KVFree {
                        int v3 = v2.value_or(0) + 1;
                        const auto &m4 =
                            smd::typeclass::monad_typeclass<KVFree>;
                        return m4.bind(
                            send<KV>(Put{key, v3}), [v3](unit) -> KVFree {
                                return pure_free<KV::template type>(v3);
                            });
                    });
            });
        });
}

/** A LONG chain of N separate suspended Get/Put pairs, each constructed
 * lazily -- one at a time, only as `run`'s loop resumes the previous node
 * -- rather than eagerly nested at construction time. Evidence for the
 * handoff's "how deep a bind chain does the loop handle comfortably"
 * question: `run`'s while-loop drives this recursion one resumption at a
 * time, in O(1) native stack space per step, regardless of N.
 */
auto counter_chain_program(int key, int remaining) -> KVFree {
    const auto &monad = smd::typeclass::monad_typeclass<KVFree>;
    if (remaining == 0) {
        return monad.bind(
            send<KV>(Get{key}), [](std::optional<int> v) -> KVFree {
                return pure_free<KV::template type>(v.value_or(0));
            });
    }
    return monad.bind(
        send<KV>(Get{key}), [key, remaining](std::optional<int> v) -> KVFree {
            int next = v.value_or(0) + 1;
            const auto &inner = smd::typeclass::monad_typeclass<KVFree>;
            return inner.bind(
                send<KV>(Put{key, next}), [key, remaining](unit) -> KVFree {
                    return counter_chain_program(key, remaining - 1);
                });
        });
}

} // namespace

// =======================================================================
// FD5/FD7: run_trace yields the expected value AND the exact op sequence,
// against the FD1 KV example.
// =======================================================================

TEST_CASE("run_trace [FD5][FD7]: KV get-bump-put program yields the "
          "fetched value and the exact op sequence") {
    std::map<int, int> store{{7, 10}};
    auto handler = store_handler(store);

    auto [value, log] = run_trace<KV, int>(
        handler, get_bump_put_program(7, [](int v) { return v + 1; }));

    CHECK(value == 10);
    CHECK(log == trace{"Get(7)", "Put(7, 11)"});
    CHECK(store.at(7) == 11);
}

TEST_CASE("run_trace [FD5]: a Get on an absent key hands the handler "
          "response through unmodified") {
    std::map<int, int> store; // key 3 absent
    auto handler = store_handler(store);

    auto [value, log] = run_trace<KV, int>(
        handler, get_bump_put_program(3, [](int v) { return v + 5; }));

    CHECK(value == 0); // Get -> nullopt -> value_or(0)
    CHECK(log == trace{"Get(3)", "Put(3, 5)"});
    CHECK(store.at(3) == 5);
}

// =======================================================================
// FD6's left-nested-bind residue: a deeper chain (four sends) resumes
// correctly, front to back, through run_trace.
// =======================================================================

TEST_CASE("run_trace [FD6]: a four-send chain (two get-bump-put rounds) "
          "resumes in order") {
    std::map<int, int> store{{2, 0}};
    auto handler = store_handler(store);

    auto [value, log] = run_trace<KV, int>(handler, double_bump_program(2));

    CHECK(value == 2);
    CHECK(log == trace{"Get(2)", "Put(2, 1)", "Get(2)", "Put(2, 2)"});
    CHECK(store.at(2) == 2);
}

// =======================================================================
// run's loop scales linearly in the number of separate effects, not the
// C++ call stack -- evidence for "how deep a bind chain does the loop
// comfortably handle" (handoff). 500 rounds = 500 get/put pairs plus one
// trailing bare Get (counter_chain_program's base case), 1001 ops total;
// construction is O(1) per round (each round is only built when the
// previous is resumed), and `run` visits them one at a time via its while
// loop.
// =======================================================================

TEST_CASE("run_trace: a long chain of separately-suspended operations "
          "(500 get/put rounds) runs without recursing the native stack") {
    constexpr int rounds = 500;
    std::map<int, int> store{{9, 0}};
    auto handler = store_handler(store);

    auto [value, log] =
        run_trace<KV, int>(handler, counter_chain_program(9, rounds));

    CHECK(value == rounds);
    CHECK(log.size() == 2 * static_cast<std::size_t>(rounds) + 1);
    CHECK(log.front() == "Get(9)");
    CHECK(log.back() == "Get(9)"); // the base case's trailing bare Get
    CHECK(store.at(9) == rounds);
}

// =======================================================================
// plain run (no trace) drives the same program to the same value.
// =======================================================================

TEST_CASE("run [FD7]: drives the KV program to completion without a "
          "trace") {
    std::map<int, int> store{{5, 41}};
    auto handler = store_handler(store);

    int value = run<KV, int>(
        handler, get_bump_put_program(5, [](int v) { return v + 1; }));

    CHECK(value == 41);
    CHECK(store.at(5) == 42);
}

// =======================================================================
// One-shot-ness of `run` (FD4/FD12), observed at compile time, not
// runtime: `run` takes `Freer<Sig,A>&&`, so calling it on a named lvalue
// program directly (no std::move) is ill-formed -- the program cannot be
// silently re-run out from under the type system.
// =======================================================================

// std::invocable, not a bare `requires{ run(h, p); }`: once Sig/A/Handler
// are all resolved, binding a (now non-dependent) `Freer<Sig,A>&&`
// parameter to a named lvalue is an ordinary reference-binding error, not
// a template substitution failure -- outside a requires-expression's
// "immediate context" carve-out, so a raw `requires{...}` here hard-errors
// instead of evaluating to false (confirmed empirically while writing this
// test; see the S04 handoff). `std::invocable` is specified precisely to
// answer "would this call compile" without that pitfall.
namespace {
using KVHandler = decltype(store_handler(std::declval<std::map<int, int> &>()));
using RunFn = decltype(run<KV, int, KVHandler &>);
} // namespace

static_assert(std::invocable<RunFn, KVHandler &, KVFree &&>,
              "run is invocable given an explicit std::move of the program");
static_assert(!std::invocable<RunFn, KVHandler &, KVFree &>,
              "run does not accept a named lvalue program -- no accidental "
              "re-run without an explicit std::move");

// =======================================================================
// FD5 restated: no `==` on a Freer/Free value anywhere in this file --
// every assertion above is on `int`, `trace` (std::vector<std::string>),
// or std::map, never on a Freer<KV,int> itself.
// =======================================================================
