#include <QByteArray>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QMatrix4x4>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QtCore/qglobal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

import Artifact.Layer.Particle;
import Artifact.Generator.Particle;
import Artifact.Render.IRenderer;
import Math.Vec;

namespace {

constexpr int Width = 1280;
constexpr int Height = 720;
constexpr int PlaybackFrame = 45;
constexpr int LaterFrame = 60;
constexpr int OrbitDifferencePixelMinimum = 128;
constexpr std::uint64_t MaxDeterminismDifferentPixels = 64;
constexpr std::uint64_t MaxDeterminismChannelDelta = 8;

struct ImageDifference {
    std::uint64_t differentPixels = 0;
    std::uint64_t maxChannelDelta = 0;
};

struct CaptureResult {
    QImage image;
    QString debugState;
    std::size_t aliveParticles = 0;
    std::uint64_t visiblePixels = 0;
    std::uint64_t warmParticlePixels = 0;
};

ImageDifference compareImages(const QImage& leftImage, const QImage& rightImage)
{
    const QImage left = leftImage.convertToFormat(QImage::Format_ARGB32);
    const QImage right = rightImage.convertToFormat(QImage::Format_ARGB32);
    ImageDifference difference;
    if (left.isNull() || right.isNull() || left.size() != right.size()) {
        difference.differentPixels = std::numeric_limits<std::uint64_t>::max();
        difference.maxChannelDelta = std::numeric_limits<std::uint64_t>::max();
        return difference;
    }

    for (int y = 0; y < left.height(); ++y) {
        for (int x = 0; x < left.width(); ++x) {
            const QRgb leftPixel = left.pixel(x, y);
            const QRgb rightPixel = right.pixel(x, y);
            const auto channelDelta = std::max({
                std::abs(qRed(leftPixel) - qRed(rightPixel)),
                std::abs(qGreen(leftPixel) - qGreen(rightPixel)),
                std::abs(qBlue(leftPixel) - qBlue(rightPixel)),
                std::abs(qAlpha(leftPixel) - qAlpha(rightPixel))});
            if (channelDelta > 0) {
                ++difference.differentPixels;
            }
            difference.maxChannelDelta = std::max(
                difference.maxChannelDelta,
                static_cast<std::uint64_t>(channelDelta));
        }
    }
    return difference;
}

std::uint64_t countVisibleParticlePixels(const QImage& source)
{
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    std::uint64_t count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = image.pixel(x, y);
            if (qRed(pixel) > 55 || qGreen(pixel) > 58 || qBlue(pixel) > 78) {
                ++count;
            }
        }
    }
    return count;
}

std::uint64_t countWarmParticlePixels(const QImage& source)
{
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    std::uint64_t count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = image.pixel(x, y);
            if (qAlpha(pixel) > 32 && qRed(pixel) > qBlue(pixel) * 1.35 &&
                qGreen(pixel) > qBlue(pixel) * 1.15) {
                ++count;
            }
        }
    }
    return count;
}

bool prepareRenderer(Artifact::ArtifactIRenderer& renderer)
{
    renderer.initializeHeadless(Width, Height);
    if (!renderer.isInitialized()) {
        return false;
    }
    renderer.setCanvasSize(1920.0f, 1080.0f);
    renderer.setViewportSize(static_cast<float>(Width), static_cast<float>(Height));
    renderer.setClearColor(ArtifactCore::FloatColor(0.018f, 0.024f, 0.04f, 1.0f));
    return true;
}

void setCamera(Artifact::ArtifactIRenderer& renderer, float orbitDegrees)
{
    constexpr float Pi = 3.14159265358979323846f;
    constexpr float CameraDistance = 2800.0f;
    const QVector3D Target(960.0f, 540.0f, 0.0f);
    const float angle = orbitDegrees * Pi / 180.0f;
    const QVector3D eye(
        Target.x() + std::sin(angle) * CameraDistance,
        Target.y(),
        Target.z() + std::cos(angle) * CameraDistance);

    QMatrix4x4 view;
    view.lookAt(eye, Target, QVector3D(0.0f, 1.0f, 0.0f));
    QMatrix4x4 projection;
    projection.perspective(45.0f, static_cast<float>(Width) / Height,
                           10.0f, 20000.0f);
    renderer.set3DCameraMatrices(view, projection);
}

CaptureResult renderFrame(Artifact::ArtifactParticle3DLayer& layer,
                          Artifact::ArtifactIRenderer& renderer,
                          int frameNumber, float orbitDegrees,
                          bool captureImage = true,
                          bool drawDepthOccluder = false,
                          bool particleDepthTest = true)
{
    setCamera(renderer, orbitDegrees);
    renderer.clear();
    layer.goToFrame(frameNumber);
    const auto originalSettings = layer.renderSettings();
    if (drawDepthOccluder) {
        auto depthSettings = originalSettings;
        depthSettings.depthTest = particleDepthTest;
        depthSettings.depthWrite = false;
        layer.setRenderSettings(depthSettings);

        QMatrix4x4 occluderModel;
        occluderModel.translate(0.0f, 0.0f, 800.0f);
        renderer.draw3DCard(QRectF(0.0, 0.0, 1920.0, 1080.0),
                            occluderModel,
                            ArtifactCore::FloatColor(0.04f, 0.08f, 0.85f, 1.0f),
                            1.0f, true);
    }
    layer.draw(&renderer);
    renderer.flushAndWait();
    if (drawDepthOccluder) {
        layer.setRenderSettings(originalSettings);
    }

    CaptureResult result;
    result.debugState = renderer.particleDebugState();
    if (const auto* system = layer.particleSystem()) {
        for (const auto& emitter : system->emitters()) {
            if (emitter) {
                result.aliveParticles += emitter->particles().size();
            }
        }
    }
    if (captureImage) {
        result.image = renderer.readbackToImage().convertToFormat(QImage::Format_ARGB32);
        result.visiblePixels = countVisibleParticlePixels(result.image);
        result.warmParticlePixels = countWarmParticlePixels(result.image);
    }
    return result;
}

bool hasQueued3DParticleDraw(const CaptureResult& capture)
{
    return capture.aliveParticles > 0 &&
           capture.debugState.contains(QStringLiteral("state=queued")) &&
           capture.debugState.contains(QStringLiteral("cameraMode=3d"));
}

bool saveCapture(const CaptureResult& capture, const QString& path,
                 QFile& report)
{
    if (capture.image.isNull() || capture.visiblePixels == 0 ||
        !hasQueued3DParticleDraw(capture)) {
        qCritical("3D particle capture did not produce a visible queued GPU draw");
        return false;
    }
    if (!capture.image.save(path, "PNG")) {
        qCritical("Could not save 3D particle capture");
        return false;
    }
    report.write(QStringLiteral(
        "image=%1 alive=%2 visible_pixels=%3 renderer=%4\n")
        .arg(path)
        .arg(static_cast<qulonglong>(capture.aliveParticles))
        .arg(static_cast<qulonglong>(capture.visiblePixels))
        .arg(capture.debugState).toUtf8());
    return true;
}

int capture3DParticles(const QString& outputDirectory)
{
    QDir output(outputDirectory);
    if (!output.mkpath(QStringLiteral("."))) {
        qCritical("Could not create 3D particle capture directory");
        return 2;
    }

    QFile report(output.filePath(QStringLiteral("particle_3d_report.txt")));
    if (!report.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical("Could not open 3D particle capture report");
        return 3;
    }

    Artifact::ArtifactIRenderer renderer;
    if (!prepareRenderer(renderer)) {
        qCritical("Could not initialize the headless GPU renderer");
        return 4;
    }

    Artifact::ArtifactParticle3DLayer playbackLayer;
    playbackLayer.loadPreset(QStringLiteral("explosion"));
    playbackLayer.resetParticleSystem();
    playbackLayer.play();

    CaptureResult playbackFrame;
    for (int frame = 1; frame <= PlaybackFrame; ++frame) {
        playbackFrame = renderFrame(playbackLayer, renderer, frame, 0.0f,
                                    frame == PlaybackFrame);
        if (frame == PlaybackFrame &&
            (playbackFrame.image.isNull() ||
             !playbackFrame.debugState.contains(QStringLiteral("cameraMode=3d")))) {
            renderer.destroy();
            return 5;
        }
    }

    const QString frontPath = output.filePath(QStringLiteral("front_view_frame_45.png"));
    if (!saveCapture(playbackFrame, frontPath, report)) {
        renderer.destroy();
        return 6;
    }

    const CaptureResult depthOccludedFrame = renderFrame(
        playbackLayer, renderer, PlaybackFrame, 0.0f, true, true);
    const QString depthPath = output.filePath(
        QStringLiteral("depth_occluded_frame_45.png"));
    if (!saveCapture(depthOccludedFrame, depthPath, report)) {
        renderer.destroy();
        return 15;
    }
    if (!depthOccludedFrame.debugState.contains(QStringLiteral("depthTest=1")) ||
        playbackFrame.warmParticlePixels == 0 ||
        depthOccludedFrame.warmParticlePixels != 0) {
        qCritical("3D particle depth test did not fully respect the nearer opaque card");
        renderer.destroy();
        return 16;
    }

    const CaptureResult depthControlFrame = renderFrame(
        playbackLayer, renderer, PlaybackFrame, 0.0f, true, true, false);
    const QString depthControlPath = output.filePath(
        QStringLiteral("depth_disabled_control_frame_45.png"));
    if (!saveCapture(depthControlFrame, depthControlPath, report)) {
        renderer.destroy();
        return 17;
    }
    const auto depthControlDifference = compareImages(
        depthOccludedFrame.image, depthControlFrame.image);
    if (!depthControlFrame.debugState.contains(QStringLiteral("depthTest=0")) ||
        depthControlDifference.differentPixels < OrbitDifferencePixelMinimum) {
        qCritical("Depth-disabled control did not reveal particles over the card");
        renderer.destroy();
        return 18;
    }
    const auto depthOcclusionDifference = compareImages(
        playbackFrame.image, depthOccludedFrame.image);
    report.write(QStringLiteral(
        "depth_occlusion_differing_pixels=%1 depth_control_differing_pixels=%2 "
        "warm_pixels_before=%3 warm_pixels_after=%4 depth_test=enabled\n")
        .arg(static_cast<qulonglong>(depthOcclusionDifference.differentPixels))
        .arg(static_cast<qulonglong>(depthControlDifference.differentPixels))
        .arg(static_cast<qulonglong>(playbackFrame.warmParticlePixels))
        .arg(static_cast<qulonglong>(depthOccludedFrame.warmParticlePixels))
        .toUtf8());

    const CaptureResult orbitFrame = renderFrame(
        playbackLayer, renderer, PlaybackFrame, 35.0f);
    const QString orbitPath = output.filePath(QStringLiteral("orbit_view_frame_45.png"));
    if (!saveCapture(orbitFrame, orbitPath, report)) {
        renderer.destroy();
        return 7;
    }
    const auto orbitDifference = compareImages(playbackFrame.image, orbitFrame.image);
    if (orbitDifference.differentPixels < OrbitDifferencePixelMinimum) {
        qCritical("Camera orbit did not produce a visibly different 3D particle image");
        renderer.destroy();
        return 8;
    }

    playbackLayer.setPosition3D(
        ArtifactCore::Coordinates::LayerLocalPoint3{160.0f, 0.0f, 0.0f});
    const CaptureResult transformedFrame = renderFrame(
        playbackLayer, renderer, PlaybackFrame, 0.0f);
    const QString transformPath = output.filePath(
        QStringLiteral("layer_transform_frame_45.png"));
    if (!saveCapture(transformedFrame, transformPath, report)) {
        renderer.destroy();
        return 9;
    }
    const auto transformDifference = compareImages(
        playbackFrame.image, transformedFrame.image);
    if (transformDifference.differentPixels < OrbitDifferencePixelMinimum) {
        qCritical("3D layer transform did not produce a visibly different image");
        renderer.destroy();
        return 10;
    }

    playbackLayer.setPosition3D(
        ArtifactCore::Coordinates::LayerLocalPoint3{0.0f, 0.0f, 0.0f});
    const CaptureResult laterFrame = renderFrame(
        playbackLayer, renderer, LaterFrame, 0.0f);
    const QString laterPath = output.filePath(QStringLiteral("later_frame_60.png"));
    if (!saveCapture(laterFrame, laterPath, report)) {
        renderer.destroy();
        return 11;
    }
    const auto temporalDifference = compareImages(playbackFrame.image, laterFrame.image);
    if (temporalDifference.differentPixels < OrbitDifferencePixelMinimum) {
        qCritical("3D particle animation did not change between capture frames");
        renderer.destroy();
        return 12;
    }

    Artifact::ArtifactParticle3DLayer directSeekLayer;
    directSeekLayer.loadPreset(QStringLiteral("explosion"));
    directSeekLayer.resetParticleSystem();
    directSeekLayer.play();
    const CaptureResult directSeekFrame = renderFrame(
        directSeekLayer, renderer, PlaybackFrame, 0.0f);
    const QString directPath = output.filePath(QStringLiteral("direct_seek_frame_45.png"));
    if (!saveCapture(directSeekFrame, directPath, report)) {
        renderer.destroy();
        return 13;
    }

    Artifact::ArtifactParticle3DLayer revisitLayer;
    revisitLayer.loadPreset(QStringLiteral("explosion"));
    revisitLayer.resetParticleSystem();
    revisitLayer.play();
    const CaptureResult revisitFrame60 = renderFrame(
        revisitLayer, renderer, LaterFrame, 0.0f, false);
    if (!hasQueued3DParticleDraw(revisitFrame60)) {
        qCritical("3D particle revisit did not queue a draw at frame 60");
        renderer.destroy();
        return 20;
    }
    const CaptureResult revisitFrame10 = renderFrame(
        revisitLayer, renderer, 10, 0.0f, false);
    if (!hasQueued3DParticleDraw(revisitFrame10)) {
        qCritical("3D particle revisit did not queue a draw at frame 10");
        renderer.destroy();
        return 21;
    }
    const CaptureResult revisitFrame = renderFrame(
        revisitLayer, renderer, PlaybackFrame, 0.0f);
    const QString revisitPath = output.filePath(
        QStringLiteral("revisited_frame_45.png"));
    if (!saveCapture(revisitFrame, revisitPath, report)) {
        renderer.destroy();
        return 19;
    }

    const auto determinismDifference = compareImages(
        playbackFrame.image, directSeekFrame.image);
    const auto revisitDifference = compareImages(
        playbackFrame.image, revisitFrame.image);
    report.write(QStringLiteral(
        "orbit_differing_pixels=%1 transform_differing_pixels=%2 "
        "temporal_differing_pixels=%3 direct_seek_differing_pixels=%4 "
        "direct_seek_max_channel_delta=%5 revisit_differing_pixels=%6 "
        "revisit_max_channel_delta=%7 determinism_tolerance=%8px/%9\n")
        .arg(static_cast<qulonglong>(orbitDifference.differentPixels))
        .arg(static_cast<qulonglong>(transformDifference.differentPixels))
        .arg(static_cast<qulonglong>(temporalDifference.differentPixels))
        .arg(static_cast<qulonglong>(determinismDifference.differentPixels))
        .arg(static_cast<qulonglong>(determinismDifference.maxChannelDelta))
        .arg(static_cast<qulonglong>(revisitDifference.differentPixels))
        .arg(static_cast<qulonglong>(revisitDifference.maxChannelDelta))
        .arg(MaxDeterminismDifferentPixels)
        .arg(MaxDeterminismChannelDelta).toUtf8());

    renderer.destroy();
    if (determinismDifference.differentPixels > MaxDeterminismDifferentPixels ||
        determinismDifference.maxChannelDelta > MaxDeterminismChannelDelta ||
        revisitDifference.differentPixels > MaxDeterminismDifferentPixels ||
        revisitDifference.maxChannelDelta > MaxDeterminismChannelDelta) {
        qCritical("3D particle seek or revisit exceeded deterministic image tolerance");
        return 14;
    }

    qInfo("3D particle PNG captures and image checks passed");
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QGuiApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    if (arguments.size() == 3 && arguments[1] == QStringLiteral("--capture")) {
        return capture3DParticles(arguments[2]);
    }
    qCritical("Usage: ArtifactParticle3DPlayground --capture <output-directory>");
    return 1;
}
