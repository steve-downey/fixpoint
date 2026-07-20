// src/smd/fixpoint/freer_baseline.t.cpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// S01 (ops/freer/steps/01-baseline-gate.md) — FD9 baseline gate: evidence,
// not API. Every type below is a *test-local* hand-rolled copy of the
// FD1/FD2/FD3/FD7 shapes (Get/Put, impure_node, KVSig, the identity-into-
// Pure continuation) — freer.hpp does not exist yet; that lands in S03,
// which reuses the local spellings that compile here. Each probe is an
// independent TEST_CASE or static_assert cluster tagged with the FD/
// decision it gates, so one probe's failure cannot take the others down.

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/fmap.hpp>
#include <smd/fixpoint/free.hpp>

#include <smd/typeclass/detail/typeclass_base.hpp>
#include <smd/typeclass/functor.hpp>
#include <smd/typeclass/monad.hpp>

#include <catch2/catch_test_macros.hpp>

#include <concepts>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

// These probes are historical evidence *about* std::move_only_function --
// the D-B input that led to one_shot.hpp (whose file comment records the
// outcome: libc++, on both the Linux and Apple rows of the CI matrix,
// does not declare std::move_only_function at all). On a toolchain
// without the type the gate cannot be expressed, so it compiles out;
// rewriting the probes in terms of one_shot would gut what they are
// evidence of. The library itself never uses std::move_only_function.
#if !defined(__cpp_lib_move_only_function)

TEST_CASE("freer baseline: gated off -- no std::move_only_function") {
    SUCCEED("this standard library does not declare "
            "std::move_only_function; the D-B baseline probes are "
            "compiled out (one_shot.hpp is the library's continuation "
            "representation on every toolchain)");
}

#else

namespace {

// ---------------------------------------------------------------------
// FD1/FD2 shape: unit, and Get/Put operation payloads. Each operation
// names its own response type (the GADT index dissolved into the
// per-alternative `response` member).
// ---------------------------------------------------------------------

struct unit {
    friend constexpr auto operator==(const unit &, const unit &)
        -> bool = default;
};
static_assert(unit{} == unit{}, "FD2 shape: unit is trivially comparable");

struct Get {
    int key;
    using response = std::optional<int>;
};

struct Put {
    int key;
    int value;
    using response = unit;
};

// ---------------------------------------------------------------------
// FD3 shape: impure_node<Op, X> — the Coyoneda node. Op's payload plus a
// one-shot, &&-qualified continuation from the handler's response to
// Box<X> (Box is the completeness firewall: X, a Free<..., A>, is still
// incomplete at this point).
// ---------------------------------------------------------------------

template <class Op, class X>
struct impure_node {
    Op op;
    std::move_only_function<smd::fixpoint::Box<X>(typename Op::response) &&> k;
};

// FD3's signature<Ops...>::type<X> shape, hand-rolled for exactly
// Get/Put — the nested member template, not an alias template (P0522
// portability, per FD3's rationale).
struct KVSig {
    template <class X>
    struct type {
        std::variant<impure_node<Get, X>, impure_node<Put, X>> node;
    };
};

using KVFree = smd::fixpoint::Free<KVSig::template type, int>;
using KVLayer = KVSig::template type<KVFree>;

} // namespace

// =======================================================================
// Probe: the FD3 shape compiles (FD9 item 1) — construct one suspended
// value by hand, resume it by hand.
// =======================================================================

TEST_CASE("freer baseline [FD3][FD9]: hand-built impure_node/Free "
          "constructs and resumes") {
    // Identity-into-Pure continuation, FD7's send() shape inlined: the
    // continuation's only job is to lift the handler's response into a
    // Pure leaf. The response type of Get is std::optional<int> while the
    // Free's value type A is fixed at int (per the step: "form
    // Free<KVSig::template type, int>"), so this forwards the response's
    // value rather than being a byte-for-byte identity — same *shape* as
    // FD7 (a single lambda, no other capture, wraps r into pure_free),
    // not a type-for-type match.
    impure_node<Get, KVFree> get_node{
        Get{42}, [](std::optional<int> r) -> smd::fixpoint::Box<KVFree> {
            return smd::fixpoint::make_box<KVFree>(
                smd::fixpoint::pure_free<KVSig::template type>(r.value_or(-1)));
        }};

    // std::variant's converting constructor gets confused by variants of
    // move-only alternatives here (impure_node holds a
    // move_only_function); std::in_place_type sidesteps overload
    // resolution entirely — the pitfall the step file calls out.
    KVLayer layer{
        std::variant<impure_node<Get, KVFree>, impure_node<Put, KVFree>>{
            std::in_place_type<impure_node<Get, KVFree>>, std::move(get_node)}};

    KVFree suspended =
        smd::fixpoint::roll_free<KVSig::template type>(std::move(layer));
    REQUIRE_FALSE(smd::fixpoint::is_pure(suspended));

    auto &suspended_layer = std::get<KVLayer>(suspended.node);
    auto &suspended_get =
        std::get<impure_node<Get, KVFree>>(suspended_layer.node);

    // Resume by hand: std::move(k)(response).
    smd::fixpoint::Box<KVFree> resumed =
        std::move(suspended_get.k)(std::optional<int>{7});
    CHECK(smd::fixpoint::is_pure(*resumed));
    CHECK(std::get<int>((*resumed).node) == 7);
}

// =======================================================================
// Probe: comparison never instantiates (FD9 item 2 / FD5).
//
// Discovery (verbatim result, not the naively-expected one — see the
// handoff): the *layer* type (no operator== written or defaulted at all)
// is correctly not std::equality_comparable. But KVFree itself IS
// std::equality_comparable under both gcc-16 and clang-23: Free's
// hand-written, non-template friend operator== has a signature that is
// visible from class-template instantiation regardless of whether its
// *body* would compile — concept checking only needs the expression
// `a == b` to resolve to something convertible to bool, which it does
// without ever instantiating the friend's body (the same laziness FD3
// credits for avoiding the eager defaulted-comparison cascade). Actually
// *calling* `==` on a KVFree is confirmed (scratch probe, not committed,
// see handoff) to hard-error inside the friend body, because
// std::variant<impure_node<Get,X>, impure_node<Put,X>> has no operator==
// (impure_node is never given one). So "comparison never instantiates"
// holds in the sense that matters operationally — the TU below compiles
// clean without ever *calling* ==, and no eager deleted-ness check fires
// — but std::equality_comparable is not the right predicate to observe
// that; it answers "is `==` well-formed to write", not "would it compile
// if called".
static_assert(!std::equality_comparable<KVLayer>,
              "FD9/FD5: the raw Coyoneda layer (no operator== declared) "
              "is correctly not equality_comparable");
static_assert(std::equality_comparable<KVFree>,
              "FD9/FD5 discovery: Free's hand-written friend operator== "
              "is declaration-visible regardless of body instantiability, "
              "so std::equality_comparable<KVFree> reports true on both "
              "gcc-16 and clang-23 even though calling == would hard-error "
              "(see comment above) — recorded verbatim per the step's "
              "instruction not to fight a divergent-from-expected result.");

TEST_CASE("freer baseline [FD5][FD9]: no == is ever named on KVFree") {
    // This TEST_CASE exists only to anchor the comment/static_asserts
    // above under a name in the Catch2 test list; the probe itself is
    // the two static_asserts (they already ran by the time this file
    // compiled). Never write `suspended == suspended` for a KVFree here.
    SUCCEED("static_assert probes above already ran at compile time");
}

// =======================================================================
// Probe (a): member-template template-template deduction (FD3 windfall /
// D-A input, FD9 item 3a / FD11 background). Does the existing
// functor_typeclass<Free<F,A>>/monad_typeclass<Free<F,A>> partial
// specialization select for Free<KVSig::template type, int>, i.e. does
// F deduce against a *member* class template used as a template-template
// argument?
// =======================================================================

static_assert(
    !std::is_same_v<std::remove_cvref_t<
                        decltype(smd::typeclass::functor_typeclass<KVFree>)>,
                    std::false_type>,
    "D-A probe (a): functor_typeclass<Free<KVSig::type, int>> selects the "
    "Free partial specialization (F deduces against the member class "
    "template KVSig::type), not the unregistered std::false_type sentinel");
static_assert(
    !std::is_same_v<
        std::remove_cvref_t<decltype(smd::typeclass::monad_typeclass<KVFree>)>,
        std::false_type>,
    "D-A probe (a): monad_typeclass<Free<KVSig::type, int>> selects the "
    "Free partial specialization the same way");

// =======================================================================
// Probe (b): constrained variable-template partial specialization (D-A
// input, FD9 item 3b / FD11 option (a)). A self-contained mock registry
// keyed on a concept over a nested typedef a local layer exposes —
// exactly the shape FD11 option (a) proposes for
// functor_typeclass<Sig::type<X>>, tried here against a throwaway
// registry so a failure can't be confused with the real functor_typeclass
// machinery.
// =======================================================================

namespace probe_b {

// A local "layer" shaped like a closed-world Coyoneda layer, exposing the
// nested typedef a `coyo_layer`-style concept would key on.
template <class X>
struct local_layer {
    using recursive_position = X; // the nested typedef being probed for
    std::variant<impure_node<Get, X>, impure_node<Put, X>> node;
};

template <class T>
concept probe_concept = requires { typename T::recursive_position; };

struct marker_t {};

// Primary: unregistered sentinel, same shape as functor_typeclass's own
// primary in smd/typeclass/functor.hpp.
template <class T>
inline constexpr auto probe_tc = std::false_type{};

// Constrained partial specialization — FD11 option (a)'s candidate shape.
template <class T>
    requires probe_concept<T>
inline constexpr auto probe_tc<T> = marker_t{};

static_assert(
    std::is_same_v<std::remove_cvref_t<decltype(probe_tc<local_layer<int>>)>,
                   marker_t>,
    "D-A probe (b): the constrained partial specialization is selected "
    "for a type exposing the keyed nested typedef");
static_assert(std::is_same_v<std::remove_cvref_t<decltype(probe_tc<int>)>,
                             std::false_type>,
              "D-A probe (b): the unconstrained primary is selected for a type "
              "that does not expose the nested typedef (int)");

} // namespace probe_b

// =======================================================================
// Probe (c): the &&-qualified move_only_function corner (D-B input, FD9
// item 3c / FD12). Exercised already by the construct/resume TEST_CASE
// above; here: move-only propagation through the self-embedding variant,
// and a runtime move + resume.
// =======================================================================

static_assert(std::movable<KVFree>,
              "D-B probe (c): KVFree is movable (the self-embedding "
              "variant-of-move_only_function shape propagates moves)");
static_assert(!std::copyable<KVFree>,
              "D-B probe (c): KVFree is NOT copyable — move_only_function "
              "inside impure_node makes the whole stack move-only, "
              "confirming the && call-signature's one-shot-ness is a "
              "type property, not just a runtime discipline");

TEST_CASE("freer baseline [FD12][FD9]: KVFree move-construction propagates "
          "and the moved-to value still resumes") {
    impure_node<Put, KVFree> put_node{
        Put{1, 2}, [](unit) -> smd::fixpoint::Box<KVFree> {
            return smd::fixpoint::make_box<KVFree>(
                smd::fixpoint::pure_free<KVSig::template type>(99));
        }};
    KVLayer layer{
        std::variant<impure_node<Get, KVFree>, impure_node<Put, KVFree>>{
            std::in_place_type<impure_node<Put, KVFree>>, std::move(put_node)}};
    KVFree original =
        smd::fixpoint::roll_free<KVSig::template type>(std::move(layer));

    KVFree moved_to = std::move(original); // runtime move
    REQUIRE_FALSE(smd::fixpoint::is_pure(moved_to));

    auto &moved_layer = std::get<KVLayer>(moved_to.node);
    auto &moved_put = std::get<impure_node<Put, KVFree>>(moved_layer.node);
    smd::fixpoint::Box<KVFree> resumed = std::move(moved_put.k)(unit{});
    CHECK(smd::fixpoint::is_pure(*resumed));
    CHECK(std::get<int>((*resumed).node) == 99);
}

#endif // __cpp_lib_move_only_function
