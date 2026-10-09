#include <QApplication>
#include <QKeyEvent>
#include <QPaintEvent>
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
import Size;

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

        layer_.setSourceSize(ArtifactCore::Size_2D(CanvasWidth, CanvasHeight));
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

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    ParticleLayerWindow window;
    window.show();
    return app.exec();
}
