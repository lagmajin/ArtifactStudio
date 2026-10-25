import Container.SmallVector;

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

using ArtifactCore::ContainerName;
using ArtifactCore::SmallVector;

TEST(SmallVectorStandaloneTest, DefaultVectorIsEmptyWithInlineCapacity)
{
  const SmallVector<int, 3> values;
  EXPECT_TRUE(values.isEmpty());
  EXPECT_EQ(values.count(), 0u);
  EXPECT_EQ(values.capacity(), 3u);
}

TEST(SmallVectorStandaloneTest, InitializerListPreservesValues)
{
  const SmallVector<int, 4> values(ContainerName{"initial"}, {1, 2, 3});
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 2, 3}));
}

TEST(SmallVectorStandaloneTest, ValuesFitInlineCapacity)
{
  SmallVector<int, 3> values;
  values.add(1);
  values.add(2);
  values.add(3);
  EXPECT_EQ(values.capacity(), 3u);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 2, 3}));
}

TEST(SmallVectorStandaloneTest, AddingPastInlineCapacityPreservesValues)
{
  SmallVector<int, 2> values;
  values.add(1);
  values.add(2);
  values.add(3);
  EXPECT_GE(values.capacity(), 3u);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 2, 3}));
}

TEST(SmallVectorStandaloneTest, AddCanReadAliasedElementAcrossGrowth)
{
  SmallVector<std::string, 2> values(ContainerName{"alias-add"}, {"first", "last"});
  const std::string& appended = values[0];

  values.add(appended);

  EXPECT_EQ(values.toStdVector(), (std::vector<std::string>{"first", "last", "first"}));
}

TEST(SmallVectorStandaloneTest, ReserveGrowsWithoutChangingElements)
{
  SmallVector<int, 2> values(ContainerName{"reserve"}, {7, 8});
  values.reserve(16);
  EXPECT_GE(values.capacity(), 16u);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{7, 8}));
}

TEST(SmallVectorStandaloneTest, ResizeDownDestroysTailAndKeepsPrefix)
{
  SmallVector<int, 4> values(ContainerName{"resize"}, {1, 2, 3});
  values.resize(2);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 2}));
}

TEST(SmallVectorStandaloneTest, ResizeUpValueInitializesNewElements)
{
  SmallVector<int, 2> values(ContainerName{"resize"}, {5});
  values.resize(3);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{5, 0, 0}));
}

TEST(SmallVectorStandaloneTest, InsertWithinRangeShiftsElements)
{
  SmallVector<int, 4> values(ContainerName{"insert"}, {1, 3, 4});
  values.insert(1, 2);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 2, 3, 4}));
}

TEST(SmallVectorStandaloneTest, InsertCanReadAliasedElementAfterShifting)
{
  SmallVector<std::string, 4> values(ContainerName{"alias-insert"}, {"first", "middle", "last"});
  const std::string& inserted = values[2];

  values.insert(0, inserted);

  EXPECT_EQ(values.toStdVector(),
            (std::vector<std::string>{"last", "first", "middle", "last"}));
}

TEST(SmallVectorStandaloneTest, InsertAtOrPastEndAppends)
{
  SmallVector<int, 2> values(ContainerName{"insert"}, {1});
  values.insert(1, 2);
  values.insert(99, 3);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 2, 3}));
}

TEST(SmallVectorStandaloneTest, AssignReplacesValuesAndCanGrow)
{
  SmallVector<int, 2> values(ContainerName{"assign"}, {1, 2});
  values.assign({4, 5, 6});
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{4, 5, 6}));
}

TEST(SmallVectorStandaloneTest, AtRejectsOutOfRangeIndex)
{
  SmallVector<int, 2> values;
  EXPECT_EQ(values.at(0), nullptr);
}

TEST(SmallVectorStandaloneTest, RemoveAtCompactsFollowingElements)
{
  SmallVector<int, 4> values(ContainerName{"remove"}, {1, 2, 3, 4});
  ASSERT_TRUE(values.removeAt(1));
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 3, 4}));
}

TEST(SmallVectorStandaloneTest, RemoveAtRejectsOutOfRangeIndex)
{
  SmallVector<int, 2> values(ContainerName{"remove"}, {1});
  EXPECT_FALSE(values.removeAt(1));
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1}));
}

TEST(SmallVectorStandaloneTest, SwapItemsAtExchangesValues)
{
  SmallVector<int, 3> values(ContainerName{"swap"}, {1, 2, 3});
  ASSERT_TRUE(values.swapItemsAt(0, 2));
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{3, 2, 1}));
}

TEST(SmallVectorStandaloneTest, PopBackAndPopFrontRemoveExpectedEnds)
{
  SmallVector<int, 3> values(ContainerName{"pop"}, {1, 2, 3});
  ASSERT_TRUE(values.popFront());
  ASSERT_TRUE(values.popBack());
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{2}));
}

TEST(SmallVectorStandaloneTest, FailedPopOnEmptyReturnsFalse)
{
  SmallVector<int, 2> values;
  EXPECT_FALSE(values.popBack());
  EXPECT_FALSE(values.popFront());
}

TEST(SmallVectorStandaloneTest, TakeAtReturnsAndRemovesSelectedValue)
{
  SmallVector<int, 3> values(ContainerName{"take"}, {1, 2, 3});
  EXPECT_EQ(values.takeAt(1), 2);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 3}));
}

TEST(SmallVectorStandaloneTest, RemoveIfHandlesAdjacentMatches)
{
  SmallVector<int, 2> values(ContainerName{"filter"}, {1, 2, 4, 6, 3});
  EXPECT_EQ(values.removeIf([](int value) { return value % 2 == 0; }), 3u);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 3}));
}

TEST(SmallVectorStandaloneTest, RemoveIfWithNoMatchesPreservesMutationDiagnostics)
{
  SmallVector<int, 3> values(ContainerName{"filter-no-match"}, {1, 3, 5});
  const auto checkpoint = values.debugCheckpoint();
  const auto historySize = values.mutationHistorySize();
  const auto mutationCount = values.counters().mutationCount;

  EXPECT_EQ(values.removeIf([](int) { return false; }), 0u);

  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 3, 5}));
  EXPECT_EQ(values.mutationHistorySize(), historySize);
  EXPECT_EQ(values.counters().mutationCount, mutationCount);
  EXPECT_FALSE(values.watchSince(checkpoint, [](const auto&) {}));
}

TEST(SmallVectorStandaloneTest, MutationHistoryKeepsOnlyTheMostRecentEightRecords)
{
  SmallVector<int, 2> values(ContainerName{"bounded-history"});
  for (int value = 0; value < 10; ++value) {
    values.append(value);
  }

  ASSERT_EQ(values.mutationHistorySize(), 8u);
  ASSERT_NE(values.mutationHistoryAt(0), nullptr);
  ASSERT_NE(values.mutationHistoryAt(7), nullptr);
  EXPECT_EQ(values.mutationHistoryAt(0)->version, 3u);
  EXPECT_EQ(values.mutationHistoryAt(7)->version, 10u);
  EXPECT_STREQ(values.mutationHistoryAt(0)->operation, "add");
  EXPECT_EQ(values.mutationHistoryAt(0)->countBefore, 2u);
  EXPECT_EQ(values.mutationHistoryAt(7)->countAfter, 10u);
}

TEST(SmallVectorStandaloneTest, WatchSinceIgnoresReadsAndReportsFailedAccess)
{
  SmallVector<int, 2> values(ContainerName{"watch-access"}, {5});
  const auto checkpoint = values.debugCheckpoint();
  int hits = 0;
  std::size_t reportedFailedAccesses = 0;
  const auto onHit = [&](const auto& hit) {
    ++hits;
    EXPECT_STREQ(hit.reason, "smallvector-changed-since-checkpoint");
    reportedFailedAccesses = hit.snapshot.counters.failedAccessCount;
  };

  EXPECT_EQ(values.size(), 1u);
  EXPECT_FALSE(values.watchSince(checkpoint, onHit));
  EXPECT_EQ(hits, 0);

  EXPECT_EQ(values.at(4), nullptr);

  EXPECT_TRUE(values.watchSince(checkpoint, onHit));
  EXPECT_EQ(hits, 1);
  EXPECT_EQ(reportedFailedAccesses, 1u);
}

TEST(SmallVectorStandaloneTest, SearchHelpersHandleDuplicatesAndBounds)
{
  const SmallVector<int, 4> values(ContainerName{"search"}, {2, 4, 2});
  EXPECT_TRUE(values.contains(4));
  EXPECT_EQ(values.indexOf(2), 0);
  EXPECT_EQ(values.indexOf(2, 1), 2);
  EXPECT_EQ(values.lastIndexOf(2), 2);
  EXPECT_EQ(values.indexOf(7), -1);
}

TEST(SmallVectorStandaloneTest, PrefixAndSuffixHelpersCheckEnds)
{
  const SmallVector<int, 3> values(ContainerName{"ends"}, {3, 4, 5});
  EXPECT_TRUE(values.startsWith(3));
  EXPECT_TRUE(values.endsWith(5));
  EXPECT_FALSE(values.startsWith(4));
}

TEST(SmallVectorStandaloneTest, FrontAndBackReturnNullWhenEmptyAndEndpointPointersWhenPopulated)
{
  SmallVector<int, 3> values;
  EXPECT_EQ(values.front(), nullptr);
  EXPECT_EQ(values.back(), nullptr);

  values.add(11);
  values.add(22);
  values.add(33);
  EXPECT_EQ(values.front(), &values[0]);
  EXPECT_EQ(values.back(), &values[2]);
  EXPECT_EQ(*values.front(), 11);
  EXPECT_EQ(*values.back(), 33);

  const auto& readOnly = values;
  EXPECT_EQ(readOnly.front(), &readOnly[0]);
  EXPECT_EQ(readOnly.back(), &readOnly[2]);
}

TEST(SmallVectorStandaloneTest, CopyConstructionCopiesHeapBackedStorage)
{
  const SmallVector<int, 2> original(ContainerName{"copy"}, {1, 2, 3, 4});
  SmallVector<int, 2> copied(original);
  copied[0] = 99;
  EXPECT_EQ(original[0], 1);
  EXPECT_EQ(copied.toStdVector(), (std::vector<int>{99, 2, 3, 4}));
}

TEST(SmallVectorStandaloneTest, CopyConstructionCopiesInlineStorage)
{
  const SmallVector<int, 4> original(ContainerName{"inline-copy"}, {1, 2, 3});
  SmallVector<int, 4> copied(original);
  copied[0] = 99;
  EXPECT_EQ(original.toStdVector(), (std::vector<int>{1, 2, 3}));
  EXPECT_EQ(copied.toStdVector(), (std::vector<int>{99, 2, 3}));
}

TEST(SmallVectorStandaloneTest, CopyAssignmentReplacesExistingStorage)
{
  SmallVector<int, 2> original(ContainerName{"copy"}, {1, 2, 3});
  SmallVector<int, 2> assigned(ContainerName{"other"}, {9});
  assigned = original;
  assigned[1] = 88;
  EXPECT_EQ(original.toStdVector(), (std::vector<int>{1, 2, 3}));
  EXPECT_EQ(assigned.toStdVector(), (std::vector<int>{1, 88, 3}));
}

TEST(SmallVectorStandaloneTest, MoveConstructionLeavesSourceEmpty)
{
  SmallVector<int, 2> original(ContainerName{"move"}, {1, 2, 3, 4});
  SmallVector<int, 2> moved(std::move(original));
  EXPECT_TRUE(original.isEmpty());
  EXPECT_EQ(moved.toStdVector(), (std::vector<int>{1, 2, 3, 4}));
}

TEST(SmallVectorStandaloneTest, MoveConstructionTransfersInlineElements)
{
  SmallVector<int, 4> original(ContainerName{"inline-move"}, {1, 2, 3});
  SmallVector<int, 4> moved(std::move(original));
  EXPECT_TRUE(original.isEmpty());
  EXPECT_EQ(moved.toStdVector(), (std::vector<int>{1, 2, 3}));
}

TEST(SmallVectorStandaloneTest, SelfMoveAssignmentPreservesInlineAndHeapStorage)
{
  SmallVector<int, 2> inlineValues;
  inlineValues.add(3);
  inlineValues.add(7);
  const auto inlineCapacity = inlineValues.capacity();
  inlineValues = std::move(inlineValues);
  ASSERT_EQ(inlineValues.count(), 2u);
  EXPECT_EQ(inlineValues[0], 3);
  EXPECT_EQ(inlineValues[1], 7);
  EXPECT_EQ(inlineValues.capacity(), inlineCapacity);

  SmallVector<int, 2> heapValues;
  heapValues.add(5);
  heapValues.add(11);
  heapValues.add(13);
  const auto heapCapacity = heapValues.capacity();
  heapValues = std::move(heapValues);
  ASSERT_EQ(heapValues.count(), 3u);
  EXPECT_EQ(heapValues[0], 5);
  EXPECT_EQ(heapValues[1], 11);
  EXPECT_EQ(heapValues[2], 13);
  EXPECT_EQ(heapValues.capacity(), heapCapacity);
}

TEST(SmallVectorStandaloneTest, MoveAssignmentTransfersValuesAndReleasesOldContents)
{
  SmallVector<int, 2> original(ContainerName{"move"}, {1, 2, 3, 4});
  SmallVector<int, 2> target(ContainerName{"target"}, {9, 8, 7});
  target = std::move(original);
  EXPECT_TRUE(original.isEmpty());
  EXPECT_EQ(target.toStdVector(), (std::vector<int>{1, 2, 3, 4}));
}

TEST(SmallVectorStandaloneTest, ClearRetainsReusableContainer)
{
  SmallVector<int, 2> values(ContainerName{"clear"}, {1, 2, 3});
  values.clear();
  EXPECT_TRUE(values.isEmpty());
  values.add(7);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{7}));
}

TEST(SmallVectorStandaloneTest, StringElementsSurviveGrowthAndRemoval)
{
  SmallVector<std::string, 1> values(ContainerName{"strings"}, {"alpha", "beta", "gamma"});
  EXPECT_EQ(values.removeIf([](const std::string& value) { return value == "beta"; }), 1u);
  EXPECT_EQ(values.toStdVector(), (std::vector<std::string>{"alpha", "gamma"}));
}

TEST(SmallVectorStandaloneTest, MoveOnlyElementsSurviveGrowthInsertAndTakeAt)
{
  SmallVector<std::unique_ptr<int>, 2> values(ContainerName{"move-only"});
  values.add(std::make_unique<int>(1));
  values.add(std::make_unique<int>(3));
  values.insert(1, std::make_unique<int>(2));

  ASSERT_EQ(values.size(), 3u);
  ASSERT_TRUE(values[0]);
  ASSERT_TRUE(values[1]);
  ASSERT_TRUE(values[2]);
  EXPECT_EQ(*values[0], 1);
  EXPECT_EQ(*values[1], 2);
  EXPECT_EQ(*values[2], 3);

  auto taken = values.takeAt(1);

  ASSERT_TRUE(taken);
  EXPECT_EQ(*taken, 2);
  ASSERT_EQ(values.size(), 2u);
  ASSERT_TRUE(values[0]);
  ASSERT_TRUE(values[1]);
  EXPECT_EQ(*values[0], 1);
  EXPECT_EQ(*values[1], 3);
}

