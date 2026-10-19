#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QComboBox>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonObject>
#include <QImage>
#include <QLabel>
#include <QMainWindow>
#include <QScreen>
#include <QPoint>
#include <QShowEvent>
#include <QThread>
#include <QTimer>
#include <QTimerEvent>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <exception>
#include <future>
#include <memory>
#include <thread>
#include <utility>

#include "../ArtifactTestMcpServer.h"
#include "../ArtifactTestWindowCapture.h"

import Artifact.Application.Manager;
import Composition3D;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Layer.Image;
import Artifact.Layers.Model3D;
import Artifact.Render.IRenderer;
import Artifact.Widgets.CompositionRenderController;
import Color.Float;
import Memory.SharedPtr;
import Time.Rational;
import Utils.Id;
import Utils.String.UniString;

namespace {

using ChannelMode = Artifact::ViewportChannelDisplayMode;

struct ChannelOption {
    const char* label;
    ChannelMode mode;
};

constexpr std::array<ChannelOption, 31> kChannelOptions{{
    {"RGB", ChannelMode::Color}, {"Alpha", ChannelMode::Alpha},
    {"RGBA", ChannelMode::ColorAlpha}, {"R", ChannelMode::Red},
    {"G", ChannelMode::Green}, {"B", ChannelMode::Blue},
    {"Depth", ChannelMode::Depth}, {"Emission", ChannelMode::Emission},
    {"Object ID", ChannelMode::ObjectId}, {"Material ID", ChannelMode::MaterialId},
    {"Albedo", ChannelMode::Albedo}, {"Albedo R", ChannelMode::AlbedoR},
    {"Albedo G", ChannelMode::AlbedoG}, {"Albedo B", ChannelMode::AlbedoB},
    {"Normal", ChannelMode::Normal}, {"Normal X", ChannelMode::NormalX},
    {"Normal Y", ChannelMode::NormalY}, {"Normal Z", ChannelMode::NormalZ},
    {"Velocity", ChannelMode::Velocity}, {"Velocity X", ChannelMode::VelocityX},
    {"Velocity Y", ChannelMode::VelocityY}, {"Position", ChannelMode::Position},
    {"Position X", ChannelMode::PositionX}, {"Position Y", ChannelMode::PositionY},
    {"Position Z", ChannelMode::PositionZ}, {"UV", ChannelMode::UV},
    {"U", ChannelMode::U}, {"V", ChannelMode::V},
    {"Unpremultiplied", ChannelMode::Unpremultiplied},
    {"Luminance", ChannelMode::Luminance}, {"Matte", ChannelMode::Matte}
}};

class CompositionViewportChannelTestWindow final : public QMainWindow {
public:
    explicit CompositionViewportChannelTestWindow()
    {
        setWindowTitle(QStringLiteral("Artifact Composition Viewport Channel Test"));
        resize(1160, 780);
        auto* root = new QWidget(this);
        auto* layout = new QVBoxLayout(root);
        auto* instructions = new QLabel(
            QStringLiteral("Choose a channel mode. The chart contains RGB bars, an alpha ramp, translucent blocks, and transparent areas."), root);
        layout->addWidget(instructions);
        channelPicker_ = new QComboBox(root);
        for (const auto& option : kChannelOptions) {
            channelPicker_->addItem(QString::fromUtf8(option.label),
                static_cast<int>(option.mode));
        }
        layout->addWidget(channelPicker_);
        status_ = new QLabel(QStringLiteral("Preparing test composition…"), root);
        layout->addWidget(status_);
        viewportHost_ = new QWidget(root);
        viewportHost_->setAttribute(Qt::WA_NativeWindow, true);
        layout->addWidget(viewportHost_, 1);
        setCentralWidget(root);
        Artifact::ArtifactApplicationManager::instance();
        prepareFixture();
        lastPickerIndex_ = channelPicker_->currentIndex();
        timerId_ = startTimer(50);
    }

    ~CompositionViewportChannelTestWindow() override
    {
        if (controller_) {
            controller_->stop();
            controller_->destroy();
        }
    }

    bool ready() const { return ready_; }

    void initializeViewport()
    {
        if (controller_) return;
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
        controller_->start();
        controller_->setCompositionBackgroundMode(
            static_cast<int>(Artifact::CompositionBackgroundMode::Checkerboard));
        ready_ = true;
        status_->setText(QStringLiteral("Ready — RGB / Alpha chart over a 3D sphere for auxiliary AOV inspection."));
        applyIndex(channelPicker_->currentIndex());
    }

    QJsonObject setChannel(const QString& name)
    {
        for (int index = 0; index < channelPicker_->count(); ++index) {
            if (channelPicker_->itemText(index).compare(name, Qt::CaseInsensitive) == 0) {
                channelPicker_->setCurrentIndex(index);
                lastPickerIndex_ = index;
                applyIndex(index);
                return state();
            }
        }
        return {{QStringLiteral("error"), QStringLiteral("unknown channel mode: %1").arg(name)}};
    }

    QJsonObject panViewport(double dx, double dy)
    {
        if (!controller_)
            return {{QStringLiteral("error"), QStringLiteral("viewport is not initialized")}};
        controller_->panBy({static_cast<float>(dx), static_cast<float>(dy)});
        controller_->markRenderDirty();
        controller_->renderOneFrame();
        return state();
    }

    QJsonObject state() const
    {
        const auto pan = controller_ ? controller_->viewportPan()
                                     : ArtifactCore::Coordinates::ScreenPhysicalVector2{};
        return {{QStringLiteral("ready"), ready_},
                {QStringLiteral("channel"), channelPicker_->currentText()},
                {QStringLiteral("compositionWidth"), 960},
                {QStringLiteral("compositionHeight"), 540},
                {QStringLiteral("presentationLayout"), controller_
                    ? static_cast<int>(controller_->presentationLayout()) : -1},
                {QStringLiteral("backgroundMode"), controller_
                    ? controller_->compositionBackgroundMode() : -1},
                {QStringLiteral("viewportZoom"), controller_
                    ? controller_->viewportZoom() : 1.0f},
                {QStringLiteral("viewportPan"), QJsonArray{pan.x, pan.y}},
                {QStringLiteral("fixture"), QStringLiteral("RGB chart with alpha ramps")},
                {QStringLiteral("error"), error_}};
    }

    QJsonObject inspectPixels() const
    {
        if (!controller_)
            return {{QStringLiteral("error"), QStringLiteral("viewport is not initialized")}};
        const auto summarize = [](QImage image) {
            if (image.isNull())
                return QJsonObject{{QStringLiteral("available"), false}};
            image = image.convertToFormat(QImage::Format_RGBA8888);
            std::array<int, 4> minimum{{255, 255, 255, 255}};
            std::array<int, 4> maximum{{0, 0, 0, 0}};
            for (int y = 0; y < image.height(); ++y) {
                const auto* row = image.constScanLine(y);
                for (int x = 0; x < image.width(); ++x) {
                    for (int channel = 0; channel < 4; ++channel) {
                        const int value = row[x * 4 + channel];
                        minimum[channel] = std::min(minimum[channel], value);
                        maximum[channel] = std::max(maximum[channel], value);
                    }
                }
            }
            QJsonArray minimumValues, maximumValues;
            for (int channel = 0; channel < 4; ++channel) {
                minimumValues.append(minimum[channel]);
                maximumValues.append(maximum[channel]);
            }
            QJsonArray edgeSamples;
            const std::array<QPoint, 5> samplePoints{{
                QPoint(0, 0), QPoint(image.width() - 1, 0),
                QPoint(0, image.height() - 1),
                QPoint(image.width() - 1, image.height() - 1),
                QPoint(image.width() / 2, image.height() / 2)}};
            for (const QPoint point : samplePoints) {
                const auto* pixel = image.constScanLine(point.y()) + point.x() * 4;
                edgeSamples.append(QJsonArray{point.x(), point.y(), pixel[0],
                                              pixel[1], pixel[2], pixel[3]});
            }
            return QJsonObject{{QStringLiteral("available"), true},
                               {QStringLiteral("width"), image.width()},
                               {QStringLiteral("height"), image.height()},
                               {QStringLiteral("minRGBA"), minimumValues},
                               {QStringLiteral("maxRGBA"), maximumValues},
                               {QStringLiteral("samplesXYRGBA"), edgeSamples}};
        };
        QJsonObject sourceAov;
        QJsonObject rendererChannels;
        if (auto* renderer = controller_->renderer()) {
            rendererChannels = {
                {QStringLiteral("multiChannelEnabled"), renderer->isMultiChannelEnabled()},
                {QStringLiteral("normalXEnabled"), renderer->isChannelEnabled(Artifact::ArtifactIRenderer::ChannelType::NormalX)},
                {QStringLiteral("normalYEnabled"), renderer->isChannelEnabled(Artifact::ArtifactIRenderer::ChannelType::NormalY)},
                {QStringLiteral("normalZEnabled"), renderer->isChannelEnabled(Artifact::ArtifactIRenderer::ChannelType::NormalZ)},
                {QStringLiteral("albedoREnabled"), renderer->isChannelEnabled(Artifact::ArtifactIRenderer::ChannelType::AlbedoR)},
                {QStringLiteral("objectIdEnabled"), renderer->isChannelEnabled(Artifact::ArtifactIRenderer::ChannelType::ObjectId)}};
            const auto readChannel = [renderer, &summarize, &sourceAov](
                const QString& name, Artifact::ArtifactIRenderer::ChannelType channel) {
                sourceAov.insert(name, summarize(renderer->readbackChannelToImage(channel)));
            };
            switch (static_cast<ChannelMode>(channelPicker_->currentData().toInt())) {
            case ChannelMode::Depth:
                readChannel(QStringLiteral("depth"), Artifact::ArtifactIRenderer::ChannelType::Depth);
                break;
            case ChannelMode::Normal:
                readChannel(QStringLiteral("normalX"), Artifact::ArtifactIRenderer::ChannelType::NormalX);
                readChannel(QStringLiteral("normalY"), Artifact::ArtifactIRenderer::ChannelType::NormalY);
                readChannel(QStringLiteral("normalZ"), Artifact::ArtifactIRenderer::ChannelType::NormalZ);
                break;
            case ChannelMode::Albedo:
                readChannel(QStringLiteral("albedoR"), Artifact::ArtifactIRenderer::ChannelType::AlbedoR);
                readChannel(QStringLiteral("albedoG"), Artifact::ArtifactIRenderer::ChannelType::AlbedoG);
                readChannel(QStringLiteral("albedoB"), Artifact::ArtifactIRenderer::ChannelType::AlbedoB);
                break;
            case ChannelMode::ObjectId:
                readChannel(QStringLiteral("objectId"), Artifact::ArtifactIRenderer::ChannelType::ObjectId);
                break;
            case ChannelMode::Emission:
                readChannel(QStringLiteral("emission"), Artifact::ArtifactIRenderer::ChannelType::Emission);
                break;
            default:
                break;
            }
        }
        return {{QStringLiteral("channel"), channelPicker_->currentText()},
                {QStringLiteral("presented"), summarize(controller_->readbackPresentedFrameImage())},
                {QStringLiteral("channelDisplay"), summarize(controller_->readbackViewportChannelDisplayImage())},
                {QStringLiteral("sourceAov"), sourceAov},
                {QStringLiteral("rendererChannels"), rendererChannels}};
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
                    result.image = artifactCaptureWindow(
                        nativeWindow, &result.error, 10000);
                    promise->set_value(std::move(result));
                });
            } catch (const std::exception& exception) {
                promise->set_value(CaptureResult{{}, QString::fromUtf8(exception.what())});
            } catch (...) {
                promise->set_value(CaptureResult{{}, QStringLiteral("Could not start capture worker")});
            }
        });
        while (future.wait_for(std::chrono::milliseconds(0)) !=
               std::future_status::ready) {
            QCoreApplication::processEvents(
                QEventLoop::AllEvents | QEventLoop::ExcludeUserInputEvents, 5);
            QThread::msleep(1);
        }
        CaptureResult captured = future.get();
        if (worker.joinable()) worker.join();
        const QImage& image = captured.image;
        const QString& error = captured.error;
        if (image.isNull())
            return {{QStringLiteral("error"), error.isEmpty()
                ? QStringLiteral("window capture failed") : error}};
        const QByteArray png = artifactEncodePng(image);
        if (png.isEmpty()) return {{QStringLiteral("error"), QStringLiteral("PNG encoding failed")}};
        QJsonObject result = state();
        result[QStringLiteral("captureWidth")] = image.width();
        result[QStringLiteral("captureHeight")] = image.height();
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
    void timerEvent(QTimerEvent* event) override
    {
        if (event && event->timerId() == timerId_) {
            const int index = channelPicker_->currentIndex();
            if (index != lastPickerIndex_) {
                lastPickerIndex_ = index;
                applyIndex(index);
            }
            return;
        }
        QMainWindow::timerEvent(event);
    }

    void prepareFixture()
    {
        Artifact::ArtifactCompositionInitParams params;
        params.setCompositionName(ArtifactCore::UniString(QStringLiteral("Viewport Channel Fixture")));
        params.setResolution(960, 540);
        params.setFrameRate(30.0);
        params.setDurationFrames(30);
        params.setBackgroundColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
        composition_ = ArtifactCore::makeShared<Artifact::ArtifactComposition3D>(
            ArtifactCore::CompositionID(QStringLiteral("viewport-channel-fixture")), params);
        if (!composition_) { fail(QStringLiteral("composition creation failed")); return; }

        auto aovSphere = ArtifactCore::makeShared<Artifact::Artifact3DLayer>();
        aovSphere->loadFromFile(
            QString::fromUtf8(ARTIFACT_CHANNEL_TEST_MODEL_OBJ_PATH));
        if (aovSphere->mesh().vertexCount() == 0) {
            fail(QStringLiteral("3D AOV model fixture failed to load"));
            return;
        }
        aovSphere->setLayerName(QStringLiteral("AOV Sphere"));
        aovSphere->transform3D().setCurrentScale(60.0f, 60.0f);
        auto& sphereTransform = aovSphere->transform3D();
        sphereTransform.setKeyframeTimeScale(30);
        sphereTransform.setPosition(ArtifactCore::RationalTime(0, 30), 480.0f, 270.0f);
        sphereTransform.setPosition(ArtifactCore::RationalTime(29, 30), 600.0f, 270.0f);
        sphereTransform.setCurrentPosition(480.0f, 270.0f);
        composition_->appendLayerTop(aovSphere);

        QImage chart(960, 540, QImage::Format_RGBA8888);
        chart.fill(Qt::transparent);
        constexpr std::array<std::array<uchar, 3>, 8> bars{{
            {{255, 0, 0}}, {{0, 255, 0}}, {{0, 0, 255}}, {{255, 255, 0}},
            {{0, 255, 255}}, {{255, 0, 255}}, {{255, 255, 255}}, {{64, 64, 64}}
        }};
        const int barWidth = chart.width() / static_cast<int>(bars.size());
        for (int y = 0; y < chart.height(); ++y) {
            auto* row = chart.scanLine(y);
            for (int x = 0; x < chart.width(); ++x) {
                const int bar = std::min(static_cast<int>(bars.size() - 1), x / barWidth);
                const int alpha = y < 100 ? 255
                    : y < 300 ? ((y - 100) * 255 / 200)
                    : (((x / 64) + (y / 64)) % 2 == 0 ? 128 : 0);
                const int offset = x * 4;
                row[offset + 0] = bars[bar][0];
                row[offset + 1] = bars[bar][1];
                row[offset + 2] = bars[bar][2];
                row[offset + 3] = static_cast<uchar>(alpha);
            }
        }
        auto imageLayer = ArtifactCore::makeShared<Artifact::ArtifactImageLayer>();
        imageLayer->setFromQImage(chart);
        imageLayer->setLayerName(QStringLiteral("RGBA Channel Chart"));
        // Transparent and translucent chart regions leave the 3D sphere
        // visible so auxiliary AOV modes have real geometry to inspect.
        composition_->appendLayerTop(imageLayer);
        chartLayer_ = imageLayer;
    }

    void applyIndex(int index)
    {
        if (index < 0 || index >= static_cast<int>(kChannelOptions.size())) return;
        const auto mode = kChannelOptions[static_cast<std::size_t>(index)].mode;
        const bool inspectAov = mode == ChannelMode::Depth ||
            mode == ChannelMode::Emission || mode == ChannelMode::ObjectId ||
            mode == ChannelMode::MaterialId || mode == ChannelMode::Albedo ||
            mode == ChannelMode::AlbedoR || mode == ChannelMode::AlbedoG ||
            mode == ChannelMode::AlbedoB || mode == ChannelMode::Normal ||
            mode == ChannelMode::NormalX || mode == ChannelMode::NormalY ||
            mode == ChannelMode::NormalZ || mode == ChannelMode::Velocity ||
            mode == ChannelMode::VelocityX || mode == ChannelMode::VelocityY ||
            mode == ChannelMode::Position || mode == ChannelMode::PositionX ||
            mode == ChannelMode::PositionY || mode == ChannelMode::PositionZ ||
            mode == ChannelMode::UV || mode == ChannelMode::U || mode == ChannelMode::V;
        if (chartLayer_) chartLayer_->setVisible(!inspectAov);
        if (controller_) {
            controller_->setViewportChannelDisplayMode(mode);
            controller_->markRenderDirty();
            controller_->renderOneFrame();
        }
        if (status_) status_->setText(QStringLiteral("Displaying: %1 — %2")
            .arg(channelPicker_->itemText(index), inspectAov
                ? QStringLiteral("3D AOV fixture")
                : QStringLiteral("RGBA chart fixture")));
    }

    void fail(const QString& message)
    {
        error_ = message;
        status_->setText(QStringLiteral("Setup failed: %1").arg(message));
    }

    bool ready_ = false;
    QString error_;
    int timerId_ = 0;
    int lastPickerIndex_ = -1;
    QComboBox* channelPicker_ = nullptr;
    QLabel* status_ = nullptr;
    Artifact::CompositionRenderController* controller_ = nullptr;
    Artifact::ArtifactCompositionPtr composition_;
    decltype(ArtifactCore::makeShared<Artifact::ArtifactImageLayer>()) chartLayer_;
    QWidget* viewportHost_ = nullptr;
};

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    Artifact::ArtifactApplicationManager::instance();
    CompositionViewportChannelTestWindow window;
    bool mcpMode = false;
    for (int i = 1; i < argc; ++i) mcpMode |= QByteArray(argv[i]) == "--mcp-stdio";
    ArtifactTestMcpServer mcp(QStringLiteral("artifact-composition-viewport-channel-test"), QStringLiteral("1.0.0"));
    bool toolsReady = true;
    if (mcpMode) {
        const QJsonObject objectSchema{{QStringLiteral("type"), QStringLiteral("object")}};
        toolsReady &= mcp.registerTool(QStringLiteral("viewport.state"), QStringLiteral("Read fixture and active channel mode"), objectSchema,
            [&window](const QJsonObject&) { return window.state(); });
        toolsReady &= mcp.registerTool(QStringLiteral("viewport.setChannel"), QStringLiteral("Switch the Composition Viewport display channel"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                        {QStringLiteral("properties"), QJsonObject{{QStringLiteral("name"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}}},
                        {QStringLiteral("required"), QJsonArray{QStringLiteral("name")}}},
            [&window](const QJsonObject& args) { return window.setChannel(args.value(QStringLiteral("name")).toString()); });
        toolsReady &= mcp.registerTool(QStringLiteral("viewport.pan"), QStringLiteral("Pan the test viewport to expose background margins"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                        {QStringLiteral("properties"), QJsonObject{
                            {QStringLiteral("dx"), QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}}},
                            {QStringLiteral("dy"), QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}}}}},
                        {QStringLiteral("required"), QJsonArray{QStringLiteral("dx"), QStringLiteral("dy")}}},
            [&window](const QJsonObject& args) {
                return window.panViewport(args.value(QStringLiteral("dx")).toDouble(),
                                          args.value(QStringLiteral("dy")).toDouble());
            });
        toolsReady &= mcp.registerTool(QStringLiteral("viewport.capture"), QStringLiteral("Capture the standalone viewport test window"), objectSchema,
            [&window](const QJsonObject&) { return window.capture(); });
        toolsReady &= mcp.registerTool(QStringLiteral("viewport.inspectPixels"), QStringLiteral("Inspect rendered channel value ranges in the standalone test viewport"), objectSchema,
            [&window](const QJsonObject&) { return window.inspectPixels(); });
        window.setAttribute(Qt::WA_ShowWithoutActivating, true);
        if (!toolsReady || !mcp.start()) return 2;
    }
    const QScreen* leftmost = app.primaryScreen();
    for (const auto* screen : app.screens())
        if (leftmost && screen->geometry().left() < leftmost->geometry().left()) leftmost = screen;
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
