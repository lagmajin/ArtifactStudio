#define NOMINMAX
#include <windows.h>
#include <io.h>
#include <QApplication>
#include <QByteArray>
#include <QBuffer>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QLocale>
#include <QPixmap>
#include <QScreen>
#include <QStringList>
#include <QTextEdit>
#include <QTimerEvent>
#include <QVBoxLayout>
#include <QVector>
#include <cstdio>
#include <exception>

import Artifact.Test.PreCompose;
import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.InitParams;
import Frame.Position;
import Composition.PreCompose;
import Undo.UndoManager;
import Utils.String.UniString;
import Utils.Id;

namespace {
using namespace Artifact;
using namespace ArtifactCore;
FILE* protocolOutput = stdout;

class PrecomposeWindow final : public QMainWindow {
public:
    explicit PrecomposeWindow(bool mcp) : mcp_(mcp)
    {
        setWindowTitle(QStringLiteral("Artifact Precompose Test"));
        resize(1000, 720);
        auto* central = new QWidget(this);
        auto* layout = new QVBoxLayout(central);
        layout->addWidget(new QLabel(QStringLiteral(
            "P: precompose A+B   A: move all   K: unpack/keep child   D: unpack/delete child\n"
            "Z: undo   Y: redo   R: new fixture   T: run regression suite"), central));
        text_ = new QTextEdit(central);
        text_->setReadOnly(true);
        text_->setFocusPolicy(Qt::NoFocus);
        layout->addWidget(text_);
        setCentralWidget(central);
        qApp->installEventFilter(this);
        reset();
        if (mcp_) startTimer(10);
    }
    ~PrecomposeWindow() override { qApp->removeEventFilter(this); }

    QJsonObject state() const
    {
        auto comp = ArtifactProjectService::instance()->currentComposition().lock();
        QJsonObject result{{"regressionFailures", failures_},
                           {"canUndo", UndoManager::instance()->canUndo()},
                           {"canRedo", UndoManager::instance()->canRedo()}};
        if (comp) {
            result["compositionId"] = comp->id().toString();
            result["layers"] = layers(comp, 0);
        }
        if (hasChild_) {
            result["lastChildId"] = childId_.toString();
            result["lastChildExists"] = bool(ArtifactProjectService::instance()->findComposition(childId_).ptr.lock());
        }
        return result;
    }

    QJsonObject action(const QString& name)
    {
        if (name == "test.capture") {
            QByteArray png;
            QBuffer buffer(&png);
            buffer.open(QIODevice::WriteOnly);
            if (!grab().save(&buffer, "PNG")) return {{"error", "Capture failed"}};
            return {{"capturePng", QString::fromLatin1(png.toBase64())}};
        }
        auto* service = ArtifactProjectService::instance();
        auto* undo = UndoManager::instance();
        auto comp = service->currentComposition().lock();
        bool ok = true;
        if (name == "test.state") return state();
        if (name == "test.reset") reset();
        else if (name == "test.runRegression") {
            failures_ = runPreComposeTests();
            reset();
            ok = failures_ == 0;
        } else if (name == "test.undo") {
            ok = undo->canUndo();
            if (ok) undo->undo();
        } else if (name == "test.redo") {
            ok = undo->canRedo();
            if (ok) undo->redo();
        } else if (name == "test.precompose" || name == "test.precomposeAll") {
            QVector<LayerID> ids;
            if (comp) for (const auto& layer : comp->allLayer()) {
                if (layer && (layer->layerName() == QStringLiteral("A") ||
                              layer->layerName() == QStringLiteral("B"))) ids.append(layer->id());
            }
            ok = !ids.isEmpty() && service->precomposeLayersWithUndo(ids,
                UniString(QStringLiteral("Nested A+B")), false, false,
                name == "test.precomposeAll" ? PrecomposeMode::MoveAllAttributes
                                              : PrecomposeMode::MoveSelected);
            if (ok) { childId_ = service->lastPrecomposeOutcome().childCompId; hasChild_ = true; }
        } else if (name == "test.unpackKeep" || name == "test.unpackDelete") {
            ok = false;
            if (comp) for (const auto& layer : comp->allLayer()) {
                if (layer && PreComposeManager::instance().isPrecomposeLayer(layer->id())) {
                    ok = service->unprecomposeLayerWithUndo(layer->id(), name == "test.unpackKeep");
                    break;
                }
            }
        } else return QJsonObject{{"error", "Unknown tool"}};
        refresh();
        auto result = state();
        if (!ok) result["error"] = QStringLiteral("Operation failed or unavailable: ") + name;
        return result;
    }

protected:
    bool eventFilter(QObject*, QEvent* event) override
    {
        if (event->type() != QEvent::KeyPress) return false;
        const auto* key = static_cast<QKeyEvent*>(event);
        QString tool;
        switch (key->key()) {
        case Qt::Key_P: tool = "test.precompose"; break;
        case Qt::Key_A: tool = "test.precomposeAll"; break;
        case Qt::Key_K: tool = "test.unpackKeep"; break;
        case Qt::Key_D: tool = "test.unpackDelete"; break;
        case Qt::Key_Z: tool = "test.undo"; break;
        case Qt::Key_Y: tool = "test.redo"; break;
        case Qt::Key_R: tool = "test.reset"; break;
        case Qt::Key_T: tool = "test.runRegression"; break;
        default: return false;
        }
        action(tool);
        return true;
    }

    void timerEvent(QTimerEvent*) override
    {
        DWORD available = 0;
        const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
        if (!PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr)) {
            qApp->quit();
            return;
        }
        if (!available) return;
        char chunk[4096];
        DWORD read = 0;
        if (!ReadFile(input, chunk, qMin<DWORD>(available, sizeof(chunk)), &read, nullptr)) return;
        pending_.append(chunk, read);
        if (pending_.size() > 1024 * 1024) { qApp->exit(2); return; }
        while (pending_.contains('\n')) {
            const auto end = pending_.indexOf('\n');
            const auto line = pending_.left(end);
            pending_.remove(0, end + 1);
            respond(line);
        }
    }

private:
    static QJsonArray layers(const ArtifactCompositionPtr& comp, int depth)
    {
        QJsonArray result;
        if (depth > 8) return result;
        auto& manager = PreComposeManager::instance();
        for (const auto& layer : comp->allLayer()) {
            if (!layer) continue;
            const auto position = layer->position3D();
            QJsonObject entry{{"id", layer->id().toString()}, {"name", layer->layerName()},
                {"startFrame", double(layer->startTime().framePosition())},
                {"inFrame", double(layer->inPoint().framePosition())},
                {"outFrame", double(layer->outPoint().framePosition())},
                {"opacity", layer->opacity()},
                {"position", QJsonArray{position.x, position.y, position.z}}};
            if (manager.isPrecomposeLayer(layer->id())) {
                const auto childId = manager.getSourceCompositionId(layer->id());
                entry["childId"] = childId.toString();
                auto child = ArtifactProjectService::instance()->findComposition(childId).ptr.lock();
                entry["childExists"] = bool(child);
                if (child) entry["children"] = layers(child, depth + 1);
            }
            result.append(entry);
        }
        return result;
    }
    void reset()
    {
        auto* service = ArtifactProjectService::instance();
        hasChild_ = false;
        UndoManager::instance()->clearHistory();
        service->createComposition(UniString(QStringLiteral("Precompose Fixture")));
        for (const auto* name : {"A", "B", "Untouched"})
            service->addLayerToCurrentComposition(ArtifactNullLayerInitParams(QString::fromLatin1(name)), false);
        auto comp = service->currentComposition().lock();
        if (comp) for (const auto& layer : comp->allLayer()) {
            if (!layer) continue;
            const auto name = layer->layerName();
            if (name == QStringLiteral("A")) {
                layer->setPosition3D({12.0f, -8.0f, 3.0f});
                layer->setOpacity(0.42f);
                layer->setInPoint(FramePosition(40));
                layer->setOutPoint(FramePosition(200));
                layer->setStartTime(FramePosition(7));
            } else if (name == QStringLiteral("B")) {
                layer->setInPoint(FramePosition(70));
                layer->setOutPoint(FramePosition(160));
                layer->setStartTime(FramePosition(9));
            } else if (name == QStringLiteral("Untouched")) {
                layer->setInPoint(FramePosition(25));
                layer->setOutPoint(FramePosition(220));
                layer->setStartTime(FramePosition(3));
            }
        }
        UndoManager::instance()->clearHistory();
        refresh();
    }
    void refresh() { text_->setPlainText(QString::fromUtf8(QJsonDocument(state()).toJson())); }
    void respond(const QByteArray& line)
    {
        const auto request = QJsonDocument::fromJson(line).object();
        if (!request.contains("id")) return;
        const auto method = request["method"].toString();
        QJsonObject response{{"jsonrpc", "2.0"}, {"id", request["id"]}};
        if (method == "initialize") {
            response["result"] = QJsonObject{{"protocolVersion", "2024-11-05"},
                {"capabilities", QJsonObject{{"tools", QJsonObject{}}}},
                {"serverInfo", QJsonObject{{"name", "artifact-precompose-test"}, {"version", "1.0.0"}}}};
        } else if (method == "ping") response["result"] = QJsonObject{};
        else if (method == "tools/list") {
            QJsonArray tools;
            for (const auto* name : {"test.state", "test.reset", "test.precompose", "test.precomposeAll",
                    "test.unpackKeep", "test.unpackDelete", "test.undo", "test.redo", "test.runRegression", "test.capture"})
                tools.append(QJsonObject{{"name", name}, {"description", name},
                    {"inputSchema", QJsonObject{{"type", "object"}, {"additionalProperties", false}}}});
            response["result"] = QJsonObject{{"tools", tools}};
        } else if (method == "tools/call") {
            QJsonObject result;
            const auto params = request["params"].toObject();
            try {
                if (!params["name"].isString() ||
                    (params.contains("arguments") && (!params["arguments"].isObject() ||
                     !params["arguments"].toObject().isEmpty())))
                    result = {{"error", "Expected a tool name and empty object arguments"}};
                else result = action(params["name"].toString());
            }
            catch (const std::exception& error) { result = QJsonObject{{"error", error.what()}}; }
            if (result.contains("capturePng")) response["result"] = QJsonObject{
                {"content", QJsonArray{QJsonObject{{"type", "image"},
                    {"mimeType", "image/png"}, {"data", result["capturePng"]}}}}};
            else response["result"] = QJsonObject{{"isError", result.contains("error")},
                {"content", QJsonArray{QJsonObject{{"type", "text"},
                    {"text", QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact))}}}}};
        } else response["error"] = QJsonObject{{"code", -32601}, {"message", "Method not found"}};
        const auto bytes = QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n';
        std::fwrite(bytes.constData(), 1, bytes.size(), protocolOutput);
        std::fflush(protocolOutput);
    }
    QTextEdit* text_ = nullptr;
    bool mcp_ = false;
    int failures_ = -1;
    QByteArray pending_;
    CompositionID childId_;
    bool hasChild_ = false;
};
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    const int languageIndex = app.arguments().indexOf(QStringLiteral("--lang"));
    if (languageIndex >= 0 && languageIndex + 1 < app.arguments().size())
        QLocale::setDefault(QLocale(app.arguments()[languageIndex + 1]));
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& message) {
        const auto bytes = message.toUtf8();
        std::fwrite(bytes.constData(), 1, bytes.size(), stderr);
        std::fputc('\n', stderr);
    });
    if (app.arguments().contains(QStringLiteral("--mcp-stdio"))) {
        const int outputDescriptor = _dup(_fileno(stdout));
        if (outputDescriptor < 0) return 2;
        protocolOutput = _fdopen(outputDescriptor, "wb");
        if (!protocolOutput || _dup2(_fileno(stderr), _fileno(stdout)) != 0) return 2;
    }
    if (app.arguments().contains(QStringLiteral("--check"))) return Artifact::runPreComposeTests() ? 1 : 0;
    PrecomposeWindow window(app.arguments().contains(QStringLiteral("--mcp-stdio")));
    window.setAttribute(Qt::WA_ShowWithoutActivating);
    const QScreen* leftmost = app.primaryScreen();
    for (const auto* screen : app.screens())
        if (screen->geometry().left() < leftmost->geometry().left()) leftmost = screen;
    if (leftmost) {
        window.resize(window.size().boundedTo(leftmost->availableGeometry().size()));
        window.move(leftmost->availableGeometry().topLeft());
    }
    window.show();
    return app.exec();
}
