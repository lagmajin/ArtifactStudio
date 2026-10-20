#include <QApplication>
#include <QImage>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QDebug>
#include <QStringList>
#include <QShowEvent>
#include <QTimer>
#include <QVector>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "ArtifactTestWindowCapture.h"

import Artifact.Render.IRenderer;
import Artifact.Widgets.ViewportOverlay;

namespace {

constexpr float CanvasWidth = 1920.0f;
constexpr float CanvasHeight = 1080.0f;
constexpr std::array<float, 4> ZoomLevels{0.25f, 0.5f, 1.0f, 2.0f};
constexpr std::array<float, 4> GridIntervals{16.0f, 32.0f, 64.0f, 128.0f};

class CompositionViewportGridTest final : public QWidget {
public:
    CompositionViewportGridTest()
    {
        setWindowTitle(QStringLiteral("Composition Viewport Grid Test"));
        setAttribute(Qt::WA_NativeWindow);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setMinimumSize(720, 480);
        resize(1280, 720);
        setFocusPolicy(Qt::StrongFocus);
    }

    void setDisplayLayers(bool grid, bool safeArea, bool guides)
    {
        showGrid_ = grid;
        showSafeArea_ = safeArea;
        showGuides_ = guides;
        if (rendererReady_) {
            updateTitle();
            update();
        }
    }

    void setTraceFirstFrame(bool enabled) { traceFirstFrame_ = enabled; }

    ~CompositionViewportGridTest() override
    {
        if (rendererReady_) renderer_.destroy();
    }

protected:
    void showEvent(QShowEvent* event) override
    {
        QWidget::showEvent(event);
        if (rendererAttempted_) return;
        rendererAttempted_ = true;
        renderer_.initialize(this);
        rendererReady_ = renderer_.isInitialized();
        if (!rendererReady_) {
            setWindowTitle(QStringLiteral(
                "Composition Viewport Grid Test — renderer initialization failed"));
            return;
        }
        renderer_.setCanvasSize(CanvasWidth, CanvasHeight);
        renderer_.setViewportSize(static_cast<float>(width() * devicePixelRatioF()),
                                  static_cast<float>(height() * devicePixelRatioF()));
        renderer_.fitToViewport(48.0f);
        syncZoomPreset();
        updateTitle();
        update();
    }

    void paintEvent(QPaintEvent*) override
    {
        if (!rendererReady_) return;

        const bool captureTrace = traceFirstFrame_ && !traceCaptured_;
        if (captureTrace) renderer_.beginFrameCostCapture();

        renderer_.setViewportSize(static_cast<float>(width() * devicePixelRatioF()),
                                  static_cast<float>(height() * devicePixelRatioF()));
        renderer_.setClearColor(ArtifactCore::FloatColor(0.035f, 0.041f, 0.052f, 1.0f));
        renderer_.clear();

        float panX = 0.0f;
        float panY = 0.0f;
        renderer_.getPan(panX, panY);
        const float zoom = std::max(0.001f, renderer_.getZoom());
        const float spacing = GridIntervals[static_cast<std::size_t>(intervalIndex_)];
        const float originX = -panX / zoom;
        const float originY = -panY / zoom;
        const float extentW = static_cast<float>(width() * devicePixelRatioF()) / zoom;
        const float extentH = static_cast<float>(height() * devicePixelRatioF()) / zoom;

        if (showGrid_) {
            renderer_.drawGrid(originX, originY, extentW, extentH, spacing,
                               std::max(1.0f, 1.0f / zoom),
                               ArtifactCore::FloatColor(0.52f, 0.62f, 0.78f, 0.30f));
            renderer_.drawGrid(originX, originY, extentW, extentH, spacing * 4.0f,
                               std::max(1.0f, 1.5f / zoom),
                               ArtifactCore::FloatColor(0.84f, 0.70f, 0.43f, 0.58f));
        }
        Artifact::ViewportOverlay::drawSafeAreaAndOrigin(
            &renderer_, CanvasWidth, CanvasHeight, showSafeArea_, false);
        Artifact::ViewportOverlay::drawGuides(
            &renderer_, verticalGuides_, horizontalGuides_, showGuides_,
            CanvasWidth, CanvasHeight);
        Artifact::ViewportOverlay::drawCompositionBoundary(
            &renderer_, CanvasWidth, CanvasHeight, true);

        renderer_.flush();
        renderer_.present();
        if (captureTrace) {
            renderer_.endFrameCostCapture();
            const auto passes = renderer_.frameDebugPasses();
            qInfo().noquote() << "[ViewportGridTest] recorded passes:"
                              << static_cast<qulonglong>(passes.size());
            for (const auto& pass : passes) {
                qInfo().noquote() << "[ViewportGridTest]" << pass.name
                                  << pass.shaderName << pass.note;
                for (const auto& binding : pass.debugBindings) {
                    qInfo().noquote() << "[ViewportGridTest] " << binding.key
                                      << '=' << binding.value << binding.stage;
                }
            }
            traceCaptured_ = true;
        }
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (!rendererReady_) {
            QWidget::keyPressEvent(event);
            return;
        }
        constexpr float PanStep = 80.0f;
        float panX = 0.0f;
        float panY = 0.0f;
        renderer_.getPan(panX, panY);
        switch (event->key()) {
        case Qt::Key_1: case Qt::Key_2: case Qt::Key_3: case Qt::Key_4:
            zoomIndex_ = event->key() - Qt::Key_1;
            renderer_.setZoom(ZoomLevels[static_cast<std::size_t>(zoomIndex_)]);
            break;
        case Qt::Key_G:
            intervalIndex_ = (intervalIndex_ + 1) % static_cast<int>(GridIntervals.size());
            break;
        case Qt::Key_S: showSafeArea_ = !showSafeArea_; break;
        case Qt::Key_H: showGuides_ = !showGuides_; break;
        case Qt::Key_V: showGrid_ = !showGrid_; break;
        case Qt::Key_Left: panX += PanStep; renderer_.setPan(panX, panY); break;
        case Qt::Key_Right: panX -= PanStep; renderer_.setPan(panX, panY); break;
        case Qt::Key_Up: panY += PanStep; renderer_.setPan(panX, panY); break;
        case Qt::Key_Down: panY -= PanStep; renderer_.setPan(panX, panY); break;
        case Qt::Key_F:
            renderer_.fitToViewport(48.0f);
            syncZoomPreset();
            break;
        default:
            QWidget::keyPressEvent(event);
            return;
        }
        updateTitle();
        update();
    }

private:
    void syncZoomPreset()
    {
        float bestDistance = std::numeric_limits<float>::max();
        for (std::size_t i = 0; i < ZoomLevels.size(); ++i) {
            const float distance = std::abs(renderer_.getZoom() - ZoomLevels[i]);
            if (distance < bestDistance) {
                bestDistance = distance;
                zoomIndex_ = static_cast<int>(i);
            }
        }
    }

    void updateTitle()
    {
        setWindowTitle(QStringLiteral("Composition Viewport Grid Test — %1% | step %2 px | grid %3, safe area %4, guides %5 | 1-4 zoom, G step, V/S/H toggles, arrows pan, F fit")
                .arg(static_cast<int>(renderer_.getZoom() * 100.0f))
                .arg(static_cast<int>(GridIntervals[static_cast<std::size_t>(intervalIndex_)]))
                .arg(showGrid_ ? QStringLiteral("on") : QStringLiteral("off"))
                .arg(showSafeArea_ ? QStringLiteral("on") : QStringLiteral("off"))
                .arg(showGuides_ ? QStringLiteral("on") : QStringLiteral("off")));
    }

    Artifact::ArtifactIRenderer renderer_;
    int zoomIndex_ = 1;
    int intervalIndex_ = 1;
    bool showGrid_ = true;
    bool showSafeArea_ = true;
    bool showGuides_ = true;
    bool rendererAttempted_ = false;
    bool rendererReady_ = false;
    bool traceFirstFrame_ = false;
    bool traceCaptured_ = false;
    QVector<float> verticalGuides_;
    QVector<float> horizontalGuides_;
};

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    CompositionViewportGridTest window;
    QString capturePath;
    QString captureMode;
    const QStringList args = app.arguments();
    for (qsizetype i = 1; i < args.size(); ++i) {
        if (args[i] == QStringLiteral("--capture") && i + 1 < args.size()) {
            capturePath = args[i + 1];
            ++i;
        } else if (args[i] == QStringLiteral("--grid-only") ||
                   args[i] == QStringLiteral("--safe-area-only") ||
                   args[i] == QStringLiteral("--guides-only")) {
            captureMode = args[i];
        }
    }
    const bool traceRender = args.contains(QStringLiteral("--trace-render"));
    window.setTraceFirstFrame(traceRender);
    if (traceRender) {
        qputenv("ARTIFACT_ENABLE_CONTINUOUS_RENDER_DIAGNOSTICS", "1");
    }
    if (captureMode == QStringLiteral("--grid-only"))
        window.setDisplayLayers(true, false, false);
    else if (captureMode == QStringLiteral("--safe-area-only"))
        window.setDisplayLayers(false, true, false);
    else if (captureMode == QStringLiteral("--guides-only"))
        window.setDisplayLayers(false, false, true);
    window.show();
    if (!capturePath.isEmpty()) {
        QTimer::singleShot(750, &app, [&app, &window, capturePath] {
#ifdef _WIN32
            QString error;
            const QImage image = artifactCaptureWindow(
                reinterpret_cast<ArtifactNativeWindowHandle>(window.winId()),
                &error, 10000, false);
            if (image.isNull() || !image.save(capturePath)) {
                qCritical().noquote() << "Viewport capture failed:" << error;
                app.exit(2);
                return;
            }
            app.exit(0);
#else
            Q_UNUSED(window)
            qCritical() << "Window capture is only supported on Windows";
            app.exit(2);
#endif
        });
    }
    return app.exec();
}
