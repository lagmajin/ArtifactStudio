#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

import Core.ArtifactSpan;

using ArtifactCore::ArtifactSpan;

static_assert(std::is_same_v<decltype(std::declval<const ArtifactSpan<int>&>()[0]), int&>);
static_assert(std::is_same_v<decltype(std::declval<const ArtifactSpan<const int>&>()[0]), const int&>);
static_assert(std::is_same_v<decltype(std::declval<const ArtifactSpan<int>&>().data()), int*>);
static_assert(std::is_same_v<decltype(std::declval<const ArtifactSpan<const int>&>().data()), const int*>);
static_assert(std::is_constructible_v<ArtifactSpan<const int>, std::array<int, 3>&>);
static_assert(!std::is_constructible_v<ArtifactSpan<int>, const std::array<int, 3>&>);

constexpr int kConstexprValues[] = {2, 7, 1};
constexpr ArtifactSpan<const int> kConstexprSpan(kConstexprValues, 3);
static_assert(kConstexprSpan.size() == 3);
static_assert(kConstexprSpan[1] == 7);
static_assert(kConstexprSpan.first(2).back() == 7);
static_assert(kConstexprSpan.last(2).front() == 7);

TEST(ArtifactSpanContractTest, DefaultConstructionIsEmpty)
{
    constexpr ArtifactSpan<int> span;
    EXPECT_EQ(span.data(), nullptr);
    EXPECT_EQ(span.size(), 0u);
    EXPECT_TRUE(span.isEmpty());
}

TEST(ArtifactSpanContractTest, PointerAndCountExposeTheProvidedRange)
{
    int values[] = {4, 8, 15, 16};
    ArtifactSpan<int> span(values, 4);
    EXPECT_EQ(span.data(), values);
    EXPECT_EQ(span.size(), 4u);
    EXPECT_FALSE(span.isEmpty());
    EXPECT_EQ(span.end() - span.begin(), 4);
}

TEST(ArtifactSpanContractTest, ContainerConstructionReflectsContainerRange)
{
    std::array<int, 3> values{2, 3, 5};
    ArtifactSpan<int> span(values);
    EXPECT_EQ(span.data(), values.data());
    EXPECT_EQ(span.size(), values.size());
    EXPECT_EQ(span[2], 5);
}

TEST(ArtifactSpanContractTest, ElementAccessAndMutationAliasTheSource)
{
    int values[] = {1, 2, 3};
    ArtifactSpan<int> span(values, 3);
    span[1] = 20;
    EXPECT_EQ(values[1], 20);
    EXPECT_EQ(span.front(), 1);
    EXPECT_EQ(span.back(), 3);
}

TEST(ArtifactSpanContractTest, ConstElementTypePreventsMutation)
{
    const int values[] = {6, 7};
    ArtifactSpan<const int> span(values, 2);
    EXPECT_EQ(span.front(), 6);
    EXPECT_EQ(span.back(), 7);
}

TEST(ArtifactSpanContractTest, ConstSpanObjectStillMutatesMutableElements)
{
    int values[] = {2, 4, 6};
    const ArtifactSpan<int> span(values, 3);

    span[1] = 40;

    EXPECT_EQ(values[1], 40);
}

TEST(ArtifactSpanContractTest, CopyAndAssignmentRetainBorrowedRangeSemantics)
{
    int firstValues[] = {1, 2};
    int secondValues[] = {7, 8, 9};
    ArtifactSpan<int> original(firstValues, 2);
    ArtifactSpan<int> copied(original);
    ArtifactSpan<int> assigned(secondValues, 3);

    assigned = copied;
    copied[0] = 11;

    EXPECT_EQ(original.data(), firstValues);
    EXPECT_EQ(assigned.data(), firstValues);
    EXPECT_EQ(assigned.size(), 2u);
    EXPECT_EQ(firstValues[0], 11);
    EXPECT_EQ(secondValues[0], 7);
}

TEST(ArtifactSpanContractTest, IterationVisitsEveryElementInOrder)
{
    int values[] = {3, 1, 4};
    ArtifactSpan<int> span(values, 3);
    int sum = 0;
    for (int& value : span)
        sum += value;
    EXPECT_EQ(sum, 8);
}

TEST(ArtifactSpanContractTest, SubspanSelectsRequestedMiddleRange)
{
    int values[] = {0, 1, 2, 3, 4};
    const auto span = ArtifactSpan<int>(values, 5).subspan(1, 3);
    ASSERT_EQ(span.size(), 3u);
    EXPECT_EQ(span.data(), values + 1);
    EXPECT_EQ(span[0], 1);
    EXPECT_EQ(span[2], 3);
}

TEST(ArtifactSpanContractTest, FirstAndLastSelectBoundaryRanges)
{
    int values[] = {10, 20, 30, 40};
    const ArtifactSpan<int> span(values, 4);
    EXPECT_EQ(span.first(2).back(), 20);
    EXPECT_EQ(span.last(2).front(), 30);
}

TEST(ArtifactSpanContractTest, ZeroLengthSubspanKeepsZeroSize)
{
    int values[] = {1, 2};
    const auto span = ArtifactSpan<int>(values, 2).subspan(1, 0);
    EXPECT_EQ(span.size(), 0u);
    EXPECT_TRUE(span.isEmpty());
    EXPECT_EQ(span.data(), values + 1);
}

TEST(ArtifactSpanContractTest, SubspanMutationUpdatesOriginalStorage)
{
    int values[] = {5, 6, 7};
    auto middle = ArtifactSpan<int>(values, 3).subspan(1, 1);
    middle.front() = 60;
    EXPECT_EQ(values[1], 60);
}

TEST(ArtifactSpanContractTest, ConstContainerBuildsReadOnlyView)
{
    const std::array<int, 3> values{8, 9, 10};
    const ArtifactSpan<const int> span(values);
    EXPECT_EQ(span.data(), values.data());
    EXPECT_EQ(span.size(), 3u);
    EXPECT_EQ(span[1], 9);
}

TEST(ArtifactSpanContractTest, ReadOnlyViewOfMutableContainerObservesSourceUpdates)
{
    std::array<int, 3> values{2, 4, 6};
    const ArtifactSpan<const int> view(values);
    values[1] = 40;

    EXPECT_EQ(view.data(), values.data());
    EXPECT_EQ(view[1], 40);
}

TEST(ArtifactSpanContractTest, EmptyContainerProducesEmptySpan)
{
    const std::array<int, 0> values{};
    const ArtifactSpan<const int> span(values);
    EXPECT_EQ(span.size(), 0u);
    EXPECT_TRUE(span.isEmpty());
}

TEST(ArtifactSpanContractTest, FirstZeroAndLastZeroPointAtRangeBoundaries)
{
    int values[] = {11, 12, 13};
    const ArtifactSpan<int> span(values, 3);
    const auto emptyPrefix = span.first(0);
    const auto emptySuffix = span.last(0);
    EXPECT_EQ(emptyPrefix.data(), values);
    EXPECT_EQ(emptyPrefix.size(), 0u);
    EXPECT_EQ(emptySuffix.data(), values + 3);
    EXPECT_EQ(emptySuffix.size(), 0u);
}

TEST(ArtifactSpanContractTest, FullRangeSubspanPreservesAllElements)
{
    int values[] = {14, 15, 16};
    const auto whole = ArtifactSpan<int>(values, 3).subspan(0, 3);
    EXPECT_EQ(whole.data(), values);
    EXPECT_EQ(whole.size(), 3u);
    EXPECT_EQ(whole.front(), 14);
    EXPECT_EQ(whole.back(), 16);
}

TEST(ArtifactSpanContractTest, LastRangeMutationAliasesOriginalStorage)
{
    int values[] = {1, 2, 3, 4};
    auto suffix = ArtifactSpan<int>(values, 4).last(2);
    suffix.front() = 30;
    suffix.back() = 40;
    EXPECT_EQ(values[2], 30);
    EXPECT_EQ(values[3], 40);
}

TEST(ArtifactSpanContractTest, RepeatedSubspansComposeOffsets)
{
    int values[] = {0, 1, 2, 3, 4, 5};
    const auto nested = ArtifactSpan<int>(values, 6).subspan(1, 4).subspan(1, 2);
    EXPECT_EQ(nested.data(), values + 2);
    EXPECT_EQ(nested.size(), 2u);
    EXPECT_EQ(nested.front(), 2);
    EXPECT_EQ(nested.back(), 3);
}

TEST(ArtifactSpanContractTest, EveryValidSmallSubspanMatchesItsSourceRange)
{
    std::array<int, 6> values{4, 8, 15, 16, 23, 42};
    const ArtifactSpan<int> source(values);

    for (std::size_t offset = 0; offset <= values.size(); ++offset) {
        for (std::size_t count = 0; count <= values.size() - offset; ++count) {
            const auto view = source.subspan(offset, count);
            SCOPED_TRACE(::testing::Message()
                << "offset=" << offset << " count=" << count);
            EXPECT_EQ(view.data(), values.data() + offset);
            ASSERT_EQ(view.size(), count);
            for (std::size_t index = 0; index < count; ++index) {
                EXPECT_EQ(view[index], values[offset + index]);
            }
        }
    }
}
