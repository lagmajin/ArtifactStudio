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
#include <QMatrix4x4>
#include <QPoint>
#include <QQuaternion>
#include <QScreen>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector3D>
#include <QWidget>
#include <QJsonArray>
#include <QJsonObject>

#include <chrono>
#include <cmath>
#include <algorithm>
#include <array>
#include <cstddef>
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
import Artifact.Layers.Model3D;
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
                           "viewport.setProjection / viewport.setView / "
                           "viewport.verifyFrustum / viewport.measureOverlay / "
                           "viewport.runChecks / viewport.cullingStats / "
                           "viewport.testCulling / viewport.capture"),
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

    QImage captureImage(QString* error) const
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
        if (error) *error = captured.error;
        return captured.image;
#else
        if (error) *error = QStringLiteral("window capture is only available on Windows");
        return {};
#endif
    }

    QJsonObject capture()
    {
        QString captureError;
        const QImage image = captureImage(&captureError);
        if (image.isNull())
            return {{QStringLiteral("error"), captureError.isEmpty()
                ? QStringLiteral("window capture failed") : captureError}};
        const QByteArray png = artifactEncodePng(image);
        if (png.isEmpty())
            return {{QStringLiteral("error"), QStringLiteral("PNG encoding failed")}};
        QJsonObject result = state();
        result[QStringLiteral("captureWidth")] = image.width();
        result[QStringLiteral("captureHeight")] = image.height();
        result[QStringLiteral("mcpContent")] = QJsonArray{QJsonObject{
            {QStringLiteral("type"), QStringLiteral("image")},
            {QStringLiteral("mimeType"), QStringLiteral("image/png")},
            {QStringLiteral("data"), QString::fromLatin1(png.toBase64())}}};
        return result;
    }

    QJsonObject verifyFrustum() const
    {
        if (!controller_)
            return {{QStringLiteral("error"), QStringLiteral("viewport is not initialized")}};

        const auto visual = controller_->cameraFrustumVisual();
        const bool cornersPresent = visual.valid &&
            visual.nearPlaneCorners.size() == 4 && visual.farPlaneCorners.size() == 4;

        const auto toVector = [](const ArtifactCore::Coordinates::WorldPoint3& point) {
            return QVector3D(point.x, point.y, point.z);
        };
        const auto isFinite = [](const ArtifactCore::Coordinates::WorldPoint3& point) {
            return std::isfinite(point.x) && std::isfinite(point.y) &&
                   std::isfinite(point.z);
        };

        bool finite = cornersPresent;
        if (finite) {
            for (const auto& point : visual.nearPlaneCorners) finite &= isFinite(point);
            for (const auto& point : visual.farPlaneCorners) finite &= isFinite(point);
            finite &= isFinite(visual.cameraPosition);
        }

        bool invertible = false;
        const QMatrix4x4 inverseView = visual.viewMatrix.inverted(&invertible);
        QVector3D forward;
        if (invertible)
            forward = inverseView.mapVector(QVector3D(0.0f, 0.0f, -1.0f)).normalized();
        const QVector3D cameraPosition = toVector(visual.cameraPosition);

        const auto distanceStats =
            [&](const QVector<ArtifactCore::Coordinates::WorldPoint3>& corners,
                float& average, float& deviation) {
                float total = 0.0f;
                for (const auto& corner : corners)
                    total += QVector3D::dotProduct(
                        toVector(corner) - cameraPosition, forward);
                average = total / static_cast<float>(corners.isEmpty() ? 1 : corners.size());
                deviation = 0.0f;
                for (const auto& corner : corners) {
                    const float distance = QVector3D::dotProduct(
                        toVector(corner) - cameraPosition, forward);
                    deviation = std::max(deviation, std::abs(distance - average));
                }
            };

        float nearAverage = 0.0f;
        float nearDeviation = 0.0f;
        float farAverage = 0.0f;
        float farDeviation = 0.0f;
        bool distancesOk = false;
        const float nearClip = camera_ ? camera_->nearClipPlane().value : 0.0f;
        const float farClip = camera_ ? camera_->farClipPlane().value : 0.0f;
        if (cornersPresent && finite && invertible) {
            distanceStats(visual.nearPlaneCorners, nearAverage, nearDeviation);
            distanceStats(visual.farPlaneCorners, farAverage, farDeviation);
            const float nearTolerance = std::max(0.5f, std::abs(nearClip) * 0.01f);
            const float farTolerance = std::max(0.5f, std::abs(farClip) * 0.01f);
            distancesOk = std::abs(nearAverage - nearClip) <= nearTolerance &&
                          nearDeviation <= nearTolerance &&
                          std::abs(farAverage - farClip) <= farTolerance &&
                          farDeviation <= farTolerance;
        }

        float nearWidth = 0.0f;
        float farWidth = 0.0f;
        bool centersAligned = false;
        float centerAlignment = 0.0f;
        if (cornersPresent) {
            nearWidth = (toVector(visual.nearPlaneCorners[1]) -
                         toVector(visual.nearPlaneCorners[0])).length();
            farWidth = (toVector(visual.farPlaneCorners[1]) -
                        toVector(visual.farPlaneCorners[0])).length();

            const QVector3D nearCenter =
                (toVector(visual.nearPlaneCorners[0]) +
                 toVector(visual.nearPlaneCorners[1]) +
                 toVector(visual.nearPlaneCorners[2]) +
                 toVector(visual.nearPlaneCorners[3])) * 0.25f;
            const QVector3D farCenter =
                (toVector(visual.farPlaneCorners[0]) +
                 toVector(visual.farPlaneCorners[1]) +
                 toVector(visual.farPlaneCorners[2]) +
                 toVector(visual.farPlaneCorners[3])) * 0.25f;
            const QVector3D axis = farCenter - nearCenter;
            if (axis.lengthSquared() > 1e-6f && forward.lengthSquared() > 0.5f) {
                centerAlignment = QVector3D::dotProduct(axis.normalized(), forward);
                centersAligned = centerAlignment > 0.999f;
            }
        }

        const bool orthographic = camera_ &&
            camera_->projectionMode() == Artifact::ProjectionMode::Orthographic;
        const float planeWidthDelta = std::abs(nearWidth - farWidth);
        const float parityTolerance = std::max(0.5f, nearWidth * 0.002f);
        const bool planeRelationshipOk = orthographic
            ? planeWidthDelta <= parityTolerance
            : farWidth > nearWidth * 1.5f;

        const bool passed = cornersPresent && finite && invertible && distancesOk &&
                            centersAligned && planeRelationshipOk;

        return {
            {QStringLiteral("passed"), passed},
            {QStringLiteral("valid"), visual.valid},
            {QStringLiteral("cornersPresent"), cornersPresent},
            {QStringLiteral("finite"), finite},
            {QStringLiteral("invertible"), invertible},
            {QStringLiteral("distancesOk"), distancesOk},
            {QStringLiteral("centersAligned"), centersAligned},
            {QStringLiteral("planeRelationshipOk"), planeRelationshipOk},
            {QStringLiteral("projection"), orthographic
                ? QStringLiteral("orthographic") : QStringLiteral("perspective")},
            {QStringLiteral("nearClip"), nearClip},
            {QStringLiteral("farClip"), farClip},
            {QStringLiteral("nearDistance"), nearAverage},
            {QStringLiteral("farDistance"), farAverage},
            {QStringLiteral("nearDistanceDeviation"), nearDeviation},
            {QStringLiteral("farDistanceDeviation"), farDeviation},
            {QStringLiteral("nearPlaneWidth"), nearWidth},
            {QStringLiteral("farPlaneWidth"), farWidth},
            {QStringLiteral("planeWidthDelta"), planeWidthDelta},
            {QStringLiteral("centerAlignment"), centerAlignment}
        };
    }

    QJsonObject measureOverlay()
    {
        if (!controller_ || !viewportHost_)
            return {{QStringLiteral("error"), QStringLiteral("viewport is not initialized")}};

        renderFrame();
        const auto pumpEnd =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(120);
        while (std::chrono::steady_clock::now() < pumpEnd) {
            QCoreApplication::processEvents(
                QEventLoop::AllEvents | QEventLoop::ExcludeUserInputEvents, 5);
            QThread::msleep(5);
        }

        QString captureError;
        const QImage image = captureImage(&captureError);
        if (image.isNull())
            return {{QStringLiteral("error"), captureError.isEmpty()
                ? QStringLiteral("window capture failed") : captureError}};

        const QPoint hostOrigin = viewportHost_->mapTo(this, QPoint(0, 0));
        const double scaleX =
            static_cast<double>(image.width()) / std::max(1, width());
        const double scaleY =
            static_cast<double>(image.height()) / std::max(1, height());
        const int left = std::clamp(
            static_cast<int>(std::floor(hostOrigin.x() * scaleX)),
            0, image.width() - 1);
        const int top = std::clamp(
            static_cast<int>(std::floor(hostOrigin.y() * scaleY)),
            0, image.height() - 1);
        const int right = std::clamp(
            static_cast<int>(std::ceil((hostOrigin.x() + viewportHost_->width()) * scaleX)),
            left + 1, image.width());
        const int bottom = std::clamp(
            static_cast<int>(std::ceil((hostOrigin.y() + viewportHost_->height()) * scaleY)),
            top + 1, image.height());

        std::size_t greenPixels = 0;
        std::size_t bluePixels = 0;
        int minX = image.width();
        int minY = image.height();
        int maxX = -1;
        int maxY = -1;
        for (int y = top; y < bottom; ++y) {
            const auto* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
            for (int x = left; x < right; ++x) {
                const QRgb pixel = row[x];
                const int red = qRed(pixel);
                const int green = qGreen(pixel);
                const int blue = qBlue(pixel);
                const bool overlayGreen =
                    green >= 120 && green >= red + 40 && green >= blue + 25;
                const bool overlayBlue =
                    blue >= 150 && blue >= green + 30 && blue >= red + 60;
                if (overlayGreen) {
                    ++greenPixels;
                } else if (overlayBlue) {
                    ++bluePixels;
                } else {
                    continue;
                }
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
        }

        const std::size_t viewportPixels =
            static_cast<std::size_t>(right - left) * static_cast<std::size_t>(bottom - top);
        const std::size_t overlayPixels = greenPixels + bluePixels;
        const double coveragePercent = viewportPixels == 0
            ? 0.0
            : 100.0 * static_cast<double>(overlayPixels) /
                  static_cast<double>(viewportPixels);

        QJsonArray boundingBox;
        if (maxX >= minX && maxY >= minY) {
            boundingBox = QJsonArray{minX, minY, maxX - minX + 1, maxY - minY + 1};
        }

        return {
            {QStringLiteral("overlayEnabled"), controller_->isShowCameraFrustumOverlay()},
            {QStringLiteral("projection"), camera_ &&
                camera_->projectionMode() == Artifact::ProjectionMode::Orthographic
                    ? QStringLiteral("orthographic") : QStringLiteral("perspective")},
            {QStringLiteral("captureWidth"), image.width()},
            {QStringLiteral("captureHeight"), image.height()},
            {QStringLiteral("viewportRect"), QJsonArray{left, top, right - left, bottom - top}},
            {QStringLiteral("greenPixels"), static_cast<int>(greenPixels)},
            {QStringLiteral("bluePixels"), static_cast<int>(bluePixels)},
            {QStringLiteral("overlayPixels"), static_cast<int>(overlayPixels)},
            {QStringLiteral("coveragePercent"), coveragePercent},
            {QStringLiteral("boundingBox"), boundingBox}
        };
    }

    QJsonObject runChecks()
    {
        if (!controller_ || !camera_)
            return {{QStringLiteral("error"), QStringLiteral("viewport is not initialized")}};

        const bool originalOverlay = controller_->isShowCameraFrustumOverlay();
        const auto originalProjection = camera_->projectionMode();

        const std::array<std::pair<const char*, Artifact::ProjectionMode>, 2> projections{{
            {"perspective", Artifact::ProjectionMode::Perspective},
            {"orthographic", Artifact::ProjectionMode::Orthographic}}};

        QJsonArray cases;
        bool allPassed = true;
        for (const auto& [name, mode] : projections) {
            camera_->setProjectionMode(mode);

            controller_->setShowCameraFrustumOverlay(false);
            const QJsonObject baseline = measureOverlay();
            controller_->setShowCameraFrustumOverlay(true);
            const QJsonObject overlayed = measureOverlay();
            const QJsonObject geometry = verifyFrustum();

            const int baselineGreen =
                baseline.value(QStringLiteral("greenPixels")).toInt();
            const int overlayGreen =
                overlayed.value(QStringLiteral("greenPixels")).toInt();
            const bool overlayDrawn = overlayGreen >= baselineGreen + 30;
            const bool geometryPassed =
                geometry.value(QStringLiteral("passed")).toBool();
            const bool passed = overlayDrawn && geometryPassed;
            allPassed &= passed;

            cases.append(QJsonObject{
                {QStringLiteral("name"), QString::fromLatin1(name)},
                {QStringLiteral("passed"), passed},
                {QStringLiteral("overlayDrawn"), overlayDrawn},
                {QStringLiteral("baselineGreenPixels"), baselineGreen},
                {QStringLiteral("overlayGreenPixels"), overlayGreen},
                {QStringLiteral("coveragePercent"),
                    overlayed.value(QStringLiteral("coveragePercent"))},
                {QStringLiteral("geometryPassed"), geometryPassed},
                {QStringLiteral("geometry"), geometry}
            });
        }

        camera_->setProjectionMode(originalProjection);
        controller_->setShowCameraFrustumOverlay(originalOverlay);
        renderFrame();

        return {
            {QStringLiteral("passed"), allPassed},
            {QStringLiteral("cases"), cases},
            {QStringLiteral("restoredOverlay"), originalOverlay},
            {QStringLiteral("restoredProjection"),
                originalProjection == Artifact::ProjectionMode::Orthographic
                    ? QStringLiteral("orthographic") : QStringLiteral("perspective")}
        };
    }

    QJsonObject cullingStats()
    {
        if (!controller_)
            return {{QStringLiteral("error"), QStringLiteral("viewport is not initialized")}};
        renderFrame();
        const auto pumpEnd =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(60);
        while (std::chrono::steady_clock::now() < pumpEnd) {
            QCoreApplication::processEvents(
                QEventLoop::AllEvents | QEventLoop::ExcludeUserInputEvents, 5);
            QThread::msleep(5);
        }
        const auto stats = controller_->viewportCullingStats();
        return {
            {QStringLiteral("tested3DLayers"), stats.tested3DLayers},
            {QStringLiteral("culled3DLayers"), stats.culled3DLayers},
            {QStringLiteral("drawn3DLayers"), stats.drawn3DLayers}
        };
    }

    QJsonObject testCulling()
    {
        if (!controller_ || !probe_)
            return {{QStringLiteral("error"), QStringLiteral("viewport or probe is unavailable")}};

        const auto measureAt = [&](const float x, const float y, const float z) {
            probe_->transform3D().setCurrentPosition(x, y);
            probe_->transform3D().setCurrentPositionZ(z);
            renderFrame();
            const auto pumpEnd =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(80);
            while (std::chrono::steady_clock::now() < pumpEnd) {
                QCoreApplication::processEvents(
                    QEventLoop::AllEvents | QEventLoop::ExcludeUserInputEvents, 5);
                QThread::msleep(5);
            }
            const auto stats = controller_->viewportCullingStats();
            return QJsonObject{
                {QStringLiteral("tested3DLayers"), stats.tested3DLayers},
                {QStringLiteral("culled3DLayers"), stats.culled3DLayers},
                {QStringLiteral("drawn3DLayers"), stats.drawn3DLayers}
            };
        };

        const QJsonObject inView = measureAt(480.0f, 270.0f, 0.0f);
        const QJsonObject outOfView = measureAt(1000480.0f, 270.0f, 0.0f);

        probe_->transform3D().setCurrentPosition(480.0f, 270.0f);
        probe_->transform3D().setCurrentPositionZ(0.0f);
        renderFrame();

        const int inTested = inView.value(QStringLiteral("tested3DLayers")).toInt();
        const int inCulled = inView.value(QStringLiteral("culled3DLayers")).toInt();
        const int outTested = outOfView.value(QStringLiteral("tested3DLayers")).toInt();
        const int outCulled = outOfView.value(QStringLiteral("culled3DLayers")).toInt();
        const bool inViewPassed = inTested > 0 && inCulled == 0;
        const bool outOfViewPassed = outTested > 0 && outCulled == outTested;

        return {
            {QStringLiteral("passed"), inViewPassed && outOfViewPassed},
            {QStringLiteral("inView"), inView},
            {QStringLiteral("outOfView"), outOfView},
            {QStringLiteral("inViewPassed"), inViewPassed},
            {QStringLiteral("outOfViewPassed"), outOfViewPassed}
        };
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

        probe_ = ArtifactCore::makeShared<Artifact::Artifact3DLayer>(
            Artifact::FixedGeometry3D::Cube);
        probe_->setLayerName(QStringLiteral("Culling Probe Cube"));
        probe_->transform3D().setCurrentPosition(480.0f, 270.0f);
        probe_->transform3D().setCurrentPositionZ(0.0f);
        composition_->appendLayerTop(probe_);
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
    ArtifactCore::SharedPtr<Artifact::Artifact3DLayer> probe_;
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
        toolsReady &= mcpServer.registerTool(QStringLiteral("viewport.verifyFrustum"),
            QStringLiteral("Verify frustum corner geometry against camera clip planes"),
            objectSchema, [&window](const QJsonObject&) { return window.verifyFrustum(); });
        toolsReady &= mcpServer.registerTool(QStringLiteral("viewport.measureOverlay"),
            QStringLiteral("Measure overlay-colored pixels inside the viewport"),
            objectSchema, [&window](const QJsonObject&) { return window.measureOverlay(); });
        toolsReady &= mcpServer.registerTool(QStringLiteral("viewport.runChecks"),
            QStringLiteral("Run perspective and orthographic frustum drawing checks"),
            objectSchema, [&window](const QJsonObject&) { return window.runChecks(); });
        toolsReady &= mcpServer.registerTool(QStringLiteral("viewport.cullingStats"),
            QStringLiteral("Read the latest view-frustum culling decision counts"),
            objectSchema, [&window](const QJsonObject&) { return window.cullingStats(); });
        toolsReady &= mcpServer.registerTool(QStringLiteral("viewport.testCulling"),
            QStringLiteral("Move the probe cube in and out of view to verify frustum culling"),
            objectSchema, [&window](const QJsonObject&) { return window.testCulling(); });
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
