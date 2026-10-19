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

TEST(PlaybackRangeServiceContractTest,
     CompositionPreviewCacheRecordsLODContractAndRejectsForeignComposition)
{
    Artifact::ArtifactPlaybackService playback;
    PlaybackCleanup cleanup(playback);
    playback.setRamPreviewEnabled(false);
    playback.setDiskPreviewCacheEnabled(false);

    Artifact::ArtifactCompositionInitParams params;
    params.setDurationFrames(4);
    const ArtifactCore::CompositionID compositionId(
        QStringLiteral("preview-lod-contract"));
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactAbstractComposition>(
        compositionId, params);
    ASSERT_TRUE(composition);
    composition->setFrameRange(ArtifactCore::FrameRange(0, 4));
    playback.setCurrentComposition(composition);

    ArtifactCore::ImageF32x4_RGBA image;
    image.resize(1, 1);
    image.setPixel(0, 0, ArtifactCore::FloatRGBA(0.2f, 0.4f, 0.8f, 1.0f));

    EXPECT_FALSE(playback.storeCompositionPreviewFrameImage(
        0, image, QStringLiteral("another-composition"),
        4, 8, QStringLiteral("draft"), QStringLiteral("lod-contract"), false));
    EXPECT_FALSE(playback.ramPreviewFrameState(0).imageAvailable);

    ASSERT_TRUE(playback.storeCompositionPreviewFrameImage(
        0, image, compositionId.toString(),
        4, 8, QStringLiteral("draft"), QStringLiteral("lod-contract"), false));
    const auto draftState = playback.ramPreviewFrameState(0);
    EXPECT_TRUE(draftState.imageAvailable);
    EXPECT_EQ(draftState.compositionId, compositionId.toString());
    EXPECT_TRUE(draftState.reason.contains(QStringLiteral("previewDownsample=4")));
    EXPECT_TRUE(draftState.reason.contains(QStringLiteral("effectiveDownsample=8")));
    EXPECT_TRUE(draftState.reason.contains(QStringLiteral("path=draft")));

    ASSERT_TRUE(playback.storeCompositionPreviewFrameImage(
        1, image, compositionId.toString(),
        1, 1, QStringLiteral("full"), QStringLiteral("lod-contract"), false));
    EXPECT_FALSE(playback.ramPreviewFrameState(0).imageAvailable)
        << "switching from Draft to Full must not leave the Draft frame reusable";
    const auto fullState = playback.ramPreviewFrameState(1);
    EXPECT_TRUE(fullState.imageAvailable);
    EXPECT_TRUE(fullState.reason.contains(QStringLiteral("previewDownsample=1")));
    EXPECT_TRUE(fullState.reason.contains(QStringLiteral("effectiveDownsample=1")));
    EXPECT_TRUE(fullState.reason.contains(QStringLiteral("path=full")));

    ASSERT_TRUE(playback.storeCompositionPreviewFrameImage(
        2, image, compositionId.toString(),
        4, 8, QStringLiteral("draft"), QStringLiteral("lod-contract"), false));
    EXPECT_FALSE(playback.ramPreviewFrameState(1).imageAvailable)
        << "switching from Full back to Draft must not leave the Full frame reusable";
    const auto restoredDraftState = playback.ramPreviewFrameState(2);
    EXPECT_TRUE(restoredDraftState.imageAvailable);
    EXPECT_TRUE(restoredDraftState.reason.contains(QStringLiteral("path=draft")));

    ArtifactCore::ImageF32x4_RGBA qualityImage;
    qualityImage.resize(2, 1);
    qualityImage.setPixel(0, 0,
                          ArtifactCore::FloatRGBA(0.9f, 0.1f, 0.2f, 1.0f));
    qualityImage.setPixel(1, 0,
                          ArtifactCore::FloatRGBA(0.1f, 0.8f, 0.3f, 1.0f));
    ASSERT_TRUE(playback.storeCompositionPreviewFrameImage(
        3, qualityImage, compositionId.toString(),
        4, 8, QStringLiteral("draft"), QStringLiteral("lod-pixel-contract"),
        false));
    ArtifactCore::ImageF32x4_RGBA draftPixels;
    ASSERT_TRUE(playback.tryGetRamPreviewFrameImage(3, draftPixels));
    ASSERT_FALSE(draftPixels.isEmpty());
    EXPECT_EQ(draftPixels.width(), 2);
    EXPECT_EQ(draftPixels.height(), 1);
    EXPECT_FLOAT_EQ(draftPixels.getPixel(0, 0).r(), 0.9f);
    EXPECT_FLOAT_EQ(draftPixels.getPixel(1, 0).g(), 0.8f);

    ArtifactCore::ImageF32x4_RGBA finalQualityImage;
    finalQualityImage.resize(4, 2);
    finalQualityImage.setPixel(0, 0,
                               ArtifactCore::FloatRGBA(0.2f, 0.3f, 0.9f, 1.0f));
    ASSERT_TRUE(playback.storeCompositionPreviewFrameImage(
        3, finalQualityImage, compositionId.toString(),
        1, 1, QStringLiteral("full"), QStringLiteral("lod-pixel-contract"),
        false));
    ArtifactCore::ImageF32x4_RGBA finalPixels;
    ASSERT_TRUE(playback.tryGetRamPreviewFrameImage(3, finalPixels));
    ASSERT_FALSE(finalPixels.isEmpty());
    EXPECT_EQ(finalPixels.width(), 4)
        << "replacing a Draft frame with Final must store the new LOD dimensions";
    EXPECT_EQ(finalPixels.height(), 2);
    EXPECT_FLOAT_EQ(finalPixels.getPixel(0, 0).b(), 0.9f)
        << "Final cache replacement must expose the new frame pixels";

    ASSERT_TRUE(playback.storeCompositionPreviewFrameImage(
        3, qualityImage, compositionId.toString(),
        4, 8, QStringLiteral("draft"), QStringLiteral("lod-pixel-contract"),
        false));
    ArtifactCore::ImageF32x4_RGBA restoredDraftPixels;
    ASSERT_TRUE(playback.tryGetRamPreviewFrameImage(3, restoredDraftPixels));
    ASSERT_FALSE(restoredDraftPixels.isEmpty());
    EXPECT_EQ(restoredDraftPixels.width(), 2)
        << "returning to Draft must replace the cached Final frame";
    EXPECT_EQ(restoredDraftPixels.height(), 1);
    EXPECT_FLOAT_EQ(restoredDraftPixels.getPixel(0, 0).r(), 0.9f)
        << "returning to Draft must restore its pixels, not only its dimensions";

    ArtifactCore::ImageF32x4_RGBA neighboringFrame;
    neighboringFrame.resize(1, 2);
    neighboringFrame.setPixel(0, 0,
                              ArtifactCore::FloatRGBA(0.7f, 0.2f, 0.1f, 1.0f));
    neighboringFrame.setPixel(0, 1,
                              ArtifactCore::FloatRGBA(0.3f, 0.6f, 0.9f, 1.0f));
    ASSERT_TRUE(playback.storeCompositionPreviewFrameImage(
        2, neighboringFrame, compositionId.toString(),
        4, 8, QStringLiteral("draft"), QStringLiteral("lod-pixel-contract"),
        false));
    ArtifactCore::ImageF32x4_RGBA neighboringPixels;
    ASSERT_TRUE(playback.tryGetRamPreviewFrameImage(2, neighboringPixels));
    ASSERT_EQ(neighboringPixels.width(), 1);
    ASSERT_EQ(neighboringPixels.height(), 2);
    EXPECT_FLOAT_EQ(neighboringPixels.getPixel(0, 0).r(), 0.7f);
    EXPECT_FLOAT_EQ(neighboringPixels.getPixel(0, 1).b(), 0.9f);

    ASSERT_TRUE(playback.storeCompositionPreviewFrameImage(
        3, finalQualityImage, compositionId.toString(),
        1, 1, QStringLiteral("full"), QStringLiteral("lod-pixel-contract"),
        false));
    ArtifactCore::ImageF32x4_RGBA revisitedFinalPixels;
    ASSERT_TRUE(playback.tryGetRamPreviewFrameImage(3, revisitedFinalPixels));
    EXPECT_EQ(revisitedFinalPixels.width(), 4)
        << "visiting another frame at Draft LOD must not contaminate frame 3";
    EXPECT_EQ(revisitedFinalPixels.height(), 2);
    EXPECT_FLOAT_EQ(revisitedFinalPixels.getPixel(0, 0).b(), 0.9f);
}
