import Container.NameMap;

#include <gtest/gtest.h>

#include <map>
#include <vector>

using ArtifactCore::ContainerName;
using ArtifactCore::NameMap;

TEST(NameMapStandaloneTest, DefaultConstructionIsEmpty)
{
  const NameMap<int, int> values;
  EXPECT_TRUE(values.isEmpty());
  EXPECT_EQ(values.count(), 0u);
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

TEST(NameMapStandaloneTest, ValueReturnsDefaultForMissingKey)
{
  const NameMap<int, int> values;
  EXPECT_EQ(values.value(99, -1), -1);
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

TEST(NameMapStandaloneTest, TryTakeTransfersAndErasesEntry)
{
  NameMap<int, int> values;
  values.insert(6, 60);
  int result = 0;
  ASSERT_TRUE(values.tryTake(6, result));
  EXPECT_EQ(result, 60);
  EXPECT_FALSE(values.contains(6));
}

TEST(NameMapStandaloneTest, TryTakeMissingKeyLeavesOutputUntouched)
{
  NameMap<int, int> values;
  int result = 77;
  EXPECT_FALSE(values.tryTake(6, result));
  EXPECT_EQ(result, 77);
}

TEST(NameMapStandaloneTest, EraseReturnsOneThenZero)
{
  NameMap<int, int> values;
  values.insert(7, 70);
  EXPECT_EQ(values.erase(7), 1u);
  EXPECT_EQ(values.erase(7), 0u);
}

TEST(NameMapStandaloneTest, RemoveReportsSuccessAndMissingKey)
{
  NameMap<int, int> values;
  values.insert(8, 80);
  EXPECT_TRUE(values.remove(8));
  EXPECT_FALSE(values.remove(8));
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

TEST(NameMapStandaloneTest, ClearRemovesAllEntries)
{
  NameMap<int, int> values;
  values.insert(1, 10);
  values.insert(2, 20);
  values.clear();
  EXPECT_TRUE(values.isEmpty());
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

TEST(NameMapStandaloneTest, ToStdMapReturnsSortedEntries)
{
  NameMap<int, int> values;
  values.insert(4, 40);
  values.insert(1, 10);
  const std::map<int, int> sorted = values.toStdMap();
  EXPECT_EQ(sorted, (std::map<int, int>{{1, 10}, {4, 40}}));
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

