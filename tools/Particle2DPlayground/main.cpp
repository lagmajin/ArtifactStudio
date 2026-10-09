#include <QApplication>
#include <QDir>
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

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    ParticleLayerWindow window;
    window.show();
    return app.exec();
}
