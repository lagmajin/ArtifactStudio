#include <gtest/gtest.h>

#include <array>
#include <forward_list>
#include <list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

import CppLinq;

using namespace ArtifactCore;

TEST(CppLinqContractTest, AnyFindsMatchingElement)
{
    constexpr std::array values{2, 4, 7, 8};
    EXPECT_TRUE(Any(values, [](int value) { return value % 2 != 0; }));
}

TEST(CppLinqContractTest, AnyReturnsFalseWhenNoElementMatches)
{
    constexpr std::array values{2, 4, 6, 8};
    EXPECT_FALSE(Any(values, [](int value) { return value % 2 != 0; }));
}

TEST(CppLinqContractTest, AnyShortCircuitsAtFirstMatch)
{
    constexpr std::array values{1, 3, 4, 5};
    int calls = 0;
    EXPECT_TRUE(Any(values, [&](int value) { ++calls; return value == 4; }));
    EXPECT_EQ(calls, 3);
}

TEST(CppLinqContractTest, AnyOnEmptyRangeIsFalse)
{
    constexpr std::array<int, 0> values{};
    EXPECT_FALSE(Any(values, [](int) { return true; }));
}

TEST(CppLinqContractTest, AllRequiresEveryElementToMatch)
{
    constexpr std::array values{2, 4, 6, 8};
    EXPECT_TRUE(All(values, [](int value) { return value % 2 == 0; }));
    EXPECT_FALSE(All(values, [](int value) { return value < 6; }));
}

TEST(CppLinqContractTest, AllShortCircuitsAtFirstFailure)
{
    constexpr std::array values{2, 4, 5, 8};
    int calls = 0;
    EXPECT_FALSE(All(values, [&](int value) { ++calls; return value % 2 == 0; }));
    EXPECT_EQ(calls, 3);
}

TEST(CppLinqContractTest, AllOnEmptyRangeIsTrue)
{
    constexpr std::array<int, 0> values{};
    EXPECT_TRUE(All(values, [](int) { return false; }));
}

TEST(CppLinqContractTest, FirstOrDefaultReturnsFirstMatchingValue)
{
    constexpr std::array values{3, 7, 8, 10};
    const auto found = FirstOrDefault(values, [](int value) { return value % 2 == 0; });
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, 8);
}

TEST(CppLinqContractTest, FirstOrDefaultReturnsEmptyWhenNoMatchOrRangeEmpty)
{
    constexpr std::array values{1, 3, 5};
    constexpr std::array<int, 0> empty{};
    EXPECT_FALSE(FirstOrDefault(values, [](int value) { return value > 9; }).has_value());
    EXPECT_FALSE(FirstOrDefault(empty, [](int) { return true; }).has_value());
}

TEST(CppLinqContractTest, WhereFiltersWithoutReorderingElements)
{
    constexpr std::array values{5, 2, 8, 1, 6};
    std::vector<int> result;
    for (const int value : Where(values, [](int item) { return item % 2 == 0; })) {
        result.push_back(value);
    }
    EXPECT_EQ(result, (std::vector<int>{2, 8, 6}));
}

TEST(CppLinqContractTest, WherePredicateIsEvaluatedLazilyDuringIteration)
{
    constexpr std::array values{1, 2, 3, 4};
    int calls = 0;
    const auto view = Where(values, [&](int value) { ++calls; return value > 2; });
    EXPECT_EQ(calls, 0);
    std::vector<int> result;
    for (const int value : view) result.push_back(value);
    EXPECT_EQ(calls, 4);
    EXPECT_EQ(result, (std::vector<int>{3, 4}));
}

TEST(CppLinqContractTest, SelectTransformsValuesLazily)
{
    constexpr std::array values{1, 2, 3};
    int calls = 0;
    const auto view = Select(values, [&](int value) { ++calls; return value * value; });
    EXPECT_EQ(calls, 0);
    std::vector<int> result(view.begin(), view.end());
    EXPECT_EQ(calls, 3);
    EXPECT_EQ(result, (std::vector<int>{1, 4, 9}));
}

TEST(CppLinqContractTest, FirstOrDefaultStopsAfterTheFirstMatch)
{
    constexpr std::array values{2, 4, 6, 8, 10};
    int calls = 0;
    const auto found = FirstOrDefault(values, [&](int value) {
        ++calls;
        return value == 6;
    });
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, 6);
    EXPECT_EQ(calls, 3);
}

TEST(CppLinqContractTest, WhereProducesNoValuesForEmptyOrUnmatchedRanges)
{
    constexpr std::array<int, 0> empty{};
    constexpr std::array values{1, 3, 5};
    const auto emptyView = Where(empty, [](int) { return true; });
    const auto unmatchedView = Where(values, [](int value) { return value % 2 == 0; });
    EXPECT_TRUE(emptyView.begin() == emptyView.end());
    EXPECT_TRUE(unmatchedView.begin() == unmatchedView.end());
}

TEST(CppLinqContractTest, SelectOnEmptyRangeDoesNotInvokeTransform)
{
    constexpr std::array<int, 0> empty{};
    int calls = 0;
    const auto view = Select(empty, [&](int value) {
        ++calls;
        return value * 2;
    });
    EXPECT_TRUE(view.begin() == view.end());
    EXPECT_EQ(calls, 0);
}

TEST(CppLinqContractTest, AnyOnEmptyRangeDoesNotInvokePredicate)
{
    constexpr std::array<int, 0> empty{};
    int calls = 0;
    EXPECT_FALSE(Any(empty, [&](int) { ++calls; return true; }));
    EXPECT_EQ(calls, 0);
}

TEST(CppLinqContractTest, AllEvaluatesEveryElementWhenAllMatch)
{
    constexpr std::array values{2, 4, 6, 8};
    int calls = 0;
    EXPECT_TRUE(All(values, [&](int value) { ++calls; return value % 2 == 0; }));
    EXPECT_EQ(calls, 4);
}

TEST(CppLinqContractTest, WhereReevaluatesPredicateOnEachTraversal)
{
    constexpr std::array values{1, 2, 3};
    int calls = 0;
    const auto view = Where(values, [&](int value) { ++calls; return value != 2; });
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(std::vector<int>(view.begin(), view.end()), (std::vector<int>{1, 3}));
    EXPECT_EQ(calls, 3);
    EXPECT_EQ(std::vector<int>(view.begin(), view.end()), (std::vector<int>{1, 3}));
    EXPECT_EQ(calls, 6);
}

TEST(CppLinqContractTest, WhereObservesSourceChangesMadeBeforeTraversal)
{
    std::array values{1, 2, 3};
    const auto view = Where(values, [](int value) { return value % 2 == 0; });

    values = {4, 5, 6};

    EXPECT_EQ(std::vector<int>(view.begin(), view.end()), (std::vector<int>{4, 6}));
}

TEST(CppLinqContractTest, WhereAllMatchPreservesTheCompleteRange)
{
    constexpr std::array values{4, 1, 7};
    const auto view = Where(values, [](int) { return true; });
    EXPECT_EQ(std::vector<int>(view.begin(), view.end()), (std::vector<int>{4, 1, 7}));
}

TEST(CppLinqContractTest, SelectCanChangeElementTypeAndPreservesOrder)
{
    constexpr std::array values{3, 1, 4};
    const auto view = Select(values, [](int value) { return std::to_string(value); });
    EXPECT_EQ(std::vector<std::string>(view.begin(), view.end()),
              (std::vector<std::string>{"3", "1", "4"}));
}

TEST(CppLinqContractTest, WhereAndSelectComposeAsLazyOrderedPipeline)
{
    constexpr std::array values{5, 2, 8, 1, 6};
    int predicateCalls = 0;
    int projectionCalls = 0;
    const auto filtered = Where(values, [&](int value) {
        ++predicateCalls;
        return value % 2 == 0;
    });
    const auto projected = Select(filtered, [&](int value) {
        ++projectionCalls;
        return value * 10;
    });

    EXPECT_EQ(predicateCalls, 0);
    EXPECT_EQ(projectionCalls, 0);
    std::vector<int> result;
    for (const int value : projected)
        result.push_back(value);
    EXPECT_EQ(result, (std::vector<int>{20, 80, 60}));
    EXPECT_EQ(predicateCalls, 5);
    EXPECT_EQ(projectionCalls, 3);
}

TEST(CppLinqContractTest, SelectReevaluatesProjectionOnEachTraversal)
{
    constexpr std::array values{2, 3};
    int calls = 0;
    const auto view = Select(values, [&](int value) {
        ++calls;
        return value + calls;
    });

    EXPECT_EQ(std::vector<int>(view.begin(), view.end()), (std::vector<int>{3, 5}));
    EXPECT_EQ(std::vector<int>(view.begin(), view.end()), (std::vector<int>{5, 7}));
    EXPECT_EQ(calls, 4);
}

TEST(CppLinqContractTest, FirstOrDefaultReturnsOwnedValueWithoutChangingSource)
{
    std::array values{2, 5, 8};
    auto found = FirstOrDefault(values, [](int value) { return value == 5; });
    ASSERT_TRUE(found.has_value());
    *found = 50;
    EXPECT_EQ(values[1], 5);
}

TEST(CppLinqContractTest, QueriesAndLazyViewsSupportForwardOnlyContainers)
{
    const std::forward_list<int> values{3, 6, 9, 12};

    EXPECT_TRUE(Any(values, [](int value) { return value == 9; }));
    const auto firstEven = FirstOrDefault(values, [](int value) {
        return value % 2 == 0;
    });
    ASSERT_TRUE(firstEven.has_value());
    EXPECT_EQ(*firstEven, 6);

    const auto doubled = Select(Where(values, [](int value) {
        return value >= 6;
    }), [](int value) {
        return value * 2;
    });
    EXPECT_EQ(std::vector<int>(doubled.begin(), doubled.end()),
              (std::vector<int>{12, 18, 24}));
}

TEST(CppLinqContractTest, WhereSupportsListAndPreservesDuplicateValues)
{
    const std::list<int> values{4, 3, 4, 2, 4};
    const auto selected = Where(values, [](int value) { return value == 4; });
    EXPECT_EQ(std::vector<int>(selected.begin(), selected.end()),
              (std::vector<int>{4, 4, 4}));
}

TEST(CppLinqContractTest, SelectSupportsSpanWithoutOwningTheSource)
{
    std::array values{2, 5, 8};
    const std::span<int> view(values);
    int calls = 0;
    const auto projected = Select(view, [&](int value) {
        ++calls;
        return value + 1;
    });

    EXPECT_EQ(calls, 0);
    EXPECT_EQ(std::vector<int>(projected.begin(), projected.end()),
              (std::vector<int>{3, 6, 9}));
    EXPECT_EQ(calls, 3);
}

TEST(CppLinqContractTest, WhereSupportsStringViewCharacterRanges)
{
    constexpr std::string_view text = "a1b2c3";
    const auto digits = Where(text, [](char value) {
        return value >= '0' && value <= '9';
    });
    EXPECT_EQ(std::string(digits.begin(), digits.end()), "123");
}

TEST(CppLinqContractTest, AnyAndFirstOrDefaultSupportVectorBoolRanges)
{
    const std::vector<bool> values{false, false, true, false};
    EXPECT_TRUE(Any(values, [](bool value) { return value; }));

    auto first = FirstOrDefault(values, [](bool value) { return value; });
    ASSERT_TRUE(first.has_value());
    EXPECT_TRUE(*first);
    *first = false;
    EXPECT_TRUE(values[2]);
}

TEST(CppLinqContractTest, SelectOnStringViewCanChangeCharacterType)
{
    constexpr std::string_view text = "abc";
    const auto codes = Select(text, [](char value) {
        return static_cast<int>(value);
    });
    EXPECT_EQ(std::vector<int>(codes.begin(), codes.end()),
              (std::vector<int>{'a', 'b', 'c'}));
}
