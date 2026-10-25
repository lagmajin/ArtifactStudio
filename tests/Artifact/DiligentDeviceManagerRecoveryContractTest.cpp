#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QGuiApplication>

import Artifact.Render.DiligentDeviceManager;

namespace {

class DeviceManagerRecoveryEnvironment final : public testing::Environment {
public:
    void SetUp() override
    {
        static int argc = 1;
        static char applicationName[] = "DiligentDeviceManagerRecoveryContractTest";
        static char* argv[] = {applicationName, nullptr};
        application_ = new QGuiApplication(argc, argv);
    }

    void TearDown() override
    {
        delete application_;
        application_ = nullptr;
    }

private:
    QGuiApplication* application_ = nullptr;
};

DeviceManagerRecoveryEnvironment* const recoveryEnvironment =
    new DeviceManagerRecoveryEnvironment();
const bool recoveryEnvironmentRegistered = [] {
    testing::AddGlobalTestEnvironment(recoveryEnvironment);
    return true;
}();

} // namespace

TEST(DiligentDeviceManagerRecoveryContractTest,
     HeadlessManagerReinitializesAcrossRepeatedDeviceLostTeardowns)
{
    Artifact::DiligentDeviceManager manager;
    manager.initializeHeadless();
    if (!manager.isInitialized() || !manager.device() ||
        !manager.immediateContext()) {
        GTEST_SKIP() << "No supported Diligent headless device is available";
    }

    constexpr int recoveryRounds = 3;
    for (int round = 0; round < recoveryRounds; ++round) {
        ASSERT_TRUE(manager.isInitialized()) << "round " << round;
        ASSERT_TRUE(manager.device()) << "round " << round;
        ASSERT_TRUE(manager.immediateContext()) << "round " << round;
        ASSERT_TRUE(manager.deferredContext()) << "round " << round;
        EXPECT_FALSE(manager.swapChain()) << "headless round " << round;
        EXPECT_EQ(manager.renderHwnd(), nullptr) << "headless round " << round;

        manager.markDeviceLost();
        manager.destroy();
        EXPECT_FALSE(manager.isInitialized()) << "round " << round;
        EXPECT_FALSE(manager.device()) << "round " << round;
        EXPECT_FALSE(manager.immediateContext()) << "round " << round;
        EXPECT_FALSE(manager.deferredContext()) << "round " << round;
        EXPECT_FALSE(manager.swapChain()) << "round " << round;
        EXPECT_EQ(manager.renderHwnd(), nullptr) << "round " << round;

        // destroy() is also used by cleanup paths after partial recovery.
        manager.destroy();
        EXPECT_FALSE(manager.isInitialized()) << "repeat destroy round " << round;
        EXPECT_FALSE(manager.device()) << "repeat destroy round " << round;
        EXPECT_FALSE(manager.immediateContext()) << "repeat destroy round " << round;

        manager.initializeHeadless();
        ASSERT_TRUE(manager.isInitialized()) << "reinit round " << round;
        ASSERT_TRUE(manager.device()) << "reinit round " << round;
        ASSERT_TRUE(manager.immediateContext()) << "reinit round " << round;
        ASSERT_TRUE(manager.deferredContext()) << "reinit round " << round;
    }
}
