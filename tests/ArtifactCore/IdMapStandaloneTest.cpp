import Container.IdMap;

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using ArtifactCore::ContainerName;
using ArtifactCore::IdMap;

namespace {
struct IdMapConstructionProbe {
  IdMapConstructionProbe(int* constructionCount, int value) : value(value)
  {
    ++*constructionCount;
  }

  int value;
};
}  // namespace

TEST(IdMapStandaloneTest, DefaultConstructionIsEmpty)
{
  const IdMap<int, std::string> values;
  EXPECT_TRUE(values.isEmpty());
  EXPECT_EQ(values.count(), 0u);
}

TEST(IdMapStandaloneTest, NotEmptyInvertsIsEmptyAndTracksReads)
{
  IdMap<int, int> values;
  EXPECT_FALSE(values.notEmpty());
  values.insert(1, 10);
  const auto versionAfterInsert = values.counters().version;
  EXPECT_TRUE(values.notEmpty());

  EXPECT_EQ(values.counters().readCount, 2u);
  EXPECT_EQ(values.counters().version, versionAfterInsert);
  EXPECT_EQ(values.counters().failedAccessCount, 0u);
}

TEST(IdMapStandaloneTest, IsEmptyTracksReadsWithoutMutation)
{
  IdMap<int, int> values;
  EXPECT_TRUE(values.isEmpty());
  values.insert(1, 10);
  const auto versionAfterInsert = values.counters().version;
  EXPECT_FALSE(values.isEmpty());

  EXPECT_EQ(values.counters().readCount, 2u);
  EXPECT_EQ(values.counters().failedAccessCount, 0u);
  EXPECT_EQ(values.counters().version, versionAfterInsert);
}

TEST(IdMapStandaloneTest, NamedConstructionPreservesName)
{
  const IdMap<int, int> values(ContainerName{"entities"});
  EXPECT_STREQ(values.name().value, "entities");
}

TEST(IdMapStandaloneTest, InsertCreatesAndUpdatesValues)
{
  IdMap<int, std::string> values;
  values.insert(7, "first");
  values.insert(7, "updated");
  EXPECT_EQ(values.count(), 1u);
  EXPECT_EQ(values.value(7), "updated");
}

TEST(IdMapStandaloneTest, AddAliasesInsert)
{
  IdMap<int, int> values;
  values.add(3, 9);
  EXPECT_TRUE(values.contains(3));
  EXPECT_EQ(values.value(3), 9);
}

TEST(IdMapStandaloneTest, SubscriptInsertsDefaultValue)
{
  IdMap<int, int> values;
  EXPECT_EQ(values[4], 0);
  EXPECT_EQ(values.count(), 1u);
}

TEST(IdMapStandaloneTest, SubscriptDefaultInsertionRecordsOneMutation)
{
  IdMap<int, int> values;
  int& inserted = values[4];
  EXPECT_EQ(inserted, 0);
  EXPECT_EQ(values.counters().version, 1u);
  EXPECT_EQ(values.counters().addedCount, 1u);
  EXPECT_EQ(values.counters().readCount, 0u);
  EXPECT_EQ(values.counters().failedAccessCount, 0u);
  EXPECT_STREQ(values.lastMutation().operation, "operator[]");
  EXPECT_EQ(values.lastMutation().countBefore, 0u);
  EXPECT_EQ(values.lastMutation().countAfter, 1u);

  values[4] = 44;
  EXPECT_EQ(values.counters().version, 1u);
  EXPECT_EQ(values.counters().addedCount, 1u);
  EXPECT_EQ(values.value(4), 44);
}

TEST(IdMapStandaloneTest, AtReturnsPointerForExistingKey)
{
  IdMap<int, int> values;
  values.insert(1, 8);
  ASSERT_NE(values.at(1), nullptr);
  EXPECT_EQ(*values.at(1), 8);
}

TEST(IdMapStandaloneTest, AtReturnsNullForMissingKey)
{
  IdMap<int, int> values;
  EXPECT_EQ(values.at(99), nullptr);
}

TEST(IdMapStandaloneTest, ContainsHitsAndMissesCountReadsWithoutFailedAccess)
{
  IdMap<int, int> values;
  values.insert(3, 30);
  const auto versionBefore = values.counters().version;

  EXPECT_TRUE(values.contains(3));
  EXPECT_FALSE(values.contains(99));

  EXPECT_EQ(values.counters().readCount, 2u);
  EXPECT_EQ(values.counters().failedAccessCount, 0u);
  EXPECT_EQ(values.counters().version, versionBefore);
}

TEST(IdMapStandaloneTest, ValueReturnsProvidedDefaultForMissingKey)
{
  const IdMap<int, std::string> values;
  EXPECT_EQ(values.value(99, "missing"), "missing");
}

TEST(IdMapStandaloneTest, ValueReadsCountWithoutFailedAccessOrMutation)
{
  IdMap<int, int> values;
  values.insert(1, 10);
  const auto versionBefore = values.counters().version;

  EXPECT_EQ(values.value(1), 10);
  EXPECT_EQ(values.value(99, -1), -1);

  EXPECT_EQ(values.counters().readCount, 2u);
  EXPECT_EQ(values.counters().failedAccessCount, 0u);
  EXPECT_EQ(values.counters().version, versionBefore);
}

TEST(IdMapStandaloneTest, TryEmplaceCreatesValueOnce)
{
  IdMap<int, std::string> values;
  auto [first, insertedFirst] = values.tryEmplace(2, "two");
  auto [second, insertedSecond] = values.tryEmplace(2, "different");
  ASSERT_TRUE(insertedFirst);
  EXPECT_FALSE(insertedSecond);
  EXPECT_EQ(first, second);
  EXPECT_EQ(*second, "two");
}

TEST(IdMapStandaloneTest, TryEmplaceDuplicateDoesNotConstructMappedValue)
{
  IdMap<int, IdMapConstructionProbe> values;
  int constructionCount = 0;
  const auto [original, inserted] = values.tryEmplace(7, &constructionCount, 12);
  ASSERT_TRUE(inserted);
  ASSERT_NE(original, nullptr);
  EXPECT_EQ(constructionCount, 1);

  const auto [duplicate, insertedAgain] = values.tryEmplace(7, &constructionCount, 99);
  EXPECT_FALSE(insertedAgain);
  EXPECT_EQ(duplicate, original);
  EXPECT_EQ(constructionCount, 1);
  ASSERT_NE(duplicate, nullptr);
  EXPECT_EQ(duplicate->value, 12);
  EXPECT_EQ(values.counters().version, 1u);
  EXPECT_EQ(values.counters().mutationCount, 1u);
  EXPECT_EQ(values.counters().addedCount, 1u);
  EXPECT_EQ(values.mutationHistorySize(), 1u);
  EXPECT_STREQ(values.lastMutation().operation, "tryEmplace");
  EXPECT_EQ(values.lastMutation().countBefore, 0u);
  EXPECT_EQ(values.lastMutation().countAfter, 1u);
}

TEST(IdMapStandaloneTest, TryTakeMovesOutAndRemovesValue)
{
  IdMap<int, std::string> values;
  values.insert(5, "five");
  const auto versionBefore = values.counters().version;
  const auto removedBefore = values.counters().removedCount;
  std::string result;
  ASSERT_TRUE(values.tryTake(5, result));
  EXPECT_EQ(result, "five");
  EXPECT_FALSE(values.contains(5));
  EXPECT_EQ(values.counters().version, versionBefore + 1);
  EXPECT_EQ(values.counters().removedCount, removedBefore + 1);
  EXPECT_EQ(values.mutationHistorySize(), 2u);
  EXPECT_STREQ(values.lastMutation().operation, "tryTake");
  EXPECT_EQ(values.lastMutation().countBefore, 1u);
  EXPECT_EQ(values.lastMutation().countAfter, 0u);
}

TEST(IdMapStandaloneTest, TryTakeMissingKeyReturnsFalse)
{
  IdMap<int, int> values;
  int result = 42;
  EXPECT_FALSE(values.tryTake(3, result));
  EXPECT_EQ(result, 42);
}

TEST(IdMapStandaloneTest, TryTakeMissingKeyUpdatesFailureDiagnosticsOnly)
{
  const ArtifactCore::ContainerSourceLocation createdAt{"idmap-test.cpp", "try-take-test", 789};
  IdMap<int, int> values(ContainerName{"entries"}, ArtifactCore::ContainerDomain::Asset, createdAt);
  values.insert(5, 50);
  const auto versionBefore = values.counters().version;
  const auto historySizeBefore = values.mutationHistorySize();
  int result = 42;

  EXPECT_FALSE(values.tryTake(99, result));

  const auto snapshot = values.debugSnapshot();
  EXPECT_EQ(result, 42);
  EXPECT_EQ(snapshot.info.count, 1u);
  EXPECT_EQ(snapshot.counters.version, versionBefore);
  EXPECT_EQ(snapshot.counters.mutationCount, 1u);
  EXPECT_EQ(snapshot.counters.failedAccessCount, 1u);
  EXPECT_EQ(values.mutationHistorySize(), historySizeBefore);
  EXPECT_STREQ(values.lastMutation().operation, "insert");
  EXPECT_STREQ(snapshot.lastFailedAccessAt.file, "idmap-test.cpp");
  EXPECT_STREQ(snapshot.lastFailedAccessAt.function, "try-take-test");
  EXPECT_EQ(snapshot.lastFailedAccessAt.line, 789);
}

TEST(IdMapStandaloneTest, TryTakeTransfersMoveOnlyValueAndPreservesOutputOnMiss)
{
  IdMap<int, std::unique_ptr<int>> values;
  ASSERT_TRUE(values.tryEmplace(5, std::make_unique<int>(50)).second);

  std::unique_ptr<int> result;
  ASSERT_TRUE(values.tryTake(5, result));
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(*result, 50);
  EXPECT_FALSE(values.contains(5));

  auto sentinel = std::make_unique<int>(99);
  int* const sentinelAddress = sentinel.get();
  EXPECT_FALSE(values.tryTake(8, sentinel));
  EXPECT_EQ(sentinel.get(), sentinelAddress);
  ASSERT_NE(sentinel, nullptr);
  EXPECT_EQ(*sentinel, 99);
}

TEST(IdMapStandaloneTest, RemoveDeletesExistingKey)
{
  IdMap<int, int> values;
  values.insert(1, 2);
  EXPECT_TRUE(values.remove(1));
  EXPECT_FALSE(values.contains(1));
}

TEST(IdMapStandaloneTest, RemoveMissingKeyReturnsFalse)
{
  IdMap<int, int> values;
  EXPECT_FALSE(values.remove(1));
}

TEST(IdMapStandaloneTest, RemoveMissingKeyUpdatesFailureDiagnosticsOnly)
{
  const ArtifactCore::ContainerSourceLocation createdAt{"idmap-test.cpp", "remove-test", 456};
  IdMap<int, int> values(ContainerName{"entries"}, ArtifactCore::ContainerDomain::Cache, createdAt);
  values.insert(1, 2);
  const auto versionBefore = values.counters().version;

  EXPECT_FALSE(values.remove(99));

  const auto snapshot = values.debugSnapshot();
  EXPECT_EQ(snapshot.info.count, 1u);
  EXPECT_EQ(snapshot.counters.version, versionBefore);
  EXPECT_EQ(snapshot.counters.failedAccessCount, 1u);
  EXPECT_STREQ(snapshot.lastFailedAccessAt.file, "idmap-test.cpp");
  EXPECT_STREQ(snapshot.lastFailedAccessAt.function, "remove-test");
  EXPECT_EQ(snapshot.lastFailedAccessAt.line, 456);
}

TEST(IdMapStandaloneTest, RemoveIfUsesKeyAndValue)
{
  IdMap<int, int> values;
  values.insert(1, 10);
  values.insert(2, 20);
  values.insert(3, 30);
  EXPECT_EQ(values.removeIf([](int key, int value) { return key == 2 || value == 30; }), 2u);
  EXPECT_EQ(values.count(), 1u);
  EXPECT_TRUE(values.contains(1));
}

TEST(IdMapStandaloneTest, RemoveIfWithNoMatchesPreservesMutationHistory)
{
  IdMap<int, int> values;
  values.insert(1, 10);
  values.insert(2, 20);
  const auto versionBefore = values.counters().version;
  const auto historySizeBefore = values.mutationHistorySize();
  const auto lastMutationBefore = values.lastMutation();

  EXPECT_EQ(values.removeIf([](int, int) { return false; }), 0u);

  EXPECT_EQ(values.count(), 2u);
  EXPECT_EQ(values.counters().version, versionBefore);
  EXPECT_EQ(values.mutationHistorySize(), historySizeBefore);
  EXPECT_EQ(values.lastMutation().version, lastMutationBefore.version);
  EXPECT_STREQ(values.lastMutation().operation, lastMutationBefore.operation);
}

TEST(IdMapStandaloneTest, RemoveIfVisitsEachEntryOnceAndRecordsRemovalTotals)
{
  IdMap<int, int> values;
  values.insert(1, 10);
  values.insert(2, 20);
  values.insert(3, 30);
  const auto versionBefore = values.counters().version;
  const auto removedBefore = values.counters().removedCount;
  int predicateCalls = 0;
  unsigned int visitedKeyMask = 0;

  const auto removed = values.removeIf([&](int key, int value) {
    ++predicateCalls;
    visitedKeyMask |= 1u << (key - 1);
    return value == 20;
  });

  EXPECT_EQ(removed, 1u);
  EXPECT_EQ(predicateCalls, 3);
  EXPECT_EQ(visitedKeyMask, 0b111u);
  EXPECT_TRUE(values.contains(1));
  EXPECT_FALSE(values.contains(2));
  EXPECT_TRUE(values.contains(3));
  EXPECT_EQ(values.counters().version, versionBefore + 1);
  EXPECT_EQ(values.counters().removedCount, removedBefore + 1);
  EXPECT_STREQ(values.lastMutation().operation, "removeIf");
  EXPECT_EQ(values.lastMutation().countBefore, 3u);
  EXPECT_EQ(values.lastMutation().countAfter, 2u);
}

TEST(IdMapStandaloneTest, ClearRemovesAllEntries)
{
  IdMap<int, int> values;
  values.insert(1, 1);
  values.insert(2, 2);
  values.clear();
  EXPECT_TRUE(values.isEmpty());
}

TEST(IdMapStandaloneTest, ClearRecordsRemovedCountAndMutationBounds)
{
  IdMap<int, int> values;
  values.insert(1, 10);
  values.insert(2, 20);
  const auto versionBefore = values.counters().version;
  const auto removedBefore = values.counters().removedCount;

  values.clear();

  EXPECT_EQ(values.counters().version, versionBefore + 1);
  EXPECT_EQ(values.counters().removedCount, removedBefore + 2);
  EXPECT_EQ(values.lastMutation().version, versionBefore + 1);
  EXPECT_STREQ(values.lastMutation().operation, "clear");
  EXPECT_EQ(values.lastMutation().countBefore, 2u);
  EXPECT_EQ(values.lastMutation().countAfter, 0u);
  EXPECT_EQ(values.debugSnapshot().info.count, 0u);
}

TEST(IdMapStandaloneTest, ClearOnEmptyContainerStillRecordsZeroBounds)
{
  IdMap<int, int> values;
  const auto versionBefore = values.counters().version;
  const auto mutationCountBefore = values.counters().mutationCount;
  const auto removedBefore = values.counters().removedCount;
  const auto historySizeBefore = values.mutationHistorySize();

  values.clear();

  EXPECT_EQ(values.counters().version, versionBefore + 1);
  EXPECT_EQ(values.counters().mutationCount, mutationCountBefore + 1);
  EXPECT_EQ(values.counters().removedCount, removedBefore);
  EXPECT_EQ(values.mutationHistorySize(), historySizeBefore + 1);
  EXPECT_STREQ(values.lastMutation().operation, "clear");
  EXPECT_EQ(values.lastMutation().countBefore, 0u);
  EXPECT_EQ(values.lastMutation().countAfter, 0u);
}

TEST(IdMapStandaloneTest, InsertAfterClearContinuesCumulativeDiagnostics)
{
  IdMap<int, int> values;
  values.insert(1, 10);
  values.insert(2, 20);
  values.clear();
  const auto historySizeAfterClear = values.mutationHistorySize();

  values.insert(3, 30);

  const auto snapshot = values.debugSnapshot();
  EXPECT_EQ(snapshot.info.count, 1u);
  EXPECT_EQ(snapshot.counters.version, 4u);
  EXPECT_EQ(snapshot.counters.mutationCount, 4u);
  EXPECT_EQ(snapshot.counters.addedCount, 3u);
  EXPECT_EQ(snapshot.counters.removedCount, 2u);
  EXPECT_EQ(values.mutationHistorySize(), historySizeAfterClear + 1);
  EXPECT_STREQ(snapshot.lastMutation.operation, "insert");
  EXPECT_EQ(snapshot.lastMutation.countBefore, 0u);
  EXPECT_EQ(snapshot.lastMutation.countAfter, 1u);
  EXPECT_EQ(values.value(1, -1), -1);
  EXPECT_EQ(values.value(2, -1), -1);
  EXPECT_EQ(values.value(3), 30);
}

TEST(IdMapStandaloneTest, ReserveAndSqueezeKeepEntries)
{
  IdMap<int, int> values;
  values.reserve(64);
  values.insert(1, 11);
  values.squeeze();
  EXPECT_GE(values.capacity(), 1u);
  EXPECT_EQ(values.value(1), 11);
}

TEST(IdMapStandaloneTest, ReserveRecordsCapacityChangeAndReadQuery)
{
  IdMap<int, int> values;
  const auto versionBefore = values.counters().version;
  const auto capacityChangesBefore = values.counters().capacityChangeCount;
  const auto readsBefore = values.counters().readCount;

  values.reserve(64);

  EXPECT_GE(values.capacity(), 64u);
  EXPECT_EQ(values.counters().version, versionBefore + 1);
  EXPECT_EQ(values.counters().capacityChangeCount, capacityChangesBefore + 1);
  EXPECT_EQ(values.counters().readCount, readsBefore + 1);
  EXPECT_STREQ(values.lastMutation().operation, "reserve");
  EXPECT_EQ(values.lastMutation().countBefore, 0u);
  EXPECT_EQ(values.lastMutation().countAfter, 0u);
}

TEST(IdMapStandaloneTest, ReserveZeroPreservesEntriesAndRecordsRequest)
{
  IdMap<int, int> values;
  values.insert(1, 10);
  values.insert(2, 20);
  const auto capacityBefore = values.capacity();
  const auto capacityChangesBefore = values.counters().capacityChangeCount;
  const auto versionBefore = values.counters().version;

  values.reserve(0);

  const auto capacityAfter = values.capacity();
  EXPECT_EQ(values.value(1), 10);
  EXPECT_EQ(values.value(2), 20);
  EXPECT_EQ(values.counters().version, versionBefore + 1);
  EXPECT_EQ(values.counters().capacityChangeCount,
            capacityChangesBefore + (capacityAfter != capacityBefore ? 1u : 0u));
  EXPECT_STREQ(values.lastMutation().operation, "reserve");
  EXPECT_EQ(values.lastMutation().countBefore, 2u);
  EXPECT_EQ(values.lastMutation().countAfter, 2u);
}

TEST(IdMapStandaloneTest, SqueezeRecordsMutationAndConditionalCapacityChange)
{
  IdMap<int, int> values;
  values.reserve(64);
  values.insert(1, 10);
  values.insert(2, 20);
  values.insert(3, 30);
  const auto capacityBefore = values.capacity();
  const auto capacityChangesBefore = values.counters().capacityChangeCount;
  const auto versionBefore = values.counters().version;

  values.squeeze();

  const auto capacityAfter = values.capacity();
  EXPECT_EQ(values.value(1), 10);
  EXPECT_EQ(values.value(2), 20);
  EXPECT_EQ(values.value(3), 30);
  EXPECT_EQ(values.counters().version, versionBefore + 1);
  EXPECT_EQ(values.counters().capacityChangeCount,
            capacityChangesBefore + (capacityAfter != capacityBefore ? 1u : 0u));
  EXPECT_STREQ(values.lastMutation().operation, "squeeze");
  EXPECT_EQ(values.lastMutation().countBefore, 3u);
  EXPECT_EQ(values.lastMutation().countAfter, 3u);
}

TEST(IdMapStandaloneTest, KeysExtractAllKeys)
{
  IdMap<int, int> values;
  values.insert(3, 30);
  values.insert(5, 50);
  const auto keys = values.keys();
  EXPECT_EQ(keys.count(), 2u);
  EXPECT_TRUE(keys.contains(3));
  EXPECT_TRUE(keys.contains(5));
}

TEST(IdMapStandaloneTest, ValuesExtractAllValues)
{
  IdMap<int, int> values;
  values.insert(3, 30);
  values.insert(5, 50);
  const auto extracted = values.values();
  EXPECT_EQ(extracted.count(), 2u);
  EXPECT_TRUE(extracted.contains(30));
  EXPECT_TRUE(extracted.contains(50));
}

TEST(IdMapStandaloneTest, KeysAndValuesPreserveMetadataAndIterationPairing)
{
  IdMap<int, int> values(ContainerName{"asset-records"}, ArtifactCore::ContainerDomain::Asset);
  values.insert(5, 50);
  values.insert(2, 20);
  values.insert(9, 90);

  const auto keys = values.keys();
  const auto extractedValues = values.values();
  EXPECT_STREQ(keys.name().value, "asset-records");
  EXPECT_STREQ(extractedValues.name().value, "asset-records");
  EXPECT_EQ(keys.domain(), ArtifactCore::ContainerDomain::Asset);
  EXPECT_EQ(extractedValues.domain(), ArtifactCore::ContainerDomain::Asset);
  ASSERT_EQ(keys.count(), 3u);
  ASSERT_EQ(extractedValues.count(), 3u);
  for (std::size_t index = 0; index < keys.count(); ++index) {
    const auto* mappedValue = values.at(keys[index]);
    ASSERT_NE(mappedValue, nullptr);
    EXPECT_EQ(*mappedValue, extractedValues[index]);
  }
}

TEST(IdMapStandaloneTest, ToStdUnorderedMapCopiesEntries)
{
  IdMap<int, int> values;
  values.insert(1, 10);
  const std::unordered_map<int, int> copy = values.toStdUnorderedMap();
  ASSERT_EQ(copy.size(), 1u);
  EXPECT_EQ(copy.at(1), 10);
}

TEST(IdMapStandaloneTest, ToStdUnorderedMapPreservesAllEntriesIndependently)
{
  IdMap<int, int> values;
  values.insert(7, 70);
  values.insert(2, 20);
  values.insert(9, 90);

  auto copy = values.toStdUnorderedMap();
  ASSERT_EQ(copy.size(), 3u);
  EXPECT_EQ(copy.at(7), 70);
  EXPECT_EQ(copy.at(2), 20);
  EXPECT_EQ(copy.at(9), 90);
  copy[7] = 700;
  copy.erase(2);

  EXPECT_EQ(values.value(7), 70);
  EXPECT_EQ(values.value(2), 20);
  EXPECT_EQ(values.value(9), 90);
  EXPECT_EQ(copy.at(7), 700);
  EXPECT_EQ(copy.count(2), 0u);
}

TEST(IdMapStandaloneTest, ToStdUnorderedMapOfEmptyContainerIsEmptyAndDoesNotMutate)
{
  IdMap<int, int> values(ContainerName{"empty"});
  const auto versionBefore = values.counters().version;
  const auto readsBefore = values.counters().readCount;

  const auto converted = values.toStdUnorderedMap();

  EXPECT_TRUE(converted.empty());
  EXPECT_EQ(values.counters().version, versionBefore);
  EXPECT_EQ(values.counters().readCount, readsBefore);
  EXPECT_TRUE(values.isEmpty());
}

TEST(IdMapStandaloneTest, ContainerCopiesOwnIndependentEntries)
{
  IdMap<int, int> source(ContainerName{"source"});
  source.insert(1, 10);
  source.insert(2, 20);
  const auto sourceHistorySize = source.mutationHistorySize();

  auto copied = source;
  copied.insert(1, 99);
  EXPECT_TRUE(copied.remove(2));

  IdMap<int, int> assigned;
  assigned = source;
  EXPECT_TRUE(assigned.remove(1));
  assigned.insert(3, 30);

  EXPECT_EQ(source.value(1), 10);
  EXPECT_EQ(source.value(2), 20);
  EXPECT_EQ(source.mutationHistorySize(), sourceHistorySize);
  EXPECT_EQ(copied.value(1), 99);
  EXPECT_FALSE(copied.contains(2));
  EXPECT_FALSE(assigned.contains(1));
  EXPECT_EQ(assigned.value(2), 20);
  EXPECT_EQ(assigned.value(3), 30);
}

TEST(IdMapStandaloneTest, EachKeyValueVisitsAllPairs)
{
  IdMap<int, int> values;
  values.insert(1, 10);
  values.insert(2, 20);
  std::unordered_map<int, int> seen;
  values.eachKeyValue([&](int key, int value) { seen.emplace(key, value); });
  EXPECT_EQ(seen, (std::unordered_map<int, int>{{1, 10}, {2, 20}}));
}

TEST(IdMapStandaloneTest, MutableEachKeyValueEditsMappedValues)
{
  IdMap<int, int> values;
  values.insert(1, 10);
  values.insert(2, 20);
  values.mutableEachKeyValue([](int, int& value) { value += 1; });
  EXPECT_EQ(values.value(1), 11);
  EXPECT_EQ(values.value(2), 21);
}

TEST(IdMapStandaloneTest, SetNameAndDomainUpdateMetadata)
{
  IdMap<int, int> values(ContainerName{"old"});
  values.setName(ContainerName{"new"});
  values.setDomain(ArtifactCore::ContainerDomain::Asset);
  EXPECT_STREQ(values.name().value, "new");
  EXPECT_EQ(values.domain(), ArtifactCore::ContainerDomain::Asset);
}

TEST(IdMapStandaloneTest, ConstIterationVisitsEntries)
{
  IdMap<int, int> values;
  values.insert(4, 40);
  values.insert(6, 60);
  int sum = 0;
  for (const auto& [key, value] : values) sum += key + value;
  EXPECT_EQ(sum, 110);
}

TEST(IdMapStandaloneTest, MutableIterationEditsValuesWithoutMutationRecord)
{
  IdMap<int, int> values;
  values.insert(4, 40);
  values.insert(6, 60);
  const auto versionBefore = values.counters().version;
  const auto historySizeBefore = values.mutationHistorySize();

  for (auto& [key, value] : values) value += key;

  EXPECT_EQ(values.value(4), 44);
  EXPECT_EQ(values.value(6), 66);
  EXPECT_EQ(values.counters().version, versionBefore);
  EXPECT_EQ(values.mutationHistorySize(), historySizeBefore);
}

TEST(IdMapStandaloneTest, WatchSinceIgnoresReadsAndReportsFailedAccessOncePerCheckpoint)
{
  IdMap<int, int> values;
  values.insert(1, 10);
  const auto checkpoint = values.debugCheckpoint();
  int callbackCount = 0;

  EXPECT_TRUE(values.contains(1));
  EXPECT_FALSE(values.watchSince(checkpoint, [&](const auto&) { ++callbackCount; }));
  EXPECT_EQ(values.at(99), nullptr);
  EXPECT_TRUE(values.watchSince(checkpoint, [&](const auto&) { ++callbackCount; }));
  EXPECT_EQ(callbackCount, 1);

  const auto refreshedCheckpoint = values.debugCheckpoint();
  EXPECT_FALSE(values.watchSince(refreshedCheckpoint, [&](const auto&) { ++callbackCount; }));
  EXPECT_EQ(callbackCount, 1);
}

TEST(IdMapStandaloneTest, DebugSamplesRespectLimitAndPointToMappedValues)
{
  IdMap<int, int> values;
  for (int key = 0; key < 5; ++key) values.insert(key, key * 10);

  const auto samples = values.debugSample(2);
  ASSERT_EQ(samples.count(), 2u);
  for (std::size_t index = 0; index < samples.count(); ++index) {
    EXPECT_EQ(samples[index].index, index);
    const auto* address = samples[index].address;
    bool pointsToMappedValue = false;
    for (int key = 0; key < 5; ++key) {
      if (address == values.at(key)) pointsToMappedValue = true;
    }
    EXPECT_TRUE(pointsToMappedValue);
  }

  const auto snapshot = values.debugSnapshot();
  EXPECT_EQ(snapshot.info.count, 5u);
  ASSERT_EQ(snapshot.samples.size(), 4u);
  for (const auto& sample : snapshot.samples) {
    bool pointsToMappedValue = false;
    for (int key = 0; key < 5; ++key) {
      if (sample.address == values.at(key)) pointsToMappedValue = true;
    }
    EXPECT_TRUE(pointsToMappedValue);
  }
}

TEST(IdMapStandaloneTest, MissingLookupUpdatesFailureSnapshotWithoutChangingVersion)
{
  const ArtifactCore::ContainerSourceLocation createdAt{"idmap-test.cpp", "lookup-test", 123};
  IdMap<int, int> values(ContainerName{"lookup"}, ArtifactCore::ContainerDomain::Asset, createdAt);
  values.insert(1, 10);
  const auto versionBeforeLookup = values.counters().version;

  EXPECT_EQ(values.at(99), nullptr);

  const auto snapshot = values.debugSnapshot();
  EXPECT_EQ(snapshot.counters.version, versionBeforeLookup);
  EXPECT_EQ(snapshot.counters.failedAccessCount, 1u);
  EXPECT_EQ(snapshot.counters.readCount, 1u);
  EXPECT_STREQ(snapshot.lastFailedAccessAt.file, "idmap-test.cpp");
  EXPECT_STREQ(snapshot.lastFailedAccessAt.function, "lookup-test");
  EXPECT_EQ(snapshot.lastFailedAccessAt.line, 123);
  EXPECT_STREQ(snapshot.info.name.value, "lookup");
  EXPECT_EQ(snapshot.info.domain, ArtifactCore::ContainerDomain::Asset);
}

TEST(IdMapStandaloneTest, DebugInfoReportsMetadataTypeCapacityAndApproximateBytes)
{
  IdMap<int, long> values(ContainerName{"diagnostic"}, ArtifactCore::ContainerDomain::Cache);
  values.setOwner(ArtifactCore::ContainerOwner{"Cache", "cache-2"});
  values.insert(1, 10);
  values.insert(2, 20);
  const auto versionBefore = values.counters().version;
  const auto readsBefore = values.counters().readCount;

  const auto info = values.debugInfo();

  EXPECT_STREQ(info.name.value, "diagnostic");
  EXPECT_EQ(info.domain, ArtifactCore::ContainerDomain::Cache);
  EXPECT_STREQ(info.owner.name, "Cache");
  EXPECT_STREQ(info.owner.id, "cache-2");
  EXPECT_NE(info.valueType, nullptr);
  EXPECT_NE(info.valueType[0], '\0');
  EXPECT_EQ(info.count, 2u);
  EXPECT_GT(info.capacity, 0u);
  EXPECT_EQ(info.approximateBytes,
            info.capacity * sizeof(void*) + info.count * (sizeof(int) + sizeof(long) + sizeof(std::size_t)));
  EXPECT_EQ(values.counters().version, versionBefore);
  EXPECT_EQ(values.counters().readCount, readsBefore);
}

TEST(IdMapStandaloneTest, OwnerChangeUpdatesDebugInfoWithoutEntryMutation)
{
  IdMap<int, int> values(ContainerName{"owned"});
  values.insert(1, 10);
  const auto versionBefore = values.counters().version;
  const auto historySizeBefore = values.mutationHistorySize();

  values.setOwner(ArtifactCore::ContainerOwner{"AssetStore", "store-4"});

  const auto info = values.debugInfo();
  EXPECT_STREQ(info.owner.name, "AssetStore");
  EXPECT_STREQ(info.owner.id, "store-4");
  EXPECT_EQ(values.counters().version, versionBefore);
  EXPECT_EQ(values.mutationHistorySize(), historySizeBefore);
}

TEST(IdMapStandaloneTest, DebugSnapshotCombinesMutationLocationCountersAndSamples)
{
  const ArtifactCore::ContainerSourceLocation createdAt{"snapshot.cpp", "build-map", 52};
  IdMap<int, int> values(ContainerName{"snapshot"}, ArtifactCore::ContainerDomain::Asset, createdAt);
  values.insert(4, 40);
  values.insert(6, 60);

  const auto snapshot = values.debugSnapshot();

  EXPECT_STREQ(snapshot.createdAt.file, "snapshot.cpp");
  EXPECT_STREQ(snapshot.createdAt.function, "build-map");
  EXPECT_EQ(snapshot.createdAt.line, 52);
  EXPECT_STREQ(snapshot.lastMutatedAt.file, "snapshot.cpp");
  EXPECT_EQ(snapshot.counters.version, 2u);
  EXPECT_EQ(snapshot.counters.mutationCount, 2u);
  EXPECT_STREQ(snapshot.lastMutation.operation, "insert");
  EXPECT_EQ(snapshot.lastMutation.countBefore, 1u);
  EXPECT_EQ(snapshot.lastMutation.countAfter, 2u);
  ASSERT_EQ(snapshot.samples.size(), 2u);
  for (const auto& sample : snapshot.samples) {
    bool pointsToMappedValue = false;
    for (int key = 4; key <= 6; key += 2) {
      if (sample.address == values.at(key)) pointsToMappedValue = true;
    }
    EXPECT_TRUE(pointsToMappedValue);
    EXPECT_STREQ(sample.note, "sample");
  }
}

TEST(IdMapStandaloneTest, WatchRuleUsesInclusiveCountBoundaryAndReturnsSnapshot)
{
  IdMap<int, int> values;
  values.insert(1, 10);
  values.insert(2, 20);
  ContainerWatchRule rule;
  rule.maxCount = 1;
  int callbackCount = 0;
  std::size_t observedCount = 0;
  const char* observedReason = "";

  EXPECT_TRUE(values.watch(rule, [&](const ContainerWatchHit& hit) {
    ++callbackCount;
    observedCount = hit.snapshot.info.count;
    observedReason = hit.reason;
  }));
  EXPECT_EQ(callbackCount, 1);
  EXPECT_EQ(observedCount, 2u);
  EXPECT_STREQ(observedReason, "idmap-watch");

  rule.maxCount = 2;
  EXPECT_FALSE(values.watch(rule, [&](const ContainerWatchHit&) { ++callbackCount; }));
  EXPECT_EQ(callbackCount, 1);
}

TEST(IdMapStandaloneTest, MutationHistoryKeepsLatestEightConsistentRecords)
{
  IdMap<int, int> values;
  for (int key = 0; key < 10; ++key) {
    values.insert(key, key * 10);
  }

  ASSERT_EQ(values.mutationHistorySize(), 8u);
  EXPECT_EQ(values.mutationHistoryAt(8), nullptr);
  for (std::size_t index = 0; index < values.mutationHistorySize(); ++index) {
    const auto* record = values.mutationHistoryAt(index);
    ASSERT_NE(record, nullptr);
    EXPECT_STREQ(record->operation, "insert");
    EXPECT_EQ(record->version, index + 3);
    EXPECT_EQ(record->countBefore, index + 2);
    EXPECT_EQ(record->countAfter, index + 3);
  }
  EXPECT_EQ(values.lastMutation().version, 10u);
  EXPECT_EQ(values.lastMutation().countAfter, 10u);
}
