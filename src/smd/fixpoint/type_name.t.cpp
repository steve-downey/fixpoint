// src/smd/fixpoint/type_name.t.cpp                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/type_name.hpp>
#include <smd/fixpoint/type_name.hpp> // Re-inclusion check

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <vector>

using smd::fixpoint::short_type_name;
using smd::fixpoint::type_name;

TEST_CASE("type_name - HeaderIsIdempotent") { REQUIRE(true); }

namespace smd::probe {
struct plain_type {};
struct tagged_t {};
template <typename T>
struct wrapper {};
} // namespace smd::probe

// The compile-time contract: pinned with static_assert per house style.
static_assert(type_name<int>() == "int");
static_assert(short_type_name<int>() == "int");
static_assert(short_type_name<smd::probe::plain_type>() == "plain_type");
static_assert(short_type_name<smd::probe::tagged_t>() == "tagged");
static_assert(short_type_name<smd::probe::wrapper<smd::probe::tagged_t>>() ==
              "wrapper");

// The string-level reduction is compiler-independent even for spellings we
// never generate locally.
static_assert(short_type_name("beman::execution::detail::then_t<"
                              "beman::execution::set_value_t>") == "then");
static_assert(short_type_name("beman::execution::detail::just_t<"
                              "beman::execution::set_value_t>") == "just");
static_assert(short_type_name("foo::bar_t<a::b<c::d>, e::f>") == "bar");
static_assert(short_type_name("no_namespace") == "no_namespace");

TEST_CASE("type_name - StdVectorShortName") {
    // The full spelling differs per standard library (allocator defaulting);
    // the short name does not.
    CHECK(short_type_name<std::vector<int>>() == "vector");
}

TEST_CASE("type_name - FullNameContainsQualification") {
    auto full = type_name<smd::probe::plain_type>();
    CHECK(full.find("plain_type") != std::string_view::npos);
    CHECK(full.find("probe") != std::string_view::npos);
}
