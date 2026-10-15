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
#include <QElapsedTimer>
#include <QImage>
#include <QIODevice>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QScreen>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>
#include <limits>

#include <cstdio>

#include "../ArtifactTestMcpServer.h"
#include "../ArtifactTestWindowCapture.h"

import Artifact.Application.Manager;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Effect.Rasterizer.Glow;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Project.Manager;
import Artifact.Widgets.CompositionEditor;
import Memory.SharedPtr;
import Utils.String.UniString;

namespace {

class LayerSurfaceCacheWindow final : public QMainWindow {
public:
    explicit LayerSurfaceCacheWindow(bool mcpMode) : mcpMode_(mcpMode)
    {
        setWindowTitle(QStringLiteral("Artifact Composition Layer Surface Cache Test"));
        resize(1100, 760);
        auto* central = new QWidget(this);
        auto* layout = new QVBoxLayout(central);
        status_ = new QLabel(QStringLiteral("Preparing fixture…"), central);
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
                           {QStringLiteral("diagnostics"), controller()
                                ? controller()->layerSurfaceCacheDiagnostics()
                                : QJsonObject{}}};
        if (!error_.isEmpty()) result[QStringLiteral("error")] = error_;
        if (!effectLayerId_.isEmpty()) result[QStringLiteral("effectLayerId")] = effectLayerId_;
        if (!otherLayerId_.isEmpty()) result[QStringLiteral("otherLayerId")] = otherLayerId_;
        return result;
    }

    QJsonObject setDiagnostics(bool enabled)
    {
        if (!controller()) return fail(QStringLiteral("composition controller unavailable"));
        controller()->setLayerSurfaceCacheDiagnosticsEnabled(enabled);
        controller()->markRenderDirty();
        return state();
    }

    QJsonObject rerender()
    {
        if (!controller()) return fail(QStringLiteral("composition controller unavailable"));
        const auto before = controller()->viewportPan();
        controller()->panBy({1.0, 0.0});
        pumpEvents(250);
        controller()->panBy({-1.0, 0.0});
        pumpEvents(250);
        if (controller()->viewportPan().x != before.x ||
            controller()->viewportPan().y != before.y)
            return fail(QStringLiteral("view did not return to its original pan"));
        return state();
    }

    QJsonObject runRegression()
    {
        if (!ready_ || !controller()) return fail(QStringLiteral("fixture is not ready"));
        setDiagnostics(true);
        pumpEvents(500);
        const QJsonObject initial = controller()->layerSurfaceCacheDiagnostics();
        if (!rerender().value(QStringLiteral("error")).isUndefined())
            return fail(QStringLiteral("unchanged rerender failed"));
        const QJsonObject afterReuse = controller()->layerSurfaceCacheDiagnostics();
        const QJsonObject targetBefore = entryFor(afterReuse, effectLayerId_);
        if (targetBefore.value(QStringLiteral("hits")).toInt() <= 0)
            return fail(QStringLiteral("unchanged layer did not produce a surface cache hit"));

        const auto invalidationsBefore = targetBefore.value(QStringLiteral("invalidations")).toInt();
        const auto buildsBefore = targetBefore.value(QStringLiteral("builds")).toInt();
        changeGlow(15.0);
        pumpEvents(500);
        const QJsonObject afterEffectEdit = controller()->layerSurfaceCacheDiagnostics();
        const QJsonObject targetAfterEdit = entryFor(afterEffectEdit, effectLayerId_);
        if (targetAfterEdit.value(QStringLiteral("invalidations")).toInt() <= invalidationsBefore ||
            targetAfterEdit.value(QStringLiteral("builds")).toInt() <= buildsBefore)
            return fail(QStringLiteral("effect edit did not invalidate and rebuild the target cache"));

        changeOtherLayer();
        pumpEvents(500);
        const QJsonObject afterOtherEdit = controller()->layerSurfaceCacheDiagnostics();
        const QJsonObject targetAfterOther = entryFor(afterOtherEdit, effectLayerId_);
        if (targetAfterOther.value(QStringLiteral("invalidations")).toInt() !=
            targetAfterEdit.value(QStringLiteral("invalidations")).toInt())
            return fail(QStringLiteral("unrelated layer edit invalidated the target cache"));

        QJsonObject result{{QStringLiteral("passed"), true},
                           {QStringLiteral("initial"), initial},
                           {QStringLiteral("afterReuse"), afterReuse},
                           {QStringLiteral("afterEffectEdit"), afterEffectEdit},
                           {QStringLiteral("afterOtherLayerEdit"), afterOtherEdit}};
        return result;
    }

    QJsonObject changeGlow(double radius)
    {
        if (!glow_ || !effectLayer_) return fail(QStringLiteral("effect fixture unavailable"));
        if (!std::isfinite(radius) || radius < 1.0 || radius > 48.0)
            return fail(QStringLiteral("radius must be finite and in [1,48]"));
        glow_->setRadius(static_cast<float>(radius));
        effectLayer_->setDirty(Artifact::LayerDirtyFlag::Effect);
        controller()->markRenderDirty();
        return state();
    }

    QJsonObject changeOtherLayer()
    {
        if (!otherLayer_) return fail(QStringLiteral("secondary fixture layer unavailable"));
        auto image = otherLayer_->toQImage();
        image.invertPixels(QImage::InvertRgb);
        otherLayer_->setFromQImage(image);
        controller()->markRenderDirty();
        return state();
    }

    QJsonObject capture(int timeoutMs)
    {
        if (timeoutMs < 1 || timeoutMs > 60000) return fail(QStringLiteral("timeoutMs must be in [1,60000]"));
        const QImage image = artifactCaptureWindow(reinterpret_cast<ArtifactNativeWindowHandle>(winId()), nullptr, timeoutMs);
        if (image.isNull()) return fail(QStringLiteral("window capture failed"));
        QByteArray png;
        QBuffer buffer(&png);
        if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG"))
            return fail(QStringLiteral("PNG encoding failed"));
        QJsonObject result = state();
        result[QStringLiteral("captureBytes")] = png.size();
        result[QStringLiteral("mcpContent")] = QJsonArray{QJsonObject{
            {QStringLiteral("type"), QStringLiteral("image")},
            {QStringLiteral("mimeType"), QStringLiteral("image/png")},
            {QStringLiteral("data"), QString::fromLatin1(png.toBase64())}}};
        return result;
    }

private:
    static QJsonObject entryFor(const QJsonObject& diagnostics, const QString& layerId)
    {
        for (const auto value : diagnostics.value(QStringLiteral("entries")).toArray()) {
            const auto entry = value.toObject();
            if (entry.value(QStringLiteral("layerId")).toString() == layerId &&
                entry.value(QStringLiteral("slot")).toString() == QStringLiteral("full"))
                return entry;
        }
        return {};
    }

    static void pumpEvents(int durationMs)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < durationMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            QThread::msleep(5);
        }
    }

    Artifact::CompositionRenderController* controller() const
    {
        return editor_ ? editor_->renderController() : nullptr;
    }

    static QJsonObject fail(const QString& message)
    {
        return {{QStringLiteral("error"), message}};
    }

    void prepare()
    {
        auto* project = Artifact::ArtifactProjectService::instance();
        if (!project) { error_ = QStringLiteral("project service unavailable"); return; }
        Artifact::ArtifactCompositionInitParams params;
        params.setCompositionName(ArtifactCore::UniString(QStringLiteral("Layer Surface Cache Fixture")));
        params.setResolution(640, 360);
        params.setFrameRate(30.0);
        params.setDurationFrames(120);
        project->createComposition(params);
        const auto composition = project->currentComposition().lock();
        if (!composition) { error_ = QStringLiteral("composition creation failed"); return; }

        QImage background(640, 360, QImage::Format_RGBA8888);
        for (int y = 0; y < background.height(); ++y) {
            auto* row = background.scanLine(y);
            for (int x = 0; x < background.width(); ++x) {
                const int checker = ((x / 32) + (y / 32)) & 1;
                const int offset = x * 4;
                row[offset] = static_cast<uchar>(checker ? 36 : 52);
                row[offset + 1] = static_cast<uchar>(checker ? 54 : 76);
                row[offset + 2] = 84;
                row[offset + 3] = 255;
            }
        }
        auto base = ArtifactCore::makeShared<Artifact::ArtifactImageLayer>();
        base->setFromQImage(background);
        base->setLayerName(QStringLiteral("Unmodified Control"));
        composition->appendLayerTop(base);
        otherLayer_ = base;
        otherLayerId_ = base->id().toString();

        QImage foreground(240, 180, QImage::Format_RGBA8888);
        foreground.fill(Qt::transparent);
        for (int y = 0; y < foreground.height(); ++y) {
            auto* row = foreground.scanLine(y);
            for (int x = 0; x < foreground.width(); ++x) {
                const int dx = x - 120;
                const int dy = y - 90;
                if (dx * dx + dy * dy < 62 * 62) {
                    const int offset = x * 4;
                    row[offset] = 255;
                    row[offset + 1] = static_cast<uchar>(176 + (x % 70));
                    row[offset + 2] = 48;
                    row[offset + 3] = 255;
                }
            }
        }
        auto foregroundLayer = ArtifactCore::makeShared<Artifact::ArtifactImageLayer>();
        foregroundLayer->setFromQImage(foreground);
        foregroundLayer->setLayerName(QStringLiteral("Raster Cache Target"));
        foregroundLayer->setPosition3D({180.0f, 90.0f, 0.0f});
        glow_ = ArtifactCore::makeShared<Artifact::RasterizerGlowEffect>();
        glow_->setRadius(8.0f);
        glow_->setIntensity(1.4f);
        glow_->setEffectID(ArtifactCore::UniString(QStringLiteral("TestGlow")));
        foregroundLayer->addEffect(glow_);
        composition->appendLayerTop(foregroundLayer);
        effectLayer_ = foregroundLayer;
        effectLayerId_ = foregroundLayer->id().toString();
        editor_->setComposition(composition);
        ready_ = true;
        status_->setText(QStringLiteral("Fixture ready — MCP tools: cache.state / cache.rerender / cache.changeGlow / cache.changeOtherLayer / cache.capture"));
        if (controller()) controller()->setLayerSurfaceCacheDiagnosticsEnabled(true);
    }

    bool mcpMode_ = false;
    bool ready_ = false;
    QString error_;
    QString effectLayerId_;
    QString otherLayerId_;
    QLabel* status_ = nullptr;
    Artifact::ArtifactCompositionEditor* editor_ = nullptr;
    ArtifactCore::SharedPtr<Artifact::ArtifactImageLayer> effectLayer_;
    ArtifactCore::SharedPtr<Artifact::ArtifactImageLayer> otherLayer_;
    ArtifactCore::SharedPtr<Artifact::RasterizerGlowEffect> glow_;
};

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    bool mcpMode = false;
    for (int index = 1; index < argc; ++index) mcpMode |= QByteArray(argv[index]) == "--mcp-stdio";
    Artifact::ArtifactApplicationManager::instance();
    LayerSurfaceCacheWindow window(mcpMode);
    ArtifactTestMcpServer mcp(QStringLiteral("artifact-composition-layer-surface-cache-test"), QStringLiteral("1.0.0"));
    if (mcpMode) {
        const QJsonObject schema{{QStringLiteral("type"), QStringLiteral("object")}};
        bool ok = true;
        ok &= mcp.registerTool(QStringLiteral("cache.state"), QStringLiteral("Read fixture and layer surface cache counters"), schema,
            [&window](const QJsonObject&) { return window.state(); });
        ok &= mcp.registerTool(QStringLiteral("cache.rerender"), QStringLiteral("Request another frame render with unchanged inputs"), schema,
            [&window](const QJsonObject&) { return window.rerender(); });
        ok &= mcp.registerTool(QStringLiteral("cache.runRegression"), QStringLiteral("Run cache reuse and targeted invalidation assertions"), schema,
            [&window](const QJsonObject&) { return window.runRegression(); });
        ok &= mcp.registerTool(QStringLiteral("cache.changeGlow"), QStringLiteral("Change the target rasterizer effect radius"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{{QStringLiteral("radius"), QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}}}}},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("radius")}}},
            [&window](const QJsonObject& args) { return window.changeGlow(args.value(QStringLiteral("radius")).toDouble(std::numeric_limits<double>::quiet_NaN())); });
        ok &= mcp.registerTool(QStringLiteral("cache.changeOtherLayer"), QStringLiteral("Change the unrelated control layer"), schema,
            [&window](const QJsonObject&) { return window.changeOtherLayer(); });
        ok &= mcp.registerTool(QStringLiteral("cache.capture"), QStringLiteral("Capture the test window without activation"), schema,
            [&window](const QJsonObject&) { return window.capture(5000); });
        if (!ok || !mcp.start()) return 2;
        window.setAttribute(Qt::WA_ShowWithoutActivating);
    }
    const QScreen* leftmost = app.primaryScreen();
    for (const auto* screen : app.screens()) if (leftmost && screen->geometry().left() < leftmost->geometry().left()) leftmost = screen;
    if (leftmost) { window.resize(window.size().boundedTo(leftmost->availableGeometry().size())); window.move(leftmost->availableGeometry().topLeft()); }
    window.show();
    return app.exec();
}
