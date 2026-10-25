import Container.NamedVector;

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using ArtifactCore::ContainerName;
using ArtifactCore::NamedVector;

TEST(NamedVectorStandaloneTest, DefaultConstructionIsEmpty)
{
  const NamedVector<int> values;
  EXPECT_TRUE(values.isEmpty());
  EXPECT_EQ(values.count(), 0u);
}

TEST(NamedVectorStandaloneTest, NamedConstructionPreservesName)
{
  const NamedVector<int> values(ContainerName{"samples"});
  EXPECT_STREQ(values.name().value, "samples");
}

TEST(NamedVectorStandaloneTest, InitializerListSetsValues)
{
  const NamedVector<int> values(ContainerName{"numbers"}, {2, 4, 6});
  ASSERT_EQ(values.count(), 3u);
  EXPECT_EQ(values[0], 2);
  EXPECT_EQ(values[2], 6);
}

TEST(NamedVectorStandaloneTest, AddAndAppendPreserveOrder)
{
  NamedVector<int> values;
  values.add(1);
  values.append(2);
  values.push_back(3);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 2, 3}));
}

TEST(NamedVectorStandaloneTest, MakeAndEmplaceBackForwardArgumentsAndReturnStoredReferences)
{
  NamedVector<std::string> values(ContainerName{"construct"});

  std::string& repeated = values.make(4, 'x');
  EXPECT_EQ(repeated, "xxxx");
  EXPECT_EQ(&repeated, values.at(0));

  std::string& appended = values.emplace_back("tail");
  EXPECT_EQ(appended, "tail");
  EXPECT_EQ(&appended, values.at(1));
  EXPECT_EQ(values.count(), 2u);
}

TEST(NamedVectorStandaloneTest, MoveOnlyElementsSurviveEditExtractionAndContainerMove)
{
  NamedVector<std::unique_ptr<int>> values(ContainerName{"move-only"});
  values.add(std::make_unique<int>(10));
  values.add(std::make_unique<int>(30));
  values.insert(1, std::make_unique<int>(20));

  ASSERT_EQ(values.count(), 3u);
  EXPECT_EQ(*values[0], 10);
  EXPECT_EQ(*values[1], 20);
  EXPECT_EQ(*values[2], 30);

  auto extracted = values.takeAt(1);
  ASSERT_NE(extracted, nullptr);
  EXPECT_EQ(*extracted, 20);
  ASSERT_TRUE(values.move(0, 1));
  EXPECT_EQ(*values[0], 30);
  EXPECT_EQ(*values[1], 10);

  NamedVector<std::unique_ptr<int>> moved(std::move(values));
  EXPECT_TRUE(values.isEmpty());
  EXPECT_STREQ(moved.name().value, "move-only");
  ASSERT_EQ(moved.count(), 2u);
  EXPECT_EQ(*moved[0], 30);
  EXPECT_EQ(*moved[1], 10);
}

TEST(NamedVectorStandaloneTest, EmptyAliasesAgree)
{
  NamedVector<int> values;
  EXPECT_TRUE(values.empty());
  EXPECT_FALSE(values.notEmpty());
  values.add(5);
  EXPECT_FALSE(values.empty());
  EXPECT_TRUE(values.notEmpty());
}

TEST(NamedVectorStandaloneTest, ReserveDoesNotChangeValues)
{
  NamedVector<int> values(ContainerName{"reserved"}, {7, 8});
  values.reserve(32);
  EXPECT_GE(values.capacity(), 32u);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{7, 8}));
}

TEST(NamedVectorStandaloneTest, SqueezeReleasesReservedCapacityAndRecordsCapacityChange)
{
  NamedVector<int> values(ContainerName{"squeeze"}, {7, 8, 9});
  values.reserve(32);
  ASSERT_GE(values.capacity(), 32u);
  const auto capacityChangesAfterReserve = values.counters().capacityChangeCount;

  values.squeeze();

  EXPECT_EQ(values.capacity(), values.count());
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{7, 8, 9}));
  EXPECT_EQ(values.counters().capacityChangeCount, capacityChangesAfterReserve + 1);
  EXPECT_STREQ(values.lastMutation().operation, "squeeze");
  ASSERT_EQ(values.mutationHistorySize(), 2u);
  ASSERT_NE(values.mutationHistoryAt(1), nullptr);
  EXPECT_STREQ(values.mutationHistoryAt(1)->operation, "squeeze");
}

TEST(NamedVectorStandaloneTest, ResizeChangesCount)
{
  NamedVector<int> values(ContainerName{"resize"}, {1, 2, 3});
  values.resize(2);
  EXPECT_EQ(values.count(), 2u);
  EXPECT_EQ(values[1], 2);
}

TEST(NamedVectorStandaloneTest, ClearRemovesAllValues)
{
  NamedVector<int> values(ContainerName{"clear"}, {1, 2});
  values.clear();
  EXPECT_TRUE(values.isEmpty());
}

TEST(NamedVectorStandaloneTest, InsertAtIndexPreservesRelativeOrder)
{
  NamedVector<int> values(ContainerName{"insert"}, {1, 3});
  values.insert(1, 2);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 2, 3}));
}

TEST(NamedVectorStandaloneTest, InsertAtOrPastEndAppends)
{
  NamedVector<int> values(ContainerName{"insert"}, {1});
  values.insert(1, 2);
  values.insert(99, 3);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 2, 3}));
}

TEST(NamedVectorStandaloneTest, ReplaceChangesExistingValue)
{
  NamedVector<int> values(ContainerName{"replace"}, {1, 2});
  ASSERT_TRUE(values.replace(1, 9));
  EXPECT_EQ(values[1], 9);
}

TEST(NamedVectorStandaloneTest, ReplaceRejectsOutOfRangeIndex)
{
  NamedVector<int> values(ContainerName{"replace"}, {1});
  EXPECT_FALSE(values.replace(1, 9));
  EXPECT_EQ(values[0], 1);
}

TEST(NamedVectorStandaloneTest, AtReturnsNullForOutOfRangeIndex)
{
  NamedVector<int> values;
  EXPECT_EQ(values.at(0), nullptr);
}

TEST(NamedVectorStandaloneTest, FirstAndLastReturnNullWhenEmpty)
{
  NamedVector<int> values;
  EXPECT_EQ(values.first(), nullptr);
  EXPECT_EQ(values.last(), nullptr);
}

TEST(NamedVectorStandaloneTest, FrontAndBackAliasFirstAndLast)
{
  NamedVector<int> values(ContainerName{"ends"}, {3, 4});
  EXPECT_EQ(values.front(), values.first());
  EXPECT_EQ(values.back(), values.last());
  EXPECT_EQ(*values.front(), 3);
  EXPECT_EQ(*values.back(), 4);
}

TEST(NamedVectorStandaloneTest, RemoveAtRejectsOutOfRangeIndex)
{
  NamedVector<int> values(ContainerName{"remove"}, {3});
  EXPECT_FALSE(values.removeAt(1));
  EXPECT_EQ(values.count(), 1u);
}

TEST(NamedVectorStandaloneTest, TryTakeAtTransfersValueAndPreservesRemainingOrder)
{
  NamedVector<int> values(ContainerName{"try-take"}, {10, 20, 30});
  const auto versionBefore = values.counters().version;
  const auto removedBefore = values.counters().removedCount;
  int result = -1;

  ASSERT_TRUE(values.tryTakeAt(1, result));

  EXPECT_EQ(result, 20);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{10, 30}));
  EXPECT_EQ(values.counters().version, versionBefore + 1);
  EXPECT_EQ(values.counters().removedCount, removedBefore + 1);
  EXPECT_STREQ(values.lastMutation().operation, "tryTakeAt");
  EXPECT_EQ(values.lastMutation().countBefore, 3u);
  EXPECT_EQ(values.lastMutation().countAfter, 2u);
}

TEST(NamedVectorStandaloneTest, TryTakeAtOutOfRangePreservesOutputAndMutationState)
{
  NamedVector<int> values(ContainerName{"try-take"}, {10, 20});
  const auto versionBefore = values.counters().version;
  const auto historySizeBefore = values.mutationHistorySize();
  int result = 77;

  EXPECT_FALSE(values.tryTakeAt(2, result));

  EXPECT_EQ(result, 77);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{10, 20}));
  EXPECT_EQ(values.counters().version, versionBefore);
  EXPECT_EQ(values.counters().failedAccessCount, 1u);
  EXPECT_EQ(values.mutationHistorySize(), historySizeBefore);
}

TEST(NamedVectorStandaloneTest, MoveReordersValues)
{
  NamedVector<int> values(ContainerName{"move"}, {1, 2, 3});
  ASSERT_TRUE(values.move(0, 2));
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{2, 3, 1}));
}

TEST(NamedVectorStandaloneTest, MoveRejectsInvalidIndices)
{
  NamedVector<int> values(ContainerName{"move"}, {1, 2});
  EXPECT_FALSE(values.move(0, 2));
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 2}));
}

TEST(NamedVectorStandaloneTest, SwapItemsAtExchangesValues)
{
  NamedVector<int> values(ContainerName{"swap"}, {1, 2, 3});
  ASSERT_TRUE(values.swapItemsAt(0, 2));
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{3, 2, 1}));
}

TEST(NamedVectorStandaloneTest, ContainsAndIndexSearchHandleDuplicates)
{
  const NamedVector<int> values(ContainerName{"search"}, {2, 4, 2});
  EXPECT_TRUE(values.contains(2));
  EXPECT_EQ(values.indexOf(2), 0);
  EXPECT_EQ(values.indexOf(2, 1), 2);
  EXPECT_EQ(values.lastIndexOf(2), 2);
  EXPECT_EQ(values.indexOf(9), -1);
}

TEST(NamedVectorStandaloneTest, SearchHelpersHandleEmptyAndPastEndPositions)
{
  const NamedVector<int> empty;
  EXPECT_FALSE(empty.contains(1));
  EXPECT_EQ(empty.indexOf(1), -1);
  EXPECT_EQ(empty.lastIndexOf(1), -1);
  EXPECT_FALSE(empty.startsWith(1));
  EXPECT_FALSE(empty.endsWith(1));

  const NamedVector<int> values(ContainerName{"search-bounds"}, {4, 7, 4});
  EXPECT_EQ(values.indexOf(4, values.count()), -1);
  EXPECT_EQ(values.indexOf(4, values.count() + 2), -1);
  EXPECT_EQ(values.lastIndexOf(7), 1);
}

TEST(NamedVectorStandaloneTest, PrefixSuffixAndRemovalHelpers)
{
  NamedVector<int> values(ContainerName{"helpers"}, {1, 2, 2, 3});
  EXPECT_TRUE(values.startsWith(1));
  EXPECT_TRUE(values.endsWith(3));
  EXPECT_TRUE(values.removeOne(2));
  EXPECT_EQ(values.removeAll(2), 1u);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 3}));
}

TEST(NamedVectorStandaloneTest, RemoveIfCompactsMatchingValues)
{
  NamedVector<int> values(ContainerName{"remove-if"}, {1, 2, 3, 4});
  EXPECT_EQ(values.removeIf([](int value) { return value % 2 == 0; }), 2u);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 3}));
}

TEST(NamedVectorStandaloneTest, TakeAndPopRemoveExpectedEnds)
{
  NamedVector<int> values(ContainerName{"take"}, {1, 2, 3});
  EXPECT_EQ(values.takeFirst(), 1);
  EXPECT_EQ(values.takeLast(), 3);
  EXPECT_TRUE(values.popBack());
  EXPECT_TRUE(values.isEmpty());
}

TEST(NamedVectorStandaloneTest, FromStdVectorMovesValuesAndKeepsName)
{
  auto values = NamedVector<int>::fromStdVector(ContainerName{"converted"}, {5, 6});
  EXPECT_STREQ(values.name().value, "converted");
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{5, 6}));
}

TEST(NamedVectorStandaloneTest, IterationAndEachExposeValuesInOrder)
{
  const NamedVector<int> values(ContainerName{"iteration"}, {4, 5, 6});
  std::vector<int> iterated;
  for (const auto value : values) iterated.push_back(value);
  EXPECT_EQ(iterated, (std::vector<int>{4, 5, 6}));
  iterated.clear();
  values.each([&](int value) { iterated.push_back(value); });
  EXPECT_EQ(iterated, (std::vector<int>{4, 5, 6}));
}

TEST(NamedVectorStandaloneTest, EachIndexedReportsZeroBasedIndices)
{
  const NamedVector<int> values(ContainerName{"indexed"}, {8, 9});
  std::vector<int> seen;
  values.eachIndexed([&](std::size_t index, int value) {
    seen.push_back(static_cast<int>(index) * 10 + value);
  });
  EXPECT_EQ(seen, (std::vector<int>{8, 19}));
}

TEST(NamedVectorStandaloneTest, MutableEachCanEditElements)
{
  NamedVector<int> values(ContainerName{"mutable"}, {1, 2});
  values.mutableEach([](int& value) { value *= 3; });
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{3, 6}));
}

TEST(NamedVectorStandaloneTest, DebugCheckpointRejectsStaleOrForeignRollbackAndRestoresValues)
{
  NamedVector<int> values(ContainerName{"checkpoint"}, {1, 2});
  const auto checkpoint = values.createDebugCheckpoint();
  values.add(3);
  const auto currentVersion = values.counters().version;

  const auto verification = values.verifyDebugCheckpoint(checkpoint);
  EXPECT_TRUE(verification.sourceMatches);
  EXPECT_FALSE(verification.unchangedSinceCheckpoint);
  EXPECT_EQ(verification.expectedCount, 2u);
  EXPECT_EQ(verification.actualCount, 3u);

  NamedVector<int> other(ContainerName{"other"}, {9});
  const auto foreignCheckpoint = other.createDebugCheckpoint();
  EXPECT_FALSE(values.rollbackToDebugCheckpoint(foreignCheckpoint, currentVersion));
  EXPECT_FALSE(values.rollbackToDebugCheckpoint(checkpoint, currentVersion - 1));
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 2, 3}));

  ASSERT_TRUE(values.rollbackToDebugCheckpoint(checkpoint, currentVersion));
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 2}));
  EXPECT_EQ(values.counters().version, currentVersion + 1);
  EXPECT_STREQ(values.lastMutation().operation, "debugRollback");
  ASSERT_EQ(values.debugNotes().size(), 1u);
  EXPECT_STREQ(values.debugNotes()[0].text.c_str(), "restored values from debug checkpoint");
}

TEST(NamedVectorStandaloneTest, DebugNotesAreBoundedAndRejectInvalidTextSizes)
{
  NamedVector<int> values;
  for (int index = 0; index < 34; ++index) {
    ASSERT_TRUE(values.addDebugNote("note-" + std::to_string(index)));
  }

  ASSERT_EQ(values.debugNotes().size(), 32u);
  EXPECT_STREQ(values.debugNotes()[0].text.c_str(), "note-2");
  EXPECT_STREQ(values.debugNotes()[31].text.c_str(), "note-33");
  EXPECT_FALSE(values.addDebugNote(""));
  EXPECT_FALSE(values.addDebugNote(std::string(1025, 'x')));
  EXPECT_EQ(values.debugNotes().size(), 32u);
  EXPECT_STREQ(values.debugNotes()[0].text.c_str(), "note-2");
}
