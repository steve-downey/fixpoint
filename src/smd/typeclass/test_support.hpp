// src/smd/typeclass/test_support.hpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_TYPECLASS_TEST_TEST_SUPPORT
#define INCLUDED_SMD_TYPECLASS_TEST_TEST_SUPPORT

#include <smd/typeclass/apply.hpp>
#include <smd/typeclass/fold.hpp>
#include <smd/typeclass/traverse.hpp>

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace smd::typeclass::test {

/** Verify the Applicative identity law: `pure(id) <*> v == v`. */
template <class CONTEXT>
auto check_applicative_identity_law(const CONTEXT &value) -> bool {
    const auto &applicative = applicative_typeclass<remove_cvref_t<CONTEXT>>;
    auto result = applicative.ap(
        applicative.pure([](const auto &x) { return x; }), value);
    return result == value;
}

/** Verify the Applicative homomorphism law: `pure(f) <*> pure(x) == pure(f x)`.
 */
template <class CONTEXT, class FUNCTION, class VALUE>
auto check_applicative_homomorphism_law(const FUNCTION &function,
                                        const VALUE &value) -> bool {
    const auto &applicative = applicative_typeclass<remove_cvref_t<CONTEXT>>;
    auto left =
        applicative.ap(applicative.pure(function), applicative.pure(value));
    auto right = applicative.pure(std::invoke(function, value));
    return left == right;
}

/** Minimal single-element applicative context used in law tests. */
template <class VALUE_TYPE>
struct Identity {
    using value_type = VALUE_TYPE;

    VALUE_TYPE value;

    friend auto operator==(const Identity &, const Identity &)
        -> bool = default;
};

/** Ordered multi-element foldable context backed by `std::vector`. */
template <class VALUE_TYPE>
struct Sequence {
    using value_type = VALUE_TYPE;

    std::vector<VALUE_TYPE> values;

    friend auto operator==(const Sequence &, const Sequence &)
        -> bool = default;
};

} // namespace smd::typeclass::test

namespace smd::typeclass {

/** Applicative implementation for Identity<V>: pure wraps, apply unwraps and
 * invokes. */
template <class VALUE_TYPE>
struct TestIdentityApplicativeImpl {
    template <class VALUE>
    auto pure(this auto &&, VALUE &&value) {
        return test::Identity<remove_cvref_t<VALUE>>{
            std::forward<VALUE>(value)};
    }

    template <class FUNCTION_IN_CONTEXT, class ARGUMENT_IN_CONTEXT>
    auto apply(this auto &&, const FUNCTION_IN_CONTEXT &function,
               const ARGUMENT_IN_CONTEXT &argument) {
        using Result = std::invoke_result_t<
            const typename remove_cvref_t<FUNCTION_IN_CONTEXT>::value_type &,
            const typename remove_cvref_t<ARGUMENT_IN_CONTEXT>::value_type &>;

        return test::Identity<remove_cvref_t<Result>>{
            std::invoke(function.value, argument.value)};
    }
};

template <class VALUE_TYPE>
struct TestIdentityApplicativeMap
    : Applicative<TestIdentityApplicativeImpl<VALUE_TYPE>> {
    using TestIdentityApplicativeImpl<VALUE_TYPE>::apply;
    using TestIdentityApplicativeImpl<VALUE_TYPE>::pure;
};

template <class VALUE_TYPE>
inline constexpr auto applicative_typeclass<test::Identity<VALUE_TYPE>> =
    TestIdentityApplicativeMap<VALUE_TYPE>{};

/** Foldable implementation for Sequence<V>: fold_map walks values
 * left-to-right. */
template <class VALUE_TYPE>
struct TestSequenceFoldableImpl {
    template <class FUNCTION>
    auto fold_map(this auto &&, FUNCTION &&function,
                  const test::Sequence<VALUE_TYPE> &sequence) {
        using Result =
            remove_cvref_t<std::invoke_result_t<FUNCTION, const VALUE_TYPE &>>;
        return std::ranges::fold_left(
            sequence.values, monoid_identity<Result>(),
            [&](Result acc, const VALUE_TYPE &value) {
                return monoid_combine(std::move(acc),
                                      std::invoke(function, value));
            });
    }
};

template <class VALUE_TYPE>
struct TestSequenceFoldableMap
    : Foldable<TestSequenceFoldableImpl<VALUE_TYPE>> {
    using TestSequenceFoldableImpl<VALUE_TYPE>::fold_map;
};

template <class VALUE_TYPE>
inline constexpr auto foldable_typeclass<test::Sequence<VALUE_TYPE>> =
    TestSequenceFoldableMap<VALUE_TYPE>{};

/** Traversable implementation for Identity<V>. */
template <class VALUE_TYPE>
struct TestIdentityTraversableImpl {
    using element_type = VALUE_TYPE;

    template <class APPLICATIVE, class FUNCTION>
    auto traverse(this auto &&, const APPLICATIVE &applicative,
                  FUNCTION &&function,
                  const test::Identity<VALUE_TYPE> &identity) {
        return applicative.invoke(
            [](auto &&value) {
                using U = remove_cvref_t<decltype(value)>;
                return test::Identity<U>{std::forward<decltype(value)>(value)};
            },
            std::invoke(std::forward<FUNCTION>(function), identity.value));
    }
};

template <class VALUE_TYPE>
struct TestIdentityTraversableMap
    : Traversable<TestIdentityTraversableImpl<VALUE_TYPE>> {
    using TestIdentityTraversableImpl<VALUE_TYPE>::traverse;
};

template <class VALUE_TYPE>
inline constexpr auto traversable_typeclass<test::Identity<VALUE_TYPE>> =
    TestIdentityTraversableMap<VALUE_TYPE>{};

} // namespace smd::typeclass

#endif // INCLUDED_SMD_TYPECLASS_TEST_TEST_SUPPORT
