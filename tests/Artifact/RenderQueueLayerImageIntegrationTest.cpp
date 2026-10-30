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
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QPointF>
#include <QPoint>
#include <QRegularExpression>
#include <QSize>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryDir>
#include <QThread>
#include <QTimerEvent>
#include <QVariantMap>
#include <QVector3D>
#include <QtGlobal>

#include <OpenImageIO/imageio.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

import Artifact.Composition.InitParams;
import Artifact.Composition.Result;
import Artifact.Layer.Abstract;
import Artifact.Layer.Camera;
import Artifact.Layer.InitParams;
import Artifact.Layer.Particle;
import Artifact.Layer.Result;
import Artifact.Layer.Shape;
import Artifact.Layers.SolidImage;
import Artifact.Layer.Text;
import Artifact.Generator.Particle;
import Artifact.Project.Manager;
import Artifact.Effect.Abstract;
import Artifact.Effect.Creative;
import Artifact.Render.IRenderer;
import Artifact.Render.DiligentDeviceManager;
import Artifact.Render.OffscreenComposition;
import Artifact.Render.CompositionViewDrawing;
import Artifact.Render.Queue.Job;
import Artifact.Render.Queue.Encoder;
import Artifact.Render.Queue.Service;
import BrightnessEffect;
import Shape.Types;
import Shape.Operator;
import Color.Float;
import Math.Interpolate;
import Property.Abstract;
import Time.Rational;
import Image.OpenEXR;
import Image.Cryptomatte.Pixel;
import IO.ImageExporter;
import Image.ExportOptions;
import Image.MultiChannelImage;
import Memory.SharedPtr;
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
        message.contains(QStringLiteral("XPU async sequence manager requested")) ||
        message.contains(QStringLiteral("XPU test failure injection")) ||
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

class RenderQueueEventLoopProbe final : public QObject {
public:
    bool watch(Artifact::ArtifactRenderQueueService* queue, int jobIndex)
    {
        queue_ = queue;
        jobIndex_ = jobIndex;
        timerId_ = startTimer(1, Qt::PreciseTimer);
        return timerId_ != 0;
    }

    void stop()
    {
        if (timerId_ != 0) {
            killTimer(timerId_);
            timerId_ = 0;
        }
    }

    bool servicedWhileRendering() const { return servicedWhileRendering_; }

protected:
    void timerEvent(QTimerEvent* event) override
    {
        if (event->timerId() == timerId_ && queue_ &&
            queue_->jobStatusAt(jobIndex_) == QStringLiteral("Rendering")) {
            servicedWhileRendering_ = true;
        }
    }

private:
    Artifact::ArtifactRenderQueueService* queue_ = nullptr;
    int jobIndex_ = -1;
    int timerId_ = 0;
    bool servicedWhileRendering_ = false;
};

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
    queue->setJobFramePaddingAt(jobIndex, 4);
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

bool hasBrightTextPixels(const QImage& image)
{
    return countPixelsMatching(image, [](QRgb pixel) {
        return qAlpha(pixel) > 32 && qRed(pixel) > 210 &&
               qGreen(pixel) > 210 && qBlue(pixel) > 210;
    }) > 20;
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
                     int frameEnd = 2,
                     RenderQueueEventLoopProbe* eventLoopProbe = nullptr,
                     bool multiChannel = false,
                     bool deep = false)
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
    queue->setJobDeepExportEnabledAt(jobIndex, deep);
    if (multiChannel) {
        queue->setJobMultiChannelEnabledAt(jobIndex, true);
        queue->setJobMultiChannelChannelsAt(
            jobIndex, {QStringLiteral("R"), QStringLiteral("G"),
                       QStringLiteral("B"), QStringLiteral("A"),
                       QStringLiteral("Depth"), QStringLiteral("Normal.X"),
                       QStringLiteral("Normal.Y"), QStringLiteral("Normal.Z"),
                       QStringLiteral("Velocity.X"),
                       QStringLiteral("Velocity.Y"),
                       QStringLiteral("ObjectId"),
                       QStringLiteral("MaterialId")});
    }
    queue->setJobFrameRangeAt(jobIndex, frameStart, frameEnd);
    auto selectiveSettings = queue->jobSelectiveSettingsAt(jobIndex);
    selectiveSettings.insert(
        QStringLiteral("frameRangeMode"),
        static_cast<int>(Artifact::ArtifactRenderJob::FrameRangeMode::Custom));
    if (!queue->setJobSelectiveSettingsAt(jobIndex, selectiveSettings)) {
        *failure = QStringLiteral("Could not select the explicit EXR frame range");
        return false;
    }
    if (eventLoopProbe && !eventLoopProbe->watch(queue, jobIndex)) {
        *failure = QStringLiteral("Could not start the GUI event-loop probe");
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
    if (eventLoopProbe) eventLoopProbe->stop();
    if (status != QStringLiteral("Completed")) {
        *failure = QStringLiteral("EXR Render Queue job ended as '%1': %2")
                       .arg(status, queue->jobErrorMessageAt(jobIndex));
        return false;
    }

    const QFileInfo outputInfo(outputBasePath);
    const QString framePattern = QStringLiteral("%1_%2.exr")
                                     .arg(outputInfo.completeBaseName());
    ArtifactCore::OpenExr exr;
    const std::size_t expectedComponents =
        static_cast<std::size_t>(expectedWidth) *
        static_cast<std::size_t>(expectedHeight) * 4u;
    for (int frame = frameStart; frame < frameEnd; ++frame) {
        const QString framePath = QDir(outputInfo.absolutePath()).filePath(
            framePattern.arg(frame, 4, 10, QLatin1Char('0')));
        if (deep) {
            std::vector<std::vector<ArtifactCore::OpenExrDeepSample>> samples;
            int width = 0;
            int height = 0;
            if (!exr.readDeepRGBA32F(framePath, samples, width, height) ||
                width != expectedWidth || height != expectedHeight ||
                samples.size() != expectedComponents / 4u) {
                *failure = QStringLiteral(
                    "Render Queue did not write a valid Deep EXR: %1")
                               .arg(framePath);
                return false;
            }
            for (std::size_t pixel = 0; pixel < samples.size(); ++pixel) {
                if (samples[pixel].size() != 1u) {
                    *failure = QStringLiteral(
                        "Render Queue Deep EXR must contain one resolved sample per pixel: %1")
                                   .arg(framePath);
                    return false;
                }
                const auto& sample = samples[pixel].front();
                if (!std::isfinite(sample.depth) ||
                    !std::isfinite(sample.red) ||
                    !std::isfinite(sample.green) ||
                    !std::isfinite(sample.blue) ||
                    !std::isfinite(sample.alpha) ||
                    sample.depth < 0.0f || sample.depth > 1.0f ||
                    sample.alpha < 0.0f || sample.alpha > 1.0f) {
                    *failure = QStringLiteral(
                        "Render Queue Deep EXR sample violates depth/alpha bounds: %1")
                                   .arg(framePath);
                    return false;
                }
            }
            const auto centerPixel =
                (static_cast<std::size_t>(expectedHeight / 2) *
                     static_cast<std::size_t>(expectedWidth) +
                 static_cast<std::size_t>(expectedWidth / 2));
            const auto& center = samples[centerPixel].front();
            if (std::abs(center.red - 0.15f) > 1.0e-3f ||
                std::abs(center.green - 0.25f) > 1.0e-3f ||
                std::abs(center.blue - 0.4f) > 1.0e-3f ||
                std::abs(center.alpha - 1.0f) > 1.0e-5f ||
                std::abs(center.depth - 1.0f) > 1.0e-5f) {
                *failure = QStringLiteral(
                    "Render Queue Deep EXR center color/depth did not match resolved AOVs: %1")
                               .arg(framePath);
                return false;
            }
            continue;
        }
        if (multiChannel) {
            const QByteArray encodedFramePath = framePath.toUtf8();
            auto input = OIIO::ImageInput::open(encodedFramePath.constData());
            if (!input) {
                *failure = QStringLiteral(
                    "Render Queue did not write a readable multi-channel EXR: %1")
                               .arg(framePath);
                return false;
            }
            const auto& spec = input->spec();
            if (spec.width != expectedWidth || spec.height != expectedHeight) {
                *failure = QStringLiteral(
                    "Multi-channel EXR dimensions were %1x%2, expected %3x%4: %5")
                               .arg(spec.width).arg(spec.height)
                               .arg(expectedWidth).arg(expectedHeight)
                               .arg(framePath);
                input->close();
                return false;
            }
            if (spec.nchannels <= 0 ||
                spec.channelnames.size() !=
                    static_cast<std::size_t>(spec.nchannels)) {
                *failure = QStringLiteral(
                    "Multi-channel EXR channel metadata is inconsistent: %1")
                               .arg(framePath);
                input->close();
                return false;
            }
            if (spec.format.basetype != OIIO::TypeDesc::FLOAT ||
                (!spec.channelformats.empty() &&
                 (spec.channelformats.size() !=
                      static_cast<std::size_t>(spec.nchannels) ||
                  !std::all_of(spec.channelformats.begin(),
                               spec.channelformats.end(), [](const auto& format) {
                                   return format.basetype == OIIO::TypeDesc::FLOAT;
                               })))) {
                *failure = QStringLiteral(
                    "Multi-channel EXR did not retain requested 32-bit float channels: %1")
                               .arg(framePath);
                input->close();
                return false;
            }
            const auto hasChannel = [&spec](const std::string& name) {
                return std::find(spec.channelnames.begin(),
                                 spec.channelnames.end(), name) !=
                       spec.channelnames.end();
            };
            const auto channelIndex = [&spec](const std::string& name) {
                const auto found = std::find(spec.channelnames.begin(),
                                             spec.channelnames.end(), name);
                return found == spec.channelnames.end()
                    ? -1 : static_cast<int>(found - spec.channelnames.begin());
            };
            for (const std::string& required : {
                     "R", "G", "B", "A", "Depth", "Normal.X", "Normal.Y",
                     "Normal.Z", "Velocity.X", "Velocity.Y", "ObjectId",
                     "MaterialId"}) {
                if (!hasChannel(required)) {
                    *failure = QStringLiteral(
                        "Multi-channel EXR is missing requested AOV %1: %2")
                                   .arg(QString::fromStdString(required), framePath);
                    input->close();
                    return false;
                }
            }
            std::vector<float> pixels(
                static_cast<std::size_t>(spec.width) *
                static_cast<std::size_t>(spec.height) *
                static_cast<std::size_t>(spec.nchannels));
            const bool readOk = input->read_image(
                OIIO::TypeDesc::FLOAT, pixels.data());
            if (!readOk || pixels.empty()) {
                input->close();
                *failure = QStringLiteral(
                    "Multi-channel EXR AOV payload was unreadable: %1")
                               .arg(framePath);
                return false;
            }
            if (!std::all_of(pixels.begin(), pixels.end(), [](float value) {
                    return std::isfinite(value);
                })) {
                input->close();
                *failure = QStringLiteral(
                    "Multi-channel EXR AOV payload contained a non-finite sample: %1")
                               .arg(framePath);
                return false;
            }
            const std::size_t centerPixel =
                (static_cast<std::size_t>(expectedHeight / 2) *
                     static_cast<std::size_t>(expectedWidth) +
                 static_cast<std::size_t>(expectedWidth / 2)) *
                static_cast<std::size_t>(spec.nchannels);
            for (const auto& [name, expected] : {
                     std::pair{std::string("R"), 0.15f},
                     std::pair{std::string("G"), 0.25f},
                     std::pair{std::string("B"), 0.4f},
                     std::pair{std::string("A"), 1.0f}}) {
                const int index = channelIndex(name);
                if (index < 0 ||
                    std::abs(pixels[centerPixel +
                                    static_cast<std::size_t>(index)] - expected) >
                        1.0e-3f) {
                    input->close();
                    *failure = QStringLiteral(
                        "Render Queue multi-channel EXR %1 sample was incorrect: %2")
                                   .arg(QString::fromStdString(name), framePath);
                    return false;
                }
            }
            if (spec.get_string_attribute("artifact/aov/normal_space", "") !=
                    "view" ||
                spec.get_string_attribute("artifact/aov/velocity_encoding", "") !=
                    "ndc_delta_xy" ||
                spec.get_string_attribute("artifact/aov/depth_encoding", "") !=
                    "device_depth_0_1" ||
                spec.get_string_attribute("artifact/frame", "") !=
                    std::to_string(frame) ||
                spec.get_string_attribute("artifact/frame_rate", "") != "30" ||
                spec.get_string_attribute("Software", "") != "ArtifactStudio") {
                input->close();
                *failure = QStringLiteral(
                    "Render Queue EXR metadata was incomplete or incorrect: %1")
                               .arg(framePath);
                return false;
            }
            const auto objectManifest = QJsonDocument::fromJson(
                QByteArray::fromStdString(spec.get_string_attribute(
                    "artifact.cryptomatte.object.manifest", "")));
            const auto materialManifest = QJsonDocument::fromJson(
                QByteArray::fromStdString(spec.get_string_attribute(
                    "artifact.cryptomatte.material.manifest", "")));
            const auto cryptomatteId = [](const QString& name) {
                QString id = QString::number(
                    ArtifactCore::Image::Cryptomatte::CryptoSample::nameToId(name),
                    16).rightJustified(8, QLatin1Char('0'));
                id.chop(1);
                return id;
            };
            const QString objectStandardPrefix = QStringLiteral("cryptomatte/%1")
                .arg(cryptomatteId(QStringLiteral("CryptoObject")));
            const QString materialStandardPrefix = QStringLiteral("cryptomatte/%1")
                .arg(cryptomatteId(QStringLiteral("CryptoMaterial")));
            const auto objectStandardManifest = QJsonDocument::fromJson(
                QByteArray::fromStdString(spec.get_string_attribute(
                    (objectStandardPrefix + QStringLiteral("/manifest")).toStdString(),
                    "")));
            const auto materialStandardManifest = QJsonDocument::fromJson(
                QByteArray::fromStdString(spec.get_string_attribute(
                    (materialStandardPrefix + QStringLiteral("/manifest")).toStdString(),
                    "")));
            const auto isValidCryptomatteManifest = [](const QJsonObject& manifest) {
                if (manifest.isEmpty()) {
                    return false;
                }
                for (auto entry = manifest.constBegin();
                     entry != manifest.constEnd(); ++entry) {
                    if (!entry.value().isString()) {
                        return false;
                    }
                    const QString expectedHash = QString::number(
                        ArtifactCore::Image::Cryptomatte::CryptoSample::nameToId(
                            entry.key()), 16).rightJustified(8, QLatin1Char('0'));
                    if (entry.value().toString() != expectedHash) {
                        return false;
                    }
                }
                return true;
            };
            if (spec.get_string_attribute(
                    "artifact.cryptomatte.object.name", "") != "CryptoObject" ||
                spec.get_string_attribute(
                    "artifact.cryptomatte.object.hash", "") !=
                    "MurmurHash3_32" ||
                !objectManifest.isObject() ||
                !isValidCryptomatteManifest(objectManifest.object()) ||
                spec.get_string_attribute(
                    "artifact.cryptomatte.material.name", "") !=
                    "CryptoMaterial" ||
                spec.get_string_attribute(
                    "artifact.cryptomatte.material.hash", "") !=
                    "MurmurHash3_32" ||
                !materialManifest.isObject() ||
                !isValidCryptomatteManifest(materialManifest.object()) ||
                spec.get_string_attribute(
                    (objectStandardPrefix + QStringLiteral("/name")).toStdString(),
                    "") != "CryptoObject" ||
                spec.get_string_attribute(
                    (objectStandardPrefix + QStringLiteral("/hash")).toStdString(),
                    "") != "MurmurHash3_32" ||
                spec.get_string_attribute(
                    (objectStandardPrefix + QStringLiteral("/conversion")).toStdString(),
                    "") != "uint32_to_float32" ||
                !objectStandardManifest.isObject() ||
                objectStandardManifest.object() != objectManifest.object() ||
                spec.get_string_attribute(
                    (materialStandardPrefix + QStringLiteral("/name")).toStdString(),
                    "") != "CryptoMaterial" ||
                spec.get_string_attribute(
                    (materialStandardPrefix + QStringLiteral("/hash")).toStdString(),
                    "") != "MurmurHash3_32" ||
                spec.get_string_attribute(
                    (materialStandardPrefix + QStringLiteral("/conversion")).toStdString(),
                    "") != "uint32_to_float32" ||
                !materialStandardManifest.isObject() ||
                materialStandardManifest.object() != materialManifest.object()) {
                input->close();
                *failure = QStringLiteral(
                    "Render Queue EXR Cryptomatte metadata or manifest was missing: %1")
                               .arg(framePath);
                return false;
            }
            const int alphaIndex = channelIndex("A");
            const auto packedCryptomatteMatches =
                [&spec, &pixels, &channelIndex, alphaIndex](
                    const std::string& idChannel,
                    const std::string& prefix) {
                    const int idIndex = channelIndex(idChannel);
                    const int redIndex = channelIndex(prefix + ".R");
                    const int greenIndex = channelIndex(prefix + ".G");
                    const int blueIndex = channelIndex(prefix + ".B");
                    const int alphaIndex = channelIndex(prefix + ".A");
                    if (idIndex < 0 || alphaIndex < 0 || redIndex < 0 || greenIndex < 0 ||
                        blueIndex < 0 || alphaIndex < 0) {
                        return false;
                    }
                    const auto component = [&pixels, &spec](
                        std::size_t pixel, int channel) {
                        return pixels[pixel *
                                          static_cast<std::size_t>(spec.nchannels) +
                                      static_cast<std::size_t>(channel)];
                    };
                    const std::size_t pixelCount =
                        static_cast<std::size_t>(spec.width) *
                        static_cast<std::size_t>(spec.height);
                    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
                        const float id = component(pixel, idIndex);
                        const float alpha = component(pixel, alphaIndex);
                        const float coverage = id > 0.0f
                            ? std::clamp(alpha, 0.0f, 1.0f) : 0.0f;
                        if (component(pixel, redIndex) != id ||
                            component(pixel, greenIndex) != coverage ||
                            component(pixel, blueIndex) != 0.0f ||
                            component(pixel, alphaIndex) != 0.0f) {
                            return false;
                        }
                    }
                    return true;
                };
            if (!packedCryptomatteMatches("ObjectId", "CryptoObject00") ||
                !packedCryptomatteMatches("MaterialId", "CryptoMaterial00")) {
                input->close();
                *failure = QStringLiteral(
                    "Render Queue Cryptomatte packed ID/coverage payload did not match its AOVs: %1")
                               .arg(framePath);
                return false;
            }
            const int depthIndex = channelIndex("Depth");
            if (depthIndex < 0) {
                input->close();
                *failure = QStringLiteral(
                    "Render Queue multi-channel EXR did not contain Depth: %1")
                               .arg(framePath);
                return false;
            }
            for (std::size_t pixel = 0;
                 pixel < static_cast<std::size_t>(spec.width) * spec.height;
                 ++pixel) {
                const float depth = pixels[
                    pixel * static_cast<std::size_t>(spec.nchannels) +
                    static_cast<std::size_t>(depthIndex)];
                if (depth < 0.0f || depth > 1.0f) {
                    input->close();
                    *failure = QStringLiteral(
                        "Render Queue EXR Depth sample was outside 0..1: %1")
                                   .arg(framePath);
                    return false;
                }
            }
            const float depthSample =
                pixels[centerPixel + static_cast<std::size_t>(depthIndex)];
            if (!std::isfinite(depthSample) ||
                std::abs(depthSample - 1.0f) > 1.0e-5f) {
                input->close();
                *failure = QStringLiteral(
                    "Render Queue multi-channel EXR Depth clear sample was incorrect: %1")
                               .arg(framePath);
                return false;
            }
            input->close();
            continue;
        }
        std::vector<float> rgba;
        int width = 0;
        int height = 0;
        if (!exr.readRGBA32F(framePath, rgba, width, height)) {
            *failure = QStringLiteral("Render Queue did not write a readable EXR: %1")
                           .arg(framePath);
            return false;
        }
        if (width != expectedWidth || height != expectedHeight) {
            *failure = QStringLiteral("EXR dimensions were %1x%2, expected %3x%4: %5")
                           .arg(width).arg(height).arg(expectedWidth).arg(expectedHeight)
                           .arg(framePath);
            return false;
        }
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
    }
    return true;
}

} // namespace

#include "../shape_visual/ShapeVisualCases.inc"

TEST(RenderQueueLayerImageIntegrationTest,
     ExportKeepsGuiEventLoopResponsiveAndWritesReadableExr)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 640;
    constexpr int height = 360;
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
    params.setDurationFrames(48);
    params.setBackgroundColor(ArtifactCore::FloatColor(
        0.15f, 0.25f, 0.4f, 1.0f));
    const auto composition = projects.createComposition(params);
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactSolidLayerInitParams outputLayerParams(
        QStringLiteral("ExrOutputLayer"));
    outputLayerParams.setWidth(width);
    outputLayerParams.setHeight(height);
    outputLayerParams.setColor(ArtifactCore::FloatColor(
        0.15f, 0.25f, 0.4f, 1.0f));
    const auto outputLayer = projects.addLayerToComposition(
        composition.id, outputLayerParams);
    ASSERT_TRUE(outputLayer.success);
    QString failure;
    RenderQueueEventLoopProbe eventLoopProbe;
    ASSERT_TRUE(addAndRenderExr(
        queue, composition.id, QStringLiteral("ExrQueueOutput"),
        isolatedOutputPath(QStringLiteral("render_queue_exr.exr")),
        width, height, &failure, 0, 48, &eventLoopProbe, true))
        << failure.toStdString();
    ASSERT_TRUE(addAndRenderExr(
        queue, composition.id, QStringLiteral("DeepExrQueueOutput"),
        isolatedOutputPath(QStringLiteral("render_queue_deep.exr")),
        width, height, &failure, 0, 1, nullptr, false, true))
        << failure.toStdString();
    EXPECT_TRUE(eventLoopProbe.servicedWhileRendering());
}

TEST(RenderQueueLayerImageIntegrationTest,
     LargeSurfaceEffectQueueWritesEveryFrameAndAppliesEffect)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 1280;
    constexpr int height = 720;
    constexpr int frameCount = 4;
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
    projects.createProject(QStringLiteral("LargeEffectRenderQueueTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams params;
    params.setCompositionName(ArtifactCore::UniString(
        QStringLiteral("LargeEffectQueue")));
    params.setResolution(width, height);
    params.setFrameRate(30.0);
    params.setDurationFrames(frameCount + 1);
    params.setBackgroundColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 1.0f));
    const auto composition = projects.createComposition(params);
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactSolidLayerInitParams layerParams(
        QStringLiteral("LargeSolid"));
    layerParams.setWidth(width);
    layerParams.setHeight(height);
    layerParams.setColor(ArtifactCore::FloatColor(0.18f, 0.28f, 0.38f, 1.0f));
    const auto created = projects.addLayerToComposition(
        composition.id, layerParams);
    ASSERT_TRUE(created.success);
    ASSERT_NE(created.layer.get(), nullptr);

    QString failure;
    const QString baselineOutput = isolatedOutputPath(
        QStringLiteral("large_effect_queue_baseline.png"));
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("LargeEffectQueue"),
        baselineOutput, width, height,
        [](const QImage& image) {
            return image.pixelColor(image.width() / 2, image.height() / 2).alpha() > 0;
        }, &failure, frameCount, QStringLiteral("gpu"), 1))
        << failure.toStdString();

    auto brightness = ArtifactCore::makeShared<Artifact::BrightnessEffect>();
    ASSERT_NE(brightness, nullptr);
    brightness->setBrightness(0.18f);
    created.layer->addEffect(brightness);

    const QString effectedOutput = isolatedOutputPath(
        QStringLiteral("large_effect_queue_brightness.png"));
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("LargeEffectQueue"),
        effectedOutput, width, height,
        [](const QImage& image) {
            return image.pixelColor(image.width() / 2, image.height() / 2).alpha() > 0;
        }, &failure, frameCount, QStringLiteral("gpu"), 1))
        << failure.toStdString();

    const QFileInfo baselineInfo(baselineOutput);
    const QFileInfo effectedInfo(effectedOutput);
    for (int frame = 1; frame <= frameCount; ++frame) {
        const auto framePath = [frame](const QFileInfo& outputInfo) {
            return QDir(outputInfo.absolutePath()).filePath(
                QStringLiteral("%1_%2.png")
                    .arg(outputInfo.completeBaseName())
                    .arg(frame, 4, 10, QLatin1Char('0')));
        };
        const QImage baseline(framePath(baselineInfo));
        const QImage effected(framePath(effectedInfo));
        ASSERT_FALSE(baseline.isNull()) << "missing baseline frame " << frame;
        ASSERT_FALSE(effected.isNull()) << "missing effected frame " << frame;
        ASSERT_EQ(baseline.size(), QSize(width, height));
        ASSERT_EQ(effected.size(), QSize(width, height));

        const QPoint sample(width / 2, height / 2);
        const QColor baselinePixel = baseline.pixelColor(sample);
        const QColor effectedPixel = effected.pixelColor(sample);
        EXPECT_GT(effectedPixel.red(), baselinePixel.red()) << "frame " << frame;
        EXPECT_GT(effectedPixel.green(), baselinePixel.green()) << "frame " << frame;
        EXPECT_GT(effectedPixel.blue(), baselinePixel.blue()) << "frame " << frame;
    }
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
    const auto projectComposition =
        projects.findComposition(particles2D.id).ptr.lock();
    ASSERT_NE(projectComposition, nullptr);
    const auto storedParticleLayer = dynamic_cast<Artifact::ArtifactParticleLayer*>(
        projectComposition->layerById(particle2DLayer.layer->id()).get());
    ASSERT_NE(storedParticleLayer, nullptr);
    EXPECT_EQ(storedParticleLayer, particle2D)
        << "the configured particle layer must belong to the queued composition";
    EXPECT_TRUE(storedParticleLayer->isVisible());
    EXPECT_TRUE(storedParticleLayer->isActiveAt(ArtifactCore::FramePosition(1)));
    ASSERT_FALSE(particle2D->particleSystem()->emitters().empty());
    auto particle2DParams =
        particle2D->particleSystem()->emitters().front()->params();
    particle2DParams.position =
        QVector3D(width * 0.25f, height / 2.0f, 0.0f);
    particle2DParams.rate = 1000.0f;
    particle2DParams.lifeMin = 10.0f;
    particle2DParams.lifeMax = 10.0f;
    particle2DParams.speedMin = 0.0f;
    particle2DParams.speedMax = 0.0f;
    particle2DParams.directionSpread = 0.0f;
    particle2DParams.colorStart = QColor(255, 32, 16, 255);
    particle2DParams.colorMid = QColor(255, 32, 16, 255);
    particle2DParams.colorEnd = QColor(255, 32, 16, 255);
    particle2DParams.texturePath.clear();
    particle2DParams.scaleMin = 12.0f;
    particle2DParams.scaleMax = 12.0f;
    particle2D->clearEmitters();
    ASSERT_NE(particle2D->addEmitter(particle2DParams), nullptr);
    particle2D->particleSystem()->goToFrame(1, 30.0, false);
    ASSERT_GT(particle2D->particleSystem()->totalParticleCount(), 8)
        << "the configured emitter should produce particles before rendering";
    const QImage directParticleFrame = particle2D->renderFrame(
        width, height, 1.0f / 30.0f);
    ASSERT_GT(countPixelsMatching(directParticleFrame, [](QRgb pixel) {
        return qAlpha(pixel) > 200 && qRed(pixel) > 200 &&
               qGreen(pixel) < 80 && qBlue(pixel) < 50;
    }), 0) << "the configured emitter should rasterize outside the render queue";
    const auto serializedParticleLayer = Artifact::ArtifactParticleLayer::fromJson(
        particle2D->toJson());
    auto* restoredParticleLayer = dynamic_cast<Artifact::ArtifactParticleLayer*>(
        serializedParticleLayer.get());
    ASSERT_NE(restoredParticleLayer, nullptr);
    restoredParticleLayer->particleSystem()->goToFrame(1, 30.0, false);
    EXPECT_GT(restoredParticleLayer->particleSystem()->totalParticleCount(), 8)
        << "the serialized emitter should still emit after layer restoration";
    const QString particles2DOutput = isolatedOutputPath(
        QStringLiteral("render_queue_particles_2d.png"));
    ASSERT_TRUE(addAndRender(
        queue, particles2D.id, QStringLiteral("Particles2D"),
        particles2DOutput,
        width, height,
        [](const QImage& image) {
            return countPixelsMatching(image, [](QRgb pixel) {
                return qAlpha(pixel) > 200 && qRed(pixel) > 200 &&
                       qGreen(pixel) < 80 && qBlue(pixel) < 50;
            }) > 0;
        }, &failure, 2, QStringLiteral("cpu"))) << failure.toStdString();

    const QFileInfo particles2DInfo(particles2DOutput);
    const QString particles2DFramePath = QDir(
        particles2DInfo.absolutePath()).filePath(
            QStringLiteral("%1_%2.png")
                .arg(particles2DInfo.completeBaseName())
                .arg(1, 4, 10, QLatin1Char('0')));
    const QImage particles2DBaseline(particles2DFramePath);
    ASSERT_FALSE(particles2DBaseline.isNull());

    const QString particles2DGpuBaselineOutput = isolatedOutputPath(
        QStringLiteral("render_queue_particles_2d_gpu_baseline.png"));
    ASSERT_TRUE(addAndRender(
        queue, particles2D.id, QStringLiteral("Particles2DGpuBaseline"),
        particles2DGpuBaselineOutput, width, height,
        [](const QImage& image) {
            const QRgb background = image.pixel(width - 10, height - 10);
            return countPixelsMatching(image, [background](QRgb pixel) {
                return qAlpha(pixel) > 200 && pixel != background;
            }) > 0;
        }, &failure, 2, QStringLiteral("gpu"))) << failure.toStdString();

    const QFileInfo particles2DGpuBaselineInfo(particles2DGpuBaselineOutput);
    const QString particles2DGpuBaselineFramePath = QDir(
        particles2DGpuBaselineInfo.absolutePath()).filePath(
            QStringLiteral("%1_%2.png")
                .arg(particles2DGpuBaselineInfo.completeBaseName())
                .arg(1, 4, 10, QLatin1Char('0')));
    const QImage particles2DGpuBaseline(particles2DGpuBaselineFramePath);
    ASSERT_FALSE(particles2DGpuBaseline.isNull());

    auto brightness = ArtifactCore::makeShared<Artifact::BrightnessEffect>();
    ASSERT_NE(brightness, nullptr);
    brightness->setBrightness(0.2f);
    brightness->setEnabled(true);
    particle2D->addEffect(brightness);

    const QString particles2DEffectedOutput = isolatedOutputPath(
        QStringLiteral("render_queue_particles_2d_brightness.png"));
    particle2D->clearFrameCache();
    ASSERT_TRUE(addAndRender(
        queue, particles2D.id, QStringLiteral("Particles2DBrightness"),
        particles2DEffectedOutput, width, height,
        [](const QImage& image) {
            return countPixelsMatching(image, [](QRgb pixel) {
                return qAlpha(pixel) > 200 && qRed(pixel) > 200 &&
                       qGreen(pixel) > 50 && qBlue(pixel) > 40;
            }) > 8;
        }, &failure, 2, QStringLiteral("gpu"))) << failure.toStdString();

    const QFileInfo particles2DEffectedInfo(particles2DEffectedOutput);
    const QString particles2DEffectedFramePath = QDir(
        particles2DEffectedInfo.absolutePath()).filePath(
            QStringLiteral("%1_%2.png")
                .arg(particles2DEffectedInfo.completeBaseName())
                .arg(1, 4, 10, QLatin1Char('0')));
    const QImage particles2DEffected(particles2DEffectedFramePath);
    ASSERT_FALSE(particles2DEffected.isNull());
    ASSERT_EQ(particles2DEffected.size(), particles2DGpuBaseline.size());

    int effectedSampleX = -1;
    int effectedSampleY = -1;
    int strongestEffectedRed = 0;
    for (int y = 0; y < particles2DEffected.height(); ++y) {
        for (int x = 0; x < particles2DEffected.width(); ++x) {
            const QRgb pixel = particles2DEffected.pixel(x, y);
            if (qAlpha(pixel) > 200 && qRed(pixel) > strongestEffectedRed &&
                qRed(pixel) > 200 && qGreen(pixel) > 50 && qBlue(pixel) > 40) {
                strongestEffectedRed = qRed(pixel);
                effectedSampleX = x;
                effectedSampleY = y;
            }
        }
    }
    ASSERT_GE(effectedSampleX, 0)
        << "brightness output did not contain a rendered particle";
    const QRgb effectedPixel =
        particles2DEffected.pixel(effectedSampleX, effectedSampleY);
    EXPECT_GT(qGreen(effectedPixel), 50)
        << "Brightness should raise the rendered particle's green channel";
    EXPECT_GT(qBlue(effectedPixel), 40)
        << "Brightness should raise the rendered particle's blue channel";
    EXPECT_EQ(particles2DEffected.pixel(width - 10, height - 10),
              particles2DGpuBaseline.pixel(width - 10, height - 10))
        << "the effect on the particle layer must leave uncovered background "
           "unchanged";

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
     FluidSmokeUsesSharedStateInCpuAndGpuRenderQueue)
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
    projects.createProject(QStringLiteral("RenderQueueSmokeParityTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());
    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("SmokeParity")));
    compositionParams.setResolution(width, height);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(150);
    compositionParams.setBackgroundColor(
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 1.0f));
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactSolidLayerInitParams layerParams(
        QStringLiteral("SmokeEmitter"));
    layerParams.setWidth(width);
    layerParams.setHeight(height);
    layerParams.setColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    const auto created = projects.addLayerToComposition(
        composition.id, layerParams);
    ASSERT_TRUE(created.success);
    const std::pair<const char*, QVariant> properties[] = {
        {"component.fluid.enabled", QVariant(true)},
        {"component.fluid.mode", QVariant(0)},
        {"component.fluid.gridWidth", QVariant(96)},
        {"component.fluid.gridHeight", QVariant(72)},
        {"component.fluid.emitterY", QVariant(0.86)},
        {"component.fluid.emitterRadius", QVariant(0.05)},
        {"component.fluid.emitterDensity", QVariant(0.3)},
        {"component.fluid.emitterVelocityY", QVariant(-1.2)},
        {"component.fluid.smokeColor",
         QVariant(QColor::fromRgbF(0.12f, 0.76f, 0.28f, 1.0f))},
    };
    for (const auto& [path, value] : properties) {
        ASSERT_TRUE(created.layer->setLayerPropertyValue(
            QString::fromLatin1(path), value)) << "property=" << path;
    }

    const auto isGreenSmoke = [](QRgb pixel) {
        return qAlpha(pixel) > 8 && qGreen(pixel) > qRed(pixel) * 1.15 &&
               qGreen(pixel) > qBlue(pixel) * 1.15;
    };
    const QString cpuOutput = isolatedOutputPath(
        QStringLiteral("render_queue_smoke_cpu.png"));
    const QString gpuOutput = isolatedOutputPath(
        QStringLiteral("render_queue_smoke_gpu.png"));
    const QString cpuRepeatOutput = isolatedOutputPath(
        QStringLiteral("render_queue_smoke_cpu_repeat.png"));
    const QString cpuRangeOutput = isolatedOutputPath(
        QStringLiteral("render_queue_smoke_cpu_range.png"));
    const QString gpuRangeOutput = isolatedOutputPath(
        QStringLiteral("render_queue_smoke_gpu_range.png"));
    QString failure;
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("SmokeParity"), cpuOutput,
        width, height,
        [&isGreenSmoke](const QImage& image) {
            return countPixelsMatching(image, isGreenSmoke) > 10;
        }, &failure, 25, QStringLiteral("cpu"), 0)) << failure.toStdString();
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("SmokeParity"), gpuOutput,
        width, height,
        [&isGreenSmoke](const QImage& image) {
            return countPixelsMatching(image, isGreenSmoke) > 10;
        }, &failure, 25, QStringLiteral("gpu"), 0)) << failure.toStdString();
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("SmokeParity"), cpuRepeatOutput,
        width, height,
        [&isGreenSmoke](const QImage& image) {
            return countPixelsMatching(image, isGreenSmoke) > 10;
        }, &failure, 25, QStringLiteral("cpu_repeat"), 0))
        << failure.toStdString();
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("SmokeParity"), cpuRangeOutput,
        width, height,
        [&isGreenSmoke](const QImage& image) {
            return countPixelsMatching(image, isGreenSmoke) > 10;
        }, &failure, 25, QStringLiteral("cpu_range"), 24))
        << failure.toStdString();
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("SmokeParity"), gpuRangeOutput,
        width, height,
        [&isGreenSmoke](const QImage& image) {
            return countPixelsMatching(image, isGreenSmoke) > 10;
        }, &failure, 25, QStringLiteral("gpu_range"), 24))
        << failure.toStdString();

    const auto readFrame = [](const QString& outputBase, const int frameNumber) {
        const QFileInfo outputInfo(outputBase);
        const QString framePath = QDir(outputInfo.absolutePath()).filePath(
            QStringLiteral("%1_%2.png")
                .arg(outputInfo.completeBaseName())
                .arg(frameNumber, 4, 10, QLatin1Char('0')));
        return QImage(framePath);
    };
    const QImage cpuImage = readFrame(cpuOutput, 24);
    const QImage gpuImage = readFrame(gpuOutput, 24);
    const QImage cpuRepeatImage = readFrame(cpuRepeatOutput, 24);
    const QImage cpuRangeImage = readFrame(cpuRangeOutput, 24);
    const QImage gpuRangeImage = readFrame(gpuRangeOutput, 24);
    ASSERT_FALSE(cpuImage.isNull());
    ASSERT_FALSE(gpuImage.isNull());
    ASSERT_FALSE(cpuRepeatImage.isNull());
    ASSERT_FALSE(cpuRangeImage.isNull());
    ASSERT_FALSE(gpuRangeImage.isNull());
    ASSERT_EQ(cpuImage.size(), cpuRepeatImage.size());

    const auto compositionObject = projects.findComposition(composition.id)
                                       .ptr.lock();
    ASSERT_NE(compositionObject, nullptr);
    Artifact::DiligentDeviceManager viewDevice;
    viewDevice.initializeHeadless();
    ASSERT_NE(viewDevice.device(), nullptr);
    Artifact::OffscreenCompositionRenderer viewRenderer(
        viewDevice.device(), viewDevice.immediateContext(), width, height);
    compositionObject->resetLayerComponentSimulation();
    for (int frame = 0; frame <= 24; ++frame) {
        const ArtifactCore::FramePosition position(frame);
        compositionObject->goToFrame(frame);
        compositionObject->evaluateLayerComponentSimulation(position, false);
    }
    const ArtifactCore::FramePosition previewPosition(24);
    viewRenderer.renderFrame(previewPosition, compositionObject.get());
    auto* previewRenderer = viewRenderer.renderer();
    ASSERT_NE(previewRenderer, nullptr);
    previewRenderer->setClearColor(compositionObject->backgroundColor());
    previewRenderer->clear();
    Artifact::drawLayerForCompositionView(created.layer.get(), previewRenderer);
    previewRenderer->flush();
    const QImage previewImage = previewRenderer->readbackToImage(
        ArtifactCore::SurfaceChannelOrder::BGRA);
    ASSERT_FALSE(previewImage.isNull());
    ASSERT_EQ(previewImage.size(), gpuImage.size());
    const QString viewOutput = isolatedOutputPath(
        QStringLiteral("render_queue_smoke_view.png"));
    EXPECT_TRUE(previewImage.save(viewOutput, "PNG"));

    struct SmokeCentroid final {
        int count = 0;
        double x = 0.0;
        double y = 0.0;
    };
    const auto centroid = [&isGreenSmoke](const QImage& image) {
        SmokeCentroid result;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (!isGreenSmoke(image.pixel(x, y))) continue;
                ++result.count;
                result.x += x;
                result.y += y;
            }
        }
        if (result.count > 0) {
            result.x /= result.count;
            result.y /= result.count;
        }
        return result;
    };
    const SmokeCentroid cpuSmoke = centroid(cpuImage);
    const SmokeCentroid gpuSmoke = centroid(gpuImage);
    const SmokeCentroid cpuRepeatSmoke = centroid(cpuRepeatImage);
    const SmokeCentroid cpuRangeSmoke = centroid(cpuRangeImage);
    const SmokeCentroid gpuRangeSmoke = centroid(gpuRangeImage);
    const SmokeCentroid previewSmoke = centroid(previewImage);
    int previewQueueChangedPixels = 0;
    for (int y = 0; y < previewImage.height(); ++y) {
        for (int x = 0; x < previewImage.width(); ++x) {
            if (previewImage.pixel(x, y) != gpuImage.pixel(x, y)) {
                ++previewQueueChangedPixels;
            }
        }
    }
    EXPECT_GT(cpuSmoke.count, 10);
    EXPECT_GT(gpuSmoke.count, 10);
    EXPECT_GT(cpuRepeatSmoke.count, 10);
    EXPECT_GT(cpuRangeSmoke.count, 10);
    EXPECT_GT(gpuRangeSmoke.count, 10);
    EXPECT_GT(previewSmoke.count, 10);
    EXPECT_EQ(previewQueueChangedPixels, 0)
        << "Composition View GPU wrapper and GPU Render Queue should match pixel-for-pixel";
    EXPECT_NEAR(cpuSmoke.x, gpuSmoke.x, 12.0);
    EXPECT_NEAR(cpuSmoke.y, gpuSmoke.y, 12.0);
    EXPECT_NEAR(cpuSmoke.x, cpuRepeatSmoke.x, 0.5);
    EXPECT_NEAR(cpuSmoke.y, cpuRepeatSmoke.y, 0.5);
    EXPECT_NEAR(cpuSmoke.x, cpuRangeSmoke.x, 0.5);
    EXPECT_NEAR(cpuSmoke.y, cpuRangeSmoke.y, 0.5);
    EXPECT_LE(std::abs(cpuSmoke.count - cpuRangeSmoke.count),
              std::max(1, cpuSmoke.count / 50));
    EXPECT_NEAR(gpuSmoke.x, gpuRangeSmoke.x, 12.0);
    EXPECT_NEAR(gpuSmoke.y, gpuRangeSmoke.y, 12.0);
    EXPECT_LE(std::abs(cpuSmoke.count - cpuRepeatSmoke.count),
              std::max(1, cpuSmoke.count / 50));

    // Moving the source in layer coordinates must also move the visible plume
    // in the Composition View, rather than only changing the stored property.
    ASSERT_TRUE(created.layer->setLayerPropertyValue(
        QStringLiteral("component.fluid.emitterY"), QVariant(0.60)));
    compositionObject->resetLayerComponentSimulation();
    for (int frame = 0; frame <= 24; ++frame) {
        compositionObject->goToFrame(frame);
        compositionObject->evaluateLayerComponentSimulation(
            ArtifactCore::FramePosition(frame), false);
    }
    previewRenderer->setClearColor(compositionObject->backgroundColor());
    previewRenderer->clear();
    Artifact::drawLayerForCompositionView(created.layer.get(), previewRenderer);
    previewRenderer->flush();
    const QImage movedImage = previewRenderer->readbackToImage(
        ArtifactCore::SurfaceChannelOrder::BGRA);
    ASSERT_FALSE(movedImage.isNull());
    const SmokeCentroid movedSmoke = centroid(movedImage);
    ASSERT_GT(movedSmoke.count, 10);
    EXPECT_LT(movedSmoke.y, previewSmoke.y - 10.0);

    for (int frame = 25; frame <= 120; ++frame) {
        compositionObject->goToFrame(frame);
        compositionObject->evaluateLayerComponentSimulation(
            ArtifactCore::FramePosition(frame), false);
    }
    previewRenderer->setClearColor(compositionObject->backgroundColor());
    previewRenderer->clear();
    Artifact::drawLayerForCompositionView(created.layer.get(), previewRenderer);
    previewRenderer->flush();
    const QImage longImage = previewRenderer->readbackToImage(
        ArtifactCore::SurfaceChannelOrder::BGRA);
    ASSERT_FALSE(longImage.isNull());
    const SmokeCentroid longSmoke = centroid(longImage);
    ASSERT_GT(longSmoke.count, 10);
    int topPixels = 0;
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < longImage.width(); ++x) {
            topPixels += isGreenSmoke(longImage.pixel(x, y)) ? 1 : 0;
        }
    }
    GTEST_LOG_(INFO) << "2D smoke frame 120 centroid=" << longSmoke.y
                     << " top-five-row pixels=" << topPixels
                     << " visible=" << longSmoke.count;

    ASSERT_TRUE(created.layer->setLayerPropertyValue(
        QStringLiteral("component.fluid.densityDissipationPerSecond"),
        QVariant(5.0)));
    compositionObject->resetLayerComponentSimulation();
    for (int frame = 0; frame <= 120; ++frame) {
        compositionObject->goToFrame(frame);
        compositionObject->evaluateLayerComponentSimulation(
            ArtifactCore::FramePosition(frame), false);
    }
    previewRenderer->setClearColor(compositionObject->backgroundColor());
    previewRenderer->clear();
    Artifact::drawLayerForCompositionView(created.layer.get(), previewRenderer);
    previewRenderer->flush();
    const QImage dissipatedImage = previewRenderer->readbackToImage(
        ArtifactCore::SurfaceChannelOrder::BGRA);
    ASSERT_FALSE(dissipatedImage.isNull());
    const SmokeCentroid dissipatedSmoke = centroid(dissipatedImage);
    int dissipatedTopPixels = 0;
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < dissipatedImage.width(); ++x) {
            dissipatedTopPixels += isGreenSmoke(dissipatedImage.pixel(x, y)) ? 1 : 0;
        }
    }
    GTEST_LOG_(INFO) << "2D smoke frame 120 higher dissipation centroid="
                     << dissipatedSmoke.y << " top-five-row pixels="
                     << dissipatedTopPixels << " visible=" << dissipatedSmoke.count;

    ASSERT_TRUE(created.layer->setLayerPropertyValue(
        QStringLiteral("component.fluid.densityDissipationPerSecond"), QVariant(2.5)));
    created.layer->setInPoint(ArtifactCore::FramePosition(10));
    const auto renderAfterSeek = [&](bool drawDuringPlayback, int explicitStart) {
        compositionObject->resetLayerComponentSimulation();
        for (int frame = explicitStart > 0 ? explicitStart : 10; frame <= 24; ++frame) {
            compositionObject->goToFrame(frame);
            if (explicitStart > 0) {
                created.layer->advanceFluidSmokeSimulation(frame, 30.0, explicitStart);
            } else {
                compositionObject->evaluateLayerComponentSimulation(
                    ArtifactCore::FramePosition(frame), false);
            }
            if (drawDuringPlayback) {
                previewRenderer->clear();
                Artifact::drawLayerForCompositionView(created.layer.get(), previewRenderer);
                previewRenderer->flush();
            }
        }
        compositionObject->goToFrame(16);
        if (explicitStart > 0) {
            created.layer->advanceFluidSmokeSimulation(16, 30.0, explicitStart);
        } else {
            compositionObject->evaluateLayerComponentSimulation(
                ArtifactCore::FramePosition(16), false);
        }
        previewRenderer->clear();
        Artifact::drawLayerForCompositionView(created.layer.get(), previewRenderer);
        previewRenderer->flush();
        return previewRenderer->readbackToImage(ArtifactCore::SurfaceChannelOrder::BGRA);
    };
    const QImage seekReference = renderAfterSeek(false, 0);
    const QImage seekAfterDrawing = renderAfterSeek(true, 0);
    ASSERT_FALSE(seekReference.isNull());
    ASSERT_FALSE(seekAfterDrawing.isNull());
    ASSERT_GT(centroid(seekReference).count, 10);
    EXPECT_EQ(seekAfterDrawing, seekReference)
        << "drawing must not invalidate smoke checkpoints for a nonzero layer in-point";
    const QImage customStartReference = renderAfterSeek(false, 12);
    const QImage customStartDrawn = renderAfterSeek(true, 12);
    ASSERT_FALSE(customStartReference.isNull());
    ASSERT_GT(centroid(customStartReference).count, 10);
    EXPECT_EQ(customStartDrawn, customStartReference)
        << "drawing must retain the explicitly requested simulation start";
    const auto softwareAfterSeek = [&](bool drawDuringPlayback) {
        compositionObject->resetLayerComponentSimulation();
        QImage surface(width, height, QImage::Format_ARGB32_Premultiplied);
        for (int frame = 12; frame <= 24; ++frame) {
            compositionObject->goToFrame(frame);
            created.layer->advanceFluidSmokeSimulation(frame, 30.0, 12);
            if (drawDuringPlayback) {
                surface.fill(Qt::transparent);
                created.layer->compositeFluidSmokeSoftwareSurface(surface);
            }
        }
        compositionObject->goToFrame(16);
        created.layer->advanceFluidSmokeSimulation(16, 30.0, 12);
        surface.fill(Qt::transparent);
        created.layer->compositeFluidSmokeSoftwareSurface(surface);
        return surface;
    };
    const QImage softwareReference = softwareAfterSeek(false);
    const QImage softwareAfterDrawing = softwareAfterSeek(true);
    ASSERT_GT(centroid(softwareReference).count, 10);
    EXPECT_EQ(softwareAfterDrawing, softwareReference)
        << "software fallback must retain the explicitly requested simulation start";
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

    QString failure;
    ASSERT_TRUE(addAndRenderTwoTextFrames(
        queue, composition.id, QStringLiteral("AnimatedText"),
        isolatedOutputPath(QStringLiteral("render_queue_animated_text.png")),
        width, height, &failure)) << failure.toStdString();
}

TEST(RenderQueueLayerImageIntegrationTest,
     AnimatedLayerTransformMovesPixelsAcrossGpuRenderedFrames)
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
    projects.createProject(QStringLiteral("AnimatedTransformGpuTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());
    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("AnimatedTransform")));
    compositionParams.setResolution(width, height);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(3);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactSolidLayerInitParams layerParams(QStringLiteral("MovingRed"));
    layerParams.setWidth(48);
    layerParams.setHeight(48);
    layerParams.setColor(ArtifactCore::FloatColor(1.0f, 0.0f, 0.0f, 1.0f));
    const auto layerResult = projects.addLayerToComposition(
        composition.id, layerParams);
    ASSERT_TRUE(layerResult.success);
    ASSERT_NE(layerResult.layer.get(), nullptr);

    auto positionX = layerResult.layer->getProperty(
        QStringLiteral("transform.position.x"));
    ASSERT_NE(positionX.get(), nullptr);
    positionX->clearKeyFrames();
    positionX->setAnimatable(true);
    const auto keyScale = layerResult.layer->keyframeTimeScale();
    positionX->addKeyFrame(ArtifactCore::RationalTime(1, keyScale), 72.0,
                           ArtifactCore::InterpolationType::Linear);
    positionX->addKeyFrame(ArtifactCore::RationalTime(2, keyScale), 220.0,
                           ArtifactCore::InterpolationType::Linear);

    QString failure;
    const QString outputBasePath = isolatedOutputPath(
        QStringLiteral("animated_transform_gpu.png"));
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("AnimatedTransform"),
        outputBasePath, width, height,
        [](const QImage& image) {
            return countPixelsMatching(image, [](QRgb pixel) {
                return qAlpha(pixel) > 220 && qRed(pixel) > 220 &&
                       qGreen(pixel) < 40 && qBlue(pixel) < 40;
            }) > 100;
        }, &failure, 2, QStringLiteral("gpu"), 1))
        << failure.toStdString();

    const QFileInfo outputInfo(outputBasePath);
    const auto framePath = [&](int frame) {
        return QDir(outputInfo.absolutePath()).filePath(
            QStringLiteral("%1_%2.png")
                .arg(outputInfo.completeBaseName())
                .arg(frame, 4, 10, QLatin1Char('0')));
    };
    const QImage firstFrame(framePath(1));
    const QImage secondFrame(framePath(2));
    ASSERT_FALSE(firstFrame.isNull());
    ASSERT_FALSE(secondFrame.isNull());

    const auto redPixelCenterX = [](const QImage& image) {
        qint64 sumX = 0;
        qint64 count = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QRgb pixel = image.pixel(x, y);
                if (qAlpha(pixel) > 220 && qRed(pixel) > 220 &&
                    qGreen(pixel) < 40 && qBlue(pixel) < 40) {
                    sumX += x;
                    ++count;
                }
            }
        }
        return count > 0 ? static_cast<double>(sumX) / count : -1.0;
    };
    const double firstCenter = redPixelCenterX(firstFrame);
    const double secondCenter = redPixelCenterX(secondFrame);
    EXPECT_GT(firstCenter, 0.0);
    EXPECT_GT(secondCenter, firstCenter + 80.0);
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
        QByteArray failGpuFrame = qgetenv(
            "ARTIFACT_XPU_TEST_FAIL_GPU_FRAME");
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
            if (failGpuFrame.isNull()) {
                qunsetenv("ARTIFACT_XPU_TEST_FAIL_GPU_FRAME");
            } else {
                qputenv("ARTIFACT_XPU_TEST_FAIL_GPU_FRAME", failGpuFrame);
            }
        }
    } restoreEnvironment;
    qputenv("ARTIFACT_XPU", "mixed=on,asyncseq=manager");
    qunsetenv("ARTIFACT_XPU_TEST_FAIL_GPU_FRAME");
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
    // Fail the first seeded GPU frame once, then require CPU fallback to
    // produce the same pixels as the sequential CPU baseline.
    qputenv("ARTIFACT_XPU_TEST_FAIL_GPU_FRAME", "1");

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
    ASSERT_TRUE(joined.contains(
        QStringLiteral("XPU test failure injection fired")))
        << "The test-only GPU failure seam did not fire: "
        << joined.toStdString();

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
        if (frameNumber == 1) {
            EXPECT_TRUE(xpuFrame == cpuFrame)
                << "Injected GPU failure must fall back to the CPU frame";
        }
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
    EXPECT_EQ(asyncSequenceMatch.captured(1), QStringLiteral("true"))
        << joined.toStdString();
    EXPECT_TRUE(joined.contains(QStringLiteral(
        "XPU async sequence manager requested; using bounded writer futures")))
        << "Manager mode must retain bounded writer backpressure: "
        << joined.toStdString();
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
    const int cpuFrameCount = cpuMatch.captured(1).toInt();
    const int gpuFrameCount = gpuMatch.captured(1).toInt();
    const int cpuFallbackFrameCount = cpuFallbackMatch.captured(1).toInt();
    const int gpuFallbackFrameCount = gpuFallbackMatch.captured(1).toInt();
    EXPECT_EQ(cpuFallbackFrameCount, 1) << joined.toStdString();
    EXPECT_EQ(gpuFallbackFrameCount, 0) << joined.toStdString();
    EXPECT_LE(cpuFallbackFrameCount, cpuFrameCount)
        << "CPU fallback frames must be a subset of completed CPU frames: "
        << joined.toStdString();
    EXPECT_LE(gpuFallbackFrameCount, gpuFrameCount)
        << "GPU fallback frames must be a subset of completed GPU frames: "
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
    EXPECT_LE(primaryGpuFramesMatch.captured(1).toInt(), gpuFrameCount)
        << "Primary GPU frames must be included in completed GPU frames: "
        << joined.toStdString();
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
     TileRenderSettingsExposeModeAndClampMinimumSize)
{
    auto* queue = Artifact::ArtifactRenderQueueService::instance();
    ASSERT_NE(queue, nullptr);
    const auto previousMode = queue->tileRenderMode();
    const int previousTileSize = queue->tileSize();
    struct RestoreTileSettings final {
        Artifact::ArtifactRenderQueueService* queue;
        Artifact::ArtifactRenderQueueService::TileRenderMode mode;
        int tileSize;
        ~RestoreTileSettings()
        {
            queue->setTileRenderMode(mode);
            queue->setTileSize(tileSize);
        }
    } restore{queue, previousMode, previousTileSize};

    queue->setTileRenderMode(
        Artifact::ArtifactRenderQueueService::TileRenderMode::Tiled);
    EXPECT_EQ(queue->tileRenderMode(),
              Artifact::ArtifactRenderQueueService::TileRenderMode::Tiled);
    queue->setTileRenderMode(
        Artifact::ArtifactRenderQueueService::TileRenderMode::FullFrame);
    EXPECT_EQ(queue->tileRenderMode(),
              Artifact::ArtifactRenderQueueService::TileRenderMode::FullFrame);

    queue->setTileSize(512);
    EXPECT_EQ(queue->tileSize(), 512);
    queue->setTileSize(64);
    EXPECT_EQ(queue->tileSize(), 64);
    queue->setTileSize(1);
    EXPECT_EQ(queue->tileSize(), 64);
    queue->setTileSize(-128);
    EXPECT_EQ(queue->tileSize(), 64);
}

TEST(RenderQueueLayerImageIntegrationTest,
     HalftoneGenericGpuNodeRendersRepeatableDotPattern)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 13;
    constexpr int height = 11;
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
    projects.createProject(QStringLiteral("RenderQueueHalftoneGpuTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams params;
    params.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("HalftoneGpu")));
    params.setResolution(width, height);
    params.setFrameRate(30.0);
    params.setDurationFrames(2);
    const auto composition = projects.createComposition(params);
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactSolidLayerInitParams layerParams(
        QStringLiteral("HalftoneGpuLayer"));
    layerParams.setWidth(width);
    layerParams.setHeight(height);
    layerParams.setColor(ArtifactCore::FloatColor(
        1.0f, 1.0f, 1.0f, 1.0f));
    const auto layer = projects.addLayerToComposition(
        composition.id, layerParams);
    ASSERT_TRUE(layer.success);

    auto halftone =
        ArtifactCore::makeShared<Artifact::ArtifactHalftoneEffect>();
    ASSERT_NE(halftone, nullptr);
    halftone->setComputeMode(Artifact::ComputeMode::GPU);
    ASSERT_TRUE(halftone->supportsGPU());
    layer.layer->addEffect(halftone);

    const QString outputPath = isolatedOutputPath(
        QStringLiteral("render_queue_halftone_gpu.png"));
    const QString repeatPath = isolatedOutputPath(
        QStringLiteral("render_queue_halftone_gpu_repeat.png"));
    QString failure;
    const auto validImage = [](const QImage& image) {
        return !image.isNull();
    };
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("HalftoneGpu"), outputPath,
        width, height, validImage, &failure, 1, QStringLiteral("gpu"), 0))
        << failure.toStdString();
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("HalftoneGpu"), repeatPath,
        width, height, validImage, &failure, 1, QStringLiteral("gpu"), 0))
        << failure.toStdString();

    const auto framePath = [](const QString& basePath) {
        const QFileInfo info(basePath);
        return QDir(info.absolutePath()).filePath(
            QStringLiteral("%1_%2.png")
                .arg(info.completeBaseName())
                .arg(0, 4, 10, QLatin1Char('0')));
    };
    const QImage output(framePath(outputPath));
    const QImage repeat(framePath(repeatPath));
    ASSERT_EQ(output.size(), QSize(width, height));
    ASSERT_EQ(repeat.size(), QSize(width, height));
    EXPECT_EQ(output, repeat)
        << "GPU Halftone output must repeat for the same input and frame";
    const auto isBlack = [&output](int x, int y) {
        const QColor pixel = output.pixelColor(x, y);
        return pixel.red() < 8 && pixel.green() < 8 && pixel.blue() < 8;
    };
    const auto isWhite = [&output](int x, int y) {
        const QColor pixel = output.pixelColor(x, y);
        return pixel.red() > 247 && pixel.green() > 247 && pixel.blue() > 247;
    };
    EXPECT_TRUE(isBlack(4, 4))
        << "the center of a full-white 8x8 cell must be inside its dot";
    EXPECT_TRUE(isWhite(0, 4))
        << "the radius boundary must remain outside the strict dot test";
    EXPECT_TRUE(isBlack(12, 10))
        << "the partial bottom-right cell must retain its local dot pattern";
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const QColor pixel = output.pixelColor(x, y);
            EXPECT_TRUE(isBlack(x, y) || isWhite(x, y))
                << "Halftone output must be binary at " << x << "," << y;
            EXPECT_EQ(pixel.alpha(), 255)
                << "Halftone output must be opaque at " << x << "," << y;
        }
    }
}

TEST(RenderQueueLayerImageIntegrationTest,
     GlitchGenericGpuNodeIsRepeatableDistinctAndPreservesAlpha)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 13;
    constexpr int height = 11;
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
    projects.createProject(QStringLiteral("RenderQueueGlitchGpuTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams params;
    params.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("GlitchGpu")));
    params.setResolution(width, height);
    params.setFrameRate(30.0);
    params.setDurationFrames(2);
    params.setBackgroundColor(ArtifactCore::FloatColor(
        0.0f, 0.0f, 0.0f, 0.0f));
    const auto composition = projects.createComposition(params);
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactSolidLayerInitParams layerParams(
        QStringLiteral("GlitchGpuLayer"));
    layerParams.setWidth(width);
    layerParams.setHeight(height);
    layerParams.setFillType(Artifact::ArtifactSolidFillType::LinearGradient);
    layerParams.setGradientStartColor(ArtifactCore::FloatColor(
        0.06f, 0.76f, 0.14f, 0.6f));
    layerParams.setGradientEndColor(ArtifactCore::FloatColor(
        0.91f, 0.18f, 0.72f, 0.6f));
    layerParams.setGradientAngleDegrees(0.0f);
    const auto layer = projects.addLayerToComposition(
        composition.id, layerParams);
    ASSERT_TRUE(layer.success);

    const QString baselinePath = isolatedOutputPath(
        QStringLiteral("render_queue_glitch_gpu_baseline.png"));
    const QString effectedPath = isolatedOutputPath(
        QStringLiteral("render_queue_glitch_gpu.png"));
    const QString repeatPath = isolatedOutputPath(
        QStringLiteral("render_queue_glitch_gpu_repeat.png"));
    const QString cpuReferencePath = isolatedOutputPath(
        QStringLiteral("render_queue_glitch_gpu_cpu_reference.png"));
    QString failure;
    const auto validImage = [](const QImage& image) {
        return !image.isNull();
    };
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("GlitchGpu"), baselinePath,
        width, height, validImage, &failure, 1, QStringLiteral("gpu"), 0))
        << failure.toStdString();

    auto glitch = ArtifactCore::makeShared<Artifact::ArtifactGlitchEffect>();
    ASSERT_NE(glitch, nullptr);
    glitch->setComputeMode(Artifact::ComputeMode::GPU);
    ASSERT_TRUE(glitch->supportsGPU());
    layer.layer->addEffect(glitch);
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("GlitchGpu"), effectedPath,
        width, height, validImage, &failure, 1, QStringLiteral("gpu"), 0))
        << failure.toStdString();
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("GlitchGpu"), repeatPath,
        width, height, validImage, &failure, 1, QStringLiteral("gpu"), 0))
        << failure.toStdString();
    glitch->setComputeMode(Artifact::ComputeMode::CPU);
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("GlitchGpu"), cpuReferencePath,
        width, height, validImage, &failure, 1, QStringLiteral("gpu"), 0))
        << failure.toStdString();

    const auto framePath = [](const QString& basePath) {
        const QFileInfo info(basePath);
        return QDir(info.absolutePath()).filePath(
            QStringLiteral("%1_%2.png")
                .arg(info.completeBaseName())
                .arg(0, 4, 10, QLatin1Char('0')));
    };
    const QImage baseline(framePath(baselinePath));
    const QImage effected(framePath(effectedPath));
    const QImage repeat(framePath(repeatPath));
    const QImage cpuReference(framePath(cpuReferencePath));
    ASSERT_EQ(baseline.size(), QSize(width, height));
    ASSERT_EQ(effected.size(), QSize(width, height));
    ASSERT_EQ(repeat.size(), QSize(width, height));
    ASSERT_EQ(cpuReference.size(), QSize(width, height));
    const QColor centerSource = baseline.pixelColor(width / 2, height / 2);
    EXPECT_GT(centerSource.alpha(), 0);
    EXPECT_LT(centerSource.alpha(), 255)
        << "the fixture must retain translucent source pixels";
    EXPECT_NE(baseline.pixelColor(2, height / 2),
              baseline.pixelColor(width - 3, height / 2))
        << "the fixture must expose horizontal color shifts";
    EXPECT_NE(effected, baseline)
        << "GPU Glitch must change the rendered horizontal color pattern";
    EXPECT_EQ(effected, repeat)
        << "GPU Glitch must repeat for the same source frame";
    EXPECT_NE(effected, cpuReference)
        << "GPU mode must use the shader's independent row/shift sequence";
    bool changedRgb = false;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const QColor sourcePixel = baseline.pixelColor(x, y);
            const QColor outputPixel = effected.pixelColor(x, y);
            changedRgb = changedRgb ||
                outputPixel.red() != sourcePixel.red() ||
                outputPixel.green() != sourcePixel.green() ||
                outputPixel.blue() != sourcePixel.blue();
            EXPECT_EQ(outputPixel.alpha(), sourcePixel.alpha())
                << "GPU Glitch must preserve alpha at " << x << "," << y;
        }
    }
    EXPECT_TRUE(changedRgb);
}

TEST(RenderQueueLayerImageIntegrationTest,
     OldTvGenericGpuNodeAppliesScanlineAndPreservesTranslucentAlphaRepeatably)
{
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    constexpr int width = 128;
    constexpr int height = 80;
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
    projects.createProject(QStringLiteral("RenderQueueOldTvGpuTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams params;
    params.setCompositionName(ArtifactCore::UniString(
        QStringLiteral("OldTvGpu")));
    params.setResolution(width, height);
    params.setFrameRate(30.0);
    params.setDurationFrames(2);
    params.setBackgroundColor(ArtifactCore::FloatColor(
        0.0f, 0.0f, 0.0f, 0.0f));
    const auto composition = projects.createComposition(params);
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactSolidLayerInitParams layerParams(
        QStringLiteral("OldTvGpuLayer"));
    layerParams.setWidth(width);
    layerParams.setHeight(height);
    layerParams.setFillType(Artifact::ArtifactSolidFillType::LinearGradient);
    layerParams.setGradientStartColor(ArtifactCore::FloatColor(
        0.05f, 0.12f, 0.85f, 0.55f));
    layerParams.setGradientEndColor(ArtifactCore::FloatColor(
        0.92f, 0.18f, 0.04f, 0.55f));
    layerParams.setGradientAngleDegrees(0.0f);
    const auto layer = projects.addLayerToComposition(
        composition.id, layerParams);
    ASSERT_TRUE(layer.success);

    const QString baselinePath = isolatedOutputPath(
        QStringLiteral("render_queue_old_tv_gpu_baseline.png"));
    const QString effectedPath = isolatedOutputPath(
        QStringLiteral("render_queue_old_tv_gpu_effect.png"));
    const QString repeatedEffectPath = isolatedOutputPath(
        QStringLiteral("render_queue_old_tv_gpu_effect_repeat.png"));
    const QString cpuReferencePath = isolatedOutputPath(
        QStringLiteral("render_queue_old_tv_gpu_cpu_reference.png"));
    QString failure;
    const auto validImage = [](const QImage& image) {
        return !image.isNull();
    };
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("OldTvGpu"), baselinePath,
        width, height, validImage, &failure, 1, QStringLiteral("gpu"), 0))
        << failure.toStdString();

    auto oldTv = ArtifactCore::makeShared<Artifact::ArtifactOldTVEffect>();
    ASSERT_NE(oldTv, nullptr);
    oldTv->setComputeMode(Artifact::ComputeMode::GPU);
    ASSERT_TRUE(oldTv->supportsGPU());
    layer.layer->addEffect(oldTv);
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("OldTvGpu"), effectedPath,
        width, height, validImage, &failure, 1, QStringLiteral("gpu"), 0))
        << failure.toStdString();
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("OldTvGpu"), repeatedEffectPath,
        width, height, validImage, &failure, 1, QStringLiteral("gpu"), 0))
        << failure.toStdString();
    oldTv->setComputeMode(Artifact::ComputeMode::CPU);
    ASSERT_TRUE(addAndRender(
        queue, composition.id, QStringLiteral("OldTvGpu"), cpuReferencePath,
        width, height, validImage, &failure, 1, QStringLiteral("gpu"), 0))
        << failure.toStdString();

    const auto framePath = [](const QString& basePath) {
        const QFileInfo info(basePath);
        return QDir(info.absolutePath()).filePath(
            QStringLiteral("%1_%2.png")
                .arg(info.completeBaseName())
                .arg(0, 4, 10, QLatin1Char('0')));
    };
    const QImage baseline(framePath(baselinePath));
    const QImage effected(framePath(effectedPath));
    const QImage repeatedEffect(framePath(repeatedEffectPath));
    const QImage cpuReference(framePath(cpuReferencePath));
    ASSERT_EQ(baseline.size(), QSize(width, height));
    ASSERT_EQ(effected.size(), QSize(width, height));
    ASSERT_EQ(repeatedEffect.size(), QSize(width, height));
    ASSERT_EQ(cpuReference.size(), QSize(width, height));
    const int centerAlpha = baseline.pixelColor(width / 2, height / 2).alpha();
    EXPECT_GT(centerAlpha, 0);
    EXPECT_LT(centerAlpha, 255)
        << "fixture must exercise genuinely translucent source pixels";
    EXPECT_NE(effected, baseline)
        << "Old TV generic GPU dispatch must affect rendered RGB pixels";
    EXPECT_EQ(effected, repeatedEffect)
        << "Old TV GPU output must repeat exactly for the same source frame";
    EXPECT_NE(effected, cpuReference)
        << "GPU mode must execute its distinct shader reference instead of silently using CPU output";
    bool gpuJitterMovesGradient = false;
    for (int y = 0; y < height && !gpuJitterMovesGradient; ++y) {
        for (int x = 5; x + 5 < width; ++x) {
            const QColor sourcePixel = baseline.pixelColor(x, y);
            const QColor outputPixel = effected.pixelColor(x, y);
            const auto differsFrom = [](const QColor& lhs, const QColor& rhs) {
                return std::abs(lhs.red() - rhs.red()) > 12 ||
                       std::abs(lhs.green() - rhs.green()) > 12 ||
                       std::abs(lhs.blue() - rhs.blue()) > 12;
            };
            if (differsFrom(outputPixel, sourcePixel)) {
                for (int shift = 1; shift <= 5; ++shift) {
                    const QColor leftPixel = baseline.pixelColor(x - shift, y);
                    const QColor rightPixel = baseline.pixelColor(x + shift, y);
                    if (differsFrom(sourcePixel, leftPixel) &&
                        differsFrom(sourcePixel, rightPixel) &&
                        (!differsFrom(outputPixel, leftPixel) ||
                         !differsFrom(outputPixel, rightPixel))) {
                        gpuJitterMovesGradient = true;
                        break;
                    }
                }
                if (gpuJitterMovesGradient) break;
            }
        }
    }
    EXPECT_TRUE(gpuJitterMovesGradient)
        << "GPU Old TV jitter must sample a horizontally shifted gradient row";
    const QColor scanlineSource = baseline.pixelColor(width / 2, 40);
    const QColor scanlineOutput = effected.pixelColor(width / 2, 40);
    const QColor adjacentRowSource = baseline.pixelColor(width / 2, 41);
    const QColor adjacentRowOutput = effected.pixelColor(width / 2, 41);
    EXPECT_LT(scanlineOutput.red(), scanlineSource.red())
        << "GPU Old TV must darken every fourth scanline red";
    EXPECT_LT(scanlineOutput.green(), scanlineSource.green())
        << "GPU Old TV must darken every fourth scanline green";
    EXPECT_LT(scanlineOutput.blue(), scanlineSource.blue())
        << "GPU Old TV must darken every fourth scanline blue";
    EXPECT_NEAR(adjacentRowOutput.red(), adjacentRowSource.red(), 5)
        << "GPU Old TV must leave non-scanline red near its source value";
    EXPECT_NEAR(adjacentRowOutput.green(), adjacentRowSource.green(), 5)
        << "GPU Old TV must leave non-scanline green near its source value";
    EXPECT_NEAR(adjacentRowOutput.blue(), adjacentRowSource.blue(), 5)
        << "GPU Old TV must leave non-scanline blue near its source value";
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            EXPECT_EQ(effected.pixelColor(x, y).alpha(),
                      baseline.pixelColor(x, y).alpha())
                << "GPU Old TV must preserve alpha at " << x << "," << y;
        }
    }
}

TEST(RenderQueueLayerImageIntegrationTest,
     VideoEncoderBackendAliasesNormalizeToStablePublicNames)
{
    EXPECT_EQ(Artifact::normalizeVideoEncodeBackendName(QStringLiteral("AUTO")),
              QStringLiteral("auto"));
    EXPECT_EQ(Artifact::normalizeVideoEncodeBackendName(QStringLiteral("ffmpeg.exe")),
              QStringLiteral("pipe"));
    EXPECT_EQ(Artifact::normalizeVideoEncodeBackendName(QStringLiteral("hardware")),
              QStringLiteral("pipe-hw"));
    EXPECT_EQ(Artifact::normalizeVideoEncodeBackendName(QStringLiteral("vulkan-hw")),
              QStringLiteral("pipe-vulkan"));
    EXPECT_EQ(Artifact::normalizeVideoEncodeBackendName(QStringLiteral("FFmpegAPI")),
              QStringLiteral("native"));
    EXPECT_EQ(Artifact::normalizeVideoEncodeBackendName(QStringLiteral("Diligent")),
              QStringLiteral("auto"));
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

TEST(ExrWriterOutputContractTest,
     UnknownCryptomatteMetadataIdsArePreservedWithoutInventedDraftAliases)
{
    QTemporaryDir outputDirectory;
    ASSERT_TRUE(outputDirectory.isValid());
    const QString path = outputDirectory.filePath(QStringLiteral("hashed_prefix.exr"));
    ArtifactCore::MultiChannelImage image(1, 1);
    image.addChannel(ArtifactCore::ChannelType::ObjectId);
    image.addChannel(ArtifactCore::ChannelType::MaterialId);
    auto objectIds = image.getChannel(ArtifactCore::ChannelType::ObjectId);
    auto materialIds = image.getChannel(ArtifactCore::ChannelType::MaterialId);
    ASSERT_NE(objectIds, nullptr);
    ASSERT_NE(materialIds, nullptr);
    objectIds->data()[0] = 13.0f;
    materialIds->data()[0] = 27.0f;

    const auto metadataId = [](const QString& layerName) {
        QString value = QString::number(
            ArtifactCore::Image::Cryptomatte::CryptoSample::nameToId(layerName), 16)
                            .rightJustified(8, QLatin1Char('0'));
        value.chop(1);
        return value;
    };
    const QString objectId = metadataId(QStringLiteral("CryptoObject"));
    const QString materialId = metadataId(QStringLiteral("CryptoMaterial"));
    // The exporter currently recognizes only the standard Cryptomatte layer
    // ids below. Arbitrary ids must remain ordinary metadata, never inferred
    // draft-channel aliases.
    ASSERT_NE(objectId, QStringLiteral("00000000"));
    ASSERT_NE(materialId, QStringLiteral("00000001"));

    ArtifactCore::ImageExportOptions options;
    options.format = QStringLiteral("exr");
    options.dataType = OIIO::TypeDesc::FLOAT;
    options.stringAttributes.insert(
        QStringLiteral("cryptomatte/%1/name").arg(objectId),
        QStringLiteral("CryptoObject"));
    options.stringAttributes.insert(
        QStringLiteral("cryptomatte/%1/manifest").arg(objectId),
        QStringLiteral("{}"));
    options.stringAttributes.insert(
        QStringLiteral("cryptomatte/%1/name").arg(materialId),
        QStringLiteral("CryptoMaterial"));
    options.stringAttributes.insert(
        QStringLiteral("cryptomatte/%1/manifest").arg(materialId),
        QStringLiteral("{}"));

    ArtifactCore::ImageExporter exporter;
    const auto result = exporter.writeMultiChannel(image, path, options);
    ASSERT_TRUE(result.success) << result.errorStage.toStdString() << ": "
                                << result.errorMessage.toStdString();

    const QByteArray encodedPath = path.toUtf8();
    auto input = OIIO::ImageInput::open(encodedPath.constData());
    ASSERT_NE(input, nullptr);
    const auto& spec = input->spec();
    const QByteArray objectNameAttribute =
        QStringLiteral("cryptomatte/%1/name").arg(objectId).toUtf8();
    const QByteArray materialNameAttribute =
        QStringLiteral("cryptomatte/%1/name").arg(materialId).toUtf8();
    EXPECT_EQ(spec.get_string_attribute(objectNameAttribute.constData(), ""),
              "CryptoObject");
    EXPECT_EQ(spec.get_string_attribute(materialNameAttribute.constData(), ""),
              "CryptoMaterial");
    const auto hasChannel = [&spec](const std::string& name) {
        return std::find(spec.channelnames.begin(), spec.channelnames.end(), name) !=
               spec.channelnames.end();
    };
    EXPECT_TRUE(hasChannel("ObjectId"));
    EXPECT_TRUE(hasChannel("MaterialId"));
    EXPECT_FALSE(hasChannel("CryptoObject00.R"));
    EXPECT_FALSE(hasChannel("CryptoMaterial00.R"));
    input->close();
}

TEST(ExrWriterOutputContractTest, CryptomatteNameHashIsStableAndAvoidsZeroAndNonFinitePayloads)
{
    using ArtifactCore::Image::Cryptomatte::CryptoSample;
    const auto idFor = [](const QString& name) {
        return CryptoSample::nameToId(name);
    };
    for (const QString& name : {QStringLiteral("Layer A"),
                                 QStringLiteral("日本語レイヤー"),
                                 QStringLiteral("Camera.001")}) {
        const std::uint32_t first = idFor(name);
        EXPECT_NE(first, 0u) << name.toStdString();
        EXPECT_EQ(idFor(name), first) << name.toStdString();

        float payload = 0.0f;
        static_assert(sizeof(payload) == sizeof(first));
        std::memcpy(&payload, &first, sizeof(payload));
        EXPECT_TRUE(std::isfinite(payload)) << name.toStdString();
    }
    EXPECT_NE(idFor(QStringLiteral("Layer A")),
              idFor(QStringLiteral("Layer B")));
}

TEST(ExrWriterOutputContractTest,
     MultiChannelValidationFailurePreservesExistingDestination)
{
    QTemporaryDir outputDirectory;
    ASSERT_TRUE(outputDirectory.isValid());
    const QString path = outputDirectory.filePath(
        QStringLiteral("existing_multichannel.exr"));
    const QByteArray preservedBytes = QByteArrayLiteral("previous EXR output");
    QFile existingOutput(path);
    ASSERT_TRUE(existingOutput.open(QIODevice::WriteOnly));
    ASSERT_EQ(existingOutput.write(preservedBytes), preservedBytes.size());
    existingOutput.close();

    ArtifactCore::MultiChannelImage emptyImage;
    ArtifactCore::ImageExportOptions options;
    options.format = QStringLiteral("exr");
    ArtifactCore::ImageExporter exporter;
    const auto result = exporter.writeMultiChannel(emptyImage, path, options);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorStage, QStringLiteral("validate"));
    EXPECT_EQ(result.errorMessage, QStringLiteral("MultiChannelImage is empty."));
    QFile preservedOutput(path);
    ASSERT_TRUE(preservedOutput.open(QIODevice::ReadOnly));
    EXPECT_EQ(preservedOutput.readAll(), preservedBytes);
}

TEST(ExrWriterOutputContractTest,
     MultiChannelExrReplacesNonFiniteAovSamplesWithZero)
{
    QTemporaryDir outputDirectory;
    ASSERT_TRUE(outputDirectory.isValid());
    const QString path = outputDirectory.filePath(
        QStringLiteral("finite_aov_values.exr"));

    ArtifactCore::MultiChannelImage image(2, 1);
    image.addChannel(ArtifactCore::ChannelType::Depth);
    image.addChannel(ArtifactCore::ChannelType::VelocityX);
    image.addChannel(ArtifactCore::ChannelType::Custom);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const auto setPair = [&image](ArtifactCore::ChannelType channel,
                                  const float first, const float second) {
        auto values = image.getChannel(channel);
        ASSERT_NE(values, nullptr);
        values->data()[0] = first;
        values->data()[1] = second;
    };
    setPair(ArtifactCore::ChannelType::Depth, nan, 0.75f);
    setPair(ArtifactCore::ChannelType::VelocityX, infinity, -3.0f);
    setPair(ArtifactCore::ChannelType::Custom, -infinity, 1.25f);

    ArtifactCore::ImageExportOptions options;
    options.format = QStringLiteral("exr");
    options.dataType = OIIO::TypeDesc::FLOAT;
    ArtifactCore::ImageExporter exporter;
    const auto result = exporter.writeMultiChannel(image, path, options);
    ASSERT_TRUE(result.success) << result.errorStage.toStdString() << ": "
                                << result.errorMessage.toStdString();

    const QByteArray encodedPath = path.toUtf8();
    auto input = OIIO::ImageInput::open(encodedPath.constData());
    ASSERT_NE(input, nullptr);
    const auto& spec = input->spec();
    ASSERT_EQ(spec.nchannels, 3);
    const auto channelIndex = [&spec](const std::string& name) {
        const auto found = std::find(spec.channelnames.begin(),
                                     spec.channelnames.end(), name);
        return found == spec.channelnames.end()
            ? -1 : static_cast<int>(found - spec.channelnames.begin());
    };
    std::vector<float> pixels(static_cast<std::size_t>(spec.nchannels) * 2);
    ASSERT_TRUE(input->read_image(OIIO::TypeDesc::FLOAT, pixels.data()));
    for (const auto& [name, finiteValue] : {
             std::pair{std::string("Depth"), 0.75f},
             std::pair{std::string("Velocity.X"), -3.0f},
             std::pair{std::string("Custom"), 1.25f}}) {
        const int index = channelIndex(name);
        ASSERT_GE(index, 0) << name;
        EXPECT_FLOAT_EQ(pixels[static_cast<std::size_t>(index)], 0.0f)
            << name << " non-finite sample";
        EXPECT_FLOAT_EQ(pixels[static_cast<std::size_t>(spec.nchannels + index)],
                        finiteValue)
            << name << " finite neighbor";
    }
    input->close();
}

TEST(ExrWriterOutputContractTest,
     MultiChannelExrRoundTripsNamedAovValuesAndSingleHitCryptomatteDraftAliases)
{
    QTemporaryDir outputDirectory;
    ASSERT_TRUE(outputDirectory.isValid());
    const QString path = outputDirectory.filePath(QStringLiteral("aov_roundtrip.exr"));

    ArtifactCore::MultiChannelImage image(2, 1);
    const std::array channels{
        ArtifactCore::ChannelType::Red,
        ArtifactCore::ChannelType::Green,
        ArtifactCore::ChannelType::Blue,
        ArtifactCore::ChannelType::Alpha,
        ArtifactCore::ChannelType::Depth,
        ArtifactCore::ChannelType::NormalX,
        ArtifactCore::ChannelType::NormalY,
        ArtifactCore::ChannelType::NormalZ,
        ArtifactCore::ChannelType::VelocityX,
        ArtifactCore::ChannelType::VelocityY,
        ArtifactCore::ChannelType::ObjectId,
        ArtifactCore::ChannelType::MaterialId,
        ArtifactCore::ChannelType::AlbedoR,
        ArtifactCore::ChannelType::AlbedoG,
        ArtifactCore::ChannelType::AlbedoB,
        ArtifactCore::ChannelType::Emission,
        ArtifactCore::ChannelType::PositionX,
        ArtifactCore::ChannelType::PositionY,
        ArtifactCore::ChannelType::PositionZ,
        ArtifactCore::ChannelType::U,
        ArtifactCore::ChannelType::V,
        ArtifactCore::ChannelType::Custom};
    for (const auto channel : channels) {
        image.addChannel(channel);
    }
    const auto setPair = [&image](ArtifactCore::ChannelType channel,
                                  const float first, const float second) {
        auto values = image.getChannel(channel);
        ASSERT_NE(values, nullptr);
        values->data()[0] = first;
        values->data()[1] = second;
    };
    setPair(ArtifactCore::ChannelType::Red, 0.125f, 0.25f);
    setPair(ArtifactCore::ChannelType::Green, 0.375f, 0.5f);
    setPair(ArtifactCore::ChannelType::Blue, 0.625f, 0.75f);
    setPair(ArtifactCore::ChannelType::Alpha, 1.0f, 0.5f);
    setPair(ArtifactCore::ChannelType::Depth, 0.2f, 0.8f);
    setPair(ArtifactCore::ChannelType::NormalX, -0.5f, 0.5f);
    setPair(ArtifactCore::ChannelType::NormalY, -0.25f, 0.25f);
    setPair(ArtifactCore::ChannelType::NormalZ, 0.75f, -0.75f);
    setPair(ArtifactCore::ChannelType::VelocityX, -12.0f, 13.0f);
    setPair(ArtifactCore::ChannelType::VelocityY, 21.0f, -22.0f);
    setPair(ArtifactCore::ChannelType::ObjectId, 17.0f, 0.0f);
    setPair(ArtifactCore::ChannelType::MaterialId, 29.0f, 0.0f);
    setPair(ArtifactCore::ChannelType::AlbedoR, 0.11f, 0.21f);
    setPair(ArtifactCore::ChannelType::AlbedoG, 0.31f, 0.41f);
    setPair(ArtifactCore::ChannelType::AlbedoB, 0.51f, 0.61f);
    setPair(ArtifactCore::ChannelType::Emission, 1.5f, 2.5f);
    setPair(ArtifactCore::ChannelType::PositionX, -101.0f, 102.0f);
    setPair(ArtifactCore::ChannelType::PositionY, -201.0f, 202.0f);
    setPair(ArtifactCore::ChannelType::PositionZ, -301.0f, 302.0f);
    setPair(ArtifactCore::ChannelType::U, 0.125f, 0.875f);
    setPair(ArtifactCore::ChannelType::V, 0.25f, 0.75f);
    setPair(ArtifactCore::ChannelType::Custom, 9.0f, -9.0f);

    ArtifactCore::ImageExportOptions options;
    options.format = QStringLiteral("exr");
    options.dataType = OIIO::TypeDesc::FLOAT;
    options.compression = QStringLiteral("zip");
    options.stringAttributes.insert(QStringLiteral("cryptomatte/00000000/name"),
                                    QStringLiteral("CryptoObject"));
    options.stringAttributes.insert(QStringLiteral("cryptomatte/00000000/hash"),
                                    QStringLiteral("MurmurHash3_32"));
    options.stringAttributes.insert(QStringLiteral("cryptomatte/00000000/conversion"),
                                    QStringLiteral("uint32_to_float32"));
    options.stringAttributes.insert(QStringLiteral("cryptomatte/00000000/manifest"),
                                    QStringLiteral("{\"LayerA\":\"00000011\"}"));
    options.stringAttributes.insert(QStringLiteral("cryptomatte/00000001/name"),
                                    QStringLiteral("CryptoMaterial"));
    options.stringAttributes.insert(QStringLiteral("cryptomatte/00000001/hash"),
                                    QStringLiteral("MurmurHash3_32"));
    options.stringAttributes.insert(QStringLiteral("cryptomatte/00000001/conversion"),
                                    QStringLiteral("uint32_to_float32"));
    options.stringAttributes.insert(QStringLiteral("cryptomatte/00000001/manifest"),
                                    QStringLiteral("{\"MaterialA\":\"0000001d\"}"));

    ArtifactCore::ImageExporter exporter;
    const auto result = exporter.writeMultiChannel(image, path, options);
    ASSERT_TRUE(result.success) << result.errorStage.toStdString() << ": "
                                << result.errorMessage.toStdString();

    const QByteArray encodedPath = path.toUtf8();
    auto input = OIIO::ImageInput::open(encodedPath.constData());
    ASSERT_NE(input, nullptr);
    const auto& spec = input->spec();
    ASSERT_EQ(spec.width, 2);
    ASSERT_EQ(spec.height, 1);
    EXPECT_EQ(spec.format.basetype, OIIO::TypeDesc::FLOAT);
    EXPECT_EQ(spec.get_string_attribute("compression", ""), "zip");
    EXPECT_EQ(spec.nchannels, static_cast<int>(channels.size()) + 8);
    ASSERT_EQ(spec.channelnames.size(), static_cast<std::size_t>(spec.nchannels));
    for (std::size_t index = 0; index < spec.channelnames.size(); ++index) {
        for (std::size_t previous = 0; previous < index; ++previous) {
            EXPECT_NE(spec.channelnames[index], spec.channelnames[previous])
                << "duplicate EXR channel " << spec.channelnames[index];
        }
    }
    if (!spec.channelformats.empty()) {
        ASSERT_EQ(spec.channelformats.size(),
                  static_cast<std::size_t>(spec.nchannels));
        for (std::size_t index = 0; index < spec.channelformats.size(); ++index) {
            EXPECT_EQ(spec.channelformats[index].basetype, OIIO::TypeDesc::FLOAT)
                << "channel " << spec.channelnames[index];
        }
    }
    const auto channelIndex = [&spec](const std::string& name) {
        const auto found = std::find(spec.channelnames.begin(),
                                     spec.channelnames.end(), name);
        return found == spec.channelnames.end()
            ? -1 : static_cast<int>(found - spec.channelnames.begin());
    };
    for (const std::string& name : {
             "R", "G", "B", "A", "Depth", "Normal.X", "Normal.Y",
             "Normal.Z", "Velocity.X", "Velocity.Y", "ObjectId", "MaterialId",
             "Albedo.R", "Albedo.G", "Albedo.B", "Emission", "Position.X",
             "Position.Y", "Position.Z", "UV.U", "UV.V", "Custom",
             "CryptoObject00.R", "CryptoObject00.G", "CryptoObject00.B",
             "CryptoObject00.A", "CryptoMaterial00.R", "CryptoMaterial00.G",
             "CryptoMaterial00.B", "CryptoMaterial00.A"}) {
        EXPECT_GE(channelIndex(name), 0) << name;
    }
    EXPECT_EQ(spec.get_string_attribute("cryptomatte/00000000/name", ""),
              "CryptoObject");
    EXPECT_EQ(spec.get_string_attribute("cryptomatte/00000000/hash", ""),
              "MurmurHash3_32");
    EXPECT_EQ(spec.get_string_attribute("cryptomatte/00000000/conversion", ""),
              "uint32_to_float32");
    EXPECT_EQ(spec.get_string_attribute("cryptomatte/00000000/manifest", ""),
              "{\"LayerA\":\"00000011\"}");
    EXPECT_EQ(spec.get_string_attribute("cryptomatte/00000001/name", ""),
              "CryptoMaterial");
    EXPECT_EQ(spec.get_string_attribute("cryptomatte/00000001/hash", ""),
              "MurmurHash3_32");
    EXPECT_EQ(spec.get_string_attribute("cryptomatte/00000001/conversion", ""),
              "uint32_to_float32");
    EXPECT_EQ(spec.get_string_attribute("cryptomatte/00000001/manifest", ""),
              "{\"MaterialA\":\"0000001d\"}");

    std::vector<float> pixels(static_cast<std::size_t>(spec.width) *
                              static_cast<std::size_t>(spec.height) *
                              static_cast<std::size_t>(spec.nchannels));
    ASSERT_TRUE(input->read_image(OIIO::TypeDesc::FLOAT, pixels.data()));
    for (const auto [name, expected] : {
             std::pair{"R", std::array{0.125f, 0.25f}},
             std::pair{"G", std::array{0.375f, 0.5f}},
             std::pair{"B", std::array{0.625f, 0.75f}},
             std::pair{"A", std::array{1.0f, 0.5f}},
             std::pair{"Depth", std::array{0.2f, 0.8f}},
             std::pair{"Normal.X", std::array{-0.5f, 0.5f}},
             std::pair{"Normal.Y", std::array{-0.25f, 0.25f}},
             std::pair{"Normal.Z", std::array{0.75f, -0.75f}},
             std::pair{"Velocity.X", std::array{-12.0f, 13.0f}},
             std::pair{"Velocity.Y", std::array{21.0f, -22.0f}},
             std::pair{"ObjectId", std::array{17.0f, 0.0f}},
             std::pair{"MaterialId", std::array{29.0f, 0.0f}},
             std::pair{"Albedo.R", std::array{0.11f, 0.21f}},
             std::pair{"Albedo.G", std::array{0.31f, 0.41f}},
             std::pair{"Albedo.B", std::array{0.51f, 0.61f}},
             std::pair{"Emission", std::array{1.5f, 2.5f}},
             std::pair{"Position.X", std::array{-101.0f, 102.0f}},
             std::pair{"Position.Y", std::array{-201.0f, 202.0f}},
             std::pair{"Position.Z", std::array{-301.0f, 302.0f}},
             std::pair{"UV.U", std::array{0.125f, 0.875f}},
             std::pair{"UV.V", std::array{0.25f, 0.75f}},
             std::pair{"Custom", std::array{9.0f, -9.0f}},
             std::pair{"CryptoObject00.R", std::array{17.0f, 0.0f}},
             std::pair{"CryptoObject00.G", std::array{1.0f, 0.0f}},
             std::pair{"CryptoObject00.B", std::array{0.0f, 0.0f}},
             std::pair{"CryptoObject00.A", std::array{0.0f, 0.0f}},
             std::pair{"CryptoMaterial00.R", std::array{29.0f, 0.0f}},
             std::pair{"CryptoMaterial00.G", std::array{1.0f, 0.0f}},
             std::pair{"CryptoMaterial00.B", std::array{0.0f, 0.0f}},
             std::pair{"CryptoMaterial00.A", std::array{0.0f, 0.0f}}}) {
        const int index = channelIndex(name);
        ASSERT_GE(index, 0) << name;
        EXPECT_FLOAT_EQ(pixels[static_cast<std::size_t>(index)], expected[0]);
        EXPECT_FLOAT_EQ(pixels[static_cast<std::size_t>(spec.nchannels + index)],
                        expected[1]);
    }
    input->close();
}

TEST(ExrWriterOutputContractTest,
     CryptomatteDraftCoverageClampsAlphaAndSuppressesZeroIds)
{
    QTemporaryDir outputDirectory;
    ASSERT_TRUE(outputDirectory.isValid());
    const QString path = outputDirectory.filePath(
        QStringLiteral("cryptomatte_draft_coverage.exr"));

    ArtifactCore::MultiChannelImage image(4, 1);
    image.addChannel(ArtifactCore::ChannelType::Alpha);
    image.addChannel(ArtifactCore::ChannelType::ObjectId);
    const auto alpha = image.getChannel(ArtifactCore::ChannelType::Alpha);
    const auto objectIds = image.getChannel(ArtifactCore::ChannelType::ObjectId);
    ASSERT_NE(alpha, nullptr);
    ASSERT_NE(objectIds, nullptr);
    const std::array<float, 4> alphaValues{0.25f, 0.5f, 2.0f, -0.25f};
    const std::array<float, 4> objectIdValues{17.0f, 0.0f, 29.0f, 31.0f};
    std::copy(alphaValues.begin(), alphaValues.end(), alpha->data());
    std::copy(objectIdValues.begin(), objectIdValues.end(), objectIds->data());

    ArtifactCore::ImageExportOptions options;
    options.format = QStringLiteral("exr");
    options.dataType = OIIO::TypeDesc::FLOAT;
    options.stringAttributes.insert(
        QStringLiteral("cryptomatte/00000000/name"),
        QStringLiteral("CryptoObject"));
    options.stringAttributes.insert(
        QStringLiteral("cryptomatte/00000000/manifest"),
        QStringLiteral("{}"));

    ArtifactCore::ImageExporter exporter;
    const auto result = exporter.writeMultiChannel(image, path, options);
    ASSERT_TRUE(result.success) << result.errorStage.toStdString() << ": "
                                << result.errorMessage.toStdString();

    const QByteArray encodedPath = path.toUtf8();
    auto input = OIIO::ImageInput::open(encodedPath.constData());
    ASSERT_NE(input, nullptr);
    const auto& spec = input->spec();
    const auto channelIndex = [&spec](const std::string& name) {
        const auto found = std::find(spec.channelnames.begin(),
                                     spec.channelnames.end(), name);
        return found == spec.channelnames.end()
            ? -1 : static_cast<int>(found - spec.channelnames.begin());
    };
    const int idIndex = channelIndex("CryptoObject00.R");
    const int coverageIndex = channelIndex("CryptoObject00.G");
    const int unusedBIndex = channelIndex("CryptoObject00.B");
    const int unusedAIndex = channelIndex("CryptoObject00.A");
    ASSERT_GE(idIndex, 0);
    ASSERT_GE(coverageIndex, 0);
    ASSERT_GE(unusedBIndex, 0);
    ASSERT_GE(unusedAIndex, 0);

    std::vector<float> pixels(static_cast<std::size_t>(spec.nchannels) * 4u);
    ASSERT_TRUE(input->read_image(OIIO::TypeDesc::FLOAT, pixels.data()));
    const auto sample = [&pixels, &spec](int x, int channel) {
        return pixels[static_cast<std::size_t>(x) *
                          static_cast<std::size_t>(spec.nchannels) +
                      static_cast<std::size_t>(channel)];
    };
    EXPECT_FLOAT_EQ(sample(0, idIndex), 17.0f);
    EXPECT_FLOAT_EQ(sample(0, coverageIndex), 0.25f);
    EXPECT_FLOAT_EQ(sample(1, idIndex), 0.0f);
    EXPECT_FLOAT_EQ(sample(1, coverageIndex), 0.0f);
    EXPECT_FLOAT_EQ(sample(2, idIndex), 29.0f);
    EXPECT_FLOAT_EQ(sample(2, coverageIndex), 1.0f);
    EXPECT_FLOAT_EQ(sample(3, idIndex), 31.0f);
    EXPECT_FLOAT_EQ(sample(3, coverageIndex), 0.0f);
    for (int x = 0; x < 4; ++x) {
        EXPECT_FLOAT_EQ(sample(x, unusedBIndex), 0.0f) << "pixel=" << x;
        EXPECT_FLOAT_EQ(sample(x, unusedAIndex), 0.0f) << "pixel=" << x;
    }
    input->close();
}

TEST(ExrWriterOutputContractTest,
     DeepExrRoundTripRetainsVariablePerPixelSamplesAndDepthOrder)
{
    QTemporaryDir outputDirectory;
    ASSERT_TRUE(outputDirectory.isValid());
    const QString path = outputDirectory.filePath(QStringLiteral("deep_samples.exr"));
    std::vector<std::vector<ArtifactCore::OpenExrDeepSample>> source(4);
    source[0].push_back({0.25f, 0.1f, 0.2f, 0.3f, 0.5f});
    source[0].push_back({0.75f, 0.4f, 0.5f, 0.6f, 1.0f});
    source[1].push_back({0.5f, 0.7f, 0.6f, 0.5f, 0.75f});
    source[2].push_back({0.1f, 0.8f, 0.7f, 0.6f, 0.5f});
    source[2].push_back({0.4f, 0.3f, 0.2f, 0.1f, 0.75f});
    source[2].push_back({0.9f, 0.2f, 0.3f, 0.4f, 0.25f});
    // Pixel 3 deliberately has no samples.

    ArtifactCore::OpenExr exr;
    ASSERT_TRUE(exr.writeDeepRGBA32F(path, 4, 1, source));
    std::vector<std::vector<ArtifactCore::OpenExrDeepSample>> decoded;
    int width = 0;
    int height = 0;
    ASSERT_TRUE(exr.readDeepRGBA32F(path, decoded, width, height));
    ASSERT_EQ(width, 4);
    ASSERT_EQ(height, 1);
    ASSERT_EQ(decoded.size(), 4u);
    ASSERT_EQ(decoded[0].size(), 2u);
    ASSERT_EQ(decoded[1].size(), 1u);
    ASSERT_EQ(decoded[2].size(), 3u);
    EXPECT_TRUE(decoded[3].empty());
    ASSERT_EQ(decoded.size(), source.size());
    for (std::size_t pixel = 0; pixel < source.size(); ++pixel) {
        ASSERT_EQ(decoded[pixel].size(), source[pixel].size()) << "pixel " << pixel;
        for (std::size_t sample = 0; sample < source[pixel].size(); ++sample) {
            const auto& expected = source[pixel][sample];
            const auto& actual = decoded[pixel][sample];
            EXPECT_FLOAT_EQ(actual.depth, expected.depth)
                << "pixel " << pixel << ", sample " << sample;
            EXPECT_FLOAT_EQ(actual.red, expected.red)
                << "pixel " << pixel << ", sample " << sample;
            EXPECT_FLOAT_EQ(actual.green, expected.green)
                << "pixel " << pixel << ", sample " << sample;
            EXPECT_FLOAT_EQ(actual.blue, expected.blue)
                << "pixel " << pixel << ", sample " << sample;
            EXPECT_FLOAT_EQ(actual.alpha, expected.alpha)
                << "pixel " << pixel << ", sample " << sample;
        }
    }
}

TEST(ExrWriterOutputContractTest,
     DeepReaderClearsOutputArgumentsWhenInputIsNotDeep)
{
    QTemporaryDir outputDirectory;
    ASSERT_TRUE(outputDirectory.isValid());
    const QString path = outputDirectory.filePath(
        QStringLiteral("flat_input.exr"));
    constexpr std::array<float, 4> rgba{0.2f, 0.4f, 0.6f, 1.0f};
    ArtifactCore::OpenExr exr;
    ASSERT_TRUE(exr.writeRGBA32F(path, rgba.data(), 1, 1));

    std::vector<std::vector<ArtifactCore::OpenExrDeepSample>> samples(1);
    samples[0].push_back({0.5f, 0.1f, 0.2f, 0.3f, 1.0f});
    int width = 17;
    int height = 19;
    EXPECT_FALSE(exr.readDeepRGBA32F(path, samples, width, height));
    EXPECT_TRUE(samples.empty());
    EXPECT_EQ(width, 0);
    EXPECT_EQ(height, 0);

    ArtifactCore::DeepImageBuffer image(1, 1);
    ArtifactCore::DeepSample sample;
    sample.depth = 0.5f;
    sample.color = {0.1f, 0.2f, 0.3f, 1.0f};
    sample.alpha = 1.0f;
    ASSERT_TRUE(image.addSample(0, 0, sample));
    EXPECT_FALSE(exr.readDeepRGBA32F(path, image));
    EXPECT_EQ(image.width(), 0);
    EXPECT_EQ(image.height(), 0);
    EXPECT_TRUE(image.isEmpty());
}

TEST(ExrWriterOutputContractTest,
     DeepImageBufferWritesOnlyDepthAndRgbaAndReadbackUsesDocumentedDefaults)
{
    QTemporaryDir outputDirectory;
    ASSERT_TRUE(outputDirectory.isValid());
    const QString path = outputDirectory.filePath(QStringLiteral("deep_buffer.exr"));

    ArtifactCore::DeepImageBuffer source(2, 1);
    ArtifactCore::DeepSample front;
    front.depth = 0.2f;
    front.depthBack = 0.35f;
    front.color = {0.1f, 0.2f, 0.3f, 1.0f};
    front.alpha = 0.6f;
    front.coverage = 0.25f;
    front.holdout = true;
    ASSERT_TRUE(source.addSample(0, 0, front));

    ArtifactCore::DeepSample back;
    back.depth = 0.8f;
    back.depthBack = 0.95f;
    back.color = {0.7f, 0.6f, 0.5f, 1.0f};
    back.alpha = 0.4f;
    back.coverage = 0.75f;
    ASSERT_TRUE(source.addSample(0, 0, back));

    ArtifactCore::OpenExr exr;
    ASSERT_TRUE(exr.writeDeepRGBA32F(path, source));

    const QByteArray encodedPath = path.toUtf8();
    auto input = OIIO::ImageInput::open(encodedPath.constData());
    ASSERT_NE(input, nullptr);
    const auto& spec = input->spec();
    ASSERT_TRUE(spec.deep);
    EXPECT_EQ(spec.nchannels, 5);
    EXPECT_EQ(spec.channelnames,
              (std::vector<std::string>{"Z", "R", "G", "B", "A"}));
    OIIO::DeepData deep;
    ASSERT_TRUE(input->read_native_deep_image(0, 0, deep));
    EXPECT_EQ(deep.samples(0), 2);
    EXPECT_EQ(deep.samples(1), 0);
    for (int sampleIndex = 0; sampleIndex < 2; ++sampleIndex) {
        const auto& expected = sampleIndex == 0 ? front : back;
        EXPECT_FLOAT_EQ(deep.deep_value(0, 0, sampleIndex), expected.depth);
        EXPECT_FLOAT_EQ(deep.deep_value(0, 1, sampleIndex), expected.color[0]);
        EXPECT_FLOAT_EQ(deep.deep_value(0, 2, sampleIndex), expected.color[1]);
        EXPECT_FLOAT_EQ(deep.deep_value(0, 3, sampleIndex), expected.color[2]);
        EXPECT_FLOAT_EQ(deep.deep_value(0, 4, sampleIndex), expected.alpha);
    }
    input->close();

    ArtifactCore::DeepImageBuffer decoded;
    ASSERT_TRUE(exr.readDeepRGBA32F(path, decoded));
    ASSERT_EQ(decoded.width(), 2);
    ASSERT_EQ(decoded.height(), 1);
    const auto* firstPixel = decoded.pixel(0, 0);
    const auto* emptyPixel = decoded.pixel(1, 0);
    ASSERT_NE(firstPixel, nullptr);
    ASSERT_NE(emptyPixel, nullptr);
    ASSERT_EQ(firstPixel->samples.size(), 2u);
    EXPECT_TRUE(emptyPixel->samples.empty());

    const auto& decodedFront = firstPixel->samples[0];
    EXPECT_FLOAT_EQ(decodedFront.depth, front.depth);
    EXPECT_FLOAT_EQ(decodedFront.depthBack, front.depth);
    EXPECT_FLOAT_EQ(decodedFront.color[0], front.color[0]);
    EXPECT_FLOAT_EQ(decodedFront.color[1], front.color[1]);
    EXPECT_FLOAT_EQ(decodedFront.color[2], front.color[2]);
    EXPECT_FLOAT_EQ(decodedFront.alpha, front.alpha);
    EXPECT_FLOAT_EQ(decodedFront.coverage, 1.0f);
    EXPECT_FALSE(decodedFront.holdout);

    const auto& decodedBack = firstPixel->samples[1];
    EXPECT_FLOAT_EQ(decodedBack.depth, back.depth);
    EXPECT_FLOAT_EQ(decodedBack.depthBack, back.depth);
    EXPECT_FLOAT_EQ(decodedBack.color[0], back.color[0]);
    EXPECT_FLOAT_EQ(decodedBack.color[1], back.color[1]);
    EXPECT_FLOAT_EQ(decodedBack.color[2], back.color[2]);
    EXPECT_FLOAT_EQ(decodedBack.alpha, back.alpha);
    EXPECT_FLOAT_EQ(decodedBack.coverage, 1.0f);
    EXPECT_FALSE(decodedBack.holdout);
}

TEST(ExrWriterOutputContractTest,
     DeepWriterRejectsUnsortedAndNonFiniteSamplePayloads)
{
    QTemporaryDir outputDirectory;
    ASSERT_TRUE(outputDirectory.isValid());
    const QString path = outputDirectory.filePath(QStringLiteral("invalid_deep.exr"));
    ArtifactCore::OpenExr exr;

    std::vector<std::vector<ArtifactCore::OpenExrDeepSample>> unsorted(1);
    unsorted[0].push_back({0.8f, 0.1f, 0.2f, 0.3f, 0.4f});
    unsorted[0].push_back({0.2f, 0.5f, 0.6f, 0.7f, 0.8f});
    EXPECT_FALSE(exr.writeDeepRGBA32F(path, 1, 1, unsorted));
    EXPECT_FALSE(QFileInfo::exists(path));

    const QByteArray preservedBytes = QByteArrayLiteral("previous deep exr output");
    QFile existingOutput(path);
    ASSERT_TRUE(existingOutput.open(QIODevice::WriteOnly));
    ASSERT_EQ(existingOutput.write(preservedBytes), preservedBytes.size());
    existingOutput.close();
    const auto readExistingOutput = [&path]() {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return QByteArray{};
        }
        return file.readAll();
    };

    EXPECT_FALSE(exr.writeDeepRGBA32F(path, 1, 1, unsorted));
    EXPECT_EQ(readExistingOutput(), preservedBytes);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float positiveInfinity = std::numeric_limits<float>::infinity();
    const float negativeInfinity = -positiveInfinity;
    for (const auto& sample : {
             ArtifactCore::OpenExrDeepSample{nan, 0.1f, 0.2f, 0.3f, 0.4f},
             ArtifactCore::OpenExrDeepSample{0.5f, nan, 0.2f, 0.3f, 0.4f},
             ArtifactCore::OpenExrDeepSample{0.5f, 0.1f, nan, 0.3f, 0.4f},
             ArtifactCore::OpenExrDeepSample{0.5f, 0.1f, 0.2f, nan, 0.4f},
             ArtifactCore::OpenExrDeepSample{0.5f, 0.1f, 0.2f, 0.3f, nan},
             ArtifactCore::OpenExrDeepSample{positiveInfinity, 0.1f, 0.2f, 0.3f, 0.4f},
             ArtifactCore::OpenExrDeepSample{0.5f, negativeInfinity, 0.2f, 0.3f, 0.4f},
             ArtifactCore::OpenExrDeepSample{0.5f, 0.1f, positiveInfinity, 0.3f, 0.4f},
             ArtifactCore::OpenExrDeepSample{0.5f, 0.1f, 0.2f, negativeInfinity, 0.4f},
             ArtifactCore::OpenExrDeepSample{0.5f, 0.1f, 0.2f, 0.3f, positiveInfinity}}) {
        std::vector<std::vector<ArtifactCore::OpenExrDeepSample>> invalid(1);
        invalid[0].push_back(sample);
        EXPECT_FALSE(exr.writeDeepRGBA32F(path, 1, 1, invalid));
        EXPECT_EQ(readExistingOutput(), preservedBytes);
    }

    std::vector<std::vector<ArtifactCore::OpenExrDeepSample>> missingPixel;
    EXPECT_FALSE(exr.writeDeepRGBA32F(path, 1, 1, missingPixel));
    EXPECT_EQ(readExistingOutput(), preservedBytes);
    EXPECT_FALSE(exr.writeDeepRGBA32F(path, 0, 1, unsorted));
    EXPECT_EQ(readExistingOutput(), preservedBytes);
}
