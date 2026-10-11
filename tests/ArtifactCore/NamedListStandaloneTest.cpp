import Container.NamedList;

#include <gtest/gtest.h>

#include <vector>

using ArtifactCore::ContainerName;
using ArtifactCore::NamedList;

TEST(NamedListStandaloneTest, DefaultConstructionIsEmpty)
{
  const NamedList<int> values;
  EXPECT_TRUE(values.isEmpty());
  EXPECT_EQ(values.count(), 0u);
}

TEST(NamedListStandaloneTest, NamedConstructionPreservesName)
{
  const NamedList<int> values(ContainerName{"events"});
  EXPECT_STREQ(values.name().value, "events");
}

TEST(NamedListStandaloneTest, InitializerListPreservesOrder)
{
  const NamedList<int> values(ContainerName{"events"}, {2, 4, 6});
  EXPECT_EQ(values.count(), 3u);
  EXPECT_EQ(*values.first(), 2);
  EXPECT_EQ(*values.last(), 6);
}

TEST(NamedListStandaloneTest, AddAndAppendAddAtBack)
{
  NamedList<int> values;
  values.add(1);
  values.append(2);
  EXPECT_EQ(std::vector<int>(values.begin(), values.end()), (std::vector<int>{1, 2}));
}

TEST(NamedListStandaloneTest, AddFirstAndPrependAddAtFront)
{
  NamedList<int> values;
  values.add(2);
  values.addFirst(1);
  values.prepend(0);
  EXPECT_EQ(std::vector<int>(values.begin(), values.end()), (std::vector<int>{0, 1, 2}));
}

TEST(NamedListStandaloneTest, EmptyAliasesAgree)
{
  NamedList<int> values;
  EXPECT_TRUE(values.isEmpty());
  EXPECT_FALSE(values.notEmpty());
  values.add(1);
  EXPECT_FALSE(values.isEmpty());
  EXPECT_TRUE(values.notEmpty());
}

TEST(NamedListStandaloneTest, AssignReplacesAllValues)
{
  NamedList<int> values(ContainerName{"assign"}, {1, 2});
  values.assign({7, 8, 9});
  EXPECT_EQ(std::vector<int>(values.begin(), values.end()), (std::vector<int>{7, 8, 9}));
}

TEST(NamedListStandaloneTest, MakeAppendsAndReturnsMutableReference)
{
  NamedList<int> values;
  int& made = values.make(12);
  made = 13;
  ASSERT_EQ(values.count(), 1u);
  EXPECT_EQ(*values.first(), 13);
}

TEST(NamedListStandaloneTest, FirstLastFrontBackExposeEnds)
{
  NamedList<int> values(ContainerName{"ends"}, {3, 4, 5});
  EXPECT_EQ(*values.first(), 3);
  EXPECT_EQ(*values.front(), 3);
  EXPECT_EQ(*values.last(), 5);
  EXPECT_EQ(*values.back(), 5);
}

TEST(NamedListStandaloneTest, EmptyEndAccessReturnsNull)
{
  NamedList<int> values;
  EXPECT_EQ(values.first(), nullptr);
  EXPECT_EQ(values.last(), nullptr);
  EXPECT_EQ(values.front(), nullptr);
  EXPECT_EQ(values.back(), nullptr);
}

TEST(NamedListStandaloneTest, RemoveFirstRemovesHead)
{
  NamedList<int> values(ContainerName{"remove-first"}, {1, 2, 3});
  ASSERT_TRUE(values.removeFirst());
  EXPECT_EQ(*values.first(), 2);
}

TEST(NamedListStandaloneTest, RemoveLastRemovesTail)
{
  NamedList<int> values(ContainerName{"remove-last"}, {1, 2, 3});
  ASSERT_TRUE(values.removeLast());
  EXPECT_EQ(*values.last(), 2);
}

TEST(NamedListStandaloneTest, PopAliasesRemoveEnds)
{
  NamedList<int> values(ContainerName{"pop"}, {1, 2});
  EXPECT_TRUE(values.popFront());
  EXPECT_TRUE(values.popBack());
  EXPECT_TRUE(values.isEmpty());
}

TEST(NamedListStandaloneTest, RemovingFromEmptyReturnsFalse)
{
  NamedList<int> values;
  EXPECT_FALSE(values.removeFirst());
  EXPECT_FALSE(values.removeLast());
  EXPECT_FALSE(values.popFront());
  EXPECT_FALSE(values.popBack());
}

TEST(NamedListStandaloneTest, ContainsFindsMatchingValue)
{
  const NamedList<int> values(ContainerName{"contains"}, {2, 4});
  EXPECT_TRUE(values.contains(4));
  EXPECT_FALSE(values.contains(5));
}

TEST(NamedListStandaloneTest, RemoveOneRemovesOnlyFirstMatch)
{
  NamedList<int> values(ContainerName{"remove-one"}, {2, 4, 2});
  EXPECT_TRUE(values.removeOne(2));
  EXPECT_EQ(std::vector<int>(values.begin(), values.end()), (std::vector<int>{4, 2}));
  EXPECT_FALSE(values.removeOne(9));
}

TEST(NamedListStandaloneTest, RemoveAllRemovesEveryMatchingValue)
{
  NamedList<int> values(ContainerName{"remove-all"}, {2, 4, 2, 6});
  EXPECT_EQ(values.removeAll(2), 2u);
  EXPECT_EQ(std::vector<int>(values.begin(), values.end()), (std::vector<int>{4, 6}));
}

TEST(NamedListStandaloneTest, RemoveIfRetainsNonmatchingValuesInOrder)
{
  NamedList<int> values(ContainerName{"remove-if"}, {1, 2, 3, 4});
  EXPECT_EQ(values.removeIf([](int value) { return value % 2 == 0; }), 2u);
  EXPECT_EQ(std::vector<int>(values.begin(), values.end()), (std::vector<int>{1, 3}));
}

TEST(NamedListStandaloneTest, EachVisitsValuesInOrder)
{
  const NamedList<int> values(ContainerName{"each"}, {3, 5, 7});
  std::vector<int> seen;
  values.each([&](int value) { seen.push_back(value); });
  EXPECT_EQ(seen, (std::vector<int>{3, 5, 7}));
}

TEST(NamedListStandaloneTest, MutableEachEditsValues)
{
  NamedList<int> values(ContainerName{"mutable"}, {2, 3});
  values.mutableEach([](int& value) { value *= 2; });
  EXPECT_EQ(std::vector<int>(values.begin(), values.end()), (std::vector<int>{4, 6}));
}

TEST(NamedListStandaloneTest, SetNameChangesContainerName)
{
  NamedList<int> values(ContainerName{"before"});
  values.setName(ContainerName{"after"});
  EXPECT_STREQ(values.name().value, "after");
}

TEST(NamedListStandaloneTest, ClearRemovesEveryValue)
{
  NamedList<int> values(ContainerName{"clear"}, {1, 2});
  values.clear();
  EXPECT_TRUE(values.isEmpty());
}
