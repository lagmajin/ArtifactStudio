#pragma once

#include <gtest/gtest.h>

#include <QByteArray>
#include <QApplication>

import Artifact.Render.IRenderer;

namespace ArtifactTest {

class OffscreenGuiApplication final : public testing::Environment {
public:
    void SetUp() override
    {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
        static int argc = 1;
        static char applicationName[] = "ArtifactDiligentHeadlessTest";
        static char* argv[] = {applicationName, nullptr};
        application_ = new QApplication(argc, argv);
    }

    void TearDown() override
    {
        delete application_;
        application_ = nullptr;
    }

private:
    QApplication* application_ = nullptr;
};

inline testing::Environment* const offscreenGuiApplication =
    testing::AddGlobalTestEnvironment(new OffscreenGuiApplication());

class HeadlessRenderer final : public Artifact::ArtifactIRenderer {
public:
    ~HeadlessRenderer() { destroy(); }

    bool initialize(int width, int height,
                    bool enableGpuClonerPlacement = false)
    {
        initializeHeadless(width, height, enableGpuClonerPlacement);
        return isInitialized();
    }
};

} // namespace ArtifactTest
