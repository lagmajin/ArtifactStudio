import Container.Algorithms;

#include <gtest/gtest.h>

#include <vector>

using ArtifactCore::NamedVector;
using ArtifactCore::SmallVector;

TEST(ContainerAlgorithmsStandaloneTest, EachVisitsNamedVectorInOrder)
{
  const NamedVector<int> values(ArtifactCore::ContainerName{"named"}, {1, 2, 3});
  std::vector<int> visited;
  ArtifactCore::each(values, [&](int value) { visited.push_back(value); });
  EXPECT_EQ(visited, (std::vector<int>{1, 2, 3}));
}

TEST(ContainerAlgorithmsStandaloneTest, EachVisitsEmptyNamedVectorZeroTimes)
{
  const NamedVector<int> values;
  int visits = 0;
  ArtifactCore::each(values, [&](int) { ++visits; });
  EXPECT_EQ(visits, 0);
}

TEST(ContainerAlgorithmsStandaloneTest, RemoveIfFromNamedVectorRemovesMatches)
{
  NamedVector<int> values(ArtifactCore::ContainerName{"named"}, {1, 2, 3, 4});
  EXPECT_EQ(ArtifactCore::removeIf(values, [](int value) { return value % 2 == 0; }), 2u);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 3}));
}

TEST(ContainerAlgorithmsStandaloneTest, RemoveIfFromNamedVectorHandlesAdjacentMatches)
{
  NamedVector<int> values(ArtifactCore::ContainerName{"named"}, {1, 2, 4, 6, 3});
  EXPECT_EQ(ArtifactCore::removeIf(values, [](int value) { return value % 2 == 0; }), 3u);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 3}));
}

TEST(ContainerAlgorithmsStandaloneTest, RemoveIfFromNamedVectorCanRemoveEverything)
{
  NamedVector<int> values(ArtifactCore::ContainerName{"named"}, {2, 4, 6});
  EXPECT_EQ(ArtifactCore::removeIf(values, [](int value) { return value > 0; }), 3u);
  EXPECT_TRUE(values.isEmpty());
}

TEST(ContainerAlgorithmsStandaloneTest, RemoveIfFromNamedVectorWithoutMatchesIsNoOp)
{
  NamedVector<int> values(ArtifactCore::ContainerName{"named"}, {1, 3, 5});
  EXPECT_EQ(ArtifactCore::removeIf(values, [](int value) { return value % 2 == 0; }), 0u);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 3, 5}));
}

TEST(ContainerAlgorithmsStandaloneTest, ToStdVectorCopiesNamedVectorValues)
{
  const NamedVector<int> values(ArtifactCore::ContainerName{"named"}, {8, 9});
  const auto copy = ArtifactCore::toStdVector(values);
  EXPECT_EQ(copy, (std::vector<int>{8, 9}));
}

TEST(ContainerAlgorithmsStandaloneTest, ToStdVectorFromNamedVectorIsIndependent)
{
  NamedVector<int> values(ArtifactCore::ContainerName{"named"}, {8, 9});
  auto copy = ArtifactCore::toStdVector(values);
  copy[0] = 100;
  EXPECT_EQ(values[0], 8);
}

TEST(ContainerAlgorithmsStandaloneTest, EachVisitsSmallVectorInOrder)
{
  const SmallVector<int, 4> values(ArtifactCore::ContainerName{"small"}, {1, 2, 3});
  std::vector<int> visited;
  ArtifactCore::each(values, [&](int value) { visited.push_back(value); });
  EXPECT_EQ(visited, (std::vector<int>{1, 2, 3}));
}

TEST(ContainerAlgorithmsStandaloneTest, EachVisitsEmptySmallVectorZeroTimes)
{
  const SmallVector<int, 4> values;
  int visits = 0;
  ArtifactCore::each(values, [&](int) { ++visits; });
  EXPECT_EQ(visits, 0);
}

TEST(ContainerAlgorithmsStandaloneTest, RemoveIfFromSmallVectorRemovesMatches)
{
  SmallVector<int, 4> values(ArtifactCore::ContainerName{"small"}, {1, 2, 3, 4});
  EXPECT_EQ(ArtifactCore::removeIf(values, [](int value) { return value % 2 == 0; }), 2u);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 3}));
}

TEST(ContainerAlgorithmsStandaloneTest, RemoveIfFromSmallVectorHandlesAdjacentMatches)
{
  SmallVector<int, 6> values(ArtifactCore::ContainerName{"small"}, {1, 2, 4, 6, 3});
  EXPECT_EQ(ArtifactCore::removeIf(values, [](int value) { return value % 2 == 0; }), 3u);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 3}));
}

TEST(ContainerAlgorithmsStandaloneTest, RemoveIfFromSmallVectorCanRemoveEverything)
{
  SmallVector<int, 4> values(ArtifactCore::ContainerName{"small"}, {2, 4, 6});
  EXPECT_EQ(ArtifactCore::removeIf(values, [](int value) { return value > 0; }), 3u);
  EXPECT_TRUE(values.isEmpty());
}

TEST(ContainerAlgorithmsStandaloneTest, RemoveIfFromSmallVectorWithoutMatchesIsNoOp)
{
  SmallVector<int, 4> values(ArtifactCore::ContainerName{"small"}, {1, 3, 5});
  EXPECT_EQ(ArtifactCore::removeIf(values, [](int value) { return value % 2 == 0; }), 0u);
  EXPECT_EQ(values.toStdVector(), (std::vector<int>{1, 3, 5}));
}

TEST(ContainerAlgorithmsStandaloneTest, ToStdVectorCopiesSmallVectorValues)
{
  const SmallVector<int, 4> values(ArtifactCore::ContainerName{"small"}, {8, 9});
  const auto copy = ArtifactCore::toStdVector(values);
  EXPECT_EQ(copy, (std::vector<int>{8, 9}));
}

TEST(ContainerAlgorithmsStandaloneTest, ToStdVectorFromSmallVectorIsIndependent)
{
  SmallVector<int, 4> values(ArtifactCore::ContainerName{"small"}, {8, 9});
  auto copy = ArtifactCore::toStdVector(values);
  copy[0] = 100;
  EXPECT_EQ(values[0], 8);
}

TEST(ContainerAlgorithmsStandaloneTest, RemoveIfPredicateReceivesCurrentValues)
{
  NamedVector<int> values(ArtifactCore::ContainerName{"predicate"}, {1, 2, 3, 4});
  std::vector<int> inspected;
  ArtifactCore::removeIf(values, [&](int value) {
    inspected.push_back(value);
    return value == 2 || value == 4;
  });
  EXPECT_EQ(inspected, (std::vector<int>{1, 2, 3, 4}));
}

TEST(ContainerAlgorithmsStandaloneTest, RemoveIfHandlesSingletonContainers)
{
  NamedVector<int> named(ArtifactCore::ContainerName{"one"}, {3});
  SmallVector<int, 2> small(ArtifactCore::ContainerName{"one"}, {3});
  EXPECT_EQ(ArtifactCore::removeIf(named, [](int value) { return value == 3; }), 1u);
  EXPECT_EQ(ArtifactCore::removeIf(small, [](int value) { return value == 3; }), 1u);
  EXPECT_TRUE(named.isEmpty());
  EXPECT_TRUE(small.isEmpty());
}
