// src/smd/fixpoint/overloaded.hpp                                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_OVERLOADED
#define INCLUDED_SMD_FIXPOINT_OVERLOADED

// overloaded<Ts...> — aggregate visitor for std::visit.
//
// Usage:
//   std::visit(overloaded{
//       [](int x)         { ... },
//       [](std::string s) { ... },
//   }, v);
//
// The consteval catch-all fires a static_assert at compile time if std::visit
// encounters an alternative not covered by the explicit cases.  This turns
// variant exhaustiveness into a hard compile error rather than a silent
// default/no-op.  Adding a new alternative to a variant without handling it
// everywhere is caught immediately.
//
// No explicit deduction guide is needed: C++20 CTAD for aggregates deduces
// overloaded<F1, F2, ...> from the constructor arguments.

namespace smd::fixpoint {

template <typename... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;

    consteval void operator()(auto) const {
        static_assert(false,
                      "overloaded: unhandled variant alternative — add a case");
    }
};

} // namespace smd::fixpoint

#endif
