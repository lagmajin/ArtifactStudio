#include <gtest/gtest.h>

#include <QString>
#include <vector>

import Event.Bus;
import Input.Surface.Manager;

namespace {

using ArtifactCore::InputSurfaceManager;
using ArtifactCore::InputSurfaceMode;
using ArtifactCore::InputSurfaceStateChangedEvent;

} // namespace

TEST(InputSurfaceManagerContractTest, RealtimeCaptureClampsFrameAndCommitClearsCaptureState)
{
    auto& manager = *InputSurfaceManager::instance();
    manager.reset();

    std::vector<InputSurfaceStateChangedEvent> changes;
    auto subscription = ArtifactCore::globalEventBus().subscribe<InputSurfaceStateChangedEvent>(
        [&](const auto& event) { changes.push_back(event); });
    ASSERT_TRUE(subscription.connected());

    manager.beginRealTimeCapture(QStringLiteral("layer-7"),
                                 QStringLiteral("Timeline"), -12);
    const auto capturing = manager.state();
    EXPECT_EQ(capturing.mode, InputSurfaceMode::RealTimeCapture);
    EXPECT_TRUE(capturing.armed);
    EXPECT_TRUE(capturing.livePreviewEnabled);
    EXPECT_FALSE(capturing.stepEntryEnabled);
    EXPECT_TRUE(capturing.capturePending);
    EXPECT_EQ(capturing.transportFrame, 0);
    EXPECT_EQ(capturing.targetId, QStringLiteral("layer-7"));
    EXPECT_EQ(capturing.context, QStringLiteral("Timeline"));

    manager.commitCapture();
    const auto committed = manager.state();
    EXPECT_EQ(committed.mode, InputSurfaceMode::Off);
    EXPECT_FALSE(committed.armed);
    EXPECT_FALSE(committed.livePreviewEnabled);
    EXPECT_FALSE(committed.stepEntryEnabled);
    EXPECT_FALSE(committed.capturePending);
    EXPECT_TRUE(committed.targetId.isEmpty());
    EXPECT_TRUE(committed.context.isEmpty());

    ASSERT_EQ(changes.size(), 2u);
    EXPECT_EQ(changes[0].reason, QStringLiteral("beginRealTimeCapture"));
    EXPECT_TRUE(changes[0].previous.capturePending == false);
    EXPECT_TRUE(changes[0].current.capturePending);
    EXPECT_EQ(changes[1].reason, QStringLiteral("commitCapture"));
    EXPECT_FALSE(changes[1].current.capturePending);
}

TEST(InputSurfaceManagerContractTest, StepAndHybridModesNormalizeFlagsAndCancelCapture)
{
    auto& manager = *InputSurfaceManager::instance();
    manager.reset();

    manager.setStepEntryEnabled(true);
    EXPECT_EQ(manager.mode(), InputSurfaceMode::StepEntry);
    EXPECT_TRUE(manager.isArmed());
    EXPECT_FALSE(manager.isLivePreviewEnabled());
    EXPECT_TRUE(manager.isStepEntryEnabled());

    manager.setLivePreviewEnabled(true);
    EXPECT_EQ(manager.mode(), InputSurfaceMode::Hybrid);
    EXPECT_TRUE(manager.isLivePreviewEnabled());
    EXPECT_TRUE(manager.isStepEntryEnabled());

    manager.setStepFrame(-3);
    manager.setTransportFrame(-9);
    EXPECT_EQ(manager.stepFrame(), 0);
    EXPECT_EQ(manager.transportFrame(), 0);
    EXPECT_TRUE(manager.isQuantizeToFrameEnabled());

    manager.beginStepEntry(QStringLiteral("shape-2"),
                           QStringLiteral("Composition"), 42);
    EXPECT_EQ(manager.mode(), InputSurfaceMode::StepEntry);
    EXPECT_TRUE(manager.isCapturePending());
    EXPECT_EQ(manager.stepFrame(), 42);
    EXPECT_EQ(manager.targetId(), QStringLiteral("shape-2"));

    manager.cancelCapture();
    EXPECT_EQ(manager.mode(), InputSurfaceMode::Off);
    EXPECT_FALSE(manager.isArmed());
    EXPECT_FALSE(manager.isCapturePending());
    EXPECT_TRUE(manager.targetId().isEmpty());
    EXPECT_TRUE(manager.context().isEmpty());
}

TEST(InputSurfaceManagerContractTest, DisarmingAndResetRestoreDefinedDefaults)
{
    auto& manager = *InputSurfaceManager::instance();
    manager.beginRealTimeCapture(QStringLiteral("target"),
                                 QStringLiteral("Viewport"), 19);
    manager.setArmed(false);

    EXPECT_EQ(manager.mode(), InputSurfaceMode::Off);
    EXPECT_FALSE(manager.isCapturePending());
    EXPECT_TRUE(manager.targetId().isEmpty());

    manager.setQuantizeToFrameEnabled(false);
    manager.setStepFrame(17);
    manager.reset();
    const auto reset = manager.state();
    EXPECT_EQ(reset.mode, InputSurfaceMode::Off);
    EXPECT_FALSE(reset.armed);
    EXPECT_TRUE(reset.quantizeToFrame);
    EXPECT_EQ(reset.transportFrame, 0);
    EXPECT_EQ(reset.stepFrame, 0);
    EXPECT_TRUE(reset.targetId.isEmpty());
    EXPECT_TRUE(reset.context.isEmpty());
}
