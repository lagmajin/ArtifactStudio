import Container.NameMap;

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <vector>

using ArtifactCore::ContainerName;
using ArtifactCore::NameMap;

namespace {
struct NameMapConstructionProbe {
  NameMapConstructionProbe(int* constructionCount, int value) : value(value)
  {
    ++*constructionCount;
  }

  int value;
};
}  // namespace

TEST(NameMapStandaloneTest, DefaultConstructionIsEmpty)
{
  const NameMap<int, int> values;
  EXPECT_TRUE(values.isEmpty());
  EXPECT_EQ(values.count(), 0u);
}

TEST(NameMapStandaloneTest, NotEmptyInvertsIsEmptyAndTracksReads)
{
  NameMap<int, int> values;
  EXPECT_FALSE(values.notEmpty());
  values.insert(1, 10);
  const auto versionAfterInsert = values.counters().version;
  EXPECT_TRUE(values.notEmpty());

  EXPECT_EQ(values.counters().readCount, 2u);
  EXPECT_EQ(values.counters().version, versionAfterInsert);
  EXPECT_EQ(values.counters().failedAccessCount, 0u);
}

TEST(NameMapStandaloneTest, SizeAliasesCountAndTracksOneRead)
{
  NameMap<int, int> values;
  values.insert(1, 10);
  values.insert(2, 20);
  const auto versionBefore = values.counters().version;

  EXPECT_EQ(values.size(), 2u);

  EXPECT_EQ(values.counters().readCount, 1u);
  EXPECT_EQ(values.counters().failedAccessCount, 0u);
  EXPECT_EQ(values.counters().version, versionBefore);
}

TEST(NameMapStandaloneTest, NamedConstructionPreservesName)
{
  const NameMap<int, int> values(ContainerName{"lookup"});
  EXPECT_STREQ(values.name().value, "lookup");
}

TEST(NameMapStandaloneTest, InsertCreatesAndUpdatesValues)
{
  NameMap<int, int> values;
  values.insert(1, 10);
  values.insert(1, 11);
  EXPECT_EQ(values.count(), 1u);
  EXPECT_EQ(values.value(1), 11);
}

TEST(NameMapStandaloneTest, InsertOverwriteRecordsMutationWithoutChangingEntryTotals)
{
  NameMap<int, int> values;
  values.insert(1, 10);
  const auto versionBefore = values.counters().version;
  const auto mutationCountBefore = values.counters().mutationCount;
  const auto addedCountBefore = values.counters().addedCount;
  const auto removedCountBefore = values.counters().removedCount;

  values.insert(1, 11);

  EXPECT_EQ(values.value(1), 11);
  EXPECT_EQ(values.count(), 1u);
  EXPECT_EQ(values.counters().version, versionBefore + 1);
  EXPECT_EQ(values.counters().mutationCount, mutationCountBefore + 1);
  EXPECT_EQ(values.counters().addedCount, addedCountBefore);
  EXPECT_EQ(values.counters().removedCount, removedCountBefore);
  EXPECT_STREQ(values.lastMutation().operation, "insert");
  EXPECT_EQ(values.lastMutation().countBefore, 1u);
  EXPECT_EQ(values.lastMutation().countAfter, 1u);
}

TEST(NameMapStandaloneTest, AddAliasesInsert)
{
  NameMap<int, int> values;
  values.add(2, 20);
  EXPECT_TRUE(values.contains(2));
  EXPECT_EQ(values.value(2), 20);
}

TEST(NameMapStandaloneTest, SubscriptInsertsDefaultValue)
{
  NameMap<int, int> values;
  EXPECT_EQ(values[4], 0);
  EXPECT_EQ(values.count(), 1u);
}

TEST(NameMapStandaloneTest, SubscriptDefaultInsertionRecordsOneMutation)
{
  NameMap<int, int> values;
  int& inserted = values[4];
  EXPECT_EQ(inserted, 0);
  EXPECT_EQ(values.counters().version, 1u);
  EXPECT_EQ(values.counters().addedCount, 1u);
  EXPECT_STREQ(values.lastMutation().operation, "operator[]");
  EXPECT_EQ(values.lastMutation().countBefore, 0u);
  EXPECT_EQ(values.lastMutation().countAfter, 1u);

  values[4] = 44;
  EXPECT_EQ(values.counters().version, 1u);
  EXPECT_EQ(values.counters().addedCount, 1u);
  EXPECT_EQ(values.value(4), 44);
}

TEST(NameMapStandaloneTest, SubscriptExistingKeyUpdatesValueWithoutMutationRecord)
{
  NameMap<int, int> values;
  values.insert(4, 40);
  const auto versionBefore = values.counters().version;
  const auto historySizeBefore = values.mutationHistorySize();

  values[4] = 44;

  EXPECT_EQ(values.value(4), 44);
  EXPECT_EQ(values.count(), 1u);
  EXPECT_EQ(values.counters().version, versionBefore);
  EXPECT_EQ(values.mutationHistorySize(), historySizeBefore);
}

TEST(NameMapStandaloneTest, AtReturnsPointerForExistingKey)
{
  NameMap<int, int> values;
  values.insert(3, 30);
  ASSERT_NE(values.at(3), nullptr);
  EXPECT_EQ(*values.at(3), 30);
}

TEST(NameMapStandaloneTest, AtReturnsNullForMissingKey)
{
  NameMap<int, int> values;
  EXPECT_EQ(values.at(99), nullptr);
}

TEST(NameMapStandaloneTest, ContainsHitsAndMissesCountReadsWithoutFailedAccess)
{
  NameMap<int, int> values;
  values.insert(3, 30);
  const auto versionBefore = values.counters().version;

  EXPECT_TRUE(values.contains(3));
  EXPECT_FALSE(values.contains(99));

  EXPECT_EQ(values.counters().readCount, 2u);
  EXPECT_EQ(values.counters().failedAccessCount, 0u);
  EXPECT_EQ(values.counters().version, versionBefore);
}

TEST(NameMapStandaloneTest, AtHitsCountReadsWithoutFailureOrMutation)
{
  NameMap<int, int> values;
  values.insert(3, 30);
  const auto versionBefore = values.counters().version;

  int* const mutableValue = values.at(3);
  ASSERT_NE(mutableValue, nullptr);
  EXPECT_EQ(*mutableValue, 30);
  const auto& constValues = values;
  const int* const constValue = constValues.at(3);
  ASSERT_NE(constValue, nullptr);
  EXPECT_EQ(*constValue, 30);

  EXPECT_EQ(values.counters().readCount, 2u);
  EXPECT_EQ(values.counters().failedAccessCount, 0u);
  EXPECT_EQ(values.counters().version, versionBefore);
}

TEST(NameMapStandaloneTest, ValueReturnsDefaultForMissingKey)
{
  const NameMap<int, int> values;
  EXPECT_EQ(values.value(99, -1), -1);
}

TEST(NameMapStandaloneTest, ValueReadsCountWithoutFailedAccessOrMutation)
{
  NameMap<int, int> values;
  values.insert(1, 10);
  const auto versionBefore = values.counters().version;

  EXPECT_EQ(values.value(1), 10);
  EXPECT_EQ(values.value(99, -1), -1);

  EXPECT_EQ(values.counters().readCount, 2u);
  EXPECT_EQ(values.counters().failedAccessCount, 0u);
  EXPECT_EQ(values.counters().version, versionBefore);
}

TEST(NameMapStandaloneTest, TryEmplaceOnlyInsertsOnce)
{
  NameMap<int, int> values;
  auto [first, insertedFirst] = values.tryEmplace(5, 50);
  auto [second, insertedSecond] = values.tryEmplace(5, 500);
  ASSERT_TRUE(insertedFirst);
  EXPECT_FALSE(insertedSecond);
  EXPECT_EQ(first, second);
  EXPECT_EQ(*second, 50);
}

TEST(NameMapStandaloneTest, TryEmplaceRecordsOnlySuccessfulConstruction)
{
  NameMap<int, NameMapConstructionProbe> values;
  int constructionCount = 0;

  const auto [first, insertedFirst] = values.tryEmplace(5, &constructionCount, 50);
  ASSERT_TRUE(insertedFirst);
  ASSERT_NE(first, nullptr);
  const auto [duplicate, insertedDuplicate] = values.tryEmplace(5, &constructionCount, 500);

  EXPECT_FALSE(insertedDuplicate);
  EXPECT_EQ(duplicate, first);
  EXPECT_EQ(constructionCount, 1);
  EXPECT_EQ(duplicate->value, 50);
  EXPECT_EQ(values.counters().version, 1u);
  EXPECT_EQ(values.counters().mutationCount, 1u);
  EXPECT_EQ(values.counters().addedCount, 1u);
  EXPECT_EQ(values.mutationHistorySize(), 1u);
  EXPECT_STREQ(values.lastMutation().operation, "tryEmplace");
  EXPECT_EQ(values.lastMutation().countBefore, 0u);
  EXPECT_EQ(values.lastMutation().countAfter, 1u);
}

TEST(NameMapStandaloneTest, TryTakeTransfersAndErasesEntry)
{
  NameMap<int, int> values;
  values.insert(6, 60);
  const auto versionBefore = values.counters().version;
  const auto removedBefore = values.counters().removedCount;
  int result = 0;
  ASSERT_TRUE(values.tryTake(6, result));
  EXPECT_EQ(result, 60);
  EXPECT_FALSE(values.contains(6));
  EXPECT_EQ(values.counters().version, versionBefore + 1);
  EXPECT_EQ(values.counters().removedCount, removedBefore + 1);
  EXPECT_EQ(values.mutationHistorySize(), 2u);
  EXPECT_STREQ(values.lastMutation().operation, "tryTake");
  EXPECT_EQ(values.lastMutation().countBefore, 1u);
  EXPECT_EQ(values.lastMutation().countAfter, 0u);
}

TEST(NameMapStandaloneTest, TryTakeRemovesOnlyTheRequestedEntry)
{
  NameMap<int, int> values;
  values.insert(2, 20);
  values.insert(6, 60);
  values.insert(9, 90);
  int result = 0;

  ASSERT_TRUE(values.tryTake(6, result));

  EXPECT_EQ(result, 60);
  EXPECT_EQ(values.count(), 2u);
  EXPECT_EQ(values.value(2), 20);
  EXPECT_EQ(values.value(9), 90);
  EXPECT_FALSE(values.contains(6));
  EXPECT_EQ(values.lastMutation().countBefore, 3u);
  EXPECT_EQ(values.lastMutation().countAfter, 2u);
}

TEST(NameMapStandaloneTest, TryTakeMissingKeyLeavesOutputUntouched)
{
  NameMap<int, int> values;
  int result = 77;
  EXPECT_FALSE(values.tryTake(6, result));
  EXPECT_EQ(result, 77);
}

TEST(NameMapStandaloneTest, TryTakeMissingKeyUpdatesFailureDiagnosticsOnly)
{
  const ArtifactCore::ContainerSourceLocation createdAt{"namemap-test.cpp", "try-take-test", 234};
  NameMap<int, int> values(ContainerName{"entries"}, ArtifactCore::ContainerDomain::Asset, createdAt);
  values.insert(6, 60);
  const auto versionBefore = values.counters().version;
  int result = 77;

  EXPECT_FALSE(values.tryTake(99, result));

  const auto snapshot = values.debugSnapshot();
  EXPECT_EQ(result, 77);
  EXPECT_EQ(snapshot.info.count, 1u);
  EXPECT_EQ(snapshot.counters.version, versionBefore);
  EXPECT_EQ(snapshot.counters.failedAccessCount, 1u);
  EXPECT_STREQ(snapshot.lastFailedAccessAt.file, "namemap-test.cpp");
  EXPECT_STREQ(snapshot.lastFailedAccessAt.function, "try-take-test");
  EXPECT_EQ(snapshot.lastFailedAccessAt.line, 234);
}

TEST(NameMapStandaloneTest, TryTakeTransfersMoveOnlyValueAndPreservesOutputOnMiss)
{
  NameMap<int, std::unique_ptr<int>> values;
  ASSERT_TRUE(values.tryEmplace(6, std::make_unique<int>(60)).second);
  std::unique_ptr<int> result;

  ASSERT_TRUE(values.tryTake(6, result));
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(*result, 60);
  EXPECT_FALSE(values.contains(6));

  auto* const retainedAddress = result.get();
  EXPECT_FALSE(values.tryTake(99, result));
  EXPECT_EQ(result.get(), retainedAddress);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(*result, 60);
}

TEST(NameMapStandaloneTest, EraseReturnsOneThenZero)
{
  NameMap<int, int> values;
  values.insert(7, 70);
  EXPECT_EQ(values.erase(7), 1u);
  EXPECT_EQ(values.erase(7), 0u);
}

TEST(NameMapStandaloneTest, EraseMissingKeyIsNonMutatingAndNotFailedAccess)
{
  NameMap<int, int> values;
  values.insert(7, 70);
  const auto versionBefore = values.counters().version;

  EXPECT_EQ(values.erase(99), 0u);

  EXPECT_EQ(values.counters().version, versionBefore);
  EXPECT_EQ(values.counters().failedAccessCount, 0u);
  EXPECT_EQ(values.debugSnapshot().info.count, 1u);
}

TEST(NameMapStandaloneTest, RemoveReportsSuccessAndMissingKey)
{
  NameMap<int, int> values;
  values.insert(8, 80);
  EXPECT_TRUE(values.remove(8));
  EXPECT_FALSE(values.remove(8));
}

TEST(NameMapStandaloneTest, RemoveMissingKeyUpdatesFailureDiagnosticsOnly)
{
  const ArtifactCore::ContainerSourceLocation createdAt{"namemap-test.cpp", "remove-test", 345};
  NameMap<int, int> values(ContainerName{"entries"}, ArtifactCore::ContainerDomain::Cache, createdAt);
  values.insert(8, 80);
  const auto versionBefore = values.counters().version;

  EXPECT_FALSE(values.remove(99));

  const auto snapshot = values.debugSnapshot();
  EXPECT_EQ(snapshot.info.count, 1u);
  EXPECT_EQ(snapshot.counters.version, versionBefore);
  EXPECT_EQ(snapshot.counters.failedAccessCount, 1u);
  EXPECT_STREQ(snapshot.lastFailedAccessAt.file, "namemap-test.cpp");
  EXPECT_STREQ(snapshot.lastFailedAccessAt.function, "remove-test");
  EXPECT_EQ(snapshot.lastFailedAccessAt.line, 345);
}

TEST(NameMapStandaloneTest, RemoveIfReceivesKeyAndValue)
{
  NameMap<int, int> values;
  values.insert(1, 10);
  values.insert(2, 20);
  values.insert(3, 30);
  EXPECT_EQ(values.removeIf([](int key, int value) { return key == 2 || value == 30; }), 2u);
  EXPECT_EQ(values.count(), 1u);
  EXPECT_TRUE(values.contains(1));
}

TEST(NameMapStandaloneTest, RemoveIfVisitsEachEntryOnceAndRecordsRemovalTotals)
{
  NameMap<int, int> values;
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

TEST(NameMapStandaloneTest, ClearRemovesAllEntries)
{
  NameMap<int, int> values;
  values.insert(1, 10);
  values.insert(2, 20);
  values.clear();
  EXPECT_TRUE(values.isEmpty());
}

TEST(NameMapStandaloneTest, ClearRecordsRemovedCountAndMutationBounds)
{
  NameMap<int, int> values;
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

TEST(NameMapStandaloneTest, InsertAfterClearContinuesCumulativeDiagnostics)
{
  NameMap<int, int> values;
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

TEST(NameMapStandaloneTest, ClearOnEmptyContainerStillRecordsZeroBounds)
{
  NameMap<int, int> values;
  const auto versionBefore = values.counters().version;
  const auto historySizeBefore = values.mutationHistorySize();

  values.clear();

  EXPECT_EQ(values.counters().version, versionBefore + 1);
  EXPECT_EQ(values.mutationHistorySize(), historySizeBefore + 1);
  EXPECT_EQ(values.counters().removedCount, 0u);
  EXPECT_STREQ(values.lastMutation().operation, "clear");
  EXPECT_EQ(values.lastMutation().countBefore, 0u);
  EXPECT_EQ(values.lastMutation().countAfter, 0u);
}

TEST(NameMapStandaloneTest, KeysExtractAllKeys)
{
  NameMap<int, int> values;
  values.insert(3, 30);
  values.insert(5, 50);
  const auto keys = values.keys();
  EXPECT_EQ(keys.count(), 2u);
  EXPECT_TRUE(keys.contains(3));
  EXPECT_TRUE(keys.contains(5));
}

TEST(NameMapStandaloneTest, ValuesExtractAllValues)
{
  NameMap<int, int> values;
  values.insert(3, 30);
  values.insert(5, 50);
  const auto extracted = values.values();
  EXPECT_EQ(extracted.count(), 2u);
  EXPECT_TRUE(extracted.contains(30));
  EXPECT_TRUE(extracted.contains(50));
}

TEST(NameMapStandaloneTest, KeysAndValuesPreserveMetadataAndPairOrder)
{
  NameMap<int, int> values(ContainerName{"asset-records"}, ArtifactCore::ContainerDomain::Asset);
  values.insert(5, 50);
  values.insert(2, 20);

  const auto keys = values.keys();
  const auto extractedValues = values.values();
  EXPECT_STREQ(keys.name().value, "asset-records");
  EXPECT_STREQ(extractedValues.name().value, "asset-records");
  EXPECT_EQ(keys.domain(), ArtifactCore::ContainerDomain::Asset);
  EXPECT_EQ(extractedValues.domain(), ArtifactCore::ContainerDomain::Asset);
  ASSERT_EQ(keys.count(), 2u);
  ASSERT_EQ(extractedValues.count(), 2u);
  EXPECT_EQ(keys[0], 2);
  EXPECT_EQ(extractedValues[0], 20);
  EXPECT_EQ(keys[1], 5);
  EXPECT_EQ(extractedValues[1], 50);
}

TEST(NameMapStandaloneTest, ToStdMapReturnsSortedEntries)
{
  NameMap<int, int> values;
  values.insert(4, 40);
  values.insert(1, 10);
  const std::map<int, int> sorted = values.toStdMap();
  EXPECT_EQ(sorted, (std::map<int, int>{{1, 10}, {4, 40}}));
}

TEST(NameMapStandaloneTest, ToStdMapOfEmptyContainerIsEmptyAndDoesNotMutate)
{
  NameMap<int, int> values(ContainerName{"empty"});
  const auto versionBefore = values.counters().version;
  const auto readsBefore = values.counters().readCount;

  const auto converted = values.toStdMap();

  EXPECT_TRUE(converted.empty());
  EXPECT_EQ(values.counters().version, versionBefore);
  EXPECT_EQ(values.counters().readCount, readsBefore);
  EXPECT_TRUE(values.isEmpty());
}

TEST(NameMapStandaloneTest, EachKeyValueVisitsEveryEntry)
{
  NameMap<int, int> values;
  values.insert(2, 20);
  values.insert(3, 30);
  std::map<int, int> seen;
  values.eachKeyValue([&](int key, int value) { seen.emplace(key, value); });
  EXPECT_EQ(seen, (std::map<int, int>{{2, 20}, {3, 30}}));
}

TEST(NameMapStandaloneTest, MutableEachKeyValueCanEditValues)
{
  NameMap<int, int> values;
  values.insert(2, 20);
  values.insert(3, 30);
  values.mutableEachKeyValue([](int, int& value) { ++value; });
  EXPECT_EQ(values.value(2), 21);
  EXPECT_EQ(values.value(3), 31);
}

TEST(NameMapStandaloneTest, MetadataNameAndDomainCanBeChanged)
{
  NameMap<int, int> values(ContainerName{"before"});
  values.setName(ContainerName{"after"});
  values.setDomain(ArtifactCore::ContainerDomain::Timeline);
  EXPECT_STREQ(values.name().value, "after");
  EXPECT_EQ(values.domain(), ArtifactCore::ContainerDomain::Timeline);
}

TEST(NameMapStandaloneTest, NameAndDomainChangesUpdateDebugInfoWithoutMutation)
{
  NameMap<int, int> values(ContainerName{"before"}, ArtifactCore::ContainerDomain::Asset);
  values.insert(1, 10);
  const auto versionBefore = values.counters().version;
  const auto historySizeBefore = values.mutationHistorySize();

  values.setName(ContainerName{"after"});
  values.setDomain(ArtifactCore::ContainerDomain::Timeline);

  const auto info = values.debugInfo();
  EXPECT_STREQ(info.name.value, "after");
  EXPECT_EQ(info.domain, ArtifactCore::ContainerDomain::Timeline);
  EXPECT_EQ(values.counters().version, versionBefore);
  EXPECT_EQ(values.mutationHistorySize(), historySizeBefore);
}

TEST(NameMapStandaloneTest, DebugMetadataSettersPopulateSnapshotWithoutMutation)
{
  NameMap<int, int> values(ContainerName{"metadata"});
  const ArtifactCore::ContainerSourceLocation createdAt{"created.cpp", "create", 10};
  const ArtifactCore::ContainerSourceLocation changedAt{"changed.cpp", "mutate", 20};
  const ArtifactCore::ContainerSourceLocation failedAt{"failed.cpp", "lookup", 30};
  const auto versionBefore = values.counters().version;

  values.setOwner(ArtifactCore::ContainerOwner{"Timeline", "timeline-7"});
  values.setCreatedAt(createdAt);
  values.setLastMutatedAt(changedAt);
  values.setLastFailedAccessAt(failedAt);

  const auto snapshot = values.debugSnapshot();
  EXPECT_EQ(snapshot.counters.version, versionBefore);
  EXPECT_STREQ(snapshot.info.owner.name, "Timeline");
  EXPECT_STREQ(snapshot.info.owner.id, "timeline-7");
  EXPECT_STREQ(snapshot.createdAt.file, "created.cpp");
  EXPECT_EQ(snapshot.createdAt.line, 10);
  EXPECT_STREQ(snapshot.lastMutatedAt.file, "changed.cpp");
  EXPECT_EQ(snapshot.lastMutatedAt.line, 20);
  EXPECT_STREQ(snapshot.lastFailedAccessAt.file, "failed.cpp");
  EXPECT_EQ(snapshot.lastFailedAccessAt.line, 30);
}

TEST(NameMapStandaloneTest, DebugInfoReportsMetadataAndNameMapCapacityEstimate)
{
  NameMap<int, long> values(ContainerName{"diagnostic"}, ArtifactCore::ContainerDomain::Cache);
  values.setOwner(ArtifactCore::ContainerOwner{"Cache", "cache-3"});
  values.insert(1, 10);
  values.insert(2, 20);
  const auto versionBefore = values.counters().version;
  const auto readsBefore = values.counters().readCount;

  const auto info = values.debugInfo();

  EXPECT_STREQ(info.name.value, "diagnostic");
  EXPECT_EQ(info.domain, ArtifactCore::ContainerDomain::Cache);
  EXPECT_STREQ(info.owner.name, "Cache");
  EXPECT_STREQ(info.owner.id, "cache-3");
  EXPECT_NE(info.valueType, nullptr);
  EXPECT_NE(info.valueType[0], '\0');
  EXPECT_EQ(info.count, 2u);
  EXPECT_EQ(info.capacity, info.count);
  EXPECT_EQ(info.approximateBytes, info.count * (sizeof(int) + sizeof(long)));
  EXPECT_EQ(values.counters().version, versionBefore);
  EXPECT_EQ(values.counters().readCount, readsBefore);
}

TEST(NameMapStandaloneTest, DebugSnapshotCombinesMutationLocationCountersAndSamples)
{
  const ArtifactCore::ContainerSourceLocation createdAt{"namemap-snapshot.cpp", "build-map", 61};
  NameMap<int, int> values(ContainerName{"snapshot"}, ArtifactCore::ContainerDomain::Asset, createdAt);
  values.insert(4, 40);
  values.insert(6, 60);

  const auto snapshot = values.debugSnapshot();

  EXPECT_STREQ(snapshot.createdAt.file, "namemap-snapshot.cpp");
  EXPECT_STREQ(snapshot.createdAt.function, "build-map");
  EXPECT_EQ(snapshot.createdAt.line, 61);
  EXPECT_STREQ(snapshot.lastMutatedAt.file, "namemap-snapshot.cpp");
  EXPECT_EQ(snapshot.counters.version, 2u);
  EXPECT_EQ(snapshot.counters.mutationCount, 2u);
  EXPECT_STREQ(snapshot.lastMutation.operation, "insert");
  EXPECT_EQ(snapshot.lastMutation.countBefore, 1u);
  EXPECT_EQ(snapshot.lastMutation.countAfter, 2u);
  ASSERT_EQ(snapshot.samples.size(), 2u);
  EXPECT_TRUE(snapshot.notes.empty());
  for (const auto& sample : snapshot.samples) {
    bool pointsToMappedValue = false;
    for (int key = 4; key <= 6; key += 2) {
      if (sample.address == values.at(key)) pointsToMappedValue = true;
    }
    EXPECT_TRUE(pointsToMappedValue);
    EXPECT_STREQ(sample.note, "sample");
  }
}

TEST(NameMapStandaloneTest, RangeIterationVisitsEveryPair)
{
  NameMap<int, int> values;
  values.insert(1, 10);
  values.insert(2, 20);
  int sum = 0;
  for (const auto& [key, value] : values) sum += key + value;
  EXPECT_EQ(sum, 33);
}

TEST(NameMapStandaloneTest, ToStdMapIsAnIndependentCopy)
{
  NameMap<int, int> values;
  values.insert(1, 10);
  auto copy = values.toStdMap();
  copy[1] = 99;
  EXPECT_EQ(values.value(1), 10);
}

TEST(NameMapStandaloneTest, ContainerCopiesOwnIndependentEntries)
{
  NameMap<int, int> source(ContainerName{"source"});
  source.insert(1, 10);
  source.insert(2, 20);

  auto copied = source;
  copied.insert(1, 99);
  EXPECT_TRUE(copied.remove(2));

  NameMap<int, int> assigned;
  assigned = source;
  EXPECT_TRUE(assigned.remove(1));
  assigned.insert(3, 30);

  EXPECT_EQ(source.value(1), 10);
  EXPECT_EQ(source.value(2), 20);
  EXPECT_EQ(copied.value(1), 99);
  EXPECT_FALSE(copied.contains(2));
  EXPECT_FALSE(assigned.contains(1));
  EXPECT_EQ(assigned.value(2), 20);
  EXPECT_EQ(assigned.value(3), 30);
}

TEST(NameMapStandaloneTest, WatchRuleReportsFailedAccessWithSnapshot)
{
  NameMap<int, int> values;
  values.insert(1, 10);
  ContainerWatchRule rule;
  rule.watchFailedAccess = true;
  int callbackCount = 0;
  std::size_t observedFailedAccessCount = 0;
  const char* observedReason = "";

  EXPECT_FALSE(values.watch(rule, [&](const ContainerWatchHit&) { ++callbackCount; }));
  EXPECT_EQ(values.at(99), nullptr);
  EXPECT_TRUE(values.watch(rule, [&](const ContainerWatchHit& hit) {
    ++callbackCount;
    observedFailedAccessCount = hit.snapshot.counters.failedAccessCount;
    observedReason = hit.reason;
  }));
  EXPECT_EQ(callbackCount, 1);
  EXPECT_EQ(observedFailedAccessCount, 1u);
  EXPECT_STREQ(observedReason, "namemap-watch");
}

TEST(NameMapStandaloneTest, WatchSinceIgnoresReadsAndReportsFailedAccessOncePerCheckpoint)
{
  NameMap<int, int> values;
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

TEST(NameMapStandaloneTest, MutationHistoryKeepsOnlyTheLatestEightRecords)
{
  NameMap<int, int> values;
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

