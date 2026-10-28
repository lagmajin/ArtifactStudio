// Standalone end-to-end pipeline test:
//   image file load -> timeline placement -> transform animation
//   -> render queue registration -> final PNG render -> image verification
//
// Unlike the layer-by-layer contract tests, this drives the production
// pipeline the way the application does: a real PNG is written to disk and
// imported through ArtifactImageInitParams, the layer is placed on a frame
// range, position/rotation/scale are keyframed (with bezier position
// interpolation and spatial tangents), a Render Manager job renders the
// composition with the GPU backend, and the written frames are inspected as
// images.
//
//   ArtifactPipelineIntegrationTest --self-check [--capture-dir DIR]
//
// Artifacts (source image, sampled frames, montage, analysis JSON) are written
// to ARTIFACT_PIPELINE_CAPTURE_DIR or a temp directory.
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
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryDir>
#include <QThread>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

import Artifact.Composition.InitParams;
import Artifact.Layer.Abstract;
import Artifact.Layer.Composition;
import Artifact.Layer.InitParams;
import Artifact.Layer.Result;
import Artifact.Project.Manager;
import Artifact.Render.Queue.Job;
import Artifact.Render.Queue.Service;
import BrightnessEffect;
import Color.Float;
import Frame.Position;
import Math.Interpolate;
import Memory.SharedPtr;
import Time.Rational;
import Utils.Id;
import Utils.String.UniString;

namespace {

constexpr int kCompWidth = 640;
constexpr int kCompHeight = 360;
constexpr int kSourceWidth = 160;
constexpr int kSourceHeight = 90;
constexpr double kFrameRate = 30.0;
constexpr int kKeyIn = 10;
constexpr int kKeyMid = 30;
constexpr int kKeyOut = 50;
constexpr int kFirstFrame = 1;
constexpr int kLastFrame = 61;
constexpr std::array<int, 7> kSampleFrames{1, 10, 20, 30, 40, 50, 61};

// Position/rotation/scale key values. The anchor stays at the layer origin, so
// a 160x90 layer rotated by up to 20 degrees still fits the 640x360 frame.
constexpr float kPositionInX = 60.0f;
constexpr float kPositionInY = 40.0f;
constexpr float kPositionMidX = 180.0f;
constexpr float kPositionMidY = 110.0f;
constexpr float kPositionOutX = 300.0f;
constexpr float kPositionOutY = 180.0f;
constexpr float kRotationIn = -10.0f;
constexpr float kRotationMid = 5.0f;
constexpr float kRotationOut = 20.0f;
constexpr float kScaleIn = 0.6f;
constexpr float kScaleMid = 1.0f;
constexpr float kScaleOut = 0.7f;

struct FrameStats {
    int visiblePixels = 0;
    double centroidX = -1.0;
    double centroidY = -1.0;
    int greenPixels = 0;
    double greenAngleDegrees = 0.0;
    int redPixels = 0;
    double redCentroidX = -1.0;
    double redCentroidY = -1.0;
    double meanLuminance = 0.0;

    bool hasLayer() const { return visiblePixels > 0; }

    double area() const { return static_cast<double>(visiblePixels); }
};

QString captureDirectory()
{
    QString directory = qEnvironmentVariable("ARTIFACT_PIPELINE_CAPTURE_DIR");
    if (directory.isEmpty()) {
        directory = QDir::tempPath() + QStringLiteral("/artifact-pipeline");
    }
    QDir().mkpath(directory);
    return directory;
}

// Four saturated quadrants with a white cross. The green quadrant (top-right)
// is far from the layer origin, so its angle around the layer centroid is a
// readable rotation measurement.
bool writeSourceImage(const QString& path)
{
    QImage image(kSourceWidth, kSourceHeight, QImage::Format_RGBA8888);
    if (image.isNull()) return false;
    image.fill(Qt::black);

    const int halfWidth = kSourceWidth / 2;
    const int halfHeight = kSourceHeight / 2;
    for (int y = 0; y < kSourceHeight; ++y) {
        for (int x = 0; x < kSourceWidth; ++x) {
            QColor color;
            if (x < halfWidth && y < halfHeight) {
                color = QColor(230, 30, 30);
            } else if (x >= halfWidth && y < halfHeight) {
                color = QColor(30, 220, 60);
            } else if (x < halfWidth) {
                color = QColor(40, 60, 230);
            } else {
                color = QColor(235, 215, 40);
            }
            if (x == halfWidth - 1 || x == halfWidth ||
                y == halfHeight - 1 || y == halfHeight) {
                color = QColor(250, 250, 250);
            }
            image.setPixelColor(x, y, color);
        }
    }
    // Give every run a unique content hash: the project asset registry is
    // process-persistent and would otherwise reuse a record whose source file
    // belonged to an older temporary project.
    const quint32 tag = QRandomGenerator::global()->generate();
    for (int index = 0; index < 4; ++index) {
        image.setPixelColor(2 + index % 2, 2 + index / 2,
                            QColor((tag >> (index * 8)) & 0xFF, 0x5A, 0xA5));
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    return image.save(path, "PNG");
}

FrameStats analyzeFrame(const QImage& image)
{
    FrameStats stats;
    if (image.isNull()) return stats;

    const QImage frame = image.convertToFormat(QImage::Format_RGBA8888);
    if (frame.isNull()) return stats;

    // The composition background is uniform; sample it away from the layer so
    // any rendered layer content stands out regardless of color management.
    const QRgb background = frame.pixel(2, 2);
    qint64 sumX = 0;
    qint64 sumY = 0;
    qint64 greenX = 0;
    qint64 greenY = 0;
    qint64 redX = 0;
    qint64 redY = 0;
    double luminanceSum = 0.0;
    for (int y = 0; y < frame.height(); ++y) {
        for (int x = 0; x < frame.width(); ++x) {
            const QRgb pixel = frame.pixel(x, y);
            const int dr = std::abs(qRed(pixel) - qRed(background));
            const int dg = std::abs(qGreen(pixel) - qGreen(background));
            const int db = std::abs(qBlue(pixel) - qBlue(background));
            if (dr <= 16 && dg <= 16 && db <= 16) {
                continue;
            }
            ++stats.visiblePixels;
            sumX += x;
            sumY += y;
            luminanceSum += (qRed(pixel) + qGreen(pixel) + qBlue(pixel)) / 3.0;

            if (qGreen(pixel) > qRed(pixel) + 40 &&
                qGreen(pixel) > qBlue(pixel) + 40) {
                ++stats.greenPixels;
                greenX += x;
                greenY += y;
            }
            if (qRed(pixel) > qGreen(pixel) + 60 &&
                qRed(pixel) > qBlue(pixel) + 60) {
                ++stats.redPixels;
                redX += x;
                redY += y;
            }
        }
    }
    if (stats.visiblePixels > 0) {
        stats.centroidX = static_cast<double>(sumX) / stats.visiblePixels;
        stats.centroidY = static_cast<double>(sumY) / stats.visiblePixels;
        stats.meanLuminance = luminanceSum / stats.visiblePixels;
    }
    if (stats.redPixels > 0) {
        stats.redCentroidX = static_cast<double>(redX) / stats.redPixels;
        stats.redCentroidY = static_cast<double>(redY) / stats.redPixels;
    }
    if (stats.greenPixels > 0) {
        const double greenCenterX = static_cast<double>(greenX) / stats.greenPixels;
        const double greenCenterY = static_cast<double>(greenY) / stats.greenPixels;
        stats.greenAngleDegrees =
            std::atan2(greenCenterY - stats.centroidY,
                       greenCenterX - stats.centroidX) *
            180.0 / 3.14159265358979323846;
    }
    return stats;
}

bool writeMontage(const std::vector<QImage>& frames, const QString& path,
                  const int columns)
{
    if (frames.empty() || columns < 1) return false;
    const int cellWidth = frames.front().width() / 2;
    const int cellHeight = frames.front().height() / 2;
    const int rows = (static_cast<int>(frames.size()) + columns - 1) / columns;
    QImage sheet(cellWidth * columns, cellHeight * rows,
                 QImage::Format_RGBA8888);
    if (sheet.isNull()) return false;
    sheet.fill(Qt::black);

    for (std::size_t index = 0; index < frames.size(); ++index) {
        const QImage cell = frames[index].convertedTo(QImage::Format_RGBA8888)
                                .scaled(cellWidth, cellHeight, Qt::IgnoreAspectRatio,
                                        Qt::SmoothTransformation);
        if (cell.isNull() || cell.size() != QSize(cellWidth, cellHeight)) continue;
        const int originX = static_cast<int>(index) % columns * cellWidth;
        const int originY = static_cast<int>(index) / columns * cellHeight;
        for (int y = 0; y < cellHeight; ++y) {
            std::copy_n(cell.constScanLine(y), cellWidth * 4,
                        sheet.scanLine(originY + y) + originX * 4);
        }
    }
    return sheet.save(path, "PNG");
}

QString framePath(const QString& basePath, const int frame)
{
    const QFileInfo info(basePath);
    return QDir(info.absolutePath()).filePath(
        QStringLiteral("%1_%2.png")
            .arg(info.completeBaseName())
            .arg(frame, 4, 10, QLatin1Char('0')));
}

bool enqueueAndRender(Artifact::ArtifactRenderQueueService* queue,
                      const ArtifactCore::CompositionID& compositionId,
                      const QString& compositionName,
                      const QString& outputBasePath,
                      const int firstFrame,
                      const int lastFrame,
                      QString* failure,
                      const QString& backend = QStringLiteral("gpu"),
                      const int width = kCompWidth,
                      const int height = kCompHeight)
{
    const QFileInfo outputInfo(outputBasePath);
    if (!QDir().mkpath(outputInfo.absolutePath())) {
        *failure = QStringLiteral("could not create the render output directory");
        return false;
    }

    queue->addRenderQueueForComposition(compositionId, compositionName);
    const int jobIndex = queue->jobCount() - 1;
    if (jobIndex < 0) {
        *failure = QStringLiteral("the render queue did not accept the composition");
        return false;
    }

    queue->setJobOutputPathAt(jobIndex, outputBasePath);
    queue->setJobOutputSettingsAt(jobIndex, QStringLiteral("PNG"),
                                  QStringLiteral("png"), width, height,
                                  kFrameRate, 8000);
    queue->setJobFramePaddingAt(jobIndex, 4);
    queue->setJobRenderBackendAt(jobIndex, backend);
    queue->setJobFrameRangeAt(jobIndex, firstFrame, lastFrame);
    auto selective = queue->jobSelectiveSettingsAt(jobIndex);
    selective.insert(QStringLiteral("frameRangeMode"),
                     static_cast<int>(Artifact::ArtifactRenderJob::FrameRangeMode::Custom));
    if (!queue->setJobSelectiveSettingsAt(jobIndex, selective)) {
        *failure = QStringLiteral("could not select an explicit frame range");
        return false;
    }
    queue->startRenderQueueAt(jobIndex);

    QElapsedTimer timeout;
    timeout.start();
    QString status;
    while (timeout.elapsed() < 240000) {
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
        *failure = QStringLiteral("render job ended as '%1': %2")
                       .arg(status, queue->jobErrorMessageAt(jobIndex));
        return false;
    }
    return true;
}

constexpr int kAdvWidth = 320;
constexpr int kAdvHeight = 180;
constexpr double kAdvFrameRate = 30.0;

using KeyList = std::array<std::pair<int, double>, 3>;

void animateChannel(Artifact::ArtifactAbstractLayer* layer, const QString& path,
                    const KeyList& keys)
{
    if (!layer) return;
    auto property = layer->getProperty(path);
    if (!property) return;
    property->clearKeyFrames();
    property->setAnimatable(true);
    const auto scale = layer->keyframeTimeScale();
    for (const auto& key : keys) {
        property->addKeyFrame(ArtifactCore::RationalTime(key.first, scale),
                              key.second,
                              ArtifactCore::InterpolationType::Bezier);
    }
}

bool createAdvancedComposition(Artifact::ArtifactProjectManager& projects,
                               const QString& projectName, const QString& compName,
                               const int frames, const ArtifactCore::FloatColor& background,
                               const int width, const int height,
                               ArtifactCore::CompositionID* compositionId)
{
    projects.closeCurrentProject();
    projects.createProject(projectName, true);
    if (!projects.isProjectCreated()) return false;

    Artifact::ArtifactCompositionInitParams params;
    params.setCompositionName(ArtifactCore::UniString(compName));
    params.setResolution(width, height);
    params.setFrameRate(kAdvFrameRate);
    params.setDurationFrames(frames);
    params.setBackgroundColor(background);
    const auto composition = projects.createComposition(params);
    if (!composition.success) return false;
    *compositionId = composition.id;
    return true;
}

bool addStaticImageLayer(Artifact::ArtifactProjectManager& projects,
                         const ArtifactCore::CompositionID& compositionId,
                         const QString& sourcePath, const QString& name,
                         ArtifactCore::SharedPtr<Artifact::ArtifactAbstractLayer>* out)
{
    Artifact::ArtifactImageInitParams params(name);
    params.setImagePath(sourcePath);
    const auto result = projects.addLayerToComposition(compositionId, params);
    if (!result.success || result.layer.get() == nullptr) return false;
    *out = result.layer;
    if (out && out->get()) {
        (*out)->setLayerPropertyValue(QStringLiteral("transform.position.x"), 40.0);
        (*out)->setLayerPropertyValue(QStringLiteral("transform.position.y"), 40.0);
        (*out)->setLayerPropertyValue(QStringLiteral("transform.scale.x"), 0.8);
        (*out)->setLayerPropertyValue(QStringLiteral("transform.scale.y"), 0.8);
    }
    return true;
}

int runPrecompScenario(const QString& captureDir)
{
    int failures = 0;
    const auto check = [&failures](const bool ok, const char* what) {
        if (!ok) {
            std::printf("FAIL precomp: %s\n", what);
            ++failures;
        }
    };

    QTemporaryDir root;
    if (!root.isValid()) {
        std::printf("FAIL precomp: temporary directory\n");
        return 1;
    }
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", root.path().toUtf8());
    const QString sourcePath = root.path() + QStringLiteral("/assets/precomp_source.png");
    if (!writeSourceImage(sourcePath)) {
        std::printf("FAIL precomp: source image\n");
        return 1;
    }

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    auto* queue = Artifact::ArtifactRenderQueueService::instance();
    if (!queue) return failures + 1;
    queue->removeAllRenderQueues();
    queue->clearCompletedJobHistory();
    projects.closeCurrentProject();
    projects.createProject(QStringLiteral("PipelinePrecompTest"), true);
    if (!projects.isProjectCreated()) return failures + 1;

    Artifact::ArtifactCompositionInitParams childParams;
    childParams.setCompositionName(ArtifactCore::UniString(QStringLiteral("NestedChild")));
    childParams.setResolution(kAdvWidth, kAdvHeight);
    childParams.setFrameRate(kAdvFrameRate);
    childParams.setDurationFrames(31);
    childParams.setBackgroundColor(
        ArtifactCore::FloatColor(0.12f, 0.16f, 0.10f, 1.0f));
    const auto child = projects.createComposition(childParams);
    check(child.success, "child composition creation failed");
    if (!child.success) {
        projects.closeCurrentProject();
        return failures;
    }

    Artifact::ArtifactImageInitParams imageParams(QStringLiteral("NestedImage"));
    imageParams.setImagePath(sourcePath);
    const auto imageLayer = projects.addLayerToComposition(child.id, imageParams);
    check(imageLayer.success && imageLayer.layer.get() != nullptr,
          "nested image layer creation failed");
    if (!imageLayer.success || imageLayer.layer.get() == nullptr) {
        projects.closeCurrentProject();
        return failures;
    }
    imageLayer.layer->setTimelineWindow(ArtifactCore::FramePosition(5),
                                        ArtifactCore::FramePosition(26));
    animateChannel(imageLayer.layer.get(), QStringLiteral("transform.position.x"),
                   {{{5, 40.0}, {15, 140.0}, {25, 240.0}}});
    animateChannel(imageLayer.layer.get(), QStringLiteral("transform.position.y"),
                   {{{5, 50.0}, {15, 70.0}, {25, 90.0}}});
    animateChannel(imageLayer.layer.get(), QStringLiteral("transform.rotation"),
                   {{{5, 0.0}, {15, 8.0}, {25, 16.0}}});

    Artifact::ArtifactCompositionInitParams parentParams;
    parentParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("NestedParent")));
    parentParams.setResolution(kCompWidth, kCompHeight);
    parentParams.setFrameRate(kAdvFrameRate);
    parentParams.setDurationFrames(31);
    parentParams.setBackgroundColor(
        ArtifactCore::FloatColor(0.08f, 0.10f, 0.16f, 1.0f));
    const auto parent = projects.createComposition(parentParams);
    check(parent.success, "parent composition creation failed");
    if (!parent.success) {
        projects.closeCurrentProject();
        return failures;
    }

    Artifact::ArtifactCompositionLayerInitParams precompParams;
    const auto precompResult = projects.addLayerToComposition(parent.id, precompParams);
    check(precompResult.success && precompResult.layer.get() != nullptr,
          "precomp layer creation failed");
    if (!precompResult.success || precompResult.layer.get() == nullptr) {
        projects.closeCurrentProject();
        return failures;
    }
    auto* precompLayer =
        dynamic_cast<Artifact::ArtifactCompositionLayer*>(precompResult.layer.get());
    check(precompLayer != nullptr, "precomp layer has an unexpected type");
    if (!precompLayer) {
        projects.closeCurrentProject();
        return failures;
    }
    precompLayer->setLayerName(QStringLiteral("NestedChild"));
    precompLayer->setCompositionId(child.id);
    precompLayer->setTimelineWindow(ArtifactCore::FramePosition(1),
                                    ArtifactCore::FramePosition(31));
    // The parent precomp layer applies its own transform on top of the child
    // composition's output.
    animateChannel(precompLayer, QStringLiteral("transform.position.x"),
                   {{{1, 20.0}, {15, 90.0}, {29, 160.0}}});
    animateChannel(precompLayer, QStringLiteral("transform.rotation"),
                   {{{1, 0.0}, {29, 10.0}}});

    const QString base = captureDir + QStringLiteral("/precomp/nested.png");
    QString failure;
    if (!enqueueAndRender(queue, parent.id, QStringLiteral("NestedParent"), base,
                          3, 27, &failure)) {
        std::printf("FAIL precomp: %s\n", qPrintable(failure));
        projects.closeCurrentProject();
        return failures + 1;
    }

    std::vector<QImage> frames;
    for (const int frame : {3, 15, 25}) {
        const QImage image(framePath(base, frame));
        check(!image.isNull(), "a sampled nested frame is missing");
        frames.push_back(image);
    }
    const FrameStats early = analyzeFrame(frames[0]);
    const FrameStats mid = analyzeFrame(frames[1]);
    const FrameStats late = analyzeFrame(frames[2]);
    check(early.visiblePixels > 1000, "the precomp background does not render");
    check(early.redPixels == 0 && early.greenPixels == 0,
          "the nested layer is visible before its in point");
    check(mid.redPixels > 50 && mid.greenPixels > 50,
          "the nested animated image is missing");
    check(late.redPixels > 50 && late.greenPixels > 50,
          "the nested animated image disappears at the out point");
    check(late.redCentroidX > mid.redCentroidX + 20.0 &&
              late.redCentroidY > mid.redCentroidY + 5.0,
          "the nested animation does not move inside the precomp");
    check(std::fabs(late.greenAngleDegrees - mid.greenAngleDegrees) > 3.0,
          "the nested rotation is not reflected inside the precomp");
    check(writeMontage(frames,
                       captureDir + QStringLiteral("/precomp/nested_montage.png"), 3),
          "could not write the nested montage");
    std::printf(
        "precomp: red=(%.1f,%.1f)->(%.1f,%.1f)->(%.1f,%.1f) counts=%d/%d/%d "
        "angles=%.1f/%.1f/%.1f\n",
        early.redCentroidX, early.redCentroidY, mid.redCentroidX,
        mid.redCentroidY, late.redCentroidX, late.redCentroidY,
        early.redPixels, mid.redPixels, late.redPixels,
        early.greenAngleDegrees, mid.greenAngleDegrees,
        late.greenAngleDegrees);
    projects.closeCurrentProject();
    return failures;
}

int runSaveLoadScenario(const QString& captureDir)
{
    int failures = 0;
    const auto check = [&failures](const bool ok, const char* what) {
        if (!ok) {
            std::printf("FAIL roundtrip: %s\n", what);
            ++failures;
        }
    };

    QTemporaryDir root;
    if (!root.isValid()) return 1;
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", root.path().toUtf8());
    const QString sourcePath = root.path() + QStringLiteral("/assets/roundtrip_source.png");
    if (!writeSourceImage(sourcePath)) return 1;

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    auto* queue = Artifact::ArtifactRenderQueueService::instance();
    if (!queue) return failures + 1;
    queue->removeAllRenderQueues();
    queue->clearCompletedJobHistory();

    ArtifactCore::CompositionID compositionId;
    if (!createAdvancedComposition(
            projects, QStringLiteral("PipelineRoundTripTest"),
            QStringLiteral("RoundTripComp"), 21,
            ArtifactCore::FloatColor(0.10f, 0.12f, 0.18f, 1.0f), kAdvWidth,
            kAdvHeight, &compositionId)) {
        std::printf("FAIL roundtrip: project/composition setup\n");
        return failures + 1;
    }

    Artifact::ArtifactImageInitParams imageParams(QStringLiteral("RoundTripImage"));
    imageParams.setImagePath(sourcePath);
    const auto imageLayer = projects.addLayerToComposition(compositionId, imageParams);
    check(imageLayer.success && imageLayer.layer.get() != nullptr,
          "image layer creation failed");
    if (!imageLayer.success || imageLayer.layer.get() == nullptr) {
        projects.closeCurrentProject();
        return failures;
    }
    imageLayer.layer->setTimelineWindow(ArtifactCore::FramePosition(3),
                                        ArtifactCore::FramePosition(14));
    animateChannel(imageLayer.layer.get(), QStringLiteral("transform.position.x"),
                   {{{3, 30.0}, {8, 110.0}, {13, 190.0}}});
    animateChannel(imageLayer.layer.get(), QStringLiteral("transform.position.y"),
                   {{{3, 40.0}, {8, 70.0}, {13, 100.0}}});
    animateChannel(imageLayer.layer.get(), QStringLiteral("transform.rotation"),
                   {{{3, 0.0}, {8, 8.0}, {13, 16.0}}});
    animateChannel(imageLayer.layer.get(), QStringLiteral("transform.scale.x"),
                   {{{3, 0.5}, {8, 0.7}, {13, 0.9}}});
    animateChannel(imageLayer.layer.get(), QStringLiteral("transform.scale.y"),
                   {{{3, 0.5}, {8, 0.7}, {13, 0.9}}});

    QString failure;
    const QString beforeBase =
        captureDir + QStringLiteral("/roundtrip/before.png");
    if (!enqueueAndRender(queue, compositionId, QStringLiteral("RoundTripComp"),
                          beforeBase, 8, 9, &failure, QStringLiteral("gpu"),
                          kAdvWidth, kAdvHeight)) {
        std::printf("FAIL roundtrip: %s\n", qPrintable(failure));
        projects.closeCurrentProject();
        return failures + 1;
    }
    const FrameStats before = analyzeFrame(QImage(framePath(beforeBase, 8)));
    check(before.redPixels > 50 && before.greenPixels > 50,
          "the animated image is missing before saving");

    const QString savePath =
        captureDir + QStringLiteral("/roundtrip/pipeline_roundtrip.artifact");
    QDir().mkpath(QFileInfo(savePath).absolutePath());
    const auto saveResult = projects.saveToFile(savePath);
    check(saveResult.success, "project save failed");
    std::printf("roundtrip: saved %lld bytes error='%s'\n",
                static_cast<long long>(QFileInfo(savePath).size()),
                qPrintable(saveResult.errorMessage));
    projects.closeCurrentProject();
    const bool reloaded = projects.loadFromFile(savePath);
    check(reloaded, "project reload failed");
    if (!reloaded) {
        projects.closeCurrentProject();
        return failures;
    }
    const auto found = projects.findComposition(compositionId);
    check(found.success, "the composition is missing after reload");
    if (!found.success) {
        projects.closeCurrentProject();
        return failures;
    }

    const QString afterBase = captureDir + QStringLiteral("/roundtrip/after.png");
    if (!enqueueAndRender(queue, compositionId, QStringLiteral("RoundTripComp"),
                          afterBase, 8, 9, &failure, QStringLiteral("gpu"),
                          kAdvWidth, kAdvHeight)) {
        std::printf("FAIL roundtrip: %s\n", qPrintable(failure));
        projects.closeCurrentProject();
        return failures + 1;
    }
    const FrameStats after = analyzeFrame(QImage(framePath(afterBase, 8)));
    check(after.redPixels > 50 && after.greenPixels > 50,
          "the reloaded image layer is not visible");
    check(std::fabs(after.area() - before.area()) <= before.area() * 0.03,
          "the reloaded composition changed the layer area");
    check(std::fabs(after.centroidX - before.centroidX) <= 3.0 &&
              std::fabs(after.centroidY - before.centroidY) <= 3.0,
          "the reloaded composition moved the layer");
    check(std::fabs(after.greenAngleDegrees - before.greenAngleDegrees) <= 5.0,
          "the reloaded composition changed the layer rotation");
    std::printf("roundtrip: before=(%.1f,%.1f)%.0f after=(%.1f,%.1f)%.0f\n",
                before.centroidX, before.centroidY, before.area(),
                after.centroidX, after.centroidY, after.area());
    projects.closeCurrentProject();
    return failures;
}

int runBackendParityScenario(const QString& captureDir)
{
    int failures = 0;
    const auto check = [&failures](const bool ok, const char* what) {
        if (!ok) {
            std::printf("FAIL parity: %s\n", what);
            ++failures;
        }
    };

    QTemporaryDir root;
    if (!root.isValid()) return 1;
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", root.path().toUtf8());
    const QString sourcePath = root.path() + QStringLiteral("/assets/parity_source.png");
    if (!writeSourceImage(sourcePath)) return 1;

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    auto* queue = Artifact::ArtifactRenderQueueService::instance();
    if (!queue) return failures + 1;
    queue->removeAllRenderQueues();
    queue->clearCompletedJobHistory();

    ArtifactCore::CompositionID compositionId;
    if (!createAdvancedComposition(
            projects, QStringLiteral("PipelineParityTest"),
            QStringLiteral("ParityComp"), 2,
            ArtifactCore::FloatColor(0.10f, 0.12f, 0.18f, 1.0f), kAdvWidth,
            kAdvHeight, &compositionId)) {
        std::printf("FAIL parity: project/composition setup\n");
        return failures + 1;
    }
    ArtifactCore::SharedPtr<Artifact::ArtifactAbstractLayer> layer;
    check(addStaticImageLayer(projects, compositionId, sourcePath,
                              QStringLiteral("ParityImage"), &layer),
          "static image layer creation failed");
    if (!layer.get()) {
        projects.closeCurrentProject();
        return failures;
    }

    QString failure;
    const QString gpuBase = captureDir + QStringLiteral("/parity/gpu.png");
    const QString cpuBase = captureDir + QStringLiteral("/parity/cpu.png");
    const bool gpuOk = enqueueAndRender(
        queue, compositionId, QStringLiteral("ParityComp"), gpuBase, 1, 2,
        &failure, QStringLiteral("gpu"), kAdvWidth, kAdvHeight);
    check(gpuOk, "gpu render failed");
    const bool cpuOk = enqueueAndRender(
        queue, compositionId, QStringLiteral("ParityComp"), cpuBase, 1, 2,
        &failure, QStringLiteral("cpu"), kAdvWidth, kAdvHeight);
    check(cpuOk, "cpu render failed");
    if (!gpuOk || !cpuOk) {
        projects.closeCurrentProject();
        return failures;
    }

    const FrameStats gpu = analyzeFrame(QImage(framePath(gpuBase, 1)));
    const FrameStats cpu = analyzeFrame(QImage(framePath(cpuBase, 1)));
    check(gpu.redPixels > 50 && gpu.greenPixels > 50,
          "the gpu frame is missing the image layer");
    check(cpu.redPixels > 50 && cpu.greenPixels > 50,
          "the cpu frame is missing the image layer");
    check(std::fabs(cpu.area() - gpu.area()) <= gpu.area() * 0.10,
          "cpu and gpu disagree on the layer area");
    check(std::fabs(cpu.centroidX - gpu.centroidX) <= 4.0 &&
              std::fabs(cpu.centroidY - gpu.centroidY) <= 4.0,
          "cpu and gpu disagree on the layer position");
    std::printf("parity: gpu=(%.1f,%.1f)%.0f cpu=(%.1f,%.1f)%.0f\n",
                gpu.centroidX, gpu.centroidY, gpu.area(), cpu.centroidX,
                cpu.centroidY, cpu.area());
    projects.closeCurrentProject();
    return failures;
}

int runEffectScenario(const QString& captureDir)
{
    int failures = 0;
    const auto check = [&failures](const bool ok, const char* what) {
        if (!ok) {
            std::printf("FAIL effect: %s\n", what);
            ++failures;
        }
    };

    QTemporaryDir root;
    if (!root.isValid()) return 1;
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", root.path().toUtf8());
    const QString sourcePath = root.path() + QStringLiteral("/assets/effect_source.png");
    if (!writeSourceImage(sourcePath)) return 1;

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    auto* queue = Artifact::ArtifactRenderQueueService::instance();
    if (!queue) return failures + 1;
    queue->removeAllRenderQueues();
    queue->clearCompletedJobHistory();

    ArtifactCore::CompositionID compositionId;
    if (!createAdvancedComposition(
            projects, QStringLiteral("PipelineEffectTest"),
            QStringLiteral("EffectComp"), 2,
            ArtifactCore::FloatColor(0.10f, 0.12f, 0.18f, 1.0f), kAdvWidth,
            kAdvHeight, &compositionId)) {
        std::printf("FAIL effect: project/composition setup\n");
        return failures + 1;
    }
    ArtifactCore::SharedPtr<Artifact::ArtifactAbstractLayer> layer;
    check(addStaticImageLayer(projects, compositionId, sourcePath,
                              QStringLiteral("EffectImage"), &layer),
          "effect image layer creation failed");
    if (!layer.get()) {
        projects.closeCurrentProject();
        return failures;
    }

    QString failure;
    const QString beforeBase = captureDir + QStringLiteral("/effect/before.png");
    const QString afterBase = captureDir + QStringLiteral("/effect/after.png");
    if (!enqueueAndRender(queue, compositionId, QStringLiteral("EffectComp"),
                          beforeBase, 1, 2, &failure, QStringLiteral("gpu"),
                          kAdvWidth, kAdvHeight)) {
        std::printf("FAIL effect: %s\n", qPrintable(failure));
        projects.closeCurrentProject();
        return failures + 1;
    }
    const FrameStats before = analyzeFrame(QImage(framePath(beforeBase, 1)));

    auto brightness = ArtifactCore::makeShared<Artifact::BrightnessEffect>();
    check(brightness != nullptr, "brightness effect allocation failed");
    if (!brightness) {
        projects.closeCurrentProject();
        return failures;
    }
    brightness->setBrightness(0.18f);
    layer->addEffect(brightness);

    if (!enqueueAndRender(queue, compositionId, QStringLiteral("EffectComp"),
                          afterBase, 1, 2, &failure, QStringLiteral("gpu"),
                          kAdvWidth, kAdvHeight)) {
        std::printf("FAIL effect: %s\n", qPrintable(failure));
        projects.closeCurrentProject();
        return failures + 1;
    }
    const FrameStats after = analyzeFrame(QImage(framePath(afterBase, 1)));
    check(before.visiblePixels > 0, "the baseline frame has no layer");
    check(after.visiblePixels > 0, "the effected frame has no layer");
    check(after.meanLuminance > before.meanLuminance * 1.02,
          "the brightness effect did not brighten the layer");
    std::printf("effect: luminance %.1f -> %.1f\n", before.meanLuminance,
                after.meanLuminance);
    projects.closeCurrentProject();
    return failures;
}

int runSelfCheck(const QString& captureDir)
{
    QDir().mkpath(captureDir);
    QTemporaryDir projectRoot;
    if (!projectRoot.isValid()) {
        std::printf("FAIL could not create a temporary project directory\n");
        return 1;
    }
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactStudioPipelineTest"));
    QStandardPaths::setTestModeEnabled(true);

    int failures = 0;
    const auto check = [&failures](const bool condition, const char* what) {
        if (!condition) {
            std::printf("FAIL %s\n", what);
            ++failures;
        }
    };

    const QString sourcePath =
        projectRoot.path() + QStringLiteral("/assets/pipeline_source.png");
    if (!writeSourceImage(sourcePath)) {
        std::printf("FAIL could not write the source image\n");
        return 1;
    }
    std::printf("source image: %s\n", qPrintable(sourcePath));

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    auto* queue = Artifact::ArtifactRenderQueueService::instance();
    if (queue == nullptr) {
        std::printf("FAIL the render queue service is unavailable\n");
        return 1;
    }
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
    projects.createProject(QStringLiteral("PipelineIntegrationTest"), true);
    check(projects.isProjectCreated(), "project creation failed");
    if (!projects.isProjectCreated()) return 1;

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("PipelineAnimatedImage")));
    compositionParams.setResolution(kCompWidth, kCompHeight);
    compositionParams.setFrameRate(kFrameRate);
    compositionParams.setDurationFrames(kLastFrame + 1);
    compositionParams.setBackgroundColor(
        ArtifactCore::FloatColor(0.08f, 0.10f, 0.16f, 1.0f));
    const auto composition = projects.createComposition(compositionParams);
    check(composition.success, "composition creation failed");
    if (!composition.success) return 1;

    // 1. Image loading: a real PNG on disk imported through the image layer
    //    init params, exactly like the application's asset drop path.
    Artifact::ArtifactImageInitParams imageParams(QStringLiteral("AnimatedImage"));
    imageParams.setImagePath(sourcePath);
    const auto layerResult = projects.addLayerToComposition(composition.id, imageParams);
    check(layerResult.success, "image layer creation failed");
    if (!layerResult.success || layerResult.layer.get() == nullptr) return 1;

    auto layer = layerResult.layer;
    check(layer->layerName() == QStringLiteral("AnimatedImage"),
          "image layer name was not applied");

    // 2. Timeline placement: the layer only exists between the in and out
    //    points (the out point is exclusive), so frames outside the window must
    //    render background only.
    layer->setTimelineWindow(ArtifactCore::FramePosition(kKeyIn),
                             ArtifactCore::FramePosition(kKeyOut + 1));
    check(!layer->isActiveAt(ArtifactCore::FramePosition(kKeyIn - 1)),
          "layer is active before its in point");
    check(layer->isActiveAt(ArtifactCore::FramePosition(kKeyMid)),
          "layer is inactive inside its timeline window");
    check(!layer->isActiveAt(ArtifactCore::FramePosition(kKeyOut + 2)),
          "layer is active after its out point");

    // 3. Transform animation through the same property paths the Inspector
    //    edits: three bezier keys per channel, so position, rotation and scale
    //    all interpolate instead of jumping.
    const auto animate = [&](const QString& path,
                             const std::array<std::pair<int, double>, 3>& keys) {
        auto property = layer->getProperty(path);
        check(property.get() != nullptr, "a transform property is missing");
        if (!property) return;
        property->clearKeyFrames();
        property->setAnimatable(true);
        const auto scale = layer->keyframeTimeScale();
        for (const auto& key : keys) {
            property->addKeyFrame(ArtifactCore::RationalTime(key.first, scale),
                                  key.second,
                                  ArtifactCore::InterpolationType::Bezier);
        }
    };
    animate(QStringLiteral("transform.position.x"),
            {{{kKeyIn, kPositionInX},
              {kKeyMid, kPositionMidX},
              {kKeyOut, kPositionOutX}}});
    animate(QStringLiteral("transform.position.y"),
            {{{kKeyIn, kPositionInY},
              {kKeyMid, kPositionMidY},
              {kKeyOut, kPositionOutY}}});
    animate(QStringLiteral("transform.rotation"),
            {{{kKeyIn, kRotationIn},
              {kKeyMid, kRotationMid},
              {kKeyOut, kRotationOut}}});
    animate(QStringLiteral("transform.scale.x"),
            {{{kKeyIn, kScaleIn}, {kKeyMid, kScaleMid}, {kKeyOut, kScaleOut}}});
    animate(QStringLiteral("transform.scale.y"),
            {{{kKeyIn, kScaleIn}, {kKeyMid, kScaleMid}, {kKeyOut, kScaleOut}}});

    const auto positionX = layer->getProperty(QStringLiteral("transform.position.x"));
    const auto rotation = layer->getProperty(QStringLiteral("transform.rotation"));
    const auto scaleX = layer->getProperty(QStringLiteral("transform.scale.x"));
    check(positionX.get() != nullptr && positionX->keyFrameCount() == 3,
          "transform.position.x does not have three keyframes");
    check(rotation.get() != nullptr && rotation->keyFrameCount() == 3,
          "transform.rotation does not have three keyframes");
    check(scaleX.get() != nullptr && scaleX->keyFrameCount() == 3,
          "transform.scale.x does not have three keyframes");
    if (positionX) {
        const auto scale = layer->keyframeTimeScale();
        check(std::fabs(positionX->evaluateValue(
                            ArtifactCore::RationalTime(kKeyIn, scale))
                            .toDouble() -
                        kPositionInX) < 0.5,
              "position is not evaluable at the in point");
    }
    if (rotation) {
        const auto scale = layer->keyframeTimeScale();
        check(std::fabs(rotation->evaluateValue(
                            ArtifactCore::RationalTime(kKeyOut, scale))
                            .toDouble() -
                        kRotationOut) < 0.5,
              "rotation is not evaluable at the out point");
    }
    if (scaleX) {
        const auto scale = layer->keyframeTimeScale();
        check(std::fabs(scaleX->evaluateValue(
                            ArtifactCore::RationalTime(kKeyMid, scale))
                            .toDouble() -
                        kScaleMid) < 0.01,
              "scale is not evaluable in the middle");
    }

    // 4. Render queue registration and final render. The job frame range is
    //    half-open, so request one frame past the last sampled frame.
    const QString outputBase = captureDir + QStringLiteral("/frames/pipeline.png");
    QString failure;
    if (!enqueueAndRender(queue, composition.id,
                          QStringLiteral("PipelineAnimatedImage"), outputBase,
                          kFirstFrame, kLastFrame + 1, &failure)) {
        std::printf("FAIL %s\n", qPrintable(failure));
        return 1;
    }

    int missingFrames = 0;
    for (int frame = kFirstFrame; frame <= kLastFrame; ++frame) {
        if (!QFileInfo::exists(framePath(outputBase, frame))) {
            ++missingFrames;
        }
    }
    check(missingFrames == 0, "the render queue did not write every frame");
    if (missingFrames != 0) {
        std::printf("     missing %d of %d frames\n", missingFrames,
                    kLastFrame - kFirstFrame + 1);
    }

    // 5. Image verification: outside the timeline window only the background
    //    is visible; inside it the layer moves, scales and rotates.
    std::vector<QImage> sampledFrames;
    std::vector<FrameStats> stats;
    for (const int frame : kSampleFrames) {
        const QImage image(framePath(outputBase, frame));
        check(!image.isNull(), "a sampled render frame is missing");
        if (image.isNull()) continue;
        check(image.size() == QSize(kCompWidth, kCompHeight),
              "a rendered frame has the wrong size");
        sampledFrames.push_back(image);
        stats.push_back(analyzeFrame(image));
    }

    const auto statAt = [&stats](const int frame) -> FrameStats {
        for (std::size_t index = 0; index < kSampleFrames.size() &&
                                    index < stats.size(); ++index) {
            if (kSampleFrames[index] == frame) return stats[index];
        }
        return FrameStats{};
    };

    std::printf("frame  visible  centroid        greenAngle  red\n");
    for (std::size_t index = 0; index < stats.size(); ++index) {
        const FrameStats& s = stats[index];
        std::printf("%5d  %7d  (%7.2f,%7.2f)  %8.2f  %5d\n", kSampleFrames[index],
                    s.visiblePixels, s.centroidX, s.centroidY,
                    s.greenAngleDegrees, s.redPixels);
    }

    check(!statAt(kFirstFrame).hasLayer(),
          "a frame before the in point contains layer pixels");
    check(!statAt(kLastFrame).hasLayer(),
          "a frame after the out point contains layer pixels");
    for (const int frame : {kKeyIn, kKeyMid, kKeyOut}) {
        check(statAt(frame).visiblePixels > 2000,
              "an in-range frame is missing the image layer");
        check(statAt(frame).redPixels > 100,
              "the loaded image's red quadrant is not visible");
        check(statAt(frame).greenPixels > 100,
              "the loaded image's green quadrant is not visible");
    }

    const FrameStats inStats = statAt(kKeyIn);
    const FrameStats midStats = statAt(kKeyMid);
    const FrameStats outStats = statAt(kKeyOut);
    check(inStats.centroidX + 40.0 < midStats.centroidX &&
              midStats.centroidX + 40.0 < outStats.centroidX,
          "the animated position did not move monotonically to the right");
    check(inStats.centroidY + 20.0 < midStats.centroidY &&
              midStats.centroidY + 20.0 < outStats.centroidY,
          "the animated position did not move monotonically downward");
    check(midStats.area() > inStats.area() * 1.6,
          "the middle scale key did not enlarge the layer");
    check(outStats.area() < midStats.area(),
          "the outgoing scale key did not shrink the layer");
    check(inStats.greenAngleDegrees < -10.0 &&
              outStats.greenAngleDegrees - inStats.greenAngleDegrees > 20.0,
          "the animated rotation did not rotate the layer content");

    // 6. The same frame rendered again must reproduce the same image content.
    //    GPU frames are compared by content statistics rather than byte
    //    equality (temporal sampling and driver rounding are free to differ a
    //    little), but the scene must not move or change size between runs.
    const QString repeatBase = captureDir + QStringLiteral("/frames/repeat.png");
    if (enqueueAndRender(queue, composition.id,
                         QStringLiteral("PipelineAnimatedImage"), repeatBase,
                         kKeyMid, kKeyMid + 1, &failure)) {
        const QImage first(framePath(outputBase, kKeyMid));
        const QImage second(framePath(repeatBase, kKeyMid));
        check(!first.isNull() && !second.isNull(),
              "the determinism pass did not produce both frames");
        if (!first.isNull() && !second.isNull()) {
            const FrameStats firstStats = analyzeFrame(first);
            const FrameStats secondStats = analyzeFrame(second);
            check(first.size() == second.size(),
                  "the determinism pass produced a different frame size");
            const double areaDelta = std::fabs(firstStats.area() - secondStats.area());
            check(areaDelta <= firstStats.area() * 0.02,
                  "re-rendering the same frame changed the layer area");
            check(std::fabs(firstStats.centroidX - secondStats.centroidX) <= 3.0 &&
                      std::fabs(firstStats.centroidY - secondStats.centroidY) <= 3.0,
                  "re-rendering the same frame moved the layer");
        }
    } else {
        std::printf("FAIL determinism pass: %s\n", qPrintable(failure));
        ++failures;
    }

    // 7. Artifacts for visual review.
    QFile::copy(sourcePath, captureDir + QStringLiteral("/source_image.png"));
    check(writeMontage(sampledFrames, captureDir + QStringLiteral("/montage.png"), 4),
          "could not write the frame montage");

    QJsonArray frameArray;
    for (std::size_t index = 0; index < stats.size(); ++index) {
        const FrameStats& s = stats[index];
        frameArray.append(QJsonObject{
            {QStringLiteral("frame"), kSampleFrames[index]},
            {QStringLiteral("visiblePixels"), s.visiblePixels},
            {QStringLiteral("centroidX"), s.centroidX},
            {QStringLiteral("centroidY"), s.centroidY},
            {QStringLiteral("greenAngleDegrees"), s.greenAngleDegrees},
            {QStringLiteral("redPixels"), s.redPixels},
            {QStringLiteral("greenPixels"), s.greenPixels}});
    }
    const QJsonObject report{
        {QStringLiteral("compositionWidth"), kCompWidth},
        {QStringLiteral("compositionHeight"), kCompHeight},
        {QStringLiteral("frameRate"), kFrameRate},
        {QStringLiteral("frameRange"),
         QJsonArray{kFirstFrame, kLastFrame}},
        {QStringLiteral("timelineWindow"), QJsonArray{kKeyIn, kKeyOut}},
        {QStringLiteral("filesWritten"), kLastFrame - kFirstFrame + 1},
        {QStringLiteral("frames"), frameArray}};
    QFile reportFile(captureDir + QStringLiteral("/pipeline_analysis.json"));
    if (reportFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        reportFile.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
        reportFile.close();
    } else {
        std::printf("FAIL could not write the analysis report\n");
        ++failures;
    }

    // 8. Advanced scenarios: nested precomp, project round trip, backend
    //    parity and the effect stack.
    failures += runPrecompScenario(captureDir);
    failures += runSaveLoadScenario(captureDir);
    failures += runBackendParityScenario(captureDir);
    failures += runEffectScenario(captureDir);

    std::printf("pipeline artifacts: %s\n", qPrintable(captureDir));
    std::printf("pipeline self-check: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}

void printUsage()
{
    std::printf(
        "ArtifactPipelineIntegrationTest\n"
        "  --self-check             run the image/transform/render-queue pipeline\n"
        "  --scenario NAME          run one scenario: precomp, roundtrip, parity, effect\n"
        "  --capture-dir DIR        where PNG artifacts are written\n");
}

} // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactPipelineIntegrationTest"));
    QCoreApplication::setOrganizationName(QStringLiteral("ArtifactStudio"));
    // Surface qWarning() diagnostics (project importer, precomp sampling) that
    // the application logging path would otherwise swallow.
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext&,
                              const QString& message) {
        if (type == QtWarningMsg || type == QtCriticalMsg ||
            type == QtFatalMsg ||
            (type == QtDebugMsg && message.contains(QStringLiteral("[Importer]")))) {
            std::fprintf(stderr, "[qt] %s\n",
                         message.toLocal8Bit().constData());
        }
    });

    QString captureDir = qEnvironmentVariable("ARTIFACT_PIPELINE_CAPTURE_DIR");
    bool selfCheck = false;
    QString scenario;
    for (int index = 1; index < argc; ++index) {
        const QString argument = QString::fromLocal8Bit(argv[index]);
        if (argument == QStringLiteral("--self-check")) {
            selfCheck = true;
        } else if (argument == QStringLiteral("--scenario") && index + 1 < argc) {
            scenario = QString::fromLocal8Bit(argv[++index]);
        } else if (argument == QStringLiteral("--capture-dir") && index + 1 < argc) {
            captureDir = QString::fromLocal8Bit(argv[++index]);
        } else if (argument == QStringLiteral("--help") ||
                   argument == QStringLiteral("-h")) {
            printUsage();
            return 0;
        }
    }
    if (captureDir.isEmpty()) {
        captureDir = QDir::tempPath() + QStringLiteral("/artifact-pipeline");
    }
    if (!scenario.isEmpty()) {
        QDir().mkpath(captureDir);
        int failures = 0;
        if (scenario == QStringLiteral("precomp")) {
            failures = runPrecompScenario(captureDir);
        } else if (scenario == QStringLiteral("roundtrip")) {
            failures = runSaveLoadScenario(captureDir);
        } else if (scenario == QStringLiteral("parity")) {
            failures = runBackendParityScenario(captureDir);
        } else if (scenario == QStringLiteral("effect")) {
            failures = runEffectScenario(captureDir);
        } else {
            printUsage();
            return 1;
        }
        std::printf("scenario %s: %d failure(s)\n", qPrintable(scenario),
                    failures);
        return failures == 0 ? 0 : 1;
    }
    if (!selfCheck) {
        printUsage();
        return 0;
    }
    return runSelfCheck(captureDir);
}
