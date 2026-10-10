#include <gtest/gtest.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSize>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryDir>
#include <QThread>
#include <QVariantMap>
#include <QVector3D>

#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>

import Artifact.Composition.InitParams;
import Artifact.Composition.Result;
import Artifact.Layer.Abstract;
import Artifact.Layer.Camera;
import Artifact.Layer.InitParams;
import Artifact.Layer.Particle;
import Artifact.Layer.Result;
import Artifact.Layer.Text;
import Artifact.Generator.Particle;
import Artifact.Project.Manager;
import Artifact.Render.IRenderer;
import Artifact.Render.Queue.Job;
import Artifact.Render.Queue.Service;
import Color.Float;
import Utils.Id;
import Utils.String.UniString;

namespace {

class RenderQueueTestEnvironment final : public testing::Environment {
public:
    void SetUp() override
    {
        static int argc = 1;
        static char applicationName[] = "ArtifactRenderQueueLayerImageIntegrationTest";
        static char* argv[] = {applicationName, nullptr};
        application_ = new QGuiApplication(argc, argv);
    }

    void TearDown() override
    {
        if (application_) {
            delete application_;
            application_ = nullptr;
        }
    }

private:
    QGuiApplication* application_ = nullptr;
};

RenderQueueTestEnvironment* const renderQueueEnvironment =
    new RenderQueueTestEnvironment();
const bool renderQueueEnvironmentRegistered = [] {
    testing::AddGlobalTestEnvironment(renderQueueEnvironment);
    return true;
}();

std::mutex xpuLogMutex;
QStringList xpuDispatchMessages;
QtMessageHandler previousQtMessageHandler = nullptr;

void captureXpuDispatchMessage(QtMsgType type,
                              const QMessageLogContext& context,
                              const QString& message)
{
    if (message.contains(QStringLiteral("XPU mixed active")) ||
        message.contains(QStringLiteral("XPU active plan")) ||
        message.contains(QStringLiteral("XPU job summary")) ||
        message.contains(QStringLiteral("XPU mixed dispatch unavailable")) ||
        message.contains(QStringLiteral("[DiligentDeviceManager] XPU DXGI"))) {
        std::lock_guard<std::mutex> lock(xpuLogMutex);
        xpuDispatchMessages.push_back(message);
    }
    if (previousQtMessageHandler) {
        previousQtMessageHandler(type, context, message);
    }
}

int countPixelsMatching(const QImage& source,
                        const std::function<bool(QRgb)>& predicate)
{
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (predicate(image.pixel(x, y))) {
                ++count;
            }
        }
    }
    return count;
}

QString isolatedOutputPath(const QString& fileName)
{
    const QString root = qEnvironmentVariable(
        "ARTIFACT_UI_TEST_PROJECT_ROOT");
    return QDir(root).filePath(QStringLiteral("captures/") + fileName);
}

bool addAndRender(Artifact::ArtifactRenderQueueService* queue,
                  const ArtifactCore::CompositionID& compositionId,
                  const QString& compositionName,
                  const QString& outputBasePath,
                  int expectedWidth,
                  int expectedHeight,
                  const std::function<bool(const QImage&)>& imageCheck,
                  QString* failure,
                  int frameEnd = 2,
                  const QString& renderBackend = QStringLiteral("gpu"),
                  int frameStart = 1)
{
    const QFileInfo requestedOutput(outputBasePath);
    if (!QDir().mkpath(requestedOutput.absolutePath())) {
        *failure = QStringLiteral("Could not create isolated render output directory");
        return false;
    }

    queue->addRenderQueueForComposition(compositionId, compositionName);
    const int jobIndex = queue->jobCount() - 1;
    if (jobIndex < 0) {
        *failure = QStringLiteral("Render Manager service did not enqueue the composition");
        return false;
    }

    queue->setJobOutputPathAt(jobIndex, outputBasePath);
    queue->setJobOutputSettingsAt(jobIndex, QStringLiteral("PNG"),
                                  QStringLiteral("png"), expectedWidth,
                                  expectedHeight, 30.0, 8000);
    queue->setJobRenderBackendAt(jobIndex, renderBackend);
    queue->setJobFrameRangeAt(jobIndex, frameStart, frameEnd);
    auto selectiveSettings = queue->jobSelectiveSettingsAt(jobIndex);
    selectiveSettings.insert(
        QStringLiteral("frameRangeMode"),
        static_cast<int>(Artifact::ArtifactRenderJob::FrameRangeMode::Custom));
    if (!queue->setJobSelectiveSettingsAt(jobIndex, selectiveSettings)) {
        *failure = QStringLiteral("Could not select the explicit frame range");
        return false;
    }
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
        *failure = QStringLiteral("Render Queue job ended as '%1': %2")
                       .arg(status, queue->jobErrorMessageAt(jobIndex));
        return false;
    }

    const QFileInfo outputInfo(outputBasePath);
    const QString framePath = QDir(outputInfo.absolutePath()).filePath(
        QStringLiteral("%1_%2.png")
            .arg(outputInfo.completeBaseName())
            .arg(frameStart, 4, 10, QLatin1Char('0')));
    const QImage image(framePath);
    if (image.isNull()) {
        *failure = QStringLiteral("Render Queue did not write a readable PNG: %1")
                       .arg(framePath);
        return false;
    }
    if (image.size() != QSize(expectedWidth, expectedHeight)) {
        *failure = QStringLiteral("PNG dimensions were %1x%2, expected %3x%4")
                       .arg(image.width()).arg(image.height())
                       .arg(expectedWidth).arg(expectedHeight);
        return false;
    }
    if (!imageCheck(image)) {
        *failure = QStringLiteral("PNG was written, but expected layer pixels were not visible: %1")
                       .arg(framePath);
        return false;
    }
    return true;
}

} // namespace

TEST(RenderQueueLayerImageIntegrationTest,
     Production2DAnd3DParticlesRenderThroughManagerQueue)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 320;
    constexpr int height = 180;
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
    projects.createProject(QStringLiteral("RenderQueueLayerImageTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    const auto makeComposition = [&](const QString& name) {
        Artifact::ArtifactCompositionInitParams params;
        params.setCompositionName(ArtifactCore::UniString(name));
        params.setResolution(width, height);
        params.setFrameRate(30.0);
        params.setDurationFrames(2);
        params.setBackgroundColor(ArtifactCore::FloatColor(
            0.012f, 0.018f, 0.032f, 1.0f));
        return projects.createComposition(params);
    };

    const auto addLayer = [&](const ArtifactCore::CompositionID& compositionId,
                              const QString& name,
                              Artifact::LayerType type) {
        Artifact::ArtifactLayerInitParams params(name, type);
        return projects.addLayerToComposition(compositionId, params);
    };

    QString failure;

    const auto particles2D = makeComposition(QStringLiteral("Particles2D"));
    ASSERT_TRUE(particles2D.success);
    const auto particle2DLayer = addLayer(particles2D.id,
                                         QStringLiteral("Particles"),
                                         Artifact::LayerType::Particle);
    ASSERT_TRUE(particle2DLayer.success);
    auto* particle2D = dynamic_cast<Artifact::ArtifactParticleLayer*>(
        particle2DLayer.layer.get());
    ASSERT_NE(particle2D, nullptr);
    ASSERT_TRUE(particle2D->setEmitterPosition(
        QVector3D(width * 0.25f, height / 2.0f, 0.0f)));
    ASSERT_FALSE(particle2D->particleSystem()->emitters().empty());
    auto& particle2DParams =
        particle2D->particleSystem()->emitters().front()->params();
    particle2DParams.rate = 1000.0f;
    particle2DParams.lifeMin = 10.0f;
    particle2DParams.lifeMax = 10.0f;
    particle2DParams.speedMin = 0.0f;
    particle2DParams.speedMax = 0.0f;
    particle2DParams.directionSpread = 0.0f;
    particle2DParams.colorStart = QColor(255, 32, 16, 255);
    particle2DParams.colorMid = QColor(255, 32, 16, 255);
    particle2DParams.colorEnd = QColor(255, 32, 16, 255);
    particle2DParams.scaleMin = 12.0f;
    particle2DParams.scaleMax = 12.0f;
    ASSERT_TRUE(addAndRender(
        queue, particles2D.id, QStringLiteral("Particles2D"),
        isolatedOutputPath(QStringLiteral("render_queue_particles_2d.png")),
        width, height,
        [](const QImage& image) {
            return countPixelsMatching(image, [](QRgb pixel) {
                return qAlpha(pixel) > 32 &&
                       (qRed(pixel) > 40 || qGreen(pixel) > 40 ||
                        qBlue(pixel) > 40);
            }) > 8;
        }, &failure)) << failure.toStdString();

    const auto particles3D = makeComposition(QStringLiteral("Particles3D"));
    ASSERT_TRUE(particles3D.success);
    const auto cameraLayer = addLayer(particles3D.id, QStringLiteral("Camera"),
                                      Artifact::LayerType::Camera);
    ASSERT_TRUE(cameraLayer.success);
    auto* camera = dynamic_cast<Artifact::ArtifactCameraLayer*>(
        cameraLayer.layer.get());
    ASSERT_NE(camera, nullptr);
    camera->setActiveCamera(true);
    camera->setPosition3D({width / 2.0f, height / 2.0f, 1920.0f});

    const auto particle3DLayer = addLayer(particles3D.id,
                                         QStringLiteral("Particles 3D"),
                                         Artifact::LayerType::Particle3D);
    ASSERT_TRUE(particle3DLayer.success);
    ASSERT_NE(dynamic_cast<Artifact::ArtifactParticle3DLayer*>(
                  particle3DLayer.layer.get()), nullptr);
    auto* particle3D = dynamic_cast<Artifact::ArtifactParticle3DLayer*>(
        particle3DLayer.layer.get());
    ASSERT_NE(particle3D, nullptr);
    ASSERT_TRUE(particle3D->setEmitterPosition(
        QVector3D(width / 2.0f, height / 2.0f, 0.0f)));
    ASSERT_FALSE(particle3D->particleSystem()->emitters().empty());
    auto& particle3DParams =
        particle3D->particleSystem()->emitters().front()->params();
    particle3DParams.rate = 1000.0f;
    particle3DParams.lifeMin = 10.0f;
    particle3DParams.lifeMax = 10.0f;
    particle3DParams.speedMin = 0.0f;
    particle3DParams.speedMax = 0.0f;
    particle3DParams.directionSpread = 0.0f;
    particle3DParams.colorStart = QColor(255, 32, 16, 255);
    particle3DParams.colorMid = QColor(255, 32, 16, 255);
    particle3DParams.colorEnd = QColor(255, 32, 16, 255);
    particle3DParams.scaleMin = 12.0f;
    particle3DParams.scaleMax = 12.0f;
    ASSERT_TRUE(addAndRender(
        queue, particles3D.id, QStringLiteral("Particles3D"),
        isolatedOutputPath(QStringLiteral("render_queue_particles_3d.png")),
        width, height,
        [](const QImage& image) {
            return countPixelsMatching(image, [](QRgb pixel) {
                return qAlpha(pixel) > 32 &&
                       (qRed(pixel) > 40 || qGreen(pixel) > 40 ||
                        qBlue(pixel) > 40);
            }) > 4;
        }, &failure)) << failure.toStdString();
}

TEST(RenderQueueLayerImageIntegrationTest,
     ProductionTextLayerRendersThroughManagerQueue)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 320;
    constexpr int height = 180;
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
    projects.createProject(QStringLiteral("RenderQueueTextImageTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("TextLayer")));
    compositionParams.setResolution(width, height);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(2);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactTextLayerInitParams textParams(QStringLiteral("Text"));
    const auto textResult = projects.addLayerToComposition(
        composition.id, textParams);
    ASSERT_TRUE(textResult.success);
    auto* text = dynamic_cast<Artifact::ArtifactTextLayer*>(
        textResult.layer.get());
    ASSERT_NE(text, nullptr);
    text->setText(ArtifactCore::UniString(QStringLiteral("Render Queue Text")));
    text->setFontSize(30.0f);
    text->setTextColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));

    QString failure;
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("TextLayer"),
        isolatedOutputPath(QStringLiteral("render_queue_text.png")),
        width, height,
        [](const QImage& image) {
            return countPixelsMatching(image, [](QRgb pixel) {
                return qAlpha(pixel) > 32 && qRed(pixel) > 210 &&
                       qGreen(pixel) > 210 && qBlue(pixel) > 210;
            }) > 20;
        }, &failure)) << failure.toStdString();
}

TEST(RenderQueueLayerImageIntegrationTest,
     XpuMixedCpuAndGpuFramesRenderThroughManagerQueue)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    struct RestoreEnvironment final {
        QByteArray xpu = qgetenv("ARTIFACT_XPU");
        QByteArray maxInFlight = qgetenv("ARTIFACT_XPU_MAX_IN_FLIGHT");
        ~RestoreEnvironment()
        {
            if (xpu.isNull()) {
                qunsetenv("ARTIFACT_XPU");
            } else {
                qputenv("ARTIFACT_XPU", xpu);
            }
            if (maxInFlight.isNull()) {
                qunsetenv("ARTIFACT_XPU_MAX_IN_FLIGHT");
            } else {
                qputenv("ARTIFACT_XPU_MAX_IN_FLIGHT", maxInFlight);
            }
        }
    } restoreEnvironment;
    qputenv("ARTIFACT_XPU", "mixed=on");
    // Keep one GPU lane plus one CPU lane so the test exercises an actual
    // mixed dispatch on hosts with different core counts and adapter counts.
    qputenv("ARTIFACT_XPU_MAX_IN_FLIGHT", "2");

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
    projects.createProject(QStringLiteral("RenderQueueXpuMixedImageTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    constexpr int width = 320;
    constexpr int height = 180;
    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("XpuMixedText")));
    compositionParams.setResolution(width, height);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(4);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactTextLayerInitParams textParams(QStringLiteral("Text"));
    const auto textResult = projects.addLayerToComposition(
        composition.id, textParams);
    ASSERT_TRUE(textResult.success);
    auto* text = dynamic_cast<Artifact::ArtifactTextLayer*>(
        textResult.layer.get());
    ASSERT_NE(text, nullptr);
    text->setText(ArtifactCore::UniString(QStringLiteral("XPU Render")));
    text->setFontSize(30.0f);
    text->setTextColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));

    const QString cpuBaselinePath = isolatedOutputPath(
        QStringLiteral("render_queue_xpu_cpu_baseline.png"));
    QString failure;
    xpuDispatchMessages.clear();
    previousQtMessageHandler = qInstallMessageHandler(
        captureXpuDispatchMessage);
    struct RestoreMessageHandler final {
        ~RestoreMessageHandler()
        {
            qInstallMessageHandler(previousQtMessageHandler);
            previousQtMessageHandler = nullptr;
        }
    } restoreMessageHandler;

    for (int frameNumber = 1; frameNumber <= 3; ++frameNumber) {
        ASSERT_TRUE(addAndRender(
            queue, composition.id, QStringLiteral("XpuCpuBaseline"),
            cpuBaselinePath, width, height,
            [](const QImage& image) {
                return countPixelsMatching(image, [](QRgb pixel) {
                    return qAlpha(pixel) > 32 && qRed(pixel) > 210 &&
                           qGreen(pixel) > 210 && qBlue(pixel) > 210;
                }) > 20;
            }, &failure, frameNumber + 1, QStringLiteral("cpu"), frameNumber))
            << "Sequential CPU baseline frame " << frameNumber << ": "
            << failure.toStdString();
    }
    QStringList cpuBaselineMessages;
    {
        std::lock_guard<std::mutex> lock(xpuLogMutex);
        cpuBaselineMessages = xpuDispatchMessages;
    }
    const QString cpuBaselineLog = cpuBaselineMessages.join(QLatin1Char('\n'));
    ASSERT_TRUE(cpuBaselineLog.contains(
        QStringLiteral("cpu-render-backend-selected")))
        << cpuBaselineLog.toStdString();
    EXPECT_FALSE(cpuBaselineLog.contains(QStringLiteral("XPU mixed active")))
        << cpuBaselineLog.toStdString();
    EXPECT_TRUE(QRegularExpression(
        QStringLiteral("hardwareThreads=\\s*0[^\\n]*hostThreadLimit=\\s*-1"))
        .match(cpuBaselineLog).hasMatch())
        << "CPU-backend baseline must not enter the mixed CPU budget: "
        << cpuBaselineLog.toStdString();
    EXPECT_FALSE(QRegularExpression(
        QStringLiteral("reservedWriterThreads=\\s*[1-9]")).match(
            cpuBaselineLog).hasMatch())
        << "CPU-backend baseline must not reserve mixed-job writer slots: "
        << cpuBaselineLog.toStdString();

    const QFileInfo cpuBaselineInfo(cpuBaselinePath);
    const QString cpuBaselineBaseName = cpuBaselineInfo.completeBaseName();

    {
        std::lock_guard<std::mutex> lock(xpuLogMutex);
        xpuDispatchMessages.clear();
    }

    const QString xpuOutputPath = isolatedOutputPath(
        QStringLiteral("render_queue_xpu_mixed.png"));
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("XpuMixedText"),
        xpuOutputPath,
        width, height,
        [](const QImage& image) {
            return countPixelsMatching(image, [](QRgb pixel) {
                return qAlpha(pixel) > 32 && qRed(pixel) > 210 &&
                       qGreen(pixel) > 210 && qBlue(pixel) > 210;
            }) > 20;
        }, &failure, 4)) << failure.toStdString();

    const QFileInfo xpuOutputInfo(xpuOutputPath);
    const QString xpuOutputBaseName = xpuOutputInfo.completeBaseName();
    for (int frameNumber = 1; frameNumber <= 3; ++frameNumber) {
        const QString frameSuffix = QStringLiteral("_%1.png")
            .arg(frameNumber, 4, 10, QLatin1Char('0'));
        const QString cpuFramePath = QDir(cpuBaselineInfo.absolutePath())
            .filePath(cpuBaselineBaseName + frameSuffix);
        const QImage cpuFrame(cpuFramePath);
        ASSERT_FALSE(cpuFrame.isNull())
            << "Missing or unreadable CPU baseline frame " << frameNumber
            << ": " << cpuFramePath.toStdString();
        ASSERT_EQ(cpuFrame.size(), QSize(width, height))
            << "Unexpected CPU baseline frame size at frame " << frameNumber;

        const QString xpuFramePath = QDir(xpuOutputInfo.absolutePath())
            .filePath(xpuOutputBaseName + frameSuffix);
        const QImage xpuFrame(xpuFramePath);
        ASSERT_FALSE(xpuFrame.isNull())
            << "Missing or unreadable XPU frame " << frameNumber << ": "
            << xpuFramePath.toStdString();
        ASSERT_EQ(xpuFrame.size(), QSize(width, height))
            << "Unexpected XPU frame size at frame " << frameNumber;

    }

    QStringList capturedMessages;
    {
        std::lock_guard<std::mutex> lock(xpuLogMutex);
        capturedMessages = xpuDispatchMessages;
    }
    const QString joined = capturedMessages.join(QLatin1Char('\n'));
    if (QRegularExpression(QStringLiteral("gpuFrames=\\s*0(?:\\D|$)"))
            .match(joined).hasMatch()) {
        GTEST_SKIP() << "No active full GPU worker is available on this host";
    }
    const auto workerLimitMatch = QRegularExpression(
        QStringLiteral("frameWorkerLimit=\\s*(\\d+)")).match(joined);
    ASSERT_TRUE(workerLimitMatch.hasMatch()) << joined.toStdString();
    EXPECT_EQ(workerLimitMatch.captured(1).toInt(), 2)
        << joined.toStdString();
    const auto hardwareThreadsMatch = QRegularExpression(
        QStringLiteral("hardwareThreads=\\s*(\\d+)"))
        .match(joined);
    const auto hostThreadLimitMatch = QRegularExpression(
        QStringLiteral("hostThreadLimit=\\s*(\\d+)"))
        .match(joined);
    const auto reservedServiceThreadsMatch = QRegularExpression(
        QStringLiteral("reservedServiceThreads=\\s*(\\d+)"))
        .match(joined);
    const auto reservedEncoderThreadsMatch = QRegularExpression(
        QStringLiteral("reservedEncoderThreads=\\s*(\\d+)"))
        .match(joined);
    const auto reservedWriterThreadsMatch = QRegularExpression(
        QStringLiteral("reservedWriterThreads=\\s*(\\d+)"))
        .match(joined);
    const auto writerMaxPendingMatch = QRegularExpression(
        QStringLiteral("writerMaxPending=\\s*(\\d+)"))
        .match(joined);
    const auto asyncSequenceMatch = QRegularExpression(
        QStringLiteral("asyncSeq=\\s*(true|false)"))
        .match(joined);
    ASSERT_TRUE(hardwareThreadsMatch.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(hostThreadLimitMatch.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(reservedServiceThreadsMatch.hasMatch())
        << joined.toStdString();
    ASSERT_TRUE(reservedEncoderThreadsMatch.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(reservedWriterThreadsMatch.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(writerMaxPendingMatch.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(asyncSequenceMatch.hasMatch()) << joined.toStdString();
    EXPECT_LE(workerLimitMatch.captured(1).toInt(),
              hostThreadLimitMatch.captured(1).toInt())
        << joined.toStdString();
    const int hardwareThreads = hardwareThreadsMatch.captured(1).toInt();
    if (hardwareThreads > 1) {
        EXPECT_LE(
            hostThreadLimitMatch.captured(1).toInt() +
                reservedServiceThreadsMatch.captured(1).toInt() +
                reservedEncoderThreadsMatch.captured(1).toInt() +
                reservedWriterThreadsMatch.captured(1).toInt(),
            hardwareThreads) << joined.toStdString();
    }
    EXPECT_EQ(reservedEncoderThreadsMatch.captured(1).toInt(), 0)
        << "Image sequence jobs must not reserve a video encoder lane: "
        << joined.toStdString();
    if (asyncSequenceMatch.captured(1) == QLatin1String("true")) {
        EXPECT_GT(reservedWriterThreadsMatch.captured(1).toInt(), 0)
            << joined.toStdString();
        EXPECT_LE(writerMaxPendingMatch.captured(1).toInt(),
                  reservedWriterThreadsMatch.captured(1).toInt())
            << joined.toStdString();
    }
    ASSERT_TRUE(joined.contains(QStringLiteral("XPU mixed active")))
        << joined.toStdString();
    EXPECT_TRUE(joined.contains(
        QStringLiteral("cpuScheduler= sharedTbbArena")))
        << "XPU CPU frame lanes must run inside the shared TBB arena: "
        << joined.toStdString();
    const auto tbbConcurrencyMatch = QRegularExpression(
        QStringLiteral("tbbConcurrency=\\s*(\\d+)"))
        .match(joined);
    ASSERT_TRUE(tbbConcurrencyMatch.hasMatch()) << joined.toStdString();
    EXPECT_LE(hostThreadLimitMatch.captured(1).toInt(),
              tbbConcurrencyMatch.captured(1).toInt())
        << joined.toStdString();
    const auto cpuMatch = QRegularExpression(
        QStringLiteral("xpuCpuFrames=\\s*(\\d+)")).match(joined);
    const auto gpuMatch = QRegularExpression(
        QStringLiteral("gpuFrames=\\s*(\\d+)")).match(joined);
    const auto cpuFallbackMatch = QRegularExpression(
        QStringLiteral("xpuCpuFallbackFrames=\\s*(\\d+)")).match(joined);
    const auto gpuFallbackMatch = QRegularExpression(
        QStringLiteral("xpuGpuFallbackFrames=\\s*(\\d+)")).match(joined);
    const auto cpuWorkersMatch = QRegularExpression(
        QStringLiteral("cpuWorkers=\\s*(\\d+)")).match(joined);
    const auto cpuPlanLimitMatch = QRegularExpression(
        QStringLiteral("id=cpu:0[^\\n]*inFlight=(\\d+)")).match(joined);
    const auto gpuWorkersMatch = QRegularExpression(
        QStringLiteral("gpuWorkers=\\s*(\\d+)")).match(joined);
    const auto primaryGpuAdapterMatch = QRegularExpression(
        QStringLiteral("primaryGpuAdapterId=\\s*(-?\\d+)")).match(joined);
    const auto primaryGpuBackendMatch = QRegularExpression(
        QStringLiteral("primaryGpuBackend=\\s*\"?([^\"\\s]*)")).match(joined);
    const auto primaryGpuFramesMatch = QRegularExpression(
        QStringLiteral("primaryGpuFrames=\\s*(\\d+)")).match(joined);
    const auto primaryGpuMsMatch = QRegularExpression(
        QStringLiteral("primaryGpuMs=\\s*(\\d+(?:\\.\\d+)?)"))
        .match(joined);
    const auto gpuRenderMsMatch = QRegularExpression(
        QStringLiteral("gpuMs=\\s*(\\d+(?:\\.\\d+)?)"))
        .match(joined);
    const auto cpuRenderMsMatch = QRegularExpression(
        QStringLiteral("cpuMs=\\s*(\\d+(?:\\.\\d+)?)"))
        .match(joined);
    ASSERT_TRUE(cpuMatch.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(gpuMatch.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(cpuFallbackMatch.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(gpuFallbackMatch.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(workerLimitMatch.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(cpuWorkersMatch.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(cpuPlanLimitMatch.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(gpuWorkersMatch.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(primaryGpuAdapterMatch.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(primaryGpuBackendMatch.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(primaryGpuFramesMatch.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(primaryGpuMsMatch.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(gpuRenderMsMatch.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(cpuRenderMsMatch.hasMatch()) << joined.toStdString();
    EXPECT_GT(cpuMatch.captured(1).toInt(), 0) << joined.toStdString();
    EXPECT_GT(gpuMatch.captured(1).toInt(), 0) << joined.toStdString();
    EXPECT_EQ(cpuMatch.captured(1).toInt() + gpuMatch.captured(1).toInt(), 3)
        << joined.toStdString();
    EXPECT_EQ(cpuWorkersMatch.captured(1).toInt(),
              cpuPlanLimitMatch.captured(1).toInt())
        << joined.toStdString();
    const bool primaryD3D12HasDedicatedLane =
        primaryGpuBackendMatch.captured(1) == QLatin1String("d3d12") &&
        QRegularExpression(QStringLiteral(
            "gpuWorkerFrames=\\s*\"?backend=d3d12\\s+adapter=\\d+\\s+frames=\\d+"))
            .match(joined).hasMatch();
    if (primaryD3D12HasDedicatedLane) {
        EXPECT_TRUE(QRegularExpression(QStringLiteral(
            "gpuWorkerFrames=\\s*\"?backend=d3d12\\s+adapter=%1\\s+frames=\\d+")
                .arg(primaryGpuAdapterMatch.captured(1)))
                .match(joined).hasMatch())
            << "Primary adapter was not prioritized within the GPU worker cap: "
            << joined.toStdString();
    } else {
        EXPECT_GT(primaryGpuFramesMatch.captured(1).toInt(), 0)
            << "The primary renderer did not process any frame: "
            << joined.toStdString();
    }
    QString activePlanMessage;
    for (const QString& message : capturedMessages) {
        if (message.contains(QStringLiteral("XPU active plan"))) {
            activePlanMessage = message;
            break;
        }
    }
    ASSERT_FALSE(activePlanMessage.isEmpty()) << joined.toStdString();
    int activeGpuNodes = 0;
    auto gpuPlanMatches = QRegularExpression(
        QStringLiteral("kind=gpu-full [^|]*inFlight=(\\d+)")).globalMatch(
            activePlanMessage);
    while (gpuPlanMatches.hasNext()) {
        if (gpuPlanMatches.next().captured(1).toInt() > 0) {
            ++activeGpuNodes;
        }
    }
    EXPECT_EQ(activeGpuNodes, gpuWorkersMatch.captured(1).toInt())
        << activePlanMessage.toStdString();
    auto workerMatches = QRegularExpression(
        QStringLiteral("adapter=(\\d+) frames=(\\d+)")).globalMatch(joined);
    while (workerMatches.hasNext()) {
        const auto workerMatch = workerMatches.next();
        EXPECT_GT(workerMatch.captured(2).toInt(), 0)
            << workerMatch.captured(0).toStdString();
    }


}

TEST(RenderQueueLayerImageIntegrationTest,
     XpuMixedTwoDAndThreeDParticlesThroughRenderQueue)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    struct RestoreEnvironment final {
        QByteArray xpu = qgetenv("ARTIFACT_XPU");
        QByteArray maxInFlight = qgetenv("ARTIFACT_XPU_MAX_IN_FLIGHT");
        ~RestoreEnvironment()
        {
            if (xpu.isNull()) qunsetenv("ARTIFACT_XPU");
            else qputenv("ARTIFACT_XPU", xpu);
            if (maxInFlight.isNull()) qunsetenv("ARTIFACT_XPU_MAX_IN_FLIGHT");
            else qputenv("ARTIFACT_XPU_MAX_IN_FLIGHT", maxInFlight);
        }
    } restoreEnvironment;
    qputenv("ARTIFACT_XPU", "mixed=on");
    qputenv("ARTIFACT_XPU_MAX_IN_FLIGHT", "2");

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
    projects.createProject(QStringLiteral("RenderQueueXpuParticlesTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());
    constexpr int width = 320;
    constexpr int height = 180;
    QString failure;
    xpuDispatchMessages.clear();
    previousQtMessageHandler = qInstallMessageHandler(captureXpuDispatchMessage);
    struct RestoreMessageHandler final {
        ~RestoreMessageHandler()
        {
            qInstallMessageHandler(previousQtMessageHandler);
            previousQtMessageHandler = nullptr;
        }
    } restoreMessageHandler;

    {
        std::lock_guard<std::mutex> lock(xpuLogMutex);
        xpuDispatchMessages.clear();
    }
    const auto addLayer = [&](const ArtifactCore::CompositionID& compositionId,
                              const QString& name,
                              Artifact::LayerType type) {
        Artifact::ArtifactLayerInitParams params(name, type);
        return projects.addLayerToComposition(compositionId, params);
    };
    Artifact::ArtifactCompositionInitParams particleCompositionParams;
    particleCompositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("XpuMixedParticles")));
    particleCompositionParams.setResolution(width, height);
    particleCompositionParams.setFrameRate(30.0);
    particleCompositionParams.setDurationFrames(4);
    const auto particleComposition = projects.createComposition(
        particleCompositionParams);
    ASSERT_TRUE(particleComposition.success);

    const auto particleCameraLayer = addLayer(
        particleComposition.id, QStringLiteral("Camera"),
        Artifact::LayerType::Camera);
    ASSERT_TRUE(particleCameraLayer.success);
    auto* particleCamera = dynamic_cast<Artifact::ArtifactCameraLayer*>(
        particleCameraLayer.layer.get());
    ASSERT_NE(particleCamera, nullptr);
    particleCamera->setActiveCamera(true);
    particleCamera->setPosition3D(
        {width / 2.0f, height / 2.0f, 1920.0f});

    const auto configureVisibleParticleEmitter = [&](const QString& layerName,
                                                      Artifact::LayerType type,
                                                      const QVector3D& position) {
        const auto layerResult = addLayer(
            particleComposition.id, layerName, type);
        if (!layerResult.success) {
            failure = QStringLiteral("Could not add particle layer: %1")
                          .arg(layerName);
            return false;
        }
        auto* particleLayer = dynamic_cast<Artifact::ArtifactParticleLayer*>(
            layerResult.layer.get());
        if (!particleLayer || !particleLayer->setEmitterPosition(position) ||
            particleLayer->particleSystem()->emitters().empty()) {
            failure = QStringLiteral("Could not configure particle layer: %1")
                          .arg(layerName);
            return false;
        }
        auto& emitterParams =
            particleLayer->particleSystem()->emitters().front()->params();
        emitterParams.randomSeed = 1337u;
        emitterParams.rate = 1000.0f;
        emitterParams.lifeMin = 10.0f;
        emitterParams.lifeMax = 10.0f;
        emitterParams.speedMin = 0.0f;
        emitterParams.speedMax = 0.0f;
        emitterParams.directionSpread = 0.0f;
        emitterParams.colorStart = QColor(255, 32, 16, 255);
        emitterParams.colorMid = QColor(255, 32, 16, 255);
        emitterParams.colorEnd = QColor(255, 32, 16, 255);
        emitterParams.scaleMin = 10.0f;
        emitterParams.scaleMax = 10.0f;
        return true;
    };
    ASSERT_TRUE(configureVisibleParticleEmitter(
        QStringLiteral("Particles 2D"), Artifact::LayerType::Particle,
        QVector3D(width * 0.3f, height * 0.5f, 0.0f)))
        << failure.toStdString();
    ASSERT_TRUE(configureVisibleParticleEmitter(
        QStringLiteral("Particles 3D"), Artifact::LayerType::Particle3D,
        QVector3D(width * 0.7f, height * 0.5f, 0.0f)))
        << failure.toStdString();

    const QString particleCpuBaselinePath = isolatedOutputPath(
        QStringLiteral("render_queue_xpu_particles_cpu.png"));
    ASSERT_TRUE(addAndRender(
        queue, particleComposition.id, QStringLiteral("XpuCpuParticles"),
        particleCpuBaselinePath, width, height,
        [](const QImage&) { return true; },
        &failure, 2, QStringLiteral("cpu"), 1))
        << "CPU particle reference frame: " << failure.toStdString();
    const QFileInfo particleCpuInfo(particleCpuBaselinePath);
    const QImage particleCpuReference(QDir(particleCpuInfo.absolutePath())
        .filePath(particleCpuInfo.completeBaseName() +
                  QStringLiteral("_0001.png")));
    ASSERT_FALSE(particleCpuReference.isNull());

    {
        std::lock_guard<std::mutex> lock(xpuLogMutex);
        xpuDispatchMessages.clear();
    }
    const QString particleOutputPath = isolatedOutputPath(
        QStringLiteral("render_queue_xpu_particles.png"));
    ASSERT_TRUE(addAndRender(
        queue, particleComposition.id, QStringLiteral("XpuMixedParticles"),
        particleOutputPath, width, height,
        [](const QImage&) { return true; }, &failure, 3))
        << failure.toStdString();

    const QFileInfo particleOutputInfo(particleOutputPath);
    const QString particleFramePath = QDir(
        particleOutputInfo.absolutePath()).filePath(
            particleOutputInfo.completeBaseName() +
            QStringLiteral("_0001.png"));
    const QImage particleFrame(particleFramePath);
    ASSERT_FALSE(particleFrame.isNull()) << particleFramePath.toStdString();
    ASSERT_EQ(particleFrame.size(), particleCpuReference.size());
    int particleParityMismatchPixels = 0;
    int particleMismatchMinX = width;
    int particleMismatchMinY = height;
    int particleMismatchMaxX = -1;
    int particleMismatchMaxY = -1;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (particleFrame.pixel(x, y) !=
                particleCpuReference.pixel(x, y)) {
                ++particleParityMismatchPixels;
                particleMismatchMinX = std::min(particleMismatchMinX, x);
                particleMismatchMinY = std::min(particleMismatchMinY, y);
                particleMismatchMaxX = std::max(particleMismatchMaxX, x);
                particleMismatchMaxY = std::max(particleMismatchMaxY, y);
            }
        }
    }
    RecordProperty("xpu_particle_parity_mismatch_pixels",
                   QString::number(particleParityMismatchPixels).toStdString());
    RecordProperty("xpu_particle_parity_mismatch_bounds",
        QStringLiteral("[%1,%2..%3,%4]")
            .arg(particleMismatchMinX).arg(particleMismatchMinY)
            .arg(particleMismatchMaxX).arg(particleMismatchMaxY)
            .toStdString());

    QStringList particleDispatchMessages;
    {
        std::lock_guard<std::mutex> lock(xpuLogMutex);
        particleDispatchMessages = xpuDispatchMessages;
    }
    const QString particleDispatch = particleDispatchMessages.join(
        QLatin1Char('\n'));
    ASSERT_TRUE(particleDispatch.contains(
        QStringLiteral("XPU active plan")))
        << particleDispatch.toStdString();
    const auto particleCpuFramesMatch = QRegularExpression(
        QStringLiteral("xpuCpuFrames=\\s*(\\d+)")).match(particleDispatch);
    const auto particleGpuFramesMatch = QRegularExpression(
        QStringLiteral("gpuFrames=\\s*(\\d+)")).match(particleDispatch);
    ASSERT_TRUE(particleCpuFramesMatch.hasMatch())
        << particleDispatch.toStdString();
    ASSERT_TRUE(particleGpuFramesMatch.hasMatch())
        << particleDispatch.toStdString();
    EXPECT_EQ(particleCpuFramesMatch.captured(1).toInt() +
                  particleGpuFramesMatch.captured(1).toInt(),
              2)
        << particleDispatch.toStdString();
}

TEST(RenderQueueLayerImageIntegrationTest,
     XpuMixedRunsEveryAvailableFullGpuAdapterAlongsideCpu)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    struct RestoreEnvironment final {
        QByteArray xpu = qgetenv("ARTIFACT_XPU");
        QByteArray maxInFlight = qgetenv("ARTIFACT_XPU_MAX_IN_FLIGHT");
        QByteArray includeIntegrated = qgetenv(
            "ARTIFACT_XPU_INCLUDE_INTEGRATED");
        ~RestoreEnvironment()
        {
            const auto restore = [](const char* name,
                                    const QByteArray& value) {
                if (value.isNull()) {
                    qunsetenv(name);
                } else {
                    qputenv(name, value);
                }
            };
            restore("ARTIFACT_XPU", xpu);
            restore("ARTIFACT_XPU_MAX_IN_FLIGHT", maxInFlight);
            restore("ARTIFACT_XPU_INCLUDE_INTEGRATED", includeIntegrated);
        }
    } restoreEnvironment;
    // Exceed the eight-frame output-buffer limit so the consumer's next-frame
    // priority path is exercised while CPU and every eligible GPU lane render.
    qputenv("ARTIFACT_XPU", "mixed=on");
    qputenv("ARTIFACT_XPU_MAX_IN_FLIGHT", "16");
    qputenv("ARTIFACT_XPU_INCLUDE_INTEGRATED", "off");

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
    projects.createProject(QStringLiteral("RenderQueueXpuAllAdaptersTest"),
                           true);
    ASSERT_TRUE(projects.isProjectCreated());

    constexpr int width = 320;
    constexpr int height = 180;
    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("XpuAllAdapters")));
    compositionParams.setResolution(width, height);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(11);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);
    Artifact::ArtifactTextLayerInitParams textParams(QStringLiteral("Text"));
    const auto textResult = projects.addLayerToComposition(
        composition.id, textParams);
    ASSERT_TRUE(textResult.success);
    auto* text = dynamic_cast<Artifact::ArtifactTextLayer*>(
        textResult.layer.get());
    ASSERT_NE(text, nullptr);
    text->setText(ArtifactCore::UniString(QStringLiteral("XPU all adapters")));
    text->setFontSize(30.0f);
    text->setTextColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));

    {
        std::lock_guard<std::mutex> lock(xpuLogMutex);
        xpuDispatchMessages.clear();
    }
    previousQtMessageHandler = qInstallMessageHandler(
        captureXpuDispatchMessage);
    struct RestoreMessageHandler final {
        ~RestoreMessageHandler()
        {
            qInstallMessageHandler(previousQtMessageHandler);
            previousQtMessageHandler = nullptr;
        }
    } restoreMessageHandler;

    QString failure;
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("XpuAllAdapters"),
        isolatedOutputPath(QStringLiteral("render_queue_xpu_all_adapters.png")),
        width, height,
        [](const QImage& image) {
            return countPixelsMatching(image, [](QRgb pixel) {
                return qAlpha(pixel) > 32 && qRed(pixel) > 210 &&
                       qGreen(pixel) > 210 && qBlue(pixel) > 210;
            }) > 20;
        }, &failure, 11)) << failure.toStdString();

    QStringList messages;
    {
        std::lock_guard<std::mutex> lock(xpuLogMutex);
        messages = xpuDispatchMessages;
    }
    const QString joined = messages.join(QLatin1Char('\n'));
    ASSERT_TRUE(joined.contains(QStringLiteral("XPU mixed active")))
        << joined.toStdString();
    const auto cpuFrames = QRegularExpression(
        QStringLiteral("xpuCpuFrames=\\s*(\\d+)")).match(joined);
    const auto gpuFrames = QRegularExpression(
        QStringLiteral("gpuFrames=\\s*(\\d+)")).match(joined);
    const auto primaryGpuBackend = QRegularExpression(
        QStringLiteral("primaryGpuBackend=\\s*\"?([^\"\\s]*)"))
        .match(joined);
    const auto primaryGpuFrames = QRegularExpression(
        QStringLiteral("primaryGpuFrames=\\s*(\\d+)")).match(joined);
    const auto planMessage = std::find_if(
        messages.cbegin(), messages.cend(), [](const QString& message) {
            return message.contains(QStringLiteral("XPU active plan"));
        });
    ASSERT_TRUE(cpuFrames.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(gpuFrames.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(primaryGpuBackend.hasMatch()) << joined.toStdString();
    ASSERT_TRUE(primaryGpuFrames.hasMatch()) << joined.toStdString();
    ASSERT_NE(planMessage, messages.cend()) << joined.toStdString();
    if (gpuFrames.captured(1).toInt() == 0) {
        GTEST_SKIP() << "No usable full GPU adapter is available: "
                     << joined.toStdString();
    }
    EXPECT_GT(cpuFrames.captured(1).toInt(), 0) << joined.toStdString();
    EXPECT_GT(gpuFrames.captured(1).toInt(), 0) << joined.toStdString();
    EXPECT_EQ(cpuFrames.captured(1).toInt() + gpuFrames.captured(1).toInt(),
              10) << joined.toStdString();

    const auto cpuPlanNode = QRegularExpression(
        QStringLiteral("id=cpu:0 kind=cpu-group [^|]*inFlight=(\\d+)"))
        .match(*planMessage);
    ASSERT_TRUE(cpuPlanNode.hasMatch()) << planMessage->toStdString();
    EXPECT_GT(cpuPlanNode.captured(1).toInt(), 0)
        << planMessage->toStdString();

    const QRegularExpression gpuPlanNode(
        QStringLiteral("id=((?:gpu|igpu):([^:]+):(\\d+)) kind=gpu-full backend=([^ ]+) [^|]*inFlight=(\\d+)"));
    auto gpuPlanNodes = gpuPlanNode.globalMatch(*planMessage);
    int plannedGpuAdapters = 0;
    int activeGpuAdapters = 0;
    int plannedD3D12Adapters = 0;
    int activeD3D12Adapters = 0;
    QStringList gpuPlanNodeIds;
    QStringList activeGpuBackendAdapters;
    while (gpuPlanNodes.hasNext()) {
        const auto node = gpuPlanNodes.next();
        ++plannedGpuAdapters;
        EXPECT_FALSE(gpuPlanNodeIds.contains(node.captured(1)))
            << "Duplicate backend-qualified XPU node ID: "
            << node.captured(1).toStdString();
        gpuPlanNodeIds.push_back(node.captured(1));
        EXPECT_EQ(node.captured(2), node.captured(4))
            << "XPU node ID backend does not match its plan backend: "
            << node.captured(1).toStdString();
        EXPECT_EQ(node.captured(1).section(QLatin1Char(':'), -1),
                  node.captured(3))
            << "XPU node ID adapter does not match its adapter suffix: "
            << node.captured(1).toStdString();
        const bool active = node.captured(5).toInt() > 0;
        if (active) {
            ++activeGpuAdapters;
            activeGpuBackendAdapters.push_back(
                QStringLiteral("%1:%2")
                    .arg(node.captured(4), node.captured(3)));
        }
        if (node.captured(4).compare(QStringLiteral("d3d12"),
                                     Qt::CaseInsensitive) == 0) {
            ++plannedD3D12Adapters;
            if (active) {
                ++activeD3D12Adapters;
            }
        }
    }
    ASSERT_GT(plannedGpuAdapters, 0) << planMessage->toStdString();
    EXPECT_GT(activeGpuAdapters, 0)
        << "No full GPU adapter obtained a render lane: "
        << planMessage->toStdString();

    auto gpuWorkerLogs = QRegularExpression(
        QStringLiteral("(?:gpuWorkerFrames=\\s*\"?|;)backend=([^\\s\";]+)\\s+adapter=(\\d+)\\s+frames=(\\d+)"))
        .globalMatch(joined);
    int participatingGpuWorkers = 0;
    QStringList gpuWorkerBackendAdapters;
    while (gpuWorkerLogs.hasNext()) {
        const auto worker = gpuWorkerLogs.next();
        EXPECT_EQ(worker.captured(1), QLatin1String("d3d12"))
            << worker.captured(0).toStdString();
        EXPECT_GT(worker.captured(3).toInt(), 0) << worker.captured(0).toStdString();
        ++participatingGpuWorkers;
        const QString backendAdapter = QStringLiteral("%1:%2")
            .arg(worker.captured(1), worker.captured(2));
        gpuWorkerBackendAdapters.push_back(backendAdapter);
        EXPECT_TRUE(activeGpuBackendAdapters.contains(backendAdapter))
            << "GPU worker is absent from the active plan: "
            << backendAdapter.toStdString();
    }
    const auto primaryGpuAdapter = QRegularExpression(
        QStringLiteral("primaryGpuAdapterId=\\s*(-?\\d+)"))
        .match(joined);
    ASSERT_TRUE(primaryGpuAdapter.hasMatch()) << joined.toStdString();
    const QString primaryBackendAdapter = QStringLiteral("%1:%2")
        .arg(primaryGpuBackend.captured(1),
             primaryGpuAdapter.captured(1));
    const bool primaryGpuParticipated =
        primaryGpuFrames.captured(1).toInt() > 0;
    if (primaryGpuParticipated) {
        EXPECT_TRUE(activeGpuBackendAdapters.contains(primaryBackendAdapter))
            << "Primary GPU is absent from the active plan: "
            << primaryBackendAdapter.toStdString();
    }
    for (const QString& activeBackendAdapter : activeGpuBackendAdapters) {
        EXPECT_TRUE(gpuWorkerBackendAdapters.contains(activeBackendAdapter) ||
                    (primaryGpuParticipated &&
                     primaryBackendAdapter == activeBackendAdapter))
            << "Active GPU lane processed no frames: "
            << activeBackendAdapter.toStdString();
    }
    if (plannedGpuAdapters > 1) {
        const int activePrimaryLanes =
            primaryGpuParticipated ? 1 : 0;
        EXPECT_EQ(participatingGpuWorkers + activePrimaryLanes,
                  activeGpuAdapters)
            << joined.toStdString();
    }
    if (qEnvironmentVariable("ARTIFACT_RENDER_BACKEND") ==
        QStringLiteral("vulkan")) {
        const bool vulkanPrimary = planMessage->contains(
            QStringLiteral("kind=gpu-full backend=vulkan adapter="));
        if (!vulkanPrimary || plannedD3D12Adapters == 0) {
            GTEST_SKIP() << "Vulkan primary plus D3D12 adapter topology is "
                            "unavailable on this host: "
                         << planMessage->toStdString();
        }
        EXPECT_GT(activeD3D12Adapters, 0) << planMessage->toStdString();
        EXPECT_EQ(primaryGpuBackend.captured(1), QLatin1String("vulkan"))
            << "Vulkan primary backend was misreported: "
            << joined.toStdString();
        EXPECT_GT(primaryGpuFrames.captured(1).toInt(), 0)
            << "The Vulkan primary adapter did not process a frame: "
            << joined.toStdString();
    }
}
