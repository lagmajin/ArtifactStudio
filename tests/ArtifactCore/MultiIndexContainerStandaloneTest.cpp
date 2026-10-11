import Container.MultiIndex;

#include <gtest/gtest.h>

#include <memory>
#include <typeindex>

namespace {

struct BaseItem {
  virtual ~BaseItem() = default;
  int value = 0;
};

struct AlphaItem : BaseItem {};
struct BetaItem : BaseItem {};

using ItemPtr = std::shared_ptr<BaseItem>;
using MultiIndex = ArtifactCore::MultiIndexContainer<ItemPtr, int>;

ItemPtr alpha(int value)
{
  auto item = std::make_shared<AlphaItem>();
  item->value = value;
  return item;
}

ItemPtr beta(int value)
{
  auto item = std::make_shared<BetaItem>();
  item->value = value;
  return item;
}

const std::type_index alphaType{typeid(AlphaItem)};
const std::type_index betaType{typeid(BetaItem)};

} // namespace

TEST(MultiIndexContainerStandaloneTest, DefaultContainerIsEmpty)
{
  const MultiIndex values;
  EXPECT_TRUE(values.all().isEmpty());
  EXPECT_FALSE(values.containsId(1));
}

TEST(MultiIndexContainerStandaloneTest, AddIndexesItemByIdAndType)
{
  MultiIndex values;
  const auto item = alpha(10);
  values.add(item, 1, alphaType);
  EXPECT_EQ(values.findById(1), item);
  EXPECT_EQ(values.findByType(alphaType).size(), 1);
  EXPECT_EQ(values.all().size(), 1);
}

TEST(MultiIndexContainerStandaloneTest, AddSafeIndexesItemByIdAndType)
{
  MultiIndex values;
  const auto item = beta(20);
  values.addSafe(item, 2, betaType);
  EXPECT_EQ(values.findById(2), item);
  EXPECT_EQ(values.findByType(betaType).size(), 1);
}

TEST(MultiIndexContainerStandaloneTest, AddSafeInfersElementType)
{
  MultiIndex values;
  const auto item = alpha(10);
  values.addSafe(item, 3);
  EXPECT_EQ(values.findByType(alphaType).size(), 1);
}

TEST(MultiIndexContainerStandaloneTest, AddTypedIndexesRequestedType)
{
  MultiIndex values;
  const auto item = beta(20);
  values.addTyped<BetaItem>(item, 4);
  EXPECT_EQ(values.findByType(betaType).size(), 1);
}

TEST(MultiIndexContainerStandaloneTest, FindByIdReturnsEmptyPointerForMissingId)
{
  const MultiIndex values;
  EXPECT_FALSE(values.findById(99));
}

TEST(MultiIndexContainerStandaloneTest, ContainsIdTracksInsertedIds)
{
  MultiIndex values;
  values.add(alpha(1), 5, alphaType);
  EXPECT_TRUE(values.containsId(5));
  EXPECT_FALSE(values.containsId(6));
}

TEST(MultiIndexContainerStandaloneTest, FindByTypeReturnsAllMatchingItems)
{
  MultiIndex values;
  const auto first = alpha(1);
  const auto second = alpha(2);
  values.add(first, 1, alphaType);
  values.add(beta(3), 3, betaType);
  values.add(second, 2, alphaType);
  const auto matches = values.findByType(alphaType);
  ASSERT_EQ(matches.size(), 2);
  EXPECT_TRUE(matches.contains(first));
  EXPECT_TRUE(matches.contains(second));
}

TEST(MultiIndexContainerStandaloneTest, FindByTypeReturnsEmptyListForUnknownType)
{
  MultiIndex values;
  EXPECT_TRUE(values.findByType(alphaType).isEmpty());
}

TEST(MultiIndexContainerStandaloneTest, AllPreservesInsertionOrder)
{
  MultiIndex values;
  const auto first = alpha(1);
  const auto second = beta(2);
  values.add(first, 1, alphaType);
  values.add(second, 2, betaType);
  ASSERT_EQ(values.all().size(), 2);
  EXPECT_EQ(values.all().at(0), first);
  EXPECT_EQ(values.all().at(1), second);
}

TEST(MultiIndexContainerStandaloneTest, InsertAtPlacesItemAtRequestedPosition)
{
  MultiIndex values;
  const auto first = alpha(1);
  const auto middle = beta(2);
  const auto last = alpha(3);
  values.add(first, 1, alphaType);
  values.add(last, 3, alphaType);
  values.insertAt(1, middle, 2, betaType);
  EXPECT_EQ(values.all().at(1), middle);
  EXPECT_EQ(values.findById(2), middle);
}

TEST(MultiIndexContainerStandaloneTest, InsertAtSafeIndexesItem)
{
  MultiIndex values;
  const auto item = beta(2);
  values.insertAtSafe(0, item, 2, betaType);
  EXPECT_EQ(values.all().first(), item);
  EXPECT_EQ(values.findById(2), item);
}

TEST(MultiIndexContainerStandaloneTest, MoveChangesOnlyListOrder)
{
  MultiIndex values;
  const auto first = alpha(1);
  const auto second = beta(2);
  values.add(first, 1, alphaType);
  values.add(second, 2, betaType);
  values.move(0, 1);
  EXPECT_EQ(values.all().at(0), second);
  EXPECT_EQ(values.all().at(1), first);
  EXPECT_EQ(values.findById(1), first);
  EXPECT_EQ(values.findByType(betaType).first(), second);
}

TEST(MultiIndexContainerStandaloneTest, MoveSafeChangesListOrder)
{
  MultiIndex values;
  const auto first = alpha(1);
  const auto second = beta(2);
  values.add(first, 1, alphaType);
  values.add(second, 2, betaType);
  values.moveSafe(1, 0);
  EXPECT_EQ(values.all().first(), second);
  EXPECT_EQ(values.findById(2), second);
}

TEST(MultiIndexContainerStandaloneTest, MoveToSameIndexKeepsOrder)
{
  MultiIndex values;
  const auto item = alpha(1);
  values.add(item, 1, alphaType);
  values.move(0, 0);
  EXPECT_EQ(values.all().first(), item);
}

TEST(MultiIndexContainerStandaloneTest, IndexOfFindsExistingItem)
{
  MultiIndex values;
  const auto first = alpha(1);
  const auto second = beta(2);
  values.add(first, 1, alphaType);
  values.add(second, 2, betaType);
  EXPECT_EQ(values.indexOf(second), 1);
}

TEST(MultiIndexContainerStandaloneTest, IndexOfSafeFindsExistingItem)
{
  MultiIndex values;
  const auto item = alpha(1);
  values.add(item, 1, alphaType);
  EXPECT_EQ(values.indexOfSafe(item), 0);
}

TEST(MultiIndexContainerStandaloneTest, IndexOfReturnsMinusOneForMissingItem)
{
  const MultiIndex values;
  EXPECT_EQ(values.indexOf(alpha(1)), -1);
}

TEST(MultiIndexContainerStandaloneTest, RemoveByIdRemovesItemFromAllIndexes)
{
  MultiIndex values;
  const auto item = alpha(1);
  values.add(item, 1, alphaType);
  values.removeById(1);
  EXPECT_FALSE(values.containsId(1));
  EXPECT_TRUE(values.all().isEmpty());
  EXPECT_TRUE(values.findByType(alphaType).isEmpty());
}

TEST(MultiIndexContainerStandaloneTest, RemoveByUnknownIdLeavesContainerUnchanged)
{
  MultiIndex values;
  const auto item = alpha(1);
  values.add(item, 1, alphaType);
  values.removeById(99);
  EXPECT_EQ(values.all().size(), 1);
  EXPECT_EQ(values.findById(1), item);
}

TEST(MultiIndexContainerStandaloneTest, ClearRemovesEveryIndexEntry)
{
  MultiIndex values;
  values.add(alpha(1), 1, alphaType);
  values.add(beta(2), 2, betaType);
  values.clear();
  EXPECT_TRUE(values.all().isEmpty());
  EXPECT_FALSE(values.containsId(1));
  EXPECT_TRUE(values.findByType(alphaType).isEmpty());
}

TEST(MultiIndexContainerStandaloneTest, RangeIterationVisitsListOrder)
{
  MultiIndex values;
  const auto first = alpha(1);
  const auto second = beta(2);
  values.add(first, 1, alphaType);
  values.add(second, 2, betaType);
  int sum = 0;
  for (const auto& item : values) sum += item->value;
  EXPECT_EQ(sum, 3);
}

TEST(MultiIndexContainerStandaloneTest, DuplicateTypeEntriesCanBeRemovedIndividually)
{
  MultiIndex values;
  const auto first = alpha(1);
  const auto second = alpha(2);
  values.add(first, 1, alphaType);
  values.add(second, 2, alphaType);
  values.removeById(1);
  const auto matches = values.findByType(alphaType);
  ASSERT_EQ(matches.size(), 1);
  EXPECT_EQ(matches.first(), second);
}
