// src/smd/fixpoint/freer.t.cpp                                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// S03 (ops/freer/steps/03-signature-layer.md): re-expresses S01's baseline
// probe (freer_baseline.t.cpp) against the real freer.hpp -- signature,
// impure_node, Freer, and the generic Coyoneda Functor instance -- and adds
// the FD6 laziness test and the FD4 capture-ownership Asan lifetime test
// S02's free.t.cpp previewed structurally against LazyLayer.

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/free.hpp>
#include <smd/fixpoint/freer.hpp>
#include <smd/fixpoint/freer.hpp> // Re-inclusion check
#include <smd/fixpoint/one_shot.hpp>

#include <smd/typeclass/detail/typeclass_base.hpp>
#include <smd/typeclass/functor.hpp>
#include <smd/typeclass/monad.hpp>

#include <catch2/catch_test_macros.hpp>

#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

using smd::fixpoint::Box;
using smd::fixpoint::Freer;
using smd::fixpoint::impure_node;
using smd::fixpoint::is_pure;
using smd::fixpoint::make_box;
using smd::fixpoint::one_shot;
using smd::fixpoint::pure_free;
using smd::fixpoint::roll_free;
using smd::fixpoint::signature;
using smd::fixpoint::unit;

namespace {

// FD1/FD2 shape: each operation names its own response type.

struct Get {
    int key;
    using response = std::optional<int>;
};

struct Put {
    int key;
    int value;
    using response = unit;
};

// No `response` member: the operation concept's negative case.
struct NotAnOperation {
    int payload;
};

using KV = signature<Get, Put>;
using KVFree = Freer<KV, int>;
using KVLayer = KV::template type<KVFree>;

/** Build a single suspended Get program: Op = Get{key}, continuation is
 * "identity-into-Pure" (FD7's send() shape, inlined -- send() itself is
 * S04's job).
 */
auto make_get_program(int key) -> KVFree {
    impure_node<Get, KVFree> get_node{
        Get{key}, one_shot<Box<KVFree>(std::optional<int>)>(
                      [](std::optional<int> r) -> Box<KVFree> {
                          return make_box<KVFree>(
                              pure_free<KV::template type>(r.value_or(-1)));
                      })};
    KVLayer layer{KVLayer::node_variant{
        std::in_place_type<impure_node<Get, KVFree>>, std::move(get_node)}};
    return roll_free<KV::template type>(std::move(layer));
}

} // namespace

// =======================================================================
// FD2: the operation concept.
// =======================================================================

static_assert(smd::fixpoint::operation<Get>,
              "Get is movable and names a response type");
static_assert(smd::fixpoint::operation<Put>,
              "Put is movable and names a response type");
static_assert(!smd::fixpoint::operation<NotAnOperation>,
              "a payload with no `response` member is not an operation");

// =======================================================================
// FD5/FD9 (S01 DEV-S01-1 fallback): the raw Coyoneda layer, not Freer<KV,
// int> itself, is the type to assert non-comparability on. Free's
// hand-written friend operator== is declaration-visible the moment
// Free<F,A> is instantiated (regardless of whether its body would compile),
// so std::equality_comparable<KVFree> reports true on both pins -- see
// freer_baseline.t.cpp and its handoff for the full discovery. The layer
// itself has no operator== at all and correctly reports false.
// =======================================================================

static_assert(!std::equality_comparable<KVLayer>,
              "the Coyoneda layer (no operator== declared) is not "
              "equality_comparable -- assert here, not on Freer<KV,int>, "
              "per DEV-S01-1");

// =======================================================================
// Probe re-expressed against the real header (FD3/FD7/FD9 item 1): build
// one suspended node by hand, resume by hand, CHECK the Pure result.
// =======================================================================

TEST_CASE("Freer [FD3][FD9]: hand-built impure_node/Freer constructs and "
          "resumes") {
    KVFree suspended = make_get_program(42);
    REQUIRE_FALSE(is_pure(suspended));

    auto &suspended_layer = std::get<KVLayer>(suspended.node);
    auto &suspended_get =
        std::get<impure_node<Get, KVFree>>(suspended_layer.node);

    Box<KVFree> resumed = std::move(suspended_get.k)(std::optional<int>{7});
    CHECK(is_pure(*resumed));
    CHECK(std::get<int>((*resumed).node) == 7);
}

// =======================================================================
// FD6: layer fmap is closure post-composition -- O(1), no traversal. fmap
// over a suspended layer must NOT invoke the stored continuation; resuming
// afterwards must yield the mapped result.
// =======================================================================

TEST_CASE("Freer layer fmap [FD6]: post-composes onto the continuation "
          "without invoking it") {
    bool invoked = false;
    impure_node<Get, KVFree> get_node{
        Get{1}, one_shot<Box<KVFree>(std::optional<int>)>(
                    [&invoked](std::optional<int> r) -> Box<KVFree> {
                        invoked = true;
                        return make_box<KVFree>(
                            pure_free<KV::template type>(r.value_or(-1)));
                    })};
    KVLayer layer{KVLayer::node_variant{
        std::in_place_type<impure_node<Get, KVFree>>, std::move(get_node)}};

    // fn maps the layer's recursive position (KVFree) to a plain int --
    // exactly FD6's granularity: fmap operates on the whole layer, mapping
    // X (here KVFree) to Y, not on whatever value eventually lands inside
    // a resumed Pure leaf.
    auto mapped = smd::fixpoint::layer_fmap(
        [](KVFree &&f) -> int { return std::get<int>(f.node) * 10; },
        std::move(layer));

    CHECK_FALSE(invoked); // one closure frame added; nothing traversed/ran

    auto &mapped_get = std::get<impure_node<Get, int>>(mapped.node);
    Box<int> resumed = std::move(mapped_get.k)(std::optional<int>{4});
    CHECK(invoked); // only *now*, on resumption, does the original k run
    CHECK(*resumed == 40);
}

// =======================================================================
// FD3 windfall, confirmed at instantiation (S01 only checked lookup
// selection): monad_typeclass<Freer<KV,int>> is reachable with zero new
// registration at the Free<F,A> level, and its consuming `bind` (S02)
// actually runs end-to-end through the new layer-level Functor instance
// this step registers.
// =======================================================================

TEST_CASE("Freer consuming bind [FD3 windfall]: monad_typeclass<Freer<KV, "
          "int>> sequences through a suspended Get") {
    const auto &monad = smd::typeclass::monad_typeclass<KVFree>;
    KVFree bound = monad.bind(make_get_program(7), [](int v) -> KVFree {
        return pure_free<KV::template type>(v + 1000);
    });

    REQUIRE_FALSE(is_pure(bound));
    auto &bound_layer = std::get<KVLayer>(bound.node);
    auto &bound_get = std::get<impure_node<Get, KVFree>>(bound_layer.node);

    Box<KVFree> resumed = std::move(bound_get.k)(std::optional<int>{7});
    CHECK(is_pure(*resumed));
    CHECK(std::get<int>((*resumed).node) == 1007);
}

// =======================================================================
// FD4 capture-ownership rule, Asan-gated (S02's deferred-invocation
// pattern, promoted against the real layer). Everything named in the inner
// scope -- the program, the continuation `k`, the monad dictionary
// reference, the bound Freer value -- is gone by the time the outer scope
// invokes the extracted continuation. `bump` is real per-call state (not
// stateless): S02 found empirically that a stateless continuation makes a
// dangling reference capture invisible to Asan, because nothing is ever
// read through it. This test's teeth were verified by hand: temporarily
// changing CoyonedaFunctorImpl::fmap's inner continuation capture in
// freer.hpp from `fn = std::move(fn)` to `&fn` reliably fails this test
// under the default (Asan) `make test` config (heap-use-after-free /
// stack-use-after-scope, depending on toolchain), and reverting to the
// move-capture makes it pass again -- see the S03 handoff for the
// transcript.
// =======================================================================

TEST_CASE("Freer consuming bind: continuation owns its captures (FD4)") {
    auto continuation = [] {
        KVFree program = make_get_program(1);

        int bump = 1000; // real captured state -- see comment above
        auto k = [bump](int v) -> KVFree {
            return pure_free<KV::template type>(v + bump);
        };
        const auto &monad = smd::typeclass::monad_typeclass<KVFree>;
        KVFree bound = monad.bind(std::move(program), k);
        auto &layer = std::get<KVLayer>(bound.node);
        auto &get = std::get<impure_node<Get, KVFree>>(layer.node);
        return std::move(get.k);
    }();

    Box<KVFree> result = std::move(continuation)(std::optional<int>{7});
    REQUIRE(is_pure(*result));
    CHECK(std::get<int>((*result).node) == 1007);
}
