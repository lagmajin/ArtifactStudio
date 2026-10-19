#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QString>
#include <QThread>

#include <cstdint>
#include <thread>

import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Composition.InOutPoints;
import Artifact.Event.Types;
import Artifact.Service.Playback;
import Artifact.Widgets.CompositionHostFrameSnapshot;
import Frame.Position;
import Frame.Range;
import FloatRGBA;
import Image.ImageF32x4_RGBA;
import Image.UploadConversion;
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
     PinnedRamPreviewUploadSurvivesCacheReplacement)
{
    Artifact::ArtifactPlaybackService playback;
    PlaybackCleanup cleanup(playback);
    playback.setRamPreviewEnabled(true);
    playback.setDiskPreviewCacheEnabled(false);

    Artifact::ArtifactCompositionInitParams params;
    params.setDurationFrames(4);
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactAbstractComposition>(
        ArtifactCore::CompositionID(), params);
    ASSERT_TRUE(composition);
    composition->setFrameRange(ArtifactCore::FrameRange(0, 4));
    playback.setCurrentComposition(composition);

    ArtifactCore::ImageF32x4_RGBA firstImage;
    firstImage.resize(2, 2);
    firstImage.fill(ArtifactCore::FloatRGBA(1.0f, 0.0f, 0.0f, 1.0f));
    ASSERT_TRUE(playback.storeRamPreviewFrameImage(
        1, firstImage, QStringLiteral("snapshot-test"), false));

    const auto firstSnapshot = playback.pinRamPreviewFrameUpload(1);
    ASSERT_TRUE(firstSnapshot.upload);
    ASSERT_TRUE(firstSnapshot.upload->isValid());
    ASSERT_EQ(firstSnapshot.width, 2);
    ASSERT_EQ(firstSnapshot.height, 2);
    const auto firstRevision = firstSnapshot.contentRevision;
    const auto firstBytes = firstSnapshot.upload->bytes;

    ArtifactCore::ImageF32x4_RGBA replacementImage;
    replacementImage.resize(2, 2);
    replacementImage.fill(ArtifactCore::FloatRGBA(0.0f, 1.0f, 0.0f, 1.0f));
    ASSERT_TRUE(playback.storeRamPreviewFrameImage(
        1, replacementImage, QStringLiteral("snapshot-replacement-test"), false));

    const auto replacementSnapshot = playback.pinRamPreviewFrameUpload(1);
    ASSERT_TRUE(replacementSnapshot.upload);
    EXPECT_NE(replacementSnapshot.contentRevision, firstRevision);
    EXPECT_NE(replacementSnapshot.upload.get(), firstSnapshot.upload.get());
    EXPECT_EQ(firstSnapshot.upload->bytes, firstBytes);
    EXPECT_TRUE(firstSnapshot.upload->isValid());
}

TEST(PlaybackRangeServiceContractTest,
     RenderFramePacketRetainsPinnedRamPreviewUploadAcrossWorkerHandoff)
{
    auto* playback = Artifact::ArtifactPlaybackService::instance();
    ASSERT_NE(playback, nullptr);
    PlaybackCleanup cleanup(*playback);
    playback->stop();
    playback->setRamPreviewEnabled(true);
    playback->setDiskPreviewCacheEnabled(false);

    Artifact::ArtifactCompositionInitParams params;
    params.setDurationFrames(4);
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactAbstractComposition>(
        ArtifactCore::CompositionID(), params);
    ASSERT_TRUE(composition);
    composition->setFrameRange(ArtifactCore::FrameRange(0, 4));
    composition->goToFrame(0);
    playback->setCurrentComposition(composition);

    Artifact::CompositionHostFrameSnapshot uncapturedHost;
    Artifact::CompositionRenderFramePacketCache uncapturedPacketCache;
    const auto uncapturedPacket =
        uncapturedPacketCache.capture(uncapturedHost, composition);
    ASSERT_TRUE(uncapturedPacket.isValid());
    EXPECT_FALSE(uncapturedPacket.host.playbackFrameCaptured)
        << "Packet construction must not fill missing playback data from the live service";
    EXPECT_FALSE(uncapturedPacket.ramPreviewUpload.upload)
        << "Only GUI-captured upload pins may cross the worker handoff";

    ArtifactCore::ImageF32x4_RGBA firstImage;
    firstImage.resize(2, 2);
    firstImage.fill(ArtifactCore::FloatRGBA(1.0f, 0.0f, 0.0f, 1.0f));
    ASSERT_TRUE(playback->storeRamPreviewFrameImage(
        0, firstImage, QStringLiteral("render-packet-handoff"), false));

    Artifact::CompositionHostFrameSnapshot host;
    host.compositionFramePosition = 0;
    host.compositionFrameCaptured = true;
    ASSERT_TRUE(Artifact::captureCompositionPlaybackFrameSnapshot(
        host, composition, playback));
    ASSERT_TRUE(host.ramPreviewStateCaptured);
    EXPECT_TRUE(host.ramPreviewEnabled);
    EXPECT_TRUE(host.ramPreviewFrameState.ready);
    EXPECT_TRUE(host.ramPreviewFrameState.imageAvailable);
    EXPECT_FALSE(host.ramPreviewPendingBuild);
    ASSERT_TRUE(host.ramPreviewUpload.upload);
    EXPECT_EQ(host.ramPreviewUpload.width, 2);
    EXPECT_EQ(host.ramPreviewUpload.height, 2);
    Artifact::CompositionHostFrameSnapshot rejectedWorkerSnapshot;
    bool workerCaptureAccepted = true;
    std::thread captureWorker([&] {
        workerCaptureAccepted =
            Artifact::captureCompositionPlaybackFrameSnapshot(
                rejectedWorkerSnapshot, composition, playback);
    });
    captureWorker.join();
    EXPECT_FALSE(workerCaptureAccepted);
    EXPECT_FALSE(rejectedWorkerSnapshot.ramPreviewStateCaptured);
    EXPECT_FALSE(rejectedWorkerSnapshot.ramPreviewUpload.upload);
    const auto packet = Artifact::captureCompositionRenderFramePacket(
        host, composition);
    ASSERT_TRUE(packet.isValid());
    ASSERT_TRUE(packet.ramPreviewUpload.upload);
    ASSERT_TRUE(packet.ramPreviewUpload.upload->isValid());
    ASSERT_EQ(packet.ramPreviewUpload.width, 2);
    ASSERT_EQ(packet.ramPreviewUpload.height, 2);
    ASSERT_TRUE(packet.host.playbackFrameCaptured);
    EXPECT_EQ(packet.host.playbackFramePosition, 0);
    EXPECT_EQ(packet.host.playbackCompositionId,
              composition->id().toString());
    EXPECT_EQ(packet.host.ramPreviewUpload.upload.get(),
              packet.ramPreviewUpload.upload.get());
    const auto pinnedBytes = packet.ramPreviewUpload.upload->bytes;
    const auto pinnedRevision = packet.ramPreviewUpload.contentRevision;

    ArtifactCore::ImageF32x4_RGBA replacementImage;
    replacementImage.resize(2, 2);
    replacementImage.fill(ArtifactCore::FloatRGBA(0.0f, 1.0f, 0.0f, 1.0f));
    ASSERT_TRUE(playback->storeRamPreviewFrameImage(
        0, replacementImage, QStringLiteral("render-packet-replacement"), false));
    const auto replacement = playback->pinRamPreviewFrameUpload(0);
    ASSERT_TRUE(replacement.upload);
    EXPECT_NE(replacement.contentRevision, pinnedRevision);
    EXPECT_NE(replacement.upload.get(), packet.ramPreviewUpload.upload.get());

    Artifact::CompositionRenderFramePacketMailbox mailbox;
    ASSERT_TRUE(mailbox.publish(packet, 1));
    Artifact::CompositionRenderFramePacket workerPacket;
    quint64 workerSequence = 0;
    bool workerSawExpectedUpload = false;
    std::thread worker([&] {
        if (mailbox.waitTakeLatest(workerPacket, workerSequence)) {
            workerSawExpectedUpload = workerPacket.ramPreviewUpload.upload &&
                workerPacket.ramPreviewUpload.contentRevision == pinnedRevision &&
                workerPacket.ramPreviewUpload.upload->bytes == pinnedBytes;
        }
    });
    worker.join();

    EXPECT_EQ(workerSequence, 1u);
    EXPECT_TRUE(workerSawExpectedUpload)
        << "The worker packet must retain the immutable upload after cache replacement";
    mailbox.close();
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
    const auto waitForFrameChange = [&playback](std::int64_t previousFrame) {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 2000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
            if (playback.currentFrame().framePosition() != previousFrame) return true;
            QThread::msleep(5);
        }
        return playback.currentFrame().framePosition() != previousFrame;
    };
    ASSERT_EQ(playback.state(), ArtifactCore::PlaybackState::Stopped);
    const auto initialFrame = playback.currentFrame().framePosition();
    playback.play();
    if (!waitForState(ArtifactCore::PlaybackState::Playing)) {
        FAIL() << "Playback Service did not enter Playing";
    }
    ASSERT_TRUE(waitForFrameChange(initialFrame))
        << "Playback Service entered Playing without advancing its frame clock";

    playback.pause();
    if (!waitForState(ArtifactCore::PlaybackState::Paused)) {
        FAIL() << "Playback Service did not enter Paused";
    }
    const auto pausedFrame = playback.currentFrame().framePosition();
    QElapsedTimer pauseObservation;
    pauseObservation.start();
    while (pauseObservation.elapsed() < 100) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QThread::msleep(5);
    }
    EXPECT_EQ(playback.currentFrame().framePosition(), pausedFrame)
        << "Playback Service advanced its frame clock while paused";

    playback.play();
    if (!waitForState(ArtifactCore::PlaybackState::Playing)) {
        FAIL() << "Playback Service did not resume Playing";
    }
    ASSERT_TRUE(waitForFrameChange(pausedFrame))
        << "Resumed Playback Service did not advance from its paused frame";

    playback.stop();
    playback.waitForStop();
    EXPECT_EQ(playback.state(), ArtifactCore::PlaybackState::Stopped);
    EXPECT_TRUE(playback.isStopped());
    EXPECT_FALSE(playback.isPlaying());
    const auto stoppedFrame = playback.currentFrame().framePosition();
    QElapsedTimer stopObservation;
    stopObservation.start();
    while (stopObservation.elapsed() < 100) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QThread::msleep(5);
    }
    EXPECT_EQ(playback.currentFrame().framePosition(), stoppedFrame)
        << "Playback Service advanced its frame clock after Stop completed";
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
