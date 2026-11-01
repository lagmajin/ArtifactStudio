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

TEST(SelectionManagerContractTest, ClearingCompositionPublishesOnceAndIsIdempotent)
{
    auto& manager = SelectionManager::instance();
    auto& bus = ArtifactCore::globalEventBus();
    std::vector<Id> observed;
    auto subscription = bus.subscribe<SelectionManagerActiveCompositionChangedEvent>(
        [&](const auto& event) { observed.push_back(event.id); });
    ASSERT_TRUE(subscription.connected());

    const Id composition = makeId("selection-test-composition-clear");
    const Id emptyComposition;
    manager.setActiveComposition(composition);
    manager.setActiveComposition(emptyComposition);
    manager.setActiveComposition(emptyComposition);

    EXPECT_EQ(manager.activeComposition(), emptyComposition);
    ASSERT_EQ(observed.size(), 2u);
    EXPECT_EQ(observed[0], composition);
    EXPECT_EQ(observed[1], emptyComposition);
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
    std::vector<Id> requested{second, first, second};
    const std::vector<Id> expected{second, first, second};
    manager.selectLayers(requested);

    requested[0] = makeId("selection-test-mutated-input");
    requested.push_back(makeId("selection-test-late-input"));

    EXPECT_EQ(manager.selectedLayers(), expected);
    EXPECT_TRUE(manager.isLayerSelected(first));
    EXPECT_TRUE(manager.isLayerSelected(second));
    EXPECT_FALSE(manager.isLayerSelected(requested[0]));
    ASSERT_EQ(observed.size(), 1u);
    EXPECT_EQ(observed.front(), expected)
        << "published selection must be a value snapshot of the request";

    manager.clearLayerSelection();
    EXPECT_TRUE(manager.selectedLayers().empty());
    EXPECT_FALSE(manager.isLayerSelected(first));
    ASSERT_EQ(observed.size(), 2u);
    EXPECT_TRUE(observed.back().empty());
}

TEST(SelectionManagerContractTest, ClearingAlreadyEmptyLayerSelectionStillPublishesEmptyState)
{
    auto& manager = SelectionManager::instance();
    auto& bus = ArtifactCore::globalEventBus();
    std::vector<std::vector<Id>> observed;
    auto subscription = bus.subscribe<SelectionManagerLayerSelectionChangedEvent>(
        [&](const auto& event) { observed.push_back(event.ids); });
    ASSERT_TRUE(subscription.connected());

    manager.clearLayerSelection();
    manager.clearLayerSelection();

    EXPECT_TRUE(manager.selectedLayers().empty());
    ASSERT_EQ(observed.size(), 2u);
    EXPECT_TRUE(observed[0].empty());
    EXPECT_TRUE(observed[1].empty());
}

TEST(SelectionManagerContractTest, EmptyLayerSelectionReplacesAndPublishesEmptyState)
{
    auto& manager = SelectionManager::instance();
    auto& bus = ArtifactCore::globalEventBus();
    std::vector<std::vector<Id>> observed;
    auto subscription = bus.subscribe<SelectionManagerLayerSelectionChangedEvent>(
        [&](const auto& event) { observed.push_back(event.ids); });
    ASSERT_TRUE(subscription.connected());

    const Id selected = makeId("selection-test-layer-before-empty");
    manager.selectLayers({selected});
    manager.selectLayers({});

    EXPECT_TRUE(manager.selectedLayers().empty());
    ASSERT_EQ(observed.size(), 2u);
    EXPECT_EQ(observed[0], (std::vector<Id>{selected}));
    EXPECT_TRUE(observed[1].empty());
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

TEST(SelectionManagerContractTest, AssetSelectionCopiesInputAndPublishedSnapshot)
{
    auto& manager = SelectionManager::instance();
    auto& bus = ArtifactCore::globalEventBus();
    std::vector<std::vector<Id>> observed;
    auto subscription = bus.subscribe<SelectionManagerAssetSelectionChangedEvent>(
        [&](const auto& event) { observed.push_back(event.ids); });
    ASSERT_TRUE(subscription.connected());

    std::vector<Id> requested{makeId("selection-test-asset-copy-a"),
                              makeId("selection-test-asset-copy-b")};
    const std::vector<Id> expected = requested;
    manager.selectAssets(requested);
    requested[0] = makeId("selection-test-asset-copy-mutated");
    requested.clear();

    EXPECT_EQ(manager.selectedAssets(), expected);
    ASSERT_EQ(observed.size(), 1u);
    EXPECT_EQ(observed.front(), expected);
}

TEST(SelectionManagerContractTest, EmptyAssetSelectionClearsAndPublishesEmptyState)
{
    auto& manager = SelectionManager::instance();
    auto& bus = ArtifactCore::globalEventBus();
    std::vector<std::vector<Id>> observed;
    auto subscription = bus.subscribe<SelectionManagerAssetSelectionChangedEvent>(
        [&](const auto& event) { observed.push_back(event.ids); });
    ASSERT_TRUE(subscription.connected());

    const Id selected = makeId("selection-test-asset-before-empty");
    manager.selectAssets({selected});
    manager.selectAssets({});

    EXPECT_TRUE(manager.selectedAssets().empty());
    ASSERT_EQ(observed.size(), 2u);
    EXPECT_EQ(observed[0], (std::vector<Id>{selected}));
    EXPECT_TRUE(observed[1].empty());
}
