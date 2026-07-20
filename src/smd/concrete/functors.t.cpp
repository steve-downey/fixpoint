// src/smd/concrete/functors.t.cpp                                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/concrete/functors.hpp>
#include <smd/concrete/functors.hpp> // Re-inclusion check

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/overloaded.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <variant>
#include <vector>

using smd::concrete::add_node;
using smd::concrete::Cons;
using smd::concrete::Const;
using smd::concrete::const_node;
using smd::concrete::eval;
using smd::concrete::Expr;
using smd::concrete::IntList;
using smd::concrete::IntListF;
using smd::concrete::IntTree;
using smd::concrete::IntTreeF;
using smd::concrete::Leaf;
using smd::concrete::list_from_vector;
using smd::concrete::list_to_vector;
using smd::concrete::ListF;
using smd::concrete::make_leaf;
using smd::concrete::make_node;
using smd::concrete::make_succ;
using smd::concrete::make_zero;
using smd::concrete::mul_node;
using smd::concrete::Nat;
using smd::concrete::nat_from_int;
using smd::concrete::nat_to_int;
using smd::concrete::NatF;
using smd::concrete::Nil;
using smd::concrete::Node;
using smd::concrete::Succ;
using smd::concrete::Zero;
using smd::fixpoint::fold_fix;
using smd::fixpoint::layer_fmap;
using smd::fixpoint::make_box;
using smd::fixpoint::overloaded;

namespace {

// Helper for the TreeF static_assert / smoke test — not part of the public
// header (design doesn't require a shipped fold for TreeF, only the
// constructors), but a minimal sum-of-leaves cata exercises fold_fix +
// TreeF's functor_typeclass instance the same way real callers will.
constexpr auto sum_leaves(const IntTree &t) -> int {
    return fold_fix<int>(
        [](const IntTreeF<int> &layer) -> int {
            return std::visit(
                overloaded{
                    [](const Leaf<int> &l) { return l.value; },
                    [](const Node<int> &n) { return *n.left + *n.right; },
                },
                layer);
        },
        t);
}

} // namespace

TEST_CASE("functors - HeaderIsIdempotent") { REQUIRE(true); }

// ---------------------------------------------------------------------
// NatF
// ---------------------------------------------------------------------

TEST_CASE("functors - NatRoundTrip") {
    for (int n = 0; n <= 10; ++n) {
        CHECK(nat_to_int(nat_from_int(n)) == n);
    }
}

TEST_CASE("functors - NatMakeZeroSucc") {
    Nat two = make_succ(make_succ(make_zero()));
    CHECK(nat_to_int(two) == 2);
}

TEST_CASE("functors - NatFmapSmoke") {
    NatF<int> one = Succ<int>{make_box<int>(0)};
    auto mapped = layer_fmap([](int x) { return x + 1; }, one);
    REQUIRE(std::holds_alternative<Succ<int>>(mapped));
    CHECK(*std::get<Succ<int>>(mapped).pred == 1);

    NatF<int> zero = Zero{};
    auto mapped_zero = layer_fmap([](int x) { return x + 1; }, zero);
    CHECK(std::holds_alternative<Zero>(mapped_zero));
}

static_assert(nat_to_int(nat_from_int(4)) == 4);

// ---------------------------------------------------------------------
// ListF<E, ·>
// ---------------------------------------------------------------------

TEST_CASE("functors - ListVectorRoundTrip") {
    std::vector<int> v{1, 2, 3, 4, 5};
    CHECK(list_to_vector(list_from_vector(v)) == v);
}

TEST_CASE("functors - ListVectorRoundTripEmpty") {
    std::vector<int> v{};
    CHECK(list_to_vector(list_from_vector(v)) == v);
}

TEST_CASE("functors - ListFmapSmoke") {
    IntListF<int> cons = Cons<int, int>{7, make_box<int>(0)};
    auto mapped = layer_fmap([](int x) { return x * 2; }, cons);
    REQUIRE(std::holds_alternative<Cons<int, int>>(mapped));
    CHECK(std::get<Cons<int, int>>(mapped).head == 7);
    CHECK(*std::get<Cons<int, int>>(mapped).tail == 0);
}

TEST_CASE("functors - ListFmapStringPayloadIsGeneric") {
    // Proves the ListF functor_typeclass instance is truly partially
    // specialized over <E, A> — here E = std::string, not int.
    using StrListF = ListF<std::string, int>;
    StrListF layer = Cons<std::string, int>{"answer", make_box<int>(21)};
    auto mapped = layer_fmap([](int x) { return x * 2; }, layer);
    REQUIRE(std::holds_alternative<Cons<std::string, int>>(mapped));
    CHECK(std::get<Cons<std::string, int>>(mapped).head == "answer");
    CHECK(*std::get<Cons<std::string, int>>(mapped).tail == 42);

    StrListF nil_layer = Nil<std::string>{};
    auto mapped_nil = layer_fmap([](int x) { return x * 2; }, nil_layer);
    CHECK(std::holds_alternative<Nil<std::string>>(mapped_nil));
}

static_assert(list_to_vector(list_from_vector(std::vector<int>{1, 2, 3})) ==
              std::vector<int>{1, 2, 3});

// ---------------------------------------------------------------------
// TreeF<E, ·>
// ---------------------------------------------------------------------

TEST_CASE("functors - TreeMakeLeafNode") {
    IntTree t = make_node(make_leaf(2), make_leaf(3));
    CHECK(sum_leaves(t) == 5);
}

TEST_CASE("functors - TreeFmapSmoke") {
    IntTreeF<int> node = Node<int>{make_box<int>(2), make_box<int>(3)};
    auto mapped = layer_fmap([](int x) { return x + 1; }, node);
    REQUIRE(std::holds_alternative<Node<int>>(mapped));
    CHECK(*std::get<Node<int>>(mapped).left == 3);
    CHECK(*std::get<Node<int>>(mapped).right == 4);

    IntTreeF<int> leaf = Leaf<int>{9};
    auto mapped_leaf = layer_fmap([](int x) { return x + 1; }, leaf);
    REQUIRE(std::holds_alternative<Leaf<int>>(mapped_leaf));
    CHECK(std::get<Leaf<int>>(mapped_leaf).value == 9);
}

static_assert(sum_leaves(make_node(make_leaf(2), make_leaf(3))) == 5);

// ---------------------------------------------------------------------
// ExprF
// ---------------------------------------------------------------------

TEST_CASE("functors - ExprEvalMatchesHandComputed") {
    // (2 * 3) + 4 == 10, mirrors fixpoint_tree_example.cpp.
    Expr tree = add_node(mul_node(const_node(2), const_node(3)), const_node(4));
    CHECK(eval(tree) == 10);
}

TEST_CASE("functors - ExprFmapSmoke") {
    using smd::concrete::Add;
    using smd::concrete::ExprF;

    ExprF<int> add = Add<int>{make_box<int>(2), make_box<int>(3)};
    auto mapped = layer_fmap([](int x) { return x * 10; }, add);
    REQUIRE(std::holds_alternative<Add<int>>(mapped));
    CHECK(*std::get<Add<int>>(mapped).left == 20);
    CHECK(*std::get<Add<int>>(mapped).right == 30);

    ExprF<int> c = Const<int>{5};
    auto mapped_const = layer_fmap([](int x) { return x * 10; }, c);
    REQUIRE(std::holds_alternative<Const<int>>(mapped_const));
    CHECK(std::get<Const<int>>(mapped_const).val == 5);
}

static_assert(eval(add_node(mul_node(const_node(2), const_node(3)),
                            const_node(4))) == 10);

// ---------------------------------------------------------------------
// RoseF
// ---------------------------------------------------------------------

namespace {
template <typename A>
using StrRoseF = smd::concrete::RoseF<std::string, A>;

using StrRose = smd::fixpoint::Fix<StrRoseF>;
} // namespace

TEST_CASE("functors - RoseFmapSmoke") {
    using smd::concrete::RoseF;

    RoseF<std::string, int> layer{"node", {1, 2, 3}};
    auto mapped = layer_fmap([](int x) { return x + 1; }, layer);
    CHECK(mapped.label == "node");
    CHECK(mapped.children == std::vector<int>{2, 3, 4});
}

TEST_CASE("functors - RoseFmapIdentityAndComposition") {
    using smd::concrete::RoseF;

    RoseF<std::string, int> layer{"node", {1, 2, 3}};

    auto identity_mapped = layer_fmap([](int x) { return x; }, layer);
    CHECK(identity_mapped.label == layer.label);
    CHECK(identity_mapped.children == layer.children);

    auto f = [](int x) { return x + 1; };
    auto g = [](int x) { return x * 2; };
    auto composed = layer_fmap([&](int x) { return g(f(x)); }, layer);
    auto staged = layer_fmap(g, layer_fmap(f, layer));
    CHECK(composed.children == staged.children);
}

TEST_CASE("functors - RoseFoldFixConcatenatesLabels") {
    using smd::concrete::rose_node;
    using smd::fixpoint::fold_fix;

    // fold_fix works for a RoseF alias with an arbitrary label type: the
    // functor_typeclass lookup resolves StrRoseF<A> to RoseF<std::string, A>.
    StrRose tree = rose_node<StrRoseF>(
        std::string("root"),
        {rose_node<StrRoseF>(std::string("a")),
         rose_node<StrRoseF>(std::string("b"),
                             {rose_node<StrRoseF>(std::string("c"))})});

    auto labels = fold_fix<std::string>(
        [](const StrRoseF<std::string> &layer) {
            std::string out = layer.label;
            for (const auto &child : layer.children) {
                out += ' ' + child;
            }
            return out;
        },
        tree);
    CHECK(labels == "root a b c");
}

TEST_CASE("functors - RoseFoldableToVectorAndLength") {
    using smd::concrete::RoseF;
    using smd::typeclass::foldable_typeclass;

    RoseF<std::string, int> layer{"node", {4, 5, 6}};
    const auto &foldable = foldable_typeclass<RoseF<std::string, int>>;
    CHECK(foldable.to_vector(layer) == std::vector<int>{4, 5, 6});
    CHECK(foldable.length(layer) == 3);
    CHECK(!foldable.empty(layer));
    CHECK(foldable.any_of(layer, [](int x) { return x == 5; }));
    CHECK(foldable.all_of(layer, [](int x) { return x > 3; }));
}

TEST_CASE("functors - RoseTraverseOptionalTransposes") {
    using smd::concrete::RoseF;
    using smd::typeclass::traverse;

    RoseF<std::string, int> layer{"node", {1, 2, 3}};

    // All effects succeed: the optional hoists out, shape and label kept.
    auto all =
        traverse([](int x) { return std::optional<int>{x * 10}; }, layer);
    REQUIRE(all.has_value());
    CHECK(all->label == "node");
    CHECK(all->children == std::vector<int>{10, 20, 30});

    // One failure collapses the whole traversal.
    auto failed = traverse(
        [](int x) {
            return x == 2 ? std::optional<int>{} : std::optional<int>{x};
        },
        layer);
    CHECK(!failed.has_value());
}
