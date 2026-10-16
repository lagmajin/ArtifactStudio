#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QString>
#include <QThread>

#include <cstdint>

import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Composition.InOutPoints;
import Artifact.Event.Types;
import Artifact.Service.Playback;
import Frame.Position;
import Frame.Range;
import FloatRGBA;
import Image.ImageF32x4_RGBA;
import Memory.SharedPtr;
import Utils.Id;

namespace {

class PlaybackServiceEnvironment final : public testing::Environment {
public:
    void SetUp() override
    {
        static int argc = 1;
        static char applicationName[] = "PlaybackRangeServiceContractTest";
        static char* argv[] = {applicationName, nullptr};
        application_ = new QCoreApplication(argc, argv);
    }

    void TearDown() override
    {
        delete application_;
        application_ = nullptr;
    }

private:
    QCoreApplication* application_ = nullptr;
};

class PlaybackCleanup final {
public:
    explicit PlaybackCleanup(Artifact::ArtifactPlaybackService& playback)
        : playback_(playback)
    {
    }

    ~PlaybackCleanup()
    {
        playback_.stop();
        playback_.waitForStop();
        playback_.setCurrentComposition({});
    }

private:
    Artifact::ArtifactPlaybackService& playback_;
};

PlaybackServiceEnvironment* const playbackServiceEnvironment =
    new PlaybackServiceEnvironment();
const bool playbackServiceEnvironmentRegistered = [] {
    testing::AddGlobalTestEnvironment(playbackServiceEnvironment);
    return true;
}();

} // namespace

TEST(PlaybackRangeServiceContractTest,
     WorkAreaModeUpdatesEngineRangeAndInOutPointsStayClipped)
{
    Artifact::ArtifactPlaybackService playback;
    PlaybackCleanup cleanup(playback);
    playback.setRamPreviewEnabled(false);
    playback.setDiskPreviewCacheEnabled(false);

    Artifact::ArtifactCompositionInitParams params;
    params.setDurationFrames(120);
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactAbstractComposition>(
        ArtifactCore::CompositionID(), params);
    ASSERT_TRUE(composition);
    composition->setFrameRange(ArtifactCore::FrameRange(10, 110));
    composition->setWorkAreaRange(ArtifactCore::FrameRange(30, 80));
    composition->inOutPoints()->setInPoint(ArtifactCore::FramePosition(40));
    composition->inOutPoints()->setOutPoint(ArtifactCore::FramePosition(90));

    playback.setCurrentComposition(composition);
    EXPECT_EQ(playback.playbackRangeMode(), Artifact::PlaybackRangeMode::All);
    EXPECT_EQ(playback.frameRange(), ArtifactCore::FrameRange(10, 110));
    EXPECT_EQ(playback.playableRange(), ArtifactCore::FrameRange(40, 90));

    playback.setPlaybackRangeMode(Artifact::PlaybackRangeMode::WorkArea);
    EXPECT_EQ(playback.playbackRangeMode(), Artifact::PlaybackRangeMode::WorkArea);
    EXPECT_EQ(playback.frameRange(), ArtifactCore::FrameRange(30, 80));
    EXPECT_EQ(playback.playableRange(), ArtifactCore::FrameRange(40, 79));

    playback.setPlaybackRangeMode(Artifact::PlaybackRangeMode::All);
    EXPECT_EQ(playback.frameRange(), ArtifactCore::FrameRange(10, 110));
    EXPECT_EQ(playback.playableRange(), ArtifactCore::FrameRange(40, 90));
}

TEST(PlaybackRangeServiceContractTest,
     TransportTransitionsPlayPauseResumeAndStop)
{
    Artifact::ArtifactPlaybackService playback;
    PlaybackCleanup cleanup(playback);
    playback.setRamPreviewEnabled(false);
    playback.setDiskPreviewCacheEnabled(false);

    Artifact::ArtifactCompositionInitParams params;
    params.setDurationFrames(120);
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactAbstractComposition>(
        ArtifactCore::CompositionID(), params);
    ASSERT_TRUE(composition);
    composition->setFrameRange(ArtifactCore::FrameRange(0, 120));
    playback.setLooping(true);
    playback.setCurrentComposition(composition);

    const auto waitForState = [&playback](ArtifactCore::PlaybackState expected) {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 2000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
            if (playback.state() == expected) return true;
            QThread::msleep(5);
        }
        return playback.state() == expected;
    };
    ASSERT_EQ(playback.state(), ArtifactCore::PlaybackState::Stopped);
    playback.play();
    if (!waitForState(ArtifactCore::PlaybackState::Playing)) {
        FAIL() << "Playback Service did not enter Playing";
    }

    playback.pause();
    if (!waitForState(ArtifactCore::PlaybackState::Paused)) {
        FAIL() << "Playback Service did not enter Paused";
    }

    playback.play();
    if (!waitForState(ArtifactCore::PlaybackState::Playing)) {
        FAIL() << "Playback Service did not resume Playing";
    }

    playback.stop();
    playback.waitForStop();
    EXPECT_EQ(playback.state(), ArtifactCore::PlaybackState::Stopped);
    EXPECT_TRUE(playback.isStopped());
    EXPECT_FALSE(playback.isPlaying());
}

TEST(PlaybackRangeServiceContractTest,
     RamPreviewCacheEnforcesBudgetAndStoreRefreshesLruRecency)
{
    Artifact::ArtifactPlaybackService playback;
    PlaybackCleanup cleanup(playback);
    playback.setRamPreviewEnabled(false);
    playback.setDiskPreviewCacheEnabled(false);

    Artifact::ArtifactCompositionInitParams params;
    params.setDurationFrames(140);
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactAbstractComposition>(
        ArtifactCore::CompositionID(), params);
    ASSERT_TRUE(composition);
    composition->setFrameRange(ArtifactCore::FrameRange(0, 140));
    playback.setCurrentComposition(composition);

    ArtifactCore::ImageF32x4_RGBA image;
    image.resize(1, 1);
    image.setPixel(0, 0, ArtifactCore::FloatRGBA(0.25f, 0.5f, 0.75f, 1.0f));
    for (std::int64_t frame = 0; frame < 129; ++frame) {
        playback.markRamPreviewFrameRequested(frame, QStringLiteral("budget-contract"));
        ASSERT_TRUE(playback.storeRamPreviewFrameImage(
            frame, image, QStringLiteral("budget-contract"), false)) << frame;
    }

    EXPECT_EQ(playback.ramPreviewCachedFrameCount(), 128);
    const auto bitmap = playback.ramPreviewCacheBitmap();
    ASSERT_EQ(bitmap.size(), 140u);
    EXPECT_FALSE(bitmap[0]);
    EXPECT_TRUE(bitmap[1]);
    EXPECT_TRUE(bitmap[128]);
    EXPECT_FALSE(bitmap[129]);
    EXPECT_TRUE(playback.ramPreviewFrameState(0).requested);
    EXPECT_FALSE(playback.ramPreviewFrameState(0).imageAvailable);
    EXPECT_TRUE(playback.ramPreviewFrameState(128).imageAvailable);

    playback.markRamPreviewFrameRequested(1, QStringLiteral("lru-touch"));
    ASSERT_TRUE(playback.storeRamPreviewFrameImage(
        1, image, QStringLiteral("lru-touch"), false));
    playback.markRamPreviewFrameRequested(129, QStringLiteral("budget-contract"));
    ASSERT_TRUE(playback.storeRamPreviewFrameImage(
        129, image, QStringLiteral("budget-contract"), false));

    EXPECT_EQ(playback.ramPreviewCachedFrameCount(), 128);
    const auto refreshedBitmap = playback.ramPreviewCacheBitmap();
    EXPECT_TRUE(refreshedBitmap[1]);
    EXPECT_FALSE(refreshedBitmap[2]);
    EXPECT_TRUE(refreshedBitmap[129]);
}
