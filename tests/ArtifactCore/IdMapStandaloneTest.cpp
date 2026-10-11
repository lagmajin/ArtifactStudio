import Container.IdMap;

#include <gtest/gtest.h>

#include <string>
#include <unordered_map>
#include <vector>

using ArtifactCore::ContainerName;
using ArtifactCore::IdMap;

TEST(IdMapStandaloneTest, DefaultConstructionIsEmpty)
{
  const IdMap<int, std::string> values;
  EXPECT_TRUE(values.isEmpty());
  EXPECT_EQ(values.count(), 0u);
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

TEST(IdMapStandaloneTest, ValueReturnsProvidedDefaultForMissingKey)
{
  const IdMap<int, std::string> values;
  EXPECT_EQ(values.value(99, "missing"), "missing");
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

TEST(IdMapStandaloneTest, TryTakeMovesOutAndRemovesValue)
{
  IdMap<int, std::string> values;
  values.insert(5, "five");
  std::string result;
  ASSERT_TRUE(values.tryTake(5, result));
  EXPECT_EQ(result, "five");
  EXPECT_FALSE(values.contains(5));
}

TEST(IdMapStandaloneTest, TryTakeMissingKeyReturnsFalse)
{
  IdMap<int, int> values;
  int result = 42;
  EXPECT_FALSE(values.tryTake(3, result));
  EXPECT_EQ(result, 42);
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

TEST(IdMapStandaloneTest, EraseReturnsNumberOfRemovedEntries)
{
  IdMap<int, int> values;
  values.insert(1, 2);
  EXPECT_EQ(values.erase(1), 1u);
  EXPECT_EQ(values.erase(1), 0u);
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

TEST(IdMapStandaloneTest, ClearRemovesAllEntries)
{
  IdMap<int, int> values;
  values.insert(1, 1);
  values.insert(2, 2);
  values.clear();
  EXPECT_TRUE(values.isEmpty());
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

TEST(IdMapStandaloneTest, ToStdUnorderedMapCopiesEntries)
{
  IdMap<int, int> values;
  values.insert(1, 10);
  const std::unordered_map<int, int> copy = values.toStdUnorderedMap();
  ASSERT_EQ(copy.size(), 1u);
  EXPECT_EQ(copy.at(1), 10);
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
