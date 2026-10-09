#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QPaintEvent>
#include <QPixmap>
#include <QShowEvent>
#include <QString>
#include <QTimerEvent>
#include <QVariant>
#include <QWidget>

#include <array>
#include <algorithm>
#include <cstdint>
#include <limits>

import Artifact.Layer.Particle;
import Artifact.Generator.Particle;
import Artifact.Render.IRenderer;

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
        flipbookPreview_ = new QLabel(this);
        flipbookPreview_->setGeometry(rect());
        flipbookPreview_->setScaledContents(true);
        flipbookPreview_->setAlignment(Qt::AlignCenter);
        flipbookPreview_->setAttribute(Qt::WA_TransparentForMouseEvents);
        flipbookPreview_->hide();

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
        if (flipbookMode_) {
            if (rendererReady_) {
                renderer_.setViewportSize(
                    static_cast<float>(width() * devicePixelRatioF()),
                    static_cast<float>(height() * devicePixelRatioF()));
                renderer_.setClearColor(
                    ArtifactCore::FloatColor(0.018f, 0.024f, 0.04f, 1.0f));
                renderer_.clear();
                renderer_.flush();
                renderer_.present();
            }
            const float seconds = static_cast<float>(frame_) / 30.0f;
            const QImage frameImage = layer_.renderFrame(
                CanvasWidth, CanvasHeight, seconds);
            if (!frameImage.isNull()) {
                flipbookPreview_->setPixmap(QPixmap::fromImage(frameImage));
            }
            return;
        }
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
        case Qt::Key_F1:
        case Qt::Key_N:
            loadPreset(0);
            break;
        case Qt::Key_F2:
            loadFlipbook(QStringLiteral("petal"),
                         event->modifiers().testFlag(Qt::ShiftModifier));
            break;
        case Qt::Key_F3:
            loadFlipbook(QStringLiteral("spark"),
                         event->modifiers().testFlag(Qt::ShiftModifier));
            break;
        case Qt::Key_F4:
            loadFlipbook(QStringLiteral("autumn_leaves"),
                         event->modifiers().testFlag(Qt::ShiftModifier));
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
        flipbookMode_ = false;
        flipbookPreview_->hide();
        static constexpr std::array<const char*, 5> Presets{
            "fountain", "fire", "smoke", "rain", "snow"};
        const int selectedIndex = std::clamp(
            index, 0, static_cast<int>(Presets.size()) - 1);
        layer_.loadPreset(
            QString::fromLatin1(Presets[static_cast<std::size_t>(selectedIndex)]));
        frame_ = 0;
        layer_.resetParticleSystem();
        setWindowTitle(QStringLiteral(
            "Artifact 2D ParticleLayer GPU Test — %1 | F2 petal, F3 spark, F4 leaves")
                           .arg(layer_.presetName()));
    }

    void loadFlipbook(const QString& assetName, bool useSheet)
    {
        QString assetPath = findParticleAsset(assetName, useSheet);
        if (assetPath.isEmpty()) {
            assetPath = useSheet
                ? QFileDialog::getOpenFileName(
                    this, QStringLiteral("Select %1 sprite sheet").arg(assetName),
                    QDir::currentPath(), QStringLiteral("PNG images (*.png)"))
                : QFileDialog::getExistingDirectory(
                    this, QStringLiteral("Select %1 PNG sequence folder").arg(assetName),
                    QDir::currentPath());
        }
        if (assetPath.isEmpty()) return;

        const QString displayName = assetName == QStringLiteral("autumn_leaves")
            ? QStringLiteral("autumn leaves") : assetName;
        layer_.loadPreset(QStringLiteral("leaves"));
        layer_.setParticleBlendMode(Artifact::ParticleBlendMode::Normal);
        layer_.setLayerPropertyValue(
            QStringLiteral("particle.emitter.texturePath"), QVariant(assetPath));
        layer_.setLayerPropertyValue(
            QStringLiteral("particle.emitter.textureRows"), QVariant(useSheet ? 4 : 1));
        layer_.setLayerPropertyValue(
            QStringLiteral("particle.emitter.textureCols"), QVariant(useSheet ? 4 : 16));
        layer_.setLayerPropertyValue(
            QStringLiteral("particle.emitter.frameCount"), QVariant(16));
        layer_.setLayerPropertyValue(
            QStringLiteral("particle.emitter.frameRate"), QVariant(12.0));
        layer_.setLayerPropertyValue(
            QStringLiteral("particle.emitter.randomFrame"), QVariant(false));
        frame_ = 0;
        layer_.resetParticleSystem();
        flipbookMode_ = true;
        flipbookPreview_->show();
        flipbookPreview_->raise();
        setWindowTitle(QStringLiteral(
            "Artifact 2D ParticleLayer %1 — %2 | Shift+F2/F3/F4 sheet, F2/F3/F4 sequence, N GPU")
                           .arg(useSheet ? QStringLiteral("Sprite Sheet")
                                         : QStringLiteral("PNG Sequence"),
                                displayName));
    }

    QString findParticleAsset(const QString& assetName, bool useSheet) const
    {
        const std::array<QString, 2> starts{
            QDir::currentPath(), QApplication::applicationDirPath()};
        for (const QString& start : starts) {
            QDir directory(start);
            for (int depth = 0; depth < 8; ++depth) {
                const QString candidate = directory.filePath(
                    QStringLiteral("temp/particle_flipbook_test/%1")
                        .arg(useSheet ? assetName + QStringLiteral("_sheet.png")
                                      : assetName));
                const QFileInfo candidateInfo(candidate);
                if (useSheet ? candidateInfo.isFile() : candidateInfo.isDir()) {
                    return candidate;
                }
                if (!directory.cdUp()) break;
            }
        }
        return {};
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
    QLabel* flipbookPreview_ = nullptr;
    int timerId_ = 0;
    std::int64_t frame_ = 0;
    bool paused_ = false;
    bool flipbookMode_ = false;
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

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    if (arguments.size() == 3 && arguments[1] == QStringLiteral("--capture-firework")) {
        return captureFirework(arguments[2]);
    }
    ParticleLayerWindow window;
    window.show();
    return app.exec();
}
