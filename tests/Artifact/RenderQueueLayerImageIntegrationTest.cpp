#include <gtest/gtest.h>

#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QList>
#include <QRegularExpression>
#include <QRect>
#include <QSize>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryDir>
#include <QThread>
#include <QVariant>
#include <QVariantMap>
#include <QVector>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

import Artifact.Composition.InitParams;
import Artifact.Composition.Result;
import Artifact.Effect.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Camera;
import Artifact.Layer.InitParams;
import Artifact.Layer.Image;
import Artifact.Layer.Particle;
import Artifact.Layer.Result;
import Artifact.Layer.Text;
import Artifact.Generator.Particle;
import Artifact.Project.Manager;
import Artifact.Project;
import Artifact.Project.Items;
import Artifact.Render.Batch;
import Artifact.Render.IRenderer;
import Artifact.Render.Queue.Job;
import Artifact.Render.Queue.Service;
import Artifact.Service.Project;
import Frame.Position;
import Frame.Range;
import Color.Float;
import Image.OpenEXR;
import BrightnessEffect;
import Memory.SharedPtr;
import Time.Rational;
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
    const QString captureRoot = qEnvironmentVariable(
        "ARTIFACT_UI_TEST_CAPTURE_ROOT");
    const QString root = captureRoot.isEmpty()
        ? qEnvironmentVariable("ARTIFACT_UI_TEST_PROJECT_ROOT")
        : captureRoot;
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
                  int frameStart = 1,
                  const QVariantMap& selectiveOverrides = {})
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
    queue->setJobFramePaddingAt(jobIndex, 4);
    queue->setJobRenderBackendAt(jobIndex, renderBackend);
    queue->setJobFrameRangeAt(jobIndex, frameStart, frameEnd);
    auto selectiveSettings = queue->jobSelectiveSettingsAt(jobIndex);
    selectiveSettings.insert(
        QStringLiteral("frameRangeMode"),
        static_cast<int>(Artifact::ArtifactRenderJob::FrameRangeMode::Custom));
    for (auto it = selectiveOverrides.cbegin();
         it != selectiveOverrides.cend(); ++it) {
        selectiveSettings.insert(it.key(), it.value());
    }
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

bool hasBrightTextPixels(const QImage& image)
{
    return countPixelsMatching(image, [](QRgb pixel) {
        return qAlpha(pixel) > 32 && qRed(pixel) > 210 &&
               qGreen(pixel) > 210 && qBlue(pixel) > 210;
    }) > 20;
}

bool hasVisibleTextPixels(const QImage& image)
{
    return countPixelsMatching(image, [](QRgb pixel) {
        return qAlpha(pixel) > 32 &&
               std::max({qRed(pixel), qGreen(pixel), qBlue(pixel)}) > 32;
    }) > 20;
}

Artifact::FootageItem* findFootageItem(
    const QVector<Artifact::ProjectItem*>& items,
    const QString& path)
{
    for (Artifact::ProjectItem* item : items) {
        if (!item) {
            continue;
        }
        if (item->type() == Artifact::eProjectItemType::Footage) {
            auto* footage = static_cast<Artifact::FootageItem*>(item);
            if (QFileInfo(footage->filePath).absoluteFilePath() ==
                QFileInfo(path).absoluteFilePath()) {
                return footage;
            }
        }
        if (auto* nested = findFootageItem(item->children, path)) {
            return nested;
        }
    }
    return nullptr;
}

QRect visibleTextBounds(const QImage& source)
{
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    QRect bounds;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = image.pixel(x, y);
            if (qAlpha(pixel) > 32 &&
                std::max({qRed(pixel), qGreen(pixel), qBlue(pixel)}) > 32) {
                bounds |= QRect(x, y, 1, 1);
            }
        }
    }
    return bounds;
}

double bestVisiblePixelMatchFraction(const QImage& source,
                                     const QImage& target,
                                     const int minOffsetX,
                                     const int maxOffsetX,
                                     const int maxOffsetY)
{
    const QImage sourcePixels = source.convertToFormat(QImage::Format_ARGB32);
    const QImage targetPixels = target.convertToFormat(QImage::Format_ARGB32);
    int visibleSourcePixels = 0;
    for (int y = 0; y < sourcePixels.height(); ++y) {
        for (int x = 0; x < sourcePixels.width(); ++x) {
            const QRgb pixel = sourcePixels.pixel(x, y);
            if (qAlpha(pixel) > 32 &&
                std::max({qRed(pixel), qGreen(pixel), qBlue(pixel)}) > 32) {
                ++visibleSourcePixels;
            }
        }
    }
    if (visibleSourcePixels == 0) {
        return 0.0;
    }

    double bestFraction = 0.0;
    for (int offsetY = -maxOffsetY; offsetY <= maxOffsetY; ++offsetY) {
        for (int offsetX = minOffsetX; offsetX <= maxOffsetX; ++offsetX) {
            int matchedPixels = 0;
            for (int y = 0; y < sourcePixels.height(); ++y) {
                const int targetY = y + offsetY;
                if (targetY < 0 || targetY >= targetPixels.height()) {
                    continue;
                }
                for (int x = 0; x < sourcePixels.width(); ++x) {
                    const QRgb sourcePixel = sourcePixels.pixel(x, y);
                    if (qAlpha(sourcePixel) <= 32 ||
                        std::max({qRed(sourcePixel), qGreen(sourcePixel),
                                  qBlue(sourcePixel)}) <= 32) {
                        continue;
                    }
                    const int targetX = x + offsetX;
                    if (targetX < 0 || targetX >= targetPixels.width()) {
                        continue;
                    }
                    const QRgb targetPixel = targetPixels.pixel(targetX, targetY);
                    if (qAlpha(targetPixel) <= 32 ||
                        std::max({qRed(targetPixel), qGreen(targetPixel),
                                  qBlue(targetPixel)}) <= 32) {
                        continue;
                    }
                    const int colorDistance =
                        std::abs(qRed(sourcePixel) - qRed(targetPixel)) +
                        std::abs(qGreen(sourcePixel) - qGreen(targetPixel)) +
                        std::abs(qBlue(sourcePixel) - qBlue(targetPixel));
                    if (colorDistance <= 96) {
                        ++matchedPixels;
                    }
                }
            }
            bestFraction = std::max(
                bestFraction,
                static_cast<double>(matchedPixels) / visibleSourcePixels);
        }
    }
    return bestFraction;
}

bool addAndRenderTwoTextFrames(
    Artifact::ArtifactRenderQueueService* queue,
    const ArtifactCore::CompositionID& compositionId,
    const QString& compositionName,
    const QString& outputBasePath,
    int expectedWidth,
    int expectedHeight,
    QString* failure)
{
    if (!addAndRender(queue, compositionId, compositionName, outputBasePath,
                      expectedWidth, expectedHeight,
                      hasBrightTextPixels, failure, 3,
                      QStringLiteral("gpu"), 1)) {
        return false;
    }

    const QFileInfo outputInfo(outputBasePath);
    const QString secondFramePath = QDir(outputInfo.absolutePath()).filePath(
        QStringLiteral("%1_%2.png")
            .arg(outputInfo.completeBaseName())
            .arg(2, 4, 10, QLatin1Char('0')));
    const QImage secondFrame(secondFramePath);
    if (secondFrame.isNull()) {
        int jobStartFrame = -1;
        int jobEndFrame = -1;
        queue->jobFrameRangeAt(queue->jobCount() - 1,
                               &jobStartFrame, &jobEndFrame);
        const QStringList outputFiles = QDir(outputInfo.absolutePath())
            .entryList(QDir::Files, QDir::Name);
        *failure = QStringLiteral("Render Queue did not write a readable second text frame: %1")
                       .arg(secondFramePath) +
                   QStringLiteral(" (job range %1..%2, progress %3%%)")
                       .arg(jobStartFrame).arg(jobEndFrame)
                       .arg(queue->jobProgressAt(queue->jobCount() - 1)) +
                   QStringLiteral("; files: %1").arg(outputFiles.join(
                       QStringLiteral(", ")));
        return false;
    }
    if (secondFrame.size() != QSize(expectedWidth, expectedHeight)) {
        *failure = QStringLiteral("Second PNG dimensions were %1x%2, expected %3x%4")
                       .arg(secondFrame.width()).arg(secondFrame.height())
                       .arg(expectedWidth).arg(expectedHeight);
        return false;
    }
    if (!hasBrightTextPixels(secondFrame)) {
        *failure = QStringLiteral("Second text frame contains no expected bright glyph pixels: %1")
                       .arg(secondFramePath);
        return false;
    }
    const QString firstFramePath = QDir(outputInfo.absolutePath()).filePath(
        QStringLiteral("%1_%2.png")
            .arg(outputInfo.completeBaseName())
            .arg(1, 4, 10, QLatin1Char('0')));
    const QImage firstFrame(firstFramePath);
    if (firstFrame.isNull()) {
        *failure = QStringLiteral("Render Queue did not write a readable first text frame: %1")
                       .arg(firstFramePath);
        return false;
    }
    if (firstFrame == secondFrame) {
        *failure = QStringLiteral("Animated text frames rendered identical pixels: %1, %2")
                       .arg(firstFramePath, secondFramePath);
        return false;
    }
    return true;
}

bool addAndRenderExr(Artifact::ArtifactRenderQueueService* queue,
                     const ArtifactCore::CompositionID& compositionId,
                     const QString& compositionName,
                     const QString& outputBasePath,
                     int expectedWidth,
                     int expectedHeight,
                     QString* failure,
                     int frameStart = 1,
                     int frameEnd = 1)
{
    const QFileInfo requestedOutput(outputBasePath);
    if (!QDir().mkpath(requestedOutput.absolutePath())) {
        *failure = QStringLiteral("Could not create isolated EXR output directory");
        return false;
    }

    queue->addRenderQueueForComposition(compositionId, compositionName);
    const int jobIndex = queue->jobCount() - 1;
    if (jobIndex < 0) {
        *failure = QStringLiteral("Render Manager service did not enqueue the EXR composition");
        return false;
    }

    queue->setJobOutputPathAt(jobIndex, outputBasePath);
    queue->setJobOutputSettingsAt(jobIndex, QStringLiteral("EXR Sequence"),
                                  QStringLiteral("exr"), expectedWidth,
                                  expectedHeight, 30.0, 8000);
    queue->setJobBitDepthAt(jobIndex, 32);
    queue->setJobExrCompressionAt(jobIndex, QStringLiteral("zip"));
    queue->setJobRenderBackendAt(jobIndex, QStringLiteral("gpu"));
    queue->setJobFrameRangeAt(jobIndex, frameStart, frameEnd);
    auto selectiveSettings = queue->jobSelectiveSettingsAt(jobIndex);
    selectiveSettings.insert(
        QStringLiteral("frameRangeMode"),
        static_cast<int>(Artifact::ArtifactRenderJob::FrameRangeMode::Custom));
    if (!queue->setJobSelectiveSettingsAt(jobIndex, selectiveSettings)) {
        *failure = QStringLiteral("Could not select the explicit EXR frame range");
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
        *failure = QStringLiteral("EXR Render Queue job ended as '%1': %2")
                       .arg(status, queue->jobErrorMessageAt(jobIndex));
        return false;
    }

    const QFileInfo outputInfo(outputBasePath);
    const QString framePath = QDir(outputInfo.absolutePath()).filePath(
        QStringLiteral("%1_%2.exr")
            .arg(outputInfo.completeBaseName())
            .arg(frameStart, 4, 10, QLatin1Char('0')));
    ArtifactCore::OpenExr exr;
    std::vector<float> rgba;
    int width = 0;
    int height = 0;
    if (!exr.readRGBA32F(framePath, rgba, width, height)) {
        *failure = QStringLiteral("Render Queue did not write a readable EXR: %1")
                       .arg(framePath);
        return false;
    }
    if (width != expectedWidth || height != expectedHeight) {
        *failure = QStringLiteral("EXR dimensions were %1x%2, expected %3x%4")
                       .arg(width).arg(height).arg(expectedWidth).arg(expectedHeight);
        return false;
    }
    const std::size_t expectedComponents =
        static_cast<std::size_t>(expectedWidth) *
        static_cast<std::size_t>(expectedHeight) * 4u;
    if (rgba.size() != expectedComponents ||
        !std::all_of(rgba.begin(), rgba.end(), [](float value) {
            return std::isfinite(value);
        })) {
        *failure = QStringLiteral("EXR pixel data was incomplete or non-finite: %1")
                       .arg(framePath);
        return false;
    }
    bool containsRenderedColor = false;
    for (std::size_t component = 0; component < rgba.size(); component += 4u) {
        if (rgba[component] > 0.05f || rgba[component + 1u] > 0.05f ||
            rgba[component + 2u] > 0.05f) {
            containsRenderedColor = true;
            break;
        }
    }
    if (!containsRenderedColor) {
        *failure = QStringLiteral("EXR was readable but contained no visible pixel values: %1")
                       .arg(framePath);
        return false;
    }
    return true;
}

} // namespace

TEST(RenderQueueLayerImageIntegrationTest,
     WritesReadableExrThroughManagerQueue)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 96;
    constexpr int height = 54;
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
    projects.createProject(QStringLiteral("RenderQueueExrImageTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams params;
    params.setCompositionName(ArtifactCore::UniString(
        QStringLiteral("ExrQueueOutput")));
    params.setResolution(width, height);
    params.setFrameRate(30.0);
    params.setDurationFrames(1);
    params.setBackgroundColor(ArtifactCore::FloatColor(
        0.15f, 0.25f, 0.4f, 1.0f));
    const auto composition = projects.createComposition(params);
    ASSERT_TRUE(composition.success);

    QString failure;
    ASSERT_TRUE(addAndRenderExr(
        queue, composition.id, QStringLiteral("ExrQueueOutput"),
        isolatedOutputPath(QStringLiteral("render_queue_exr.exr")),
        width, height, &failure)) << failure.toStdString();
}

TEST(RenderQueueLayerImageIntegrationTest,
     WorkAreaRangeWritesOnlyFramesInsideTheCompositionWorkArea)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 32;
    constexpr int height = 24;
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
    projects.createProject(QStringLiteral("RenderQueueWorkAreaRangeTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams params;
    params.setCompositionName(ArtifactCore::UniString(
        QStringLiteral("WorkAreaRange")));
    params.setResolution(width, height);
    params.setFrameRate(30.0);
    params.setDurationFrames(8);
    params.setBackgroundColor(ArtifactCore::FloatColor(
        0.12f, 0.24f, 0.36f, 1.0f));
    const auto compositionResult = projects.createComposition(params);
    ASSERT_TRUE(compositionResult.success);

    const auto composition = projects.findComposition(compositionResult.id);
    ASSERT_TRUE(composition.success);
    auto compositionPtr = composition.ptr.lock();
    ASSERT_TRUE(compositionPtr);
    compositionPtr->setWorkAreaRange(ArtifactCore::FrameRange(
        ArtifactCore::FramePosition(2), ArtifactCore::FramePosition(5)));

    const QString outputBase = isolatedOutputPath(
        QStringLiteral("render_queue_work_area.png"));
    ASSERT_TRUE(QDir().mkpath(QFileInfo(outputBase).absolutePath()));
    queue->addRenderQueueForComposition(compositionResult.id,
                                       QStringLiteral("WorkAreaRange"));
    const int jobIndex = queue->jobCount() - 1;
    ASSERT_GE(jobIndex, 0);
    queue->setJobOutputPathAt(jobIndex, outputBase);
    queue->setJobOutputSettingsAt(jobIndex, QStringLiteral("PNG"),
                                  QStringLiteral("png"), width, height,
                                  30.0, 8000);
    queue->setJobFramePaddingAt(jobIndex, 4);
    auto settings = queue->jobSelectiveSettingsAt(jobIndex);
    settings.insert(QStringLiteral("frameRangeMode"),
                    static_cast<int>(Artifact::ArtifactRenderJob::FrameRangeMode::WorkArea));
    ASSERT_TRUE(queue->setJobSelectiveSettingsAt(jobIndex, settings));
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
    ASSERT_EQ(status, QStringLiteral("Completed"))
        << queue->jobErrorMessageAt(jobIndex).toStdString();

    const QFileInfo outputInfo(outputBase);
    const QDir outputDir(outputInfo.absolutePath());
    const QString outputStem = outputInfo.completeBaseName();
    for (int frame = 0; frame < 8; ++frame) {
        const QString framePath = outputDir.filePath(
            QStringLiteral("%1_%2.png").arg(outputStem).arg(frame, 4, 10,
                                                            QLatin1Char('0')));
        const bool shouldExist = frame >= 2 && frame < 5;
        EXPECT_EQ(QFileInfo::exists(framePath), shouldExist)
            << "Unexpected Work Area output membership for frame " << frame
            << ": " << framePath.toStdString();
        if (shouldExist) {
            const QImage image(framePath);
            ASSERT_FALSE(image.isNull()) << framePath.toStdString();
            EXPECT_EQ(image.size(), QSize(width, height));
        }
    }
}

TEST(RenderQueueLayerImageIntegrationTest,
     SelectedFrameRangesSurviveQueueJsonRoundTripAndRenderDisjointIntervals)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 32;
    constexpr int height = 24;
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
    projects.createProject(QStringLiteral("RenderQueueSelectedRangesTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams params;
    params.setCompositionName(ArtifactCore::UniString(
        QStringLiteral("SelectedRanges")));
    params.setResolution(width, height);
    params.setFrameRate(30.0);
    params.setDurationFrames(8);
    params.setBackgroundColor(ArtifactCore::FloatColor(
        0.12f, 0.24f, 0.36f, 1.0f));
    const auto composition = projects.createComposition(params);
    ASSERT_TRUE(composition.success);

    const QString outputBase = isolatedOutputPath(
        QStringLiteral("render_queue_selected_ranges.png"));
    ASSERT_TRUE(QDir().mkpath(QFileInfo(outputBase).absolutePath()));
    queue->addRenderQueueForComposition(composition.id,
                                       QStringLiteral("SelectedRanges"));
    ASSERT_EQ(queue->jobCount(), 1);
    queue->setJobOutputPathAt(0, outputBase);
    queue->setJobOutputSettingsAt(0, QStringLiteral("PNG"),
                                  QStringLiteral("png"), width, height,
                                  30.0, 8000);
    queue->setJobFramePaddingAt(0, 4);
    auto settings = queue->jobSelectiveSettingsAt(0);
    settings.insert(QStringLiteral("frameRangeMode"),
                    static_cast<int>(Artifact::ArtifactRenderJob::FrameRangeMode::SelectedFrames));
    QVariantList selectedRanges;
    selectedRanges.append(QVariantMap{
        {QStringLiteral("start"), 1}, {QStringLiteral("end"), 3}});
    selectedRanges.append(QVariantMap{
        {QStringLiteral("start"), 5}, {QStringLiteral("end"), 7}});
    settings.insert(QStringLiteral("selectedFrameRanges"), selectedRanges);
    ASSERT_TRUE(queue->setJobSelectiveSettingsAt(0, settings));

    const QString queueJsonPath = QDir(projectRoot.path()).filePath(
        QStringLiteral("selected-ranges-queue.json"));
    ASSERT_TRUE(queue->exportQueueJson(queueJsonPath));
    queue->removeAllRenderQueues();
    ASSERT_TRUE(queue->importQueueJson(queueJsonPath));
    ASSERT_EQ(queue->jobCount(), 1);
    const auto restoredSettings = queue->jobSelectiveSettingsAt(0);
    EXPECT_EQ(restoredSettings.value(QStringLiteral("frameRangeMode")).toInt(),
              static_cast<int>(Artifact::ArtifactRenderJob::FrameRangeMode::SelectedFrames));
    const auto restoredRanges = restoredSettings.value(
        QStringLiteral("selectedFrameRanges")).toList();
    ASSERT_EQ(restoredRanges.size(), 2);
    EXPECT_EQ(restoredRanges[0].toMap().value(QStringLiteral("start")).toInt(), 1);
    EXPECT_EQ(restoredRanges[0].toMap().value(QStringLiteral("end")).toInt(), 3);
    EXPECT_EQ(restoredRanges[1].toMap().value(QStringLiteral("start")).toInt(), 5);
    EXPECT_EQ(restoredRanges[1].toMap().value(QStringLiteral("end")).toInt(), 7);

    // Starting the full queue exercises the production expansion path that
    // turns each selected interval into its own output job.
    queue->startAllJobs();
    QElapsedTimer timeout;
    timeout.start();
    while (timeout.elapsed() < 60000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        bool allTerminal = queue->jobCount() == 2;
        bool allCompleted = allTerminal;
        for (int index = 0; index < queue->jobCount(); ++index) {
            const QString status = queue->jobStatusAt(index);
            const bool terminal = status == QStringLiteral("Completed") ||
                                  status == QStringLiteral("Failed") ||
                                  status == QStringLiteral("Canceled");
            allTerminal = allTerminal && terminal;
            allCompleted = allCompleted &&
                           status == QStringLiteral("Completed");
        }
        if (allTerminal) {
            ASSERT_TRUE(allCompleted);
            break;
        }
        QThread::msleep(10);
    }
    ASSERT_EQ(queue->jobCount(), 2)
        << "The two disjoint selected ranges should expand into two queue jobs";
    for (int index = 0; index < queue->jobCount(); ++index) {
        ASSERT_EQ(queue->jobStatusAt(index), QStringLiteral("Completed"))
            << queue->jobErrorMessageAt(index).toStdString();
    }

    const QFileInfo outputInfo(outputBase);
    const QDir outputDir(outputInfo.absolutePath());
    const QString stem = outputInfo.completeBaseName();
    const auto framePath = [&](const QString& interval, int frame) {
        return outputDir.filePath(QStringLiteral("%1_f%2_%3.png")
            .arg(stem, interval)
            .arg(frame, 4, 10, QLatin1Char('0')));
    };
    const QList<QPair<QString, int>> expectedFrames{
        {QStringLiteral("1-3"), 1}, {QStringLiteral("1-3"), 2},
        {QStringLiteral("5-7"), 5}, {QStringLiteral("5-7"), 6}};
    for (const auto& [interval, frame] : expectedFrames) {
        const QString path = framePath(interval, frame);
        ASSERT_TRUE(QFileInfo::exists(path)) << path.toStdString();
        const QImage image(path);
        ASSERT_FALSE(image.isNull()) << path.toStdString();
        EXPECT_EQ(image.size(), QSize(width, height));
    }
    for (int frame = 0; frame < 8; ++frame) {
        const bool selected = frame == 1 || frame == 2 || frame == 5 || frame == 6;
        const QString interval = frame < 4 ? QStringLiteral("1-3")
                                           : QStringLiteral("5-7");
        EXPECT_EQ(QFileInfo::exists(framePath(interval, frame)), selected)
            << "Unexpected output membership for frame " << frame;
    }
}

TEST(RenderQueueLayerImageIntegrationTest,
     SelectedFrameRangesWithoutAValidIntervalFailPreflight)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 32;
    constexpr int height = 24;
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
    projects.createProject(QStringLiteral("RenderQueueInvalidSelectedRangesTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams params;
    params.setCompositionName(ArtifactCore::UniString(
        QStringLiteral("InvalidSelectedRanges")));
    params.setResolution(width, height);
    params.setFrameRate(30.0);
    params.setDurationFrames(8);
    const auto composition = projects.createComposition(params);
    ASSERT_TRUE(composition.success);

    const QString outputBase = isolatedOutputPath(
        QStringLiteral("render_queue_invalid_selected_ranges.png"));
    ASSERT_TRUE(QDir().mkpath(QFileInfo(outputBase).absolutePath()));
    const QList<QVariantList> invalidRangeCases{
        {},
        {QVariantMap{{QStringLiteral("start"), 3},
                     {QStringLiteral("end"), 3}},
         QVariantMap{{QStringLiteral("start"), 6},
                     {QStringLiteral("end"), 2}}}
    };
    for (int caseIndex = 0; caseIndex < invalidRangeCases.size(); ++caseIndex) {
        queue->addRenderQueueForComposition(
            composition.id, QStringLiteral("InvalidSelectedRanges_%1")
                               .arg(caseIndex));
        const int jobIndex = queue->jobCount() - 1;
        ASSERT_GE(jobIndex, 0);
        const QString caseOutput = QFileInfo(outputBase).dir().filePath(
            QStringLiteral("case_%1.png").arg(caseIndex));
        queue->setJobOutputPathAt(jobIndex, caseOutput);
        queue->setJobOutputSettingsAt(jobIndex, QStringLiteral("PNG"),
                                      QStringLiteral("png"), width, height,
                                      30.0, 8000);
        auto settings = queue->jobSelectiveSettingsAt(jobIndex);
        settings.insert(QStringLiteral("frameRangeMode"),
                        static_cast<int>(Artifact::ArtifactRenderJob::FrameRangeMode::SelectedFrames));
        settings.insert(QStringLiteral("selectedFrameRanges"),
                        invalidRangeCases[caseIndex]);
        ASSERT_TRUE(queue->setJobSelectiveSettingsAt(jobIndex, settings));

        const auto preflight = queue->preflightRenderQueueAt(jobIndex);
        ASSERT_TRUE(preflight.hasErrors());
        const QString details =
            Artifact::ArtifactRenderQueueService::formatPreflightDetails(preflight)
                .join(QLatin1Char('\n'));
        EXPECT_TRUE(details.contains(
            QStringLiteral("Selected frame ranges are empty"))) << details.toStdString();
        EXPECT_EQ(queue->jobStatusAt(jobIndex), QStringLiteral("Pending"));
        EXPECT_FALSE(QFileInfo::exists(caseOutput));
    }
}

TEST(RenderQueueLayerImageIntegrationTest,
     UnorderedOverlappingSelectedRangesRenderTheirFullUnionOnce)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 32;
    constexpr int height = 24;
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
    projects.createProject(QStringLiteral("RenderQueueOverlappingRangesTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams params;
    params.setCompositionName(ArtifactCore::UniString(
        QStringLiteral("OverlappingSelectedRanges")));
    params.setResolution(width, height);
    params.setFrameRate(30.0);
    params.setDurationFrames(8);
    params.setBackgroundColor(ArtifactCore::FloatColor(
        0.12f, 0.24f, 0.36f, 1.0f));
    const auto composition = projects.createComposition(params);
    ASSERT_TRUE(composition.success);

    const QString outputBase = isolatedOutputPath(
        QStringLiteral("render_queue_overlapping_ranges.png"));
    ASSERT_TRUE(QDir().mkpath(QFileInfo(outputBase).absolutePath()));
    queue->addRenderQueueForComposition(composition.id,
                                       QStringLiteral("OverlappingRanges"));
    ASSERT_EQ(queue->jobCount(), 1);
    queue->setJobOutputPathAt(0, outputBase);
    queue->setJobOutputSettingsAt(0, QStringLiteral("PNG"),
                                  QStringLiteral("png"), width, height,
                                  30.0, 8000);
    queue->setJobFramePaddingAt(0, 4);
    auto settings = queue->jobSelectiveSettingsAt(0);
    settings.insert(QStringLiteral("frameRangeMode"),
                    static_cast<int>(Artifact::ArtifactRenderJob::FrameRangeMode::SelectedFrames));
    QVariantList selectedRanges;
    // Deliberately out of order and overlapping: their union is [1, 7).
    selectedRanges.append(QVariantMap{
        {QStringLiteral("start"), 5}, {QStringLiteral("end"), 7}});
    selectedRanges.append(QVariantMap{
        {QStringLiteral("start"), 1}, {QStringLiteral("end"), 4}});
    selectedRanges.append(QVariantMap{
        {QStringLiteral("start"), 3}, {QStringLiteral("end"), 6}});
    settings.insert(QStringLiteral("selectedFrameRanges"), selectedRanges);
    ASSERT_TRUE(queue->setJobSelectiveSettingsAt(0, settings));

    queue->startAllJobs();
    QElapsedTimer timeout;
    timeout.start();
    while (timeout.elapsed() < 60000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        const QString status = queue->jobStatusAt(0);
        if (status == QStringLiteral("Completed") ||
            status == QStringLiteral("Failed") ||
            status == QStringLiteral("Canceled")) {
            break;
        }
        QThread::msleep(10);
    }
    ASSERT_EQ(queue->jobCount(), 1)
        << "Overlapping intervals should normalize into one continuous job";
    ASSERT_EQ(queue->jobStatusAt(0), QStringLiteral("Completed"))
        << queue->jobErrorMessageAt(0).toStdString();

    const QFileInfo outputInfo(outputBase);
    const QDir outputDir(outputInfo.absolutePath());
    const QString stem = outputInfo.completeBaseName();
    for (int frame = 0; frame < 8; ++frame) {
        const QString path = outputDir.filePath(
            QStringLiteral("%1_f1-7_%2.png").arg(stem).arg(
                frame, 4, 10, QLatin1Char('0')));
        const bool expected = frame >= 1 && frame < 7;
        EXPECT_EQ(QFileInfo::exists(path), expected)
            << "Unexpected overlap-union output membership for frame " << frame
            << ": " << path.toStdString();
        if (expected && QFileInfo::exists(path)) {
            const QImage image(path);
            ASSERT_FALSE(image.isNull()) << path.toStdString();
            EXPECT_EQ(image.size(), QSize(width, height));
        }
    }
}

TEST(RenderQueueLayerImageIntegrationTest,
     BatchRendererAddsJobsForValidCompositionsAndReportsMissingIds)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    auto* queue = Artifact::ArtifactRenderQueueService::instance();
    auto* batch = Artifact::ArtifactBatchRenderer::instance();
    ASSERT_NE(queue, nullptr);
    ASSERT_NE(batch, nullptr);
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
    projects.createProject(QStringLiteral("BatchRendererQueueIntegrationTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    const auto createComposition = [&](const QString& name) {
        Artifact::ArtifactCompositionInitParams params;
        params.setCompositionName(ArtifactCore::UniString(name));
        params.setResolution(32, 24);
        params.setFrameRate(30.0);
        params.setDurationFrames(2);
        return projects.createComposition(params);
    };
    const auto first = createComposition(QStringLiteral("Batch One"));
    const auto second = createComposition(QStringLiteral("Batch:Two"));
    ASSERT_TRUE(first.success);
    ASSERT_TRUE(second.success);

    const QString outputDirectory = QDir(projectRoot.path()).filePath(
        QStringLiteral("batch-output"));
    const ArtifactCore::CompositionID missingId(
        QStringLiteral("00000000-0000-0000-0000-000000000001"));
    const QVector<ArtifactCore::CompositionID> ids{
        first.id, missingId, second.id};
    EXPECT_EQ(batch->addCompositions(ids, outputDirectory,
                                     QStringLiteral("%compName%")), 2);
    EXPECT_EQ(queue->jobCount(), 2);

    EXPECT_EQ(queue->jobCompositionIdAt(0), first.id);
    EXPECT_EQ(queue->jobCompositionIdAt(1), second.id);
    EXPECT_EQ(queue->jobCompositionNameAt(0), QStringLiteral("Batch One"));
    EXPECT_EQ(queue->jobCompositionNameAt(1), QStringLiteral("Batch:Two"));
    EXPECT_FALSE(batch->lastBatchErrors().isEmpty());
    EXPECT_TRUE(batch->lastBatchErrors().join(QLatin1Char('\n')).contains(
        QStringLiteral("Composition was not found")));

    for (int index = 0; index < queue->jobCount(); ++index) {
        const QString expectedStem =
            Artifact::ArtifactBatchRenderer::resolveFileNamePattern(
                QStringLiteral("%compName%"), queue->jobCompositionNameAt(index));
        const QFileInfo output(queue->jobOutputPathAt(index));
        EXPECT_EQ(output.absolutePath(), QFileInfo(outputDirectory).absoluteFilePath());
        EXPECT_EQ(output.completeBaseName(), expectedStem);
        EXPECT_FALSE(output.suffix().isEmpty());
        EXPECT_EQ(queue->jobStatusAt(index), QStringLiteral("Pending"));
    }
}

TEST(RenderQueueLayerImageIntegrationTest,
     BatchTemplateAppliesOverridesAndRejectsMissingPresetWithoutPartialJobs)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    auto* queue = Artifact::ArtifactRenderQueueService::instance();
    auto* batch = Artifact::ArtifactBatchRenderer::instance();
    ASSERT_NE(queue, nullptr);
    ASSERT_NE(batch, nullptr);
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
    projects.createProject(QStringLiteral("BatchTemplateQueueIntegrationTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    const auto createComposition = [&](const QString& name) {
        Artifact::ArtifactCompositionInitParams params;
        params.setCompositionName(ArtifactCore::UniString(name));
        params.setResolution(320, 180);
        params.setFrameRate(30.0);
        params.setDurationFrames(8);
        return projects.createComposition(params);
    };
    const auto first = createComposition(QStringLiteral("Template One"));
    const auto second = createComposition(QStringLiteral("Template Two"));
    ASSERT_TRUE(first.success);
    ASSERT_TRUE(second.success);

    Artifact::BatchTemplate tmpl;
    tmpl.name = QStringLiteral("Integration Template");
    tmpl.outputDirectory = QDir(projectRoot.path()).filePath(
        QStringLiteral("template-output"));
    tmpl.fileNamePattern = QStringLiteral("%compName%");
    tmpl.presetId = QStringLiteral("png_sequence");
    tmpl.overrideWidth = 64;
    tmpl.overrideHeight = 48;
    tmpl.overrideFps = 24.0;
    tmpl.startFrame = 2;
    tmpl.endFrame = 5;
    tmpl.format = QStringLiteral("PNG");
    tmpl.codec = QStringLiteral("PNG");
    tmpl.framePadding = 6;
    const QVector<ArtifactCore::CompositionID> ids{first.id, second.id};
    ASSERT_TRUE(Artifact::ArtifactBatchRenderer::validateTemplate(tmpl));
    ASSERT_EQ(batch->addCompositionsWithTemplate(ids, tmpl), 2);
    ASSERT_EQ(queue->jobCount(), 2);

    for (int index = 0; index < queue->jobCount(); ++index) {
        EXPECT_EQ(queue->jobCompositionIdAt(index), ids[index]);
        EXPECT_EQ(queue->jobFramePaddingAt(index), 6);
        int startFrame = -1;
        int endFrame = -1;
        ASSERT_TRUE(queue->jobFrameRangeAt(index, &startFrame, &endFrame));
        EXPECT_EQ(startFrame, 2);
        EXPECT_EQ(endFrame, 5);
        const auto settings = queue->jobSelectiveSettingsAt(index);
        EXPECT_EQ(settings.value(QStringLiteral("frameRangeMode")).toInt(),
                  static_cast<int>(Artifact::ArtifactRenderJob::FrameRangeMode::Custom));

        QString format;
        QString codec;
        QString profile;
        int width = 0;
        int height = 0;
        double fps = 0.0;
        int bitrate = 0;
        ASSERT_TRUE(queue->jobOutputSettingsAt(index, &format, &codec, &profile,
                                               &width, &height, &fps, &bitrate));
        EXPECT_EQ(format, QStringLiteral("PNG"));
        EXPECT_EQ(codec, QStringLiteral("PNG"));
        EXPECT_EQ(width, 64);
        EXPECT_EQ(height, 48);
        EXPECT_DOUBLE_EQ(fps, 24.0);
        EXPECT_EQ(QFileInfo(queue->jobOutputPathAt(index)).suffix(),
                  QStringLiteral("png"));
    }

    queue->removeAllRenderQueues();
    batch->clearBatchErrors();
    tmpl.presetId = QStringLiteral("missing-preset-for-integration-test");
    EXPECT_EQ(batch->addCompositionsWithTemplate(ids, tmpl), 0);
    EXPECT_EQ(queue->jobCount(), 0);
    EXPECT_TRUE(batch->lastBatchErrors().join(QLatin1Char('\n')).contains(
        QStringLiteral("Render preset is unavailable")));
}

TEST(RenderQueueLayerImageIntegrationTest,
     GrayscaleImageSurvivesProjectReloadAndRendersAsOpaqueNeutralRgb)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 64;
    constexpr int height = 64;
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
    projects.createProject(QStringLiteral("GrayscaleImageRenderTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    const QString sourcePath = QDir(projectRoot.path()).filePath(
        QStringLiteral("neutral-gray.png"));
    QImage source(width, height, QImage::Format_Grayscale8);
    source.fill(96);
    ASSERT_TRUE(source.save(sourcePath));

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(ArtifactCore::UniString(
        QStringLiteral("GrayscaleImage")));
    compositionParams.setResolution(width, height);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(2);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactImageInitParams imageParams(QStringLiteral("GraySource"));
    imageParams.setImagePath(sourcePath);
    const auto layerResult = projects.addLayerToComposition(
        composition.id, imageParams);
    ASSERT_TRUE(layerResult.success);
    ASSERT_NE(dynamic_cast<Artifact::ArtifactImageLayer*>(
                  layerResult.layer.get()), nullptr);

    const QString projectPath = QDir(projectRoot.path()).filePath(
        QStringLiteral("grayscale-image.artifact"));
    ASSERT_TRUE(projects.saveToFile(projectPath).success);
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(projectPath));
    const auto loadedComposition = projects.findComposition(composition.id);
    ASSERT_TRUE(loadedComposition.success);
    const auto loadedCompositionPtr = loadedComposition.ptr.lock();
    ASSERT_TRUE(loadedCompositionPtr);
    const auto loadedLayers = loadedCompositionPtr->allLayer();
    ASSERT_EQ(loadedLayers.size(), 1);
    auto* loadedImage = dynamic_cast<Artifact::ArtifactImageLayer*>(
        loadedLayers.front().get());
    ASSERT_NE(loadedImage, nullptr);
    EXPECT_EQ(loadedImage->sourcePath(), sourcePath);

    const QString outputBase = isolatedOutputPath(
        QStringLiteral("render_queue_grayscale.png"));
    QString failure;
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("GrayscaleImage"),
        outputBase, width, height,
        [](const QImage& image) { return !image.isNull(); },
        &failure)) << failure.toStdString();

    const QFileInfo outputInfo(outputBase);
    const QString framePath = QDir(outputInfo.absolutePath()).filePath(
        QStringLiteral("%1_0001.png").arg(outputInfo.completeBaseName()));
    const QImage rendered(framePath);
    ASSERT_FALSE(rendered.isNull()) << framePath.toStdString();
    const QColor center = rendered.pixelColor(width / 2, height / 2);
    EXPECT_LE(std::abs(center.red() - center.green()), 2);
    EXPECT_LE(std::abs(center.green() - center.blue()), 2);
    EXPECT_GE(center.alpha(), 250);
}

TEST(RenderQueueLayerImageIntegrationTest,
     RgbaImageAlphaSurvivesProjectReloadAndScaledCompositeHasNoHiddenRgbFringe)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 64;
    constexpr int height = 48;
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
    projects.createProject(QStringLiteral("RgbaImageAlphaRenderTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    const QString sourcePath = QDir(projectRoot.path()).filePath(
        QStringLiteral("rgba-alpha.png"));
    QImage source(width, height, QImage::Format_RGBA8888);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            source.setPixelColor(x, y, x < width / 2
                ? QColor(255, 0, 0, 0)
                : QColor(20, 40, 235, 255));
        }
    }
    ASSERT_TRUE(source.save(sourcePath, "PNG"));

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(ArtifactCore::UniString(
        QStringLiteral("RgbaAlpha")));
    compositionParams.setResolution(width, height);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(2);
    compositionParams.setBackgroundColor(ArtifactCore::FloatColor(
        0.0f, 0.5f, 0.0f, 1.0f));
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);
    Artifact::ArtifactImageInitParams imageParams(QStringLiteral("RgbaSource"));
    imageParams.setImagePath(sourcePath);
    const auto layerResult = projects.addLayerToComposition(
        composition.id, imageParams);
    ASSERT_TRUE(layerResult.success);
    auto* imageLayer = dynamic_cast<Artifact::ArtifactImageLayer*>(
        layerResult.layer.get());
    ASSERT_NE(imageLayer, nullptr);
    imageLayer->transform2D().setScale(1.5f, 1.5f);

    const QString projectPath = QDir(projectRoot.path()).filePath(
        QStringLiteral("rgba-alpha.artifact"));
    ASSERT_TRUE(projects.saveToFile(projectPath).success);
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(projectPath));
    const auto loadedComposition = projects.findComposition(composition.id);
    ASSERT_TRUE(loadedComposition.success);
    const auto loadedCompositionPtr = loadedComposition.ptr.lock();
    ASSERT_TRUE(loadedCompositionPtr);
    const auto loadedLayers = loadedCompositionPtr->allLayer();
    ASSERT_EQ(loadedLayers.size(), 1);
    auto* loadedImage = dynamic_cast<Artifact::ArtifactImageLayer*>(
        loadedLayers.front().get());
    ASSERT_NE(loadedImage, nullptr);
    EXPECT_EQ(loadedImage->sourcePath(), sourcePath);
    float scaleX = 0.0f;
    float scaleY = 0.0f;
    loadedImage->transform2D().scale(scaleX, scaleY);
    EXPECT_FLOAT_EQ(scaleX, 1.5f);
    EXPECT_FLOAT_EQ(scaleY, 1.5f);

    const QString outputBase = isolatedOutputPath(
        QStringLiteral("render_queue_rgba_alpha.png"));
    QString failure;
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("RgbaAlpha"),
        outputBase, width, height,
        [](const QImage& image) { return !image.isNull(); },
        &failure)) << failure.toStdString();
    const QString framePath = QDir(QFileInfo(outputBase).absolutePath()).filePath(
        QStringLiteral("%1_0001.png").arg(QFileInfo(outputBase).completeBaseName()));
    const QImage rendered(framePath);
    ASSERT_FALSE(rendered.isNull()) << framePath.toStdString();
    const auto backgroundPixels = countPixelsMatching(rendered, [](QRgb pixel) {
        return qRed(pixel) < 40 && qGreen(pixel) > 100 && qBlue(pixel) < 40;
    });
    const auto opaqueBluePixels = countPixelsMatching(rendered, [](QRgb pixel) {
        return qRed(pixel) < 60 && qGreen(pixel) < 80 && qBlue(pixel) > 150 &&
               qAlpha(pixel) > 250;
    });
    const auto redFringePixels = countPixelsMatching(rendered, [](QRgb pixel) {
        return qRed(pixel) > qBlue(pixel) + 20 &&
               qRed(pixel) > qGreen(pixel) + 20;
    });
    EXPECT_GT(backgroundPixels, 100)
        << "Transparent source pixels should reveal the green composition background";
    EXPECT_GT(opaqueBluePixels, 100)
        << "The opaque half of the RGBA source should remain visible after scaling";
    EXPECT_EQ(redFringePixels, 0)
        << "RGB values hidden under zero alpha must not create red fringes";
}

TEST(RenderQueueLayerImageIntegrationTest,
     HdrExrImageRetainsValuesAboveOneThroughProjectReloadAndRenderQueue)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 16;
    constexpr int height = 16;
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
    projects.createProject(QStringLiteral("HdrExrImageRenderTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    std::vector<float> sourcePixels(
        static_cast<std::size_t>(width) * height * 4u);
    for (std::size_t offset = 0; offset < sourcePixels.size(); offset += 4u) {
        sourcePixels[offset] = 2.0f;
        sourcePixels[offset + 1u] = 0.5f;
        sourcePixels[offset + 2u] = 0.25f;
        sourcePixels[offset + 3u] = 1.0f;
    }
    const QString sourcePath = QDir(projectRoot.path()).filePath(
        QStringLiteral("hdr-source.exr"));
    ArtifactCore::OpenExr exr;
    ASSERT_TRUE(exr.writeRGBA32F(sourcePath, sourcePixels.data(), width, height));

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(ArtifactCore::UniString(
        QStringLiteral("HdrSource")));
    compositionParams.setResolution(width, height);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(2);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);
    Artifact::ArtifactImageInitParams imageParams(QStringLiteral("HdrImage"));
    imageParams.setImagePath(sourcePath);
    const auto layerResult = projects.addLayerToComposition(
        composition.id, imageParams);
    ASSERT_TRUE(layerResult.success);

    const QString projectPath = QDir(projectRoot.path()).filePath(
        QStringLiteral("hdr-source.artifact"));
    ASSERT_TRUE(projects.saveToFile(projectPath).success);
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(projectPath));
    const auto loadedComposition = projects.findComposition(composition.id);
    ASSERT_TRUE(loadedComposition.success);
    const auto loadedCompositionPtr = loadedComposition.ptr.lock();
    ASSERT_TRUE(loadedCompositionPtr);
    const auto loadedLayers = loadedCompositionPtr->allLayer();
    ASSERT_EQ(loadedLayers.size(), 1);
    auto* loadedImage = dynamic_cast<Artifact::ArtifactImageLayer*>(
        loadedLayers.front().get());
    ASSERT_NE(loadedImage, nullptr);
    EXPECT_EQ(loadedImage->sourcePath(), sourcePath);

    QString failure;
    const QString outputBase = isolatedOutputPath(
        QStringLiteral("render_queue_hdr_input.exr"));
    ASSERT_TRUE(addAndRenderExr(
        queue, composition.id, QStringLiteral("HdrSource"), outputBase,
        width, height, &failure)) << failure.toStdString();

    const QFileInfo outputInfo(outputBase);
    const QString outputFrame = QDir(outputInfo.absolutePath()).filePath(
        QStringLiteral("%1_0001.exr").arg(outputInfo.completeBaseName()));
    std::vector<float> renderedPixels;
    int renderedWidth = 0;
    int renderedHeight = 0;
    ASSERT_TRUE(exr.readRGBA32F(outputFrame, renderedPixels,
                                renderedWidth, renderedHeight));
    ASSERT_EQ(renderedWidth, width);
    ASSERT_EQ(renderedHeight, height);
    ASSERT_EQ(renderedPixels.size(),
              static_cast<std::size_t>(width) * height * 4u);
    ASSERT_EQ(renderedPixels.size(), sourcePixels.size());
    const std::size_t centerOffset =
        (static_cast<std::size_t>(height / 2) * width + width / 2) * 4u;
    EXPECT_GT(renderedPixels[centerOffset], 1.0f)
        << "HDR red values above one were clipped before EXR output";
    EXPECT_GT(renderedPixels[centerOffset + 1u], 0.25f);
    EXPECT_GT(renderedPixels[centerOffset + 3u], 0.9f);
}

TEST(RenderQueueLayerImageIntegrationTest,
     SixteenBitGrayscaleKeepsSubEightBitDifferencesInQueueExr)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 32;
    constexpr int height = 16;
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
    projects.createProject(QStringLiteral("Gray16QueueIntegrationTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    const QString sourcePath = QDir(projectRoot.path()).filePath(
        QStringLiteral("gray16.png"));
    QImage source(width, height, QImage::Format_Grayscale16);
    for (int y = 0; y < height; ++y) {
        auto* samples = reinterpret_cast<std::uint16_t*>(source.scanLine(y));
        for (int x = 0; x < width; ++x) {
            // Both values round to the same 8-bit grayscale code, but differ
            // enough to survive the renderer's float working buffer.
            samples[x] = x < width / 2 ? 0x2000u : 0x207Fu;
        }
    }
    ASSERT_TRUE(source.save(sourcePath, "PNG"));

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(ArtifactCore::UniString(
        QStringLiteral("Gray16Source")));
    compositionParams.setResolution(width, height);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(2);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);
    Artifact::ArtifactImageInitParams imageParams(QStringLiteral("Gray16Image"));
    imageParams.setImagePath(sourcePath);
    ASSERT_TRUE(projects.addLayerToComposition(
        composition.id, imageParams).success);

    const QString projectPath = QDir(projectRoot.path()).filePath(
        QStringLiteral("gray16.artifact"));
    ASSERT_TRUE(projects.saveToFile(projectPath).success);
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(projectPath));
    const auto loadedComposition = projects.findComposition(composition.id);
    ASSERT_TRUE(loadedComposition.success);
    const auto loadedCompositionPtr = loadedComposition.ptr.lock();
    ASSERT_TRUE(loadedCompositionPtr);
    const auto loadedLayers = loadedCompositionPtr->allLayer();
    ASSERT_EQ(loadedLayers.size(), 1);
    auto* loadedImage = dynamic_cast<Artifact::ArtifactImageLayer*>(
        loadedLayers.front().get());
    ASSERT_NE(loadedImage, nullptr);
    EXPECT_EQ(loadedImage->sourcePath(), sourcePath);

    QString failure;
    const QString outputBase = isolatedOutputPath(
        QStringLiteral("render_queue_gray16.exr"));
    ASSERT_TRUE(addAndRenderExr(
        queue, composition.id, QStringLiteral("Gray16Source"), outputBase,
        width, height, &failure)) << failure.toStdString();
    const QFileInfo outputInfo(outputBase);
    const QString outputFrame = QDir(outputInfo.absolutePath()).filePath(
        QStringLiteral("%1_0001.exr").arg(outputInfo.completeBaseName()));
    ArtifactCore::OpenExr exr;
    std::vector<float> renderedPixels;
    int renderedWidth = 0;
    int renderedHeight = 0;
    ASSERT_TRUE(exr.readRGBA32F(outputFrame, renderedPixels,
                                renderedWidth, renderedHeight));
    ASSERT_EQ(renderedWidth, width);
    ASSERT_EQ(renderedHeight, height);
    ASSERT_EQ(renderedPixels.size(),
              static_cast<std::size_t>(width) * height * 4u);
    const std::size_t leftOffset =
        (static_cast<std::size_t>(height / 2) * width + width / 4) * 4u;
    const std::size_t rightOffset =
        (static_cast<std::size_t>(height / 2) * width + width * 3 / 4) * 4u;
    EXPECT_GT(renderedPixels[rightOffset], renderedPixels[leftOffset] + 1e-5f)
        << "16-bit input values that share an 8-bit code must remain distinct";
    EXPECT_NEAR(renderedPixels[leftOffset], renderedPixels[leftOffset + 1u], 1e-5f);
    EXPECT_NEAR(renderedPixels[rightOffset], renderedPixels[rightOffset + 1u], 1e-5f);
}

TEST(RenderQueueLayerImageIntegrationTest,
     GrayAlphaPngSurvivesProjectReloadAndCompositesByChannelAlpha)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 2;
    constexpr int height = 1;
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
    projects.createProject(QStringLiteral("GrayAlphaQueueIntegrationTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    // PNG color type 4: gray+alpha samples (80,64) and (200,192).
    const QByteArray pngBytes = QByteArray::fromBase64(QByteArrayLiteral(
        "iVBORw0KGgoAAAANSUhEUgAAAAIAAAABCAQAAABeK7cBAAAADUlEQVR4nGMIcDhxAAAEVQIZTuh/jwAAAABJRU5ErkJggg=="));
    ASSERT_EQ(pngBytes.size(), 70);
    const QString sourcePath = QDir(projectRoot.path()).filePath(
        QStringLiteral("gray-alpha.png"));
    QFile sourceFile(sourcePath);
    ASSERT_TRUE(sourceFile.open(QIODevice::WriteOnly));
    ASSERT_EQ(sourceFile.write(pngBytes), pngBytes.size());
    sourceFile.close();

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(ArtifactCore::UniString(
        QStringLiteral("GrayAlpha")));
    compositionParams.setResolution(width, height);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(2);
    compositionParams.setBackgroundColor(ArtifactCore::FloatColor(
        0.0f, 0.0f, 1.0f, 1.0f));
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);
    Artifact::ArtifactImageInitParams imageParams(QStringLiteral("GrayAlphaSource"));
    imageParams.setImagePath(sourcePath);
    ASSERT_TRUE(projects.addLayerToComposition(
        composition.id, imageParams).success);

    const QString projectPath = QDir(projectRoot.path()).filePath(
        QStringLiteral("gray-alpha.artifact"));
    ASSERT_TRUE(projects.saveToFile(projectPath).success);
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(projectPath));

    const QString outputBase = isolatedOutputPath(
        QStringLiteral("render_queue_gray_alpha.png"));
    QString failure;
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("GrayAlpha"), outputBase,
        width, height, [](const QImage& image) { return !image.isNull(); },
        &failure)) << failure.toStdString();
    const QString outputFrame = QDir(QFileInfo(outputBase).absolutePath()).filePath(
        QStringLiteral("%1_0001.png").arg(QFileInfo(outputBase).completeBaseName()));
    const QImage rendered(outputFrame);
    ASSERT_FALSE(rendered.isNull()) << outputFrame.toStdString();
    ASSERT_EQ(rendered.size(), QSize(width, height));
    const QColor lowAlpha = rendered.pixelColor(0, 0);
    const QColor highAlpha = rendered.pixelColor(1, 0);
    EXPECT_GT(lowAlpha.blue(), lowAlpha.red() + 50)
        << "Low-alpha gray should reveal the blue composition background";
    EXPECT_NEAR(lowAlpha.red(), lowAlpha.green(), 4);
    EXPECT_GT(highAlpha.red(), highAlpha.blue() + 20)
        << "High-alpha gray should dominate over the blue background";
    EXPECT_NEAR(highAlpha.red(), highAlpha.green(), 4);
    EXPECT_GE(lowAlpha.alpha(), 250);
    EXPECT_GE(highAlpha.alpha(), 250);
}

TEST(RenderQueueLayerImageIntegrationTest,
     CompletedQueueJobPersistsInHistoryAndCanBeRequeued)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 32;
    constexpr int height = 24;
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
            queue->clearCompletedJobHistory();
            projects.closeCurrentProject();
        }
    } cleanup{projects, queue};

    projects.closeCurrentProject();
    projects.createProject(QStringLiteral("CompletedQueueHistoryTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());
    Artifact::ArtifactCompositionInitParams params;
    params.setCompositionName(ArtifactCore::UniString(
        QStringLiteral("HistoryComposition")));
    params.setResolution(width, height);
    params.setFrameRate(30.0);
    params.setDurationFrames(2);
    params.setBackgroundColor(ArtifactCore::FloatColor(
        0.2f, 0.1f, 0.05f, 1.0f));
    const auto composition = projects.createComposition(params);
    ASSERT_TRUE(composition.success);

    const QString outputBase = isolatedOutputPath(
        QStringLiteral("render_queue_history.png"));
    QString failure;
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("HistoryComposition"),
        outputBase, width, height,
        [](const QImage& image) { return !image.isNull(); },
        &failure)) << failure.toStdString();
    ASSERT_EQ(queue->completedJobHistory().size(), 1)
        << "A successfully completed queue job must be added to history";
    EXPECT_EQ(queue->completedJobHistory().at(0).toObject()
                  .value(QStringLiteral("outputPath")).toString(),
              outputBase);

    const QString queueJsonPath = QDir(projectRoot.path()).filePath(
        QStringLiteral("completed-queue.json"));
    ASSERT_TRUE(queue->exportQueueJson(queueJsonPath));
    queue->removeAllRenderQueues();
    queue->clearCompletedJobHistory();
    ASSERT_TRUE(queue->importQueueJson(queueJsonPath));
    ASSERT_EQ(queue->completedJobHistory().size(), 1);
    ASSERT_TRUE(queue->requeueCompletedHistoryAt(0));
    ASSERT_EQ(queue->jobCount(), 2);
    EXPECT_EQ(queue->jobStatusAt(1), QStringLiteral("Pending"));

    const QFileInfo outputInfo(outputBase);
    const QString outputFrame = QDir(outputInfo.absolutePath()).filePath(
        QStringLiteral("%1_0001.png").arg(outputInfo.completeBaseName()));
    ASSERT_TRUE(QFileInfo::exists(outputFrame));
    ASSERT_TRUE(QFile::remove(outputFrame));
    queue->startAllJobs();
    QElapsedTimer timeout;
    timeout.start();
    QString status;
    while (timeout.elapsed() < 60000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        status = queue->jobStatusAt(1);
        if (status == QStringLiteral("Completed") ||
            status == QStringLiteral("Failed") ||
            status == QStringLiteral("Canceled")) {
            break;
        }
        QThread::msleep(10);
    }
    ASSERT_EQ(status, QStringLiteral("Completed"))
        << queue->jobErrorMessageAt(1).toStdString();
    EXPECT_TRUE(QFileInfo::exists(outputFrame))
        << "Requeued history job did not recreate its render output";
}

TEST(RenderQueueLayerImageIntegrationTest,
     BrightnessEffectSurvivesProjectReloadAndChangesQueuePixels)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 48;
    constexpr int height = 32;
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
    projects.createProject(QStringLiteral("ImageEffectQueueIntegrationTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    const QString sourcePath = QDir(projectRoot.path()).filePath(
        QStringLiteral("effect-source.png"));
    QImage source(width, height, QImage::Format_RGBA8888);
    source.fill(QColor(64, 64, 64, 255));
    ASSERT_TRUE(source.save(sourcePath, "PNG"));

    const auto createComposition = [&](const QString& name) {
        Artifact::ArtifactCompositionInitParams params;
        params.setCompositionName(ArtifactCore::UniString(name));
        params.setResolution(width, height);
        params.setFrameRate(30.0);
        params.setDurationFrames(2);
        return projects.createComposition(params);
    };
    const auto baseline = createComposition(QStringLiteral("EffectBaseline"));
    const auto processed = createComposition(QStringLiteral("EffectProcessed"));
    ASSERT_TRUE(baseline.success);
    ASSERT_TRUE(processed.success);

    const auto addImage = [&](const ArtifactCore::CompositionID& compositionId,
                              const QString& name) {
        Artifact::ArtifactImageInitParams params(name);
        params.setImagePath(sourcePath);
        return projects.addLayerToComposition(compositionId, params);
    };
    const auto baselineLayer = addImage(baseline.id,
                                        QStringLiteral("BaselineImage"));
    const auto processedLayer = addImage(processed.id,
                                         QStringLiteral("ProcessedImage"));
    ASSERT_TRUE(baselineLayer.success);
    ASSERT_TRUE(processedLayer.success);
    auto* processedImage = dynamic_cast<Artifact::ArtifactImageLayer*>(
        processedLayer.layer.get());
    ASSERT_NE(processedImage, nullptr);
    auto brightness = ArtifactCore::makeShared<Artifact::BrightnessEffect>();
    brightness->setBrightness(0.2f);
    processedImage->addEffect(brightness);
    ASSERT_EQ(processedImage->effectCount(), 1);

    const QString projectPath = QDir(projectRoot.path()).filePath(
        QStringLiteral("image-effect.artifact"));
    ASSERT_TRUE(projects.saveToFile(projectPath).success);
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(projectPath));
    const auto loadedProcessed = projects.findComposition(processed.id);
    ASSERT_TRUE(loadedProcessed.success);
    const auto loadedProcessedPtr = loadedProcessed.ptr.lock();
    ASSERT_TRUE(loadedProcessedPtr);
    const auto loadedLayers = loadedProcessedPtr->allLayer();
    ASSERT_EQ(loadedLayers.size(), 1);
    auto* loadedImage = dynamic_cast<Artifact::ArtifactImageLayer*>(
        loadedLayers.front().get());
    ASSERT_NE(loadedImage, nullptr);
    ASSERT_EQ(loadedImage->effectCount(), 1);

    const QString baselineOutput = isolatedOutputPath(
        QStringLiteral("render_queue_effect_baseline.png"));
    const QString processedOutput = isolatedOutputPath(
        QStringLiteral("render_queue_effect_processed.png"));
    QString failure;
    ASSERT_TRUE(addAndRender(
        queue, baseline.id, QStringLiteral("EffectBaseline"),
        baselineOutput, width, height,
        [](const QImage& image) { return !image.isNull(); },
        &failure)) << failure.toStdString();
    ASSERT_TRUE(addAndRender(
        queue, processed.id, QStringLiteral("EffectProcessed"),
        processedOutput, width, height,
        [](const QImage& image) { return !image.isNull(); },
        &failure)) << failure.toStdString();

    const auto readCenter = [&](const QString& outputBase) {
        const QFileInfo info(outputBase);
        const QString frame = QDir(info.absolutePath()).filePath(
            QStringLiteral("%1_0001.png").arg(info.completeBaseName()));
        const QImage rendered(frame);
        EXPECT_FALSE(rendered.isNull()) << frame.toStdString();
        return rendered.pixelColor(width / 2, height / 2);
    };
    const QColor baselinePixel = readCenter(baselineOutput);
    const QColor processedPixel = readCenter(processedOutput);
    EXPECT_GT(processedPixel.red(), baselinePixel.red() + 20);
    EXPECT_GT(processedPixel.green(), baselinePixel.green() + 20);
    EXPECT_GT(processedPixel.blue(), baselinePixel.blue() + 20);
    EXPECT_NEAR(processedPixel.alpha(), baselinePixel.alpha(), 1);
}

TEST(RenderQueueLayerImageIntegrationTest,
     CustomOutputResolutionResizesCompositionBackground)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int compositionWidth = 48;
    constexpr int compositionHeight = 32;
    constexpr int outputWidth = 24;
    constexpr int outputHeight = 16;
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
    projects.createProject(QStringLiteral("CustomOutputResolutionTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());
    Artifact::ArtifactCompositionInitParams params;
    params.setCompositionName(ArtifactCore::UniString(
        QStringLiteral("ResolutionComposition")));
    params.setResolution(compositionWidth, compositionHeight);
    params.setFrameRate(30.0);
    params.setDurationFrames(2);
    params.setBackgroundColor(ArtifactCore::FloatColor(
        0.18f, 0.42f, 0.68f, 1.0f));
    const auto composition = projects.createComposition(params);
    ASSERT_TRUE(composition.success);

    const QString outputBase = isolatedOutputPath(
        QStringLiteral("render_queue_custom_resolution.png"));
    QString failure;
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("ResolutionComposition"),
        outputBase, outputWidth, outputHeight,
        [](const QImage& image) {
            const QColor pixel = image.pixelColor(image.width() / 2,
                                                   image.height() / 2);
            return pixel.red() > 35 && pixel.green() > 90 && pixel.blue() > 150;
        },
        &failure)) << failure.toStdString();

    const QFileInfo outputInfo(outputBase);
    const QString framePath = QDir(outputInfo.absolutePath()).filePath(
        QStringLiteral("%1_0001.png").arg(outputInfo.completeBaseName()));
    const QImage output(framePath);
    ASSERT_FALSE(output.isNull());
    EXPECT_EQ(output.size(), QSize(outputWidth, outputHeight));
    EXPECT_EQ(output.pixelColor(0, 0), output.pixelColor(outputWidth - 1,
                                                        outputHeight - 1));
}

TEST(RenderQueueLayerImageIntegrationTest,
     CustomCropRendersOnlyTheRequestedImageRegion)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 48;
    constexpr int height = 32;
    constexpr int cropX = 24;
    constexpr int cropWidth = 24;
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
    projects.createProject(QStringLiteral("RenderQueueCustomCropTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());
    const QString sourcePath = QDir(projectRoot.path()).filePath(
        QStringLiteral("two-region-source.png"));
    QImage source(width, height, QImage::Format_RGBA8888);
    source.fill(QColor(28, 220, 64, 255));
    for (int y = 0; y < height; ++y) {
        for (int x = cropX; x < width; ++x) {
            source.setPixelColor(x, y, QColor(220, 32, 176, 255));
        }
    }
    ASSERT_TRUE(source.save(sourcePath, "PNG"));

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(ArtifactCore::UniString(
        QStringLiteral("CroppedComposition")));
    compositionParams.setResolution(width, height);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(2);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);
    Artifact::ArtifactImageInitParams imageParams(QStringLiteral("TwoRegionImage"));
    imageParams.setImagePath(sourcePath);
    const auto layer = projects.addLayerToComposition(composition.id, imageParams);
    ASSERT_TRUE(layer.success);

    const QString outputBase = isolatedOutputPath(
        QStringLiteral("render_queue_custom_crop.png"));
    const QVariantMap cropSettings{
        {QStringLiteral("regionMode"),
         static_cast<int>(Artifact::ArtifactRenderJob::RegionMode::CustomCrop)},
        {QStringLiteral("cropX"), cropX},
        {QStringLiteral("cropY"), 0},
        {QStringLiteral("cropW"), cropWidth},
        {QStringLiteral("cropH"), height}};
    QString failure;
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("CroppedComposition"),
        outputBase, cropWidth, height,
        [](const QImage& image) {
            const QColor center = image.pixelColor(image.width() / 2,
                                                    image.height() / 2);
            return center.red() > 180 && center.green() < 70 &&
                   center.blue() > 130;
        },
        &failure, 2, QStringLiteral("gpu"), 1, cropSettings))
        << failure.toStdString();
}

TEST(RenderQueueLayerImageIntegrationTest,
     MissingStillImageRelinkSurvivesProjectReloadAndRendersMovedSource)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 64;
    constexpr int height = 48;
    const QString originalDirectory = QDir(projectRoot.path()).filePath(
        QStringLiteral("still-source"));
    const QString relocatedDirectory = QDir(projectRoot.path()).filePath(
        QStringLiteral("still-relocated"));
    ASSERT_TRUE(QDir().mkpath(originalDirectory));
    const QString originalPath = QDir(originalDirectory).filePath(
        QStringLiteral("plate.png"));
    QImage source(width, height, QImage::Format_RGBA8888);
    source.fill(QColor(225, 30, 18, 255));
    ASSERT_TRUE(source.save(originalPath, "PNG"));

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    auto* queue = Artifact::ArtifactRenderQueueService::instance();
    auto* projectService = Artifact::ArtifactProjectService::instance();
    ASSERT_NE(queue, nullptr);
    ASSERT_NE(projectService, nullptr);
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
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("MissingStillImageRelink"), true);
    ASSERT_TRUE(projects.isProjectCreated());
    auto project = projects.getCurrentProjectSharedPtr();
    ASSERT_TRUE(project);
    project->addAssetFromPath(originalPath);

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(ArtifactCore::UniString(
        QStringLiteral("MissingStillImage")));
    compositionParams.setResolution(width, height);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(2);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);
    Artifact::ArtifactImageInitParams imageParams(QStringLiteral("StillSource"));
    imageParams.setImagePath(originalPath);
    const auto layerResult = projects.addLayerToComposition(
        composition.id, imageParams);
    ASSERT_TRUE(layerResult.success);

    const QString projectPath = QDir(projectRoot.path()).filePath(
        QStringLiteral("missing-still-image.artifact"));
    ASSERT_TRUE(projects.saveToFile(projectPath).success);
    QDir root(projectRoot.path());
    ASSERT_TRUE(root.rename(QStringLiteral("still-source"),
                            QStringLiteral("still-relocated")));
    const QString relocatedPath = QDir(relocatedDirectory).filePath(
        QStringLiteral("plate.png"));
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(projectPath));

    const auto loadedComposition = projects.findComposition(composition.id);
    ASSERT_TRUE(loadedComposition.success);
    const auto loadedCompositionPtr = loadedComposition.ptr.lock();
    ASSERT_TRUE(loadedCompositionPtr);
    const auto loadedLayers = loadedCompositionPtr->allLayer();
    ASSERT_EQ(loadedLayers.size(), 1);
    auto* loadedImage = dynamic_cast<Artifact::ArtifactImageLayer*>(
        loadedLayers.front().get());
    ASSERT_NE(loadedImage, nullptr);
    EXPECT_EQ(loadedImage->sourcePath(), originalPath);
    EXPECT_FALSE(QFileInfo::exists(originalPath));

    const QString outputBase = isolatedOutputPath(
        QStringLiteral("render_queue_relinked_still.png"));
    ASSERT_TRUE(QDir().mkpath(QFileInfo(outputBase).absolutePath()));
    queue->addRenderQueueForComposition(composition.id,
                                       QStringLiteral("MissingStillImage"));
    ASSERT_EQ(queue->jobCount(), 1);
    queue->setJobOutputPathAt(0, outputBase);
    queue->setJobOutputSettingsAt(0, QStringLiteral("PNG"),
                                  QStringLiteral("png"), width, height,
                                  30.0, 8000);
    const auto missingPreflight = queue->preflightRenderQueueAt(0);
    ASSERT_TRUE(missingPreflight.hasErrors());
    const QString missingDetails =
        Artifact::ArtifactRenderQueueService::formatPreflightDetails(
            missingPreflight).join(QLatin1Char('\n'));
    EXPECT_TRUE(missingDetails.contains(originalPath))
        << missingDetails.toStdString();
    EXPECT_EQ(queue->jobStatusAt(0), QStringLiteral("Pending"));
    EXPECT_FALSE(QFileInfo::exists(outputBase));

    ASSERT_TRUE(projectService->relinkFootageByPath(originalPath, relocatedPath));
    ASSERT_TRUE(projects.saveToFile(projectPath).success);
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(projectPath));
    auto* relinkedFootage = findFootageItem(projects.projectItems(),
                                            relocatedPath);
    ASSERT_NE(relinkedFootage, nullptr);
    EXPECT_EQ(relinkedFootage->filePath, relocatedPath);
    const auto reloadedComposition = projects.findComposition(composition.id);
    ASSERT_TRUE(reloadedComposition.success);
    const auto reloadedCompositionPtr = reloadedComposition.ptr.lock();
    ASSERT_TRUE(reloadedCompositionPtr);
    const auto reloadedLayers = reloadedCompositionPtr->allLayer();
    ASSERT_EQ(reloadedLayers.size(), 1);
    auto* reloadedImage = dynamic_cast<Artifact::ArtifactImageLayer*>(
        reloadedLayers.front().get());
    ASSERT_NE(reloadedImage, nullptr);
    EXPECT_EQ(reloadedImage->sourcePath(), relocatedPath);
    const auto recoveredPreflight = queue->preflightRenderQueueAt(0);
    EXPECT_FALSE(recoveredPreflight.hasErrors())
        << Artifact::ArtifactRenderQueueService::formatPreflightDetails(
               recoveredPreflight).join(QLatin1Char('\n')).toStdString();

    queue->setJobFramePaddingAt(0, 4);
    queue->setJobFrameRangeAt(0, 1, 2);
    auto selectiveSettings = queue->jobSelectiveSettingsAt(0);
    selectiveSettings.insert(QStringLiteral("frameRangeMode"),
        static_cast<int>(Artifact::ArtifactRenderJob::FrameRangeMode::Custom));
    ASSERT_TRUE(queue->setJobSelectiveSettingsAt(0, selectiveSettings));
    queue->startRenderQueueAt(0);

    QElapsedTimer timeout;
    timeout.start();
    QString status;
    while (timeout.elapsed() < 60000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        status = queue->jobStatusAt(0);
        if (status == QStringLiteral("Completed") ||
            status == QStringLiteral("Failed") ||
            status == QStringLiteral("Canceled")) {
            break;
        }
        QThread::msleep(10);
    }
    ASSERT_EQ(status, QStringLiteral("Completed"))
        << queue->jobErrorMessageAt(0).toStdString();

    const QString framePath = QDir(QFileInfo(outputBase).absolutePath()).filePath(
        QStringLiteral("%1_0001.png").arg(QFileInfo(outputBase).completeBaseName()));
    const QImage rendered(framePath);
    ASSERT_FALSE(rendered.isNull()) << framePath.toStdString();
    ASSERT_EQ(rendered.size(), QSize(width, height));
    const QColor center = rendered.pixelColor(width / 2, height / 2);
    EXPECT_GT(center.red(), center.green() + 80);
    EXPECT_GT(center.red(), center.blue() + 80);
}

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
     RelinkedImageSequenceSurvivesProjectReloadAndRendersEveryFrame)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(
        QStringLiteral("ArtifactImageSequenceRelinkIntegrationTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 64;
    constexpr int height = 48;
    const QString originalDirectory =
        projectRoot.filePath(QStringLiteral("source_sequence"));
    const QString relocatedDirectory =
        projectRoot.filePath(QStringLiteral("relocated_sequence"));
    ASSERT_TRUE(QDir().mkpath(originalDirectory));

    const QList<QColor> sourceColors = {
        QColor(230, 20, 15), QColor(20, 220, 35), QColor(25, 40, 235)};
    QStringList originalPaths;
    for (int index = 0; index < sourceColors.size(); ++index) {
        QImage frame(width, height, QImage::Format_RGBA8888);
        frame.fill(sourceColors.at(index));
        const QString framePath = QDir(originalDirectory).filePath(
            QStringLiteral("plate_%1.png")
                .arg(index + 1, 4, 10, QLatin1Char('0')));
        ASSERT_TRUE(frame.save(framePath, "PNG"));
        originalPaths.append(framePath);
    }

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    auto* queue = Artifact::ArtifactRenderQueueService::instance();
    auto* projectService = Artifact::ArtifactProjectService::instance();
    ASSERT_NE(queue, nullptr);
    ASSERT_NE(projectService, nullptr);
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
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("ImageSequenceRelink"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(ArtifactCore::UniString(
        QStringLiteral("RelinkedSequence")));
    compositionParams.setResolution(width, height);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(sourceColors.size());
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);

    auto project = projects.getCurrentProjectSharedPtr();
    ASSERT_TRUE(project);
    project->addAssetFromPath(originalPaths.first(), originalPaths, 30.0);
    auto* originalFootage = findFootageItem(projects.projectItems(),
                                            originalPaths.first());
    ASSERT_NE(originalFootage, nullptr);
    ASSERT_TRUE(originalFootage->isSequence);
    ASSERT_EQ(originalFootage->sequencePaths, originalPaths);

    Artifact::ArtifactImageInitParams imageParams(
        QStringLiteral("RelinkableSequence"));
    imageParams.setImagePath(originalPaths.first());
    imageParams.setSequencePaths(originalPaths);
    imageParams.setSequenceFrameRate(30.0);
    const auto layerResult =
        projects.addLayerToComposition(composition.id, imageParams);
    ASSERT_TRUE(layerResult.success);
    auto* imageLayer = dynamic_cast<Artifact::ArtifactImageLayer*>(
        layerResult.layer.get());
    ASSERT_NE(imageLayer, nullptr);
    ASSERT_TRUE(imageLayer->isImageSequence());

    QDir root(projectRoot.path());
    ASSERT_TRUE(root.rename(QStringLiteral("source_sequence"),
                            QStringLiteral("relocated_sequence")));
    QStringList relocatedPaths;
    for (const QString& originalPath : originalPaths) {
        relocatedPaths.append(QDir(relocatedDirectory).filePath(
            QFileInfo(originalPath).fileName()));
    }
    ASSERT_TRUE(projectService->relinkFootageByPath(
        originalPaths.first(), relocatedPaths.first()));

    auto* relinkedFootage = findFootageItem(projects.projectItems(),
                                            relocatedPaths.first());
    ASSERT_NE(relinkedFootage, nullptr);
    EXPECT_EQ(relinkedFootage->sequencePaths, relocatedPaths);
    EXPECT_EQ(imageLayer->sequenceFramePaths(), relocatedPaths);

    const QString projectPath =
        projectRoot.filePath(QStringLiteral("relinked-sequence.artifact"));
    ASSERT_TRUE(projects.saveToFile(projectPath).success);
    const auto compositionId = composition.id;
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(projectPath));

    auto loadedProject = projects.getCurrentProjectSharedPtr();
    ASSERT_TRUE(loadedProject);
    auto* loadedFootage = findFootageItem(projects.projectItems(),
                                          relocatedPaths.first());
    ASSERT_NE(loadedFootage, nullptr);
    EXPECT_EQ(loadedFootage->sequencePaths, relocatedPaths);
    const auto loadedComposition = projects.findComposition(compositionId);
    ASSERT_TRUE(loadedComposition.success);
    auto loadedCompositionPtr = loadedComposition.ptr.lock();
    ASSERT_TRUE(loadedCompositionPtr);
    auto loadedLayer = loadedCompositionPtr->frontMostLayer();
    ASSERT_NE(loadedLayer.get(), nullptr);
    auto* loadedImageLayer =
        dynamic_cast<Artifact::ArtifactImageLayer*>(loadedLayer.get());
    ASSERT_NE(loadedImageLayer, nullptr);
    EXPECT_EQ(loadedImageLayer->sequenceFramePaths(), relocatedPaths);

    QString failure;
    const QString outputBasePath =
        QDir(projectRoot.path()).filePath(QStringLiteral("captures/sequence.png"));
    ASSERT_TRUE(addAndRender(
        queue, compositionId, QStringLiteral("RelinkedSequence"),
        outputBasePath, width, height,
        [](const QImage& image) {
            return countPixelsMatching(image, [](QRgb pixel) {
                return qAlpha(pixel) > 32 &&
                       std::max({qRed(pixel), qGreen(pixel), qBlue(pixel)}) > 32;
            }) > 8;
        }, &failure, 2,
        QStringLiteral("gpu"), 0)) << failure.toStdString();

    const QFileInfo outputInfo(outputBasePath);
    for (int index = 0; index < sourceColors.size(); ++index) {
        const QString framePath = QDir(outputInfo.absolutePath()).filePath(
            QStringLiteral("sequence_%1.png")
                .arg(index, 4, 10, QLatin1Char('0')));
        const QImage rendered(framePath);
        ASSERT_FALSE(rendered.isNull()) << framePath.toStdString();
        ASSERT_EQ(rendered.size(), QSize(width, height));
        const QColor pixel = rendered.pixelColor(width / 2, height / 2);
        const QColor expected = sourceColors.at(index);
        const int dominant = index == 0 ? pixel.red()
                             : index == 1 ? pixel.green()
                                          : pixel.blue();
        const int otherA = index == 0 ? pixel.green()
                          : index == 1 ? pixel.red()
                                       : pixel.red();
        const int otherB = index == 2 ? pixel.green() : pixel.blue();
        EXPECT_GT(dominant, otherA + 40) << framePath.toStdString();
        EXPECT_GT(dominant, otherB + 40) << framePath.toStdString();
        EXPECT_GT(dominant, 100) << "Unexpected source frame color in "
                                 << framePath.toStdString()
                                 << " (expected dominant channel "
                                 << expected.name().toStdString() << ')';
    }
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
     AnimatedSourceTextRendersBothFramesThroughManagerQueue)
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
    projects.createProject(QStringLiteral("RenderQueueAnimatedTextTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("AnimatedText")));
    compositionParams.setResolution(width, height);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(3);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactTextLayerInitParams textParams(QStringLiteral("Text"));
    const auto textResult = projects.addLayerToComposition(
        composition.id, textParams);
    ASSERT_TRUE(textResult.success);
    auto* text = dynamic_cast<Artifact::ArtifactTextLayer*>(
        textResult.layer.get());
    ASSERT_NE(text, nullptr);
    (void)text->getLayerPropertyGroups();
    text->setText(ArtifactCore::UniString(QStringLiteral("Frame One")));
    text->setSourceTextAtFrame(1, QStringLiteral("Frame One"));
    text->setSourceTextAtFrame(2, QStringLiteral("Frame Two"));
    ASSERT_TRUE(text->hasSourceTextKeyframes());
    ASSERT_EQ(text->sourceTextAtFrame(1), QStringLiteral("Frame One"));
    ASSERT_EQ(text->sourceTextAtFrame(2), QStringLiteral("Frame Two"));
    text->setFontSize(30.0f);
    text->setTextColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));

    const QString projectPath =
        projectRoot.filePath(QStringLiteral("animated-text.artifact"));
    ASSERT_TRUE(projects.saveToFile(projectPath).success);
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(projectPath));

    const auto loadedComposition = projects.findComposition(composition.id);
    ASSERT_TRUE(loadedComposition.success);
    auto loadedCompositionPtr = loadedComposition.ptr.lock();
    ASSERT_TRUE(loadedCompositionPtr);
    auto loadedLayer = loadedCompositionPtr->frontMostLayer();
    ASSERT_NE(loadedLayer.get(), nullptr);
    auto* reloadedText =
        dynamic_cast<Artifact::ArtifactTextLayer*>(loadedLayer.get());
    ASSERT_NE(reloadedText, nullptr);
    ASSERT_TRUE(reloadedText->hasSourceTextKeyframes());
    EXPECT_EQ(reloadedText->sourceTextAtFrame(1), QStringLiteral("Frame One"));
    EXPECT_EQ(reloadedText->sourceTextAtFrame(2), QStringLiteral("Frame Two"));

    QString failure;
    ASSERT_TRUE(addAndRenderTwoTextFrames(
        queue, composition.id, QStringLiteral("AnimatedText"),
        isolatedOutputPath(QStringLiteral("render_queue_animated_text.png")),
        width, height, &failure)) << failure.toStdString();
}

TEST(RenderQueueLayerImageIntegrationTest,
     AnimatedTextAnimatorRendersBothFramesThroughManagerQueue)
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
    projects.createProject(QStringLiteral("RenderQueueTextAnimatorTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("AnimatedTextAnimator")));
    compositionParams.setResolution(width, height);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(3);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactTextLayerInitParams textParams(QStringLiteral("Text"));
    const auto textResult = projects.addLayerToComposition(
        composition.id, textParams);
    ASSERT_TRUE(textResult.success);
    auto* text = dynamic_cast<Artifact::ArtifactTextLayer*>(
        textResult.layer.get());
    ASSERT_NE(text, nullptr);
    text->setText(ArtifactCore::UniString(
        QStringLiteral("Animated Glyph Position")));
    text->setFontSize(30.0f);
    text->setTextColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    ASSERT_TRUE(text->addAnimatorProperty(QStringLiteral("position")));
    (void)text->getLayerPropertyGroups();
    const auto positionX =
        text->getProperty(QStringLiteral("text.animators.0.positionX"));
    ASSERT_TRUE(positionX);
    positionX->addKeyFrame(ArtifactCore::RationalTime(1, 30), QVariant(0.0));
    positionX->addKeyFrame(ArtifactCore::RationalTime(2, 30), QVariant(48.0));
    const QJsonArray animatorSnapshot = text->textAnimatorStackSnapshot();

    const QString projectPath =
        projectRoot.filePath(QStringLiteral("animated-glyph-position.artifact"));
    ASSERT_TRUE(projects.saveToFile(projectPath).success);
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(projectPath));

    const auto loadedComposition = projects.findComposition(composition.id);
    ASSERT_TRUE(loadedComposition.success);
    auto loadedCompositionPtr = loadedComposition.ptr.lock();
    ASSERT_TRUE(loadedCompositionPtr);
    auto loadedLayer = loadedCompositionPtr->frontMostLayer();
    ASSERT_NE(loadedLayer.get(), nullptr);
    auto* reloadedText =
        dynamic_cast<Artifact::ArtifactTextLayer*>(loadedLayer.get());
    ASSERT_NE(reloadedText, nullptr);
    EXPECT_EQ(reloadedText->textAnimatorStackSnapshot(), animatorSnapshot);
    const auto reloadedPositionX =
        reloadedText->getProperty(QStringLiteral("text.animators.0.positionX"));
    ASSERT_TRUE(reloadedPositionX);
    ASSERT_EQ(reloadedPositionX->getKeyFrames().size(), 2u);
    EXPECT_DOUBLE_EQ(
        reloadedPositionX
            ->interpolateValue(ArtifactCore::RationalTime(2, 30))
            .toDouble(),
        48.0);

    QString failure;
    ASSERT_TRUE(addAndRenderTwoTextFrames(
        queue, composition.id, QStringLiteral("AnimatedTextAnimator"),
        isolatedOutputPath(QStringLiteral("render_queue_text_animator.png")),
        width, height, &failure)) << failure.toStdString();
}

TEST(RenderQueueLayerImageIntegrationTest,
     ComplexArabicIndicTextRendersThroughManagerQueue)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 480;
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
    projects.createProject(QStringLiteral("RenderQueueArabicTextTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("ArabicText")));
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
    text->setText(ArtifactCore::UniString(QStringLiteral(
        "السَّلَامُ عَلَيْكُمْ १२३४ العربية हिन्दी")));
    text->setFontSize(26.0f);
    text->setTextColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));

    QString failure;
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("ArabicText"),
        isolatedOutputPath(QStringLiteral("render_queue_arabic_text.png")),
        width, height, hasBrightTextPixels, &failure)) << failure.toStdString();
}

TEST(RenderQueueLayerImageIntegrationTest,
     EmojiSequencesRenderThroughManagerQueue)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 480;
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
    projects.createProject(QStringLiteral("RenderQueueEmojiTextTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("EmojiText")));
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
    text->setText(ArtifactCore::UniString(QStringLiteral(
        "Family 👨‍👩‍👧‍👦  Skin 👩🏽‍💻  Flag 🇯🇵  Keycap 1️⃣")));
    text->setFontSize(26.0f);
    text->setTextColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));

    QString failure;
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("EmojiText"),
        isolatedOutputPath(QStringLiteral("render_queue_emoji_text.png")),
        width, height, hasBrightTextPixels, &failure)) << failure.toStdString();
}

TEST(RenderQueueLayerImageIntegrationTest,
     AdjacentEmojiGraphemesKeepDistinctVisualSpacing)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 320;
    constexpr int height = 120;
    constexpr int fontSize = 32;
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
    projects.createProject(QStringLiteral("RenderQueueEmojiGraphemeTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("EmojiGrapheme")));
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
    const QString firstEmoji = QStringLiteral("👩🏽‍💻");
    const QString secondEmoji = QStringLiteral("👨‍👩‍👧‍👦");
    text->setText(ArtifactCore::UniString(firstEmoji));
    (void)text->getLayerPropertyGroups();
    text->setSourceTextAtFrame(1, firstEmoji);
    text->setSourceTextAtFrame(2, secondEmoji);
    text->setSourceTextAtFrame(3, firstEmoji + secondEmoji);
    ASSERT_TRUE(text->hasSourceTextKeyframes());
    text->setFontSize(static_cast<float>(fontSize));
    text->setTextColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));

    QString failure;
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("EmojiGrapheme"),
        isolatedOutputPath(QStringLiteral("render_queue_emoji_grapheme.png")),
        width, height, hasVisibleTextPixels, &failure, 4)) << failure.toStdString();

    const QString outputPath = isolatedOutputPath(
        QStringLiteral("render_queue_emoji_grapheme.png"));
    const QString outputDirectory = QFileInfo(outputPath).absolutePath();
    const QString outputBaseName = QFileInfo(outputPath).completeBaseName();
    const auto readFrame = [&](const int frame) {
        return QImage(QDir(outputDirectory).filePath(
            QStringLiteral("%1_%2.png")
                .arg(outputBaseName)
                .arg(frame, 4, 10, QLatin1Char('0'))));
    };
    const QImage firstImage = readFrame(1);
    const QImage secondImage = readFrame(2);
    const QImage adjacentImage = readFrame(3);
    ASSERT_FALSE(firstImage.isNull());
    ASSERT_FALSE(secondImage.isNull());
    ASSERT_FALSE(adjacentImage.isNull());

    const int firstWidth = visibleTextBounds(firstImage).width();
    const int secondWidth = visibleTextBounds(secondImage).width();
    const QRect firstBounds = visibleTextBounds(firstImage);
    const QRect secondBounds = visibleTextBounds(secondImage);
    const QRect adjacentBounds = visibleTextBounds(adjacentImage);
    const int adjacentWidth = adjacentBounds.width();
    ASSERT_GT(firstWidth, 0);
    ASSERT_GT(secondWidth, 0);
    ASSERT_GT(adjacentWidth, 0);
    ASSERT_GT(firstBounds.height(), 0);
    ASSERT_GT(secondBounds.height(), 0);
    ASSERT_GT(adjacentBounds.height(), 0);
    EXPECT_GE(adjacentWidth,
              static_cast<int>((firstWidth + secondWidth) * 0.85))
        << "Adjacent emoji graphemes lost horizontal advance and overlap; "
        << "individual widths=" << firstWidth << "+" << secondWidth
        << ", combined width=" << adjacentWidth;
    EXPECT_LE(adjacentWidth, fontSize * 4)
        << "ZWJ/modifier sequences expanded across too many glyph cells; "
        << "measured width=" << adjacentWidth;
    EXPECT_GE(adjacentBounds.height(),
              static_cast<int>(std::max(firstBounds.height(), secondBounds.height()) * 0.8))
        << "Adjacent emoji graphemes lost their vertical ink extent";
    EXPECT_LE(adjacentBounds.height(),
              static_cast<int>(std::max(firstBounds.height(), secondBounds.height()) * 1.25))
        << "Adjacent emoji graphemes unexpectedly expanded into extra rows; "
        << "individual heights=" << firstBounds.height() << ", "
        << secondBounds.height() << ", combined height="
        << adjacentBounds.height();
    const double firstEmojiMatch = bestVisiblePixelMatchFraction(
        firstImage, adjacentImage, -2, 2, 2);
    EXPECT_GT(firstEmojiMatch, 0.6)
        << "The first color emoji no longer appears at its single-glyph position; "
        << "visible pixel match=" << firstEmojiMatch;
    const double secondEmojiMatch = bestVisiblePixelMatchFraction(
        secondImage, adjacentImage, std::max(firstWidth * 3 / 4, 1),
        fontSize * 3, 2);
    EXPECT_GT(secondEmojiMatch, 0.6)
        << "The second color emoji is missing, collapsed into the first cell, "
        << "or visually overlapped; separated visible pixel match="
        << secondEmojiMatch;
}

TEST(RenderQueueLayerImageIntegrationTest,
     JapaneseVerticalWritingProducesTallColumnThroughManagerQueue)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 240;
    constexpr int height = 360;
    constexpr int fontSize = 28;
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
    projects.createProject(QStringLiteral("RenderQueueVerticalTextTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("VerticalJapanese")));
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
    text->setText(ArtifactCore::UniString(
        QStringLiteral("「日本語」12、縦書き。")));
    text->setFontSize(static_cast<float>(fontSize));
    text->setTextColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    text->setWritingMode(ArtifactCore::TextWritingMode::Vertical);

    QString failure;
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("VerticalJapanese"),
        isolatedOutputPath(QStringLiteral("render_queue_vertical_japanese.png")),
        width, height, hasBrightTextPixels, &failure)) << failure.toStdString();
    const QImage image(QDir(QFileInfo(isolatedOutputPath(
        QStringLiteral("render_queue_vertical_japanese.png"))).absolutePath())
        .filePath(QStringLiteral("render_queue_vertical_japanese_0001.png")));
    ASSERT_FALSE(image.isNull());
    const QRect bounds = visibleTextBounds(image);
    ASSERT_GT(bounds.width(), 0);
    ASSERT_GT(bounds.height(), 0);
    EXPECT_GT(bounds.height(), bounds.width() * 2)
        << "Vertical Japanese text should form a tall column; measured bounds="
        << bounds.width() << "x" << bounds.height();
    EXPECT_GT(bounds.height(), fontSize * 4)
        << "Vertical Japanese text did not extend beyond a few glyph cells; "
        << "measured bounds=" << bounds.width() << "x" << bounds.height();
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
    if (qEnvironmentVariable("ARTIFACT_RENDER_BACKEND") ==
        QStringLiteral("d3d12")) {
        EXPECT_GT(plannedD3D12Adapters, 0) << planMessage->toStdString();
        EXPECT_EQ(primaryGpuBackend.captured(1), QLatin1String("d3d12"))
            << "The requested D3D12 backend was not selected: "
            << joined.toStdString();
        EXPECT_GT(primaryGpuFrames.captured(1).toInt(), 0)
            << "The D3D12 primary adapter did not process a frame: "
            << joined.toStdString();
    }

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
