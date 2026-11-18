#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QElapsedTimer>
#include <QPaintEvent>
#include <QShowEvent>
#include <QString>
#include <QTimerEvent>
#include <QVariant>
#include <QtGlobal>
#include <QWidget>

#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

import Artifact.Layer.Particle;
import Artifact.Layer.Shape;
import Artifact.Layer.Text;
import Artifact.Generator.Particle;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Render.DiligentDeviceManager;
import Artifact.Render.IRenderer;
import Artifact.Render.OffscreenComposition;
import Color.Float;
import Shape.Types;
import Frame.Position;
import Graphics.ParticleData;
import Core.ArtifactArray;
import Memory.SharedPtr;
import Utils.Id;
import Utils.String.UniString;

namespace {

constexpr int CanvasWidth = 1920;
constexpr int CanvasHeight = 1080;

class ParticleLayerWindow final : public QWidget {
public:
    ParticleLayerWindow()
    {
        setWindowTitle(QStringLiteral("Artifact 2D ParticleLayer Test"));
        setAttribute(Qt::WA_NativeWindow);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setFixedSize(1280, 720);
        setFocusPolicy(Qt::StrongFocus);

        loadPreset(0);
        timerId_ = startTimer(33, Qt::PreciseTimer);
    }

    ~ParticleLayerWindow() override
    {
        if (rendererReady_) {
            renderer_.destroy();
        }
    }

protected:
    void showEvent(QShowEvent* event) override
    {
        QWidget::showEvent(event);
        if (!rendererAttempted_) {
            rendererAttempted_ = true;
            renderer_.initialize(this);
            rendererReady_ = renderer_.isInitialized();
            if (!rendererReady_) {
                setWindowTitle(QStringLiteral(
                    "Artifact 2D ParticleLayer Test — Diligent initialization failed"));
                return;
            }
            renderer_.setCanvasSize(static_cast<float>(CanvasWidth),
                                    static_cast<float>(CanvasHeight));
            renderer_.setViewportSize(
                static_cast<float>(width() * devicePixelRatioF()),
                static_cast<float>(height() * devicePixelRatioF()));
            renderer_.fitToViewport(0.0f);
            update();
        }
    }

    void paintEvent(QPaintEvent*) override
    {
        if (!rendererReady_) return;

        renderer_.setViewportSize(
            static_cast<float>(width() * devicePixelRatioF()),
            static_cast<float>(height() * devicePixelRatioF()));
        renderer_.setClearColor(ArtifactCore::FloatColor(0.018f, 0.024f, 0.04f, 1.0f));
        renderer_.clear();

        layer_.goToFrame(frame_);
        layer_.draw(&renderer_);

        renderer_.flush();
        renderer_.present();
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        switch (event->key()) {
        case Qt::Key_Space:
            paused_ = !paused_;
            break;
        case Qt::Key_R:
            frame_ = 0;
            layer_.resetParticleSystem();
            break;
        case Qt::Key_1:
            loadPreset(0);
            break;
        case Qt::Key_2:
            loadPreset(1);
            break;
        case Qt::Key_3:
            loadPreset(2);
            break;
        case Qt::Key_4:
            loadPreset(3);
            break;
        case Qt::Key_5:
            loadPreset(4);
            break;
        case Qt::Key_6:
            loadFirework();
            break;
        case Qt::Key_7:
            loadWindLeaves();
            break;
        case Qt::Key_Up:
        case Qt::Key_Plus:
            adjustEmissionRate(25.0);
            break;
        case Qt::Key_Down:
        case Qt::Key_Minus:
            adjustEmissionRate(-25.0);
            break;
        default:
            QWidget::keyPressEvent(event);
            return;
        }
        update();
    }

    void timerEvent(QTimerEvent* event) override
    {
        if (event->timerId() != timerId_) {
            QWidget::timerEvent(event);
            return;
        }
        if (!paused_) {
            ++frame_;
            update();
        }
    }

private:
    void loadPreset(int index)
    {
        static constexpr std::array<const char*, 5> Presets{
            "fountain", "fire", "smoke", "rain", "snow"};
        const int selectedIndex = std::clamp(
            index, 0, static_cast<int>(Presets.size()) - 1);
        layer_.loadPreset(
            QString::fromLatin1(Presets[static_cast<std::size_t>(selectedIndex)]));
        frame_ = 0;
        layer_.resetParticleSystem();
        setWindowTitle(QStringLiteral(
            "Artifact 2D ParticleLayer Test — %1 | 1-5 preset, Space pause, R reset, Up/Down rate")
                           .arg(layer_.presetName()));
    }

    void loadFirework()
    {
        layer_.loadPreset(QStringLiteral("explosion"));
        frame_ = 0;
        layer_.resetParticleSystem();
        setWindowTitle(QStringLiteral(
            "Artifact 2D ParticleLayer Test — Firework | 6 burst, Space pause, R reset"));
    }

    void loadWindLeaves()
    {
        layer_.loadPreset(QStringLiteral("leaves"));
        layer_.setLayerPropertyValue(QStringLiteral("particle.physics.windDirectionX"),
                                     QVariant(1.0));
        layer_.setLayerPropertyValue(QStringLiteral("particle.physics.windDirectionY"),
                                     QVariant(0.0));
        layer_.setLayerPropertyValue(QStringLiteral("particle.physics.windStrength"),
                                     QVariant(80.0));
        layer_.setLayerPropertyValue(QStringLiteral("particle.physics.turbulenceAmplitude"),
                                     QVariant(35.0));
        layer_.setLayerPropertyValue(QStringLiteral("particle.physics.turbulenceFrequency"),
                                     QVariant(0.45));
        layer_.setLayerPropertyValue(QStringLiteral("particle.physics.turbulenceEvolution"),
                                     QVariant(0.6));
        layer_.setLayerPropertyValue(QStringLiteral("particle.physics.drag"),
                                     QVariant(0.1));
        frame_ = 0;
        layer_.resetParticleSystem();
        setWindowTitle(QStringLiteral(
            "Artifact 2D ParticleLayer Test — Wind Leaves | 7 leaves, Space pause, R reset"));
    }

    void adjustEmissionRate(double delta)
    {
        if (layer_.emitterCount() <= 0) return;
        auto* system = layer_.particleSystem();
        if (!system || system->emitters().empty() || !system->emitters().front()) return;
        const double currentRate = system->emitters().front()->params().rate;
        const double nextRate = std::clamp(currentRate + delta, 0.0, 2000.0);
        layer_.setLayerPropertyValue(
            QStringLiteral("particle.emitter.rate"), QVariant(nextRate));
        frame_ = 0;
        layer_.resetParticleSystem();
    }

    Artifact::ArtifactParticleLayer layer_;
    Artifact::ArtifactIRenderer renderer_;
    int timerId_ = 0;
    std::int64_t frame_ = 0;
    bool paused_ = false;
    bool rendererAttempted_ = false;
    bool rendererReady_ = false;
};

} // namespace

static int captureFirework(const QString& outputDirectory)
{
    QDir output(outputDirectory);
    if (!output.mkpath(QStringLiteral("."))) {
        qCritical("Could not create firework capture directory");
        return 2;
    }

    constexpr std::array<std::int64_t, 5> CaptureFrames{33, 39, 45, 51, 57};
    Artifact::ArtifactIRenderer softwareRenderer;
    const auto captureAtFrame = [&softwareRenderer](
        Artifact::ArtifactParticleLayer& layer,
        std::int64_t frameNumber,
        QImage& frame) {
        layer.goToFrame(frameNumber);
        layer.draw(&softwareRenderer);
        return layer.getCachedFrame(frameNumber, frame) && !frame.isNull();
    };
    constexpr std::uint64_t MaxDifferentPixels = 64;
    constexpr std::uint64_t MaxChannelDelta = 8;
    const auto imagesMatch = [](const std::array<std::uint64_t, 2>& difference) {
        return difference[0] <= MaxDifferentPixels &&
               difference[1] <= MaxChannelDelta;
    };
    const auto hasVisiblePixel = [](const QImage& frame) {
        for (int y = 0; y < frame.height(); ++y) {
            const auto* row = reinterpret_cast<const QRgb*>(frame.constScanLine(y));
            for (int x = 0; x < frame.width(); ++x) {
                if (qAlpha(row[x]) > 0) return true;
            }
        }
        return false;
    };
    const auto imageDifference = [](const QImage& left, const QImage& right) {
        std::array<std::uint64_t, 2> result{};
        if (left.size() != right.size() || left.format() != right.format()) {
            result[0] = std::numeric_limits<std::uint64_t>::max();
            result[1] = std::numeric_limits<std::uint64_t>::max();
            return result;
        }
        for (int y = 0; y < left.height(); ++y) {
            const auto* leftRow = reinterpret_cast<const QRgb*>(left.constScanLine(y));
            const auto* rightRow = reinterpret_cast<const QRgb*>(right.constScanLine(y));
            for (int x = 0; x < left.width(); ++x) {
                const int delta = std::max({
                    std::abs(qRed(leftRow[x]) - qRed(rightRow[x])),
                    std::abs(qGreen(leftRow[x]) - qGreen(rightRow[x])),
                    std::abs(qBlue(leftRow[x]) - qBlue(rightRow[x])),
                    std::abs(qAlpha(leftRow[x]) - qAlpha(rightRow[x]))});
                if (delta > 0) ++result[0];
                result[1] = std::max(result[1], static_cast<std::uint64_t>(delta));
            }
        }
        return result;
    };

    Artifact::ArtifactParticleLayer playbackLayer;
    playbackLayer.loadPreset(QStringLiteral("explosion"));
    playbackLayer.resetParticleSystem();
    playbackLayer.play();
    QImage playbackReference;
    for (std::int64_t frameNumber = 1; frameNumber <= CaptureFrames.back(); ++frameNumber) {
        QImage frame;
        if (!captureAtFrame(playbackLayer, frameNumber, frame)) {
            qCritical("Firework layer did not produce a frame during playback");
            return 3;
        }
        const auto capture = std::find(CaptureFrames.begin(), CaptureFrames.end(), frameNumber);
        if (capture == CaptureFrames.end()) continue;

        const auto* system = playbackLayer.particleSystem();
        const std::size_t aliveCount = system && !system->emitters().empty()
            && system->emitters().front()
            ? system->emitters().front()->particles().size() : 0;
        if (aliveCount == 0 || !hasVisiblePixel(frame)) {
            qCritical("Firework playback produced an empty image");
            return 4;
        }

        const auto index = static_cast<std::size_t>(capture - CaptureFrames.begin());
        const QString path = output.filePath(
            QStringLiteral("firework_%1.png").arg(static_cast<int>(index), 2, 10, QLatin1Char('0')));
        if (!frame.save(path, "PNG")) {
            qCritical("Could not save firework capture");
            return 5;
        }
        if (frameNumber == CaptureFrames[2]) playbackReference = frame;
    }

    Artifact::ArtifactParticleLayer directLayer;
    directLayer.loadPreset(QStringLiteral("explosion"));
    directLayer.resetParticleSystem();
    directLayer.play();
    QImage directFrame;
    if (!captureAtFrame(directLayer, CaptureFrames[2], directFrame)) {
        qCritical("Firework direct seek did not produce a frame");
        return 61;
    }
    const auto directDifference = imageDifference(playbackReference, directFrame);
    if (!imagesMatch(directDifference)) {
        return 62;
    }

    Artifact::ArtifactParticleLayer revisitLayer;
    revisitLayer.loadPreset(QStringLiteral("explosion"));
    revisitLayer.resetParticleSystem();
    revisitLayer.play();
    QImage scratchFrame;
    QImage revisitFrame;
    if (!captureAtFrame(revisitLayer, 60, scratchFrame) ||
        !captureAtFrame(revisitLayer, 10, scratchFrame) ||
        !captureAtFrame(revisitLayer, CaptureFrames[2], revisitFrame)) {
        return 7;
    }
    const auto revisitDifference = imageDifference(playbackReference, revisitFrame);
    if (!imagesMatch(revisitDifference)) return 8;

    QFile report(output.filePath(QStringLiteral("determinism_report.txt")));
    if (!report.open(QIODevice::WriteOnly | QIODevice::Text)) return 9;
    report.write(QStringLiteral(
        "frame=%1 direct_seek_differing_pixels=%2 direct_seek_max_channel_delta=%3 "
        "revisit_differing_pixels=%4 revisit_max_channel_delta=%5 tolerance=%6px/%7\n")
        .arg(CaptureFrames[2]).arg(directDifference[0]).arg(directDifference[1])
        .arg(revisitDifference[0]).arg(revisitDifference[1])
        .arg(MaxDifferentPixels).arg(MaxChannelDelta).toUtf8());
    return 0;
}

static int captureWindLeaves(const QString& outputDirectory)
{
    QDir output(outputDirectory);
    if (!output.mkpath(QStringLiteral("."))) {
        qCritical("Could not create wind-leaves capture directory");
        return 2;
    }

    constexpr std::array<std::int64_t, 5> CaptureFrames{30, 60, 90, 120, 150};
    Artifact::ArtifactParticleLayer layer;
    layer.loadPreset(QStringLiteral("leaves"));
    layer.setLayerPropertyValue(QStringLiteral("particle.physics.windDirectionX"),
                                QVariant(1.0));
    layer.setLayerPropertyValue(QStringLiteral("particle.physics.windDirectionY"),
                                QVariant(0.0));
    layer.setLayerPropertyValue(QStringLiteral("particle.physics.windStrength"),
                                QVariant(80.0));
    layer.setLayerPropertyValue(QStringLiteral("particle.physics.turbulenceAmplitude"),
                                QVariant(35.0));
    layer.setLayerPropertyValue(QStringLiteral("particle.physics.turbulenceFrequency"),
                                QVariant(0.45));
    layer.setLayerPropertyValue(QStringLiteral("particle.physics.turbulenceEvolution"),
                                QVariant(0.6));
    layer.setLayerPropertyValue(QStringLiteral("particle.physics.drag"),
                                QVariant(0.1));
    layer.resetParticleSystem();
    layer.play();

    Artifact::ArtifactIRenderer softwareRenderer;
    QImage firstFrame;
    QImage lastFrame;
    QFile report(output.filePath(QStringLiteral("wind_leaves_report.txt")));
    if (!report.open(QIODevice::WriteOnly | QIODevice::Text)) return 3;
    for (std::size_t index = 0; index < CaptureFrames.size(); ++index) {
        const auto frameNumber = CaptureFrames[index];
        layer.goToFrame(frameNumber);
        layer.draw(&softwareRenderer);
        QImage frame;
        if (!layer.getCachedFrame(frameNumber, frame) || frame.isNull()) {
            qCritical("Wind leaves did not produce a frame");
            return 4;
        }
        const auto* system = layer.particleSystem();
        const std::size_t aliveCount = system && !system->emitters().empty()
            && system->emitters().front()
            ? system->emitters().front()->particles().size() : 0;
        if (aliveCount == 0) {
            qCritical("Wind leaves produced no live particles");
            return 5;
        }
        if (index == 0) firstFrame = frame;
        if (index + 1 == CaptureFrames.size()) lastFrame = frame;
        const QString path = output.filePath(
            QStringLiteral("wind_leaves_%1.png").arg(static_cast<int>(index), 2, 10,
                                                       QLatin1Char('0')));
        if (!frame.save(path, "PNG")) return 6;
        report.write(QStringLiteral("frame=%1 alive=%2 image=%3\n")
                         .arg(frameNumber).arg(aliveCount).arg(path).toUtf8());
    }

    std::uint64_t differingPixels = 0;
    if (firstFrame.size() == lastFrame.size() &&
        firstFrame.format() == lastFrame.format()) {
        for (int y = 0; y < firstFrame.height(); ++y) {
            const auto* firstRow = reinterpret_cast<const QRgb*>(firstFrame.constScanLine(y));
            const auto* lastRow = reinterpret_cast<const QRgb*>(lastFrame.constScanLine(y));
            for (int x = 0; x < firstFrame.width(); ++x) {
                if (firstRow[x] != lastRow[x]) ++differingPixels;
            }
        }
    }
    if (differingPixels == 0) {
        qCritical("Wind leaves frames did not change over time");
        return 7;
    }
    report.write(QStringLiteral("first_to_last_differing_pixels=%1\n")
                     .arg(differingPixels).toUtf8());
    return 0;
}

static int captureGpuParticle(const QString& outputDirectory)
{
    QDir output(outputDirectory);
    if (!output.mkpath(QStringLiteral("."))) return 2;

    constexpr int Width = 1600;
    constexpr int Height = 900;
    Artifact::ArtifactIRenderer renderer;
    renderer.initializeHeadless(Width, Height);
    if (!renderer.isInitialized()) {
        qCritical("Could not initialize the headless GPU renderer");
        return 3;
    }
    renderer.setCanvasSize(static_cast<float>(Width), static_cast<float>(Height));
    renderer.setViewportSize(static_cast<float>(Width), static_cast<float>(Height));
    renderer.setClearColor(ArtifactCore::FloatColor(0.018f, 0.024f, 0.04f, 1.0f));
    renderer.clear();
    renderer.drawSolidRect(32.0f, 32.0f, 96.0f, 96.0f,
                           ArtifactCore::FloatColor(0.0f, 1.0f, 0.1f, 1.0f));

    ArtifactCore::ParticleRenderData data;
    data.options.blend = ArtifactCore::ParticleBlendPolicy::Alpha;
    data.options.billboard = ArtifactCore::ParticleBillboardPolicy::None;
    ArtifactCore::ParticleVertex particle{};
    particle.px = Width * 0.5f;
    particle.py = Height * 0.5f;
    particle.pz = 0.0f;
    particle.r = 1.0f;
    particle.g = 0.08f;
    particle.b = 0.25f;
    particle.a = 1.0f;
    particle.size = 30.0f;
    particle.lifetime = 10.0f;
    data.particles.push_back(particle);
    renderer.drawParticles(data);
    renderer.flushAndWait();

    const QString debug = renderer.particleDebugState();
    const QImage image = renderer.readbackToImage().convertToFormat(QImage::Format_ARGB32);
    std::uint64_t particlePixels = 0;
    std::uint64_t greenRectPixels = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = image.pixel(x, y);
            if (x < 160 && y < 160 && qGreen(pixel) > 200 &&
                qRed(pixel) < 30 && qBlue(pixel) < 120) ++greenRectPixels;
            if (qRed(pixel) > 180 && qGreen(pixel) < 80 && qBlue(pixel) < 150)
                ++particlePixels;
        }
    }

    const QString imagePath = output.filePath(QStringLiteral("gpu_particle.png"));
    if (image.isNull() || !image.save(imagePath, "PNG")) {
        renderer.destroy();
        qCritical("Could not read back and save the GPU particle image");
        return 4;
    }
    QFile report(output.filePath(QStringLiteral("gpu_particle_report.txt")));
    if (!report.open(QIODevice::WriteOnly | QIODevice::Text)) {
        renderer.destroy();
        return 5;
    }

    renderer.clear();
    renderer.drawSolidRect(32.0f, 32.0f, 96.0f, 96.0f,
                           ArtifactCore::FloatColor(0.0f, 1.0f, 0.1f, 1.0f));
    Artifact::ArtifactParticleLayer layer;
    layer.loadPreset(QStringLiteral("fountain"));
    if (!layer.setLayerPropertyValue(QStringLiteral("particle.emitter.positionX"),
                                     QVariant(800.0)) ||
    !layer.setLayerPropertyValue(QStringLiteral("particle.emitter.positionY"),
                                     QVariant(450.0)) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.emitter.rate"),
                                     QVariant(100.0)) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.emitter.speedMin"),
                                     QVariant(0.0)) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.emitter.speedMax"),
                                     QVariant(0.0)) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.emitter.scaleMin"),
                                     QVariant(30.0)) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.emitter.scaleMax"),
                                     QVariant(30.0))) {
        renderer.destroy();
        return 7;
    }
    layer.resetParticleSystem();
    layer.play();
    layer.goToFrame(1);
    layer.draw(&renderer);
    renderer.flushAndWait();
    const QString layerDebug = renderer.particleDebugState();
    const QImage layerImage = renderer.readbackToImage().convertToFormat(QImage::Format_ARGB32);
    std::uint64_t layerParticlePixels = 0;
    std::uint64_t layerGreenRectPixels = 0;
    for (int y = 0; y < layerImage.height(); ++y) {
        for (int x = 0; x < layerImage.width(); ++x) {
            const QRgb pixel = layerImage.pixel(x, y);
            if (x < 160 && y < 160 && qGreen(pixel) > 200 &&
                qRed(pixel) < 30 && qBlue(pixel) < 120) ++layerGreenRectPixels;
            if (qBlue(pixel) > qRed(pixel) + 20 &&
                qBlue(pixel) > qGreen(pixel) + 10) ++layerParticlePixels;
        }
    }
    if (layerImage.isNull() ||
        !layerImage.save(output.filePath(QStringLiteral("gpu_particle_layer.png")), "PNG")) {
        renderer.destroy();
        return 8;
    }
    report.write(QStringLiteral(
        "particle_pixels=%1 green_rect_pixels=%2\n%3\n"
        "layer_particle_pixels=%4 layer_green_rect_pixels=%5\n%6\n")
        .arg(particlePixels).arg(greenRectPixels).arg(debug)
        .arg(layerParticlePixels).arg(layerGreenRectPixels).arg(layerDebug).toUtf8());
    renderer.destroy();
    if (greenRectPixels == 0 || particlePixels == 0 ||
        !debug.contains(QStringLiteral("state=drawn")) ||
        layerGreenRectPixels == 0 || layerParticlePixels == 0 ||
        !layerDebug.contains(QStringLiteral("state=drawn"))) {
        qCritical("GPU particle capture did not contain the expected reference shapes");
        return 6;
    }
    return 0;
}

static int captureFlipbookMode(const QString& sourcePath, bool isSequence,
                               const QString& outputDirectory,
                               const QString& filePrefix,
                               Artifact::ArtifactIRenderer& renderer,
                               QImage& firstFrame, QImage& lastFrame,
                               QFile& report)
{
    constexpr std::array<float, 5> Times{0.35f, 0.50f, 0.65f, 0.80f, 0.95f};
    Artifact::ArtifactParticleLayer layer;
    layer.loadPreset(QStringLiteral("fountain"));
    if (!layer.setLayerPropertyValue(QStringLiteral("particle.emitter.positionX"),
                                     QVariant(960.0)) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.emitter.positionY"),
                                     QVariant(540.0)) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.emitter.rate"),
                                     QVariant(10.0)) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.emitter.scaleMin"),
                                     QVariant(40.0)) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.emitter.scaleMax"),
                                     QVariant(55.0)) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.emitter.speedMin"),
                                     QVariant(150.0)) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.emitter.speedMax"),
                                     QVariant(220.0)) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.emitter.texturePath"),
                                     QVariant(sourcePath)) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.emitter.textureRows"),
                                     QVariant(4)) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.emitter.textureCols"),
                                     QVariant(4)) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.emitter.randomFrame"),
                                     QVariant(false)) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.emitter.startFrame"),
                                     QVariant(0)) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.emitter.frameCount"),
                                     QVariant(16)) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.emitter.frameRate"),
                                     QVariant(12.0))) {
        return 1;
    }
    layer.resetParticleSystem();
    layer.play();

    for (std::size_t index = 0; index < Times.size(); ++index) {
        const auto frameNumber = static_cast<std::int64_t>(std::lround(Times[index] * 30.0f));
        renderer.clear();
        layer.goToFrame(frameNumber);
        layer.draw(&renderer);
        renderer.flushAndWait();
        QImage frame = renderer.readbackToImage().convertToFormat(QImage::Format_ARGB32);
        if (frame.isNull()) return 2;
        std::uint64_t alphaPixels = 0;
        for (int y = 0; y < frame.height(); ++y) {
            const auto* row = reinterpret_cast<const QRgb*>(frame.constScanLine(y));
            for (int x = 0; x < frame.width(); ++x) {
                if (qAlpha(row[x]) > 0) ++alphaPixels;
            }
        }
        if (alphaPixels == 0) return 3;
        if (index == 0) firstFrame = frame;
        if (index + 1 == Times.size()) lastFrame = frame;
        const QString path = QDir(outputDirectory).filePath(
            filePrefix + QStringLiteral("_%1.png")
                .arg(static_cast<int>(index), 2, 10, QLatin1Char('0')));
        if (!frame.save(path, "PNG")) return 4;
        report.write(QStringLiteral("mode=%1 time=%2 alpha_pixels=%3 image=%4\n")
                         .arg(isSequence ? QStringLiteral("sequence")
                                         : QStringLiteral("sprite-sheet"))
                         .arg(Times[index]).arg(alphaPixels).arg(path).toUtf8());
    }

    std::uint64_t changedPixels = 0;
    for (int y = 0; y < firstFrame.height(); ++y) {
        const auto* first = reinterpret_cast<const QRgb*>(firstFrame.constScanLine(y));
        const auto* last = reinterpret_cast<const QRgb*>(lastFrame.constScanLine(y));
        for (int x = 0; x < firstFrame.width(); ++x) {
            if (first[x] != last[x]) ++changedPixels;
        }
    }
    return changedPixels > 0 ? 0 : 5;
}

static int captureFlipbookSources(const QString& sequenceDirectory,
                                  const QString& spriteSheetPath,
                                  const QString& outputDirectory)
{
    const QDir sequenceSource(sequenceDirectory);
    if (!sequenceSource.exists() || !QFile::exists(spriteSheetPath)) return 2;
    QDir output(outputDirectory);
    if (!output.mkpath(QStringLiteral("sequence")) ||
        !output.mkpath(QStringLiteral("sprite_sheet"))) return 3;
    QFile report(output.filePath(QStringLiteral("flipbook_report.txt")));
    if (!report.open(QIODevice::WriteOnly | QIODevice::Text)) return 4;
    Artifact::ArtifactIRenderer renderer;
    renderer.initializeHeadless(1920, 1080);
    if (!renderer.isInitialized()) return 5;
    renderer.setCanvasSize(1920.0f, 1080.0f);
    renderer.setViewportSize(1920.0f, 1080.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    QImage sequenceFirst, sequenceLast, sheetFirst, sheetLast;
    const int sequenceResult = captureFlipbookMode(
        sequenceSource.absolutePath(), true,
        output.filePath(QStringLiteral("sequence")), QStringLiteral("sequence"),
        renderer, sequenceFirst, sequenceLast, report);
    if (sequenceResult != 0) return 10 + sequenceResult;
    const int sheetResult = captureFlipbookMode(
        QDir::cleanPath(spriteSheetPath), false,
        output.filePath(QStringLiteral("sprite_sheet")), QStringLiteral("sheet"),
        renderer, sheetFirst, sheetLast, report);
    if (sheetResult != 0) return 20 + sheetResult;
    const auto pixelDifference = [](const QImage& left, const QImage& right) {
        std::array<std::uint64_t, 2> difference{};
        if (left.size() != right.size() || left.format() != right.format()) {
            difference[0] = std::numeric_limits<std::uint64_t>::max();
            difference[1] = std::numeric_limits<std::uint64_t>::max();
            return difference;
        }
        for (int y = 0; y < left.height(); ++y) {
            const auto* leftRow = reinterpret_cast<const QRgb*>(left.constScanLine(y));
            const auto* rightRow = reinterpret_cast<const QRgb*>(right.constScanLine(y));
            for (int x = 0; x < left.width(); ++x) {
                const int delta = std::max({
                    std::abs(qRed(leftRow[x]) - qRed(rightRow[x])),
                    std::abs(qGreen(leftRow[x]) - qGreen(rightRow[x])),
                    std::abs(qBlue(leftRow[x]) - qBlue(rightRow[x])),
                    std::abs(qAlpha(leftRow[x]) - qAlpha(rightRow[x]))});
                if (delta > 0) ++difference[0];
                difference[1] = std::max(difference[1],
                    static_cast<std::uint64_t>(delta));
            }
        }
        return difference;
    };
    for (int index = 0; index < 5; ++index) {
        const QString sequencePath = output.filePath(
            QStringLiteral("sequence/sequence_%1.png")
                .arg(index, 2, 10, QLatin1Char('0')));
        const QString sheetPath = output.filePath(
            QStringLiteral("sprite_sheet/sheet_%1.png")
                .arg(index, 2, 10, QLatin1Char('0')));
        const auto difference = pixelDifference(QImage(sequencePath), QImage(sheetPath));
        if (difference[0] > 8 || difference[1] > 2) return 30;
        report.write(QStringLiteral(
            "sequence_sheet_frame=%1 differing_pixels=%2 max_channel_delta=%3 tolerance=8px/2\n")
            .arg(index).arg(difference[0]).arg(difference[1]).toUtf8());
    }
    return 0;
}

static bool checkLifeCurveEvaluation()
{
    Artifact::EmitterParams params;
    params.mode = Artifact::EmissionMode::Triggered;
    params.deterministic = false;
    params.lifeMin = params.lifeMax = 2.0f;
    params.speedMin = params.speedMax = 0.0f;
    params.scaleMin = params.scaleMax = 0.0f;
    params.scaleMidMin = params.scaleMidMax = 5.0f;
    params.scaleEndMin = params.scaleEndMax = 10.0f;
    params.opacityMin = params.opacityMax = 0.0f;
    params.opacityMidMin = params.opacityMidMax = 0.5f;
    params.opacityEndMin = params.opacityEndMax = 1.0f;
    params.colorStart = QColor(0, 0, 0);
    params.colorMid = QColor(128, 128, 128);
    params.colorEnd = QColor(255, 255, 255);
    params.scaleLifeCurve = 2;   // Ease In
    params.opacityLifeCurve = 3; // Ease Out
    params.colorLifeCurve = 1;   // Linear

    Artifact::ParticleEmitter emitter;
    emitter.setParams(params);
    emitter.emitParticles(1);
    emitter.update(0.5f); // 25% of a two-second life
    if (emitter.particles().size() != 1) return false;
    const auto& particle = emitter.particles().front();
    if (std::abs(particle.scale - 0.625f) >= 0.001f ||
        std::abs(particle.opacity - 0.4375f) >= 0.001f ||
        std::abs(particle.color.red() - 64) > 1) return false;

    // Old projects inherit the shared curve; an explicit identical curve on
    // all channels must produce the same values for the same particle age.
    params.lifeCurve = Artifact::ParticleLifeCurve::Smooth;
    params.scaleLifeCurve = params.colorLifeCurve = params.opacityLifeCurve = 0;
    Artifact::ParticleEmitter inherited;
    inherited.setParams(params);
    inherited.emitParticles(1);
    inherited.update(0.5f);
    params.scaleLifeCurve = params.colorLifeCurve = params.opacityLifeCurve = 4;
    Artifact::ParticleEmitter explicitCurves;
    explicitCurves.setParams(params);
    explicitCurves.emitParticles(1);
    explicitCurves.update(0.5f);
    if (inherited.particles().size() != 1 || explicitCurves.particles().size() != 1)
        return false;
    const auto& legacy = inherited.particles().front();
    const auto& explicitParticle = explicitCurves.particles().front();
    return std::abs(legacy.scale - explicitParticle.scale) < 0.00001f &&
           std::abs(legacy.opacity - explicitParticle.opacity) < 0.00001f &&
           legacy.color == explicitParticle.color;
}

static int captureGpuShapes(const QString& outputDirectory)
{
    QDir output(outputDirectory);
    if (!output.mkpath(QStringLiteral("."))) return 1;
    constexpr int width = 800;
    constexpr int height = 600;
    Artifact::ArtifactIRenderer renderer;
    renderer.initializeHeadless(width, height);
    if (!renderer.isInitialized()) return 2;
    renderer.setCanvasSize(static_cast<float>(width), static_cast<float>(height));
    renderer.setViewportSize(static_cast<float>(width), static_cast<float>(height));
    renderer.setClearColor(ArtifactCore::FloatColor(0.018f, 0.024f, 0.04f, 1.0f));

    Artifact::ArtifactParticleLayer layer;
    layer.loadPreset(QStringLiteral("fountain"));
    const auto set = [&layer](const char* path, const QVariant& value) {
        return layer.setLayerPropertyValue(QString::fromLatin1(path), value);
    };
    if (!set("particle.emitter.positionX", width / 2.0) ||
        !set("particle.emitter.positionY", height / 2.0) ||
        !set("particle.emitter.rate", 100.0) ||
        !set("particle.emitter.speedMin", 0.0) ||
        !set("particle.emitter.speedMax", 0.0) ||
        !set("particle.emitter.scaleMin", 40.0) ||
        !set("particle.emitter.scaleMax", 40.0) ||
        !set("particle.render.fullParticleDensity", true)) {
        renderer.destroy();
        return 3;
    }

    struct ShapeCase { const char* name; int falloff; bool stretch; };
    constexpr std::array<ShapeCase, 7> cases{{
        {"soft_circle", 0, false}, {"gaussian", 1, false},
        {"hard_circle", 2, false}, {"ring", 3, false},
        {"stretched_ring", 3, true}, {"diamond", 4, false},
        {"cross", 5, false}}};
    QFile report(output.filePath(QStringLiteral("gpu_shapes_report.txt")));
    if (!report.open(QIODevice::WriteOnly | QIODevice::Text)) {
        renderer.destroy();
        return 4;
    }
    for (const auto& shape : cases) {
        if (!set("particle.render.falloffMode", shape.falloff) ||
            !set("particle.render.stretchEnabled", shape.stretch) ||
            !set("particle.render.stretchFactor", 3.0)) {
            renderer.destroy();
            return 5;
        }
        layer.resetParticleSystem();
        layer.play();
        layer.goToFrame(1);
        renderer.clear();
        layer.draw(&renderer);
        renderer.flushAndWait();
        const QString state = renderer.particleDebugState();
        const QImage image = renderer.readbackToImage().convertToFormat(QImage::Format_ARGB32);
        if (image.isNull()) {
            renderer.destroy();
            return 6;
        }
        const QRgb background = image.pixel(0, 0);
        std::uint64_t changed = 0;
        for (int y = 0; y < image.height(); ++y) {
            const auto* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                const QRgb pixel = row[x];
                if (std::abs(qRed(pixel) - qRed(background)) > 8 ||
                    std::abs(qGreen(pixel) - qGreen(background)) > 8 ||
                    std::abs(qBlue(pixel) - qBlue(background)) > 8) ++changed;
            }
        }
        const QString name = QString::fromLatin1(shape.name);
        if (!image.save(output.filePath(name + QStringLiteral(".png")), "PNG")) {
            renderer.destroy();
            return 7;
        }
        report.write(QStringLiteral("%1 changed_pixels=%2 %3\n")
                         .arg(name).arg(changed).arg(state).toUtf8());
        if (changed == 0 || !state.contains(QStringLiteral("state=drawn"))) {
            renderer.destroy();
            return 8;
        }
    }
    renderer.destroy();
    return 0;
}

static int checkLifeCurvePersistence()
{
    if (!checkLifeCurveEvaluation()) {
        qCritical("Independent particle life curves produced unexpected values");
        return 23;
    }
    Artifact::ArtifactParticleLayer layer;
    layer.loadPreset(QStringLiteral("fountain"));
    if (!layer.setLayerPropertyValue(QStringLiteral("particle.particle.lifeCurve"), 3) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.particle.scaleLifeCurve"), 2) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.particle.colorLifeCurve"), 4) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.particle.opacityLifeCurve"), 5) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.render.falloffMode"), 3) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.render.ringInnerRadius"), 0.75) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.render.animateRingInnerRadius"), true) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.render.ringInnerRadiusEnd"), 0.25) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.render.colorIntensity"), 2.5) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.render.fullParticleDensity"), true) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.render.trailEnabled"), true) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.render.trailWidth"), 2.5) ||
        !layer.setLayerPropertyValue(QStringLiteral("particle.render.trailFade"), 0.25)) {
        qCritical("Could not set particle life curve properties");
        return 1;
    }
    QJsonObject saved = layer.toJson();
    const auto checkTrailSettings = [](const QJsonObject& json) {
        const QJsonObject render = json.value(QStringLiteral("renderSettings")).toObject();
        return render.value(QStringLiteral("trailEnabled")).toBool(false) &&
               render.value(QStringLiteral("trailWidth")).toDouble(-1.0) == 2.5 &&
               render.value(QStringLiteral("trailFade")).toDouble(-1.0) == 0.25;
    };
    if (!checkTrailSettings(saved)) return 25;
    if (saved.value(QStringLiteral("renderSettings")).toObject()
            .value(QStringLiteral("falloffMode")).toInt(-1) != 3) return 8;
    if (saved.value(QStringLiteral("renderSettings")).toObject()
            .value(QStringLiteral("colorIntensity")).toDouble(-1.0) != 2.5) return 11;
    if (saved.value(QStringLiteral("renderSettings")).toObject()
            .value(QStringLiteral("ringInnerRadius")).toDouble(-1.0) != 0.75) return 14;
    if (!saved.value(QStringLiteral("renderSettings")).toObject()
             .value(QStringLiteral("fullParticleDensity")).toBool(false)) return 17;
    if (!saved.value(QStringLiteral("renderSettings")).toObject()
             .value(QStringLiteral("animateRingInnerRadius")).toBool(false) ||
        saved.value(QStringLiteral("renderSettings")).toObject()
             .value(QStringLiteral("ringInnerRadiusEnd")).toDouble(-1.0) != 0.25) return 20;
    QJsonArray emitters = saved.value(QStringLiteral("emitters")).toArray();
    if (emitters.isEmpty()) return 2;
    const QJsonObject first = emitters.at(0).toObject();
    if (first.value(QStringLiteral("lifeCurve")).toInt(-1) != 3 ||
        first.value(QStringLiteral("scaleLifeCurve")).toInt(-1) != 2 ||
        first.value(QStringLiteral("colorLifeCurve")).toInt(-1) != 4 ||
        first.value(QStringLiteral("opacityLifeCurve")).toInt(-1) != 5) return 3;

    Artifact::ArtifactParticleLayer restored;
    restored.fromJsonProperties(saved);
    if (!checkTrailSettings(restored.toJson())) return 26;
    if (restored.toJson().value(QStringLiteral("renderSettings")).toObject()
            .value(QStringLiteral("falloffMode")).toInt(-1) != 3) return 9;
    if (restored.toJson().value(QStringLiteral("renderSettings")).toObject()
            .value(QStringLiteral("colorIntensity")).toDouble(-1.0) != 2.5) return 12;
    if (restored.toJson().value(QStringLiteral("renderSettings")).toObject()
            .value(QStringLiteral("ringInnerRadius")).toDouble(-1.0) != 0.75) return 15;
    if (!restored.toJson().value(QStringLiteral("renderSettings")).toObject()
             .value(QStringLiteral("fullParticleDensity")).toBool(false)) return 18;
    if (!restored.toJson().value(QStringLiteral("renderSettings")).toObject()
             .value(QStringLiteral("animateRingInnerRadius")).toBool(false) ||
        restored.toJson().value(QStringLiteral("renderSettings")).toObject()
             .value(QStringLiteral("ringInnerRadiusEnd")).toDouble(-1.0) != 0.25) return 21;
    const QJsonArray roundTrip = restored.toJson().value(QStringLiteral("emitters")).toArray();
    if (roundTrip.isEmpty()) return 4;
    const QJsonObject reloaded = roundTrip.at(0).toObject();
    if (reloaded.value(QStringLiteral("lifeCurve")).toInt(-1) != 3 ||
        reloaded.value(QStringLiteral("scaleLifeCurve")).toInt(-1) != 2 ||
        reloaded.value(QStringLiteral("colorLifeCurve")).toInt(-1) != 4 ||
        reloaded.value(QStringLiteral("opacityLifeCurve")).toInt(-1) != 5) return 5;

    QJsonObject legacyEmitter = first;
    legacyEmitter.remove(QStringLiteral("scaleLifeCurve"));
    legacyEmitter.remove(QStringLiteral("colorLifeCurve"));
    legacyEmitter.remove(QStringLiteral("opacityLifeCurve"));
    emitters.replace(0, legacyEmitter);
    saved.insert(QStringLiteral("emitters"), emitters);
    QJsonObject legacyRenderSettings = saved.value(QStringLiteral("renderSettings")).toObject();
    legacyRenderSettings.remove(QStringLiteral("falloffMode"));
    legacyRenderSettings.remove(QStringLiteral("colorIntensity"));
    legacyRenderSettings.remove(QStringLiteral("ringInnerRadius"));
    legacyRenderSettings.remove(QStringLiteral("animateRingInnerRadius"));
    legacyRenderSettings.remove(QStringLiteral("ringInnerRadiusEnd"));
    legacyRenderSettings.remove(QStringLiteral("fullParticleDensity"));
    legacyRenderSettings.remove(QStringLiteral("trailEnabled"));
    legacyRenderSettings.remove(QStringLiteral("trailWidth"));
    legacyRenderSettings.remove(QStringLiteral("trailFade"));
    saved.insert(QStringLiteral("renderSettings"), legacyRenderSettings);
    Artifact::ArtifactParticleLayer legacy;
    legacy.fromJsonProperties(saved);
    const QJsonObject legacyRender = legacy.toJson().value(QStringLiteral("renderSettings")).toObject();
    if (legacyRender.value(QStringLiteral("trailEnabled")).toBool(true) ||
        legacyRender.value(QStringLiteral("trailWidth")).toDouble(-1.0) != 1.0 ||
        legacyRender.value(QStringLiteral("trailFade")).toDouble(-1.0) != 0.5) return 27;
    if (legacy.toJson().value(QStringLiteral("renderSettings")).toObject()
            .value(QStringLiteral("falloffMode")).toInt(-1) != 0) return 10;
    if (legacy.toJson().value(QStringLiteral("renderSettings")).toObject()
            .value(QStringLiteral("colorIntensity")).toDouble(-1.0) != 1.0) return 13;
    if (std::abs(legacy.toJson().value(QStringLiteral("renderSettings")).toObject()
            .value(QStringLiteral("ringInnerRadius")).toDouble(-1.0) - 0.6) > 0.00001) return 16;
    if (legacy.toJson().value(QStringLiteral("renderSettings")).toObject()
            .value(QStringLiteral("fullParticleDensity")).toBool(true)) return 19;
    if (legacy.toJson().value(QStringLiteral("renderSettings")).toObject()
            .value(QStringLiteral("animateRingInnerRadius")).toBool(true)) return 22;
    const QJsonArray legacyRoundTrip = legacy.toJson().value(QStringLiteral("emitters")).toArray();
    if (legacyRoundTrip.isEmpty()) return 6;
    const QJsonObject inherited = legacyRoundTrip.at(0).toObject();
    if (inherited.value(QStringLiteral("lifeCurve")).toInt(-1) != 3 ||
        inherited.value(QStringLiteral("scaleLifeCurve")).toInt(-1) != 0 ||
        inherited.value(QStringLiteral("colorLifeCurve")).toInt(-1) != 0 ||
        inherited.value(QStringLiteral("opacityLifeCurve")).toInt(-1) != 0) return 7;
    if (!layer.setLayerPropertyValue(QStringLiteral("particle.render.falloffMode"), 5) ||
        layer.toJson().value(QStringLiteral("renderSettings")).toObject()
            .value(QStringLiteral("falloffMode")).toInt(-1) != 5) return 24;
    return 0;
}

static QJsonObject particleRegionStats(const Artifact::ArtifactParticleLayer& layer,
                                      const QRectF& textTargetBounds,
                                      const QImage& shapeMask,
                                      const QImage& textMask)
{
    QJsonObject result;
    const auto* system = layer.particleSystem();
    if (!system) return result;
    const auto data = system->captureRenderData();
    std::uint64_t insideShape = 0;
    std::uint64_t insideHole = 0;
    std::uint64_t insideTextBounds = 0;
    std::uint64_t insideShapeMask = 0;
    std::uint64_t insideTextMask = 0;
    const auto containsMaskPixel = [](const QImage& mask, const float x,
                                      const float y) {
        const int pixelX = static_cast<int>(std::floor(x));
        const int pixelY = static_cast<int>(std::floor(y));
        return pixelX >= 0 && pixelY >= 0 && pixelX < mask.width() &&
            pixelY < mask.height() && qAlpha(mask.pixel(pixelX, pixelY)) >= 128;
    };
    for (const auto& particle : data.particles) {
        const double dx = (particle.px - 154.0) / 56.0;
        const double dy = (particle.py - 121.0) / 39.0;
        if (dx * dx + dy * dy <= 1.0) ++insideShape;
        const double hx = (particle.px - 154.0) / 25.0;
        const double hy = (particle.py - 121.0) / 17.0;
        if (hx * hx + hy * hy <= 1.0) ++insideHole;
        if (textTargetBounds.contains(QPointF(particle.px, particle.py))) {
            ++insideTextBounds;
        }
        if (containsMaskPixel(shapeMask, particle.px, particle.py)) {
            ++insideShapeMask;
        }
        if (containsMaskPixel(textMask, particle.px, particle.py)) {
            ++insideTextMask;
        }
    }
    result.insert(QStringLiteral("alive"), static_cast<qint64>(data.particles.size()));
    result.insert(QStringLiteral("inside_shape_ellipse"), static_cast<qint64>(insideShape));
    result.insert(QStringLiteral("inside_shape_hole"), static_cast<qint64>(insideHole));
    result.insert(QStringLiteral("inside_text_bounds"), static_cast<qint64>(insideTextBounds));
    result.insert(QStringLiteral("inside_shape_visible_mask"),
                  static_cast<qint64>(insideShapeMask));
    result.insert(QStringLiteral("inside_text_visible_mask"),
                  static_cast<qint64>(insideTextMask));
    return result;
}

static std::array<std::uint64_t, 2> imageDifference(const QImage& leftImage,
                                                   const QImage& rightImage)
{
    if (leftImage.size() != rightImage.size() || leftImage.isNull() ||
        rightImage.isNull()) {
        return {std::numeric_limits<std::uint64_t>::max(),
                std::numeric_limits<std::uint64_t>::max()};
    }
    const QImage left = leftImage.convertToFormat(QImage::Format_ARGB32);
    const QImage right = rightImage.convertToFormat(QImage::Format_ARGB32);
    std::array<std::uint64_t, 2> result{};
    for (int y = 0; y < left.height(); ++y) {
        const auto* a = reinterpret_cast<const QRgb*>(left.constScanLine(y));
        const auto* b = reinterpret_cast<const QRgb*>(right.constScanLine(y));
        for (int x = 0; x < left.width(); ++x) {
            const auto delta = static_cast<std::uint64_t>(std::max({
                std::abs(qRed(a[x]) - qRed(b[x])),
                std::abs(qGreen(a[x]) - qGreen(b[x])),
                std::abs(qBlue(a[x]) - qBlue(b[x])),
                std::abs(qAlpha(a[x]) - qAlpha(b[x]))}));
            if (delta > 0) ++result[0];
            result[1] = std::max(result[1], delta);
        }
    }
    return result;
}

static std::uint64_t processWorkingSetBytes()
{
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    return GetProcessMemoryInfo(GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
        sizeof(counters))
        ? static_cast<std::uint64_t>(counters.WorkingSetSize)
        : 0;
}

static int captureShapeTarget(const QString& outputDirectory)
{
    constexpr int Width = 320;
    constexpr int Height = 180;
    QDir output(outputDirectory);
    if (!output.mkpath(QStringLiteral("."))) return 2;

    Artifact::DiligentDeviceManager deviceManager;
    deviceManager.initializeHeadless();
    if (!deviceManager.device() || !deviceManager.immediateContext()) {
        qCritical("Could not initialize the selected headless Diligent backend");
        return 3;
    }
    Artifact::OffscreenCompositionRenderer renderer(
        deviceManager.device(), deviceManager.immediateContext(), Width, Height);
    if (!renderer.renderer() || !renderer.renderer()->isInitialized()) return 4;

    Artifact::ArtifactCompositionInitParams compositionParams(
        ArtifactCore::UniString(QStringLiteral("ParticleShapeTargetEvaluation")),
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    compositionParams.setResolution(Width, Height);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(300);
    Artifact::ArtifactAbstractComposition composition(
        ArtifactCore::CompositionID(QStringLiteral("particle-shape-target-evaluation")),
        compositionParams);
    composition.setBackGroundColor(
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    auto shape = ArtifactCore::makeShared<Artifact::ArtifactShapeLayer>();
    shape->setId(ArtifactCore::LayerID(
        QStringLiteral("00000000-0000-4000-8000-000000000001")));
    shape->setLayerName(QStringLiteral("Hole Shape Target"));
    shape->setShapeType(Artifact::ShapeType::Ellipse);
    shape->setSize(112, 78);
    shape->setFillEnabled(true);
    shape->setFillColor(ArtifactCore::FloatColor(0.20f, 0.48f, 0.78f, 0.72f));
    shape->setStrokeEnabled(true);
    shape->setStrokeWidth(2.0f);
    shape->setStrokeColor(ArtifactCore::FloatColor(0.65f, 0.82f, 1.0f, 1.0f));
    shape->setLayerPropertyValue(QStringLiteral("transform.position.x"), 98.0);
    shape->setLayerPropertyValue(QStringLiteral("transform.position.y"), 82.0);
    auto innerHole = shape->makeContentFromLegacy();
    innerHole.name = QStringLiteral("Subtracted Inner Ellipse");
    innerHole.geometry.width = 50;
    innerHole.geometry.height = 34;
    innerHole.transform.position = QPointF(31.0, 22.0);
    innerHole.fill.enabled = false;
    innerHole.stroke.enabled = false;
    shape->addShapeContent(innerHole);
    shape->addShapeOperator(ArtifactCore::ShapeOperatorType::MergePaths);
    shape->setLayerPropertyValue(QStringLiteral("shape.operator.0.mode"), 1);
    shape->resetShapeStackOrder();

    auto emptyShape = ArtifactCore::makeShared<Artifact::ArtifactShapeLayer>();
    emptyShape->setId(ArtifactCore::LayerID(
        QStringLiteral("00000000-0000-4000-8000-000000000002")));
    emptyShape->setLayerName(QStringLiteral("Empty Shape Target"));
    emptyShape->setShapeType(Artifact::ShapeType::Ellipse);
    emptyShape->setSize(96, 64);
    emptyShape->setFillEnabled(false);
    emptyShape->setStrokeEnabled(false);
    emptyShape->setLayerPropertyValue(QStringLiteral("transform.position.x"), 180.0);
    emptyShape->setLayerPropertyValue(QStringLiteral("transform.position.y"), 90.0);

    auto text = ArtifactCore::makeShared<Artifact::ArtifactTextLayer>();
    text->setId(ArtifactCore::LayerID(
        QStringLiteral("00000000-0000-4000-8000-000000000003")));
    text->setLayerName(QStringLiteral("Text Shape Target"));
    text->setText(ArtifactCore::UniString(QStringLiteral("A8")));
    text->setFontFamily(ArtifactCore::UniString(QStringLiteral("Arial")));
    text->setFontWeight(400);
    text->setFontSize(54.0f);
    text->setStrokeEnabled(true);
    text->setStrokeWidth(1.5f);
    text->setLayerPropertyValue(QStringLiteral("transform.position.x"), 178.0);
    text->setLayerPropertyValue(QStringLiteral("transform.position.y"), 46.0);

    auto particle = ArtifactCore::makeShared<Artifact::ArtifactParticleLayer>();
    particle->setId(ArtifactCore::LayerID(
        QStringLiteral("00000000-0000-4000-8000-000000000004")));
    particle->setLayerName(QStringLiteral("Shape Target Particles"));
    particle->setLayerPropertyValue(QStringLiteral("particle.render.fullParticleDensity"), true);
    Artifact::EmitterParams emitter;
    emitter.shape = Artifact::EmitterShape::Point;
    emitter.position = QVector3D(28.0f, 142.0f, 0.0f);
    emitter.mode = Artifact::EmissionMode::Continuous;
    emitter.rate = 180.0f;
    emitter.maxParticles = 360;
    emitter.lifeMin = 10.0f;
    emitter.lifeMax = 10.0f;
    emitter.speedMin = 38.0f;
    emitter.speedMax = 52.0f;
    emitter.direction = QVector3D(0.8f, -0.6f, 0.0f);
    emitter.directionSpread = 24.0f;
    emitter.drag = 0.35f;
    emitter.scaleMin = 1.7f;
    emitter.scaleMax = 2.2f;
    emitter.colorStart = QColor(255, 126, 38, 255);
    emitter.colorEnd = QColor(255, 210, 92, 100);
    emitter.randomSeed = 20261118u;
    emitter.deterministic = true;
    particle->addEmitter(emitter);
    const auto resetSimulation = [&]() {
        particle->reset();
        return particle->particleSystem() != nullptr;
    };
    const auto stopEmission = [&]() {
        return particle->setLayerPropertyValue(
            QStringLiteral("particle.emitter.rate"), 0.0);
    };

    if (!composition.appendLayerTop(shape).success ||
        !composition.appendLayerTop(text).success ||
        !composition.appendLayerTop(emptyShape).success ||
        !composition.appendLayerTop(particle).success) return 5;

    QJsonObject report;
    const auto countMaskPixels = [](const QImage& image) {
        qint64 count = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (qAlpha(image.pixel(x, y)) >= 128) ++count;
            }
        }
        return count;
    };
    const QImage shapeLayerImage = renderer.renderLayerToQImage(
        ArtifactCore::FramePosition(0), shape.get());
    if (shapeLayerImage.isNull() || !shapeLayerImage.save(
            output.filePath(QStringLiteral("shape_target_layer.png")), "PNG")) {
        return 15;
    }
    report.insert(QStringLiteral("shape_layer_image"),
                  QStringLiteral("shape_target_layer.png"));
    report.insert(QStringLiteral("backend"), deviceManager.selectedAdapterInfo().backend);
    report.insert(QStringLiteral("adapter"), deviceManager.selectedAdapterInfo().name);
    report.insert(QStringLiteral("width"), Width);
    report.insert(QStringLiteral("height"), Height);
    report.insert(QStringLiteral("fps"), 30);
    report.insert(QStringLiteral("seed"), 20261118);
    QJsonObject fixtureLayerIds;
    fixtureLayerIds.insert(QStringLiteral("shape_ring"), shape->id().toString());
    fixtureLayerIds.insert(QStringLiteral("empty_shape"), emptyShape->id().toString());
    fixtureLayerIds.insert(QStringLiteral("text_A8"), text->id().toString());
    fixtureLayerIds.insert(QStringLiteral("particle_layer"), particle->id().toString());
    report.insert(QStringLiteral("fixture_layer_ids"), fixtureLayerIds);
    const QFontInfo fontInfo(QFont(QStringLiteral("Arial"), 54, QFont::Normal));
    report.insert(QStringLiteral("requested_font_family"), QStringLiteral("Arial"));
    report.insert(QStringLiteral("resolved_font_family"), fontInfo.family());
    report.insert(QStringLiteral("font_exact_match"), fontInfo.exactMatch());
    report.insert(QStringLiteral("font_weight"), 400);
    report.insert(QStringLiteral("qt_version"), QStringLiteral(QT_VERSION_STR));
    std::array<QPointF, 4096> textSamplePoints{};
    std::size_t textSampleCount = 0;
    const bool textSamplingSupported = text->supportsStaticShapeTargetSampling();
    const bool textSamplesGenerated = text->sampleFillPoints(
        textSamplePoints.data(), textSamplePoints.size(), textSampleCount,
        20261118u);
    const auto textGeometry = text->textGlyphGeometry();
    report.insert(QStringLiteral("text_sampling_supported"), textSamplingSupported);
    report.insert(QStringLiteral("text_samples_generated"), textSamplesGenerated);
    report.insert(QStringLiteral("text_sample_count"),
                  static_cast<qint64>(textSampleCount));
    report.insert(QStringLiteral("text_glyph_count"),
                  static_cast<qint64>(textGeometry.glyphs.size()));
    QJsonArray textGlyphs;
    for (const auto& glyph : textGeometry.glyphs) {
        QJsonObject glyphInfo;
        glyphInfo.insert(QStringLiteral("codepoint"),
                         static_cast<qint64>(glyph.charCode));
        glyphInfo.insert(QStringLiteral("render_mode"),
                         static_cast<int>(glyph.renderMode));
        glyphInfo.insert(QStringLiteral("is_emoji"), glyph.isEmojiSequence);
        textGlyphs.append(glyphInfo);
    }
    report.insert(QStringLiteral("text_glyphs"), textGlyphs);
    const QImage textLayerImage = renderer.renderLayerToQImage(
        ArtifactCore::FramePosition(0), text.get());
    if (textLayerImage.isNull() || !textLayerImage.save(
            output.filePath(QStringLiteral("text_A8_layer.png")), "PNG")) {
        return 14;
    }
    report.insert(QStringLiteral("text_layer_image"),
                  QStringLiteral("text_A8_layer.png"));
    report.insert(QStringLiteral("shape_visible_mask_pixels"),
                  countMaskPixels(shapeLayerImage));
    report.insert(QStringLiteral("text_visible_mask_pixels"),
                  countMaskPixels(textLayerImage));
    report.insert(QStringLiteral("shape_render_hole_center_alpha"),
                  shapeLayerImage.pixelColor(154, 121).alpha());
    report.insert(QStringLiteral("shape_render_ring_sample_alpha"),
                  shapeLayerImage.pixelColor(154, 95).alpha());
    const auto visibleShapePaths = shape->nativeShapeFillPathsForParticleTarget();
    report.insert(QStringLiteral("shape_target_path_count"),
                  static_cast<qint64>(visibleShapePaths.size()));
    if (!visibleShapePaths.empty()) {
        report.insert(QStringLiteral("shape_path_contains_hole_center"),
                      visibleShapePaths.front().contains(QPointF(56.0, 39.0)));
        report.insert(QStringLiteral("shape_path_fill_rule"),
                      static_cast<int>(visibleShapePaths.front().fillRule()));
    }
    if (!textGeometry.glyphs.empty()) {
        report.insert(QStringLiteral("text_glyph_codepoint"),
                      static_cast<qint64>(textGeometry.glyphs.front().charCode));
        report.insert(QStringLiteral("text_glyph_render_mode"),
                      static_cast<int>(textGeometry.glyphs.front().renderMode));
        report.insert(QStringLiteral("text_glyph_is_emoji"),
                      textGeometry.glyphs.front().isEmojiSequence);
    }
    QRectF textTargetBounds;
    if (textSampleCount > 0) {
        const QTransform textGlobal = text->getGlobalTransformAt(0);
        QPointF minimum = textGlobal.map(textSamplePoints.front());
        QPointF maximum = minimum;
        for (std::size_t index = 1; index < textSampleCount; ++index) {
            const QPointF point = textGlobal.map(textSamplePoints[index]);
            minimum.setX(std::min(minimum.x(), point.x()));
            minimum.setY(std::min(minimum.y(), point.y()));
            maximum.setX(std::max(maximum.x(), point.x()));
            maximum.setY(std::max(maximum.y(), point.y()));
        }
        textTargetBounds = QRectF(minimum, maximum).normalized();
        QJsonObject textBoundsReport;
        textBoundsReport.insert(QStringLiteral("left"), textTargetBounds.left());
        textBoundsReport.insert(QStringLiteral("top"), textTargetBounds.top());
        textBoundsReport.insert(QStringLiteral("right"), textTargetBounds.right());
        textBoundsReport.insert(QStringLiteral("bottom"), textTargetBounds.bottom());
        report.insert(QStringLiteral("text_target_bounds"), textBoundsReport);
    }
    QJsonObject captures;
    const auto captureMode = [&](const QString& label, const QString& targetId,
                                 const bool enabled) -> bool {
        if (!particle->setLayerPropertyValue(
                QStringLiteral("particle.shapeTarget.layerId"), targetId) ||
            !particle->setLayerPropertyValue(
                QStringLiteral("particle.shapeTarget.strength"), 1200.0) ||
            !particle->setLayerPropertyValue(
                QStringLiteral("particle.shapeTarget.enabled"), enabled)) return false;
        if (!resetSimulation()) return false;
        const std::uint64_t workingSetBeforeTargetPreparation =
            processWorkingSetBytes();
        QElapsedTimer targetPreparationTimer;
        targetPreparationTimer.start();
        QImage preparationSurface(Width, Height, QImage::Format_ARGB32);
        particle->renderToImage(preparationSurface, 0.0f);
        const qint64 targetPreparationNanoseconds =
            targetPreparationTimer.nsecsElapsed();
        const std::uint64_t workingSetAfterTargetPreparation =
            processWorkingSetBytes();
        if (!resetSimulation()) return false;
        QJsonObject frames;
        QImage frame60;
        ArtifactCore::ArtifactArray<int> frame60ParticleIds;
        ArtifactCore::ArtifactArray<QVector3D> frame60ParticlePositions;
        for (const int frameNumber : {30, 60, 90, 180, 240}) {
            if (frameNumber == 60 && !stopEmission()) return false;
            const QImage image = renderer.renderToQImage(
                ArtifactCore::FramePosition(frameNumber), &composition);
            if (image.isNull()) return false;
            const QString imageName = QStringLiteral("%1_f%2.png")
                .arg(label).arg(frameNumber, 4, 10, QLatin1Char('0'));
            if (!image.save(output.filePath(imageName), "PNG")) return false;
            QJsonObject frame;
            frame.insert(QStringLiteral("image"), imageName);
            frame.insert(QStringLiteral("particle_regions"),
                         particleRegionStats(*particle, textTargetBounds,
                                             shapeLayerImage, textLayerImage));
            if (frameNumber == 90 || frameNumber == 240) {
                const auto* system = particle->particleSystem();
                if (!system || system->emitters().empty() ||
                    !system->emitters().front()) return false;
                QJsonArray centers;
                const auto renderData = system->captureRenderData();
                for (std::size_t index = 0;
                     index < renderData.particles.size(); ++index) {
                    const auto& item = renderData.particles[index];
                    QJsonObject center;
                    center.insert(QStringLiteral("index"),
                                  static_cast<qint64>(index));
                    center.insert(QStringLiteral("x"), item.px);
                    center.insert(QStringLiteral("y"), item.py);
                    centers.append(center);
                }
                frame.insert(QStringLiteral("particle_centers"), centers);
            }
            frames.insert(QString::number(frameNumber), frame);
            if (frameNumber == 60) {
                frame60 = image;
                auto* system = particle->particleSystem();
                if (!system || system->emitters().empty() ||
                    !system->emitters().front()) return false;
                const auto& particles = system->emitters().front()->particles();
                frame60ParticleIds.reserve(particles.size());
                frame60ParticlePositions.reserve(particles.size());
                for (const auto& item : particles) {
                    frame60ParticleIds.append(item.id);
                    frame60ParticlePositions.append(item.position);
                }
            }
        }
        if (!resetSimulation()) return false;
        if (renderer.renderToQImage(ArtifactCore::FramePosition(30),
                                    &composition).isNull() ||
            !stopEmission()) return false;
        const QImage directSeek = renderer.renderToQImage(
            ArtifactCore::FramePosition(60), &composition);
        if (directSeek.isNull() || frame60.isNull()) return false;
        const auto seekDifference = imageDifference(frame60, directSeek);
        auto* directSystem = particle->particleSystem();
        if (!directSystem || directSystem->emitters().empty() ||
            !directSystem->emitters().front()) return false;
        const auto& directParticles = directSystem->emitters().front()->particles();
        std::size_t particleIdMismatches = 0;
        float maxParticlePositionDelta = 0.0f;
        const std::size_t commonParticleCount = std::min(
            frame60ParticleIds.size(), directParticles.size());
        for (std::size_t index = 0; index < commonParticleCount; ++index) {
            if (frame60ParticleIds[index] != directParticles[index].id) {
                ++particleIdMismatches;
                continue;
            }
            maxParticlePositionDelta = std::max(maxParticlePositionDelta,
                (frame60ParticlePositions[index] -
                 directParticles[index].position).length());
        }
        particleIdMismatches += frame60ParticleIds.size() > commonParticleCount
            ? frame60ParticleIds.size() - commonParticleCount : 0;
        particleIdMismatches += directParticles.size() > commonParticleCount
            ? directParticles.size() - commonParticleCount : 0;
        const QString seekName = QStringLiteral("%1_f0060_direct.png").arg(label);
        if (!directSeek.save(output.filePath(seekName), "PNG")) return false;
        frames.insert(QStringLiteral("direct_seek_image"), seekName);
        frames.insert(QStringLiteral("continuous_vs_direct_different_pixels"),
                      static_cast<qint64>(seekDifference[0]));
        frames.insert(QStringLiteral("continuous_vs_direct_max_channel_delta"),
                      static_cast<qint64>(seekDifference[1]));
        frames.insert(QStringLiteral("continuous_vs_direct_particle_id_mismatches"),
                      static_cast<qint64>(particleIdMismatches));
        frames.insert(QStringLiteral("continuous_vs_direct_max_particle_position_delta_px"),
                      maxParticlePositionDelta);

        // Keep the same live-particle count and fixed timestep for each mode.
        // Measure only ParticleSystem::update(), separately from rasterization,
        // readback, PNG encoding, and image statistics above.
        if (!resetSimulation() ||
            renderer.renderToQImage(ArtifactCore::FramePosition(30),
                                    &composition).isNull() ||
            !stopEmission() ||
            renderer.renderToQImage(ArtifactCore::FramePosition(90),
                                    &composition).isNull()) return false;
        auto* benchmarkSystem = particle->particleSystem();
        if (!benchmarkSystem || benchmarkSystem->captureRenderData().particles.size() != 360) {
            return false;
        }
        benchmarkSystem->setPaused(false);
        constexpr int kWarmupSteps = 12;
        constexpr int kMeasuredSteps = 120;
        constexpr int kMeasurementBatches = 5;
        constexpr int kStepsPerBatch = kMeasuredSteps / kMeasurementBatches;
        constexpr float kBenchmarkDelta = 1.0f / 30.0f;
        for (int index = 0; index < kWarmupSteps; ++index) {
            benchmarkSystem->update(kBenchmarkDelta);
        }
        const std::uint64_t workingSetBefore = processWorkingSetBytes();
        std::uint64_t peakWorkingSet = workingSetBefore;
        std::array<qint64, kMeasurementBatches> batchNanoseconds{};
        qint64 totalUpdateNanoseconds = 0;
        for (int batch = 0; batch < kMeasurementBatches; ++batch) {
            QElapsedTimer updateTimer;
            updateTimer.start();
            for (int step = 0; step < kStepsPerBatch; ++step) {
                benchmarkSystem->update(kBenchmarkDelta);
                if ((step + 1) % 8 == 0) {
                    peakWorkingSet = std::max(peakWorkingSet,
                        processWorkingSetBytes());
                }
            }
            batchNanoseconds[static_cast<std::size_t>(batch)] =
                updateTimer.nsecsElapsed();
            totalUpdateNanoseconds +=
                batchNanoseconds[static_cast<std::size_t>(batch)];
        }
        const std::uint64_t workingSetAfter = processWorkingSetBytes();
        peakWorkingSet = std::max(peakWorkingSet, workingSetAfter);
        auto sortedBatchNanoseconds = batchNanoseconds;
        std::sort(sortedBatchNanoseconds.begin(), sortedBatchNanoseconds.end());
        QJsonArray batchMeanMicroseconds;
        for (const qint64 batchDuration : batchNanoseconds) {
            batchMeanMicroseconds.append(
                static_cast<double>(batchDuration) /
                (static_cast<double>(kStepsPerBatch) * 1000.0));
        }
        QJsonObject performance;
        performance.insert(QStringLiteral("scope"),
            QStringLiteral("ParticleSystem::update only; excludes rendering/readback/PNG"));
        performance.insert(QStringLiteral("live_particles_before_measurement"), 360);
        performance.insert(QStringLiteral("fixed_timestep_seconds"), kBenchmarkDelta);
        performance.insert(QStringLiteral("warmup_steps"), kWarmupSteps);
        performance.insert(QStringLiteral("measured_steps"), kMeasuredSteps);
        performance.insert(QStringLiteral("measurement_batches"),
                           kMeasurementBatches);
        performance.insert(QStringLiteral("steps_per_batch"), kStepsPerBatch);
        performance.insert(QStringLiteral("total_update_nanoseconds"),
                           totalUpdateNanoseconds);
        performance.insert(QStringLiteral("mean_update_microseconds"),
            static_cast<double>(totalUpdateNanoseconds) /
                (static_cast<double>(kMeasuredSteps) * 1000.0));
        performance.insert(QStringLiteral("median_batch_mean_update_microseconds"),
            static_cast<double>(sortedBatchNanoseconds[kMeasurementBatches / 2]) /
                (static_cast<double>(kStepsPerBatch) * 1000.0));
        performance.insert(QStringLiteral("batch_mean_update_microseconds"),
                           batchMeanMicroseconds);
        performance.insert(QStringLiteral("target_preparation_and_layer_render_nanoseconds"),
                           targetPreparationNanoseconds);
        performance.insert(QStringLiteral("process_working_set_before_target_preparation_bytes"),
            static_cast<qint64>(workingSetBeforeTargetPreparation));
        performance.insert(QStringLiteral("process_working_set_after_target_preparation_bytes"),
            static_cast<qint64>(workingSetAfterTargetPreparation));
        performance.insert(QStringLiteral("target_preparation_working_set_growth_bytes"),
            static_cast<qint64>(workingSetAfterTargetPreparation >=
                workingSetBeforeTargetPreparation
                    ? workingSetAfterTargetPreparation -
                        workingSetBeforeTargetPreparation
                    : 0));
        performance.insert(QStringLiteral("process_working_set_before_bytes"),
                           static_cast<qint64>(workingSetBefore));
        performance.insert(QStringLiteral("process_working_set_after_bytes"),
                           static_cast<qint64>(workingSetAfter));
        performance.insert(QStringLiteral("process_working_set_peak_sampled_bytes"),
                           static_cast<qint64>(peakWorkingSet));
        performance.insert(QStringLiteral("working_set_growth_peak_bytes"),
            static_cast<qint64>(peakWorkingSet >= workingSetBefore
                ? peakWorkingSet - workingSetBefore : 0));
        performance.insert(QStringLiteral("memory_sampling_interval_steps"), 8);
        frames.insert(QStringLiteral("performance"), performance);
        captures.insert(label, frames);
        return true;
    };

    if (!captureMode(QStringLiteral("baseline_disabled_default"), QString(), false)) return 6;
    if (!particle->setLayerPropertyValue(
            QStringLiteral("particle.shapeTarget.enabled"), false)) return 7;
    if (!resetSimulation()) return 16;
    if (renderer.renderToQImage(ArtifactCore::FramePosition(30),
                                &composition).isNull() ||
        !stopEmission()) return 17;
    const QImage explicitDisabled = renderer.renderToQImage(
        ArtifactCore::FramePosition(90), &composition);
    const QImage defaultDisabled(output.filePath(
        QStringLiteral("baseline_disabled_default_f0090.png")));
    if (explicitDisabled.isNull() || !explicitDisabled.save(
            output.filePath(QStringLiteral("baseline_disabled_explicit_f0090.png")), "PNG")) return 8;
    const auto disabledDifference = imageDifference(defaultDisabled, explicitDisabled);
    report.insert(QStringLiteral("disabled_default_vs_explicit_different_pixels"),
                  static_cast<qint64>(disabledDifference[0]));
    report.insert(QStringLiteral("disabled_default_vs_explicit_max_channel_delta"),
                  static_cast<qint64>(disabledDifference[1]));
    if (!captureMode(QStringLiteral("shape_ring"), shape->id().toString(), true) ||
        !captureMode(QStringLiteral("text_A8"), text->id().toString(), true)) return 9;

    const QImage baselineFrame90(output.filePath(
        QStringLiteral("baseline_disabled_default_f0090.png")));
    if (baselineFrame90.isNull()) return 12;
    QJsonObject noOpTargets;
    const auto validateNoOpTarget = [&](const QString& label,
                                        const QString& targetId) {
        if (!particle->setLayerPropertyValue(
                QStringLiteral("particle.shapeTarget.layerId"), targetId) ||
            !particle->setLayerPropertyValue(
                QStringLiteral("particle.shapeTarget.enabled"), true)) {
            return false;
        }
        if (!resetSimulation()) return false;
        if (renderer.renderToQImage(ArtifactCore::FramePosition(30),
                                    &composition).isNull() ||
            !stopEmission()) return false;
        const QImage image = renderer.renderToQImage(
            ArtifactCore::FramePosition(90), &composition);
        if (image.isNull()) return false;
        const QString imageName = label + QStringLiteral("_f0090.png");
        if (!image.save(output.filePath(imageName), "PNG")) return false;
        const auto difference = imageDifference(baselineFrame90, image);
        QJsonObject result;
        result.insert(QStringLiteral("image"), imageName);
        result.insert(QStringLiteral("different_pixels"),
                      static_cast<qint64>(difference[0]));
        result.insert(QStringLiteral("max_channel_delta"),
                      static_cast<qint64>(difference[1]));
        result.insert(QStringLiteral("particle_regions"),
                      particleRegionStats(*particle, textTargetBounds,
                                          shapeLayerImage, textLayerImage));
        noOpTargets.insert(label, result);
        return difference[0] == 0 && difference[1] == 0;
    };
    if (!validateNoOpTarget(QStringLiteral("missing_target"),
                            QStringLiteral("missing-particle-target-id")) ||
        !validateNoOpTarget(QStringLiteral("self_target"),
                            particle->id().toString()) ||
        !validateNoOpTarget(QStringLiteral("empty_shape_target"),
                            emptyShape->id().toString())) return 13;
    report.insert(QStringLiteral("no_op_targets"), noOpTargets);
    report.insert(QStringLiteral("captures"), captures);
    QFile reportFile(output.filePath(QStringLiteral("shape_target_report.json")));
    if (!reportFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 10;
    reportFile.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
    reportFile.close();
    if (disabledDifference[0] != 0 || disabledDifference[1] != 0) return 11;
    return 0;
}

static int checkShapeTargetPersistence()
{
    Artifact::ArtifactParticleLayer layer;
    const QJsonObject defaults = layer.toJson()
        .value(QStringLiteral("shapeTarget")).toObject();
    if (defaults.value(QStringLiteral("enabled")).toBool(true) ||
        defaults.value(QStringLiteral("strength")).toDouble(-1.0) != 180.0) {
        return 1;
    }
    if (!layer.setLayerPropertyValue(
            QStringLiteral("particle.shapeTarget.enabled"), true) ||
        !layer.setLayerPropertyValue(
            QStringLiteral("particle.shapeTarget.layerId"),
            QStringLiteral("f7b5a122-756e-4fd4-9f48-5070a1b50002")) ||
        !layer.setLayerPropertyValue(
            QStringLiteral("particle.shapeTarget.strength"), 725.0)) {
        return 2;
    }
    const QJsonObject saved = layer.toJson()
        .value(QStringLiteral("shapeTarget")).toObject();
    if (!saved.value(QStringLiteral("enabled")).toBool(false) ||
        saved.value(QStringLiteral("layerId")).toString() !=
            QStringLiteral("f7b5a122-756e-4fd4-9f48-5070a1b50002") ||
        saved.value(QStringLiteral("strength")).toDouble(-1.0) != 725.0) {
        return 3;
    }
    Artifact::ArtifactParticleLayer restored;
    restored.fromJsonProperties(layer.toJson());
    if (restored.toJson().value(QStringLiteral("shapeTarget")) !=
        layer.toJson().value(QStringLiteral("shapeTarget"))) {
        return 4;
    }
    QJsonObject legacy = layer.toJson();
    legacy.remove(QStringLiteral("shapeTarget"));
    restored.fromJsonProperties(legacy);
    const QJsonObject reset = restored.toJson()
        .value(QStringLiteral("shapeTarget")).toObject();
    if (reset.value(QStringLiteral("enabled")).toBool(true) ||
        !reset.value(QStringLiteral("layerId")).toString().isEmpty() ||
        reset.value(QStringLiteral("strength")).toDouble(-1.0) != 180.0) {
        return 5;
    }
    return 0;
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    if (arguments.size() == 2 && arguments[1] == QStringLiteral("--check-life-curves")) {
        return checkLifeCurvePersistence();
    }
    if (arguments.size() == 3 && arguments[1] == QStringLiteral("--capture-gpu-shapes")) {
        return captureGpuShapes(arguments[2]);
    }
    if (arguments.size() == 3 && arguments[1] == QStringLiteral("--capture-firework")) {
        return captureFirework(arguments[2]);
    }
    if (arguments.size() == 3 && arguments[1] == QStringLiteral("--capture-wind-leaves")) {
        return captureWindLeaves(arguments[2]);
    }
    if (arguments.size() == 3 && arguments[1] == QStringLiteral("--capture-gpu-particle")) {
        return captureGpuParticle(arguments[2]);
    }
    if (arguments.size() == 3 && arguments[1] == QStringLiteral("--capture-shape-target")) {
        return captureShapeTarget(arguments[2]);
    }
    if (arguments.size() == 2 && arguments[1] == QStringLiteral("--check-shape-target-persistence")) {
        return checkShapeTargetPersistence();
    }
    if (arguments.size() == 5 && arguments[1] == QStringLiteral("--capture-flipbook")) {
        return captureFlipbookSources(arguments[2], arguments[3], arguments[4]);
    }
    if (arguments.size() > 1) return 10;
    ParticleLayerWindow window;
    window.show();
    return app.exec();
}
