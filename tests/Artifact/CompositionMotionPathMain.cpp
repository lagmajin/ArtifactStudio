#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <QApplication>
#include <QBuffer>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMouseEvent>
#include <QMainWindow>
#include <QScreen>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

// MSVC 19.51 emits two equivalent decorations for this STL helper across
// module-import and header-instantiation paths. Alias the imported spelling
// used by ArtifactCore to the spelling emitted by this translation unit.
#pragma comment(linker, "/alternatename:??$_Fill_vbool@V?$_Vb_iterator@U?$_Wrap_alloc@V?$allocator@I@std@@@std@@@std@@@std@@YAXV?$_Vb_iterator@U?$_Wrap_alloc@V?$allocator@I@std@@@std@@@0@0_N@Z=??$_Fill_vbool@V?$_Vb_iterator@U?$_Wrap_alloc@V?$allocator@I@std@@@std@@@std@@@std@@YAXV?$_Vb_iterator@U?$_Wrap_alloc@V?$allocator@I@std@@@std@@@0@V10@_N@Z")

#include "../ArtifactTestMcpServer.h"
#include "../ArtifactTestImageArtifacts.h"
#include "../ArtifactTestWindowCapture.h"

// VS 2026's STL routes std::fill(vector<bool>) through an out-of-line
// _Fill_vbool specialization. ArtifactCore's AudioBassTreble module sees the
// vector<bool> declaration through a module import, so provide that same
// specialization in this standalone test executable's link unit.
__declspec(noinline) void artifactProvideVectorBoolFillForMsvc()
{
    std::vector<bool> values(1);
    std::fill(values.begin(), values.end(), false);
}

import Artifact.Application.Manager;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Widgets.CompositionEditor;
import Artifact.Widgets.CompositionRenderController;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Render.IRenderer;
import Animation.Transform3D;
import Frame.Position;
import Time.Rational;
import Memory.SharedPtr;
import Undo.UndoManager;
import Utils.String.UniString;

namespace {

class MotionPathWindow final : public QMainWindow {
public:
    explicit MotionPathWindow(bool mcpMode) : mcpMode_(mcpMode)
    {
        setWindowTitle(QStringLiteral("Artifact Composition Motion Path Test"));
        resize(1100, 760);
        auto* central = new QWidget(this);
        auto* layout = new QVBoxLayout(central);
        status_ = new QLabel(QStringLiteral("Preparing deterministic Transform fixture…"), central);
        layout->addWidget(status_);
        editor_ = new Artifact::ArtifactCompositionEditor(central);
        layout->addWidget(editor_, 1);
        setCentralWidget(central);
        prepare();
        if (mcpMode_) setAttribute(Qt::WA_ShowWithoutActivating);
    }

    QJsonObject state() const
    {
        QJsonObject result{{QStringLiteral("ready"), ready_},
                           {QStringLiteral("frame"), frame_},
                           {QStringLiteral("layerId"), layerId_},
                           {QStringLiteral("controllerSelectedLayerId"),
                            controller() ? controller()->selectedLayerId().toString() : QString()},
                           {QStringLiteral("pathOverlayEnabled"), controller() && controller()->isShowMotionPathOverlay()}};
        if (auto* playback = Artifact::ArtifactPlaybackService::instance())
            result[QStringLiteral("playbackFrame")] = playback->currentFrame().framePosition();
        if (!error_.isEmpty()) result[QStringLiteral("error")] = error_;
        if (layer_) {
            const auto& transform = layer_->transform3D();
            const auto time = ArtifactCore::RationalTime(frame_, 30);
            result[QStringLiteral("evaluatedPosition")] = QJsonArray{transform.positionXAt(time), transform.positionYAt(time)};
            result[QStringLiteral("keyTimes")] = QJsonArray{0, 30, 60};
            result[QStringLiteral("keyValues")] = QJsonArray{QJsonArray{120, 180}, QJsonArray{320, 100}, QJsonArray{520, 180}};
        }
        return result;
    }

    QJsonObject seek(int frame)
    {
        if (!ready_ || frame < 0 || frame > 90) return fail(QStringLiteral("frame must be in [0,90] and fixture ready"));
        frame_ = frame;
        if (auto* playback = Artifact::ArtifactPlaybackService::instance();
            playback && playback->currentComposition() &&
            playback->currentComposition()->id() == composition_->id()) {
            playback->setCurrentFrame(ArtifactCore::FramePosition(frame));
        } else {
            composition_->setFramePosition(ArtifactCore::FramePosition(frame));
        }
        if (controller()) controller()->markRenderDirty();
        pumpEvents(250);
        // The standalone fixture is not hosted by the application's timeline
        // selection bridge. Reassert the fixture selection after queued editor
        // synchronization has run so Controller actions target this layer.
        if (controller()) controller()->setSelectedLayerId(layer_->id());
        return state();
    }

    QJsonObject setOverlay(bool enabled)
    {
        if (!controller()) return fail(QStringLiteral("composition controller unavailable"));
        controller()->setShowMotionPathOverlay(enabled);
        controller()->markRenderDirty();
        pumpEvents(200);
        return state();
    }

    QJsonObject editKeyAtCurrentFrame()
    {
        if (!layer_ || !composition_ || composition_->framePosition().framePosition() != frame_)
            return fail(QStringLiteral("composition cursor and fixture frame are out of sync"));
        if (!controller() || !controller()->setSelectedLayerMotionPathKeyframeAtCurrentFrame())
            return fail(QStringLiteral("VP Controller failed to set a Position key at the current frame"));
        const auto key = ArtifactCore::RationalTime(frame_, 30);
        const auto& transform = layer_->transform3D();
        if (!transform.hasPositionKeyFrameAt(key))
            return fail(QStringLiteral("Controller operation did not create a Transform Position key"));
        pumpEvents(200);
        return state();
    }

    QJsonObject runRegression()
    {
        if (!ready_ || !controller()) return fail(QStringLiteral("fixture is not ready"));
        auto* undo = Artifact::UndoManager::instance();
        if (!undo) return fail(QStringLiteral("Undo Manager unavailable"));
        undo->clearHistory();
        const int originalFrame = frame_;
        const bool originalOverlay = controller()->isShowMotionPathOverlay();
        auto restore = [this, originalFrame, originalOverlay]() {
            setOverlay(originalOverlay);
            seek(originalFrame);
        };
        setOverlay(true);
        QJsonArray samples;
        bool aligned = true;
        bool bezierShapeObserved = false;
        for (const int frame : {0, 15, 30, 45, 59, 60}) {
            seek(frame);
            const auto s = state();
            const auto p = s.value(QStringLiteral("evaluatedPosition")).toArray();
            const auto& transform = layer_->transform3D();
            const auto time = ArtifactCore::RationalTime(frame, 30);
            const double x = transform.positionXAt(time), y = transform.positionYAt(time);
            const bool finite = std::isfinite(x) && std::isfinite(y);
            bool endpointMatches = true;
            if (frame == 0) endpointMatches = std::abs(x - 120.0) < 0.01 && std::abs(y - 180.0) < 0.01;
            if (frame == 30) endpointMatches = std::abs(x - 320.0) < 0.01 && std::abs(y - 100.0) < 0.01;
            if (frame == 60) endpointMatches = std::abs(x - 520.0) < 0.01 && std::abs(y - 180.0) < 0.01;
            if (frame == 15)
                bezierShapeObserved |= std::abs(x - 220.0) > 0.1 || std::abs(y - 140.0) > 0.1;
            if (frame == 45)
                bezierShapeObserved |= std::abs(x - 420.0) > 0.1 || std::abs(y - 140.0) > 0.1;
            aligned &= finite && endpointMatches && p.size() == 2 &&
                       std::abs(p[0].toDouble() - x) < 0.001 &&
                       std::abs(p[1].toDouble() - y) < 0.001;
            samples.append(QJsonObject{{QStringLiteral("frame"), frame}, {QStringLiteral("position"), p}});
        }
        if (!aligned || !bezierShapeObserved) { restore(); return fail(QStringLiteral("Transform evaluation, Bezier trajectory, or frame snapshot disagrees with expected fixture values")); }
        const QJsonObject historicDrag = exerciseHistoricMotionPathDrag();
        if (historicDrag.contains(QStringLiteral("error"))) {
            restore();
            return historicDrag;
        }
        const double midBefore = layer_->transform3D().positionXAt(ArtifactCore::RationalTime(15, 30));
        seek(15);
        if (!editKeyAtCurrentFrame().value(QStringLiteral("error")).isUndefined()) {
            const int failedFrame = frame_;
            const auto compositionFrame = composition_->framePosition().framePosition();
            const auto playbackFrame = Artifact::ArtifactPlaybackService::instance()
                ? Artifact::ArtifactPlaybackService::instance()->currentFrame().framePosition() : -1;
            const QString selectedLayer = controller()->selectedLayerId().toString();
            restore();
            return fail(QStringLiteral("Controller key insertion failed at frame %1 (composition=%2, playback=%3, selected=%4, layer=%5)")
                .arg(failedFrame).arg(compositionFrame).arg(playbackFrame)
                .arg(selectedLayer, layerId_));
        }
        const auto midKeyTime = ArtifactCore::RationalTime(15, 30);
        if (!layer_->transform3D().hasPositionKeyFrameAt(midKeyTime) ||
            std::abs(layer_->transform3D().positionXAt(midKeyTime) - midBefore) > 0.01)
            { restore(); return fail(QStringLiteral("new VP key does not preserve the evaluated Position")); }
        if (!undo->canUndo()) { restore(); return fail(QStringLiteral("Undo Manager did not record the VP key insertion")); }
        undo->undo();
        if (layer_->transform3D().hasPositionKeyFrameAt(midKeyTime))
            { restore(); return fail(QStringLiteral("Undo did not remove the inserted Position key")); }
        undo->redo();
        if (!layer_->transform3D().hasPositionKeyFrameAt(midKeyTime))
            { restore(); return fail(QStringLiteral("Redo did not restore the inserted Position key")); }
        undo->undo();
        if (layer_->transform3D().hasPositionKeyFrameAt(midKeyTime))
            { restore(); return fail(QStringLiteral("test cleanup could not undo the inserted Position key")); }
        seek(45);
        QString captureSource;
        const QImage image = captureViewportImage(&captureSource);
        if (image.isNull()) { restore(); return fail(QStringLiteral("presented-frame capture failed (readback and native-window VP crop unavailable)")); }
        restore();
        return {{QStringLiteral("passed"), true},
                {QStringLiteral("checks"), QJsonArray{QStringLiteral("expected Position at key frames"), QStringLiteral("Bezier samples differ from straight-line interpolation"), QStringLiteral("odd-frame evaluation matches Transform snapshot"), QStringLiteral("historical Motion Path drag preserves current frame and evaluated Position"), QStringLiteral("historical key drag and Undo restoration"), QStringLiteral("VP Controller key insertion"), QStringLiteral("Undo/Redo key lifecycle"), QStringLiteral("presented VP frame available")} },
                {QStringLiteral("historicalDrag"), historicDrag},
                {QStringLiteral("samples"), samples}, {QStringLiteral("frameWidth"), image.width()}, {QStringLiteral("frameHeight"), image.height()}, {QStringLiteral("captureSource"), captureSource}};
    }

    QJsonObject capture(int timeoutMs)
    {
        if (timeoutMs < 1 || timeoutMs > 60000) return fail(QStringLiteral("timeoutMs must be in [1,60000]"));
        const QImage image = artifactCaptureWindow(reinterpret_cast<ArtifactNativeWindowHandle>(winId()), nullptr, timeoutMs);
        if (image.isNull()) return fail(QStringLiteral("window capture failed"));
        QByteArray png; QBuffer buffer(&png);
        if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) return fail(QStringLiteral("PNG encoding failed"));
        QJsonObject result = state(); result[QStringLiteral("mcpContent")] = QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("image")}, {QStringLiteral("mimeType"), QStringLiteral("image/png")}, {QStringLiteral("data"), QString::fromLatin1(png.toBase64())}}};
        return result;
    }

    QJsonObject captureOverlayArtifacts(int channelThreshold)
    {
        if (!ready_ || !controller() || channelThreshold < 0 || channelThreshold > 255)
            return fail(QStringLiteral("fixture/controller unavailable or threshold outside [0,255]"));
        const bool restoreOverlay = controller()->isShowMotionPathOverlay();
        QString withoutSource, withSource;
        setOverlay(false);
        const QImage withoutOverlay = captureViewportImage(&withoutSource);
        setOverlay(true);
        const QImage withOverlay = captureViewportImage(&withSource);
        setOverlay(restoreOverlay);
        if (withoutOverlay.isNull() || withOverlay.isNull())
            return fail(QStringLiteral("GPU frame readback failed for overlay pair"));
        const auto diff = ArtifactTestImageArtifacts::differenceMask(
            withoutOverlay, withOverlay, channelThreshold);
        if (diff.mask.isNull()) return fail(QStringLiteral("overlay image dimensions differ"));
        if (diff.changedPixels == 0 || diff.bounds.isEmpty())
            return fail(QStringLiteral("Motion Path overlay toggle produced no changed pixels"));
        const auto withoutContent = ArtifactTestImageArtifacts::imageContent(
            QStringLiteral("without-motion-path"), withoutOverlay);
        const auto withContent = ArtifactTestImageArtifacts::imageContent(
            QStringLiteral("with-motion-path"), withOverlay);
        const auto maskContent = ArtifactTestImageArtifacts::imageContent(
            QStringLiteral("motion-path-difference-mask"), diff.mask);
        if (withoutContent.isEmpty() || withContent.isEmpty() || maskContent.isEmpty())
            return fail(QStringLiteral("PNG encoding failed for overlay artifacts"));
        QJsonObject result = state();
        result[QStringLiteral("artifactKind")] = QStringLiteral("paired-state-overlay-difference");
        result[QStringLiteral("withoutOverlayCaptureSource")] = withoutSource;
        result[QStringLiteral("withOverlayCaptureSource")] = withSource;
        result[QStringLiteral("channelThreshold") ] = channelThreshold;
        result[QStringLiteral("changedPixels")] = static_cast<qint64>(diff.changedPixels);
        result[QStringLiteral("changedBounds")] = ArtifactTestImageArtifacts::regionMetadata(
            QStringLiteral("overlay-difference"), diff.bounds);
        result[QStringLiteral("mcpContent")] = QJsonArray{
            withoutContent, withContent, maskContent};
        return result;
    }

private:
    static QJsonObject fail(const QString& message) { return {{QStringLiteral("error"), message}}; }
    static void pumpEvents(int ms) { QElapsedTimer timer; timer.start(); while (timer.elapsed() < ms) { QCoreApplication::processEvents(QEventLoop::AllEvents, 20); QThread::msleep(5); } }
    Artifact::CompositionRenderController* controller() const { return editor_ ? editor_->renderController() : nullptr; }
    QWidget* compositionViewport() const
    {
        auto* host = editor_ ? editor_->viewportHostWidget() : nullptr;
        if (!host) return nullptr;
        const auto children = host->findChildren<QWidget*>();
        for (auto* child : children) {
            if (child->accessibleName() == QStringLiteral("Composition Editor") &&
                child->isVisible() && child->width() > 50 && child->height() > 50)
                return child;
        }
        return nullptr;
    }

    QJsonObject exerciseHistoricMotionPathDrag()
    {
        auto* playback = Artifact::ArtifactPlaybackService::instance();
        auto* viewport = compositionViewport();
        if (!controller() || !controller()->renderer() || !playback || !viewport)
            return fail(QStringLiteral("historical Motion Path drag prerequisites unavailable"));

        seek(45);
        const int currentFrame = playback->currentFrame().framePosition();
        const auto currentTime = ArtifactCore::RationalTime(currentFrame, 30);
        const double currentXBefore = layer_->transform3D().positionXAt(currentTime);
        const double currentYBefore = layer_->transform3D().positionYAt(currentTime);

        const auto keyTime = ArtifactCore::RationalTime(0, 30);
        const auto& transform = layer_->transform3D();
        const double keyXBefore = transform.positionXAt(keyTime);
        const QPointF keyAnchor = layer_->getGlobalTransformAt(0).map(
            QPointF(transform.anchorXAt(keyTime), transform.anchorYAt(keyTime)));
        const auto canvasPosition = controller()->renderer()->canvasToViewport(
            {static_cast<float>(keyAnchor.x()), static_cast<float>(keyAnchor.y())});
        const QPointF logicalPosition(
            static_cast<qreal>(canvasPosition.x),
            static_cast<qreal>(canvasPosition.y));

        QMouseEvent press(QEvent::MouseButtonPress, logicalPosition,
                          viewport->mapToGlobal(logicalPosition.toPoint()),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        controller()->handleMousePress(&press);
        if (!press.isAccepted()) {
            const auto canvasAtClick = controller()->renderer()->viewportToCanvas(
                {static_cast<float>(logicalPosition.x() * viewport->devicePixelRatioF()),
                 static_cast<float>(logicalPosition.y() * viewport->devicePixelRatioF())});
            controller()->handleMouseRelease();
            return fail(QStringLiteral("historical Motion Path key hit was not accepted (click=%1,%2 canvas=%3,%4 dpr=%5)")
                .arg(logicalPosition.x()).arg(logicalPosition.y())
                .arg(canvasAtClick.x).arg(canvasAtClick.y)
                .arg(viewport->devicePixelRatioF()));
        }
        const bool pressPreservedFrame =
            playback->currentFrame().framePosition() == currentFrame &&
            composition_->framePosition().framePosition() == currentFrame;
        if (!pressPreservedFrame) {
            controller()->handleMouseRelease();
            return fail(QStringLiteral("selecting a historical Motion Path key changed the current frame"));
        }

        const QPointF dragPosition = logicalPosition + QPointF(18.0, 0.0);
        controller()->handleMouseMove(
            ArtifactCore::Coordinates::screenLogicalPointFromQPointF(dragPosition));
        controller()->handleMouseRelease();
        pumpEvents(100);

        const double draggedKeyX = layer_->transform3D().positionXAt(keyTime);
        const double currentXAfter = layer_->transform3D().positionXAt(currentTime);
        const double currentYAfter = layer_->transform3D().positionYAt(currentTime);
        const bool framePreserved =
            playback->currentFrame().framePosition() == currentFrame &&
            composition_->framePosition().framePosition() == currentFrame;
        const bool keyMoved = std::abs(draggedKeyX - keyXBefore) > 0.01;
        const bool currentPositionPreserved =
            std::abs(currentXAfter - currentXBefore) < 0.01 &&
            std::abs(currentYAfter - currentYBefore) < 0.01;
        if (!framePreserved || !keyMoved || !currentPositionPreserved) {
            auto* undo = Artifact::UndoManager::instance();
            if (undo && undo->canUndo()) undo->undo();
            return fail(QStringLiteral("historical drag regression: framePreserved=%1, keyMoved=%2, currentPositionPreserved=%3")
                .arg(framePreserved).arg(keyMoved).arg(currentPositionPreserved));
        }
        auto* undo = Artifact::UndoManager::instance();
        if (!undo || !undo->canUndo())
            return fail(QStringLiteral("historical key drag was not recorded for Undo"));
        undo->undo();
        const bool undoRestoredKey =
            std::abs(layer_->transform3D().positionXAt(keyTime) - 120.0) < 0.01 &&
            std::abs(layer_->transform3D().positionYAt(keyTime) - 180.0) < 0.01;
        if (!undoRestoredKey)
            return fail(QStringLiteral("Undo did not restore the historical Motion Path key"));
        return {{QStringLiteral("currentFrame"), currentFrame},
                {QStringLiteral("draggedKeyFrame"), 0},
                {QStringLiteral("draggedKeyX"), draggedKeyX},
                {QStringLiteral("currentPosition"), QJsonArray{currentXAfter, currentYAfter}},
                {QStringLiteral("undoRestoredKey"), undoRestoredKey}};
    }

    QImage captureViewportImage(QString* source = nullptr) const
    {
        if (!controller()) return {};
        QImage image = controller()->readbackPresentedFrameImage();
        if (!image.isNull()) {
            if (source) *source = QStringLiteral("gpu-composition-readback");
            return image;
        }
        auto* viewportHost = editor_ ? editor_->viewportHostWidget() : nullptr;
        if (!viewportHost) return {};
        QWidget* viewport = compositionViewport();
        if (!viewport) return {};
        QString captureError;
        const QImage windowImage = artifactCaptureWindow(
            reinterpret_cast<ArtifactNativeWindowHandle>(winId()),
            &captureError, 3000, false);
        if (windowImage.isNull()) return {};
        const QPoint hostOrigin = viewport->mapTo(this, QPoint(0, 0));
        const qreal scaleX = static_cast<qreal>(windowImage.width()) / width();
        const qreal scaleY = static_cast<qreal>(windowImage.height()) / height();
        const QRect crop(qRound(hostOrigin.x() * scaleX),
                         qRound(hostOrigin.y() * scaleY),
                         qRound(viewport->width() * scaleX),
                         qRound(viewport->height() * scaleY));
        image = windowImage.copy(crop.intersected(windowImage.rect()));
        if (!image.isNull() && source)
            *source = QStringLiteral("native-window-capture-vp-crop");
        return image;
    }

    void prepare()
    {
        auto* project = Artifact::ArtifactProjectService::instance();
        if (!project) { error_ = QStringLiteral("project service unavailable"); return; }
        Artifact::ArtifactCompositionInitParams params;
        params.setCompositionName(ArtifactCore::UniString(QStringLiteral("Motion Path Transform Fixture")));
        params.setResolution(640, 360); params.setFrameRate(30.0); params.setDurationFrames(90);
        project->createComposition(params); composition_ = project->currentComposition().lock();
        if (!composition_) { error_ = QStringLiteral("composition creation failed"); return; }
        if (auto* playback = Artifact::ArtifactPlaybackService::instance())
            playback->setCurrentComposition(composition_);
        QImage pixels(100, 100, QImage::Format_RGBA8888); pixels.fill(Qt::transparent);
        for (int y = 10; y < 90; ++y) for (int x = 10; x < 90; ++x) {
            auto* px = pixels.scanLine(y) + x * 4;
            px[0] = 245; px[1] = static_cast<uchar>(90 + x); px[2] = 45; px[3] = 255;
        }
        layer_ = ArtifactCore::makeShared<Artifact::ArtifactImageLayer>();
        layer_->setFromQImage(pixels); layer_->setLayerName(QStringLiteral("Animated Position Target"));
        auto& transform = layer_->transform3D();
        transform.setKeyframeTimeScale(30);
        transform.setPositionKeyFrameValueAt(ArtifactCore::RationalTime(0, 30), 120.0f, 180.0f);
        transform.setPositionKeyFrameValueAt(ArtifactCore::RationalTime(30, 30), 320.0f, 100.0f);
        transform.setPositionKeyFrameValueAt(ArtifactCore::RationalTime(60, 30), 520.0f, 180.0f);
        for (const auto frame : {30, 60}) transform.setPositionKeyFrameInterpolationAt(ArtifactCore::RationalTime(frame, 30), ArtifactCore::InterpolationType::Bezier, ArtifactCore::InterpolationType::Bezier);
        ArtifactCore::PositionSpatialTangents tangent; tangent.inTangent = {-80.0f, 0.0f}; tangent.outTangent = {80.0f, -80.0f}; tangent.linked = false;
        transform.setPositionKeyFrameSpatialTangentsAt(ArtifactCore::RationalTime(30, 30), tangent);
        composition_->appendLayerTop(layer_); layerId_ = layer_->id().toString();
        editor_->setComposition(composition_);
        if (controller()) { controller()->setSelectedLayerId(layer_->id()); controller()->setShowMotionPathOverlay(true); }
        ready_ = true;
        status_->setText(QStringLiteral("Fixture ready — motionpath.state / motionpath.seek / motionpath.runRegression / motionpath.capture / motionpath.captureOverlayArtifacts"));
    }

    bool mcpMode_ = false, ready_ = false;
    int frame_ = 0;
    QString error_, layerId_;
    QLabel* status_ = nullptr;
    Artifact::ArtifactCompositionEditor* editor_ = nullptr;
    Artifact::ArtifactCompositionPtr composition_;
    ArtifactCore::SharedPtr<Artifact::ArtifactImageLayer> layer_;
};
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv); bool mcpMode = false;
    for (int i = 1; i < argc; ++i) mcpMode |= QByteArray(argv[i]) == "--mcp-stdio";
    Artifact::ArtifactApplicationManager::instance(); MotionPathWindow window(mcpMode);
    ArtifactTestMcpServer mcp(QStringLiteral("artifact-composition-motion-path-test"), QStringLiteral("1.0.0"));
    if (mcpMode) {
        const QJsonObject objectSchema{{QStringLiteral("type"), QStringLiteral("object")}};
        bool ok = true;
        ok &= mcp.registerTool(QStringLiteral("motionpath.state"), QStringLiteral("Read fixture state and evaluated Transform Position"), objectSchema, [&window](const auto&) { return window.state(); });
        ok &= mcp.registerTool(QStringLiteral("motionpath.seek"), QStringLiteral("Evaluate Transform at a composition frame"), QJsonObject{{QStringLiteral("type"), QStringLiteral("object")}, {QStringLiteral("properties"), QJsonObject{{QStringLiteral("frame"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}}}}}, {QStringLiteral("required"), QJsonArray{QStringLiteral("frame")}}}, [&window](const auto& a) { return window.seek(a.value(QStringLiteral("frame")).toInt(-1)); });
        ok &= mcp.registerTool(QStringLiteral("motionpath.runRegression"), QStringLiteral("Run the independent motion-path and Transform integration assertions"), objectSchema, [&window](const auto&) { return window.runRegression(); });
        ok &= mcp.registerTool(QStringLiteral("motionpath.capture"), QStringLiteral("Capture the independent VP test window"), objectSchema, [&window](const auto&) { return window.capture(5000); });
        ok &= mcp.registerTool(QStringLiteral("motionpath.captureOverlayArtifacts"), QStringLiteral("Return composition-only images with/without Motion Path and a semantic difference mask"), QJsonObject{{QStringLiteral("type"), QStringLiteral("object")}, {QStringLiteral("properties"), QJsonObject{{QStringLiteral("channelThreshold"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}}}}}}, [&window](const auto& a) { return window.captureOverlayArtifacts(a.value(QStringLiteral("channelThreshold")).toInt(4)); });
        if (!ok || !mcp.start()) return 2;
        window.setAttribute(Qt::WA_ShowWithoutActivating);
    }
    const QScreen* leftmost = app.primaryScreen();
    for (const auto* screen : app.screens()) if (leftmost && screen->geometry().left() < leftmost->geometry().left()) leftmost = screen;
    if (leftmost) { window.resize(window.size().boundedTo(leftmost->availableGeometry().size())); window.move(leftmost->availableGeometry().topLeft()); }
    window.show();
    return app.exec();
}
