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
#include <QtEndian>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstdint>

import Artifact.Audio.Waveform;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Composition.Result;
import Artifact.Layer.Audio;
import Artifact.Layer.InitParams;
import Artifact.Layer.Result;
import Artifact.Layer.Video;
import Artifact.Project.Manager;
import Artifact.Render.Queue.Job;
import Artifact.Render.Queue.Service;
import Audio.SimpleWav;
import Audio.Segment;
import Encoder.FFmpegEncoder;
import FloatRGBA;
import Graphics.SurfaceColorContract;
import Image.ImageF32x4_RGBA;
import Media.Encoder.FFmpegAudioEncoder;
import Media.Encoder.FFmpegAudioDecoder;
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

bool writeFixtureWavWithTimedTones(const QString& path)
{
    constexpr int sampleRate = 48000;
    constexpr int durationSeconds = 3;
    constexpr int frameCount = sampleRate * durationSeconds;
    constexpr double pi = 3.14159265358979323846;
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
    for (int frame = 0; frame < frameCount; ++frame) {
        const double timeSeconds =
            static_cast<double>(frame) / sampleRate;
        const bool toneActive =
            (timeSeconds >= 0.35 && timeSeconds < 0.85) ||
            (timeSeconds >= 2.15 && timeSeconds < 2.65);
        const double phase = 2.0 * pi * 440.0 * frame / sampleRate;
        const auto sample = static_cast<qint16>(toneActive
            ? std::lround(std::sin(phase) * 12000.0)
            : 0);
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

double rmsWindow(const ArtifactCore::SimpleWav& wav, double startSeconds,
                 double endSeconds)
{
    const int sampleRate = wav.sampleRate();
    const int channelCount = wav.channelCount();
    const auto& samples = wav.getAudioData();
    if (sampleRate <= 0 || channelCount <= 0 || startSeconds < 0.0 ||
        endSeconds <= startSeconds) {
        return 0.0;
    }
    const qsizetype firstSample = std::min<qsizetype>(
        samples.size(), static_cast<qsizetype>(
            std::floor(startSeconds * sampleRate) * channelCount));
    const qsizetype lastSample = std::min<qsizetype>(
        samples.size(), static_cast<qsizetype>(
            std::ceil(endSeconds * sampleRate) * channelCount));
    if (firstSample >= lastSample) {
        return 0.0;
    }
    double sumSquares = 0.0;
    for (qsizetype index = firstSample; index < lastSample; ++index) {
        const double sample = samples[index];
        sumSquares += sample * sample;
    }
    return std::sqrt(sumSquares / static_cast<double>(lastSample - firstSample));
}

double pcm24WavWindowRms(const QString& path, int sampleRate,
                         int channelCount, double startSeconds,
                         double endSeconds)
{
    if (sampleRate <= 0 || channelCount <= 0 || startSeconds < 0.0 ||
        endSeconds <= startSeconds) {
        return -1.0;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return -1.0;
    }
    const QByteArray bytes = file.readAll();
    if (bytes.size() < 12 || bytes.first(4) != QByteArrayLiteral("RIFF") ||
        bytes.mid(8, 4) != QByteArrayLiteral("WAVE")) {
        return -1.0;
    }
    const int dataTag = bytes.indexOf(QByteArrayLiteral("data"), 12);
    if (dataTag < 0 || dataTag + 8 > bytes.size()) {
        return -1.0;
    }
    const auto* sizeBytes = reinterpret_cast<const uchar*>(
        bytes.constData() + dataTag + 4);
    const quint32 dataSize = qFromLittleEndian<quint32>(sizeBytes);
    const int dataOffset = dataTag + 8;
    const quint32 bytesPerFrame = static_cast<quint32>(3 * channelCount);
    if (dataSize == 0 || dataSize % bytesPerFrame != 0 ||
        dataSize > static_cast<quint32>(bytes.size() - dataOffset)) {
        return -1.0;
    }

    const auto* samples = reinterpret_cast<const uchar*>(
        bytes.constData() + dataOffset);
    const quint32 frameCount = dataSize / bytesPerFrame;
    const quint32 firstFrame = static_cast<quint32>(std::min<double>(
        frameCount, std::floor(startSeconds * sampleRate)));
    const quint32 lastFrame = static_cast<quint32>(std::min<double>(
        frameCount, std::ceil(endSeconds * sampleRate)));
    if (firstFrame >= lastFrame) {
        return -1.0;
    }

    double sumSquares = 0.0;
    quint64 sampleCount = 0;
    for (quint32 frame = firstFrame; frame < lastFrame; ++frame) {
        for (int channel = 0; channel < channelCount; ++channel) {
            const quint32 offset = frame * bytesPerFrame +
                static_cast<quint32>(channel * 3);
            std::int32_t sample = static_cast<std::int32_t>(samples[offset]) |
                (static_cast<std::int32_t>(samples[offset + 1]) << 8) |
                (static_cast<std::int32_t>(samples[offset + 2]) << 16);
            if ((sample & 0x00800000) != 0) {
                sample |= ~0x00FFFFFF;
            }
            const double normalized = static_cast<double>(sample) / 8388608.0;
            sumSquares += normalized * normalized;
            ++sampleCount;
        }
    }
    return std::sqrt(sumSquares / static_cast<double>(sampleCount));
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

bool renderCompositionMp4WithAudio(
    Artifact::ArtifactRenderQueueService* queue,
    const auto& compositionId,
    const QString& compositionName,
    const QString& outputPath,
    QString* failure,
    double frameRate = 30.0)
{
    if (!QDir().mkpath(QFileInfo(outputPath).absolutePath())) {
        *failure = QStringLiteral("Could not create MP4 output directory");
        return false;
    }
    queue->addRenderQueueForComposition(compositionId, compositionName);
    const int jobIndex = queue->jobCount() - 1;
    if (jobIndex < 0) {
        *failure = QStringLiteral("Render Manager did not enqueue the MP4 job");
        return false;
    }
    queue->setJobOutputPathAt(jobIndex, outputPath);
    queue->setJobOutputSettingsAt(jobIndex, QStringLiteral("MP4"),
                                  QStringLiteral("h264"), 320, 180,
                                  frameRate, 2000);
    queue->setJobAudioCodecAt(jobIndex, QStringLiteral("aac"));
    queue->setJobAudioBitrateKbpsAt(jobIndex, 128);
    queue->setJobAudioChannelModeAt(jobIndex, QStringLiteral("stereo"));
    queue->setJobAudioSampleRateAt(jobIndex, 48000);
    queue->setJobIntegratedRenderEnabledAt(jobIndex, true);
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
        *failure = QStringLiteral("MP4 Render Queue job ended as '%1': %2")
                       .arg(status, queue->jobErrorMessageAt(jobIndex));
        return false;
    }
    return true;
}

void waitForVideoLayer(Artifact::ArtifactVideoLayer* video)
{
    QElapsedTimer timeout;
    timeout.start();
    while (video && !video->isLoaded() && timeout.elapsed() < 30000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
}

ArtifactCore::FloatRGBA canonicalPixel(
    const ArtifactCore::ImageF32x4_RGBA& image, int x, int y)
{
    const auto pixel = image.getPixel(x, y);
    if (image.colorDescriptor().channelOrder ==
        ArtifactCore::SurfaceChannelOrder::BGRA) {
        return ArtifactCore::FloatRGBA(
            pixel.b(), pixel.g(), pixel.r(), pixel.a());
    }
    return pixel;
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

TEST(AudioLayerRenderIntegrationTest,
     RendersSynchronizedVideoAndAudioLayersIntoMuxedMp4)
{
    QTemporaryDir root;
    ASSERT_TRUE(root.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", root.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    if (!ArtifactCore::FFmpegEncoder::isCodecAvailable(QStringLiteral("mjpeg"))) {
        GTEST_SKIP() << "FFmpeg MJPEG encoder is unavailable on this host";
    }
    if (!ArtifactCore::FFmpegEncoder::isEncoderAvailableByName(
            QStringLiteral("mjpeg"))) {
        GTEST_SKIP() << "FFmpeg MJPEG encoder is unavailable on this host";
    }

    constexpr int width = 320;
    constexpr int height = 180;
    const QString videoPath = root.filePath(QStringLiteral("timed-color-video.avi"));
    ArtifactCore::FFmpegEncoderSettings videoSettings;
    videoSettings.width = width;
    videoSettings.height = height;
    videoSettings.fps = 2.0;
    videoSettings.videoCodec = QStringLiteral("mjpeg");
    videoSettings.container = QStringLiteral("avi");
    videoSettings.encoderName = QStringLiteral("mjpeg");
    videoSettings.gopSize = 1;
    videoSettings.maxBFrames = 0;
    videoSettings.threadCount = 1;
    ArtifactCore::FFmpegEncoder videoEncoder;
    ASSERT_TRUE(videoEncoder.open(videoPath, videoSettings))
        << videoEncoder.lastError().toStdString();
    ArtifactCore::ImageF32x4_RGBA videoFrame;
    videoFrame.resize(width, height);
    for (int frame = 0; frame < 6; ++frame) {
        const ArtifactCore::FloatRGBA color = frame < 2
            ? ArtifactCore::FloatRGBA(0.9f, 0.08f, 0.04f, 1.0f)
            : frame < 4
                ? ArtifactCore::FloatRGBA(0.04f, 0.85f, 0.08f, 1.0f)
                : ArtifactCore::FloatRGBA(0.04f, 0.08f, 0.9f, 1.0f);
        videoFrame.fill(color);
        ASSERT_TRUE(videoEncoder.addImage(videoFrame))
            << videoEncoder.lastError().toStdString();
    }
    videoEncoder.close();
    ASSERT_TRUE(QFileInfo::exists(videoPath));

    const QString audioPath = root.filePath(QStringLiteral("timed-tones.wav"));
    ASSERT_TRUE(writeFixtureWavWithTimedTones(audioPath));

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
    projects.createProject(QStringLiteral("VideoAudioMuxRenderTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("VideoAndAudio")));
    compositionParams.setResolution(width, height);
    compositionParams.setFrameRate(2.0);
    compositionParams.setDurationFrames(6);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactVideoInitParams videoParams(QStringLiteral("Timed Color Video"));
    videoParams.setVideoPath(videoPath);
    const auto videoResult = projects.addLayerToComposition(
        composition.id, videoParams);
    ASSERT_TRUE(videoResult.success);
    auto* video = dynamic_cast<Artifact::ArtifactVideoLayer*>(
        videoResult.layer.get());
    ASSERT_NE(video, nullptr);
    waitForVideoLayer(video);
    ASSERT_TRUE(video->isLoaded()) << video->debugState().toStdString();
    video->preloadFrames(0, 6);
    const auto sourceVideoFrame = video->decodeFrameToImageBuffer(
        std::int64_t{0});
    ASSERT_FALSE(sourceVideoFrame.isEmpty());

    Artifact::ArtifactAudioInitParams audioParams(QStringLiteral("Tone"));
    audioParams.setAudioPath(audioPath);
    const auto audioResult = projects.addLayerToComposition(
        composition.id, audioParams);
    ASSERT_TRUE(audioResult.success);
    auto* audio = dynamic_cast<Artifact::ArtifactAudioLayer*>(
        audioResult.layer.get());
    ASSERT_NE(audio, nullptr);
    audio->setTimelineWindow(ArtifactCore::FramePosition(0),
                             ArtifactCore::FramePosition(6));
    ASSERT_TRUE(audio->isLoaded());

    QString failure;
    const QString outputPath = root.filePath(QStringLiteral("video-with-audio.mp4"));
    ASSERT_TRUE(renderCompositionMp4WithAudio(
        queue, composition.id, QStringLiteral("VideoAndAudio"), outputPath,
        &failure, 2.0)) << failure.toStdString();
    ASSERT_TRUE(QFileInfo(outputPath).size() > 0);
    const QString preservedOutputDir = qEnvironmentVariable(
        "ARTIFACT_AUDIO_TEST_OUTPUT_DIR");
    if (!preservedOutputDir.isEmpty()) {
        ASSERT_TRUE(QDir().mkpath(preservedOutputDir));
        ASSERT_TRUE(QFile::copy(
            outputPath,
            QDir(preservedOutputDir).filePath(
                QStringLiteral("synchronized-video-audio.mp4"))));
    }

    const auto audioInfo = ArtifactCore::FFmpegAudioEncoder::probeAudioFile(outputPath);
    ASSERT_TRUE(audioInfo.valid) << audioInfo.errorMessage.toStdString();
    EXPECT_EQ(audioInfo.codec, QStringLiteral("aac"));
    EXPECT_EQ(audioInfo.sampleRate, 48000);
    EXPECT_EQ(audioInfo.channels, 2);

    Artifact::ArtifactVideoLayer renderedVideo;
    ASSERT_TRUE(renderedVideo.loadFromPath(outputPath));
    waitForVideoLayer(&renderedVideo);
    ASSERT_TRUE(renderedVideo.isLoaded())
        << renderedVideo.debugState().toStdString();
    EXPECT_TRUE(renderedVideo.streamInfo().hasAudio);
    EXPECT_GT(renderedVideo.streamInfo().frameCount, 0);
    EXPECT_NEAR(audioInfo.durationMs / 1000.0,
                renderedVideo.streamInfo().duration, 0.15);
    const auto expectRenderedFrameColor = [&](std::int64_t frameNumber,
                                              char expectedChannel) {
        const auto frame = renderedVideo.decodeFrameToImageBuffer(frameNumber);
        ASSERT_FALSE(frame.isEmpty()) << "frame=" << frameNumber;
        const auto pixel = canonicalPixel(frame, width / 2, height / 2);
        if (expectedChannel == 'r') {
            EXPECT_GT(pixel.r(), 0.5f) << "frame=" << frameNumber;
            EXPECT_LT(pixel.g(), 0.25f) << "frame=" << frameNumber;
            EXPECT_LT(pixel.b(), 0.25f) << "frame=" << frameNumber;
        } else if (expectedChannel == 'g') {
            EXPECT_LT(pixel.r(), 0.25f) << "frame=" << frameNumber;
            EXPECT_GT(pixel.g(), 0.5f) << "frame=" << frameNumber;
            EXPECT_LT(pixel.b(), 0.25f) << "frame=" << frameNumber;
        } else {
            EXPECT_LT(pixel.r(), 0.25f) << "frame=" << frameNumber;
            EXPECT_LT(pixel.g(), 0.25f) << "frame=" << frameNumber;
            EXPECT_GT(pixel.b(), 0.5f) << "frame=" << frameNumber;
        }
    };
    expectRenderedFrameColor(1, 'r');
    expectRenderedFrameColor(3, 'g');
    expectRenderedFrameColor(5, 'b');

    EXPECT_GT(renderedVideo.streamInfo().duration, 0.0);

    QString audioFailure;
    const QString renderedAudioPath = root.filePath(
        QStringLiteral("timed-composition-audio.wav"));
    ASSERT_TRUE(renderCompositionWav(
        queue, composition.id, QStringLiteral("VideoAndAudio"),
        renderedAudioPath, &audioFailure)) << audioFailure.toStdString();
    ArtifactCore::SimpleWav renderedAudio;
    ASSERT_TRUE(renderedAudio.loadFromFile(renderedAudioPath));
    ASSERT_EQ(renderedAudio.sampleRate(), 48000);
    ASSERT_EQ(renderedAudio.frameCount(), 144000);
    EXPECT_GT(rmsWindow(renderedAudio, 0.45, 0.70), 0.05);
    EXPECT_LT(rmsWindow(renderedAudio, 1.20, 1.80), 0.01);
    EXPECT_GT(rmsWindow(renderedAudio, 2.25, 2.50), 0.05);

    const QString decodedMuxedAudioPath = root.filePath(
        QStringLiteral("decoded-muxed-audio.wav"));
    ASSERT_TRUE(ArtifactCore::FFmpegAudioEncoder::encodeAudio(
        outputPath, decodedMuxedAudioPath, QStringLiteral("pcm"),
        256000, 48000));
    const auto decodedAudioInfo =
        ArtifactCore::FFmpegAudioEncoder::probeAudioFile(decodedMuxedAudioPath);
    ASSERT_TRUE(decodedAudioInfo.valid)
        << decodedAudioInfo.errorMessage.toStdString();
    EXPECT_EQ(decodedAudioInfo.sampleRate, 48000);
    EXPECT_EQ(decodedAudioInfo.channels, 2);
    EXPECT_NEAR(decodedAudioInfo.durationMs / 1000.0,
                renderedVideo.streamInfo().duration, 0.15);
    EXPECT_GT(pcm24WavWindowRms(decodedMuxedAudioPath, 48000, 2,
                                0.45, 0.70), 0.05);
    EXPECT_LT(pcm24WavWindowRms(decodedMuxedAudioPath, 48000, 2,
                                1.20, 1.80), 0.01);
    EXPECT_GT(pcm24WavWindowRms(decodedMuxedAudioPath, 48000, 2,
                                2.25, 2.50), 0.05);

    ArtifactCore::FFmpegAudioDecoder muxedAudioDecoder;
    ASSERT_TRUE(muxedAudioDecoder.openFile(outputPath));
    EXPECT_EQ(muxedAudioDecoder.sampleRate(), 48000);
    EXPECT_EQ(muxedAudioDecoder.channelCount(), 2);
    ArtifactCore::AudioSegment firstMuxedAudioSegment;
    ASSERT_TRUE(muxedAudioDecoder.decodeNextSegment(firstMuxedAudioSegment));
    EXPECT_GT(firstMuxedAudioSegment.frameCount(), 0);
    EXPECT_EQ(firstMuxedAudioSegment.sampleRate, 48000);
}
