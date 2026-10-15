#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <QApplication>
#include <QCoreApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QEventLoop>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMimeData>
#include <QRect>
#include <QStatusBar>
#include <QScreen>
#include <QSizePolicy>
#include <QThread>
#include <QTimer>
#include <QtLogging>
#include <QUrl>
#include <QVector3D>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>
#include <cstdio>
#include <limits>
#include <algorithm>
#include <chrono>
#include <exception>
#include <future>
#include <thread>
#include <utility>

#include "../ArtifactTestMcpServer.h"
#include "../ArtifactTestWindowCapture.h"

import Artifact.Widgets.ModelViewer;
import Utils.String.UniString;

namespace {

class FbxAnimationViewerWindow final : public QMainWindow {
public:
    FbxAnimationViewerWindow()
    {
        setWindowTitle(QStringLiteral("Artifact FBX Animation Viewer"));
        resize(1280, 800);
        setAcceptDrops(true);

        auto* central = new QWidget(this);
        auto* layout = new QVBoxLayout(central);
        auto* instructions = new QLabel(
            QStringLiteral("Open: O    Play/Pause: Space    Next clip: C    "
                           "Reset view: R    Or drop an FBX file here"), central);
        layout->addWidget(instructions);
        viewer_ = new Artifact::Artifact3DModelViewer(central);
        if (auto* status = viewer_->findChild<QLabel*>(
                QStringLiteral("Artifact3DViewerStatus"))) {
            // Full paths remain available in MCP state. Their text must not
            // enlarge the test window across the neighboring display.
            status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        }
        qApp->installEventFilter(this);
        layout->addWidget(viewer_, 1);
        setCentralWidget(central);
        statusBar()->showMessage(QStringLiteral("Waiting for an FBX model"));
    }

    ~FbxAnimationViewerWindow() override
    {
        qApp->removeEventFilter(this);
    }

    void loadModel(const QString& path, const int initialClipIndex = 0)
    {
        if (path.isEmpty()) return;
        hasRequestedSampleTime_ = false;
        currentModelPath_ = path;
        viewer_->loadModel(ArtifactCore::UniString(path));
        if (initialClipIndex > 0) {
            viewer_->setAnimationClipIndex(initialClipIndex);
        }
        viewer_->setAnimationPlaybackEnabled(viewer_->animationClipCount() > 0);
        const QFileInfo info(path);
        if (!viewer_->hasModel()) {
            statusBar()->showMessage(QStringLiteral("Could not load %1: %2")
                .arg(info.fileName(), viewer_->lastErrorText()));
            return;
        }
        const QString clip = viewer_->animationClipCount() > 0
            ? viewer_->animationClipName(viewer_->animationClipIndex())
            : QStringLiteral("no animation clips found");
        statusBar()->showMessage(QStringLiteral("%1 | %2 clip(s) | %3")
            .arg(info.fileName())
            .arg(viewer_->animationClipCount())
            .arg(clip));
    }

    QJsonObject viewerState() const
    {
        QString displayMode = QStringLiteral("solid");
        if (viewer_->displayMode() == Artifact::Artifact3DModelViewer::DisplayMode::Wireframe)
            displayMode = QStringLiteral("wireframe");
        else if (viewer_->displayMode() ==
                 Artifact::Artifact3DModelViewer::DisplayMode::SolidWithWire)
            displayMode = QStringLiteral("solidWithWire");

        QJsonArray clips;
        for (int index = 0; index < viewer_->animationClipCount(); ++index) {
            clips.append(QJsonObject{
                {QStringLiteral("index"), index},
                {QStringLiteral("name"), viewer_->animationClipName(index)}});
        }
        const QVector3D extents = viewer_->meshExtents();
        const QLabel* previewStatus = viewer_->findChild<QLabel*>(
            QStringLiteral("Artifact3DViewerStatus"));
        const QScreen* currentScreen = screen();
        const QRect screenGeometry = currentScreen ? currentScreen->geometry() : QRect();
        bool onLeftmostDisplay = currentScreen != nullptr;
        for (const QScreen* candidate : QGuiApplication::screens()) {
            onLeftmostDisplay = onLeftmostDisplay &&
                candidate->geometry().left() >= screenGeometry.left();
        }
        return {
            {QStringLiteral("modelPath"), currentModelPath_},
            {QStringLiteral("loaded"), viewer_->hasModel()},
            {QStringLiteral("error"), viewer_->lastErrorText()},
            {QStringLiteral("previewStatus"), previewStatus
                ? previewStatus->text() : QString()},
            {QStringLiteral("vertices"), viewer_->vertexCount()},
            {QStringLiteral("polygons"), viewer_->polygonCount()},
            {QStringLiteral("extents"), QJsonArray{extents.x(), extents.y(), extents.z()}},
            {QStringLiteral("backend"), viewer_->backendName()},
            {QStringLiteral("clipIndex"), viewer_->animationClipIndex()},
            {QStringLiteral("clips"), clips},
            {QStringLiteral("requestedSampleTimeSeconds"), hasRequestedSampleTime_
                ? QJsonValue(requestedSampleTimeSeconds_) : QJsonValue(QJsonValue::Null)},
            {QStringLiteral("playing"), viewer_->animationPlaybackEnabled()},
            {QStringLiteral("displayMode"), displayMode},
            {QStringLiteral("window"), QJsonObject{
                {QStringLiteral("x"), frameGeometry().x()},
                {QStringLiteral("y"), frameGeometry().y()},
                {QStringLiteral("width"), frameGeometry().width()},
                {QStringLiteral("height"), frameGeometry().height()},
                {QStringLiteral("screenName"), currentScreen ? currentScreen->name() : QString()},
                {QStringLiteral("onLeftmostDisplay"), onLeftmostDisplay},
                {QStringLiteral("withinScreenWorkArea"), currentScreen &&
                    currentScreen->availableGeometry().contains(frameGeometry())}}},
            {QStringLiteral("camera"), QJsonObject{
                {QStringLiteral("yaw"), viewer_->cameraYaw()},
                {QStringLiteral("pitch"), viewer_->cameraPitch()},
                {QStringLiteral("zoom"), viewer_->zoomFactor()}}}
        };
    }

    void setPlayback(bool playing)
    {
        if (playing) hasRequestedSampleTime_ = false;
        viewer_->setAnimationPlaybackEnabled(playing);
        statusBar()->showMessage(playing ? QStringLiteral("Animation playing")
                                          : QStringLiteral("Animation paused"));
    }

    bool seekTime(const double timeSeconds)
    {
        if (!viewer_->hasModel() || viewer_->animationClipCount() <= 0 ||
            !std::isfinite(timeSeconds)) {
            return false;
        }
        hasRequestedSampleTime_ = false;
        viewer_->setAnimationPlaybackEnabled(false);
        viewer_->loadModelAtTime(
            ArtifactCore::UniString(currentModelPath_), timeSeconds,
            viewer_->animationClipIndex());
        viewer_->setAnimationPlaybackEnabled(false);
        statusBar()->showMessage(
            QStringLiteral("Sampled animation at %1 seconds")
                .arg(timeSeconds, 0, 'f', 3));
        return viewer_->hasModel();
    }

    void setRequestedSampleTime(const double timeSeconds)
    {
        requestedSampleTimeSeconds_ = timeSeconds;
        hasRequestedSampleTime_ = true;
    }

    bool selectClip(int index)
    {
        if (index < 0 || index >= viewer_->animationClipCount()) return false;
        hasRequestedSampleTime_ = false;
        viewer_->setAnimationClipIndex(index);
        statusBar()->showMessage(viewer_->animationClipName(index));
        return true;
    }

    void setDisplayMode(Artifact::Artifact3DModelViewer::DisplayMode mode)
    {
        viewer_->setDisplayMode(mode);
    }

    void resetView()
    {
        viewer_->resetView();
    }

    void setCamera(float yaw, float pitch, float zoom)
    {
        viewer_->setCameraRotation(yaw, pitch);
        viewer_->setZoom(zoom);
    }

    void placeOnLeftmostDisplay()
    {
        const auto screens = QGuiApplication::screens();
        if (screens.isEmpty()) return;
        const auto screen = *std::min_element(
            screens.cbegin(), screens.cend(), [](const QScreen* left,
                                                  const QScreen* right) {
                return left->geometry().left() < right->geometry().left();
            });
        if (!screen) return;
        const QRect available = screen->availableGeometry();
        resize(std::min(width(), available.width()),
               std::min(height(), available.height()));
        move(available.topLeft());
    }

    void captureFrames(const QString& outputDirectory, int count)
    {
        if (outputDirectory.isEmpty() || count <= 0) {
            return;
        }
        if (!viewer_->hasModel()) {
            qWarning("Cannot capture FBX frames because no model is loaded");
            return;
        }
        if (!QDir().mkpath(outputDirectory)) {
            qWarning("Could not create FBX frame capture directory");
            return;
        }

        for (int frame = 0; frame < count; ++frame) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            QThread::msleep(500);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            QString captureError;
#ifdef _WIN32
            const QImage image = artifactCaptureWindow(
                reinterpret_cast<HWND>(winId()), &captureError);
#else
            const QImage image = artifactCaptureWindow(nullptr, &captureError);
#endif
            if (image.isNull()) {
                qWarning().noquote() << "Could not capture FBX viewer window:" << captureError;
                return;
            }
            const QString path = QDir(outputDirectory).filePath(
                QStringLiteral("frame_%1.png").arg(frame, 2, 10, QLatin1Char('0')));
            if (!image.save(path)) {
                qWarning("Could not save FBX frame capture");
                return;
            }
            qInfo().noquote() << "Captured" << path;
        }
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        auto* keyEvent = event->type() == QEvent::KeyPress
            ? static_cast<QKeyEvent*>(event) : nullptr;
        const auto* watchedWidget = qobject_cast<QWidget*>(watched);
        const bool belongsToViewer = watched == viewer_ ||
            (watchedWidget && viewer_->isAncestorOf(watchedWidget));
        if (belongsToViewer && keyEvent &&
            (keyEvent->key() == Qt::Key_O || keyEvent->key() == Qt::Key_Space ||
             keyEvent->key() == Qt::Key_C || keyEvent->key() == Qt::Key_R)) {
            keyPressEvent(keyEvent);
            return true;
        }
        return false;
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_O) {
            const QString path = QFileDialog::getOpenFileName(
                this, QStringLiteral("Open animated FBX model"), QString(),
                QStringLiteral("FBX files (*.fbx);;All files (*.*)"));
            loadModel(path);
            return;
        }
        if (event->key() == Qt::Key_Space) {
            setPlayback(!viewer_->animationPlaybackEnabled());
            return;
        }
        if (event->key() == Qt::Key_C && viewer_->animationClipCount() > 0) {
            const int next = (viewer_->animationClipIndex() + 1) %
                             viewer_->animationClipCount();
            viewer_->setAnimationClipIndex(next);
            statusBar()->showMessage(viewer_->animationClipName(next));
            return;
        }
        if (event->key() == Qt::Key_R) {
            viewer_->resetView();
            return;
        }
        QMainWindow::keyPressEvent(event);
    }

    void dragEnterEvent(QDragEnterEvent* event) override
    {
        if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    }

    void dropEvent(QDropEvent* event) override
    {
        const auto urls = event->mimeData()->urls();
        if (!urls.isEmpty() && urls.front().isLocalFile()) {
            loadModel(urls.front().toLocalFile());
            event->acceptProposedAction();
        }
    }

private:
    Artifact::Artifact3DModelViewer* viewer_ = nullptr;
    QString currentModelPath_;
    double requestedSampleTimeSeconds_ = 0.0;
    bool hasRequestedSampleTime_ = false;
};

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    // A Windows GUI subsystem binary may send Qt messages to the debugger
    // instead of the redirected stderr pipe. Keep this standalone test's
    // diagnostics observable and its MCP stdout reserved for JSON-RPC.
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext&,
                             const QString& message) {
        const QByteArray utf8 = message.toUtf8();
        std::fwrite(utf8.constData(), 1, static_cast<size_t>(utf8.size()), stderr);
        std::fputc('\n', stderr);
        if (type == QtFatalMsg) std::fflush(stderr);
    });
    QString modelPath;
    QString captureDirectory;
    bool alwaysOnTop = false;
    bool mcpStdio = false;
    int initialClipIndex = 0;
    for (int index = 1; index < argc; ++index) {
        const QString argument = QString::fromLocal8Bit(argv[index]);
        if (argument == QStringLiteral("--capture-frames") &&
            index + 1 < argc) {
            captureDirectory = QString::fromLocal8Bit(argv[++index]);
        } else if (argument == QStringLiteral("--always-on-top")) {
            alwaysOnTop = true;
        } else if (argument == QStringLiteral("--mcp-stdio")) {
            mcpStdio = true;
        } else if (argument == QStringLiteral("--clip-index") &&
                   index + 1 < argc) {
            bool validIndex = false;
            const int parsedIndex = QString::fromLocal8Bit(argv[++index])
                .toInt(&validIndex);
            if (validIndex && parsedIndex >= 0) {
                initialClipIndex = parsedIndex;
            }
        } else if (!argument.startsWith(QLatin1Char('-')) &&
                   modelPath.isEmpty()) {
            modelPath = argument;
        }
    }

    FbxAnimationViewerWindow window;
    ArtifactTestMcpServer mcpServer(
        QStringLiteral("artifact-fbx-animation-viewer"),
        QStringLiteral("1.0.0"));
    if (mcpStdio) {
        bool toolsRegistered = true;
        toolsRegistered &= mcpServer.registerTool(QStringLiteral("viewer.state"),
            QStringLiteral("Read model, animation, import backend, window placement, and camera state"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")}},
            [&window](const QJsonObject&) { return window.viewerState(); });
        toolsRegistered &= mcpServer.registerTool(QStringLiteral("viewer.loadModel"),
            QStringLiteral("Load an FBX by path without mouse or keyboard input"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                        {QStringLiteral("properties"), QJsonObject{
                            {QStringLiteral("path"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}}},
                        {QStringLiteral("required"), QJsonArray{QStringLiteral("path")}}},
            [&window](const QJsonObject& arguments) {
                const QString path = arguments.value(QStringLiteral("path")).toString();
                if (path.isEmpty()) return QJsonObject{{QStringLiteral("error"), QStringLiteral("path is required")}};
                window.loadModel(path);
                return window.viewerState();
            });
        toolsRegistered &= mcpServer.registerTool(QStringLiteral("viewer.setPlayback"),
            QStringLiteral("Set animation playback state"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                        {QStringLiteral("properties"), QJsonObject{
                            {QStringLiteral("playing"), QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}}}},
                        {QStringLiteral("required"), QJsonArray{QStringLiteral("playing")}}},
            [&window](const QJsonObject& arguments) {
                if (!arguments.value(QStringLiteral("playing")).isBool())
                    return QJsonObject{{QStringLiteral("error"), QStringLiteral("playing must be boolean")}};
                window.setPlayback(arguments.value(QStringLiteral("playing")).toBool());
                return window.viewerState();
            });
        toolsRegistered &= mcpServer.registerTool(QStringLiteral("viewer.seekTime"),
            QStringLiteral("Evaluate the selected animation at an absolute time in seconds and pause"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                        {QStringLiteral("properties"), QJsonObject{
                            {QStringLiteral("timeSeconds"), QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}}}}},
                        {QStringLiteral("required"), QJsonArray{QStringLiteral("timeSeconds")}}},
            [&window](const QJsonObject& arguments) {
                const QJsonValue timeValue = arguments.value(
                    QStringLiteral("timeSeconds"));
                if (!timeValue.isDouble() ||
                    !std::isfinite(timeValue.toDouble())) {
                    return QJsonObject{{QStringLiteral("error"),
                        QStringLiteral("timeSeconds must be a finite number")}};
                }
                const double requestedTime = timeValue.toDouble();
                if (!window.seekTime(requestedTime)) {
                    return QJsonObject{{QStringLiteral("error"),
                        QStringLiteral("Could not sample the selected animation clip")}};
                }
                window.setRequestedSampleTime(requestedTime);
                return window.viewerState();
            });
        toolsRegistered &= mcpServer.registerTool(QStringLiteral("viewer.selectClip"),
            QStringLiteral("Select an animation clip by index"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                        {QStringLiteral("properties"), QJsonObject{
                            {QStringLiteral("index"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}}}}},
                        {QStringLiteral("required"), QJsonArray{QStringLiteral("index")}}},
            [&window](const QJsonObject& arguments) {
                const QJsonValue indexValue = arguments.value(QStringLiteral("index"));
                if (!indexValue.isDouble())
                    return QJsonObject{{QStringLiteral("error"), QStringLiteral("index must be an integer")}};
                const double requestedIndex = indexValue.toDouble();
                if (!std::isfinite(requestedIndex) ||
                    std::trunc(requestedIndex) != requestedIndex ||
                    requestedIndex < 0.0 ||
                    requestedIndex > std::numeric_limits<int>::max()) {
                    return QJsonObject{{QStringLiteral("error"), QStringLiteral("index must be an integer in range")}};
                }
                const int index = static_cast<int>(requestedIndex);
                if (!window.selectClip(index))
                    return QJsonObject{{QStringLiteral("error"), QStringLiteral("clip index is out of range")}};
                return window.viewerState();
            });
        toolsRegistered &= mcpServer.registerTool(QStringLiteral("viewer.setDisplayMode"),
            QStringLiteral("Set solid, wireframe, or solidWithWire viewport shading"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                        {QStringLiteral("properties"), QJsonObject{
                            {QStringLiteral("mode"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                {QStringLiteral("enum"), QJsonArray{QStringLiteral("solid"), QStringLiteral("wireframe"), QStringLiteral("solidWithWire")}}}}}},
                        {QStringLiteral("required"), QJsonArray{QStringLiteral("mode")}}},
            [&window](const QJsonObject& arguments) {
                const QString mode = arguments.value(QStringLiteral("mode")).toString();
                using Mode = Artifact::Artifact3DModelViewer::DisplayMode;
                if (mode == QStringLiteral("solid")) window.setDisplayMode(Mode::Solid);
                else if (mode == QStringLiteral("wireframe")) window.setDisplayMode(Mode::Wireframe);
                else if (mode == QStringLiteral("solidWithWire")) window.setDisplayMode(Mode::SolidWithWire);
                else return QJsonObject{{QStringLiteral("error"), QStringLiteral("unknown display mode")}};
                return window.viewerState();
            });
        toolsRegistered &= mcpServer.registerTool(QStringLiteral("viewer.resetView"),
            QStringLiteral("Reset the viewport camera"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")}},
            [&window](const QJsonObject&) {
                window.resetView();
                return window.viewerState();
            });
        toolsRegistered &= mcpServer.registerTool(QStringLiteral("viewer.setCamera"),
            QStringLiteral("Set orbit yaw/pitch in degrees and zoom without input focus"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("yaw"), QJsonObject{{QStringLiteral("type"), QStringLiteral("number")},
                        {QStringLiteral("minimum"), -360}, {QStringLiteral("maximum"), 360}}},
                    {QStringLiteral("pitch"), QJsonObject{{QStringLiteral("type"), QStringLiteral("number")},
                        {QStringLiteral("minimum"), -89}, {QStringLiteral("maximum"), 89}}},
                    {QStringLiteral("zoom"), QJsonObject{{QStringLiteral("type"), QStringLiteral("number")},
                        {QStringLiteral("minimum"), 0.05}, {QStringLiteral("maximum"), 100}}}}},
                {QStringLiteral("required"), QJsonArray{
                    QStringLiteral("yaw"), QStringLiteral("pitch"), QStringLiteral("zoom")}}},
            [&window](const QJsonObject& arguments) {
                const auto valid = [&arguments](const char* name, double minimum, double maximum) {
                    const QJsonValue value = arguments.value(QLatin1String(name));
                    return value.isDouble() && std::isfinite(value.toDouble()) &&
                        value.toDouble() >= minimum && value.toDouble() <= maximum;
                };
                if (!valid("yaw", -360, 360) || !valid("pitch", -89, 89) ||
                    !valid("zoom", 0.05, 100)) {
                    return QJsonObject{{QStringLiteral("error"), QStringLiteral(
                        "yaw [-360,360], pitch [-89,89], and zoom [0.05,100] must be finite numbers")}};
                }
                window.setCamera(static_cast<float>(arguments.value(QStringLiteral("yaw")).toDouble()),
                    static_cast<float>(arguments.value(QStringLiteral("pitch")).toDouble()),
                    static_cast<float>(arguments.value(QStringLiteral("zoom")).toDouble()));
                return window.viewerState();
            });
        toolsRegistered &= mcpServer.registerTool(QStringLiteral("viewer.capture"),
            QStringLiteral("Capture this viewer window directly, including when it is not active"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                        {QStringLiteral("properties"), QJsonObject{
                            {QStringLiteral("timeoutMs"), QJsonObject{
                                {QStringLiteral("type"), QStringLiteral("integer")},
                                {QStringLiteral("minimum"), 1},
                                {QStringLiteral("maximum"), 60000}}}}}},
            [&window](const QJsonObject& arguments) {
                int timeoutMs = 3000;
                if (arguments.contains(QStringLiteral("timeoutMs"))) {
                    const QJsonValue timeoutValue = arguments.value(
                        QStringLiteral("timeoutMs"));
                    if (!timeoutValue.isDouble() ||
                        !std::isfinite(timeoutValue.toDouble()) ||
                        std::trunc(timeoutValue.toDouble()) != timeoutValue.toDouble() ||
                        timeoutValue.toDouble() < 1.0 ||
                        timeoutValue.toDouble() > 60000.0) {
                        return QJsonObject{{QStringLiteral("error"),
                            QStringLiteral("timeoutMs must be an integer from 1 to 60000")}};
                    }
                    timeoutMs = static_cast<int>(timeoutValue.toDouble());
                }
                struct CaptureResult {
                    QImage image;
                    QString error;
                };
                auto captureResult = std::make_shared<std::promise<CaptureResult>>();
                auto captureFuture = captureResult->get_future().share();
                const ArtifactNativeWindowHandle nativeWindow =
#ifdef _WIN32
                    reinterpret_cast<HWND>(window.winId());
#else
                    nullptr;
#endif
                std::thread captureWorker;

                // Let pending render requests reach the swap chain before
                // starting WGC. Pump Qt events while the capture worker waits,
                // without activating the window or pausing its render ticker.
                QTimer::singleShot(50,
                    [captureResult, &captureWorker, nativeWindow, timeoutMs] {
                        try {
                            captureWorker = std::thread(
                                [captureResult, nativeWindow, timeoutMs] {
                                    CaptureResult result;
                                    try {
                                        result.image = artifactCaptureWindow(
                                            nativeWindow, &result.error, timeoutMs);
                                    } catch (const std::exception& exception) {
                                        result.error = QString::fromUtf8(exception.what());
                                    } catch (...) {
                                        result.error = QStringLiteral(
                                            "Unexpected capture worker failure");
                                    }
                                    captureResult->set_value(std::move(result));
                                });
                        } catch (const std::exception& exception) {
                            captureResult->set_value(CaptureResult{
                                {}, QString::fromUtf8(exception.what())});
                        } catch (...) {
                            captureResult->set_value(CaptureResult{
                                {}, QStringLiteral("Could not start capture worker")});
                        }
                    });
                while (captureFuture.wait_for(std::chrono::milliseconds(0)) !=
                       std::future_status::ready) {
                    QCoreApplication::processEvents(
                        QEventLoop::AllEvents | QEventLoop::ExcludeUserInputEvents,
                        5);
                    QThread::msleep(1);
                }
                CaptureResult captured = captureFuture.get();
                if (captureWorker.joinable()) captureWorker.join();
                const QImage& image = captured.image;
                if (image.isNull())
                    return QJsonObject{{QStringLiteral("error"), captured.error}};
                const QByteArray png = artifactEncodePng(image);
                if (png.isEmpty())
                    return QJsonObject{{QStringLiteral("error"), QStringLiteral("PNG encoding failed")}};
                QJsonObject result = window.viewerState();
                result[QStringLiteral("captureWidth")] = image.width();
                result[QStringLiteral("captureHeight")] = image.height();
                result[QStringLiteral("mcpContent")] = QJsonArray{QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("image")},
                    {QStringLiteral("data"), QString::fromLatin1(png.toBase64())},
                    {QStringLiteral("mimeType"), QStringLiteral("image/png")}}};
                return result;
            });
        if (!toolsRegistered || !mcpServer.start()) {
            qCritical("Could not start the FBX Viewer MCP stdio transport");
            return 2;
        }
    }
    if (mcpStdio || !captureDirectory.isEmpty())
        window.setAttribute(Qt::WA_ShowWithoutActivating, true);
    if (alwaysOnTop) {
        window.setWindowFlag(Qt::WindowStaysOnTopHint, true);
    }
    window.placeOnLeftmostDisplay();
    window.show();
    if (!modelPath.isEmpty()) window.loadModel(modelPath, initialClipIndex);
    if (!captureDirectory.isEmpty()) {
        qInfo().noquote() << "Capturing FBX frames to" << captureDirectory;
        window.captureFrames(captureDirectory, 6);
    }
    return app.exec();
}
