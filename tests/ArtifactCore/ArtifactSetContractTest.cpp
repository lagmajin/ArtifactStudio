#include <gtest/gtest.h>

#include <cstddef>
#include <random>
#include <type_traits>
#include <unordered_set>
#include <utility>

import Core.ArtifactSet;

using namespace ArtifactCore;

static_assert(std::is_same_v<decltype(*std::declval<HashSet<int>&>().begin()), const int&>);
static_assert(std::is_same_v<decltype(*std::declval<const HashSet<int>&>().begin()), const int&>);

namespace {

struct ConstantHash {
    std::size_t operator()(int) const noexcept { return 1; }
};

struct ModuloTenEqual {
    bool operator()(int left, int right) const noexcept {
        return left % 10 == right % 10;
    }
};

struct TrackedValue {
    static inline int liveCount = 0;
    int value;

    explicit TrackedValue(int input) : value(input) { ++liveCount; }
    TrackedValue(const TrackedValue& other) : value(other.value) { ++liveCount; }
    TrackedValue(TrackedValue&& other) noexcept : value(other.value) { ++liveCount; }
    ~TrackedValue() { --liveCount; }
};

struct TrackedValueHash {
    std::size_t operator()(const TrackedValue& value) const noexcept {
        return static_cast<std::size_t>(value.value);
    }
};

struct TrackedValueEqual {
    bool operator()(const TrackedValue& left, const TrackedValue& right) const noexcept {
        return left.value == right.value;
    }
};

template <typename T, typename Hasher>
Array<T> toArray(const HashSet<T, Hasher>& values)
{
    Array<T> result;
    for (const T& value : values) result.append(value);
    return result;
}

} // namespace

TEST(ArtifactSetContractTest, EmptyAndMissingLookupsHaveEmptyResults)
{
    HashSet<int> values;
    EXPECT_TRUE(values.isEmpty());
    EXPECT_EQ(values.size(), 0u);
    EXPECT_FALSE(values.contains(1));
    EXPECT_FALSE(values.find(1).has_value());
    EXPECT_TRUE(values.values().isEmpty());
    EXPECT_EQ(values.begin(), values.end());
}

TEST(ArtifactSetContractTest, AddAndInsertRejectDuplicatesWithoutChangingSize)
{
    HashSet<int> values;
    EXPECT_TRUE(values.add(4));
    EXPECT_FALSE(values.insert(4));
    EXPECT_TRUE(values.insert(8));
    EXPECT_FALSE(values.add(8));
    EXPECT_EQ(values.size(), 2u);
}

TEST(ArtifactSetContractTest, FindReturnsAReferenceToTheStoredValue)
{
    HashSet<int> values;
    values.add(17);
    const auto found = values.find(17);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found.value(), 17);
}

TEST(ArtifactSetContractTest, IterationAndValuesUseInsertionOrder)
{
    HashSet<int> values;
    values.add(10);
    values.add(20);
    values.add(30);
    EXPECT_EQ(toArray(values), (Array<int>{30, 20, 10}));
    const Array<int> ordered = values.values();
    ASSERT_EQ(ordered.size(), 3u);
    EXPECT_EQ(ordered[0], 10);
    EXPECT_EQ(ordered[1], 20);
    EXPECT_EQ(ordered[2], 30);
}

TEST(ArtifactSetContractTest, CollisionChainsSupportFindRemoveAndRemainingIteration)
{
    HashSet<int, ConstantHash> values;
    values.add(1);
    values.add(2);
    values.add(3);
    values.add(4);
    EXPECT_EQ(toArray(values), (Array<int>{4, 3, 2, 1}));
    EXPECT_TRUE(values.remove(3));
    EXPECT_FALSE(values.contains(3));
    EXPECT_EQ(toArray(values), (Array<int>{4, 2, 1}));
    EXPECT_TRUE(values.erase(1));
    EXPECT_FALSE(values.erase(1));
    EXPECT_EQ(toArray(values), (Array<int>{4, 2}));
}

TEST(ArtifactSetContractTest, RehashPreservesAllValuesAndIterationOrder)
{
    HashSet<int> values;
    for (int value = 0; value < 200; ++value) values.add(value);
    ASSERT_EQ(values.size(), 200u);
    EXPECT_TRUE(values.contains(0));
    EXPECT_TRUE(values.contains(199));
    for (int value = 0; value < 200; ++value) {
        ASSERT_TRUE(values.contains(value));
    }
    EXPECT_EQ(*values.begin(), 199);
    EXPECT_EQ(values.values()[0], 0);
}

TEST(ArtifactSetContractTest, CopyAndAssignmentPreserveIndependentMembership)
{
    HashSet<int> source;
    source.add(1);
    source.add(2);
    source.add(3);
    HashSet<int> copied(source);
    HashSet<int> assigned;
    assigned = source;
    EXPECT_EQ(toArray(copied), toArray(source));
    EXPECT_EQ(toArray(assigned), toArray(source));
    copied.remove(2);
    EXPECT_TRUE(source.contains(2));
    EXPECT_FALSE(copied.contains(2));
}

TEST(ArtifactSetContractTest, MoveConstructionAndAssignmentLeaveReusableSources)
{
    HashSet<int> source;
    source.add(5);
    source.add(6);
    HashSet<int> moved(std::move(source));
    EXPECT_EQ(toArray(moved), (Array<int>{6, 5}));
    EXPECT_TRUE(source.isEmpty());
    EXPECT_TRUE(source.add(7));
    EXPECT_TRUE(source.contains(7));

    HashSet<int> destination;
    destination.add(99);
    destination = std::move(moved);
    EXPECT_EQ(toArray(destination), (Array<int>{6, 5}));
    EXPECT_TRUE(moved.isEmpty());
    EXPECT_TRUE(moved.add(8));
    EXPECT_TRUE(moved.contains(8));
}

TEST(ArtifactSetContractTest, ClearIsIdempotentAndSetCanBeReused)
{
    HashSet<int> values;
    values.add(1);
    values.add(2);
    values.clear();
    values.clear();
    EXPECT_TRUE(values.isEmpty());
    EXPECT_TRUE(values.add(3));
    EXPECT_TRUE(values.contains(3));
}

TEST(ArtifactSetContractTest, SwapExchangesMembershipAndSupportsEmptySets)
{
    HashSet<int> populated;
    populated.add(1);
    populated.add(2);
    HashSet<int> empty;
    populated.swap(empty);
    EXPECT_TRUE(populated.isEmpty());
    EXPECT_EQ(toArray(empty), (Array<int>{2, 1}));
    empty.swap(empty);
    EXPECT_EQ(toArray(empty), (Array<int>{2, 1}));
}

TEST(ArtifactSetContractTest, CustomEqualityDefinesDuplicateAndLookupIdentity)
{
    HashSet<int, ConstantHash, ModuloTenEqual> values;
    EXPECT_TRUE(values.add(12));
    EXPECT_FALSE(values.add(22));
    EXPECT_TRUE(values.contains(32));
    const auto found = values.find(42);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found.value(), 12);
    EXPECT_EQ(values.size(), 1u);
}

TEST(ArtifactSetContractTest, CustomEqualityDistinguishesMultipleCollisionChainClasses)
{
    HashSet<int, ConstantHash, ModuloTenEqual> values;
    EXPECT_TRUE(values.add(12));
    EXPECT_TRUE(values.add(13));
    EXPECT_TRUE(values.add(14));
    EXPECT_FALSE(values.add(22));
    EXPECT_FALSE(values.add(33));
    EXPECT_FALSE(values.add(44));
    EXPECT_TRUE(values.contains(32));
    EXPECT_TRUE(values.contains(43));
    EXPECT_EQ(toArray(values), (Array<int>{14, 13, 12}));
    EXPECT_TRUE(values.remove(23));
    EXPECT_FALSE(values.contains(13));
    EXPECT_TRUE(values.contains(12));
    EXPECT_TRUE(values.contains(14));
    EXPECT_EQ(toArray(values), (Array<int>{14, 12}));
}

TEST(ArtifactSetContractTest, ReferencesRemainStableWhenBucketsRehash)
{
    HashSet<int> values;
    values.add(7);
    const auto initial = values.find(7);
    ASSERT_TRUE(initial.has_value());
    const int* address = &initial.value();
    for (int value = 0; value < 200; ++value) values.add(value + 100);
    const auto afterGrowth = values.find(7);
    ASSERT_TRUE(afterGrowth.has_value());
    EXPECT_EQ(&afterGrowth.value(), address);
    EXPECT_EQ(afterGrowth.value(), 7);
}

TEST(ArtifactSetContractTest, IteratorPostIncrementReturnsPreviousPosition)
{
    HashSet<int> values;
    values.add(1);
    values.add(2);
    values.add(3);
    auto iterator = values.begin();
    const auto previous = iterator++;
    EXPECT_EQ(*previous, 3);
    EXPECT_EQ(*iterator, 2);
    EXPECT_EQ(*iterator.operator->(), 2);
}

TEST(ArtifactSetContractTest, RemovingHeadAndFinalElementUpdatesEmptyState)
{
    HashSet<int> values;
    values.add(10);
    values.add(20);
    ASSERT_TRUE(values.remove(20));
    ASSERT_EQ(*values.begin(), 10);
    ASSERT_TRUE(values.remove(10));
    EXPECT_TRUE(values.isEmpty());
    EXPECT_EQ(values.begin(), values.end());
}

TEST(ArtifactSetContractTest, SelfCopyAssignmentPreservesAllMembership)
{
    HashSet<int> values;
    values.add(2);
    values.add(4);
    values = values;
    EXPECT_EQ(values.size(), 2u);
    EXPECT_TRUE(values.contains(2));
    EXPECT_TRUE(values.contains(4));
}

TEST(ArtifactSetContractTest, SwapExchangesTwoPopulatedSets)
{
    HashSet<int> left;
    left.add(1);
    left.add(2);
    HashSet<int> right;
    right.add(8);
    right.add(9);
    left.swap(right);
    EXPECT_TRUE(left.contains(8));
    EXPECT_TRUE(left.contains(9));
    EXPECT_FALSE(left.contains(1));
    EXPECT_TRUE(right.contains(1));
    EXPECT_TRUE(right.contains(2));
    EXPECT_FALSE(right.contains(8));
}

TEST(ArtifactSetContractTest, SelfMoveAssignmentPreservesMembership)
{
    HashSet<int> values;
    values.add(6);
    values.add(7);
    values = std::move(values);
    EXPECT_EQ(values.size(), 2u);
    EXPECT_TRUE(values.contains(6));
    EXPECT_TRUE(values.contains(7));
}

TEST(ArtifactSetContractTest, RemoveDestroysStoredValueExactlyOnce)
{
    const int baseline = TrackedValue::liveCount;
    {
        TrackedValue key(41);
        HashSet<TrackedValue, TrackedValueHash, TrackedValueEqual> values;
        ASSERT_TRUE(values.add(key));
        EXPECT_EQ(TrackedValue::liveCount, baseline + 2);

        EXPECT_TRUE(values.remove(key));
        EXPECT_EQ(TrackedValue::liveCount, baseline + 1);
        EXPECT_FALSE(values.remove(key));
        EXPECT_EQ(TrackedValue::liveCount, baseline + 1);
    }
    EXPECT_EQ(TrackedValue::liveCount, baseline);
}

TEST(ArtifactSetContractTest, ClearDestroysStoredValuesAndAllowsReuse)
{
    const int baseline = TrackedValue::liveCount;
    {
        TrackedValue first(1);
        TrackedValue second(2);
        HashSet<TrackedValue, TrackedValueHash, TrackedValueEqual> values;
        ASSERT_TRUE(values.add(first));
        ASSERT_TRUE(values.add(second));
        ASSERT_EQ(TrackedValue::liveCount, baseline + 4);

        values.clear();
        EXPECT_EQ(TrackedValue::liveCount, baseline + 2);
        EXPECT_TRUE(values.isEmpty());
        ASSERT_TRUE(values.add(first));
        EXPECT_EQ(TrackedValue::liveCount, baseline + 3);
    }
    EXPECT_EQ(TrackedValue::liveCount, baseline);
}

TEST(ArtifactSetContractTest, ValuesReturnsIndependentMembershipSnapshot)
{
    HashSet<int> values;
    values.add(3);
    values.add(5);
    Array<int> snapshot = values.values();

    ASSERT_EQ(snapshot.size(), 2u);
    EXPECT_TRUE(values.remove(3));
    values.add(7);

    EXPECT_EQ(snapshot, (Array<int>{3, 5}));
    EXPECT_EQ(values.values(), (Array<int>{5, 7}));
}

TEST(ArtifactSetContractTest, RemovingAllCollisionChainNodesLeavesReusableSet)
{
    HashSet<int, ConstantHash> values;
    values.add(1);
    values.add(2);
    values.add(3);
    EXPECT_TRUE(values.remove(2));
    EXPECT_TRUE(values.remove(1));
    EXPECT_TRUE(values.remove(3));
    EXPECT_TRUE(values.isEmpty());
    EXPECT_EQ(values.begin(), values.end());
    EXPECT_TRUE(values.add(4));
    EXPECT_TRUE(values.contains(4));
}

TEST(ArtifactSetContractTest, DeterministicOperationSequenceMatchesStandardSetModel)
{
    HashSet<int> actual;
    std::unordered_set<int> expected;
    std::mt19937 generator(0x5E7A11u);
    std::uniform_int_distribution<int> operationDistribution(0, 3);
    std::uniform_int_distribution<int> valueDistribution(-64, 64);

    for (int step = 0; step < 5000; ++step) {
        const int operation = operationDistribution(generator);
        const int value = valueDistribution(generator);

        if (operation == 0) {
            EXPECT_EQ(actual.add(value), expected.insert(value).second) << "step=" << step;
        } else if (operation == 1) {
            EXPECT_EQ(actual.remove(value), expected.erase(value) != 0) << "step=" << step;
        } else if (operation == 2) {
            EXPECT_EQ(actual.contains(value), expected.contains(value)) << "step=" << step;
        } else {
            EXPECT_EQ(actual.find(value).has_value(), expected.contains(value)) << "step=" << step;
        }

        EXPECT_EQ(actual.size(), expected.size()) << "step=" << step;
        for (int candidate = -64; candidate <= 64; ++candidate) {
            EXPECT_EQ(actual.contains(candidate), expected.contains(candidate))
                << "step=" << step << " candidate=" << candidate;
        }
    }
}

