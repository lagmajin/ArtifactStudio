#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <vector>

import Core.ArtifactArray;
import Core.ArtifactAlgorithms;

using namespace ArtifactCore;

namespace {

struct EraseAtArray {
    Array<int> values;

    auto begin() { return values.begin(); }
    auto end() { return values.end(); }
    int& operator[](std::size_t index) { return values[index]; }
    void eraseAt(std::size_t index) { values.removeAt(index); }
};

} // namespace

TEST(ArtifactAlgorithmsContractTest, FindAndFindIfReturnEndWhenNoMatch)
{
    constexpr std::array values{2, 4, 6};
    EXPECT_EQ(artifactFind(values.begin(), values.end(), 5), values.end());
    EXPECT_EQ(artifactFindIf(values.begin(), values.end(), [](int value) { return value < 0; }), values.end());
    constexpr std::array<int, 0> empty{};
    EXPECT_EQ(artifactFind(empty.begin(), empty.end(), 0), empty.end());
}

TEST(ArtifactAlgorithmsContractTest, FindIfAndPredicatesShortCircuit)
{
    constexpr std::array values{1, 3, 4, 6};
    int calls = 0;
    const auto found = artifactFindIf(values.begin(), values.end(), [&](int value) {
        ++calls;
        return value % 2 == 0;
    });
    EXPECT_EQ(*found, 4);
    EXPECT_EQ(calls, 3);

    EXPECT_TRUE(artifactAllOf(values.begin(), values.begin(), [](int) { return false; }));
    EXPECT_FALSE(artifactAnyOf(values.begin(), values.begin(), [](int) { return true; }));
    EXPECT_TRUE(artifactNoneOf(values.begin(), values.end(), [](int value) { return value < 0; }));
}

TEST(ArtifactAlgorithmsContractTest, SearchPredicatesAreNotCalledForEmptyRanges)
{
    constexpr std::array<int, 0> empty{};
    int predicateCalls = 0;
    const auto predicate = [&](int) {
        ++predicateCalls;
        return true;
    };

    EXPECT_EQ(artifactFindIf(empty.begin(), empty.end(), predicate), empty.end());
    EXPECT_FALSE(artifactAnyOf(empty.begin(), empty.end(), predicate));
    EXPECT_TRUE(artifactAllOf(empty.begin(), empty.end(), predicate));
    EXPECT_TRUE(artifactNoneOf(empty.begin(), empty.end(), predicate));
    EXPECT_EQ(predicateCalls, 0);
}

TEST(ArtifactAlgorithmsContractTest, MinMaxAndMinMaxPairHandleEmptyAndRepeatedExtrema)
{
    constexpr std::array<int, 0> empty{};
    EXPECT_EQ(artifactMinElement(empty.begin(), empty.end()), empty.end());
    EXPECT_EQ(artifactMaxElement(empty.begin(), empty.end()), empty.end());
    const auto emptyPair = artifactMinMaxElement(empty.begin(), empty.end());
    EXPECT_EQ(emptyPair.first, empty.end());
    EXPECT_EQ(emptyPair.second, empty.end());

    constexpr std::array values{3, 1, 1, 8, 8, 4};
    const auto extrema = artifactMinMaxElement(values.begin(), values.end());
    EXPECT_EQ(*extrema.first, 1);
    EXPECT_EQ(*extrema.second, 8);
}

TEST(ArtifactAlgorithmsContractTest, AccumulateSupportsDefaultAndCustomOperations)
{
    constexpr std::array values{1, 2, 3, 4};
    EXPECT_EQ(artifactAccumulate(values.begin(), values.end(), 10), 20);
    EXPECT_EQ(artifactAccumulate(values.begin(), values.end(), 1,
                                 [](int product, int value) { return product * value; }), 24);
    constexpr std::array<int, 0> empty{};
    EXPECT_EQ(artifactAccumulate(empty.begin(), empty.end(), 17), 17);
}

TEST(ArtifactAlgorithmsContractTest, IotaCountAndCountIfCoverEmptyAndPopulatedRanges)
{
    std::array<int, 5> values{};
    artifactIota(values.begin(), values.end(), 4);
    EXPECT_EQ(values, (std::array<int, 5>{4, 5, 6, 7, 8}));
    EXPECT_EQ(artifactCount(values.begin(), values.end(), 6), 1);
    EXPECT_EQ(artifactCountIf(values.begin(), values.end(), [](int value) { return value >= 6; }), 3);
    EXPECT_EQ(artifactCount(values.begin(), values.end(), 99), 0);
}

TEST(ArtifactAlgorithmsContractTest, CountAndCountIfOnEmptyRangeReturnZero)
{
    constexpr std::array<int, 0> empty{};
    int predicateCalls = 0;

    EXPECT_EQ(artifactCount(empty.begin(), empty.end(), 0), 0);
    EXPECT_EQ(artifactCountIf(empty.begin(), empty.end(), [&](int) {
        ++predicateCalls;
        return true;
    }), 0);
    EXPECT_EQ(predicateCalls, 0);
}

TEST(ArtifactAlgorithmsContractTest, FillAndReverseHandleSingleAndEmptyRanges)
{
    std::array<int, 4> values{1, 2, 3, 4};
    artifactFill(values.begin() + 1, values.end() - 1, 9);
    EXPECT_EQ(values, (std::array<int, 4>{1, 9, 9, 4}));
    artifactReverse(values.begin(), values.end());
    EXPECT_EQ(values, (std::array<int, 4>{4, 9, 9, 1}));
    artifactReverse(values.begin(), values.begin() + 1);
    std::array<int, 0> empty{};
    artifactReverse(empty.begin(), empty.end());
    EXPECT_TRUE(empty.empty());
}

TEST(ArtifactAlgorithmsContractTest, LowerBoundReturnsFirstDuplicateAndRangeEnd)
{
    constexpr std::array values{1, 2, 2, 2, 5, 8};
    const auto firstTwo = artifactLowerBound(values.begin(), values.end(), 2);
    ASSERT_NE(firstTwo, values.end());
    EXPECT_EQ(firstTwo - values.begin(), 1);
    EXPECT_EQ(artifactLowerBound(values.begin(), values.end(), 9), values.end());
    EXPECT_EQ(artifactLowerBound(values.begin(), values.begin(), 0), values.begin());
}

TEST(ArtifactAlgorithmsContractTest, BinarySearchFindsBoundariesAndRejectsMissingValues)
{
    constexpr std::array values{-5, -1, 0, 9, 20};
    EXPECT_TRUE(artifactBinarySearch(values.begin(), values.end(), -5));
    EXPECT_TRUE(artifactBinarySearch(values.begin(), values.end(), 20));
    EXPECT_FALSE(artifactBinarySearch(values.begin(), values.end(), 1));
    constexpr std::array<int, 0> empty{};
    EXPECT_FALSE(artifactBinarySearch(empty.begin(), empty.end(), 1));
}

TEST(ArtifactAlgorithmsContractTest, SortHandlesDuplicatesAndComparatorDirection)
{
    Array<int> values{4, 1, 4, 2, 1, 3};
    artifactSort(values);
    EXPECT_TRUE(artifactIsSorted(values.begin(), values.end()));
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[5], 4);
    artifactSort(values, [](int left, int right) { return left > right; });
    EXPECT_EQ(values[0], 4);
    EXPECT_EQ(values[5], 1);
}

TEST(ArtifactAlgorithmsContractTest, RemoveIfCompactsAndErasesMatchingElements)
{
    EraseAtArray values{Array<int>{0, 1, 2, 3, 4, 5}};
    const std::size_t remaining = artifactRemoveIf(values, [](int value) { return value % 2 == 0; });
    EXPECT_EQ(remaining, 3u);
    EXPECT_EQ(values.values.size(), 3u);
    EXPECT_EQ(values.values[0], 1);
    EXPECT_EQ(values.values[1], 3);
    EXPECT_EQ(values.values[2], 5);
}

TEST(ArtifactAlgorithmsContractTest, ContainerFindReturnsMatchingIteratorOrEnd)
{
    Array<int> values{3, 6, 9};
    EXPECT_EQ(*artifactFindIn(values, 6), 6);
    EXPECT_EQ(artifactFindIn(values, 5), values.end());
}

TEST(ArtifactAlgorithmsContractTest, SortedCheckAcceptsEmptyAndEqualElements)
{
    constexpr std::array<int, 0> empty{};
    constexpr std::array equalValues{4, 4, 4};
    constexpr std::array descending{3, 2, 1};
    EXPECT_TRUE(artifactIsSorted(empty.begin(), empty.end()));
    EXPECT_TRUE(artifactIsSorted(equalValues.begin(), equalValues.end()));
    EXPECT_FALSE(artifactIsSorted(descending.begin(), descending.end()));
}

TEST(ArtifactAlgorithmsContractTest, ContainsHandlesEmptyAndPopulatedContainers)
{
    constexpr std::array<int, 0> empty{};
    constexpr std::array values{4, 8, 12};
    EXPECT_FALSE(artifactContains(empty, 4));
    EXPECT_TRUE(artifactContains(values, 8));
    EXPECT_FALSE(artifactContains(values, 9));
}

TEST(ArtifactAlgorithmsContractTest, AnyAllAndNonePredicatesShortCircuitAtCorrectElement)
{
    constexpr std::array values{2, 4, 5, 8};
    int calls = 0;
    EXPECT_TRUE(artifactAnyOf(values.begin(), values.end(), [&](int value) {
        ++calls;
        return value == 4;
    }));
    EXPECT_EQ(calls, 2);

    calls = 0;
    EXPECT_FALSE(artifactAllOf(values.begin(), values.end(), [&](int value) {
        ++calls;
        return value < 5;
    }));
    EXPECT_EQ(calls, 3);

    calls = 0;
    EXPECT_FALSE(artifactNoneOf(values.begin(), values.end(), [&](int value) {
        ++calls;
        return value == 5;
    }));
    EXPECT_EQ(calls, 3);
}

TEST(ArtifactAlgorithmsContractTest, MinAndMaxElementDefineTieSelection)
{
    constexpr std::array values{7, 1, 1, 9, 9, 4};
    const auto minimum = artifactMinElement(values.begin(), values.end());
    const auto maximum = artifactMaxElement(values.begin(), values.end());
    EXPECT_EQ(minimum - values.begin(), 1);
    EXPECT_EQ(maximum - values.begin(), 4);
}

TEST(ArtifactAlgorithmsContractTest, RemoveIfHandlesEmptyNoMatchAndAllMatch)
{
    EraseAtArray empty;
    EXPECT_EQ(artifactRemoveIf(empty, [](int) { return true; }), 0u);

    EraseAtArray unchanged{Array<int>{1, 3, 5}};
    EXPECT_EQ(artifactRemoveIf(unchanged, [](int value) { return value % 2 == 0; }), 3u);
    EXPECT_EQ(unchanged.values, (Array<int>{1, 3, 5}));

    EraseAtArray removed{Array<int>{2, 4, 6}};
    EXPECT_EQ(artifactRemoveIf(removed, [](int value) { return value % 2 == 0; }), 0u);
    EXPECT_TRUE(removed.values.isEmpty());
}

TEST(ArtifactAlgorithmsContractTest, SortHandlesEmptySingletonAndAlreadySortedRanges)
{
    std::array<int, 0> empty{};
    std::array<int, 1> singleton{7};
    std::array<int, 4> sorted{1, 2, 3, 4};
    artifactSort(empty.begin(), empty.end());
    artifactSort(singleton.begin(), singleton.end());
    artifactSort(sorted.begin(), sorted.end());
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(singleton[0], 7);
    EXPECT_TRUE(artifactIsSorted(sorted.begin(), sorted.end()));
}

TEST(ArtifactAlgorithmsContractTest, FillCoversFullAndEmptyRanges)
{
    std::array<int, 3> values{1, 2, 3};
    artifactFill(values.begin(), values.end(), 6);
    EXPECT_EQ(values, (std::array<int, 3>{6, 6, 6}));
    std::array<int, 0> empty{};
    artifactFill(empty.begin(), empty.end(), 9);
    EXPECT_TRUE(empty.empty());
}

TEST(ArtifactAlgorithmsContractTest, ReverseHandlesTwoElementsAndOddMiddle)
{
    std::array<int, 2> pair{1, 2};
    artifactReverse(pair.begin(), pair.end());
    EXPECT_EQ(pair, (std::array<int, 2>{2, 1}));

    std::array<int, 5> values{1, 2, 3, 4, 5};
    artifactReverse(values.begin(), values.end());
    EXPECT_EQ(values, (std::array<int, 5>{5, 4, 3, 2, 1}));
}

TEST(ArtifactAlgorithmsContractTest, LowerBoundReturnsInsertionPositionBetweenValues)
{
    constexpr std::array values{1, 3, 5, 7};
    const auto position = artifactLowerBound(values.begin(), values.end(), 4);
    EXPECT_EQ(position - values.begin(), 2);
    EXPECT_EQ(*position, 5);
}

TEST(ArtifactAlgorithmsContractTest, SortMatchesStandardSortForExhaustiveSmallInputs)
{
    for (std::size_t length = 0; length <= 6; ++length) {
        std::size_t combinations = 1;
        for (std::size_t i = 0; i < length; ++i) combinations *= 4;

        for (std::size_t encoded = 0; encoded < combinations; ++encoded) {
            std::vector<int> actual(length);
            std::size_t digits = encoded;
            for (auto& value : actual) {
                value = static_cast<int>(digits % 4) - 1;
                digits /= 4;
            }

            auto expected = actual;
            artifactSort(actual.begin(), actual.end());
            std::sort(expected.begin(), expected.end());
            EXPECT_EQ(actual, expected) << "length=" << length << " encoded=" << encoded;
        }
    }
}

TEST(ArtifactAlgorithmsContractTest, SortedSearchMatchesStandardAlgorithmsForExhaustiveInputs)
{
    for (std::size_t length = 0; length <= 5; ++length) {
        std::size_t combinations = 1;
        for (std::size_t i = 0; i < length; ++i) combinations *= 4;

        for (std::size_t encoded = 0; encoded < combinations; ++encoded) {
            std::vector<int> values(length);
            std::size_t digits = encoded;
            for (auto& value : values) {
                value = static_cast<int>(digits % 4) - 1;
                digits /= 4;
            }
            std::sort(values.begin(), values.end());

            for (int query = -2; query <= 4; ++query) {
                const auto actual = artifactLowerBound(values.begin(), values.end(), query);
                const auto expected = std::lower_bound(values.begin(), values.end(), query);
                EXPECT_EQ(actual - values.begin(), expected - values.begin());
                EXPECT_EQ(artifactBinarySearch(values.begin(), values.end(), query),
                          std::binary_search(values.begin(), values.end(), query));
            }
        }
    }
}

TEST(ArtifactAlgorithmsContractTest, ContainerSortSupportsCustomComparator)
{
    std::vector<int> values{1, 7, 2, 9, 3};
    artifactSort(values, [](int left, int right) { return left > right; });
    EXPECT_EQ(values, (std::vector<int>{9, 7, 3, 2, 1}));
    EXPECT_TRUE(std::is_sorted(values.begin(), values.end(), std::greater<int>{}));
}

TEST(ArtifactAlgorithmsContractTest, FindInReturnsMutableIteratorIntoContainer)
{
    std::vector<int> values{4, 8, 12};
    auto found = artifactFindIn(values, 8);
    ASSERT_NE(found, values.end());
    *found = 80;
    EXPECT_EQ(values[1], 80);
    EXPECT_EQ(artifactFindIn(values, 99), values.end());
}

TEST(ArtifactAlgorithmsContractTest, RemoveIfRetainsRelativeOrderAfterAdjacentMatches)
{
    EraseAtArray values{Array<int>{1, 2, 4, 6, 3, 8, 5}};
    EXPECT_EQ(artifactRemoveIf(values, [](int value) { return value % 2 == 0; }), 3u);
    EXPECT_EQ(values.values, (Array<int>{1, 3, 5}));
}

TEST(ArtifactAlgorithmsContractTest, ReverseIsAnInvolutionAcrossLengths)
{
    for (std::size_t length = 0; length <= 16; ++length) {
        std::vector<int> values(length);
        for (std::size_t index = 0; index < length; ++index) {
            values[index] = static_cast<int>(index * 3) - 11;
        }
        const auto original = values;
        artifactReverse(values.begin(), values.end());
        artifactReverse(values.begin(), values.end());
        EXPECT_EQ(values, original) << "length=" << length;
    }
}

