#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>
#include <QThread>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

import Artifact.Audio.Waveform;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Composition.Result;
import Artifact.Layer.Audio;
import Artifact.Layer.InitParams;
import Artifact.Layer.Result;
import Artifact.Project.Manager;
import Artifact.Render.Queue.Job;
import Artifact.Render.Queue.Service;
import Audio.SimpleWav;
import Audio.Segment;
import Utils.String.UniString;

namespace {

class AudioLayerRenderTestEnvironment final : public testing::Environment {
public:
    void SetUp() override
    {
        static int argc = 1;
        static char applicationName[] = "AudioLayerRenderIntegrationTest";
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

testing::Environment* const audioLayerRenderQueueEnvironment =
    testing::AddGlobalTestEnvironment(
        new AudioLayerRenderTestEnvironment());

bool writeFixtureWav(const QString& path, double frequencyHz = 440.0,
                     double amplitude = 12000.0)
{
    constexpr int sampleRate = 48000;
    constexpr int frameCount = 4800;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("RIFF", 4);
    stream << quint32(36 + frameCount * 2);
    stream.writeRawData("WAVE", 4);
    stream.writeRawData("fmt ", 4);
    stream << quint32(16) << quint16(1) << quint16(1)
           << quint32(sampleRate) << quint32(sampleRate * 2)
           << quint16(2) << quint16(16);
    stream.writeRawData("data", 4);
    stream << quint32(frameCount * 2);
    for (int i = 0; i < frameCount; ++i) {
        constexpr double pi = 3.14159265358979323846;
        const double phase = 2.0 * pi * frequencyHz * i / sampleRate;
        const auto sample = static_cast<qint16>(
            std::lround(std::sin(phase) * amplitude));
        stream << sample;
    }
    return stream.status() == QDataStream::Ok;
}

double rms(const QVector<float>& samples)
{
    if (samples.isEmpty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (const float sample : samples) {
        sum += static_cast<double>(sample) * sample;
    }
    return std::sqrt(sum / samples.size());
}

bool renderCompositionWav(Artifact::ArtifactRenderQueueService* queue,
                          const auto& compositionId,
                          const QString& compositionName,
                          const QString& outputPath,
                          QString* failure)
{
    if (!QDir().mkpath(QFileInfo(outputPath).absolutePath())) {
        *failure = QStringLiteral("Could not create WAV output directory");
        return false;
    }
    queue->addRenderQueueForComposition(compositionId, compositionName);
    const int jobIndex = queue->jobCount() - 1;
    if (jobIndex < 0) {
        *failure = QStringLiteral("Render Manager did not enqueue the audio job");
        return false;
    }
    queue->setJobOutputPathAt(jobIndex, outputPath);
    queue->setJobOutputSettingsAt(jobIndex, QStringLiteral("WAV"),
                                  QStringLiteral("pcm_s16le"), 320, 180,
                                  30.0, 8000);
    queue->setJobAudioSampleRateAt(jobIndex, 48000);
    queue->setJobIntegratedRenderEnabledAt(jobIndex, false);
    queue->startRenderQueueAt(jobIndex);

    QElapsedTimer timeout;
    timeout.start();
    QString status;
    while (timeout.elapsed() < 60000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        status = queue->jobStatusAt(jobIndex);
        if (status == QStringLiteral("Completed") ||
            status == QStringLiteral("Failed") ||
            status == QStringLiteral("Canceled")) {
            break;
        }
        QThread::msleep(10);
    }
    if (status != QStringLiteral("Completed")) {
        *failure = QStringLiteral("WAV Render Queue job ended as '%1': %2")
                       .arg(status, queue->jobErrorMessageAt(jobIndex));
        return false;
    }
    return true;
}

} // namespace

TEST(AudioLayerRenderIntegrationTest,
     AudioLayerWaveformAndVolumeArePreserved)
{
    QTemporaryDir root;
    ASSERT_TRUE(root.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", root.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    const QString sourcePath = root.filePath(QStringLiteral("tone.wav"));
    ASSERT_TRUE(writeFixtureWav(sourcePath));

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    struct Cleanup final {
        Artifact::ArtifactProjectManager& projects;
        ~Cleanup()
        {
            projects.closeCurrentProject();
        }
    } cleanup{projects};

    projects.closeCurrentProject();
    projects.createProject(QStringLiteral("AudioRenderQueueTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("AudioComposition")));
    compositionParams.setResolution(320, 180);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(3);
    const auto compositionResult = projects.createComposition(compositionParams);
    ASSERT_TRUE(compositionResult.success);

    Artifact::ArtifactAudioInitParams audioParams(QStringLiteral("Tone"));
    audioParams.setAudioPath(sourcePath);
    const auto layerResult = projects.addLayerToComposition(
        compositionResult.id, audioParams);
    ASSERT_TRUE(layerResult.success);
    auto* audio = dynamic_cast<Artifact::ArtifactAudioLayer*>(layerResult.layer.get());
    ASSERT_NE(audio, nullptr);
    audio->setTimelineWindow(ArtifactCore::FramePosition(0),
                             ArtifactCore::FramePosition(3));
    ASSERT_TRUE(audio->isLoaded());
    EXPECT_NEAR(audio->duration(), 0.1, 1.0e-9);

    const auto waveform = audio->buildWaveformData(64);
    ASSERT_EQ(waveform.peaks.size(), 64);
    ASSERT_EQ(waveform.rms.size(), 64);
    EXPECT_GT(*std::max_element(waveform.peaks.begin(), waveform.peaks.end()),
              0.25f);
    EXPECT_GT(*std::max_element(waveform.rms.begin(), waveform.rms.end()), 0.1f);

    ArtifactCore::AudioSegment fullOutput;
    ASSERT_TRUE(audio->getAudio(fullOutput, ArtifactCore::FramePosition(0),
                                4800, 48000));
    ASSERT_GE(fullOutput.channelCount(), 1);
    const double fullRms = rms(fullOutput.channelData[0]);
    EXPECT_GT(fullRms, 0.1);

    const auto composition = projects.findComposition(compositionResult.id).ptr.lock();
    ASSERT_TRUE(composition);
    ArtifactCore::AudioSegment renderedFullOutput;
    ASSERT_TRUE(composition->getAudio(renderedFullOutput,
                                      ArtifactCore::FramePosition(0),
                                      4800, 48000));
    const double renderedFullRms = rms(renderedFullOutput.channelData[0]);
    EXPECT_GT(renderedFullRms, 0.05);

    audio->setVolume(0.5f);
    ArtifactCore::AudioSegment halfOutput;
    ASSERT_TRUE(audio->getAudio(halfOutput, ArtifactCore::FramePosition(0),
                                4800, 48000));
    EXPECT_NEAR(rms(halfOutput.channelData[0]) / fullRms, 0.5, 0.03);

    ArtifactCore::AudioSegment renderedHalfOutput;
    ASSERT_TRUE(composition->getAudio(renderedHalfOutput,
                                      ArtifactCore::FramePosition(0),
                                      4800, 48000));
    EXPECT_NEAR(rms(renderedHalfOutput.channelData[0]) / renderedFullRms,
                0.5, 0.08);
}

TEST(AudioLayerRenderIntegrationTest, OverlappingSamplesMixInCompositionOutput)
{
    QTemporaryDir root;
    ASSERT_TRUE(root.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", root.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    const QString tone440 = root.filePath(QStringLiteral("tone-440.wav"));
    const QString tone880 = root.filePath(QStringLiteral("tone-880.wav"));
    ASSERT_TRUE(writeFixtureWav(tone440, 440.0, 9000.0));
    ASSERT_TRUE(writeFixtureWav(tone880, 880.0, 9000.0));

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    struct Cleanup final {
        Artifact::ArtifactProjectManager& projects;
        ~Cleanup() { projects.closeCurrentProject(); }
    } cleanup{projects};

    projects.closeCurrentProject();
    projects.createProject(QStringLiteral("AudioMixRenderQueueTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());
    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("AudioMixComposition")));
    compositionParams.setResolution(320, 180);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(3);
    const auto compositionResult = projects.createComposition(compositionParams);
    ASSERT_TRUE(compositionResult.success);
    Artifact::ArtifactAudioInitParams firstParams(QStringLiteral("Tone 440"));
    firstParams.setAudioPath(tone440);
    const auto firstResult = projects.addLayerToComposition(
        compositionResult.id, firstParams);
    ASSERT_TRUE(firstResult.success);
    auto* first = dynamic_cast<Artifact::ArtifactAudioLayer*>(firstResult.layer.get());
    ASSERT_NE(first, nullptr);
    first->setTimelineWindow(ArtifactCore::FramePosition(0),
                             ArtifactCore::FramePosition(3));

    Artifact::ArtifactAudioInitParams secondParams(QStringLiteral("Tone 880"));
    secondParams.setAudioPath(tone880);
    const auto secondResult = projects.addLayerToComposition(
        compositionResult.id, secondParams);
    ASSERT_TRUE(secondResult.success);
    auto* secondLayer = dynamic_cast<Artifact::ArtifactAudioLayer*>(secondResult.layer.get());
    ASSERT_NE(secondLayer, nullptr);
    secondLayer->setTimelineWindow(ArtifactCore::FramePosition(0),
                                   ArtifactCore::FramePosition(3));
    ASSERT_TRUE(first->isLoaded());
    ASSERT_TRUE(secondLayer->isLoaded());

    ArtifactCore::AudioSegment firstSignal;
    ArtifactCore::AudioSegment secondSignal;
    ASSERT_TRUE(first->getAudio(firstSignal, ArtifactCore::FramePosition(0),
                                4800, 48000));
    ASSERT_TRUE(secondLayer->getAudio(secondSignal, ArtifactCore::FramePosition(0),
                                      4800, 48000));

    const auto composition = projects.findComposition(compositionResult.id).ptr.lock();
    ASSERT_TRUE(composition);
    secondLayer->setMuted(true);
    ArtifactCore::AudioSegment renderedSingleLayer;
    ASSERT_TRUE(composition->getAudio(renderedSingleLayer,
                                      ArtifactCore::FramePosition(0),
                                      4800, 48000));
    secondLayer->setMuted(false);
    ArtifactCore::AudioSegment renderedMix;
    ASSERT_TRUE(composition->getAudio(renderedMix,
                                      ArtifactCore::FramePosition(0),
                                      4800, 48000));
    EXPECT_GT(rms(renderedMix.channelData[0]),
              rms(renderedSingleLayer.channelData[0]));

    ASSERT_EQ(firstSignal.frameCount(), secondSignal.frameCount());
    QVector<float> mixed(firstSignal.frameCount());
    for (int frame = 0; frame < mixed.size(); ++frame) {
        mixed[frame] = firstSignal.channelData[0][frame] +
                       secondSignal.channelData[0][frame];
    }
    EXPECT_GT(rms(mixed), rms(firstSignal.channelData[0]));
    EXPECT_GT(rms(mixed), rms(secondSignal.channelData[0]));
}

TEST(AudioLayerRenderIntegrationTest,
     WritesMixedAudioThroughRenderQueueAndHonorsVolume)
{
    QTemporaryDir root;
    ASSERT_TRUE(root.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", root.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    const QString tone440 = root.filePath(QStringLiteral("tone-440.wav"));
    const QString tone880 = root.filePath(QStringLiteral("tone-880.wav"));
    ASSERT_TRUE(writeFixtureWav(tone440, 440.0, 9000.0));
    ASSERT_TRUE(writeFixtureWav(tone880, 880.0, 9000.0));

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    auto* queue = Artifact::ArtifactRenderQueueService::instance();
    ASSERT_NE(queue, nullptr);
    queue->removeAllRenderQueues();
    queue->clearCompletedJobHistory();
    struct Cleanup final {
        Artifact::ArtifactProjectManager& projects;
        Artifact::ArtifactRenderQueueService* queue;
        ~Cleanup()
        {
            queue->removeAllRenderQueues();
            projects.closeCurrentProject();
        }
    } cleanup{projects, queue};

    projects.closeCurrentProject();
    projects.createProject(QStringLiteral("AudioRenderQueueMixTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());
    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("AudioRenderQueueMix")));
    compositionParams.setResolution(320, 180);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(3);
    const auto compositionResult = projects.createComposition(compositionParams);
    ASSERT_TRUE(compositionResult.success);

    Artifact::ArtifactAudioInitParams firstParams(QStringLiteral("Tone 440"));
    firstParams.setAudioPath(tone440);
    const auto firstResult = projects.addLayerToComposition(
        compositionResult.id, firstParams);
    ASSERT_TRUE(firstResult.success);
    auto* first = dynamic_cast<Artifact::ArtifactAudioLayer*>(firstResult.layer.get());
    ASSERT_NE(first, nullptr);
    first->setTimelineWindow(ArtifactCore::FramePosition(0),
                             ArtifactCore::FramePosition(3));

    Artifact::ArtifactAudioInitParams secondParams(QStringLiteral("Tone 880"));
    secondParams.setAudioPath(tone880);
    const auto secondResult = projects.addLayerToComposition(
        compositionResult.id, secondParams);
    ASSERT_TRUE(secondResult.success);
    auto* second = dynamic_cast<Artifact::ArtifactAudioLayer*>(secondResult.layer.get());
    ASSERT_NE(second, nullptr);
    second->setTimelineWindow(ArtifactCore::FramePosition(0),
                              ArtifactCore::FramePosition(3));
    ASSERT_TRUE(first->isLoaded());
    ASSERT_TRUE(second->isLoaded());

    QString failure;
    const QString fullPath = root.filePath(QStringLiteral("mixed-full.wav"));
    ASSERT_TRUE(renderCompositionWav(queue, compositionResult.id,
                                     QStringLiteral("AudioRenderQueueMix"),
                                     fullPath, &failure)) << failure.toStdString();
    ArtifactCore::SimpleWav fullWav;
    ASSERT_TRUE(fullWav.loadFromFile(fullPath));
    ASSERT_EQ(fullWav.sampleRate(), 48000);
    ASSERT_EQ(fullWav.frameCount(), 4800);
    ASSERT_GE(fullWav.channelCount(), 1);
    const double fullRms = rms(fullWav.getAudioData());
    EXPECT_GT(fullRms, 0.05);

    const QString preservedOutputDir = qEnvironmentVariable(
        "ARTIFACT_AUDIO_TEST_OUTPUT_DIR");
    if (!preservedOutputDir.isEmpty()) {
        ASSERT_TRUE(QDir().mkpath(preservedOutputDir));
        ASSERT_TRUE(QFile::copy(
            fullPath,
            QDir(preservedOutputDir).filePath(QStringLiteral("mixed-full.wav"))));
    }

    first->setVolume(0.5f);
    second->setVolume(0.5f);
    const QString halfPath = root.filePath(QStringLiteral("mixed-half.wav"));
    ASSERT_TRUE(renderCompositionWav(queue, compositionResult.id,
                                     QStringLiteral("AudioRenderQueueMix"),
                                     halfPath, &failure)) << failure.toStdString();
    ArtifactCore::SimpleWav halfWav;
    ASSERT_TRUE(halfWav.loadFromFile(halfPath));
    ASSERT_EQ(halfWav.sampleRate(), 48000);
    ASSERT_EQ(halfWav.frameCount(), 4800);
    EXPECT_NEAR(rms(halfWav.getAudioData()) / fullRms, 0.5, 0.08);
    if (!preservedOutputDir.isEmpty()) {
        ASSERT_TRUE(QFile::copy(
            halfPath,
            QDir(preservedOutputDir).filePath(QStringLiteral("mixed-half.wav"))));
    }
}
