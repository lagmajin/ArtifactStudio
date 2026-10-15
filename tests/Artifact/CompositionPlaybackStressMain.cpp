#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <QApplication>
#include <QByteArray>
#include <QBuffer>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QMainWindow>
#include <QScreen>
#include <QString>
#include <QStringList>
#include <QTimerEvent>
#include <QVector>
#include <QVBoxLayout>
#include <QWidget>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>

#include "../ArtifactTestMcpServer.h"
#include "../ArtifactTestWindowCapture.h"

import Artifact.Application.Manager;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Layer.InitParams;
import Artifact.Layers.SolidImage;
import Artifact.Widgets.CompositionRenderWidget;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Color.Float;
import Utils.String.UniString;

namespace {

struct StressConfig {
    int compositionCount = 1;
    int layersPerComposition = 100;
    int width = 1920;
    int height = 1080;
    double fps = 30.0;
    QString transformProfile = QStringLiteral("animated");
    QString rendererBackend = QStringLiteral("auto");
    int durationSeconds = 600;
    int sampleIntervalMs = 1000;
    QString outputDirectory;
};

class CompositionPlaybackStressWindow final : public QMainWindow {
public:
    explicit CompositionPlaybackStressWindow(StressConfig config)
        : config_(std::move(config)), playback_(Artifact::ArtifactPlaybackService::instance()),
          project_(Artifact::ArtifactProjectService::instance())
    {
        setWindowTitle(QStringLiteral("Artifact Composition Playback Stress Test"));
        resize(900, 520);
        auto* central = new QWidget(this);
        auto* layout = new QVBoxLayout(central);
        status_ = new QLabel(central);
        status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(status_);
        viewport_ = new Artifact::ArtifactCompositionRenderWidget(central);
        layout->addWidget(viewport_, 1);
        setCentralWidget(central);
        if (!prepare()) {
            setupError_ = QStringLiteral("Could not create the requested stress fixture");
        }
        refresh();
    }

    ~CompositionPlaybackStressWindow() override
    {
        if (sampleTimerId_ != 0) killTimer(sampleTimerId_);
        if (playback_) {
            playback_->stop();
            playback_->waitForStop();
            playback_->setCurrentComposition({});
        }
    }

    bool ready() const { return setupError_.isEmpty() && !compositions_.isEmpty(); }

    QJsonObject state() const
    {
        QJsonObject result{
            {QStringLiteral("ready"), ready()},
            {QStringLiteral("compositionCount"), compositions_.size()},
            {QStringLiteral("layersPerComposition"), config_.layersPerComposition},
            {QStringLiteral("width"), config_.width},
            {QStringLiteral("height"), config_.height},
            {QStringLiteral("fps"), config_.fps},
            {QStringLiteral("transformProfile"), config_.transformProfile},
            {QStringLiteral("rendererBackendRequested"), config_.rendererBackend},
            {QStringLiteral("durationSeconds"), config_.durationSeconds},
            {QStringLiteral("playing"), playback_ && playback_->isPlaying()},
            {QStringLiteral("playbackState"), playback_ ? static_cast<int>(playback_->state()) : -1},
            {QStringLiteral("frame"), playback_ ? playback_->currentFrame().framePosition() : -1},
            {QStringLiteral("droppedFrames"), playback_ ? playback_->droppedFrameCount() : 0},
            {QStringLiteral("observedPresentedFrames"), playback_ ? playback_->observedPresentedFrameCount() : 0},
            {QStringLiteral("runActive"), runActive_},
            {QStringLiteral("runElapsedMs"), runTimer_.isValid() ? runTimer_.elapsed() : 0},
            {QStringLiteral("frameChangeSamples"), frameAdvances_},
            {QStringLiteral("stalledSamples"), stalledSamples_},
            {QStringLiteral("sampleCount"), sampleTotalCount_},
            {QStringLiteral("retainedSampleCount"), samples_.size()},
            {QStringLiteral("outputDirectory"), config_.outputDirectory}
        };
        if (!setupError_.isEmpty()) result[QStringLiteral("error")] = setupError_;
        if (!compositions_.isEmpty()) {
            result[QStringLiteral("activeCompositionIndex")] = activeCompositionIndex_;
            result[QStringLiteral("activeCompositionId")] =
                compositions_.at(activeCompositionIndex_)->id().toString();
            result[QStringLiteral("activeLayerCount")] =
                compositions_.at(activeCompositionIndex_)->allLayer().size();
        }
        if (!lastCapturePath_.isEmpty())
            result[QStringLiteral("lastCapturePath")] = lastCapturePath_;
        if (!lastResultPath_.isEmpty())
            result[QStringLiteral("resultPath")] = lastResultPath_;
        return result;
    }

    QJsonObject startRun()
    {
        if (!ready()) return error(QStringLiteral("fixture is not ready"));
        if (runActive_) return error(QStringLiteral("a stress run is already active"));
        playback_->setCurrentComposition(compositions_.at(activeCompositionIndex_));
        playback_->setLooping(true);
        playback_->goToStartFrame();
        playback_->play();
        if (!playback_->isPlaying()) return error(QStringLiteral("PlaybackService did not enter playing state"));
        samples_ = {};
        sampleTotalCount_ = 0;
        frameAdvances_ = 0;
        stalledSamples_ = 0;
        previousFrame_ = playback_->currentFrame().framePosition();
        runTimer_.start();
        runActive_ = true;
        if (sampleTimerId_ != 0) killTimer(sampleTimerId_);
        sampleTimerId_ = startTimer(config_.sampleIntervalMs, Qt::PreciseTimer);
        if (sampleTimerId_ == 0) {
            runActive_ = false;
            playback_->stop();
            return error(QStringLiteral("could not start playback sampling timer"));
        }
        qInfo().noquote() << "[CompositionStress] run started"
                          << "compositions=" << config_.compositionCount
                          << "layersPerComposition=" << config_.layersPerComposition
                          << "durationSeconds=" << config_.durationSeconds;
        refresh();
        return state();
    }

    QJsonObject pause() { if (!playback_) return error(QStringLiteral("playback service unavailable")); playback_->pause(); refresh(); return state(); }
    QJsonObject stop() { finishRun(QStringLiteral("stopped-by-request")); refresh(); return state(); }

    QJsonObject waitForRun(int timeoutMs)
    {
        if (!runActive_ && lastRunResult_.isEmpty())
            return error(QStringLiteral("no stress run has been started"));
        QElapsedTimer waitTimer;
        waitTimer.start();
        while (runActive_ && waitTimer.elapsed() < timeoutMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
            QThread::msleep(10);
        }
        if (runActive_) {
            finishRun(QStringLiteral("wait-timeout"));
            refresh();
            return error(QStringLiteral("timed out waiting for stress run"));
        }
        refresh();
        if (lastRunResult_.isEmpty()) return state();
        QJsonObject summary = lastRunResult_;
        summary.remove(QStringLiteral("samples"));
        summary[QStringLiteral("samplesInResultFile")] = true;
        if (!summary.value(QStringLiteral("passed")).toBool())
            summary[QStringLiteral("error")] =
                QStringLiteral("stress run did not complete successfully");
        return summary;
    }

    QJsonObject switchComposition(int index)
    {
        if (index < 0 || index >= compositions_.size())
            return error(QStringLiteral("composition index is out of range"));
        const bool resume = playback_ && playback_->isPlaying();
        playback_->stop();
        playback_->waitForStop();
        activeCompositionIndex_ = index;
        playback_->setCurrentComposition(compositions_.at(index));
        viewport_->setComposition(compositions_.at(index));
        if (resume) playback_->play();
        refresh();
        return state();
    }

    QJsonObject capture(int timeoutMs)
    {
#ifdef _WIN32
        QString captureError;
        const QImage image = artifactCaptureWindow(
            reinterpret_cast<HWND>(winId()), &captureError, timeoutMs);
        if (image.isNull()) return error(captureError.isEmpty()
            ? QStringLiteral("window capture failed") : captureError);
        QByteArray png;
        QBuffer buffer(&png);
        if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG"))
            return error(QStringLiteral("could not encode captured window"));
        if (!config_.outputDirectory.isEmpty()) {
            QDir directory(config_.outputDirectory);
            if (!directory.mkpath(QStringLiteral(".")))
                return error(QStringLiteral("could not create output directory"));
            lastCapturePath_ = directory.filePath(
                QStringLiteral("capture-%1.png").arg(captureIndex_++, 4, 10, QLatin1Char('0')));
            QFile file(lastCapturePath_);
            if (!file.open(QIODevice::WriteOnly) || file.write(png) != png.size())
                return error(QStringLiteral("could not save capture"));
        }
        QJsonObject result = state();
        result[QStringLiteral("mcpContent")] = QJsonArray{QJsonObject{
            {QStringLiteral("type"), QStringLiteral("image")},
            {QStringLiteral("mimeType"), QStringLiteral("image/png")},
            {QStringLiteral("data"), QString::fromLatin1(png.toBase64())}}};
        return result;
#else
        Q_UNUSED(timeoutMs)
        return error(QStringLiteral("window capture is only available on Windows"));
#endif
    }

private:
    void timerEvent(QTimerEvent* event) override
    {
        if (event && event->timerId() == sampleTimerId_) samplePlayback();
        else QMainWindow::timerEvent(event);
    }

    static QJsonObject error(const QString& message)
    { return {{QStringLiteral("error"), message}}; }

    bool prepare()
    {
        if (!playback_ || !project_) { setupError_ = QStringLiteral("required services are unavailable"); return false; }
        if (config_.compositionCount < 1 || config_.layersPerComposition < 1 ||
            static_cast<qint64>(config_.compositionCount) * config_.layersPerComposition > 5000 ||
            config_.width < 64 || config_.height < 64 || !std::isfinite(config_.fps) ||
            config_.fps <= 0.0 || config_.durationSeconds < 1 ||
            config_.sampleIntervalMs < 100) {
            setupError_ = QStringLiteral("invalid stress configuration"); return false;
        }
        for (int compositionIndex = 0; compositionIndex < config_.compositionCount; ++compositionIndex) {
            Artifact::ArtifactCompositionInitParams params;
            params.setCompositionName(ArtifactCore::UniString(
                QStringLiteral("Playback Stress %1").arg(compositionIndex + 1)));
            params.setResolution(config_.width, config_.height);
            params.setFrameRate(config_.fps);
            params.setDurationFrames(std::max(120, static_cast<int>(std::ceil(config_.fps * 10.0))));
            project_->createComposition(params);
            auto composition = project_->currentComposition().lock();
            if (!composition) { setupError_ = QStringLiteral("composition creation failed"); return false; }
            compositions_.append(composition);
            const double aspect = static_cast<double>(config_.width) / config_.height;
            const int columns = std::max(1, std::min(config_.layersPerComposition,
                static_cast<int>(std::ceil(std::sqrt(config_.layersPerComposition * aspect)))));
            const int rows = (config_.layersPerComposition + columns - 1) / columns;
            const float cellWidth = static_cast<float>(config_.width) / columns;
            const float cellHeight = static_cast<float>(config_.height) / rows;
            for (int layerIndex = 0; layerIndex < config_.layersPerComposition; ++layerIndex) {
                Artifact::ArtifactSolidLayerInitParams layerParams(
                    QStringLiteral("Stress Layer %1").arg(layerIndex + 1));
                layerParams.setWidth(std::min(32, config_.width));
                layerParams.setHeight(std::min(32, config_.height));
                const int phase = (layerIndex * 37 + compositionIndex * 71) % 255;
                layerParams.setColor(ArtifactCore::FloatColor(
                    static_cast<float>((phase * 13) % 255) / 255.0f,
                    static_cast<float>((phase * 29 + 61) % 255) / 255.0f,
                    static_cast<float>((phase * 47 + 127) % 255) / 255.0f, 1.0f));
                project_->addLayerToCurrentComposition(layerParams, false, false, false);
                const auto layers = composition->allLayer();
                if (layers.isEmpty() || !layers.back()) {
                    setupError_ = QStringLiteral("layer creation failed at index %1").arg(layerIndex);
                    return false;
                }
                const float x = (static_cast<float>(layerIndex % columns) + 0.5f) * cellWidth - 16.0f;
                const float y = (static_cast<float>(layerIndex / columns) + 0.5f) * cellHeight - 16.0f;
                layers.back()->setPosition3D({x, y, 0.0f});
                if (config_.transformProfile == QStringLiteral("animated")) {
                    auto& transform = layers.back()->transform3D();
                    constexpr int keyFrames[] = {0, 75, 150, 225, 299};
                    for (int keyIndex = 0; keyIndex < 5; ++keyIndex) {
                        const auto time = layers.back()->keyframeTimeAtFrame(keyFrames[keyIndex]);
                        const float phase = static_cast<float>(
                            (layerIndex * 19 + compositionIndex * 43 + keyIndex * 67) % 360);
                        const float radians = phase * 0.017453292519943295f;
                        const float step = static_cast<float>(keyIndex);
                        const float xOffset = std::sin(radians) * (12.0f + step * 3.0f);
                        const float yOffset = std::cos(radians * 1.7f) * (10.0f + step * 2.0f);
                        const float scaleX = 0.55f + static_cast<float>(
                            (layerIndex * 7 + keyIndex * 13) % 95) / 100.0f;
                        const float scaleY = 0.6f + static_cast<float>(
                            (layerIndex * 11 + keyIndex * 17) % 85) / 100.0f;
                        const float scaleZ = 0.75f + static_cast<float>(
                            (layerIndex * 5 + keyIndex * 9) % 60) / 100.0f;
                        transform.setPosition(time, x + xOffset, y + yOffset);
                        transform.setPositionZ(time, static_cast<float>(
                            ((layerIndex * 3 + keyIndex * 7) % 41) - 20));
                        transform.setRotation(time, phase + step * 11.0f);
                        transform.setRotationX(time, std::sin(radians) * 24.0f);
                        transform.setRotationY(time, std::cos(radians * 1.3f) * 24.0f);
                        transform.setScale(time, scaleX, scaleY, scaleZ);
                        transform.setAnchor(time,
                            16.0f + std::sin(radians) * 8.0f,
                            16.0f + std::cos(radians) * 8.0f,
                            std::sin(radians * 0.5f) * 4.0f);
                    }
                }
            }
            if (composition->allLayer().size() != config_.layersPerComposition) {
                setupError_ = QStringLiteral("generated layer count does not match request");
                return false;
            }
        }
        activeCompositionIndex_ = 0;
        playback_->setCurrentComposition(compositions_.front());
        viewport_->setComposition(compositions_.front());
        playback_->setLooping(true);
        return true;
    }

    void samplePlayback()
    {
        if (!runActive_ || !playback_) return;
        const qint64 now = runTimer_.elapsed();
        const qint64 frame = playback_->currentFrame().framePosition();
        if (frame != previousFrame_) frameAdvances_++;
        else if (playback_->isPlaying()) stalledSamples_++;
        previousFrame_ = frame;
        samples_.append(QJsonObject{
            {QStringLiteral("elapsedMs"), now},
            {QStringLiteral("frame"), frame},
            {QStringLiteral("playing"), playback_->isPlaying()},
            {QStringLiteral("droppedFrames"), playback_->droppedFrameCount()}});
        ++sampleTotalCount_;
        if (samples_.size() > 3600) samples_.removeFirst();
        if (now >= static_cast<qint64>(config_.durationSeconds) * 1000)
            finishRun(QStringLiteral("duration-complete"));
        refresh();
    }

    void finishRun(const QString& reason)
    {
        if (!runActive_) {
            if (playback_) { playback_->stop(); playback_->waitForStop(); }
            return;
        }
        runActive_ = false;
        if (sampleTimerId_ != 0) {
            killTimer(sampleTimerId_);
            sampleTimerId_ = 0;
        }
        if (playback_) { playback_->stop(); playback_->waitForStop(); }
        const qint64 elapsed = runTimer_.isValid() ? runTimer_.elapsed() : 0;
        QJsonArray compositionIds;
        for (const auto& composition : compositions_)
            if (composition) compositionIds.append(composition->id().toString());
        QJsonObject result{{QStringLiteral("finishReason"), reason},
            {QStringLiteral("passed"), reason == QStringLiteral("duration-complete") && frameAdvances_ > 0},
            {QStringLiteral("elapsedMs"), elapsed},
            {QStringLiteral("compositionCount"), config_.compositionCount},
            {QStringLiteral("layersPerComposition"), config_.layersPerComposition},
            {QStringLiteral("totalLayerCount"), config_.compositionCount * config_.layersPerComposition},
            {QStringLiteral("width"), config_.width},
            {QStringLiteral("height"), config_.height},
            {QStringLiteral("fps"), config_.fps},
            {QStringLiteral("transformProfile"), config_.transformProfile},
            {QStringLiteral("rendererBackendRequested"), config_.rendererBackend},
            {QStringLiteral("requestedDurationSeconds"), config_.durationSeconds},
            {QStringLiteral("sampleIntervalMs"), config_.sampleIntervalMs},
            {QStringLiteral("outputDirectory"), config_.outputDirectory},
            {QStringLiteral("compositionIds"), compositionIds},
            {QStringLiteral("frameChangeSamples"), frameAdvances_},
            {QStringLiteral("stalledSamples"), stalledSamples_},
            {QStringLiteral("sampleCount"), sampleTotalCount_},
            {QStringLiteral("retainedSampleCount"), samples_.size()},
            {QStringLiteral("samplesTruncated"), sampleTotalCount_ > samples_.size()},
            {QStringLiteral("samples"), samples_}};
        if (!config_.outputDirectory.isEmpty()) {
            QDir directory(config_.outputDirectory);
            if (directory.mkpath(QStringLiteral("."))) {
                lastResultPath_ = directory.filePath(QStringLiteral("playback-run.json"));
                QFile file(lastResultPath_);
                const QByteArray resultBytes = QJsonDocument(result).toJson(QJsonDocument::Indented);
                if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
                    file.write(resultBytes) != resultBytes.size()) {
                    result[QStringLiteral("passed")] = false;
                    result[QStringLiteral("error")] = QStringLiteral("could not write playback-run.json");
                    qWarning().noquote() << "[CompositionStress] could not write playback-run.json";
                }
            } else {
                result[QStringLiteral("passed")] = false;
                result[QStringLiteral("error")] = QStringLiteral("could not create output directory");
            }
        }
        qInfo().noquote() << "[CompositionStress] run finished"
                          << "reason=" << reason << "frameChangeSamples=" << frameAdvances_
                          << "stalledSamples=" << stalledSamples_;
        lastRunResult_ = result;
    }

    void refresh()
    {
        status_->setText(QString::fromUtf8(QJsonDocument(state()).toJson(QJsonDocument::Indented)));
    }

    StressConfig config_;
    Artifact::ArtifactPlaybackService* playback_ = nullptr;
    Artifact::ArtifactProjectService* project_ = nullptr;
    QVector<Artifact::ArtifactCompositionPtr> compositions_;
    QLabel* status_ = nullptr;
    Artifact::ArtifactCompositionRenderWidget* viewport_ = nullptr;
    QElapsedTimer runTimer_;
    QJsonArray samples_;
    QJsonObject lastRunResult_;
    QString setupError_;
    QString lastCapturePath_;
    QString lastResultPath_;
    int activeCompositionIndex_ = 0;
    int sampleTimerId_ = 0;
    int captureIndex_ = 0;
    int previousFrame_ = -1;
    int frameAdvances_ = 0;
    qint64 sampleTotalCount_ = 0;
    int stalledSamples_ = 0;
    bool runActive_ = false;
};

bool parsePositive(const QString& text, int& output, int maximum)
{
    bool valid = false;
    const int value = text.toInt(&valid);
    if (!valid || value < 1 || value > maximum) return false;
    output = value;
    return true;
}

#ifdef _WIN32
LONG CALLBACK captureDebugBreakpointStack(EXCEPTION_POINTERS* exceptionInfo)
{
    if (!exceptionInfo || !exceptionInfo->ExceptionRecord ||
        exceptionInfo->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    void* frames[32]{};
    const USHORT frameCount = CaptureStackBackTrace(
        0, 32, frames, nullptr);
    std::fprintf(stderr, "[CompositionStress][Breakpoint] code=0x%08lx address=%p frames=%u\n",
        exceptionInfo->ExceptionRecord->ExceptionCode,
        exceptionInfo->ExceptionRecord->ExceptionAddress,
        static_cast<unsigned>(frameCount));
    for (USHORT index = 0; index < frameCount; ++index) {
        std::fprintf(stderr, "[CompositionStress][Breakpoint] frame[%u]=%p\n",
            static_cast<unsigned>(index), frames[index]);
    }
    std::fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    StressConfig config;
    const QStringList arguments = app.arguments();
    bool captureDebugBreakpoints = false;
    for (qsizetype index = 1; index < arguments.size(); ++index) {
        const QString option = arguments[index];
        if (option == QStringLiteral("--mcp-stdio")) continue;
        if (option == QStringLiteral("--capture-debug-breakpoints")) {
            captureDebugBreakpoints = true;
            continue;
        }
        if (index + 1 >= arguments.size()) continue;
        const QString value = arguments[++index];
        int parsed = 0;
        if (option == QStringLiteral("--renderer")) {
            const QString backend = value.trimmed().toLower();
            if (backend != QStringLiteral("auto") && backend != QStringLiteral("d3d12") &&
                backend != QStringLiteral("dx12") && backend != QStringLiteral("vulkan")) {
                std::fprintf(stderr, "Unsupported renderer backend: %s\n", value.toUtf8().constData());
                return 2;
            }
            config.rendererBackend = backend == QStringLiteral("d3d12")
                ? QStringLiteral("dx12") : backend;
        }
        else if (option == QStringLiteral("--composition-count") && parsePositive(value, parsed, 200)) config.compositionCount = parsed;
        else if (option == QStringLiteral("--layer-count") && parsePositive(value, parsed, 5000)) config.layersPerComposition = parsed;
        else if (option == QStringLiteral("--width") && parsePositive(value, parsed, 8192)) config.width = parsed;
        else if (option == QStringLiteral("--height") && parsePositive(value, parsed, 8192)) config.height = parsed;
        else if (option == QStringLiteral("--duration-seconds") && parsePositive(value, parsed, 86400)) config.durationSeconds = parsed;
        else if (option == QStringLiteral("--sample-interval-ms") && parsePositive(value, parsed, 60000)) config.sampleIntervalMs = parsed;
        else if (option == QStringLiteral("--transform-profile")) {
            const QString profile = value.trimmed().toLower();
            if (profile != QStringLiteral("static") && profile != QStringLiteral("animated")) {
                std::fprintf(stderr, "Unsupported transform profile: %s\n", value.toUtf8().constData());
                return 2;
            }
            config.transformProfile = profile;
        }
        else if (option == QStringLiteral("--fps")) { bool ok = false; const double fps = value.toDouble(&ok); if (ok && std::isfinite(fps) && fps > 0.0 && fps <= 240.0) config.fps = fps; }
        else if (option == QStringLiteral("--output-dir")) config.outputDirectory = value;
        else --index;
    }
    qputenv("ARTIFACT_RENDER_BACKEND", config.rendererBackend.toUtf8());
    Artifact::ArtifactApplicationManager::instance();
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& message) {
        const QByteArray bytes = message.toUtf8();
        std::fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stderr);
        std::fputc('\n', stderr);
    });
    CompositionPlaybackStressWindow window(std::move(config));
    ArtifactTestMcpServer mcp(QStringLiteral("artifact-composition-playback-stress"), QStringLiteral("1.0.0"));
    bool mcpStdio = false;
    for (const QString& argument : arguments) mcpStdio |= argument == QStringLiteral("--mcp-stdio");
    if (mcpStdio) {
        const QJsonObject objectSchema{{QStringLiteral("type"), QStringLiteral("object")}};
        bool ok = true;
        ok &= mcp.registerTool(QStringLiteral("stress.state"), QStringLiteral("Read test configuration and playback status"), objectSchema,
            [&window](const QJsonObject&) { return window.state(); });
        ok &= mcp.registerTool(QStringLiteral("stress.start"), QStringLiteral("Start timed playback using ArtifactPlaybackService"), objectSchema,
            [&window](const QJsonObject&) { return window.startRun(); });
        ok &= mcp.registerTool(QStringLiteral("stress.wait"), QStringLiteral("Wait for the timed playback run to finish and return its report"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{{QStringLiteral("timeoutMs"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("minimum"), 1}, {QStringLiteral("maximum"), 86430000}}}}}},
            [&window](const QJsonObject& args) {
                int timeout = 660000;
                if (args.contains(QStringLiteral("timeoutMs")) &&
                    !parsePositive(args.value(QStringLiteral("timeoutMs")).toVariant().toString(), timeout, 86430000))
                    return QJsonObject{{QStringLiteral("error"), QStringLiteral("timeoutMs must be in [1,86430000]")}};
                return window.waitForRun(timeout);
            });
        ok &= mcp.registerTool(QStringLiteral("stress.pause"), QStringLiteral("Pause playback"), objectSchema,
            [&window](const QJsonObject&) { return window.pause(); });
        ok &= mcp.registerTool(QStringLiteral("stress.stop"), QStringLiteral("Stop playback and finalize the run result"), objectSchema,
            [&window](const QJsonObject&) { return window.stop(); });
        ok &= mcp.registerTool(QStringLiteral("stress.shutdown"), QStringLiteral("Stop playback and exit the standalone test process"), objectSchema,
            [&window](const QJsonObject&) {
                window.stop();
                QCoreApplication::quit();
                QJsonObject result = window.state();
                result[QStringLiteral("shutdownRequested")] = true;
                return result;
            });
        ok &= mcp.registerTool(QStringLiteral("stress.switchComposition"), QStringLiteral("Select a generated composition by zero-based index"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{{QStringLiteral("index"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}}}}},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("index")}}},
            [&window](const QJsonObject& args) {
                const QJsonValue value = args.value(QStringLiteral("index"));
                if (!value.isDouble() || std::trunc(value.toDouble()) != value.toDouble() ||
                    value.toDouble() < 0 || value.toDouble() > std::numeric_limits<int>::max())
                    return QJsonObject{{QStringLiteral("error"), QStringLiteral("index must be a non-negative integer")}};
                return window.switchComposition(value.toInt());
            });
        ok &= mcp.registerTool(QStringLiteral("stress.capture"), QStringLiteral("Capture the test window without activating it"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{{QStringLiteral("timeoutMs"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("minimum"), 1}, {QStringLiteral("maximum"), 60000}}}}}},
            [&window](const QJsonObject& args) {
                int timeout = 3000;
                if (args.contains(QStringLiteral("timeoutMs")) &&
                    !parsePositive(args.value(QStringLiteral("timeoutMs")).toVariant().toString(), timeout, 60000))
                    return QJsonObject{{QStringLiteral("error"), QStringLiteral("timeoutMs must be in [1,60000]")}};
                return window.capture(timeout);
            });
        if (!ok || !mcp.start()) return 2;
    }
    const QScreen* leftmost = app.primaryScreen();
    for (const auto* screen : app.screens()) if (leftmost && screen->geometry().left() < leftmost->geometry().left()) leftmost = screen;
    if (mcpStdio) window.setAttribute(Qt::WA_ShowWithoutActivating);
    if (leftmost) { window.resize(window.size().boundedTo(leftmost->availableGeometry().size())); window.move(leftmost->availableGeometry().topLeft()); }
    window.show();
#ifdef _WIN32
    const PVOID breakpointHandler = captureDebugBreakpoints
        ? AddVectoredExceptionHandler(1, captureDebugBreakpointStack)
        : nullptr;
    if (captureDebugBreakpoints && !breakpointHandler) {
        std::fprintf(stderr, "Could not install breakpoint stack diagnostic handler.\n");
    }
#endif
    const int exitCode = app.exec();
    return exitCode;
}
