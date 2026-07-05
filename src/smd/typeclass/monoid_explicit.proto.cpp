// Standalone prototype driver for monoid_explicit.hpp (experiment branch).
//   g++ -std=c++23 -I src monoid_explicit.proto.cpp -o /tmp/mproto && /tmp/mproto
// Define MONOID_BAD_INSTANCE to see the static_assert fire for a non-monoid.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/typeclass/monoid_explicit.hpp>

// The library's global monoid, for contrast: monoid_v<int> is additive, and
// combine_all can only ever use it.
#include <smd/typeclass/monoid.hpp>

#include <array>
#include <cassert>
#include <limits>

namespace proto {

// Four equally-valid monoids over int. None is canonical; each is an ordinary
// empty value object -- no newtype wrapper around int anywhere.
struct IntSum {
    constexpr auto identity() const -> int { return 0; }
    constexpr auto combine(int a, int b) const -> int { return a + b; }
};
struct IntProduct {
    constexpr auto identity() const -> int { return 1; }
    constexpr auto combine(int a, int b) const -> int { return a * b; }
};
struct IntMax {
    constexpr auto identity() const -> int {
        return std::numeric_limits<int>::min();
    }
    constexpr auto combine(int a, int b) const -> int { return a > b ? a : b; }
};
struct IntMin {
    constexpr auto identity() const -> int {
        return std::numeric_limits<int>::max();
    }
    constexpr auto combine(int a, int b) const -> int { return a < b ? a : b; }
};

#ifdef MONOID_BAD_INSTANCE
// combine but no identity() -> not a monoid.
struct NotAMonoid {
    constexpr auto combine(int a, int b) const -> int { return a + b; }
};
#endif

constexpr std::array<int, 8> xs{3, 1, 4, 1, 5, 9, 2, 6};

} // namespace proto

int main() {
    using smd::typeclass::experimental::combine_all_with;
    using smd::typeclass::experimental::fold_map_with;

    // The SAME container of ints, combined four ways by threading four
    // different instances. No wrapper types; no change to any global.
    assert(combine_all_with(proto::IntSum{}, proto::xs) == 31);
    assert(combine_all_with(proto::IntProduct{}, proto::xs) == 6480);
    assert(combine_all_with(proto::IntMax{}, proto::xs) == 9);
    assert(combine_all_with(proto::IntMin{}, proto::xs) == 1);

    // fold_map_with: map then combine under an explicit monoid (sum of
    // squares via the product... no -- sum of squares via IntSum on x*x).
    assert(fold_map_with(proto::IntSum{}, [](int x) { return x * x; },
                         proto::xs) == 9 + 1 + 16 + 1 + 25 + 81 + 4 + 36);

    // Contrast: the global lookup can only give the one baked-in choice.
    // monoid_v<int> is additive, so this matches IntSum and nothing else.
    {
        int acc = smd::typeclass::monoid_v<int>.identity();
        for (int x : proto::xs)
            acc = smd::typeclass::monoid_v<int>.combine(acc, x);
        assert(acc == 31); // product/min/max are simply unreachable this way
    }

    // constexpr in all cases.
    static_assert(combine_all_with(proto::IntProduct{}, proto::xs) == 6480);
    static_assert(combine_all_with(proto::IntMax{}, proto::xs) == 9);

#ifdef MONOID_BAD_INSTANCE
    (void)combine_all_with(proto::NotAMonoid{}, proto::xs);
#endif

    return 0;
}
