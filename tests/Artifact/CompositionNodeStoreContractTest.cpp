#include <gtest/gtest.h>

#include <vector>

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

import Artifact.Composition.Nodes;

namespace {

Artifact::CompositionNode makeNode(const QString& id, const QString& parentId = {},
                                   int order = 0)
{
    Artifact::CompositionNode node;
    node.id = id;
    node.parentId = parentId;
    node.order = order;
    return node;
}

} // namespace

TEST(CompositionNodeStoreContractTest, RejectsDuplicateIdsAndUnknownParents)
{
    Artifact::CompositionNodeStore store;
    ASSERT_TRUE(store.addNode(makeNode(QStringLiteral("root"))));
    ASSERT_TRUE(store.addNode(makeNode(QStringLiteral("child"), QStringLiteral("root"))));

    EXPECT_FALSE(store.addNode(makeNode(QStringLiteral("child"))));
    EXPECT_FALSE(store.addNode(makeNode(QStringLiteral("orphan"), QStringLiteral("missing"))));
    EXPECT_FALSE(store.addNode(makeNode(QStringLiteral("  "))));
    EXPECT_EQ(store.nodes().size(), 2u);
    EXPECT_TRUE(store.contains(QStringLiteral("root")));
    EXPECT_TRUE(store.contains(QStringLiteral("child")));
    EXPECT_FALSE(store.contains(QStringLiteral("orphan")));
}

TEST(CompositionNodeStoreContractTest, RejectsSelfParentAndAncestorCyclesAtomically)
{
    Artifact::CompositionNodeStore store;
    ASSERT_TRUE(store.addNode(makeNode(QStringLiteral("root"))));
    ASSERT_TRUE(store.addNode(makeNode(QStringLiteral("child"), QStringLiteral("root"))));
    ASSERT_TRUE(store.addNode(makeNode(QStringLiteral("grandchild"), QStringLiteral("child"))));

    EXPECT_FALSE(store.setParent(QStringLiteral("root"), QStringLiteral("root")));
    EXPECT_FALSE(store.setParent(QStringLiteral("root"), QStringLiteral("grandchild")));
    EXPECT_FALSE(store.setParent(QStringLiteral("grandchild"), QStringLiteral("missing")));

    ASSERT_NE(store.node(QStringLiteral("root")), nullptr);
    ASSERT_NE(store.node(QStringLiteral("child")), nullptr);
    ASSERT_NE(store.node(QStringLiteral("grandchild")), nullptr);
    EXPECT_TRUE(store.node(QStringLiteral("root"))->parentId.isEmpty());
    EXPECT_EQ(store.node(QStringLiteral("child"))->parentId, QStringLiteral("root"));
    EXPECT_EQ(store.node(QStringLiteral("grandchild"))->parentId, QStringLiteral("child"));
}

TEST(CompositionNodeStoreContractTest, RemovingParentDetachesDirectChildren)
{
    Artifact::CompositionNodeStore store;
    ASSERT_TRUE(store.addNode(makeNode(QStringLiteral("root"))));
    ASSERT_TRUE(store.addNode(makeNode(QStringLiteral("child"), QStringLiteral("root"))));
    ASSERT_TRUE(store.addNode(makeNode(QStringLiteral("grandchild"), QStringLiteral("child"))));

    ASSERT_TRUE(store.removeNode(QStringLiteral("child")));
    EXPECT_FALSE(store.contains(QStringLiteral("child")));
    ASSERT_NE(store.node(QStringLiteral("grandchild")), nullptr);
    EXPECT_TRUE(store.node(QStringLiteral("grandchild"))->parentId.isEmpty());
    EXPECT_TRUE(store.childrenOf(QStringLiteral("root")).empty());
    EXPECT_FALSE(store.removeNode(QStringLiteral("child")));
}

TEST(CompositionNodeStoreContractTest, ReparentingMovesNodeBetweenParentChildLists)
{
    Artifact::CompositionNodeStore store;
    ASSERT_TRUE(store.addNode(makeNode(QStringLiteral("left"))));
    ASSERT_TRUE(store.addNode(makeNode(QStringLiteral("right"))));
    ASSERT_TRUE(store.addNode(makeNode(QStringLiteral("child"),
                                      QStringLiteral("left"))));

    ASSERT_TRUE(store.setParent(QStringLiteral("child"), QStringLiteral("right")));

    ASSERT_NE(store.node(QStringLiteral("child")), nullptr);
    EXPECT_EQ(store.node(QStringLiteral("child"))->parentId, QStringLiteral("right"));
    EXPECT_TRUE(store.childrenOf(QStringLiteral("left")).empty());
    EXPECT_EQ(store.childrenOf(QStringLiteral("right")),
              (std::vector<QString>{QStringLiteral("child")}));

    ASSERT_TRUE(store.setParent(QStringLiteral("child"), QString()));
    EXPECT_TRUE(store.node(QStringLiteral("child"))->parentId.isEmpty());
    EXPECT_TRUE(store.childrenOf(QStringLiteral("right")).empty());
}

TEST(CompositionNodeStoreContractTest, OrdersChildrenAndPreservesHierarchyThroughJson)
{
    Artifact::CompositionNodeStore store;
    ASSERT_TRUE(store.addNode(makeNode(QStringLiteral("root"))));
    ASSERT_TRUE(store.addNode(makeNode(QStringLiteral("later"), QStringLiteral("root"), 20)));
    ASSERT_TRUE(store.addNode(makeNode(QStringLiteral("earlier"), QStringLiteral("root"), 5)));

    const auto children = store.childrenOf(QStringLiteral("root"));
    ASSERT_EQ(children.size(), 2u);
    EXPECT_EQ(children[0], QStringLiteral("earlier"));
    EXPECT_EQ(children[1], QStringLiteral("later"));

    const auto restored = Artifact::CompositionNodeStore::fromJson(store.toJson());
    EXPECT_EQ(restored.nodes().size(), 3u);
    EXPECT_EQ(restored.childrenOf(QStringLiteral("root")), children);
    ASSERT_NE(restored.node(QStringLiteral("earlier")), nullptr);
    EXPECT_EQ(restored.node(QStringLiteral("earlier"))->parentId, QStringLiteral("root"));
    EXPECT_EQ(restored.node(QStringLiteral("earlier"))->order, 5);
}

TEST(CompositionNodeStoreContractTest, ChangingChildOrderIsVisibleAndRejectsInvalidUpdates)
{
    Artifact::CompositionNodeStore store;
    ASSERT_TRUE(store.addNode(makeNode(QStringLiteral("root"))));
    ASSERT_TRUE(store.addNode(makeNode(QStringLiteral("first"), QStringLiteral("root"), 0)));
    ASSERT_TRUE(store.addNode(makeNode(QStringLiteral("second"), QStringLiteral("root"), 1)));

    EXPECT_FALSE(store.setOrder(QStringLiteral("missing"), 0));
    EXPECT_FALSE(store.setOrder(QStringLiteral("second"), -1));
    ASSERT_TRUE(store.setOrder(QStringLiteral("second"), 0));
    ASSERT_TRUE(store.setOrder(QStringLiteral("first"), 1));

    const auto children = store.childrenOf(QStringLiteral("root"));
    ASSERT_EQ(children.size(), 2u);
    EXPECT_EQ(children[0], QStringLiteral("second"));
    EXPECT_EQ(children[1], QStringLiteral("first"));
    EXPECT_EQ(store.node(QStringLiteral("second"))->order, 0);
    EXPECT_EQ(store.node(QStringLiteral("first"))->order, 1);
}

TEST(CompositionNodeStoreContractTest, JsonImportKeepsValidNodesAndSkipsInvalidReferences)
{
    QJsonArray json;
    json.append(makeNode(QStringLiteral("root")).toJson());
    json.append(makeNode(QStringLiteral("child"), QStringLiteral("root"), 3).toJson());
    auto firstDuplicate = makeNode(QStringLiteral("duplicate"));
    firstDuplicate.order = 4;
    auto secondDuplicate = makeNode(QStringLiteral("duplicate"));
    secondDuplicate.order = 9;
    json.append(firstDuplicate.toJson());
    json.append(secondDuplicate.toJson());
    json.append(makeNode(QStringLiteral("orphan"), QStringLiteral("absent")).toJson());
    json.append(makeNode(QStringLiteral("  ")).toJson());
    json.append(QStringLiteral("not-an-object"));

    const auto restored = Artifact::CompositionNodeStore::fromJson(json);
    ASSERT_EQ(restored.nodes().size(), 3u);
    EXPECT_TRUE(restored.contains(QStringLiteral("root")));
    EXPECT_TRUE(restored.contains(QStringLiteral("child")));
    ASSERT_NE(restored.node(QStringLiteral("duplicate")), nullptr);
    EXPECT_EQ(restored.node(QStringLiteral("duplicate"))->order, 4);
    EXPECT_FALSE(restored.contains(QStringLiteral("orphan")));
    EXPECT_FALSE(restored.contains(QStringLiteral("")));
    EXPECT_EQ(restored.childrenOf(QStringLiteral("root")),
              (std::vector<QString>{QStringLiteral("child")}));
}
