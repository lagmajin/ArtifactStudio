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
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimerEvent>
#include <QVBoxLayout>
#include <QVector>
#include <cstdio>
#include <exception>

import Artifact.Test.PreCompose;
import Artifact.Service.Project;
import Artifact.Project.Manager;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.InitParams;
import Artifact.Layer.Composition;
import Frame.Position;
import Composition.PreCompose;
import Undo.UndoManager;
import Utils.String.UniString;
import Utils.Id;
import Memory.SharedPtr;

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
        else if (name == "test.cycleRejected") {
            QJsonObject cycleResult;
            if (!comp) {
                cycleResult["error"] = QStringLiteral("No current composition");
                return cycleResult;
            }
            ArtifactCompositionLayerInitParams params;
            params.setName(UniString(QStringLiteral("Cycle Candidate")));
            service->addLayerToCurrentComposition(params, false);
            comp = service->currentComposition().lock();
            SharedPtr<ArtifactCompositionLayer> candidate;
            if (comp) for (const auto& layer : comp->allLayer()) {
                if (layer && layer->layerName() == QStringLiteral("Cycle Candidate")) {
                    candidate = dynamicPointerCast<ArtifactCompositionLayer>(layer);
                    break;
                }
            }
            if (!candidate) {
                cycleResult["error"] = QStringLiteral("Could not create composition-layer fixture");
                return cycleResult;
            }
            candidate->setCompositionId(comp->id());
            undo->clearHistory();
            const auto countBefore = comp->allLayer().size();
            const auto undoBefore = undo->undoCount();
            const bool rejected = !service->precomposeLayersWithUndo(
                {candidate->id()}, UniString(QStringLiteral("Rejected Cycle")),
                false, false, PrecomposeMode::MoveSelected);
            const auto outcome = service->lastPrecomposeOutcome();
            const bool unchanged = comp->allLayer().size() == countBefore &&
                comp->layerById(candidate->id()) &&
                !PreComposeManager::instance().isPrecomposeLayer(candidate->id()) &&
                outcome.precompLayerId.isNil() && outcome.childCompId.isNil() &&
                undo->undoCount() == undoBefore;
            const bool removed =
                service->removeLayerFromComposition(comp->id(), candidate->id());
            cycleResult["cycleRejected"] = rejected;
            cycleResult["stateUnchanged"] = unchanged;
            cycleResult["fixtureCleaned"] = removed;
            cycleResult["passed"] = rejected && unchanged && removed;
            if (!cycleResult["passed"].toBool()) reset();
            refresh();
            cycleResult["state"] = state();
            return cycleResult;
        } else if (name == "test.transitiveCycleRejected") {
            QJsonObject cycleResult;
            const auto root = service->currentComposition().lock();
            if (!root) {
                cycleResult["error"] = QStringLiteral("No root composition");
                return cycleResult;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2 || !service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Cycle Parent")), false, false,
                    PrecomposeMode::MoveSelected)) {
                cycleResult["error"] = QStringLiteral("Could not create cycle parent fixture");
                return cycleResult;
            }
            const auto outer = service->lastPrecomposeOutcome();
            service->changeCurrentComposition(outer.childCompId);
            const auto child = service->currentComposition().lock();
            if (!child) {
                service->changeCurrentComposition(rootId);
                cycleResult["error"] = QStringLiteral("Could not activate cycle child fixture");
                return cycleResult;
            }
            ArtifactCompositionLayerInitParams params;
            params.setName(UniString(QStringLiteral("Transitive Cycle Candidate")));
            service->addLayerToCurrentComposition(params, false);
            const auto candidateIds = namedLayerIds(child, {QStringLiteral("Transitive Cycle Candidate")});
            const auto candidate = candidateIds.isEmpty()
                ? SharedPtr<ArtifactCompositionLayer>{}
                : dynamicPointerCast<ArtifactCompositionLayer>(child->layerById(candidateIds.front()));
            if (!candidate) {
                service->changeCurrentComposition(rootId);
                cycleResult["error"] = QStringLiteral("Could not create composition-layer fixture");
                return cycleResult;
            }
            candidate->setCompositionId(rootId);
            const auto layerCountBefore = child->allLayer().size();
            const auto compositionCountBefore = ArtifactProjectManager::getInstance().compositionCount();
            const auto undoCountBefore = undo->undoCount();
            const bool rejected = !service->precomposeLayersWithUndo(
                {candidate->id()}, UniString(QStringLiteral("Rejected Transitive Cycle")),
                false, false, PrecomposeMode::MoveSelected);
            const auto outcome = service->lastPrecomposeOutcome();
            const bool unchanged = child->allLayer().size() == layerCountBefore &&
                child->layerById(candidate->id()) &&
                !PreComposeManager::instance().isPrecomposeLayer(candidate->id()) &&
                outcome.precompLayerId.isNil() && outcome.childCompId.isNil() &&
                ArtifactProjectManager::getInstance().compositionCount() == compositionCountBefore &&
                undo->undoCount() == undoCountBefore;
            const bool removed = service->removeLayerFromComposition(child->id(), candidate->id());
            service->changeCurrentComposition(rootId);
            childId_ = outer.childCompId;
            hasChild_ = true;
            cycleResult["transitiveCycleRejected"] = rejected;
            cycleResult["stateUnchanged"] = unchanged;
            cycleResult["fixtureCleaned"] = removed;
            cycleResult["passed"] = rejected && unchanged && removed;
            if (!cycleResult["passed"].toBool())
                cycleResult["error"] = QStringLiteral("Transitive cycle was accepted or mutated the project");
            refresh();
            cycleResult["state"] = state();
            return cycleResult;
        } else if (name == "test.externalParentRestored") {
            QJsonObject parentResult;
            const auto parent = service->currentComposition().lock();
            if (!parent) {
                parentResult["error"] = QStringLiteral("No current composition");
                return parentResult;
            }
            const auto idsA = namedLayerIds(parent, {QStringLiteral("A")});
            const auto idsB = namedLayerIds(parent, {QStringLiteral("B")});
            const auto idsParent = namedLayerIds(parent, {QStringLiteral("Untouched")});
            if (idsA.size() != 1 || idsB.size() != 1 || idsParent.size() != 1) {
                parentResult["error"] = QStringLiteral("External-parent fixture layers are missing");
                return parentResult;
            }
            const auto sourceId = idsA.front();
            const auto externalParentId = idsParent.front();
            const auto sourceLayer = parent->layerById(sourceId);
            if (!sourceLayer) {
                parentResult["error"] = QStringLiteral("External-parent source layer is missing");
                return parentResult;
            }
            sourceLayer->setParentById(externalParentId);
            const bool authoredParent = sourceLayer->parentLayerId() == externalParentId;
            const QVector<LayerID> selected{sourceId, idsB.front()};
            const bool precomposed = authoredParent && service->precomposeLayersWithUndo(
                selected, UniString(QStringLiteral("External Parent Precompose")), false, false,
                PrecomposeMode::MoveSelected);
            if (!precomposed) {
                parentResult["error"] = QStringLiteral("Could not precompose external-parent fixture");
                return parentResult;
            }
            const auto outcome = service->lastPrecomposeOutcome();
            const auto wrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                parent->layerById(outcome.precompLayerId));
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            const auto movedSource = child ? child->layerById(sourceId) : ArtifactAbstractLayerPtr{};
            const bool externalParentStored = wrapper &&
                wrapper->restoreExternalParent(sourceId) == externalParentId;
            const bool childParentCleared = movedSource && movedSource->parentLayerId().isNil();
            const bool unpacked = service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const auto restoredSource = service->currentComposition().lock()
                ? service->currentComposition().lock()->layerById(sourceId)
                : ArtifactAbstractLayerPtr{};
            const bool unpackRestoredParent = unpacked && restoredSource &&
                restoredSource->parentLayerId() == externalParentId;
            const bool undoAvailable = undo->canUndo();
            if (undoAvailable) undo->undo();
            const auto undoWrapper = service->currentComposition().lock()
                ? dynamicPointerCast<ArtifactCompositionLayer>(
                    service->currentComposition().lock()->layerById(outcome.precompLayerId))
                : SharedPtr<ArtifactCompositionLayer>{};
            const bool undoRestoredMetadata = undoAvailable && undoWrapper &&
                undoWrapper->restoreExternalParent(sourceId) == externalParentId;
            const bool redoAvailable = undo->canRedo();
            if (redoAvailable) undo->redo();
            const auto redoSource = service->currentComposition().lock()
                ? service->currentComposition().lock()->layerById(sourceId)
                : ArtifactAbstractLayerPtr{};
            const bool redoRestoredParent = redoAvailable && redoSource &&
                redoSource->parentLayerId() == externalParentId;
            parentResult["externalParentStoredOnWrapper"] = externalParentStored;
            parentResult["externalParentClearedInChild"] = childParentCleared;
            parentResult["unpackRestoredExternalParent"] = unpackRestoredParent;
            parentResult["undoRestoredMetadata"] = undoRestoredMetadata;
            parentResult["redoRestoredExternalParent"] = redoRestoredParent;
            parentResult["passed"] = externalParentStored && childParentCleared &&
                unpackRestoredParent && undoRestoredMetadata && redoRestoredParent;
            if (!parentResult["passed"].toBool())
                parentResult["error"] = QStringLiteral("External parent relationship changed across precompose/undo/redo");
            refresh();
            parentResult["state"] = state();
            return parentResult;
        }
        else if (name == "test.runRegression") {
            failures_ = runPreComposeTests();
            reset();
            ok = failures_ == 0;
        } else if (name == "test.nestedHierarchy") {
            QJsonObject nestedResult;
            const auto root = service->currentComposition().lock();
            if (!root) {
                nestedResult["error"] = QStringLiteral("No root composition");
                return nestedResult;
            }
            const auto rootId = root->id();
            const auto outerIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            if (outerIds.size() != 2 || !service->precomposeLayersWithUndo(
                    outerIds, UniString(QStringLiteral("Outer Precompose")), false, false,
                    PrecomposeMode::MoveSelected)) {
                nestedResult["error"] = QStringLiteral("Could not create outer precompose");
                return nestedResult;
            }
            const auto outer = service->lastPrecomposeOutcome();
            service->changeCurrentComposition(outer.childCompId);
            const auto middle = service->currentComposition().lock();
            const auto innerIds = namedLayerIds(middle, {QStringLiteral("A"), QStringLiteral("B")});
            if (!middle || innerIds.size() != 2 || !service->precomposeLayersWithUndo(
                    innerIds, UniString(QStringLiteral("Inner Precompose")), false, false,
                    PrecomposeMode::MoveSelected)) {
                nestedResult["error"] = QStringLiteral("Could not create inner precompose");
                service->changeCurrentComposition(rootId);
                return nestedResult;
            }
            const auto inner = service->lastPrecomposeOutcome();
            const auto hierarchy = PreComposeManager::instance().getCompositionHierarchy(inner.childCompId);
            const bool hierarchyOk = hierarchy.size() == 3 && hierarchy[0] == rootId &&
                hierarchy[1] == outer.childCompId && hierarchy[2] == inner.childCompId;
            const double rootTime = NestedTimeUtils::convertTime(5.0, inner.childCompId, rootId);
            const double nestedTime = NestedTimeUtils::convertTime(rootTime, rootId, inner.childCompId);
            const bool timeOk = rootTime == 45.0 && nestedTime == 5.0;
            service->changeCurrentComposition(rootId);
            childId_ = inner.childCompId;
            hasChild_ = true;
            nestedResult["hierarchy"] = QJsonArray{rootId.toString(), outer.childCompId.toString(),
                                                   inner.childCompId.toString()};
            nestedResult["hierarchyPreserved"] = hierarchyOk;
            nestedResult["grandchildTimeInRoot"] = rootTime;
            nestedResult["rootTimeInGrandchild"] = nestedTime;
            nestedResult["timeRoundTripPreserved"] = timeOk;
            nestedResult["passed"] = hierarchyOk && timeOk;
            if (!nestedResult["passed"].toBool())
                nestedResult["error"] = QStringLiteral("Nested hierarchy or time conversion changed unexpectedly");
            refresh();
            nestedResult["state"] = state();
            return nestedResult;
        } else if (name == "test.saveReload") {
            QJsonObject roundTrip;
            auto& projects = ArtifactProjectManager::getInstance();
            const auto parent = service->currentComposition().lock();
            const auto original = parent ? layers(parent, 0) : QJsonArray{};
            const auto originalParentId = parent ? parent->id() : CompositionID{};
            const auto originalChildId = childId_;
            QTemporaryDir tempDir;
            if (!parent || originalChildId.isNil() || !tempDir.isValid()) {
                roundTrip["error"] = QStringLiteral("Precompose fixture or temporary directory unavailable");
                return roundTrip;
            }
            const auto filePath = tempDir.filePath(QStringLiteral("precompose-roundtrip.artifact"));
            const auto save = projects.saveToFile(filePath);
            roundTrip["saved"] = save.success;
            if (!save.success) {
                roundTrip["error"] = save.errorMessage;
                return roundTrip;
            }
            const bool loaded = projects.loadFromFile(filePath);
            roundTrip["loaded"] = loaded;
            if (loaded) service->changeCurrentComposition(originalParentId);
            const auto reloadedParent = service->findComposition(originalParentId).ptr.lock();
            const auto reloadedChild = service->findComposition(originalChildId).ptr.lock();
            const auto reloaded = reloadedParent ? layers(reloadedParent, 0) : QJsonArray{};
            const bool parentChildLink = reloaded == original && bool(reloadedChild);
            roundTrip["parentChildLinkPreserved"] = parentChildLink;
            roundTrip["layerStatePreserved"] = reloaded == original;
            roundTrip["passed"] = save.success && loaded && parentChildLink;
            if (!roundTrip["passed"].toBool())
                roundTrip["error"] = QStringLiteral("Precompose project round-trip changed parent/child layer state");
            childId_ = originalChildId;
            hasChild_ = true;
            refresh();
            roundTrip["state"] = state();
            return roundTrip;
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
    static QVector<LayerID> namedLayerIds(const ArtifactCompositionPtr& comp,
                                           const QStringList& names)
    {
        QVector<LayerID> result;
        if (!comp) return result;
        for (const auto& layer : comp->allLayer())
            if (layer && names.contains(layer->layerName())) result.append(layer->id());
        return result;
    }

    static QJsonArray layers(const ArtifactCompositionPtr& comp, int depth)
    {
        QJsonArray result;
        if (depth > 8) return result;
        for (const auto& layer : comp->allLayer()) {
            if (!layer) continue;
            const auto position = layer->position3D();
            QJsonObject entry{{"id", layer->id().toString()}, {"name", layer->layerName()},
                {"startFrame", double(layer->startTime().framePosition())},
                {"inFrame", double(layer->inPoint().framePosition())},
                {"outFrame", double(layer->outPoint().framePosition())},
                {"opacity", layer->opacity()},
                {"position", QJsonArray{position.x, position.y, position.z}}};
            auto compositionLayer = dynamicPointerCast<ArtifactCompositionLayer>(layer);
            const auto childId = compositionLayer ? compositionLayer->sourceCompositionId()
                                                  : CompositionID{};
            if (!childId.isNil()) {
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
            for (const auto* name : {"test.state", "test.reset", "test.cycleRejected", "test.transitiveCycleRejected", "test.externalParentRestored", "test.precompose", "test.precomposeAll", "test.saveReload", "test.nestedHierarchy",
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
