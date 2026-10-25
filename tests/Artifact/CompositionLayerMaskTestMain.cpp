#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QPointF>
#include <QScreen>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QTimerEvent>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <chrono>
#include <exception>
#include <future>
#include <memory>
#include <thread>
#include <utility>

#include "../ArtifactTestMcpServer.h"
#include "../ArtifactTestWindowCapture.h"

import Artifact.Application.Manager;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Layer.Solid2D;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Artifact.Widgets.CompositionRenderController;
import Color.Float;
import Composition3D;
import Memory.SharedPtr;
import Utils.Id;
import Utils.String.UniString;

namespace {

constexpr int kCompositionWidth = 320;
constexpr int kCompositionHeight = 240;

class CompositionLayerMaskTestWindow final : public QMainWindow {
public:
    CompositionLayerMaskTestWindow()
    {
        setWindowTitle(QStringLiteral("Artifact Composition Layer Mask Test"));
        resize(960, 720);

        auto* root = new QWidget(this);
        auto* layout = new QVBoxLayout(root);
        layout->addWidget(new QLabel(
            QStringLiteral("The left half of the red layer is covered by a rectangular mask. "
                           "Use the selector or MCP to enable, disable, or invert it."), root));
        modePicker_ = new QComboBox(root);
        modePicker_->addItem(QStringLiteral("Enabled"), QStringLiteral("enabled"));
        modePicker_->addItem(QStringLiteral("Disabled"), QStringLiteral("disabled"));
        modePicker_->addItem(QStringLiteral("Inverted"), QStringLiteral("inverted"));
        layout->addWidget(modePicker_);
        status_ = new QLabel(QStringLiteral("Preparing mask fixture…"), root);
        layout->addWidget(status_);
        viewportHost_ = new QWidget(root);
        viewportHost_->setAttribute(Qt::WA_NativeWindow, true);
        layout->addWidget(viewportHost_, 1);
        setCentralWidget(root);

        Artifact::ArtifactApplicationManager::instance();
        prepareFixture();
        lastPickerIndex_ = modePicker_->currentIndex();
        timerId_ = startTimer(50);
    }

    ~CompositionLayerMaskTestWindow() override
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
            fail(QStringLiteral("CompositionRenderController initialization failed"));
            return;
        }
        controller_->setComposition(composition_);
        controller_->setShowOutsideComposition(false);
        controller_->setPresentationLayout(
            Artifact::CompositionViewportPresentationLayout::Single);
        controller_->setShowOriginOverlay(false);
        controller_->setShowAnchorCenterOverlay(false);
        controller_->setShowGizmoOverlay(false);
        controller_->start();
        ready_ = true;
        applyMode(QStringLiteral("enabled"));
    }

    QJsonObject state() const
    {
        const Artifact::LayerMask currentMask = layer_
            ? layer_->mask(0) : Artifact::LayerMask{};
        const Artifact::MaskPath path = currentMask.maskPathCount() > 0
            ? currentMask.maskPath(0) : Artifact::MaskPath{};
        const QImage frame = presentedFrame_;
        const QColor inside = sample(frame, 80, 120);
        const QColor outside = sample(frame, 240, 120);
        return {{QStringLiteral("ready"), ready_},
                {QStringLiteral("mode"), mode_},
                {QStringLiteral("maskEnabled"), currentMask.isEnabled()},
                {QStringLiteral("pathInverted"), path.isInverted()},
                {QStringLiteral("compositionWidth"), kCompositionWidth},
                {QStringLiteral("compositionHeight"), kCompositionHeight},
                {QStringLiteral("renderFrameSerial"), controller_
                    ? static_cast<qint64>(controller_->currentFrameSerial()) : -1},
                {QStringLiteral("presentedFrameWidth"), frame.width()},
                {QStringLiteral("presentedFrameHeight"), frame.height()},
                {QStringLiteral("insideSampleRGBA"), pixelJson(inside)},
                {QStringLiteral("outsideSampleRGBA"), pixelJson(outside)},
                {QStringLiteral("error"), error_}};
    }

    QJsonObject setMode(const QString& requestedMode)
    {
        if (requestedMode != QStringLiteral("enabled") &&
            requestedMode != QStringLiteral("disabled") &&
            requestedMode != QStringLiteral("inverted")) {
            return {{QStringLiteral("error"), QStringLiteral(
                "mode must be enabled, disabled, or inverted")}};
        }
        if (!ready_ || !layer_ || !controller_)
            return {{QStringLiteral("error"), QStringLiteral("mask fixture is not ready")}};

        for (int index = 0; index < modePicker_->count(); ++index) {
            if (modePicker_->itemData(index).toString() == requestedMode) {
                modePicker_->setCurrentIndex(index);
                lastPickerIndex_ = index;
                break;
            }
        }
        applyMode(requestedMode);
        return state();
    }

    QJsonObject verify()
    {
        if (!ready_ || !controller_)
            return {{QStringLiteral("error"), QStringLiteral("mask fixture is not ready")}};
        presentedFrame_ = renderFrame();
        if (presentedFrame_.isNull())
            return {{QStringLiteral("error"), QStringLiteral("presented Composition frame is unavailable")}};
        const QImage repeatedFrame = renderFrame();
        if (repeatedFrame.isNull())
            return {{QStringLiteral("error"), QStringLiteral("repeat Composition frame is unavailable")}};
        const bool repeatCaptureMatches = presentedFrame_ == repeatedFrame;

        const QColor inside = sample(presentedFrame_, 80, 120);
        const QColor outside = sample(presentedFrame_, 240, 120);
        const bool insideVisible = inside.red() > 100;
        const bool outsideVisible = outside.red() > 100;
        const bool expectedInside = mode_ != QStringLiteral("inverted");
        const bool expectedOutside = mode_ != QStringLiteral("enabled");
        const bool passed = insideVisible == expectedInside &&
                            outsideVisible == expectedOutside &&
                            repeatCaptureMatches;
        QJsonObject result = state();
        result.insert(QStringLiteral("passed"), passed);
        result.insert(QStringLiteral("expectedInsideVisible"), expectedInside);
        result.insert(QStringLiteral("expectedOutsideVisible"), expectedOutside);
        result.insert(QStringLiteral("insideVisible"), insideVisible);
        result.insert(QStringLiteral("outsideVisible"), outsideVisible);
        result.insert(QStringLiteral("repeatCaptureMatches"), repeatCaptureMatches);
        status_->setText(QStringLiteral("%1 — inside=%2 outside=%3")
            .arg(passed ? QStringLiteral("PASS") : QStringLiteral("FAIL"))
            .arg(insideVisible ? QStringLiteral("visible") : QStringLiteral("masked"))
            .arg(outsideVisible ? QStringLiteral("visible") : QStringLiteral("masked")));
        return result;
    }

    QJsonObject capture()
    {
        QJsonObject verification = verify();
        if (verification.contains(QStringLiteral("error"))) return verification;

        struct CaptureResult {
            QImage image;
            QString error;
        };
        auto promise = std::make_shared<std::promise<CaptureResult>>();
        auto future = promise->get_future();
        std::thread worker;
        const auto nativeWindow = reinterpret_cast<ArtifactNativeWindowHandle>(winId());
        QTimer::singleShot(50, [promise, &worker, nativeWindow] {
            try {
                worker = std::thread([promise, nativeWindow] {
                    CaptureResult result;
                    result.image = artifactCaptureWindow(
                        nativeWindow, &result.error, 10000, false);
                    promise->set_value(std::move(result));
                });
            } catch (const std::exception& exception) {
                promise->set_value(CaptureResult{{}, QString::fromUtf8(exception.what())});
            } catch (...) {
                promise->set_value(CaptureResult{{},
                    QStringLiteral("Could not start window capture worker")});
            }
        });
        while (future.wait_for(std::chrono::milliseconds(0)) !=
               std::future_status::ready) {
            QCoreApplication::processEvents(
                QEventLoop::AllEvents | QEventLoop::ExcludeUserInputEvents, 5);
            QThread::msleep(1);
        }
        CaptureResult captureResult = future.get();
        if (worker.joinable()) worker.join();
        const QImage& captured = captureResult.image;
        if (captured.isNull()) {
            verification.insert(QStringLiteral("captureError"), captureResult.error);
            return verification;
        }

        const QString outputDirectory = QDir::cleanPath(qEnvironmentVariable(
            "ARTIFACT_COMPOSITION_LAYER_MASK_CAPTURE_DIR",
            QDir(QDir::tempPath()).filePath(
                QStringLiteral("ArtifactCompositionLayerMaskTest"))));
        if (!QDir().mkpath(outputDirectory)) {
            verification.insert(QStringLiteral("error"),
                QStringLiteral("Could not create capture output directory"));
            return verification;
        }
        const QString imagePath = QDir(outputDirectory).filePath(
            QStringLiteral("mask_%1.png").arg(mode_));
        if (!captured.save(imagePath, "PNG")) {
            verification.insert(QStringLiteral("error"),
                QStringLiteral("Could not save captured viewport PNG"));
            return verification;
        }
        verification.insert(QStringLiteral("capturePath"), imagePath);
        verification.insert(QStringLiteral("captureWidth"), captured.width());
        verification.insert(QStringLiteral("captureHeight"), captured.height());

        QFile diagnosticFile(QDir(outputDirectory).filePath(
            QStringLiteral("mask_%1_diagnostic.json").arg(mode_)));
        if (!diagnosticFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            verification.insert(QStringLiteral("diagnosticError"),
                diagnosticFile.errorString());
            return verification;
        }
        const QByteArray diagnostic = QJsonDocument(verification).toJson(
            QJsonDocument::Indented);
        if (diagnosticFile.write(diagnostic) != diagnostic.size()) {
            verification.insert(QStringLiteral("diagnosticError"),
                diagnosticFile.errorString());
            return verification;
        }
        verification.insert(QStringLiteral("diagnosticPath"), diagnosticFile.fileName());
        const QByteArray png = artifactEncodePng(captured);
        if (!png.isEmpty()) {
            verification.insert(QStringLiteral("mcpContent"), QJsonArray{QJsonObject{
                {QStringLiteral("type"), QStringLiteral("image")},
                {QStringLiteral("mimeType"), QStringLiteral("image/png")},
                {QStringLiteral("data"), QString::fromLatin1(png.toBase64())}}});
        }
        return verification;
    }

private:
    void prepareFixture()
    {
        Artifact::ArtifactCompositionInitParams params;
        params.setCompositionName(ArtifactCore::UniString(
            QStringLiteral("Composition Layer Mask Test")));
        params.setResolution(kCompositionWidth, kCompositionHeight);
        params.setFrameRate(30.0);
        params.setDurationFrames(2);
        params.setBackgroundColor(ArtifactCore::FloatColor(
            0.0f, 0.0f, 0.0f, 1.0f));
        composition_ = ArtifactCore::makeShared<Artifact::ArtifactComposition3D>(
            ArtifactCore::CompositionID(QStringLiteral("mask-test-composition")),
            params);
        if (!composition_) {
            fail(QStringLiteral("Composition fixture creation failed"));
            return;
        }

        layer_ = ArtifactCore::makeShared<Artifact::ArtifactSolid2DLayer>();
        if (!layer_) {
            fail(QStringLiteral("Solid layer fixture creation failed"));
            return;
        }
        layer_->setLayerName(QStringLiteral("Mask fixture solid"));
        layer_->setSize(kCompositionWidth, kCompositionHeight);
        layer_->setColor(ArtifactCore::FloatColor(0.95f, 0.12f, 0.08f, 1.0f));

        Artifact::MaskPath path;
        path.setClosed(true);
        path.addVertex({QPointF(0.0, 0.0), {}, {}});
        path.addVertex({QPointF(kCompositionWidth / 2.0, 0.0), {}, {}});
        path.addVertex({QPointF(kCompositionWidth / 2.0, kCompositionHeight), {}, {}});
        path.addVertex({QPointF(0.0, kCompositionHeight), {}, {}});
        Artifact::LayerMask mask;
        mask.addMaskPath(path);
        layer_->addMask(mask);
        if (!composition_->appendLayerTop(layer_).success) {
            fail(QStringLiteral("Could not append Solid layer to Composition"));
            return;
        }
    }

    void applyMode(const QString& mode)
    {
        if (!layer_ || !controller_) return;
        Artifact::LayerMask mask = layer_->mask(0);
        Artifact::MaskPath path = mask.maskPath(0);
        mask.setEnabled(mode != QStringLiteral("disabled"));
        path.setInverted(mode == QStringLiteral("inverted"));
        mask.setMaskPath(0, path);
        layer_->setMask(0, mask);
        mode_ = mode;
        presentedFrame_ = renderFrame();
        status_->setText(QStringLiteral("Mode: %1 — use mask.verify to check path samples")
            .arg(mode_));
    }

    QImage renderFrame()
    {
        if (!controller_ || !controller_->renderer()) return {};
        const quint64 previousPresent =
            controller_->renderer()->presentSuccessCount();
        controller_->markRenderDirty();
        controller_->renderOneFrame();
        QElapsedTimer timeout;
        timeout.start();
        while (controller_->renderer()->presentSuccessCount() <= previousPresent &&
               timeout.elapsed() < 5000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
        if (controller_->renderer()->presentSuccessCount() <= previousPresent)
            return {};
        QImage frame;
        while (frame.isNull() && timeout.elapsed() < 5000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            frame = controller_->readbackPresentedFrameImage();
        }
        return frame;
    }

    static QColor sample(const QImage& image, int x, int y)
    {
        if (image.isNull() || image.width() <= 0 || image.height() <= 0) return {};
        const int sampleX = std::clamp(
            x * image.width() / kCompositionWidth, 0, image.width() - 1);
        const int sampleY = std::clamp(
            y * image.height() / kCompositionHeight, 0, image.height() - 1);
        return image.pixelColor(sampleX, sampleY);
    }

    static QJsonArray pixelJson(const QColor& color)
    {
        return {color.red(), color.green(), color.blue(), color.alpha()};
    }

    void fail(const QString& message)
    {
        error_ = message;
        status_->setText(QStringLiteral("Setup failed: %1").arg(message));
    }

    void timerEvent(QTimerEvent* event) override
    {
        if (event && event->timerId() == timerId_) {
            const int index = modePicker_->currentIndex();
            if (index != lastPickerIndex_) {
                lastPickerIndex_ = index;
                applyMode(modePicker_->itemData(index).toString());
            }
            return;
        }
        QMainWindow::timerEvent(event);
    }

    bool ready_ = false;
    QString error_;
    QString mode_ = QStringLiteral("enabled");
    int timerId_ = 0;
    int lastPickerIndex_ = -1;
    QComboBox* modePicker_ = nullptr;
    QLabel* status_ = nullptr;
    QWidget* viewportHost_ = nullptr;
    Artifact::CompositionRenderController* controller_ = nullptr;
    Artifact::ArtifactCompositionPtr composition_;
    decltype(ArtifactCore::makeShared<Artifact::ArtifactSolid2DLayer>()) layer_;
    QImage presentedFrame_;
};

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    Artifact::ArtifactApplicationManager::instance();
    CompositionLayerMaskTestWindow window;
    bool mcpMode = false;
    for (int index = 1; index < argc; ++index)
        mcpMode |= QByteArray(argv[index]) == "--mcp-stdio";

    ArtifactTestMcpServer mcp(
        QStringLiteral("artifact-composition-layer-mask-test"),
        QStringLiteral("1.0.0"));
    bool toolsReady = true;
    if (mcpMode) {
        const QJsonObject objectSchema{{QStringLiteral("type"), QStringLiteral("object")}};
        toolsReady &= mcp.registerTool(QStringLiteral("mask.state"),
            QStringLiteral("Read the Composition mask fixture and pixel samples"),
            objectSchema, [&window](const QJsonObject&) { return window.state(); });
        toolsReady &= mcp.registerTool(QStringLiteral("mask.setMode"),
            QStringLiteral("Set the fixture mask to enabled, disabled, or inverted"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{{QStringLiteral("mode"),
                    QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}}},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("mode")}}},
            [&window](const QJsonObject& args) {
                return window.setMode(args.value(QStringLiteral("mode")).toString());
            });
        toolsReady &= mcp.registerTool(QStringLiteral("mask.verify"),
            QStringLiteral("Verify inside/outside layer pixels for the current mask mode"),
            objectSchema, [&window](const QJsonObject&) { return window.verify(); });
        toolsReady &= mcp.registerTool(QStringLiteral("mask.capture"),
            QStringLiteral("Capture the non-activated mask viewport and write PNG plus diagnostic JSON"),
            objectSchema, [&window](const QJsonObject&) { return window.capture(); });
        window.setAttribute(Qt::WA_ShowWithoutActivating, true);
        if (!toolsReady || !mcp.start()) return 2;
    }

    const QScreen* leftmost = app.primaryScreen();
    for (const QScreen* screen : app.screens()) {
        if (leftmost && screen->geometry().left() < leftmost->geometry().left())
            leftmost = screen;
    }
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
