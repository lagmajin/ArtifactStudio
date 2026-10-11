#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

import Core.ArtifactTuple;

using namespace ArtifactCore;

static_assert(tupleSizeV<Tuple<int>> == 1);
static_assert(tupleSizeV<Tuple<int, char, double>> == 3);
static_assert(std::is_copy_constructible_v<Tuple<int, std::string>>);
static_assert(std::is_move_constructible_v<Tuple<std::unique_ptr<int>>>);
static_assert(!std::is_copy_constructible_v<Tuple<std::unique_ptr<int>>>);
static_assert(std::is_same_v<decltype(artifactMakeTuple("abc")), Tuple<const char*>>);
constexpr Tuple<int, char> kConstexprTuple(4, 'x');
static_assert(artifactGet<0>(kConstexprTuple) == 4);
static_assert(artifactGet<1>(kConstexprTuple) == 'x');
static_assert(kConstexprTuple == Tuple<int, char>(4, 'x'));

TEST(ArtifactTupleContractTest, EmptyTupleHasZeroSizeAndComparesEqual)
{
    constexpr Tuple<> first;
    constexpr Tuple<> second;
    static_assert(tupleSizeV<Tuple<>> == 0);
    EXPECT_EQ(first, second);
    EXPECT_FALSE(first != second);
}

TEST(ArtifactTupleContractTest, AccessorsReturnCorrectConstAndMutableReferences)
{
    Tuple<int, std::string, double> values(3, "value", 2.5);
    static_assert(std::is_same_v<decltype(artifactGet<0>(values)), int&>);
    static_assert(std::is_same_v<decltype(artifactGet<1>(values)), std::string&>);
    artifactGet<0>(values) = 8;
    artifactGet<1>(values).append("-updated");

    const auto& readOnly = values;
    static_assert(std::is_same_v<decltype(artifactGet<0>(readOnly)), const int&>);
    static_assert(std::is_same_v<decltype(artifactGet<2>(readOnly)), const double&>);
    EXPECT_EQ(artifactGet<0>(readOnly), 8);
    EXPECT_EQ(artifactGet<1>(readOnly), "value-updated");
    EXPECT_DOUBLE_EQ(artifactGet<2>(readOnly), 2.5);
}

TEST(ArtifactTupleContractTest, MakeTupleDecaysLvalueReferencesIntoOwnedValues)
{
    int source = 4;
    auto values = artifactMakeTuple(source, std::string("owned"));
    static_assert(std::is_same_v<decltype(values), Tuple<int, std::string>>);
    artifactGet<0>(values) = 9;
    EXPECT_EQ(source, 4);
    EXPECT_EQ(artifactGet<0>(values), 9);
}

TEST(ArtifactTupleContractTest, MakeTupleForwardsMoveOnlyValues)
{
    auto values = artifactMakeTuple(std::make_unique<int>(27));
    ASSERT_TRUE(artifactGet<0>(values));
    EXPECT_EQ(*artifactGet<0>(values), 27);
    auto moved = std::move(values);
    ASSERT_TRUE(artifactGet<0>(moved));
    EXPECT_EQ(*artifactGet<0>(moved), 27);
}

TEST(ArtifactTupleContractTest, EqualityChecksHeadAndEveryTailElement)
{
    const Tuple<int, int, int> baseline(1, 2, 3);
    EXPECT_EQ(baseline, Tuple<int, int, int>(1, 2, 3));
    EXPECT_NE(baseline, Tuple<int, int, int>(9, 2, 3));
    EXPECT_NE(baseline, Tuple<int, int, int>(1, 9, 3));
    EXPECT_NE(baseline, Tuple<int, int, int>(1, 2, 9));
}

TEST(ArtifactTupleContractTest, CopyAndAssignmentPreserveIndependentElements)
{
    const Tuple<std::string, int> original("keep", 6);
    auto copied = original;
    Tuple<std::string, int> assigned("old", 0);
    assigned = original;
    artifactGet<0>(copied) = "changed";
    EXPECT_EQ(artifactGet<0>(original), "keep");
    EXPECT_EQ(artifactGet<0>(assigned), "keep");
    EXPECT_EQ(artifactGet<1>(assigned), 6);
}

TEST(ArtifactTupleContractTest, NestedTuplesRemainRegularElements)
{
    const Tuple<int, Tuple<std::string, bool>> nested(1, Tuple<std::string, bool>("inner", true));
    EXPECT_EQ(tupleSizeV<Tuple<int, Tuple<std::string, bool>>>, 2u);
    EXPECT_EQ(artifactGet<0>(artifactGet<1>(nested)), "inner");
    EXPECT_TRUE(artifactGet<1>(artifactGet<1>(nested)));
}

TEST(ArtifactTupleContractTest, SingleElementTupleSupportsHeadAndIndexedAccess)
{
    Tuple<std::string> value("only");
    EXPECT_EQ(value.head(), "only");
    EXPECT_EQ(artifactGet<0>(value), "only");
    value.head() = "updated";
    EXPECT_EQ(artifactGet<0>(value), "updated");
}

TEST(ArtifactTupleContractTest, MakeTupleWithNoArgumentsCreatesEmptyTuple)
{
    constexpr auto value = artifactMakeTuple();
    static_assert(tupleSizeV<decltype(value)> == 0);
    EXPECT_EQ(value, Tuple<>{});
}

TEST(ArtifactTupleContractTest, MoveConstructionTransfersMoveOnlyElement)
{
    auto source = artifactMakeTuple(std::make_unique<int>(41));
    auto moved = std::move(source);
    ASSERT_TRUE(artifactGet<0>(moved));
    EXPECT_EQ(*artifactGet<0>(moved), 41);
}

TEST(ArtifactTupleContractTest, MoveAssignmentReplacesExistingElements)
{
    auto source = artifactMakeTuple(std::make_unique<int>(52));
    auto destination = artifactMakeTuple(std::make_unique<int>(1));
    destination = std::move(source);
    ASSERT_TRUE(artifactGet<0>(destination));
    EXPECT_EQ(*artifactGet<0>(destination), 52);
}

TEST(ArtifactTupleContractTest, HeadAccessPreservesConstness)
{
    Tuple<int, std::string> value(2, "tail");
    static_assert(std::is_same_v<decltype(value.head()), int&>);
    const auto& readOnly = value;
    static_assert(std::is_same_v<decltype(readOnly.head()), const int&>);
    EXPECT_EQ(readOnly.head(), 2);
}

TEST(ArtifactTupleContractTest, EqualityWorksWithEmptyTupleAndInequality)
{
    constexpr Tuple<> empty;
    EXPECT_TRUE(empty == Tuple<>{});
    EXPECT_FALSE(empty != Tuple<>{});
}

TEST(ArtifactTupleContractTest, DefaultConstructionValueInitializesEveryElement)
{
    Tuple<int, std::string, double> values;
    EXPECT_EQ(artifactGet<0>(values), 0);
    EXPECT_TRUE(artifactGet<1>(values).empty());
    EXPECT_DOUBLE_EQ(artifactGet<2>(values), 0.0);
}

TEST(ArtifactTupleContractTest, DirectConstructionCopiesLvalueElements)
{
    std::string source = "copy-me";
    Tuple<std::string, int> values(source, 12);
    source = "changed";
    EXPECT_EQ(artifactGet<0>(values), "copy-me");
    EXPECT_EQ(artifactGet<1>(values), 12);
}

TEST(ArtifactTupleContractTest, MakeTupleDecaysStringLiteral)
{
    auto value = artifactMakeTuple("abc");
    static_assert(std::is_same_v<decltype(value), Tuple<const char*>>);
    EXPECT_STREQ(artifactGet<0>(value), "abc");
}

TEST(ArtifactTupleContractTest, SelfCopyAssignmentPreservesAllElements)
{
    Tuple<std::string, int, bool> values("stable", 33, true);
    values = values;
    EXPECT_EQ(artifactGet<0>(values), "stable");
    EXPECT_EQ(artifactGet<1>(values), 33);
    EXPECT_TRUE(artifactGet<2>(values));
}

TEST(ArtifactTupleContractTest, ReferenceElementAliasesOriginalValue)
{
    int original = 12;
    Tuple<int&> reference(original);
    static_assert(std::is_same_v<decltype(artifactGet<0>(reference)), int&>);

    artifactGet<0>(reference) = 27;

    EXPECT_EQ(original, 27);
    EXPECT_EQ(reference.head(), 27);
}

TEST(ArtifactTupleContractTest, ConstReferenceElementRemainsReadOnlyAlias)
{
    const int original = 35;
    Tuple<const int&> reference(original);
    static_assert(std::is_same_v<decltype(artifactGet<0>(reference)), const int&>);

    EXPECT_EQ(&artifactGet<0>(reference), &original);
    EXPECT_EQ(artifactGet<0>(reference), 35);
}

TEST(ArtifactTupleContractTest, HighArityTupleAccessesEveryRecursiveTailPosition)
{
    Tuple<int, char, short, long, float, double, bool, std::string> values(
        1, 'a', 2, 3L, 4.5f, 6.25, true, "last");
    static_assert(tupleSizeV<decltype(values)> == 8);
    EXPECT_EQ(artifactGet<0>(values), 1);
    EXPECT_EQ(artifactGet<1>(values), 'a');
    EXPECT_EQ(artifactGet<2>(values), 2);
    EXPECT_EQ(artifactGet<3>(values), 3L);
    EXPECT_FLOAT_EQ(artifactGet<4>(values), 4.5f);
    EXPECT_DOUBLE_EQ(artifactGet<5>(values), 6.25);
    EXPECT_TRUE(artifactGet<6>(values));
    EXPECT_EQ(artifactGet<7>(values), "last");
}

TEST(ArtifactTupleContractTest, NestedMoveOnlyTailSurvivesMoveConstructionAndAssignment)
{
    using Nested = Tuple<std::string, Tuple<std::unique_ptr<int>, std::unique_ptr<int>>>;
    Nested source("nested", Tuple<std::unique_ptr<int>, std::unique_ptr<int>>(
        std::make_unique<int>(17), std::make_unique<int>(29)));
    Nested moved(std::move(source));
    ASSERT_TRUE(artifactGet<0>(artifactGet<1>(moved)));
    ASSERT_TRUE(artifactGet<1>(artifactGet<1>(moved)));
    EXPECT_EQ(*artifactGet<0>(artifactGet<1>(moved)), 17);
    EXPECT_EQ(*artifactGet<1>(artifactGet<1>(moved)), 29);

    Nested destination("old", Tuple<std::unique_ptr<int>, std::unique_ptr<int>>(
        std::make_unique<int>(1), std::make_unique<int>(2)));
    destination = std::move(moved);
    EXPECT_EQ(artifactGet<0>(destination), "nested");
    EXPECT_EQ(*artifactGet<0>(artifactGet<1>(destination)), 17);
    EXPECT_EQ(*artifactGet<1>(artifactGet<1>(destination)), 29);
}

TEST(ArtifactTupleContractTest, MakeTupleOwnsMixedValuesAndPreservesReferenceWrapper)
{
    int source = 12;
    auto values = artifactMakeTuple(source, std::string("owned"), std::ref(source));
    static_assert(std::is_same_v<decltype(values), Tuple<int, std::string, std::reference_wrapper<int>>>);
    artifactGet<0>(values) = 40;
    artifactGet<1>(values).append("-value");
    artifactGet<2>(values).get() = 77;
    EXPECT_EQ(source, 77);
    EXPECT_EQ(artifactGet<0>(values), 40);
    EXPECT_EQ(artifactGet<1>(values), "owned-value");
}

