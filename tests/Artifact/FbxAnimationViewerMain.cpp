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
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMimeData>
#include <QPixmap>
#include <QScreen>
#include <QStatusBar>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

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

        QScreen* screen = QGuiApplication::primaryScreen();
        if (!screen) {
            qWarning("No screen available for FBX frame capture");
            return;
        }

        for (int frame = 0; frame < count; ++frame) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            QThread::msleep(500);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            const QPixmap image = screen->grabWindow(viewer_->winId());
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
            viewer_->setAnimationPlaybackEnabled(
                !viewer_->animationPlaybackEnabled());
            statusBar()->showMessage(viewer_->animationPlaybackEnabled()
                ? QStringLiteral("Animation playing")
                : QStringLiteral("Animation paused"));
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
};

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QString modelPath;
    QString captureDirectory;
    bool alwaysOnTop = false;
    int initialClipIndex = 0;
    for (int index = 1; index < argc; ++index) {
        const QString argument = QString::fromLocal8Bit(argv[index]);
        if (argument == QStringLiteral("--capture-frames") &&
            index + 1 < argc) {
            captureDirectory = QString::fromLocal8Bit(argv[++index]);
        } else if (argument == QStringLiteral("--always-on-top")) {
            alwaysOnTop = true;
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
    if (alwaysOnTop) {
        window.setWindowFlag(Qt::WindowStaysOnTopHint, true);
    }
    window.show();
    if (!modelPath.isEmpty()) window.loadModel(modelPath, initialClipIndex);
    if (!captureDirectory.isEmpty()) {
        qInfo().noquote() << "Capturing FBX frames to" << captureDirectory;
        window.captureFrames(captureDirectory, 6);
    }
    return app.exec();
}
