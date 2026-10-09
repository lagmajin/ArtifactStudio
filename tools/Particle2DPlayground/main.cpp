#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QShowEvent>
#include <QString>
#include <QTimerEvent>
#include <QVariant>
#include <QWidget>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

import Artifact.Layer.Particle;
import Artifact.Generator.Particle;
import Artifact.Render.IRenderer;
import Graphics.ParticleData;

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
    report.write(QStringLiteral("particle_pixels=%1 green_rect_pixels=%2\n%3\n")
                     .arg(particlePixels).arg(greenRectPixels).arg(debug).toUtf8());
    renderer.destroy();
    if (greenRectPixels == 0 || particlePixels == 0 ||
        !debug.contains(QStringLiteral("state=drawn"))) {
        qCritical("GPU particle capture did not contain the expected reference shapes");
        return 6;
    }
    return 0;
}

static int captureFlipbookMode(const QString& sourcePath, bool isSequence,
                               const QString& outputDirectory,
                               const QString& filePrefix,
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
    Artifact::ArtifactIRenderer softwareRenderer;

    for (std::size_t index = 0; index < Times.size(); ++index) {
        const auto frameNumber = static_cast<std::int64_t>(std::lround(Times[index] * 30.0f));
        layer.goToFrame(frameNumber);
        layer.draw(&softwareRenderer);
        QImage frame;
        if (!layer.getCachedFrame(frameNumber, frame) || frame.isNull()) return 2;
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

    QImage sequenceFirst, sequenceLast, sheetFirst, sheetLast;
    const int sequenceResult = captureFlipbookMode(
        sequenceSource.absolutePath(), true,
        output.filePath(QStringLiteral("sequence")), QStringLiteral("sequence"),
        sequenceFirst, sequenceLast, report);
    if (sequenceResult != 0) return 10 + sequenceResult;
    const int sheetResult = captureFlipbookMode(
        QDir::cleanPath(spriteSheetPath), false,
        output.filePath(QStringLiteral("sprite_sheet")), QStringLiteral("sheet"),
        sheetFirst, sheetLast, report);
    if (sheetResult != 0) return 20 + sheetResult;
    const auto arePixelIdentical = [](const QImage& left, const QImage& right) {
        if (left.size() != right.size() || left.format() != right.format()) return false;
        for (int y = 0; y < left.height(); ++y) {
            const auto* leftRow = reinterpret_cast<const QRgb*>(left.constScanLine(y));
            const auto* rightRow = reinterpret_cast<const QRgb*>(right.constScanLine(y));
            for (int x = 0; x < left.width(); ++x) {
                if (leftRow[x] != rightRow[x]) return false;
            }
        }
        return true;
    };
    if (!arePixelIdentical(sequenceFirst, sheetFirst) ||
        !arePixelIdentical(sequenceLast, sheetLast)) return 30;
    report.write("sequence_sheet_first_and_last_frames_pixel_identical=true\n");
    return 0;
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    if (arguments.size() == 3 && arguments[1] == QStringLiteral("--capture-firework")) {
        return captureFirework(arguments[2]);
    }
    if (arguments.size() == 3 && arguments[1] == QStringLiteral("--capture-wind-leaves")) {
        return captureWindLeaves(arguments[2]);
    }
    if (arguments.size() == 3 && arguments[1] == QStringLiteral("--capture-gpu-particle")) {
        return captureGpuParticle(arguments[2]);
    }
    if (arguments.size() == 5 && arguments[1] == QStringLiteral("--capture-flipbook")) {
        return captureFlipbookSources(arguments[2], arguments[3], arguments[4]);
    }
    if (arguments.size() > 1) return 10;
    ParticleLayerWindow window;
    window.show();
    return app.exec();
}
