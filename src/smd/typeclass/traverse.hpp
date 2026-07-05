// src/smd/typeclass/traverse.hpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_TYPECLASS_TRAVERSE
#define INCLUDED_SMD_TYPECLASS_TRAVERSE

#include <smd/typeclass/apply.hpp>
#include <smd/typeclass/detail/typeclass_base.hpp>
#include <smd/typeclass/functor.hpp>

#include <type_traits>
#include <utility>

namespace smd::typeclass {

// Traversable pattern invariants:
// - Instances are single lookup objects that provide traverse(F, T).
// - transpose is a derived object operation implemented from
//   traverse(identity).
// - Dispatch happens through a provided object or
//   traversable_typeclass<Concrete>.
// - Traversal must preserve container shape while transposing structure and
//   context.

/** CRTP base for Traversable instances.
 * `Impl` must provide `traverse(applicative, f, container)` and declare
 * `element_type`. All other operations (`transpose`, `for_each`,
 * `traverse_with`, `transpose_with`) are derived.
 */
template <class Impl>
struct Traversable : protected Impl {
    static_assert(!std::is_same_v<Impl, std::false_type>,
                  "No traversable_typeclass<T> specialization found. "
                  "Specialize smd::typeclass::traversable_typeclass<T> for "
                  "your type T, "
                  "provide traverse(applicative, F, T), and declare 'using "
                  "element_type = T;'.");
    static_assert(
        requires { typename Impl::element_type; },
        "Traversable Impl must declare 'using element_type = T;' "
        "so that transpose() and traverse_with() can deduce the element type.");
    // Alternate-core: Impl::traverse is the primitive; transpose is derived
    // from it. A transpose-primitive Impl would shadow transpose instead.
    using Impl::traverse;
    using element_type = typename Impl::element_type;

    /** Applies `function` to each element and transposes the resulting
     * effects; the applicative is inferred from the return type of `function`.
     */
    template <class T, class F>
    auto for_each(this auto &&self, T &&value, F &&function) {
        using Context =
            remove_cvref_t<std::invoke_result_t<F, const element_type &>>;
        const auto &applicative = applicative_typeclass<Context>;
        return self.traverse(applicative, std::forward<F>(function),
                             std::forward<T>(value));
    }

    /** Transposes a structure of effectful values into one outer effect
     * containing the structure. The element type must itself be an
     * applicative context.
     */
    template <class T>
    auto transpose(this auto &&self, T &&value) {
        using Context = element_type;
        const auto &applicative = applicative_typeclass<Context>;
        return self.traverse(
            applicative, [](auto &&x) { return std::forward<decltype(x)>(x); },
            std::forward<T>(value));
    }

    /** Traverses using a different traversable instance; applicative is
     * inferred from the return type of `function`.
     */
    template <class TRAVERSABLE_MAP, class T, class F>
    auto traverse_with(this auto &&, const TRAVERSABLE_MAP &traversable_map,
                       F &&function, T &&value) {
        using Context = remove_cvref_t<std::invoke_result_t<
            F, const typename remove_cvref_t<TRAVERSABLE_MAP>::element_type &>>;
        const auto &applicative = applicative_typeclass<Context>;
        return traversable_map.traverse(applicative, std::forward<F>(function),
                                        std::forward<T>(value));
    }

    /** Traverses using explicit traversable and applicative instances. */
    template <class TRAVERSABLE_MAP, class APPLICATIVE_MAP, class T, class F>
    auto traverse_with(this auto &&, const TRAVERSABLE_MAP &traversable_map,
                       const APPLICATIVE_MAP &applicative_map, F &&function,
                       T &&value) {
        return traversable_map.traverse(
            applicative_map, std::forward<F>(function), std::forward<T>(value));
    }

    /** Transposes using a different traversable instance; applicative is
     * inferred from the container's element type.
     */
    template <class TRAVERSABLE_MAP, class T>
    auto transpose_with(this auto &&self,
                        const TRAVERSABLE_MAP &traversable_map, T &&value) {
        return self.traverse_with(
            traversable_map,
            [](auto &&x) { return std::forward<decltype(x)>(x); },
            std::forward<T>(value));
    }
};

/** Typeclass lookup variable for Traversable; specialize for each container
 * type. */
template <class T>
inline constexpr auto traversable_typeclass = std::false_type{};

/** @brief Maps `function` over `value`, traverses effects left-to-right,
 *         and preserves container shape.
 *
 * @param function  A callable returning an applicative effect for each element.
 * @param value     The traversable container to process.
 * @return          The container shape transposed into the applicative effect.
 */
template <class F, class T>
auto traverse(F &&function, T &&value) {
    const auto &map = traversable_typeclass<remove_cvref_t<T>>;
    using element_type = typename remove_cvref_t<decltype(map)>::element_type;
    using Context =
        remove_cvref_t<std::invoke_result_t<F, const element_type &>>;
    const auto &applicative = applicative_typeclass<Context>;
    return map.traverse(applicative, std::forward<F>(function),
                        std::forward<T>(value));
}

} // namespace smd::typeclass

#endif // INCLUDED_SMD_TYPECLASS_TRAVERSE
