#include <gtest/gtest.h>

#include <QString>
#include <vector>

import Event.Bus;
import UI.SelectionManager;
import Utils.Id;

namespace {

using ArtifactCore::Id;
using ArtifactCore::SelectionManager;
using ArtifactCore::SelectionManagerActiveCompositionChangedEvent;
using ArtifactCore::SelectionManagerAssetSelectionChangedEvent;
using ArtifactCore::SelectionManagerLayerSelectionChangedEvent;

Id makeId(const char* value)
{
    return Id(QString::fromLatin1(value));
}

} // namespace

TEST(SelectionManagerContractTest, CompositionChangesPublishOnlyWhenIdentityChanges)
{
    auto& manager = SelectionManager::instance();
    auto& bus = ArtifactCore::globalEventBus();
    std::vector<Id> observed;
    auto subscription = bus.subscribe<SelectionManagerActiveCompositionChangedEvent>(
        [&](const auto& event) { observed.push_back(event.id); });
    ASSERT_TRUE(subscription.connected());

    const Id composition = makeId("selection-test-composition");
    manager.setActiveComposition(composition);
    manager.setActiveComposition(composition);

    EXPECT_EQ(manager.activeComposition(), composition);
    ASSERT_EQ(observed.size(), 1u);
    EXPECT_EQ(observed.front(), composition);
}

TEST(SelectionManagerContractTest, LayerSelectionPreservesInputOrderAndClearPublishesEmptyState)
{
    auto& manager = SelectionManager::instance();
    auto& bus = ArtifactCore::globalEventBus();
    std::vector<std::vector<Id>> observed;
    auto subscription = bus.subscribe<SelectionManagerLayerSelectionChangedEvent>(
        [&](const auto& event) { observed.push_back(event.ids); });
    ASSERT_TRUE(subscription.connected());

    const Id first = makeId("selection-test-layer-a");
    const Id second = makeId("selection-test-layer-b");
    const std::vector<Id> requested{second, first, second};
    manager.selectLayers(requested);

    EXPECT_EQ(manager.selectedLayers(), requested);
    EXPECT_TRUE(manager.isLayerSelected(first));
    EXPECT_TRUE(manager.isLayerSelected(second));
    ASSERT_EQ(observed.size(), 1u);
    EXPECT_EQ(observed.front(), requested);

    manager.clearLayerSelection();
    EXPECT_TRUE(manager.selectedLayers().empty());
    EXPECT_FALSE(manager.isLayerSelected(first));
    ASSERT_EQ(observed.size(), 2u);
    EXPECT_TRUE(observed.back().empty());
}

TEST(SelectionManagerContractTest, AssetSelectionReplacesStoredSelectionAndPublishesSnapshot)
{
    auto& manager = SelectionManager::instance();
    auto& bus = ArtifactCore::globalEventBus();
    std::vector<std::vector<Id>> observed;
    auto subscription = bus.subscribe<SelectionManagerAssetSelectionChangedEvent>(
        [&](const auto& event) { observed.push_back(event.ids); });
    ASSERT_TRUE(subscription.connected());

    const std::vector<Id> firstSelection{makeId("selection-test-asset-a"),
                                         makeId("selection-test-asset-b")};
    const std::vector<Id> replacement{makeId("selection-test-asset-c")};
    manager.selectAssets(firstSelection);
    manager.selectAssets(replacement);

    EXPECT_EQ(manager.selectedAssets(), replacement);
    ASSERT_EQ(observed.size(), 2u);
    EXPECT_EQ(observed[0], firstSelection);
    EXPECT_EQ(observed[1], replacement);
}
