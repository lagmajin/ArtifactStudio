#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QEventLoop>
#include <QImage>
#include <QLabel>
#include <QMainWindow>
#include <QQuaternion>
#include <QScreen>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QJsonArray>
#include <QJsonObject>

#include <chrono>
#include <cmath>
#include <future>
#include <memory>
#include <thread>
#include <utility>

#include "../ArtifactTestMcpServer.h"
#include "../ArtifactTestWindowCapture.h"

import Artifact.Application.Manager;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Layer.Camera;
import Artifact.Render.IRenderer;
import Artifact.Widgets.CompositionRenderController;
import Animation.Transform3D;
import Color.Float;
import Composition3D;
import Math.Vec;
import Memory.SharedPtr;
import Utils.Id;
import Utils.String.UniString;

namespace {

class CameraFrustumViewerWindow final : public QMainWindow {
public:
    CameraFrustumViewerWindow()
    {
        setWindowTitle(QStringLiteral("Artifact Camera Frustum Viewport Test"));
        resize(1280, 820);

        auto* root = new QWidget(this);
        auto* layout = new QVBoxLayout(root);
        layout->addWidget(new QLabel(
            QStringLiteral("Composition Viewport camera frustum overlay | "
                           "MCP: viewport.state / viewport.setOverlay / "
                           "viewport.setProjection / viewport.setView / viewport.capture"),
            root));
        status_ = new QLabel(QStringLiteral("Preparing camera fixture…"), root);
        layout->addWidget(status_);
        viewportHost_ = new QWidget(root);
        viewportHost_->setAttribute(Qt::WA_NativeWindow, true);
        layout->addWidget(viewportHost_, 1);
        setCentralWidget(root);

        Artifact::ArtifactApplicationManager::instance();
        prepareFixture();
    }

    ~CameraFrustumViewerWindow() override
    {
        if (controller_) {
            controller_->stop();
            controller_->destroy();
        }
    }

    bool ready() const { return ready_; }

    void initializeViewport()
    {
        if (controller_ || !composition_) return;
        controller_ = new Artifact::CompositionRenderController(this);
        controller_->initialize(viewportHost_);
        if (!controller_->isInitialized()) {
            delete controller_;
            controller_ = nullptr;
            status_->setText(QStringLiteral("CompositionRenderController initialization failed"));
            error_ = status_->text();
            return;
        }
        controller_->setComposition(composition_);
        controller_->setShowOutsideComposition(false);
        controller_->setPresentationLayout(
            Artifact::CompositionViewportPresentationLayout::Single);
        controller_->setSelectedLayerId(camera_->id());
        controller_->setShowCameraFrustumOverlay(true);
        controller_->setViewportOrientationQuaternion(
            QQuaternion::fromEulerAngles(20.0f, -32.0f, 0.0f));
        controller_->start();
        controller_->markRenderDirty();
        controller_->renderOneFrame();
        ready_ = true;
        status_->setText(QStringLiteral("Ready — active perspective camera, 52° FOV, "
                                        "near 80, far 900. Toggle the overlay or projection with MCP."));
    }

    QJsonObject state() const
    {
        QJsonArray nearCorners;
        QJsonArray farCorners;
        bool visualValid = false;
        if (controller_) {
            const auto visual = controller_->cameraFrustumVisual();
            visualValid = visual.valid;
            for (const auto& point : visual.nearPlaneCorners)
                nearCorners.append(QJsonArray{point.x, point.y, point.z});
            for (const auto& point : visual.farPlaneCorners)
                farCorners.append(QJsonArray{point.x, point.y, point.z});
        }
        return {
            {QStringLiteral("ready"), ready_},
            {QStringLiteral("error"), error_},
            {QStringLiteral("overlayEnabled"), controller_ &&
                controller_->isShowCameraFrustumOverlay()},
            {QStringLiteral("visualValid"), visualValid},
            {QStringLiteral("selectedLayer"), camera_ ? camera_->id().toString() : QString()},
            {QStringLiteral("activeCamera"), camera_ && camera_->isActiveCamera()},
            {QStringLiteral("projection"), camera_ && camera_->projectionMode() ==
                Artifact::ProjectionMode::Orthographic
                    ? QStringLiteral("orthographic") : QStringLiteral("perspective")},
            {QStringLiteral("fieldOfViewDegrees"), camera_ ? camera_->fov().value : 0.0f},
            {QStringLiteral("nearClip"), camera_ ? camera_->nearClipPlane().value : 0.0f},
            {QStringLiteral("farClip"), camera_ ? camera_->farClipPlane().value : 0.0f},
            {QStringLiteral("nearPlaneCorners"), nearCorners},
            {QStringLiteral("farPlaneCorners"), farCorners},
            {QStringLiteral("viewportOrientationEuler"),
                QJsonArray{viewYaw_, viewPitch_, 0.0}},
            {QStringLiteral("fixture"), QStringLiteral("single active camera layer")}
        };
    }

    QJsonObject setOverlay(bool enabled)
    {
        if (!controller_)
            return {{QStringLiteral("error"), QStringLiteral("viewport is not initialized")}};
        controller_->setShowCameraFrustumOverlay(enabled);
        renderFrame();
        return state();
    }

    QJsonObject setProjection(const QString& projection)
    {
        if (!camera_)
            return {{QStringLiteral("error"), QStringLiteral("camera fixture is unavailable")}};
        if (projection.compare(QStringLiteral("perspective"), Qt::CaseInsensitive) == 0) {
            camera_->setProjectionMode(Artifact::ProjectionMode::Perspective);
        } else if (projection.compare(QStringLiteral("orthographic"), Qt::CaseInsensitive) == 0) {
            camera_->setProjectionMode(Artifact::ProjectionMode::Orthographic);
        } else {
            return {{QStringLiteral("error"), QStringLiteral("projection must be perspective or orthographic")}};
        }
        renderFrame();
        return state();
    }

    QJsonObject setView(double yaw, double pitch)
    {
        if (!controller_ || !std::isfinite(yaw) || !std::isfinite(pitch) ||
            yaw < -180.0 || yaw > 180.0 || pitch < -89.0 || pitch > 89.0) {
            return {{QStringLiteral("error"), QStringLiteral("view requires yaw [-180,180] and pitch [-89,89]")}};
        }
        viewYaw_ = yaw;
        viewPitch_ = pitch;
        controller_->setViewportOrientationQuaternion(QQuaternion::fromEulerAngles(
            static_cast<float>(viewPitch_), static_cast<float>(viewYaw_), 0.0f));
        renderFrame();
        return state();
    }

    QJsonObject capture()
    {
#ifdef _WIN32
        struct CaptureResult { QImage image; QString error; };
        auto promise = std::make_shared<std::promise<CaptureResult>>();
        auto future = promise->get_future();
        std::thread worker;
        const HWND nativeWindow = reinterpret_cast<HWND>(winId());
        QTimer::singleShot(50, [promise, &worker, nativeWindow] {
            try {
                worker = std::thread([promise, nativeWindow] {
                    CaptureResult result;
                    result.image = artifactCaptureWindow(nativeWindow, &result.error, 10000);
                    promise->set_value(std::move(result));
                });
            } catch (...) {
                promise->set_value(CaptureResult{{}, QStringLiteral("Could not start capture worker")});
            }
        });
        while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
            QCoreApplication::processEvents(
                QEventLoop::AllEvents | QEventLoop::ExcludeUserInputEvents, 5);
            QThread::msleep(1);
        }
        auto captured = future.get();
        if (worker.joinable()) worker.join();
        if (captured.image.isNull())
            return {{QStringLiteral("error"), captured.error.isEmpty()
                ? QStringLiteral("window capture failed") : captured.error}};
        const QByteArray png = artifactEncodePng(captured.image);
        if (png.isEmpty())
            return {{QStringLiteral("error"), QStringLiteral("PNG encoding failed")}};
        QJsonObject result = state();
        result[QStringLiteral("captureWidth")] = captured.image.width();
        result[QStringLiteral("captureHeight")] = captured.image.height();
        result[QStringLiteral("mcpContent")] = QJsonArray{QJsonObject{
            {QStringLiteral("type"), QStringLiteral("image")},
            {QStringLiteral("mimeType"), QStringLiteral("image/png")},
            {QStringLiteral("data"), QString::fromLatin1(png.toBase64())}}};
        return result;
#else
        return {{QStringLiteral("error"), QStringLiteral("window capture is only available on Windows")}};
#endif
    }

private:
    void prepareFixture()
    {
        Artifact::ArtifactCompositionInitParams params;
        params.setCompositionName(ArtifactCore::UniString(
            QStringLiteral("Camera Frustum Viewport Fixture")));
        params.setResolution(960, 540);
        params.setFrameRate(30.0);
        params.setDurationFrames(30);
        params.setBackgroundColor(ArtifactCore::FloatColor(0.025f, 0.035f, 0.055f, 1.0f));
        composition_ = ArtifactCore::makeShared<Artifact::ArtifactComposition3D>(
            ArtifactCore::CompositionID(QStringLiteral("camera-frustum-viewport-fixture")), params);
        if (!composition_) {
            error_ = QStringLiteral("composition creation failed");
            status_->setText(error_);
            return;
        }

        camera_ = ArtifactCore::makeShared<Artifact::ArtifactCameraLayer>();
        camera_->setLayerName(QStringLiteral("Frustum Test Camera"));
        camera_->setActiveCamera(true);
        camera_->setProjectionMode(Artifact::ProjectionMode::Perspective);
        camera_->setUseManualFov(true);
        camera_->setFov(ArtifactCore::Units::Degrees{52.0f});
        camera_->setNearClipPlane(ArtifactCore::Units::Pixels{80.0f});
        camera_->setFarClipPlane(ArtifactCore::Units::Pixels{900.0f});
        camera_->transform3D().setCurrentPosition(480.0f, 270.0f);
        camera_->transform3D().setCurrentPositionZ(1050.0f);
        composition_->appendLayerTop(camera_);
    }

    void renderFrame()
    {
        if (!controller_) return;
        controller_->markRenderDirty();
        controller_->renderOneFrame();
    }

    QWidget* viewportHost_ = nullptr;
    QLabel* status_ = nullptr;
    Artifact::CompositionRenderController* controller_ = nullptr;
    Artifact::ArtifactCompositionPtr composition_;
    ArtifactCore::SharedPtr<Artifact::ArtifactCameraLayer> camera_;
    double viewYaw_ = -32.0;
    double viewPitch_ = 20.0;
    bool ready_ = false;
    QString error_;
};

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    Artifact::ArtifactApplicationManager::instance();

    CameraFrustumViewerWindow window;
    bool mcpStdio = false;
    for (int index = 1; index < argc; ++index)
        mcpStdio |= QString::fromLocal8Bit(argv[index]) == QStringLiteral("--mcp-stdio");

    ArtifactTestMcpServer mcpServer(
        QStringLiteral("artifact-camera-frustum-viewer"), QStringLiteral("1.0.0"));
    if (mcpStdio) {
        const QJsonObject objectSchema{{QStringLiteral("type"), QStringLiteral("object")}};
        const QJsonObject enabledSchema{{QStringLiteral("type"), QStringLiteral("boolean")}};
        const QJsonObject enabledToolSchema{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("enabled"), enabledSchema}}},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("enabled")}}};
        const QJsonObject projectionValueSchema{
            {QStringLiteral("type"), QStringLiteral("string")},
            {QStringLiteral("enum"), QJsonArray{
                QStringLiteral("perspective"), QStringLiteral("orthographic")}}};
        const QJsonObject projectionToolSchema{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("projection"), projectionValueSchema}}},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("projection")}}};
        const QJsonObject yawSchema{
            {QStringLiteral("type"), QStringLiteral("number")},
            {QStringLiteral("minimum"), -180}, {QStringLiteral("maximum"), 180}};
        const QJsonObject pitchSchema{
            {QStringLiteral("type"), QStringLiteral("number")},
            {QStringLiteral("minimum"), -89}, {QStringLiteral("maximum"), 89}};
        const QJsonObject viewToolSchema{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("yaw"), yawSchema},
                {QStringLiteral("pitch"), pitchSchema}}},
            {QStringLiteral("required"), QJsonArray{
                QStringLiteral("yaw"), QStringLiteral("pitch")}}};
        bool toolsReady = true;
        toolsReady &= mcpServer.registerTool(QStringLiteral("viewport.state"),
            QStringLiteral("Read camera frustum fixture, overlay, projection, and corners"),
            objectSchema, [&window](const QJsonObject&) { return window.state(); });
        toolsReady &= mcpServer.registerTool(QStringLiteral("viewport.setOverlay"),
            QStringLiteral("Enable or disable the camera frustum overlay"),
            enabledToolSchema,
            [&window](const QJsonObject& args) {
                const auto enabled = args.value(QStringLiteral("enabled"));
                if (!enabled.isBool())
                    return QJsonObject{{QStringLiteral("error"), QStringLiteral("enabled must be boolean")}};
                return window.setOverlay(enabled.toBool());
            });
        toolsReady &= mcpServer.registerTool(QStringLiteral("viewport.setProjection"),
            QStringLiteral("Switch the camera frustum between perspective and orthographic projection"),
            projectionToolSchema,
            [&window](const QJsonObject& args) {
                return window.setProjection(args.value(QStringLiteral("projection")).toString());
            });
        toolsReady &= mcpServer.registerTool(QStringLiteral("viewport.setView"),
            QStringLiteral("Set the viewport inspection yaw and pitch in degrees"),
            viewToolSchema,
            [&window](const QJsonObject& args) {
                const auto yaw = args.value(QStringLiteral("yaw"));
                const auto pitch = args.value(QStringLiteral("pitch"));
                if (!yaw.isDouble() || !pitch.isDouble())
                    return QJsonObject{{QStringLiteral("error"), QStringLiteral("yaw and pitch must be numbers")}};
                return window.setView(yaw.toDouble(), pitch.toDouble());
            });
        toolsReady &= mcpServer.registerTool(QStringLiteral("viewport.capture"),
            QStringLiteral("Capture the standalone camera frustum viewport window"),
            objectSchema, [&window](const QJsonObject&) { return window.capture(); });
        window.setAttribute(Qt::WA_ShowWithoutActivating, true);
        if (!toolsReady || !mcpServer.start()) return 2;
    }

    const QScreen* leftmost = app.primaryScreen();
    for (const auto* screen : app.screens())
        if (leftmost && screen->geometry().left() < leftmost->geometry().left())
            leftmost = screen;
    if (leftmost) {
        window.resize(window.size().boundedTo(leftmost->availableGeometry().size()));
        window.move(leftmost->availableGeometry().topLeft());
    }
    window.show();
    app.processEvents();
    window.initializeViewport();
    if (!window.ready()) return 2;
    return app.exec();
}
