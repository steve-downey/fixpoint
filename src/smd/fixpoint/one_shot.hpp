// src/smd/fixpoint/one_shot.hpp                                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_ONE_SHOT
#define INCLUDED_SMD_FIXPOINT_ONE_SHOT

// one_shot<R(Args...)>: a small type-erased, move-only, call-consuming
// callable box (design doc FD12, decision D-B). Chosen over
// std::move_only_function because, at LLVM 23, libc++ does not declare
// std::move_only_function at all -- not merely untagged by the feature-test
// macro (ops/freer/handoffs/00-ground.handoff.md, DEV-S00-1) -- and libc++
// is a row the CI matrix claims. FD12's decision rule therefore takes a
// bespoke type uniformly, on every toolchain, rather than a macro-gated
// fast path plus a bespoke fallback (strictly more surface than just the
// fallback, which has to be written regardless).
//
// It mirrors Box's completeness-firewall role (box.hpp): the stored target
// sits behind a type-erased pointer, so `one_shot<Box<X>(...)>` names a
// complete type even while X is still incomplete at the point it is used
// (impure_node<Op, X>, freer.hpp).
//
// Call-consumes, not merely move-only: operator()(Args...) && invokes the
// target and destroys it as part of the same call. The type only offers an
// rvalue-qualified call operator (the && makes one-shot-ness a type
// property, exactly as FD3 does for the continuation's own && qualifier),
// so a second call requires std::move-ing an already-emptied object --
// caught by an assertion rather than silently dereferencing a null or
// moved-from target.

#include <cassert>
#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

namespace smd::fixpoint {

/** one_shot<R(Args...)>: see the file comment. Only the function-type
 * partial specialization below is usable; the primary template is
 * declared but never defined.
 */
template <class Sig>
class one_shot;

template <class R, class... Args>
class one_shot<R(Args...)> {
    struct concept_t {
        virtual auto invoke(Args... args) -> R = 0;
        virtual ~concept_t() = default;
    };

    template <class F>
    struct model_t final : concept_t {
        F f;

        explicit model_t(F fn) : f(std::move(fn)) {}

        auto invoke(Args... args) -> R override {
            return std::invoke(f, std::forward<Args>(args)...);
        }
    };

    concept_t *target_ = nullptr;

  public:
    one_shot() = default;

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, one_shot>) &&
                std::invocable<F &, Args...> &&
                std::convertible_to<std::invoke_result_t<F &, Args...>, R>
    explicit one_shot(F f)
        : target_(new model_t<std::remove_cvref_t<F>>(std::move(f))) {}

    one_shot(const one_shot &) = delete;
    auto operator=(const one_shot &) -> one_shot & = delete;

    one_shot(one_shot &&other) noexcept
        : target_(std::exchange(other.target_, nullptr)) {}

    auto operator=(one_shot &&other) noexcept -> one_shot & {
        if (this != &other) {
            delete target_;
            target_ = std::exchange(other.target_, nullptr);
        }
        return *this;
    }

    ~one_shot() { delete target_; }

    /** Invoke the stored target and consume it -- the target is destroyed
     * as part of this call whether or not it throws. Precondition: this
     * one_shot currently holds a target (a default-constructed, moved-from,
     * or already-called one_shot does not) -- asserted, so a violation is a
     * hard, diagnosable error rather than a UB-adjacent null/moved-from
     * call.
     */
    auto operator()(Args... args) && -> R {
        assert(target_ != nullptr &&
               "one_shot: call on an empty target (default-constructed, "
               "moved-from, or already consumed by a previous call)");
        concept_t *owned = std::exchange(target_, nullptr);
        struct scope_delete {
            concept_t *p;

            ~scope_delete() { delete p; }
        } cleanup{owned};
        return owned->invoke(std::forward<Args>(args)...);
    }

    /** True iff this one_shot currently holds a target that has not yet
     * been consumed by a call (or moved away).
     */
    [[nodiscard]] explicit operator bool() const noexcept {
        return target_ != nullptr;
    }
};

} // namespace smd::fixpoint

#endif
