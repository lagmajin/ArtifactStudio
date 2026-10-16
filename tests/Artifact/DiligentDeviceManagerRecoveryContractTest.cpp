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
     HeadlessManagerReinitializesAfterDeviceLostTeardown)
{
    Artifact::DiligentDeviceManager manager;
    manager.initializeHeadless();
    if (!manager.isInitialized() || !manager.device() ||
        !manager.immediateContext()) {
        GTEST_SKIP() << "No supported Diligent headless device is available";
    }

    ASSERT_TRUE(manager.device());
    ASSERT_TRUE(manager.immediateContext());

    manager.markDeviceLost();
    manager.destroy();
    EXPECT_FALSE(manager.isInitialized());
    EXPECT_FALSE(manager.device());
    EXPECT_FALSE(manager.immediateContext());

    manager.initializeHeadless();
    ASSERT_TRUE(manager.isInitialized());
    ASSERT_TRUE(manager.device());
    ASSERT_TRUE(manager.immediateContext());
}
