// src/smd/fixpoint/type_name.hpp                                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_TYPE_NAME
#define INCLUDED_SMD_FIXPOINT_TYPE_NAME

// Compile-time type names without RTTI: type_name<T>() carves T's spelling
// out of the enclosing function's signature macro, and short_type_name()
// reduces that spelling to the leaf identifier a human wants in a diagram
// label ("beman::execution::detail::then_t<...>" -> "then"). The full
// spelling is compiler-specific (gcc, clang, and MSVC each format the
// signature differently); the short form of a non-template library type is
// stable across all three, which is what tests and labels should assert on.

#include <string_view>

namespace smd::fixpoint {

// 7fe5093e-c875-4524-9da0-ee7767f1733f
/** The compiler's spelling of type @p T, at compile time. */
template <typename T>
constexpr auto type_name() -> std::string_view {
#if defined(__clang__) || defined(__GNUC__)
    // clang: "std::string_view smd::fixpoint::type_name() [T = int]"
    // gcc:   "constexpr std::string_view smd::fixpoint::type_name() [with T
    //         = int; std::string_view = std::basic_string_view<char>]"
    constexpr std::string_view fn = __PRETTY_FUNCTION__;
    constexpr auto start = fn.find("T = ") + 4;
    constexpr auto semi = fn.find(';', start); // gcc's "; std::string_view..."
    constexpr auto close = fn.rfind(']');
    return fn.substr(start, (semi < close ? semi : close) - start);
#elif defined(_MSC_VER)
    // "class std::basic_string_view<...> __cdecl
    //  smd::fixpoint::type_name<int>(void)"
    constexpr std::string_view fn = __FUNCSIG__;
    constexpr auto start = fn.find("type_name<") + 10;
    constexpr auto stop = fn.rfind(">(");
    return fn.substr(start, stop - start);
#else
    return "unknown-type";
#endif
}

/** Reduce a full type spelling to its leaf identifier: drop template
 * arguments, then any namespace qualification, then a trailing "_t".
 * The order matters — the namespace strip must not find a "::" inside
 * template arguments, so those go first.
 */
constexpr auto short_type_name(std::string_view full) -> std::string_view {
    if (auto angle = full.find('<'); angle != std::string_view::npos) {
        full = full.substr(0, angle);
    }
    if (auto colons = full.rfind("::"); colons != std::string_view::npos) {
        full = full.substr(colons + 2);
    }
    if (full.ends_with("_t")) {
        full.remove_suffix(2);
    }
    return full;
}

/** The leaf-identifier spelling of type @p T. */
template <typename T>
constexpr auto short_type_name() -> std::string_view {
    // The constexpr local is load-bearing, not style: it forces manifest
    // constant evaluation. GCC 15.2's opportunistic fold of a plain
    // `return short_type_name(type_name<T>());` miscompiles find('<') on
    // the view into __PRETTY_FUNCTION__ -- it reports the character's
    // offset in the underlying array (past the view's end), so the
    // template-argument strip silently no-ops and "vector<int>" comes
    // back instead of "vector". The constexpr evaluator gets it right;
    // only the runtime fold is wrong. Callers wanting a short name for a
    // type should come through here, not through the string overload on a
    // type_name<T>() result, for the same reason.
    constexpr std::string_view stripped = short_type_name(type_name<T>());
    return stripped;
}
// 7fe5093e-c875-4524-9da0-ee7767f1733f end

} // namespace smd::fixpoint

#endif // INCLUDED_SMD_FIXPOINT_TYPE_NAME
