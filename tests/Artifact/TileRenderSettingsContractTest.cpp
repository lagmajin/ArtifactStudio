#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QStandardPaths>

#include <limits>

import Artifact.Render.Queue.Service;

namespace {

QCoreApplication* ensureApplication()
{
    static int argc = 1;
    static char appName[] = "ArtifactTileRenderSettingsContractTest";
    static char* argv[] = {appName, nullptr};
    static QCoreApplication app(argc, argv);
    return &app;
}

} // namespace

TEST(TileRenderSettingsContractTest,
     ExposesModeAndClampsTileSizeToTheMinimum)
{
    auto* application = ensureApplication();
    application->setApplicationName(
        QStringLiteral("ArtifactTileRenderSettingsContractTest"));
    QStandardPaths::setTestModeEnabled(true);
    auto* queue = Artifact::ArtifactRenderQueueService::instance();
    ASSERT_NE(queue, nullptr);

    const auto previousMode = queue->tileRenderMode();
    const int previousTileSize = queue->tileSize();
    struct RestoreSettings final {
        Artifact::ArtifactRenderQueueService* queue;
        Artifact::ArtifactRenderQueueService::TileRenderMode mode;
        int tileSize;
        ~RestoreSettings()
        {
            queue->setTileRenderMode(mode);
            queue->setTileSize(tileSize);
        }
    } restore{queue, previousMode, previousTileSize};

    using Mode = Artifact::ArtifactRenderQueueService::TileRenderMode;
    queue->setTileRenderMode(Mode::Tiled);
    EXPECT_EQ(queue->tileRenderMode(), Mode::Tiled);
    queue->setTileRenderMode(Mode::FullFrame);
    EXPECT_EQ(queue->tileRenderMode(), Mode::FullFrame);

    queue->setTileSize(512);
    EXPECT_EQ(queue->tileSize(), 512);
    queue->setTileSize(64);
    EXPECT_EQ(queue->tileSize(), 64);
    queue->setTileSize(1);
    EXPECT_EQ(queue->tileSize(), 64);
    queue->setTileSize(std::numeric_limits<int>::min());
    EXPECT_EQ(queue->tileSize(), 64);
}
