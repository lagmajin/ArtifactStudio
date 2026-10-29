#include <gtest/gtest.h>

#include <QThread>
#include <QString>

#include <vector>

import Command.LayerLockManager;
import Event.Bus;

namespace {

using ArtifactCore::LayerLockChangeKind;
using ArtifactCore::LayerLockChangedEvent;
using ArtifactCore::LayerLockManager;

} // namespace

TEST(LayerLockManagerContractTest, EnforcesOwnershipAndAppliesRemoteReleaseAndDenial)
{
    LayerLockManager manager;
    std::vector<LayerLockChangedEvent> changes;
    auto subscription = ArtifactCore::globalEventBus().subscribe<LayerLockChangedEvent>(
        [&](const auto& event) { changes.push_back(event); });
    ASSERT_TRUE(subscription.connected());

    const QString layer = QStringLiteral("lock-contract-layer");
    EXPECT_TRUE(manager.acquireLock(layer, QStringLiteral("client-a"),
                                    QStringLiteral("user-a"),
                                    QStringLiteral("User A")));
    EXPECT_TRUE(manager.isLocked(layer));
    ASSERT_NE(manager.lockInfo(layer), nullptr);
    EXPECT_EQ(manager.lockInfo(layer)->userName, QStringLiteral("User A"));
    EXPECT_FALSE(manager.acquireLock(layer, QStringLiteral("client-b"),
                                     QStringLiteral("user-b"),
                                     QStringLiteral("User B")));
    EXPECT_FALSE(manager.releaseLock(layer, QStringLiteral("client-b")));
    EXPECT_TRUE(manager.isLocked(layer));

    manager.onRemoteLockReleased(layer, QStringLiteral("other-client"));
    EXPECT_TRUE(manager.isLocked(layer));
    manager.onRemoteLockReleased(layer, QStringLiteral("client-a"));
    EXPECT_FALSE(manager.isLocked(layer));
    EXPECT_EQ(manager.lockInfo(layer), nullptr);

    ASSERT_EQ(changes.size(), 2u);
    EXPECT_EQ(changes[0].kind, LayerLockChangeKind::Acquired);
    EXPECT_EQ(changes[1].kind, LayerLockChangeKind::Released);

    EXPECT_TRUE(manager.acquireLock(layer, QStringLiteral("client-c"),
                                    QStringLiteral("user-c"),
                                    QStringLiteral("User C")));
    manager.onRemoteLockDenied(layer, QStringLiteral("server conflict"));
    EXPECT_FALSE(manager.isLocked(layer));
    ASSERT_EQ(changes.size(), 4u);
    EXPECT_EQ(changes[3].kind, LayerLockChangeKind::Denied);
    EXPECT_EQ(changes[3].reason, QStringLiteral("server conflict"));
}

TEST(LayerLockManagerContractTest, PurgesExpiredLocksAndPublishesExpiry)
{
    LayerLockManager manager;
    std::vector<LayerLockChangedEvent> changes;
    auto subscription = ArtifactCore::globalEventBus().subscribe<LayerLockChangedEvent>(
        [&](const auto& event) { changes.push_back(event); });
    ASSERT_TRUE(subscription.connected());

    const QString layer = QStringLiteral("expired-lock-contract-layer");
    ASSERT_TRUE(manager.acquireLock(layer, QStringLiteral("client-expiring"),
                                   QStringLiteral("user"), QStringLiteral("User")));
    manager.setLockTimeout(1);
    QThread::msleep(3);

    EXPECT_FALSE(manager.isLocked(layer));
    EXPECT_EQ(manager.lockInfo(layer), nullptr);
    EXPECT_TRUE(manager.activeLocks().isEmpty());
    manager.purgeExpired(1);
    EXPECT_EQ(manager.lockInfo(layer), nullptr);
    ASSERT_EQ(changes.size(), 2u);
    EXPECT_EQ(changes.back().kind, LayerLockChangeKind::Expired);
    EXPECT_EQ(changes.back().layerId, layer);
    EXPECT_EQ(changes.back().clientId, QStringLiteral("client-expiring"));
}
