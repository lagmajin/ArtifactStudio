#include <gtest/gtest.h>

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

TEST(ArtifactArrayContractTest, SqueezeReleasesExcessCapacityWithoutChangingElements)
{
    Array<int> values{4, 5, 6};
    values.reserve(32);
    ASSERT_GE(values.capacity(), 32u);

    values.squeeze();

    EXPECT_EQ(values.capacity(), values.size());
    EXPECT_EQ(toVector(values), (std::vector<int>{4, 5, 6}));
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

