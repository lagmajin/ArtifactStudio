#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>

import Core.ArtifactHashMap;

using namespace ArtifactCore;

namespace {

struct ConstantHash {
    std::size_t operator()(int) const noexcept { return 1; }
};

struct ModuloTenHash {
    std::size_t operator()(int value) const noexcept {
        return static_cast<std::size_t>(value % 10);
    }
};

struct ModuloTenEqual {
    bool operator()(int left, int right) const noexcept {
        return left % 10 == right % 10;
    }
};

struct ConstructionTrackedMapped {
    static inline int directConstructionCount = 0;
    int value;

    explicit ConstructionTrackedMapped(int input) : value(input) {
        ++directConstructionCount;
    }
};

struct DestructionTrackedMapped {
    explicit DestructionTrackedMapped(int& destructionCount)
        : destructionCount(&destructionCount) {}
    ~DestructionTrackedMapped() { ++*destructionCount; }

    int* destructionCount;
};

} // namespace

TEST(ArtifactHashMapContractTest, EmptyMapReportsMissingKeysAndSupportsReuse)
{
    ArtifactHashMap<int, std::string> map;
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0u);
    EXPECT_FALSE(map.contains(5));
    EXPECT_EQ(map.count(5), 0u);
    EXPECT_EQ(map.find(5), map.end());
    EXPECT_EQ(map.findValue(5), nullptr);
    EXPECT_FALSE(map.at(5).has_value());
}

TEST(ArtifactHashMapContractTest, TryEmplaceAndFindExposeInsertedEntry)
{
    ArtifactHashMap<int, std::string> map;
    const auto [inserted, wasInserted] = map.tryEmplace(7, std::string("seven"));
    ASSERT_TRUE(wasInserted);
    EXPECT_EQ(inserted->first, 7);
    EXPECT_EQ(inserted->second, "seven");
    EXPECT_TRUE(map.contains(7));
    EXPECT_EQ(map.count(7), 1u);
    EXPECT_EQ(map.find(7)->second, "seven");
}

TEST(ArtifactHashMapContractTest, TryEmplaceDuplicateKeepsOriginalMappedValue)
{
    ArtifactHashMap<int, std::string> map;
    const auto [original, inserted] = map.tryEmplace(7, std::string("original"));
    ASSERT_TRUE(inserted);

    const auto [duplicate, insertedAgain] =
        map.tryEmplace(7, std::string("replacement"));

    EXPECT_FALSE(insertedAgain);
    EXPECT_EQ(duplicate, original);
    EXPECT_EQ(duplicate->second, "original");
    EXPECT_EQ(map.size(), 1u);
}

TEST(ArtifactHashMapContractTest, TryEmplaceDuplicateDoesNotConstructMappedValue)
{
    const int baseline = ConstructionTrackedMapped::directConstructionCount;
    ArtifactHashMap<int, ConstructionTrackedMapped> map;
    const auto [original, inserted] = map.tryEmplace(7, 12);
    ASSERT_TRUE(inserted);
    ASSERT_EQ(ConstructionTrackedMapped::directConstructionCount, baseline + 1);

    const auto [duplicate, insertedAgain] = map.tryEmplace(7, 99);

    EXPECT_FALSE(insertedAgain);
    EXPECT_EQ(duplicate, original);
    EXPECT_EQ(ConstructionTrackedMapped::directConstructionCount, baseline + 1);
    EXPECT_EQ(duplicate->second.value, 12);
}

TEST(ArtifactHashMapContractTest, BracketAccessDefaultInsertsAndThenReturnsStoredValue)
{
    ArtifactHashMap<int, int> map;
    EXPECT_EQ(map[3], 0);
    EXPECT_EQ(map.size(), 1u);
    map[3] = 42;
    EXPECT_EQ(map[3], 42);
    EXPECT_EQ(map.size(), 1u);
}

TEST(ArtifactHashMapContractTest, AtAndFindValueExposeMutableAndConstValues)
{
    ArtifactHashMap<int, int> map;
    map.tryEmplace(2, 10);
    map.at(2).value() = 11;
    *map.findValue(2) = 12;
    const auto& readOnly = map;
    EXPECT_EQ(readOnly.at(2).value(), 12);
    EXPECT_EQ(*readOnly.findValue(2), 12);
    EXPECT_FALSE(readOnly.at(9).has_value());
    EXPECT_EQ(readOnly.findValue(9), nullptr);
}

TEST(ArtifactHashMapContractTest, CollisionChainsSupportLookupAndRemoval)
{
    ArtifactHashMap<int, std::string, ConstantHash> map;
    map.tryEmplace(1, std::string("one"));
    map.tryEmplace(2, std::string("two"));
    map.tryEmplace(3, std::string("three"));
    EXPECT_EQ(map.find(1)->second, "one");
    EXPECT_EQ(map.find(2)->second, "two");
    EXPECT_EQ(map.find(3)->second, "three");
    EXPECT_EQ(map.erase(2), 1u);
    EXPECT_FALSE(map.contains(2));
    EXPECT_TRUE(map.contains(1));
    EXPECT_TRUE(map.contains(3));
    EXPECT_EQ(map.erase(1), 1u);
    EXPECT_EQ(map.erase(3), 1u);
    EXPECT_TRUE(map.empty());
}

TEST(ArtifactHashMapContractTest, EraseReturnsZeroForMissingAndOneForExistingKey)
{
    ArtifactHashMap<int, int> map;
    map.tryEmplace(4, 40);
    EXPECT_EQ(map.erase(9), 0u);
    EXPECT_EQ(map.erase(4), 1u);
    EXPECT_EQ(map.erase(4), 0u);
    EXPECT_EQ(map.size(), 0u);
}

TEST(ArtifactHashMapContractTest, RehashPreservesAllEntriesAndUpdates)
{
    ArtifactHashMap<int, int> map;
    for (int key = 0; key < 200; ++key) {
        map.tryEmplace(key, key * 10);
    }
    EXPECT_EQ(map.size(), 200u);
    EXPECT_GT(map.bucket_count(), 16u);
    for (int key = 0; key < 200; ++key) {
        ASSERT_TRUE(map.contains(key));
        EXPECT_EQ(map.at(key).value(), key * 10);
    }
    map[137] = -1;
    EXPECT_EQ(map.at(137).value(), -1);
}

TEST(ArtifactHashMapContractTest, EraseAfterRehashKeepsRemainingEntriesReachable)
{
    ArtifactHashMap<int, int> map;
    for (int key = 0; key < 100; ++key) {
        map.tryEmplace(key, key * 3);
    }
    ASSERT_GT(map.bucket_count(), 16u);

    EXPECT_EQ(map.erase(37), 1u);
    EXPECT_EQ(map.size(), 99u);
    EXPECT_FALSE(map.contains(37));
    EXPECT_EQ(map.at(36).value(), 108);
    EXPECT_EQ(map.at(38).value(), 114);
    EXPECT_EQ(map.erase(37), 0u);
}

TEST(ArtifactHashMapContractTest, IterationVisitsEveryEntryAcrossBuckets)
{
    ArtifactHashMap<int, int> map;
    map.tryEmplace(1, 10);
    map.tryEmplace(2, 20);
    map.tryEmplace(3, 30);
    std::array<bool, 4> keys{};
    int valueSum = 0;
    for (const auto& entry : map) {
        ASSERT_GE(entry.first, 0);
        ASSERT_LT(entry.first, static_cast<int>(keys.size()));
        keys[static_cast<std::size_t>(entry.first)] = true;
        valueSum += entry.second;
    }
    EXPECT_TRUE(keys[1]);
    EXPECT_TRUE(keys[2]);
    EXPECT_TRUE(keys[3]);
    EXPECT_EQ(valueSum, 60);
}

TEST(ArtifactHashMapContractTest, ClearDestroysEntriesAndAllowsSubsequentInsertion)
{
    ArtifactHashMap<int, int> map;
    for (int key = 0; key < 32; ++key) map.tryEmplace(key, key);
    map.clear();
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0u);
    EXPECT_FALSE(map.contains(1));
    map.tryEmplace(99, 101);
    EXPECT_EQ(map.at(99).value(), 101);
}

TEST(ArtifactHashMapContractTest, EraseAndClearDestroyMappedValuesExactlyOnce)
{
    int destructionCount = 0;
    {
        ArtifactHashMap<int, DestructionTrackedMapped> map;
        ASSERT_TRUE(map.tryEmplace(1, destructionCount).second);
        ASSERT_TRUE(map.tryEmplace(2, destructionCount).second);
        ASSERT_TRUE(map.tryEmplace(3, destructionCount).second);

        EXPECT_EQ(map.erase(2), 1u);
        EXPECT_EQ(destructionCount, 1);
        EXPECT_EQ(map.erase(2), 0u);
        EXPECT_EQ(destructionCount, 1);

        map.clear();
        EXPECT_EQ(destructionCount, 3);
        EXPECT_TRUE(map.empty());
        map.clear();
        EXPECT_EQ(destructionCount, 3);
    }
    EXPECT_EQ(destructionCount, 3);
}

TEST(ArtifactHashMapContractTest, EmplaceForwardsMappedValueConstructorArguments)
{
    ArtifactHashMap<int, std::string> map;
    const auto [entry, inserted] = map.emplace(6, std::string(4, 'x'));
    ASSERT_TRUE(inserted);
    EXPECT_EQ(entry->first, 6);
    EXPECT_EQ(entry->second, "xxxx");
}

TEST(ArtifactHashMapContractTest, TryEmplaceMovesMoveOnlyMappedValue)
{
    ArtifactHashMap<int, std::unique_ptr<int>> map;
    auto value = std::make_unique<int>(42);

    const auto [entry, inserted] = map.tryEmplace(6, std::move(value));

    ASSERT_TRUE(inserted);
    EXPECT_FALSE(value);
    ASSERT_TRUE(entry->second);
    EXPECT_EQ(*entry->second, 42);
}

TEST(ArtifactHashMapContractTest, SelfMoveAssignmentPreservesEntries)
{
    ArtifactHashMap<int, std::string> map;
    map.tryEmplace(3, std::string("kept"));
    auto& sameMap = map;

    map = std::move(sameMap);

    ASSERT_EQ(map.size(), 1u);
    ASSERT_NE(map.find(3), map.end());
    EXPECT_EQ(map.find(3)->second, "kept");
}

TEST(ArtifactHashMapContractTest, MoveConstructionTransfersEntries)
{
    ArtifactHashMap<int, std::string> source;
    source.tryEmplace(4, std::string("four"));
    source.tryEmplace(8, std::string("eight"));

    ArtifactHashMap<int, std::string> destination(std::move(source));

    EXPECT_TRUE(source.empty());
    EXPECT_EQ(source.size(), 0u);
    ASSERT_EQ(destination.size(), 2u);
    ASSERT_NE(destination.find(4), destination.end());
    EXPECT_EQ(destination.find(4)->second, "four");
    ASSERT_NE(destination.find(8), destination.end());
    EXPECT_EQ(destination.find(8)->second, "eight");
}

TEST(ArtifactHashMapContractTest, MoveAssignmentReplacesDestinationAndTransfersEntries)
{
    ArtifactHashMap<int, std::string> destination;
    destination.tryEmplace(1, std::string("old"));
    ArtifactHashMap<int, std::string> source;
    source.tryEmplace(2, std::string("two"));
    source.tryEmplace(3, std::string("three"));

    destination = std::move(source);

    EXPECT_TRUE(source.empty());
    EXPECT_EQ(source.size(), 0u);
    ASSERT_EQ(destination.size(), 2u);
    EXPECT_FALSE(destination.contains(1));
    ASSERT_NE(destination.find(2), destination.end());
    EXPECT_EQ(destination.find(2)->second, "two");
    ASSERT_NE(destination.find(3), destination.end());
    EXPECT_EQ(destination.find(3)->second, "three");
}

TEST(ArtifactHashMapContractTest, CustomHashAndEqualityControlKeyIdentity)
{
    ArtifactHashMap<int, std::string, ModuloTenHash, ModuloTenEqual> map;
    const auto [entry, inserted] = map.tryEmplace(12, std::string("first"));
    ASSERT_TRUE(inserted);
    const auto [equivalent, insertedEquivalent] = map.tryEmplace(22, std::string("second"));
    EXPECT_FALSE(insertedEquivalent);
    EXPECT_EQ(entry, equivalent);
    EXPECT_EQ(map.size(), 1u);
    EXPECT_TRUE(map.contains(32));
    EXPECT_EQ(map.find(42)->first, 12);
}

TEST(ArtifactHashMapContractTest, MappedValuePointerRemainsStableAcrossRehash)
{
    ArtifactHashMap<int, int> map;
    map.tryEmplace(7, 70);
    int* originalValue = map.findValue(7);
    ASSERT_NE(originalValue, nullptr);
    for (int key = 100; key < 300; ++key) map.tryEmplace(key, key * 10);
    EXPECT_EQ(map.findValue(7), originalValue);
    EXPECT_EQ(*originalValue, 70);
}

TEST(ArtifactHashMapContractTest, IteratorPostIncrementReturnsPreviousEntry)
{
    ArtifactHashMap<int, int> map;
    map.tryEmplace(1, 10);
    map.tryEmplace(3, 30);
    auto iterator = map.find(1);
    ASSERT_NE(iterator, map.end());
    const auto previous = iterator++;
    EXPECT_EQ(previous->first, 1);
    ASSERT_NE(iterator, map.end());
    EXPECT_EQ(iterator->first, 3);
}

TEST(ArtifactHashMapContractTest, IterationVisitsEveryEntryInCollisionChain)
{
    ArtifactHashMap<int, int, ConstantHash> map;
    map.tryEmplace(1, 10);
    map.tryEmplace(2, 20);
    map.tryEmplace(3, 30);

    std::unordered_map<int, int> observed;
    for (const auto& entry : map) observed.emplace(entry.first, entry.second);

    EXPECT_EQ(observed.size(), 3u);
    EXPECT_EQ(observed, (std::unordered_map<int, int>{{1, 10}, {2, 20}, {3, 30}}));
}

TEST(ArtifactHashMapContractTest, DeterministicOperationsMatchStandardMapModel)
{
    ArtifactHashMap<int, int> actual;
    std::unordered_map<int, int> expected;
    std::mt19937 generator(0xA47F12u);
    std::uniform_int_distribution<int> operationDistribution(0, 3);
    std::uniform_int_distribution<int> keyDistribution(-48, 48);
    std::uniform_int_distribution<int> valueDistribution(-500, 500);

    for (int step = 0; step < 3000; ++step) {
        const int operation = operationDistribution(generator);
        const int key = keyDistribution(generator);
        const int value = valueDistribution(generator);

        if (operation == 0) {
            const auto [entry, inserted] = actual.tryEmplace(key, value);
            const auto [expectedEntry, expectedInserted] = expected.try_emplace(key, value);
            EXPECT_EQ(inserted, expectedInserted) << "step=" << step;
            EXPECT_EQ(entry->second, expectedEntry->second) << "step=" << step;
        } else if (operation == 1) {
            EXPECT_EQ(actual.erase(key), expected.erase(key)) << "step=" << step;
        } else if (operation == 2) {
            EXPECT_EQ(actual.contains(key), expected.contains(key)) << "step=" << step;
        } else {
            actual[key] = value;
            expected[key] = value;
        }

        EXPECT_EQ(actual.size(), expected.size()) << "step=" << step;
        for (int candidate = -48; candidate <= 48; ++candidate) {
            const auto expectedEntry = expected.find(candidate);
            const auto actualEntry = actual.find(candidate);
            EXPECT_EQ(actualEntry != actual.end(), expectedEntry != expected.end())
                << "step=" << step << " key=" << candidate;
            if (expectedEntry != expected.end() && actualEntry != actual.end()) {
                EXPECT_EQ(actualEntry->second, expectedEntry->second)
                    << "step=" << step << " key=" << candidate;
            }
        }
    }
}

