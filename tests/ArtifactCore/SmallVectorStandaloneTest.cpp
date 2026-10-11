import Container.SmallVector;

#include <gtest/gtest.h>

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

