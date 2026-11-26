#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

import Core.ArtifactArray;

using namespace ArtifactCore;

namespace {

struct TrackedValue {
    static inline int liveCount = 0;

    int value = 0;

    TrackedValue() { ++liveCount; }
    explicit TrackedValue(int input) : value(input) { ++liveCount; }
    TrackedValue(const TrackedValue& other) : value(other.value) { ++liveCount; }
    TrackedValue(TrackedValue&& other) noexcept : value(other.value) { ++liveCount; }
    TrackedValue& operator=(const TrackedValue&) = default;
    TrackedValue& operator=(TrackedValue&&) noexcept = default;
    ~TrackedValue() { --liveCount; }

    bool operator==(const TrackedValue&) const = default;
};
struct ConditionallyThrowingValue {
    static inline bool shouldThrow = false;

    explicit ConditionallyThrowingValue(int input) : value(input) {
        if (shouldThrow) throw std::runtime_error("value construction failed");
    }
    ConditionallyThrowingValue(const ConditionallyThrowingValue& other)
        : value(other.value)
    {
        if (shouldThrow) throw std::runtime_error("value copy failed");
    }
    ConditionallyThrowingValue(ConditionallyThrowingValue&& other) noexcept
        : value(other.value) {}
    ConditionallyThrowingValue& operator=(const ConditionallyThrowingValue&) = default;
    ConditionallyThrowingValue& operator=(
        ConditionallyThrowingValue&&) noexcept = default;

    int value;
};

struct alignas(64) OverAlignedValue {
    std::uint64_t value = 0;
};

struct NonDefaultConstructibleValue {
    NonDefaultConstructibleValue() = delete;
};

struct ThrowingCopyValue {
    static inline int liveCount = 0;
    static inline int copiesBeforeThrow = -1;

    int value = 0;

    explicit ThrowingCopyValue(int input) : value(input) { ++liveCount; }
    ThrowingCopyValue(const ThrowingCopyValue& other) : value(other.value) {
        if (copiesBeforeThrow == 0) throw std::runtime_error("copy failure");
        if (copiesBeforeThrow > 0) --copiesBeforeThrow;
        ++liveCount;
    }
    ThrowingCopyValue(ThrowingCopyValue&& other) noexcept(false) : value(other.value) { ++liveCount; }
    ~ThrowingCopyValue() { --liveCount; }
};

struct MoveOnlyThrowingValue {
    static inline int movesBeforeThrow = -1;

    explicit MoveOnlyThrowingValue(int input) : value(input) {}
    MoveOnlyThrowingValue(const MoveOnlyThrowingValue&) = delete;
    MoveOnlyThrowingValue& operator=(const MoveOnlyThrowingValue&) = delete;
    MoveOnlyThrowingValue(MoveOnlyThrowingValue&& other) noexcept(false)
        : value(other.value)
    {
        if (movesBeforeThrow == 0) throw std::runtime_error("move failure");
        if (movesBeforeThrow > 0) --movesBeforeThrow;
        other.value = -1;
    }

    int value;
};

template <typename T>
std::vector<T> toVector(const Array<T>& values)
{
    return {values.begin(), values.end()};
}

} // namespace

TEST(ArtifactArrayContractTest, EmptyAndOptionalAccessorsReportNoElements)
{
    Array<int> values;
    EXPECT_TRUE(values.isEmpty());
    EXPECT_EQ(values.size(), 0u);
    EXPECT_FALSE(values.first().has_value());
    EXPECT_FALSE(values.last().has_value());
    EXPECT_FALSE(values.at(0).has_value());
    EXPECT_EQ(values.value(0, 42), 42);
}

TEST(ArtifactArrayContractTest, AppendPrependAndInsertPreserveOrder)
{
    Array<int> values;
    values.append(2);
    values.prepend(1);
    values.insert(2, 4);
    values.insert(2, 3);
    values.insert(99, 8);
    EXPECT_EQ(toVector(values), (std::vector<int>{1, 2, 3, 4}));
}

TEST(ArtifactArrayContractTest, AppendAllAndConcatenationKeepSourcesIndependent)
{
    Array<int> left{1, 2};
    const Array<int> right{3, 4};
    left.appendAll(right);
    const Array<int> joined = Array<int>{1, 2} + right;
    EXPECT_EQ(toVector(left), (std::vector<int>{1, 2, 3, 4}));
    EXPECT_EQ(toVector(joined), (std::vector<int>{1, 2, 3, 4}));
    EXPECT_EQ(toVector(right), (std::vector<int>{3, 4}));
}

TEST(ArtifactArrayContractTest, AppendAllCanAppendArrayToItselfAcrossCapacityGrowth)
{
    Array<int> values{1, 2, 3};
    ASSERT_EQ(values.capacity(), values.size());

    values.appendAll(values);

    EXPECT_EQ(values, (Array<int>{1, 2, 3, 1, 2, 3}));
}

TEST(ArtifactArrayContractTest, AppendCanReadAliasedElementAcrossCapacityGrowth)
{
    Array<std::string> values{"first", "last"};
    ASSERT_EQ(values.capacity(), values.size());
    const std::string& appended = values[0];

    values.append(appended);

    EXPECT_EQ(values, (Array<std::string>{"first", "last", "first"}));
}

TEST(ArtifactArrayContractTest, CopyAndMoveOperationsPreserveElements)
{
    const Array<std::string> original{"alpha", "beta"};
    Array<std::string> copied(original);
    Array<std::string> assigned;
    assigned = original;
    copied[0] = "changed";
    EXPECT_EQ(original[0], "alpha");
    EXPECT_EQ(assigned, original);

    Array<std::string> moved(std::move(copied));
    EXPECT_TRUE(copied.isEmpty());
    EXPECT_EQ(moved[0], "changed");
    Array<std::string> moveAssigned;
    moveAssigned = std::move(moved);
    EXPECT_TRUE(moved.isEmpty());
    EXPECT_EQ(moveAssigned[1], "beta");
}

TEST(ArtifactArrayContractTest, MoveAssignmentReleasesReplacedElementsExactlyOnce)
{
    const int baseline = TrackedValue::liveCount;
    {
        Array<TrackedValue> destination;
        destination.append(TrackedValue{1});
        Array<TrackedValue> source;
        source.append(TrackedValue{7});
        source.append(TrackedValue{9});
        ASSERT_EQ(TrackedValue::liveCount, baseline + 3);

        destination = std::move(source);

        ASSERT_TRUE(source.isEmpty());
        ASSERT_EQ(destination.size(), 2u);
        EXPECT_EQ(destination[0].value, 7);
        EXPECT_EQ(destination[1].value, 9);
        EXPECT_EQ(TrackedValue::liveCount, baseline + 2);
    }
    EXPECT_EQ(TrackedValue::liveCount, baseline);
}

TEST(ArtifactArrayContractTest, MoveOnlyElementsSupportInsertionExtractionAndArrayMove)
{
    Array<std::unique_ptr<int>> values;
    values.append(std::make_unique<int>(1));
    values.append(std::make_unique<int>(3));
    values.insert(1, std::make_unique<int>(2));

    ASSERT_EQ(values.size(), 3u);
    ASSERT_NE(values[0], nullptr);
    ASSERT_NE(values[1], nullptr);
    ASSERT_NE(values[2], nullptr);
    EXPECT_EQ(*values[0], 1);
    EXPECT_EQ(*values[1], 2);
    EXPECT_EQ(*values[2], 3);

    auto extracted = values.takeAt(1);
    ASSERT_NE(extracted, nullptr);
    EXPECT_EQ(*extracted, 2);
    ASSERT_EQ(values.size(), 2u);
    EXPECT_EQ(*values[0], 1);
    EXPECT_EQ(*values[1], 3);

    Array<std::unique_ptr<int>> moved(std::move(values));
    EXPECT_TRUE(values.isEmpty());
    ASSERT_EQ(moved.size(), 2u);
    EXPECT_EQ(*moved[0], 1);
    EXPECT_EQ(*moved[1], 3);
}

TEST(ArtifactArrayContractTest, AtFirstAndLastExposeMutableAndConstReferences)
{
    Array<int> values{5, 9};
    values.at(1).value() = 10;
    values.first().value() = 4;
    const Array<int>& readOnly = values;
    EXPECT_EQ(readOnly.at(1).value(), 10);
    EXPECT_EQ(readOnly.first().value(), 4);
    EXPECT_EQ(readOnly.last().value(), 10);
    EXPECT_FALSE(readOnly.at(2).has_value());
}

TEST(ArtifactArrayContractTest, RemoveAndTakeOperationsHandleEndsAndMiddle)
{
    Array<int> values{1, 2, 3, 4, 5};
    values.removeAt(99);
    values.removeAt(1);
    EXPECT_EQ(values.takeAt(1), 3);
    EXPECT_EQ(values.takeFirst(), 1);
    EXPECT_EQ(values.takeLast(), 5);
    values.removeFirst();
    values.removeLast();
    EXPECT_TRUE(values.isEmpty());
    values.removeFirst();
    values.removeLast();
    EXPECT_TRUE(values.isEmpty());
}

TEST(ArtifactArrayContractTest, RemoveOneAndRemoveAllHandleDuplicateValues)
{
    Array<int> values{2, 1, 2, 3, 2, 4};
    values.removeOne(2);
    EXPECT_EQ(toVector(values), (std::vector<int>{1, 2, 3, 2, 4}));
    values.removeAll(2);
    EXPECT_EQ(toVector(values), (std::vector<int>{1, 3, 4}));
    values.removeAll();
    EXPECT_TRUE(values.isEmpty());
}

TEST(ArtifactArrayContractTest, SearchCountsAndMissingIndicesAreConsistent)
{
    const Array<int> values{3, 1, 3, 2, 3};
    EXPECT_TRUE(values.contains(2));
    EXPECT_FALSE(values.contains(8));
    EXPECT_EQ(values.indexOf(3), 0);
    EXPECT_EQ(values.lastIndexOf(3), 4);
    EXPECT_EQ(values.indexOf(8), -1);
    EXPECT_EQ(values.lastIndexOf(8), -1);
    EXPECT_EQ(values.count(3), 3);
}

TEST(ArtifactArrayContractTest, SlicesAndReverseReturnIndependentArrays)
{
    const Array<int> values{1, 2, 3, 4, 5};
    EXPECT_EQ(toVector(values.mid(1, 3)), (std::vector<int>{2, 3, 4}));
    EXPECT_EQ(toVector(values.mid(4, 9)), (std::vector<int>{5}));
    EXPECT_TRUE(values.mid(5).isEmpty());
    EXPECT_EQ(toVector(values.left(2)), (std::vector<int>{1, 2}));
    EXPECT_EQ(toVector(values.right(2)), (std::vector<int>{4, 5}));
    EXPECT_EQ(toVector(values.reversed()), (std::vector<int>{5, 4, 3, 2, 1}));

    Array<int> slice = values.mid(1, 2);
    slice[0] = 99;
    EXPECT_EQ(values[1], 2);
}

TEST(ArtifactArrayContractTest, SliceBoundariesHandleZeroAndOversizedLengths)
{
    const Array<int> values{4, 5, 6};

    EXPECT_TRUE(values.mid(0, 0).isEmpty());
    EXPECT_TRUE(values.mid(values.size(), 0).isEmpty());
    EXPECT_TRUE(values.left(0).isEmpty());
    EXPECT_EQ(values.left(values.size() + 2), values);
    EXPECT_TRUE(values.right(0).isEmpty());
    EXPECT_EQ(values.right(values.size() + 2), values);
}

TEST(ArtifactArrayContractTest, ResizeAndFillRespectGrowthAndShrink)
{
    Array<int> values{1, 2, 3};
    values.resize(5);
    EXPECT_EQ(toVector(values), (std::vector<int>{1, 2, 3, 0, 0}));
    values.fill(7);
    EXPECT_EQ(toVector(values), (std::vector<int>{7, 7, 7, 7, 7}));
    values.resize(7, 9);
    EXPECT_EQ(toVector(values), (std::vector<int>{7, 7, 7, 7, 7, 9, 9}));
    values.resize(2);
    EXPECT_EQ(toVector(values), (std::vector<int>{7, 7}));
}

TEST(ArtifactArrayContractTest, CapacityAndSwapPreserveOwnedStorageContents)
{
    Array<int> first{1, 2};
    first.reserve(16);
    ASSERT_GE(first.capacity(), 16u);
    Array<int> second{8};
    first.swap(second);
    EXPECT_EQ(toVector(first), (std::vector<int>{8}));
    EXPECT_EQ(toVector(second), (std::vector<int>{1, 2}));
}

TEST(ArtifactArrayContractTest, ComparisonsAndPrefixSuffixHandleEmptyArrays)
{
    const Array<int> values{1, 2, 3};
    EXPECT_EQ(values, (Array<int>{1, 2, 3}));
    EXPECT_NE(values, (Array<int>{1, 2}));
    EXPECT_TRUE(values.startsWith(Array<int>{1, 2}));
    EXPECT_TRUE(values.endsWith(Array<int>{2, 3}));
    EXPECT_FALSE(values.startsWith(Array<int>{2}));
    EXPECT_FALSE(values.endsWith(Array<int>{2}));
    EXPECT_TRUE(values.startsWith(Array<int>{}));
    EXPECT_TRUE(values.endsWith(Array<int>{}));
}

TEST(ArtifactArrayContractTest, PrefixAndSuffixLongerThanArrayAreRejected)
{
    const Array<int> values{1, 2};

    EXPECT_FALSE(values.startsWith(Array<int>{1, 2, 3}));
    EXPECT_FALSE(values.endsWith(Array<int>{0, 1, 2}));
}

TEST(ArtifactArrayContractTest, StaticArraySupportsConstexprAccessAndFill)
{
    constexpr StaticArray<int, 3> initial{{1, 2, 3}};
    static_assert(initial.size() == 3);
    static_assert(!initial.isEmpty());
    static_assert(initial[1] == 2);
    StaticArray<int, 3> values;
    values.fill(6);
    EXPECT_EQ(values[0], 6);
    EXPECT_EQ(values[2], 6);
    EXPECT_EQ(values.end() - values.begin(), 3);
}

TEST(ArtifactArrayContractTest, ZeroLengthStaticArrayIsEmptyAndNeedsNoElementConstruction)
{
    constexpr StaticArray<int, 0> empty{};
    static_assert(empty.size() == 0);
    static_assert(empty.isEmpty());
    static_assert(empty.begin() == empty.end());

    StaticArray<NonDefaultConstructibleValue, 0> noElements{};
    EXPECT_EQ(noElements.size(), 0u);
    EXPECT_TRUE(noElements.isEmpty());
    EXPECT_EQ(noElements.begin(), noElements.end());
}

TEST(ArtifactArrayContractTest, NonTrivialElementsRemainLiveExactlyOnceAcrossMutations)
{
    const int baseline = TrackedValue::liveCount;
    {
        Array<TrackedValue> values;
        values.append(TrackedValue{1});
        values.append(TrackedValue{2});
        values.reserve(24);
        values.insert(1, TrackedValue{3});
        values.prepend(TrackedValue{0});
        ASSERT_EQ(values.size(), 4u);
        EXPECT_EQ(values[0].value, 0);
        EXPECT_EQ(values[1].value, 1);
        EXPECT_EQ(values[2].value, 3);
        EXPECT_EQ(values[3].value, 2);

        values.removeAt(2);
        EXPECT_EQ(values.size(), 3u);
        values.resize(5);
        EXPECT_EQ(values.size(), 5u);
        values.removeLast();
        values.removeAll();
        EXPECT_TRUE(values.isEmpty());
        EXPECT_EQ(TrackedValue::liveCount, baseline);
    }
    EXPECT_EQ(TrackedValue::liveCount, baseline);
}

TEST(ArtifactArrayContractTest, TakeAtTransfersValueAndBalancesShiftedLifetimes)
{
    const int baseline = TrackedValue::liveCount;
    {
        Array<TrackedValue> values;
        values.append(TrackedValue{1});
        values.append(TrackedValue{2});
        values.append(TrackedValue{3});
        ASSERT_EQ(TrackedValue::liveCount, baseline + 3);

        {
            TrackedValue extracted = values.takeAt(1);
            EXPECT_EQ(extracted.value, 2);
            ASSERT_EQ(values.size(), 2u);
            EXPECT_EQ(values[0].value, 1);
            EXPECT_EQ(values[1].value, 3);
            EXPECT_EQ(TrackedValue::liveCount, baseline + 3);
        }
        EXPECT_EQ(TrackedValue::liveCount, baseline + 2);
    }
    EXPECT_EQ(TrackedValue::liveCount, baseline);
}

TEST(ArtifactArrayContractTest, RemoveAllMatchingValuesDestroysThemExactlyOnce)
{
    const int baseline = TrackedValue::liveCount;
    {
        TrackedValue target(2);
        Array<TrackedValue> values;
        values.append(TrackedValue{1});
        values.append(TrackedValue{2});
        values.append(TrackedValue{2});
        values.append(TrackedValue{3});
        ASSERT_EQ(TrackedValue::liveCount, baseline + 5);

        values.removeAll(target);

        ASSERT_EQ(values.size(), 2u);
        EXPECT_EQ(values[0].value, 1);
        EXPECT_EQ(values[1].value, 3);
        EXPECT_EQ(TrackedValue::liveCount, baseline + 3);
    }
    EXPECT_EQ(TrackedValue::liveCount, baseline);
}

TEST(ArtifactArrayContractTest, ReplaceUpdatesOnlyAnExistingIndex)
{
    Array<std::string> values{"first", "middle", "last"};
    values.replace(1, "updated");
    values.replace(3, "out of range");
    EXPECT_EQ(values.size(), 3u);
    EXPECT_EQ(values[0], "first");
    EXPECT_EQ(values[1], "updated");
    EXPECT_EQ(values[2], "last");
}

TEST(ArtifactArrayContractTest, ReplaceKeepsArrayStorageAndElementAddressStable)
{
    Array<std::string> values{"first", "middle", "last"};
    values.reserve(12);
    std::string* const storage = values.data();
    std::string* const element = &values[1];

    values.replace(1, "updated");

    EXPECT_EQ(values.data(), storage);
    EXPECT_EQ(&values[1], element);
    EXPECT_EQ(*element, "updated");
    EXPECT_EQ(values.capacity(), 12u);
}

TEST(ArtifactArrayContractTest, ReplaceCanReadReplacementValueFromSameElement)
{
    Array<std::string> values{"first", "middle", "last"};
    const std::string& replacement = values[1];

    values.replace(1, replacement);

    EXPECT_EQ(values[1], "middle");
    EXPECT_EQ(values.size(), 3u);
}

TEST(ArtifactArrayContractTest, InsertCanReadAliasedElementAfterShifting)
{
    Array<std::string> values{"first", "middle", "last"};
    values.reserve(8);
    const std::string& inserted = values[2];

    values.insert(1, inserted);

    EXPECT_EQ(values, (Array<std::string>{"first", "last", "middle", "last"}));
}

TEST(ArtifactArrayContractTest,
     InsertCanReadAliasedElementWhileCapacityGrowthAndShiftingOverlap)
{
    Array<std::string> values{"first", "middle", "last"};
    ASSERT_EQ(values.capacity(), values.size());
    const std::string& inserted = values[2];

    values.insert(1, inserted);

    EXPECT_EQ(values,
              (Array<std::string>{"first", "last", "middle", "last"}));
    EXPECT_GT(values.capacity(), 3u);
}

TEST(ArtifactArrayContractTest,
     ThrowingAppendAtCapacityBoundaryPreservesElementsAndAllowsRetry)
{
    Array<ConditionallyThrowingValue> values;
    int nextValue = 0;
    while (values.capacity() == 0 || values.size() < values.capacity()) {
        values.append(ConditionallyThrowingValue{nextValue++});
    }
    ASSERT_EQ(values.size(), values.capacity());
    const std::size_t fullCapacity = values.capacity();

    const ConditionallyThrowingValue candidate{nextValue};
    ConditionallyThrowingValue::shouldThrow = true;
    EXPECT_THROW(values.append(candidate), std::runtime_error);
    ConditionallyThrowingValue::shouldThrow = false;

    ASSERT_EQ(values.size(), fullCapacity);
    for (std::size_t index = 0; index < values.size(); ++index)
        EXPECT_EQ(values[index].value, static_cast<int>(index));

    values.append(candidate);
    ASSERT_EQ(values.size(), fullCapacity + 1);
    EXPECT_GT(values.capacity(), fullCapacity);
    for (std::size_t index = 0; index < fullCapacity; ++index)
        EXPECT_EQ(values[index].value, static_cast<int>(index));
    EXPECT_EQ(values[fullCapacity].value, nextValue);
}

TEST(ArtifactArrayContractTest, InsertAtBeginningAndEndAcceptsBoundaryIndices)
{
    Array<int> values{2, 3};
    values.insert(0, 1);
    values.insert(values.size(), 4);
    EXPECT_EQ(toVector(values), (std::vector<int>{1, 2, 3, 4}));
}

TEST(ArtifactArrayContractTest, ReserveDoesNotChangeSizeOrElements)
{
    Array<int> values{4, 5, 6};
    values.reserve(32);
    ASSERT_GE(values.capacity(), 32u);
    EXPECT_EQ(values.size(), 3u);
    EXPECT_EQ(toVector(values), (std::vector<int>{4, 5, 6}));
    const auto reservedCapacity = values.capacity();
    values.reserve(8);
    EXPECT_EQ(values.capacity(), reservedCapacity);
}

TEST(ArtifactArrayContractTest, FailedReservePreservesElementsAndReleasesPartialCopies)
{
    const int baseline = ThrowingCopyValue::liveCount;
    {
        Array<ThrowingCopyValue> values;
        values.append(ThrowingCopyValue{3});
        values.append(ThrowingCopyValue{7});
        const auto originalCapacity = values.capacity();
        ThrowingCopyValue::copiesBeforeThrow = 1;

        EXPECT_THROW(values.reserve(originalCapacity + 8), std::runtime_error);

        ThrowingCopyValue::copiesBeforeThrow = -1;
        ASSERT_EQ(values.size(), 2u);
        EXPECT_EQ(values.capacity(), originalCapacity);
        EXPECT_EQ(values[0].value, 3);
        EXPECT_EQ(values[1].value, 7);
        EXPECT_EQ(ThrowingCopyValue::liveCount, baseline + 2);
    }
    EXPECT_EQ(ThrowingCopyValue::liveCount, baseline);
}

TEST(ArtifactArrayContractTest, FailedInsertPreservesElementsStorageAndLifetimes)
{
    const int baseline = ThrowingCopyValue::liveCount;
    {
        Array<ThrowingCopyValue> values;
        values.reserve(8);
        values.append(ThrowingCopyValue{3});
        values.append(ThrowingCopyValue{7});
        values.append(ThrowingCopyValue{11});
        const auto* const originalData = values.data();
        const auto originalCapacity = values.capacity();
        const ThrowingCopyValue candidate{5};

        ThrowingCopyValue::copiesBeforeThrow = 2;
        EXPECT_THROW(values.insert(1, candidate), std::runtime_error);
        ThrowingCopyValue::copiesBeforeThrow = -1;

        ASSERT_EQ(values.size(), 3u);
        EXPECT_EQ(values.data(), originalData);
        EXPECT_EQ(values.capacity(), originalCapacity);
        EXPECT_EQ(values[0].value, 3);
        EXPECT_EQ(values[1].value, 7);
        EXPECT_EQ(values[2].value, 11);
        EXPECT_EQ(ThrowingCopyValue::liveCount, baseline + 4);
    }
    EXPECT_EQ(ThrowingCopyValue::liveCount, baseline);
}

TEST(ArtifactArrayContractTest, FailedReplacePreservesElementsStorageAndLifetimes)
{
    const int baseline = ThrowingCopyValue::liveCount;
    {
        Array<ThrowingCopyValue> values;
        values.reserve(8);
        values.append(ThrowingCopyValue{3});
        values.append(ThrowingCopyValue{7});
        values.append(ThrowingCopyValue{11});
        const auto* const originalData = values.data();
        const auto originalCapacity = values.capacity();
        const ThrowingCopyValue candidate{5};

        ThrowingCopyValue::copiesBeforeThrow = 2;
        EXPECT_THROW(values.replace(1, candidate), std::runtime_error);
        ThrowingCopyValue::copiesBeforeThrow = -1;

        ASSERT_EQ(values.size(), 3u);
        EXPECT_EQ(values.data(), originalData);
        EXPECT_EQ(values.capacity(), originalCapacity);
        EXPECT_EQ(values[0].value, 3);
        EXPECT_EQ(values[1].value, 7);
        EXPECT_EQ(values[2].value, 11);
        EXPECT_EQ(ThrowingCopyValue::liveCount, baseline + 4);
    }
    EXPECT_EQ(ThrowingCopyValue::liveCount, baseline);
}

TEST(ArtifactArrayContractTest, FailedRemovePreservesElementsStorageAndLifetimes)
{
    const int baseline = ThrowingCopyValue::liveCount;
    {
        Array<ThrowingCopyValue> values;
        values.reserve(8);
        values.append(ThrowingCopyValue{3});
        values.append(ThrowingCopyValue{7});
        values.append(ThrowingCopyValue{11});
        const auto* const originalData = values.data();
        const auto originalCapacity = values.capacity();

        ThrowingCopyValue::copiesBeforeThrow = 1;
        EXPECT_THROW(values.removeAt(1), std::runtime_error);
        ThrowingCopyValue::copiesBeforeThrow = -1;

        ASSERT_EQ(values.size(), 3u);
        EXPECT_EQ(values.data(), originalData);
        EXPECT_EQ(values.capacity(), originalCapacity);
        EXPECT_EQ(values[0].value, 3);
        EXPECT_EQ(values[1].value, 7);
        EXPECT_EQ(values[2].value, 11);
        EXPECT_EQ(ThrowingCopyValue::liveCount, baseline + 3);
    }
    EXPECT_EQ(ThrowingCopyValue::liveCount, baseline);
}

TEST(ArtifactArrayContractTest,
     MoveOnlyThrowingElementsKeepArrayValidWhenInsertRelocationFails)
{
    Array<MoveOnlyThrowingValue> values;
    values.reserve(8);
    values.append(MoveOnlyThrowingValue{3});
    values.append(MoveOnlyThrowingValue{7});
    values.append(MoveOnlyThrowingValue{11});
    MoveOnlyThrowingValue candidate{5};

    MoveOnlyThrowingValue::movesBeforeThrow = 1;
    EXPECT_THROW(values.insert(1, std::move(candidate)), std::runtime_error);
    MoveOnlyThrowingValue::movesBeforeThrow = -1;

    ASSERT_EQ(values.size(), 3u);
    EXPECT_EQ(values[0].value, 3);
    EXPECT_EQ(values[1].value, 7);
    EXPECT_EQ(values[2].value, 11);
}

TEST(ArtifactArrayContractTest, RemoveAllRetainsCapacityForSubsequentAppends)
{
    Array<int> values{4, 5, 6};
    values.reserve(16);
    ASSERT_GE(values.capacity(), 16u);
    const auto reservedCapacity = values.capacity();
    const int* const storage = values.data();

    values.removeAll();

    EXPECT_TRUE(values.isEmpty());
    EXPECT_EQ(values.capacity(), reservedCapacity);
    EXPECT_EQ(values.data(), storage);

    values.append(9);
    values.append(12);

    EXPECT_EQ(values.data(), storage);
    EXPECT_EQ(toVector(values), (std::vector<int>{9, 12}));
}

TEST(ArtifactArrayContractTest, SqueezeReleasesExcessCapacityWithoutChangingElements)
{
    Array<int> values{4, 5, 6};
    values.reserve(32);
    ASSERT_GE(values.capacity(), 32u);

    values.squeeze();

    EXPECT_EQ(values.capacity(), values.size());
    EXPECT_EQ(toVector(values), (std::vector<int>{4, 5, 6}));
}

TEST(ArtifactArrayContractTest, SqueezeReleasesCapacityOfEmptyArray)
{
    Array<int> values;
    values.reserve(16);
    ASSERT_GE(values.capacity(), 16u);
    values.removeAll();

    values.squeeze();

    EXPECT_TRUE(values.isEmpty());
    EXPECT_EQ(values.capacity(), 0u);
    EXPECT_EQ(values.data(), nullptr);
}

TEST(ArtifactArrayContractTest, StorageRespectsOverAlignedElementRequirements)
{
    Array<OverAlignedValue> values;
    values.append(OverAlignedValue{42});
    values.reserve(12);

    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(values.data()) % alignof(OverAlignedValue), 0u);
    EXPECT_EQ(values[0].value, 42u);

    Array<OverAlignedValue> copied(values);
    ASSERT_EQ(copied.size(), 1u);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(copied.data()) % alignof(OverAlignedValue), 0u);

    values.squeeze();
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(values.data()) % alignof(OverAlignedValue), 0u);
}

TEST(ArtifactArrayContractTest, FindReturnsMutableReferenceToStoredElement)
{
    Array<int> values{10, 20, 30};
    auto found = values.find(20);
    ASSERT_TRUE(found.has_value());
    found.value() = 25;
    EXPECT_EQ(values[1], 25);
    EXPECT_FALSE(values.find(99).has_value());
}

TEST(ArtifactArrayContractTest, ConstDataAndIterationExposeReadOnlyStorage)
{
    const Array<int> values{3, 6, 9};
    static_assert(std::is_same_v<decltype(values.data()), const int*>);
    static_assert(std::is_same_v<decltype(values.constData()), const int*>);
    EXPECT_EQ(values.data(), values.constData());
    EXPECT_EQ(values.constEnd() - values.constBegin(), 3);
    EXPECT_EQ(toVector(values), (std::vector<int>{3, 6, 9}));
}

TEST(ArtifactArrayContractTest,
     MixedDeterministicMutationsMatchIndependentVectorModel)
{
    Array<int> actual;
    std::vector<int> expected;
    std::mt19937 generator(0xA22A91u);
    std::uniform_int_distribution<int> operationDistribution(0, 7);
    std::uniform_int_distribution<int> valueDistribution(-50, 50);

    for (int step = 0; step < 1600; ++step) {
        const int operation = operationDistribution(generator);
        const int value = valueDistribution(generator);
        const std::size_t index = expected.empty()
            ? 0u
            : static_cast<std::size_t>(generator()) % (expected.size() + 1u);

        switch (operation) {
        case 0:
            actual.append(value);
            expected.push_back(value);
            break;
        case 1:
            actual.prepend(value);
            expected.insert(expected.begin(), value);
            break;
        case 2:
            actual.insert(index, value);
            expected.insert(expected.begin() + static_cast<std::ptrdiff_t>(index), value);
            break;
        case 3: {
            const std::size_t removeIndex = expected.empty()
                ? 1u
                : static_cast<std::size_t>(generator()) % (expected.size() + 2u);
            actual.removeAt(removeIndex);
            if (removeIndex < expected.size()) {
                expected.erase(expected.begin() +
                               static_cast<std::ptrdiff_t>(removeIndex));
            }
            break;
        }
        case 4:
            if (!expected.empty()) {
                const std::size_t takeIndex =
                    static_cast<std::size_t>(generator()) % expected.size();
                EXPECT_EQ(actual.takeAt(takeIndex), expected[takeIndex])
                    << "step=" << step;
                expected.erase(expected.begin() +
                               static_cast<std::ptrdiff_t>(takeIndex));
            }
            break;
        case 5: {
            const std::size_t replaceIndex = expected.empty()
                ? 0u
                : static_cast<std::size_t>(generator()) % (expected.size() + 1u);
            actual.replace(replaceIndex, value);
            if (replaceIndex < expected.size()) expected[replaceIndex] = value;
            break;
        }
        case 6: {
            actual.removeOne(value);
            const auto found = std::find(expected.begin(), expected.end(), value);
            if (found != expected.end()) expected.erase(found);
            break;
        }
        case 7:
            if (step % 2 == 0)
                actual.reserve(actual.size() + static_cast<std::size_t>(generator() % 9u));
            else
                actual.squeeze();
            break;
        }

        EXPECT_EQ(actual.size(), expected.size()) << "step=" << step;
        EXPECT_EQ(toVector(actual), expected) << "step=" << step;
    }
}

