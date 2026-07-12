// src/smd/fixpoint/freer_cosignature.t.cpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// S06 (ops/freer/steps/06-cofree-pairing.md): unfold_cofree (Cofree's ana
// half, tested standalone against a pure-data functor), the lazy
// co-signature Functor instance (FD8, the dual of S03's Coyoneda instance),
// and pair_run/pair_run_trace -- the Free/Cofree pairing as a second,
// synchronous interpreter alongside run/run_trace (freer_run.hpp). The
// gate: FD10's failing-twice Network script drives the retry-with-backoff
// program over row<Clock, Network> synchronously through pair_run_trace.

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/cofree.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/freer.hpp>
#include <smd/fixpoint/freer_cosignature.hpp>
#include <smd/fixpoint/freer_cosignature.hpp> // Re-inclusion check
#include <smd/fixpoint/freer_row.hpp>
#include <smd/fixpoint/freer_run.hpp>
#include <smd/fixpoint/unfold_cofree.hpp>

#include <examples/freer_retry_program.hpp>

#include <smd/concrete/functors.hpp>

#include <smd/typeclass/detail/typeclass_base.hpp>
#include <smd/typeclass/monad.hpp>

#include <catch2/catch_test_macros.hpp>

#include <expected>
#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>

using smd::concrete::NatF;
using smd::concrete::Succ;
using smd::concrete::Zero;
using smd::fixpoint::Box;
using smd::fixpoint::Cofree;
using smd::fixpoint::cosignature;
using smd::fixpoint::extract;
using smd::fixpoint::Freer;
using smd::fixpoint::Mock;
using smd::fixpoint::operation;
using smd::fixpoint::pair_run;
using smd::fixpoint::pair_run_trace;
using smd::fixpoint::pure_free;
using smd::fixpoint::responder;
using smd::fixpoint::row;
using smd::fixpoint::send;
using smd::fixpoint::signature;
using smd::fixpoint::trace;
using smd::fixpoint::unfold_cofree;
using smd::fixpoint::unit;
using smd::fixpoint::unwrap_cofree;
using smd::typeclass::mbind;

// =======================================================================
// unfold_cofree [FD8/audit correction]: stands alone, independent of the
// co-signature -- exercised against a pure-data functor (NatF), the same
// functor recursion_schemes.t.cpp/cofree.t.cpp already use, registered
// with functor_typeclass in smd/concrete/functors.hpp.
// =======================================================================

namespace {

using AnnNat = Cofree<NatF, std::string>;

} // namespace

TEST_CASE("unfold_cofree [FD8]: builds a Cofree tree over a pure-data "
          "functor (NatF) by unfolding a coalgebra pair") {
    auto coalgebra = [](const int &n) -> NatF<int> {
        if (n <= 0) {
            return Zero{};
        }
        return Succ<int>{smd::fixpoint::make_box<int>(n - 1)};
    };
    auto head_fn = [](const int &n) -> std::string {
        return "n=" + std::to_string(n);
    };

    AnnNat tree = unfold_cofree<NatF>(head_fn, coalgebra, 3);

    CHECK(extract(tree) == "n=3");
    REQUIRE(std::holds_alternative<Succ<AnnNat>>(unwrap_cofree(tree)));
    const auto &s3 = std::get<Succ<AnnNat>>(unwrap_cofree(tree));
    CHECK(extract(*s3.pred) == "n=2");
    REQUIRE(std::holds_alternative<Succ<AnnNat>>(unwrap_cofree(*s3.pred)));
    const auto &s2 = std::get<Succ<AnnNat>>(unwrap_cofree(*s3.pred));
    CHECK(extract(*s2.pred) == "n=1");
    REQUIRE(std::holds_alternative<Succ<AnnNat>>(unwrap_cofree(*s2.pred)));
    const auto &s1 = std::get<Succ<AnnNat>>(unwrap_cofree(*s2.pred));
    CHECK(extract(*s1.pred) == "n=0");
    CHECK(std::holds_alternative<Zero>(unwrap_cofree(*s1.pred)));
}

TEST_CASE("unfold_cofree: a terminal seed (n=0) yields a single "
          "Zero-tailed node") {
    auto coalgebra = [](const int &n) -> NatF<int> {
        if (n <= 0) {
            return Zero{};
        }
        return Succ<int>{smd::fixpoint::make_box<int>(n - 1)};
    };
    auto head_fn = [](const int &n) -> int { return n * n; };

    auto tree = unfold_cofree<NatF>(head_fn, coalgebra, 0);
    CHECK(extract(tree) == 0);
    CHECK(std::holds_alternative<Zero>(unwrap_cofree(tree)));
}

// =======================================================================
// The co-signature's Functor instance is LAZY (FD8, the dual of FD6):
// fmap must post-compose onto each stored responder WITHOUT invoking it --
// asserted with a side-effect flag, exactly as S03's Coyoneda laziness
// test does (freer.t.cpp).
// =======================================================================

namespace {

struct Ask {
    using response = int;
};

using AskSig = signature<Ask>;

} // namespace

// Asan-gated (FD4), as well as the laziness assertion: `mapped` is built
// entirely inside an IIFE, so everything named at construction time --
// `layer`, and (load-bearing) the `fn` lambda's own captured `salt` -- is
// gone by the time the outer scope invokes `mapped.respond`. `salt` is real
// per-call state, not stateless (S02-S05's lesson: a stateless closure
// makes a dangling reference capture invisible to Asan because nothing is
// ever read through it). This test's teeth were verified by hand:
// temporarily changing detail::cosignature_layer<...>::fmap_one's new
// responder capture in freer_cosignature.hpp from `[old =
// std::move(r.respond), fn]` to `[old = std::move(r.respond), &fn]`,
// rebuilding under gcc-16, and running this test in isolation reliably
// fails (`AddressSanitizer: stack-use-after-scope ... in operator()` at
// the line reading the dangling `salt` through the reference-captured
// `fn`); reverting to the value capture and rebuilding both pinned gates
// passes again -- see the S06 handoff for the transcript.
TEST_CASE("cosignature layer fmap [FD8]: post-composes onto the stored "
          "responder without invoking it; the new responder owns its "
          "captures (FD4)") {
    bool invoked = false;

    auto mapped = [&invoked] {
        // real captured state, HEAP-backed (a std::string, not a bare int):
        // a dangling reference to `fn` becomes a heap-use-after-free Asan
        // catches regardless of how aggressively the (all-inline, templated)
        // fmap machinery collapses stack frames -- a bare int on reclaimed
        // stack reads clean under inlining and would make the teeth silent.
        std::string salt = "salt";
        cosignature<AskSig>::type<int> layer{
            responder<Ask, int>{[&invoked](Ask) -> std::pair<int, int> {
                invoked = true;
                return {42, 99};
            }}};

        // fn maps the layer's recursive position (int, the mock's "next
        // state" seed) to a std::string -- exactly FD8's granularity: fmap
        // operates on the whole layer, mapping X to Y, never touching the
        // response type.
        return smd::fixpoint::layer_fmap(
            [salt](int &&next) -> std::string {
                return salt + "=" + std::to_string(next);
            },
            std::move(layer));
    }();

    CHECK_FALSE(invoked); // one closure frame added; the old responder
                          // was never called

    auto [response, next] = mapped.respond(Ask{});
    CHECK(invoked); // only *now*, on invocation, does the original
                    // responder run
    CHECK(response == 42);
    CHECK(next == "salt=99");
}

// =======================================================================
// A scripted KV mock, built with unfold_cofree over the co-signature, paired
// against the same KV program shape S04 drove through run/run_trace
// (freer_run.t.cpp) -- value and trace assertions in the same form.
// =======================================================================

namespace {

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
using KVFree = Freer<KV, int>;
using Store = std::map<int, int>;

/** FD7's usage sketch, written with monad_typeclass<KVFree>.bind + pure
 * only (FD4), matching freer_run.t.cpp's get_bump_put_program shape
 * exactly (duplicated locally -- anonymous-namespace types cannot cross
 * translation units).
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

/** The scripted mock's coalgebra: Store -> cosignature<KV>::type<Store> --
 * Get answers from the current store (unchanged next state), Put answers
 * unit and yields a next state with the key updated. Never reaches a
 * terminal layer (there is always a "next" store) -- fine, because the
 * co-signature's Functor instance is lazy (FD8): unfold_cofree only
 * builds one node eagerly per call, deferring the rest until a responder
 * is actually invoked by pair_run.
 */
auto kv_coalgebra(const Store &store) -> cosignature<KV>::type<Store> {
    return cosignature<KV>::type<Store>{
        responder<Get, Store>{
            [store](Get op) -> std::pair<Get::response, Store> {
                auto it = store.find(op.key);
                if (it == store.end()) {
                    return {std::nullopt, store};
                }
                return {it->second, store};
            }},
        responder<Put, Store>{
            [store](Put op) -> std::pair<Put::response, Store> {
                Store next = store;
                next[op.key] = op.value;
                return {unit{}, next};
            }}};
}

auto kv_head_fn(const Store &store) -> Store { return store; }

} // namespace

TEST_CASE("pair_run [FD8]: a scripted KV mock drives the same program "
          "run/run_trace would, without a trace") {
    Store initial_store{{5, 41}};
    Mock<KV, Store> mock = unfold_cofree<cosignature<KV>::template type>(
        kv_head_fn, kv_coalgebra, initial_store);

    auto [value, final_store] = pair_run<KV, int, Store>(
        std::move(mock), get_bump_put_program(5, [](int v) { return v + 1; }));

    CHECK(value == 41);
    CHECK(final_store.at(5) == 42);
}

TEST_CASE("pair_run_trace [FD8]: a scripted KV mock (built with "
          "unfold_cofree) yields the fetched value and the exact op "
          "sequence, same form as S04's run_trace") {
    Store initial_store{{7, 10}};
    Mock<KV, Store> mock = unfold_cofree<cosignature<KV>::template type>(
        kv_head_fn, kv_coalgebra, initial_store);

    auto [value, final_store, log] = pair_run_trace<KV, int, Store>(
        std::move(mock), get_bump_put_program(7, [](int v) { return v + 1; }));

    CHECK(value == 10);
    CHECK(log == trace{"Get(7)", "Put(7, 11)"});
    CHECK(final_store.at(7) == 11);
}

// =======================================================================
// FD10's motivating example: retry-with-backoff over row<Clock, Network>,
// a scripted Network (via the co-signature/unfold_cofree, failing twice
// then succeeding) paired with a virtual-time Clock in the SAME mock --
// the paper's central demonstration (FD8). The program, its Clock/Network
// vocabulary, and the scripted mock now live in ONE canonical place,
// examples/freer_retry_program.hpp (S08 consolidation); this test drives
// that canonical program through the pairing interpreter. The live S/R
// interpreter drives the SAME program in freer_task.t.cpp, and both meet
// on one program in freer_retry.t.cpp.
// =======================================================================

using retry_example::retry_coalgebra;
using retry_example::retry_head_fn;
using retry_example::retry_program;
using retry_example::RetryScript;
using retry_example::Row;

TEST_CASE("pair_run_trace [FD10]: a scripted Network (failing twice, then "
          "succeeding) paired with a virtual-time Clock drives the retry "
          "program synchronously") {
    Mock<Row, RetryScript> mock =
        unfold_cofree<cosignature<Row>::template type>(
            retry_head_fn, retry_coalgebra, RetryScript{});

    auto [value, final_state, log] =
        pair_run_trace<Row, int, RetryScript>(std::move(mock), retry_program());

    CHECK(value == 11); // "hello-reply".size()
    CHECK(log == trace{"Now()", "Send(hello)", "SleepFor(1)", "Send(hello)",
                       "SleepFor(2)", "Send(hello)"}); // fetch time, then
                                                       // exactly three Sends,
                                                       // backoff 1s/2s, no
                                                       // fourth attempt
    CHECK(final_state.send_count == 3);
    CHECK(final_state.virtual_time == 3); // 1 + 2
}

// =======================================================================
// FD5 restated: no `==` on a Freer/Free/Cofree/row value anywhere in this
// file -- every assertion above is on `int`, `Store`/`RetryScript`
// (plain-data types with their own `==`/member access), `trace`
// (std::vector<std::string>), or a plain `bool`, never on a Freer/Mock
// value itself.
// =======================================================================
