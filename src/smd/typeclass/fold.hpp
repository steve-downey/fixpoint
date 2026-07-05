// src/smd/typeclass/fold.hpp                                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_TYPECLASS_FOLD
#define INCLUDED_SMD_TYPECLASS_FOLD

#include <smd/typeclass/detail/typeclass_base.hpp>
#include <smd/typeclass/monoid.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace smd::typeclass::detail {

// Identity function for fold composition - no type erasure needed
template <class STATE>
struct IdentityFoldFunc {
    constexpr auto operator()(STATE s) const -> STATE { return s; }
};

// Compose two fold functions without type erasure
template <class F1, class F2>
struct ComposedFoldFunc {
    F1 d_f1;
    F2 d_f2;

    template <class STATE>
    auto operator()(STATE s) const -> STATE {
        return d_f2(d_f1(std::move(s)));
    }
};

// Left fold composition (same order as f1 then f2)
template <class STATE>
struct LeftFoldProgram {
    std::function<STATE(STATE)> d_run;

    auto operator()(STATE state) const -> STATE {
        return d_run(std::move(state));
    }
};

// Template-based version that avoids type erasure - used internally
template <class F>
struct LeftFoldProgramT {
    F d_run;

    template <class STATE>
    auto operator()(STATE state) const -> STATE {
        return d_run(std::move(state));
    }
};

template <class STATE>
struct RightFoldProgram {
    std::function<STATE(STATE)> d_run;

    auto operator()(STATE state) const -> STATE {
        return d_run(std::move(state));
    }
};

// Template-based version that avoids type erasure - used internally
template <class F>
struct RightFoldProgramT {
    F d_run;

    template <class STATE>
    auto operator()(STATE state) const -> STATE {
        return d_run(std::move(state));
    }
};

struct Any {
    bool d_value;
};

struct All {
    bool d_value;
};

template <class VALUE_TYPE>
struct First {
    std::optional<VALUE_TYPE> d_value;
};

} // namespace smd::typeclass::detail

namespace smd::typeclass {

template <class STATE>
struct Monoid<detail::LeftFoldProgram<STATE>> {
    auto identity() const -> detail::LeftFoldProgram<STATE> {
        return detail::LeftFoldProgram<STATE>{[](STATE s) { return s; }};
    }

    auto combine(const detail::LeftFoldProgram<STATE> &lhs,
                 const detail::LeftFoldProgram<STATE> &rhs) const
        -> detail::LeftFoldProgram<STATE> {
        return detail::LeftFoldProgram<STATE>{
            [lhs, rhs](STATE s) { return rhs(lhs(std::move(s))); }};
    }
};

// Template specialization for LeftFoldProgramT - avoids type erasure
template <class F>
struct Monoid<detail::LeftFoldProgramT<F>> {
    auto identity() const
        -> detail::LeftFoldProgramT<detail::IdentityFoldFunc<int>> {
        return detail::LeftFoldProgramT<detail::IdentityFoldFunc<int>>{
            detail::IdentityFoldFunc<int>{}};
    }

    template <class G>
    auto combine(const detail::LeftFoldProgramT<F> &lhs,
                 const detail::LeftFoldProgramT<G> &rhs) const
        -> detail::LeftFoldProgramT<detail::ComposedFoldFunc<F, G>> {
        return detail::LeftFoldProgramT<detail::ComposedFoldFunc<F, G>>{
            detail::ComposedFoldFunc<F, G>{lhs.d_run, rhs.d_run}};
    }
};

template <class STATE>
struct Monoid<detail::RightFoldProgram<STATE>> {
    auto identity() const -> detail::RightFoldProgram<STATE> {
        return detail::RightFoldProgram<STATE>{[](STATE s) { return s; }};
    }

    auto combine(const detail::RightFoldProgram<STATE> &lhs,
                 const detail::RightFoldProgram<STATE> &rhs) const
        -> detail::RightFoldProgram<STATE> {
        return detail::RightFoldProgram<STATE>{
            [lhs, rhs](STATE s) { return lhs(rhs(std::move(s))); }};
    }
};

// Template specialization for RightFoldProgramT - avoids type erasure
template <class F>
struct Monoid<detail::RightFoldProgramT<F>> {
    auto identity() const
        -> detail::RightFoldProgramT<detail::IdentityFoldFunc<int>> {
        return detail::RightFoldProgramT<detail::IdentityFoldFunc<int>>{
            detail::IdentityFoldFunc<int>{}};
    }

    template <class G>
    auto combine(const detail::RightFoldProgramT<F> &lhs,
                 const detail::RightFoldProgramT<G> &rhs) const
        -> detail::RightFoldProgramT<detail::ComposedFoldFunc<F, G>> {
        return detail::RightFoldProgramT<detail::ComposedFoldFunc<F, G>>{
            detail::ComposedFoldFunc<F, G>{lhs.d_run, rhs.d_run}};
    }
};

template <>
struct Monoid<detail::Any> {
    constexpr auto identity() const -> detail::Any { return {false}; }

    constexpr auto combine(detail::Any lhs, detail::Any rhs) const
        -> detail::Any {
        return {lhs.d_value || rhs.d_value};
    }
};

template <>
struct Monoid<detail::All> {
    constexpr auto identity() const -> detail::All { return {true}; }

    constexpr auto combine(detail::All lhs, detail::All rhs) const
        -> detail::All {
        return {lhs.d_value && rhs.d_value};
    }
};

template <class VALUE_TYPE>
struct Monoid<detail::First<VALUE_TYPE>> {
    auto identity() const -> detail::First<VALUE_TYPE> { return {{}}; }

    auto combine(const detail::First<VALUE_TYPE> &lhs,
                 const detail::First<VALUE_TYPE> &rhs) const
        -> detail::First<VALUE_TYPE> {
        if (lhs.d_value) {
            return lhs;
        }
        return rhs;
    }
};

// Foldable pattern invariants:
// - Generic entry point is fold_map(F, T) via foldable_typeclass<T>.
// - Instances provide an object with fold_map(F, T) and specialize
//   foldable_typeclass<Concrete>.
// - Derived APIs (length, fold_left, fold_right, combine_all, any_of, all_of,
//   empty, to_vector, find_first) live on the same looked-up object.
// - Traversal order is instance-defined but must be coherent per instance.

/** CRTP base for Foldable instances.
 * `Impl` must provide either `fold_map(f, container)` or `fold_right` +
 * `element_type`; all other operations are derived from whichever is the
 * primitive.
 */
template <class Impl>
struct Foldable : protected Impl {
    static_assert(
        !std::is_same_v<Impl, std::false_type>,
        "No foldable_typeclass<T> specialization found. "
        "Specialize smd::typeclass::foldable_typeclass<T> for your "
        "type T and provide fold_map(F, T) or fold_right(T, STATE, F) "
        "+ element_type.");
    // Alternate-core: Impl provides either fold_map or fold_right as primitive.
    // The Map class's using-declaration selects which; the base derives the
    // other. Haskell equivalent: {-# MINIMAL foldMap | foldr #-}

    // Derived fold_map from fold_right. Active when a fold_right-primitive
    // Impl's using-declaration shadows the base's derived fold_right with the
    // real one. Requires element_type to deduce the monoid result type. foldMap
    // f = foldr (\x acc -> f x <> acc) mempty
    template <class F, class T>
    auto fold_map(this auto &&self, F &&function, T &&value)
        requires requires { typename Impl::element_type; }
    {
        using Result = remove_cvref_t<
            std::invoke_result_t<F, const typename Impl::element_type &>>;
        return self.fold_right(
            std::forward<T>(value), monoid_identity<Result>(),
            [&function](const auto &elem, Result acc) {
                return monoid_combine(std::invoke(function, elem),
                                      std::move(acc));
            });
    }

    /** Returns the number of elements in the foldable container. */
    template <class T>
    auto length(this auto &&self, T &&value) -> std::size_t {
        const auto count = self.fold_map([](const auto &) { return Count{1}; },
                                         std::forward<T>(value));
        return count.d_value;
    }

    /** Left-associative fold: applies `function(state, element)` for each
     * element in traversal order, starting from `initial_state`.
     */
    template <class T, class STATE, class F>
    auto fold_left(this auto &&self, T &&value, STATE initial_state,
                   F &&function) {
        using StateType = remove_cvref_t<STATE>;
        auto step = std::forward<F>(function);

        const auto program = self.fold_map(
            [&step](const auto &x) {
                using ValueType = remove_cvref_t<decltype(x)>;
                return detail::LeftFoldProgram<StateType>{
                    [x_copy = ValueType(x), &step](StateType s) {
                        return std::invoke(step, std::move(s), x_copy);
                    }};
            },
            std::forward<T>(value));

        return program(StateType(std::move(initial_state)));
    }

    /** Right-associative fold: applies `function(element, state)` for each
     * element in reverse traversal order, starting from `initial_state`.
     */
    template <class T, class STATE, class F>
    auto fold_right(this auto &&self, T &&value, STATE initial_state,
                    F &&function) {
        using StateType = remove_cvref_t<STATE>;
        auto step = std::forward<F>(function);

        const auto program = self.fold_map(
            [&step](const auto &x) {
                using ValueType = remove_cvref_t<decltype(x)>;
                return detail::RightFoldProgram<StateType>{
                    [x_copy = ValueType(x), &step](StateType s) {
                        return std::invoke(step, x_copy, std::move(s));
                    }};
            },
            std::forward<T>(value));

        return program(StateType(std::move(initial_state)));
    }

    /** Combines all elements using the Monoid of the element type
     * (requires elements themselves to be Monoid values).
     */
    template <class T>
    auto combine_all(this auto &&self, T &&value) {
        return self.fold_map([](const auto &x) { return x; },
                             std::forward<T>(value));
    }

    /** Alias for `combine_all`. */
    template <class T>
    auto fold(this auto &&self, T &&value) {
        return self.combine_all(std::forward<T>(value));
    }

    /** Returns `true` if any element satisfies `predicate`. */
    template <class T, class PREDICATE>
    auto any_of(this auto &&self, T &&value, PREDICATE &&predicate) -> bool {
        const auto result = self.fold_map(
            [&predicate](const auto &x) {
                return detail::Any{std::invoke(predicate, x)};
            },
            std::forward<T>(value));

        return result.d_value;
    }

    /** Returns `true` if all elements satisfy `predicate`. */
    template <class T, class PREDICATE>
    auto all_of(this auto &&self, T &&value, PREDICATE &&predicate) -> bool {
        const auto result = self.fold_map(
            [&predicate](const auto &x) {
                return detail::All{std::invoke(predicate, x)};
            },
            std::forward<T>(value));

        return result.d_value;
    }

    /** Returns `true` if the container holds no elements. */
    template <class T>
    auto empty(this auto &&self, T &&value) -> bool {
        return !self.any_of(std::forward<T>(value),
                            [](const auto &) { return true; });
    }

    /** Collects all elements into a `std::vector` in traversal order. */
    template <class T>
    auto to_vector(this auto &&self, T &&value) {
        return self.fold_map(
            [](const auto &x) {
                using ValueType = remove_cvref_t<decltype(x)>;
                return std::vector<ValueType>{x};
            },
            std::forward<T>(value));
    }

    /** Returns the first element satisfying `predicate`, or an empty optional.
     */
    template <class T, class PREDICATE>
    auto find_first(this auto &&self, T &&value, PREDICATE &&predicate) {
        const auto result = self.fold_map(
            [&predicate](const auto &x) {
                using X = remove_cvref_t<decltype(x)>;
                if (std::invoke(predicate, x)) {
                    return detail::First<X>{{x}};
                }
                return detail::First<X>{{}};
            },
            std::forward<T>(value));

        return result.d_value;
    }
};

/** Typeclass lookup variable for Foldable; specialize for each container type.
 */
template <class T>
inline constexpr auto foldable_typeclass = std::false_type{};

} // namespace smd::typeclass

#endif // INCLUDED_SMD_TYPECLASS_FOLD
