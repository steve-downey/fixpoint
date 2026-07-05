// src/smd/fixpoint/box.hpp                                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_BOX
#define INCLUDED_SMD_FIXPOINT_BOX

#include <utility>

namespace smd::fixpoint {

/** Constexpr-capable owning pointer with deep-copy value semantics.
 * Nullable (default ptr=nullptr), so it works in static_vector storage
 * without wasteful default-allocations. Uses raw new/delete, which are
 * constexpr in C++20+ for transient allocations.
 *
 * This is a workaround until consteval allocation lands in a future
 * standard; std::indirect has the right semantics but its explicit
 * default constructor blocks use in aggregate-initialised containers.
 *
 * @tparam A the wrapped type (typically a recursive Fix instantiation)
 */
// a1f81cb9-ebb3-4872-9549-03ca140c61b2
template <typename A>
struct Box {
    A *ptr = nullptr;

    constexpr Box() = default;

    constexpr explicit Box(A *p) : ptr(p) {}

    constexpr Box(Box const &other)
        : ptr(other.ptr ? new A(*other.ptr) : nullptr) {}

    constexpr auto operator=(Box const &other) -> Box & {
        if (this != &other) {
            delete ptr;
            ptr = other.ptr ? new A(*other.ptr) : nullptr;
        }
        return *this;
    }

    constexpr Box(Box &&other) noexcept
        : ptr(std::exchange(other.ptr, nullptr)) {}

    constexpr auto operator=(Box &&other) noexcept -> Box & {
        if (this != &other) {
            delete ptr;
            ptr = std::exchange(other.ptr, nullptr);
        }
        return *this;
    }

    constexpr ~Box() { delete ptr; }

    constexpr auto operator*() const -> A & { return *ptr; }
    constexpr auto operator->() const -> A * { return ptr; }

    friend constexpr auto operator==(Box const &lhs, Box const &rhs) -> bool {
        if (lhs.ptr == rhs.ptr)
            return true;
        if (!lhs.ptr || !rhs.ptr)
            return false;
        return *lhs.ptr == *rhs.ptr;
    }
};
// a1f81cb9-ebb3-4872-9549-03ca140c61b2 end

// 45a64b08-8217-4baf-b19b-146434de1e44
/** Construct a Box<A>, forwarding @p args to A's constructor. */
template <typename A, typename... Args>
constexpr auto make_box(Args &&...args) -> Box<A> {
    return Box<A>(new A(std::forward<Args>(args)...));
}
// 45a64b08-8217-4baf-b19b-146434de1e44 end

} // namespace smd::fixpoint

#endif
