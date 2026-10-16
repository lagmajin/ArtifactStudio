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
#include <QPointF>
#include <QScreen>
#include <QSize>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimerEvent>
#include <QVariant>
#include <QVector3D>
#include <QVBoxLayout>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <vector>

import Artifact.Test.PreCompose;
import Artifact.Service.Project;
import Artifact.Project.Manager;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Layer.Abstract;
import Artifact.Layer.InitParams;
import Artifact.Layer.Composition;
import Artifact.Layers.Selection.Manager;
import Artifact.Effect.Abstract;
import BrightnessEffect;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Artifact.Layer.Matte;
import Color.Float;
import Property.Abstract;
import Property.ExposedPropertyRegistry;
import Math.Interpolate;
import Frame.Position;
import Frame.Rate;
import Frame.Range;
import Time.Rational;
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
            const auto size = comp->settings().compositionSize();
            const auto color = comp->backgroundColor();
            result["resolution"] = QJsonArray{size.width(), size.height()};
            result["frameRate"] = comp->frameRate().framerate();
            result["frameRangeDuration"] = comp->frameRange().duration();
            result["backgroundColor"] = QJsonArray{color.r(), color.g(), color.b(), color.a()};
        }
        if (auto* selection = ArtifactLayerSelectionManager::instance()) {
            QJsonObject selectionState;
            const auto active = selection->activeComposition();
            const auto current = selection->currentLayer();
            if (active) selectionState["activeCompositionId"] = active->id().toString();
            if (current) selectionState["currentLayerId"] = current->id().toString();
            QJsonArray selectedIds;
            for (const auto& layer : selection->selectedLayersInOrder())
                if (layer) selectedIds.append(layer->id().toString());
            selectionState["selectedLayerIds"] = selectedIds;
            result["selection"] = selectionState;
        }
        if (hasChild_) {
            result["lastChildId"] = childId_.toString();
            result["lastChildExists"] = bool(ArtifactProjectService::instance()->findComposition(childId_).ptr.lock());
        }
        return result;
    }

    QJsonObject action(const QString& name)
    {
        if (name == "test.guiKeyboardRoundTrip") {
            reset();
            const auto originalLayers = state()["layers"].toArray();
            const auto sendKey = [this](int key) {
                QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
                QApplication::sendEvent(this, &event);
            };
            const auto originalState = state();
            const auto rootCompositionId = originalState["compositionId"].toString();
            const auto selectedLayerName = [](const QJsonObject& snapshot,
                                              const QString& rootId) {
                const auto selection = snapshot["selection"].toObject();
                if (snapshot["compositionId"].toString() != rootId ||
                    selection["activeCompositionId"].toString() != rootId)
                    return QString{};
                const auto selectedIds = selection["selectedLayerIds"].toArray();
                if (selectedIds.size() != 1) return QString{};
                for (const auto& value : snapshot["layers"].toArray()) {
                    const auto layer = value.toObject();
                    if (layer["id"].toString() == selectedIds[0].toString() &&
                        selection["currentLayerId"].toString() == selectedIds[0].toString())
                        return layer["name"].toString();
                }
                return QString{};
            };
            const auto allLayersMoved = [](const QJsonObject& snapshot) {
                const auto rootLayers = snapshot["layers"].toArray();
                if (rootLayers.size() != 1) return false;
                const auto childLayers = rootLayers[0].toObject()["children"].toArray();
                bool foundA = false;
                bool foundB = false;
                bool foundUntouched = false;
                bool timingPreserved = true;
                QStringList childNames;
                for (const auto& value : childLayers) {
                    const auto layer = value.toObject();
                    const auto name = layer["name"].toString();
                    childNames.append(name);
                    foundA = foundA || name == QStringLiteral("A");
                    foundB = foundB || name == QStringLiteral("B");
                    foundUntouched = foundUntouched || name == QStringLiteral("Untouched");
                    if (name == QStringLiteral("A"))
                        timingPreserved = timingPreserved && layer["inFrame"].toInt() == 40 &&
                            layer["outFrame"].toInt() == 200 && layer["startFrame"].toInt() == 7;
                    else if (name == QStringLiteral("B"))
                        timingPreserved = timingPreserved && layer["inFrame"].toInt() == 70 &&
                            layer["outFrame"].toInt() == 160 && layer["startFrame"].toInt() == 9;
                    else if (name == QStringLiteral("Untouched"))
                        timingPreserved = timingPreserved && layer["inFrame"].toInt() == 25 &&
                            layer["outFrame"].toInt() == 220 && layer["startFrame"].toInt() == 3;
                }
                return childLayers.size() == 3 && foundA && foundB && foundUntouched &&
                    childNames == QStringList{QStringLiteral("A"), QStringLiteral("B"),
                                              QStringLiteral("Untouched")} && timingPreserved;
            };
            const auto childSettingsCopied = [](const QJsonObject& snapshot) {
                const auto parentResolution = snapshot["resolution"].toArray();
                const auto parentColor = snapshot["backgroundColor"].toArray();
                if (parentResolution != QJsonArray{1280, 720} ||
                    std::abs(snapshot["frameRate"].toDouble() - 24.0) > 1.0e-6 ||
                    snapshot["frameRangeDuration"].toInt() != 300 || parentColor.size() != 4 ||
                    std::abs(parentColor[0].toDouble() - 0.125) > 1.0e-6 ||
                    std::abs(parentColor[1].toDouble() - 0.25) > 1.0e-6 ||
                    std::abs(parentColor[2].toDouble() - 0.5) > 1.0e-6 ||
                    std::abs(parentColor[3].toDouble() - 1.0) > 1.0e-6)
                    return false;
                for (const auto& value : snapshot["layers"].toArray()) {
                    const auto child = value.toObject()["childComposition"].toObject();
                    if (child.isEmpty()) continue;
                    const auto childResolution = child["resolution"].toArray();
                    const auto childColor = child["backgroundColor"].toArray();
                    if (childResolution == parentResolution &&
                        std::abs(child["frameRate"].toDouble() - snapshot["frameRate"].toDouble()) <= 1.0e-6 &&
                        child["frameRangeDuration"].toInt() == snapshot["frameRangeDuration"].toInt() &&
                        childColor.size() == 4 &&
                        std::abs(childColor[0].toDouble() - parentColor[0].toDouble()) <= 1.0e-6 &&
                        std::abs(childColor[1].toDouble() - parentColor[1].toDouble()) <= 1.0e-6 &&
                        std::abs(childColor[2].toDouble() - parentColor[2].toDouble()) <= 1.0e-6 &&
                        std::abs(childColor[3].toDouble() - parentColor[3].toDouble()) <= 1.0e-6)
                        return true;
                }
                return false;
            };
            sendKey(Qt::Key_P);
            const auto afterPrecompose = state();
            const bool precomposedByKey = afterPrecompose["canUndo"].toBool() &&
                afterPrecompose["lastChildExists"].toBool() &&
                afterPrecompose["layers"].toArray().size() == 2;
            const bool selectedSettingsCopiedByKey = precomposedByKey &&
                childSettingsCopied(afterPrecompose);
            const bool precomposeSelectionByKey =
                selectedLayerName(afterPrecompose, rootCompositionId) == QStringLiteral("Nested A+B");
            sendKey(Qt::Key_Z);
            const auto afterUndo = state();
            const bool undoneByKey = afterUndo["canRedo"].toBool() &&
                !afterUndo["lastChildExists"].toBool() &&
                afterUndo["layers"].toArray() == originalLayers;
            sendKey(Qt::Key_Y);
            const auto afterRedo = state();
            const bool redoneByKey = afterRedo["canUndo"].toBool() &&
                !afterRedo["canRedo"].toBool() &&
                afterRedo["lastChildExists"].toBool() &&
                afterRedo["layers"].toArray().size() == 2;
            sendKey(Qt::Key_K);
            const auto afterKeepUnpack = state();
            const bool unpackedKeepByKey = afterKeepUnpack["canUndo"].toBool() &&
                afterKeepUnpack["lastChildExists"].toBool() &&
                afterKeepUnpack["layers"].toArray() == originalLayers &&
                selectedLayerName(afterKeepUnpack, rootCompositionId) == QStringLiteral("A");
            sendKey(Qt::Key_Z);
            const auto afterKeepUndo = state();
            const bool keepUndoByKey = afterKeepUndo["canRedo"].toBool() &&
                afterKeepUndo["lastChildExists"].toBool() &&
                afterKeepUndo["layers"].toArray().size() == 2 &&
                selectedLayerName(afterKeepUndo, rootCompositionId) == QStringLiteral("Nested A+B");
            sendKey(Qt::Key_Y);
            const auto afterKeepRedo = state();
            const bool keepRedoByKey = afterKeepRedo["lastChildExists"].toBool() &&
                afterKeepRedo["layers"].toArray() == originalLayers &&
                selectedLayerName(afterKeepRedo, rootCompositionId) == QStringLiteral("A");
            sendKey(Qt::Key_P);
            const auto beforeDeleteUnpack = state();
            const bool precomposedForDeleteByKey = beforeDeleteUnpack["lastChildExists"].toBool() &&
                beforeDeleteUnpack["layers"].toArray().size() == 2;
            sendKey(Qt::Key_D);
            const auto afterDeleteUnpack = state();
            const bool unpackedDeleteByKey = precomposedForDeleteByKey &&
                afterDeleteUnpack["canUndo"].toBool() &&
                !afterDeleteUnpack["lastChildExists"].toBool() &&
                afterDeleteUnpack["layers"].toArray() == originalLayers &&
                selectedLayerName(afterDeleteUnpack, rootCompositionId) == QStringLiteral("A");
            sendKey(Qt::Key_Z);
            const auto afterDeleteUndo = state();
            const bool deleteUndoByKey = afterDeleteUndo["canRedo"].toBool() &&
                afterDeleteUndo["lastChildExists"].toBool() &&
                afterDeleteUndo["layers"].toArray().size() == 2 &&
                selectedLayerName(afterDeleteUndo, rootCompositionId) == QStringLiteral("Nested A+B");
            sendKey(Qt::Key_Y);
            const auto afterDeleteRedo = state();
            const bool deleteRedoByKey = afterDeleteRedo["canUndo"].toBool() &&
                !afterDeleteRedo["canRedo"].toBool() &&
                !afterDeleteRedo["lastChildExists"].toBool() &&
                afterDeleteRedo["layers"].toArray() == originalLayers &&
                selectedLayerName(afterDeleteRedo, rootCompositionId) == QStringLiteral("A");
            sendKey(Qt::Key_A);
            const auto afterMoveAll = state();
            const bool allMovedByKey = afterMoveAll["canUndo"].toBool() &&
                afterMoveAll["lastChildExists"].toBool() && allLayersMoved(afterMoveAll) &&
                selectedLayerName(afterMoveAll, rootCompositionId) == QStringLiteral("Nested A+B");
            const bool allSettingsCopiedByKey = allMovedByKey && childSettingsCopied(afterMoveAll);
            sendKey(Qt::Key_Z);
            const auto afterMoveAllUndo = state();
            const bool allUndoByKey = afterMoveAllUndo["canRedo"].toBool() &&
                !afterMoveAllUndo["lastChildExists"].toBool() &&
                afterMoveAllUndo["layers"].toArray() == originalLayers;
            sendKey(Qt::Key_Y);
            const auto afterMoveAllRedo = state();
            const bool allRedoByKey = afterMoveAllRedo["canUndo"].toBool() &&
                !afterMoveAllRedo["canRedo"].toBool() &&
                afterMoveAllRedo["lastChildExists"].toBool() && allLayersMoved(afterMoveAllRedo) &&
                selectedLayerName(afterMoveAllRedo, rootCompositionId) == QStringLiteral("Nested A+B");
            sendKey(Qt::Key_R);
            const auto afterReset = state();
            const auto resetLayers = afterReset["layers"].toArray();
            bool resetHasExpectedNames = resetLayers.size() == 3;
            QStringList resetNames;
            for (const auto& value : resetLayers) resetNames.append(value.toObject()["name"].toString());
            resetNames.sort();
            resetHasExpectedNames = resetHasExpectedNames && resetNames ==
                QStringList{QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("Untouched")};
            const bool resetByKey = resetHasExpectedNames && !afterReset["canUndo"].toBool() &&
                !afterReset["canRedo"].toBool() && !afterReset.contains("lastChildExists");
            const auto resetSelection = afterReset["selection"].toObject();
            const bool resetSelectionByKey =
                resetSelection["activeCompositionId"].toString() ==
                    afterReset["compositionId"].toString() &&
                resetSelection["selectedLayerIds"].toArray().isEmpty() &&
                !resetSelection.contains("currentLayerId");
            QJsonObject result{{"precomposedByKey", precomposedByKey},
                               {"undoneByKey", undoneByKey},
                               {"redoneByKey", redoneByKey},
                               {"selectedSettingsCopiedByKey", selectedSettingsCopiedByKey},
                               {"precomposeSelectionByKey", precomposeSelectionByKey},
                               {"unpackedKeepByKey", unpackedKeepByKey},
                               {"keepUndoByKey", keepUndoByKey},
                               {"keepRedoByKey", keepRedoByKey},
                               {"precomposedForDeleteByKey", precomposedForDeleteByKey},
                               {"unpackedDeleteByKey", unpackedDeleteByKey},
                               {"deleteUndoByKey", deleteUndoByKey},
                               {"deleteRedoByKey", deleteRedoByKey},
                               {"allMovedByKey", allMovedByKey},
                               {"allSettingsCopiedByKey", allSettingsCopiedByKey},
                               {"allUndoByKey", allUndoByKey},
                               {"allRedoByKey", allRedoByKey},
                               {"resetByKey", resetByKey},
                               {"resetSelectionByKey", resetSelectionByKey},
                               {"passed", precomposedByKey && selectedSettingsCopiedByKey &&
                                   precomposeSelectionByKey &&
                                   undoneByKey && redoneByKey &&
                                   unpackedKeepByKey && keepUndoByKey && keepRedoByKey &&
                                   precomposedForDeleteByKey && unpackedDeleteByKey &&
                                   deleteUndoByKey && deleteRedoByKey && allMovedByKey &&
                                   allSettingsCopiedByKey &&
                                   allUndoByKey && allRedoByKey && resetByKey && resetSelectionByKey},
                               {"state", afterReset}};
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("GUI P/K/D/Z/Y keyboard routing failed to round-trip precompose and unprecompose");
            return result;
        }
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
            auto* selection = ArtifactLayerSelectionManager::instance();
            if (!selection) {
                cycleResult["error"] = QStringLiteral("Layer-selection service is unavailable");
                return cycleResult;
            }
            const auto expectedCompositionId = comp->id();
            service->selectLayer(candidate->id());
            const auto selectionUnchanged = [&]() {
                const auto active = selection->activeComposition();
                const auto current = selection->currentLayer();
                const auto selected = selection->selectedLayersInOrder();
                return active && active->id() == expectedCompositionId && current &&
                    current->id() == candidate->id() && selected.size() == 1 &&
                    selected.front() && selected.front()->id() == candidate->id();
            };
            const auto countBefore = comp->allLayer().size();
            const auto undoBefore = undo->undoCount();
            const auto redoBefore = undo->redoCount();
            const bool rejected = !service->precomposeLayersWithUndo(
                {candidate->id()}, UniString(QStringLiteral("Rejected Cycle")),
                false, false, PrecomposeMode::MoveSelected);
            const auto outcome = service->lastPrecomposeOutcome();
            const bool unchanged = comp->allLayer().size() == countBefore &&
                comp->layerById(candidate->id()) &&
                !PreComposeManager::instance().isPrecomposeLayer(candidate->id()) &&
                outcome.precompLayerId.isNil() && outcome.childCompId.isNil() &&
                undo->undoCount() == undoBefore && undo->redoCount() == redoBefore &&
                selectionUnchanged();
            const bool selectionPreserved = selectionUnchanged();
            const bool removed =
                service->removeLayerFromComposition(comp->id(), candidate->id());
            cycleResult["cycleRejected"] = rejected;
            cycleResult["stateUnchanged"] = unchanged;
            cycleResult["selectionPreserved"] = selectionPreserved;
            cycleResult["fixtureCleaned"] = removed;
            cycleResult["passed"] = rejected && unchanged && selectionPreserved && removed;
            if (!cycleResult["passed"].toBool()) reset();
            refresh();
            cycleResult["state"] = state();
            return cycleResult;
        } else if (name == "test.invalidRequestsAtomic") {
            QJsonObject invalidResult;
            if (!comp) {
                invalidResult["error"] = QStringLiteral("No current composition");
                return invalidResult;
            }
            const auto originalLayers = layers(comp, 0);
            const auto originalLayerCount = comp->allLayer().size();
            const auto originalCompositionCount = ArtifactProjectManager::getInstance().compositionCount();
            const auto sourceIds = namedLayerIds(comp, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2) {
                invalidResult["error"] = QStringLiteral("Invalid-request fixture is missing A/B layers");
                return invalidResult;
            }
            const bool seedRedoHistory = service->precomposeLayersWithUndo(
                sourceIds, UniString(QStringLiteral("Redo Preservation Fixture")), false, false,
                PrecomposeMode::MoveSelected);
            const auto seedOutcome = service->lastPrecomposeOutcome();
            const bool seedUndoAvailable = undo->canUndo();
            if (seedRedoHistory && seedUndoAvailable) undo->undo();
            const bool redoSeedReady = seedRedoHistory && seedUndoAvailable && undo->redoCount() > 0 &&
                comp->allLayer().size() == originalLayerCount && layers(comp, 0) == originalLayers &&
                ArtifactProjectManager::getInstance().compositionCount() == originalCompositionCount;
            if (!redoSeedReady) {
                invalidResult["error"] = QStringLiteral("Could not seed a reversible precompose history fixture");
                return invalidResult;
            }
            const auto beforeLayers = layers(comp, 0);
            const auto beforeLayerCount = comp->allLayer().size();
            const auto beforeCompositionCount = ArtifactProjectManager::getInstance().compositionCount();
            const auto beforeUndoCount = undo->undoCount();
            const auto beforeRedoCount = undo->redoCount();
            const auto untouchedIds = namedLayerIds(comp, {QStringLiteral("Untouched")});
            auto* selection = ArtifactLayerSelectionManager::instance();
            if (untouchedIds.size() != 1 || !selection) {
                invalidResult["error"] = QStringLiteral("Invalid-request selection fixture is unavailable");
                return invalidResult;
            }
            const auto expectedSelectionId = untouchedIds.front();
            const auto expectedCompositionId = comp->id();
            service->selectLayer(expectedSelectionId);
            const auto selectionUnchanged = [&]() {
                const auto active = selection->activeComposition();
                const auto current = selection->currentLayer();
                const auto selected = selection->selectedLayersInOrder();
                return active && active->id() == expectedCompositionId && current &&
                    current->id() == expectedSelectionId && selected.size() == 1 &&
                    selected.front() && selected.front()->id() == expectedSelectionId;
            };
            const bool blankNameRejected = !service->precomposeLayersWithUndo(
                sourceIds, UniString(QStringLiteral("   ")), false, false,
                PrecomposeMode::MoveSelected);
            const auto blankNameOutcome = service->lastPrecomposeOutcome();
            const bool blankNameAtomic = comp->allLayer().size() == beforeLayerCount &&
                layers(comp, 0) == beforeLayers &&
                ArtifactProjectManager::getInstance().compositionCount() == beforeCompositionCount &&
                undo->undoCount() == beforeUndoCount && undo->redoCount() == beforeRedoCount &&
                blankNameOutcome.precompLayerId.isNil() &&
                blankNameOutcome.childCompId.isNil() && selectionUnchanged();
            const bool invalidSelectionRejected = !service->precomposeLayersWithUndo(
                {LayerID()}, UniString(QStringLiteral("Invalid Selection")), false, false,
                PrecomposeMode::MoveSelected);
            const auto invalidSelectionOutcome = service->lastPrecomposeOutcome();
            const bool invalidSelectionAtomic = comp->allLayer().size() == beforeLayerCount &&
                layers(comp, 0) == beforeLayers &&
                ArtifactProjectManager::getInstance().compositionCount() == beforeCompositionCount &&
                undo->undoCount() == beforeUndoCount && undo->redoCount() == beforeRedoCount &&
                invalidSelectionOutcome.precompLayerId.isNil() &&
                invalidSelectionOutcome.childCompId.isNil() && selectionUnchanged();
            const bool emptySelectionRejected = !service->precomposeLayersWithUndo(
                QVector<LayerID>{}, UniString(QStringLiteral("Empty Selection")), false, false,
                PrecomposeMode::MoveSelected);
            const auto emptySelectionOutcome = service->lastPrecomposeOutcome();
            const bool emptySelectionAtomic = comp->allLayer().size() == beforeLayerCount &&
                layers(comp, 0) == beforeLayers &&
                ArtifactProjectManager::getInstance().compositionCount() == beforeCompositionCount &&
                undo->undoCount() == beforeUndoCount && undo->redoCount() == beforeRedoCount &&
                emptySelectionOutcome.precompLayerId.isNil() &&
                emptySelectionOutcome.childCompId.isNil() && selectionUnchanged();
            const bool emptyPrecompIdRejected = !service->unprecomposeLayerWithUndo(LayerID(), true);
            const bool emptyPrecompIdAtomic = comp->allLayer().size() == beforeLayerCount &&
                layers(comp, 0) == beforeLayers &&
                ArtifactProjectManager::getInstance().compositionCount() == beforeCompositionCount &&
                undo->undoCount() == beforeUndoCount && undo->redoCount() == beforeRedoCount &&
                service->lastUnprecomposePrecompLayerId().isNil() &&
                service->lastUnprecomposeChildCompId().isNil() &&
                service->lastUnprecomposeMovedLayerIds().isEmpty() && selectionUnchanged();
            const bool ordinaryLayerRejected = !service->unprecomposeLayerWithUndo(sourceIds.front(), true);
            const bool ordinaryLayerAtomic = comp->allLayer().size() == beforeLayerCount &&
                layers(comp, 0) == beforeLayers &&
                ArtifactProjectManager::getInstance().compositionCount() == beforeCompositionCount &&
                undo->undoCount() == beforeUndoCount && undo->redoCount() == beforeRedoCount &&
                service->lastUnprecomposePrecompLayerId().isNil() &&
                service->lastUnprecomposeChildCompId().isNil() &&
                service->lastUnprecomposeMovedLayerIds().isEmpty() && selectionUnchanged();
            invalidResult["blankNameRejected"] = blankNameRejected;
            invalidResult["blankNameAtomic"] = blankNameAtomic;
            invalidResult["invalidSelectionRejected"] = invalidSelectionRejected;
            invalidResult["invalidSelectionAtomic"] = invalidSelectionAtomic;
            invalidResult["emptySelectionRejected"] = emptySelectionRejected;
            invalidResult["emptySelectionAtomic"] = emptySelectionAtomic;
            invalidResult["emptyPrecompIdRejected"] = emptyPrecompIdRejected;
            invalidResult["emptyPrecompIdAtomic"] = emptyPrecompIdAtomic;
            invalidResult["ordinaryLayerRejected"] = ordinaryLayerRejected;
            invalidResult["ordinaryLayerAtomic"] = ordinaryLayerAtomic;
            const bool selectionPreserved = selectionUnchanged();
            invalidResult["selectionPreserved"] = selectionPreserved;
            const bool redoHistoryPreserved = undo->redoCount() == beforeRedoCount;
            if (undo->canRedo()) undo->redo();
            const bool preservedRedoExecuted = redoHistoryPreserved &&
                comp->id() == expectedCompositionId && comp->layerById(seedOutcome.precompLayerId) &&
                undo->redoCount() + 1 == beforeRedoCount && undo->undoCount() == beforeUndoCount + 1;
            invalidResult["redoHistoryPreserved"] = redoHistoryPreserved;
            invalidResult["preservedRedoExecuted"] = preservedRedoExecuted;
            invalidResult["passed"] = blankNameRejected && blankNameAtomic &&
                invalidSelectionRejected && invalidSelectionAtomic && emptySelectionRejected &&
                emptySelectionAtomic && emptyPrecompIdRejected && emptyPrecompIdAtomic &&
                ordinaryLayerRejected && ordinaryLayerAtomic && selectionPreserved &&
                redoHistoryPreserved && preservedRedoExecuted;
            if (!invalidResult["passed"].toBool())
                invalidResult["error"] = QStringLiteral("Rejected precompose/unprecompose input mutated project state or history");
            refresh();
            invalidResult["state"] = state();
            return invalidResult;
        } else if (name == "test.undoBudgetRejectsStructureChanges") {
            QJsonObject budgetResult;
            if (!comp) {
                budgetResult["error"] = QStringLiteral("No current composition");
                return budgetResult;
            }
            auto& projects = ArtifactProjectManager::getInstance();
            auto& manager = PreComposeManager::instance();
            undo->clearHistory();
            const auto originalBudget = undo->budget();
            auto rejectingBudget = originalBudget;
            rejectingBudget.maxSingleEntryBytes = 0;
            const auto rootId = comp->id();
            const auto sourceIds = namedLayerIds(comp, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2) {
                budgetResult["error"] = QStringLiteral("Undo-budget fixture is missing A/B layers");
                return budgetResult;
            }
            const auto initialLayers = layers(comp, 0);
            const auto initialCompositionCount = projects.compositionCount();
            undo->setBudget(rejectingBudget);
            const bool precomposeRejected = !service->precomposeLayersWithUndo(
                sourceIds, UniString(QStringLiteral("Budget Rejected Precompose")), false, false,
                PrecomposeMode::MoveSelected);
            const auto rejectedPrecompose = service->lastPrecomposeOutcome();
            const bool precomposeBudgetAtomic = precomposeRejected &&
                layers(comp, 0) == initialLayers && projects.compositionCount() == initialCompositionCount &&
                undo->undoCount() == 0 && undo->redoCount() == 0 &&
                rejectedPrecompose.precompLayerId.isNil() && rejectedPrecompose.childCompId.isNil();
            undo->setBudget(originalBudget);
            if (!precomposeBudgetAtomic) {
                budgetResult["precomposeRejected"] = precomposeRejected;
                budgetResult["precomposeBudgetAtomic"] = false;
                budgetResult["passed"] = false;
                budgetResult["error"] = QStringLiteral("Undo-budget rejection changed precompose state");
                return budgetResult;
            }
            const bool precomposeCreated = service->precomposeLayersWithUndo(
                sourceIds, UniString(QStringLiteral("Budget Rejected Unprecompose")), false, false,
                PrecomposeMode::MoveSelected);
            const auto created = service->lastPrecomposeOutcome();
            const auto wrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                comp->layerById(created.precompLayerId));
            const auto child = service->findComposition(created.childCompId).ptr.lock();
            if (!precomposeCreated || !wrapper || !child) {
                undo->setBudget(originalBudget);
                budgetResult["error"] = QStringLiteral("Could not establish budget-rejection fixtures");
                return budgetResult;
            }
            const auto preUnprecomposeLayers = layers(comp, 0);
            const auto preUnprecomposeCompositionCount = projects.compositionCount();
            const auto preUnprecomposeUndoCount = undo->undoCount();
            undo->setBudget(rejectingBudget);
            const bool unprecomposeRejected = !service->unprecomposeLayerWithUndo(
                created.precompLayerId, true);
            const bool unprecomposeBudgetAtomic = unprecomposeRejected &&
                layers(comp, 0) == preUnprecomposeLayers &&
                projects.compositionCount() == preUnprecomposeCompositionCount &&
                service->findComposition(created.childCompId).ptr.lock() &&
                comp->layerById(created.precompLayerId) &&
                manager.isPrecomposeLayer(created.precompLayerId) &&
                manager.getSourceCompositionId(created.precompLayerId) == created.childCompId &&
                undo->undoCount() == 0 && undo->redoCount() == 0;
            undo->setBudget(originalBudget);
            budgetResult["precomposeRejected"] = precomposeRejected;
            budgetResult["precomposeBudgetAtomic"] = precomposeBudgetAtomic;
            budgetResult["unprecomposeRejected"] = unprecomposeRejected;
            budgetResult["unprecomposeBudgetAtomic"] = unprecomposeBudgetAtomic;
            budgetResult["passed"] = precomposeBudgetAtomic && unprecomposeBudgetAtomic;
            if (!budgetResult["passed"].toBool())
                budgetResult["error"] = QStringLiteral("Undo-budget rejection changed composition structure or precompose mapping");
            childId_ = created.childCompId;
            hasChild_ = true;
            service->changeCurrentComposition(rootId);
            refresh();
            budgetResult["state"] = state();
            return budgetResult;
        } else if (name == "test.redoBranchDiscarded") {
            QJsonObject branchResult;
            if (!comp) {
                branchResult["error"] = QStringLiteral("No current composition");
                return branchResult;
            }
            const auto rootId = comp->id();
            const auto originalLayers = layers(comp, 0);
            const auto originalCompositionCount = ArtifactProjectManager::getInstance().compositionCount();
            const auto selectedIds = namedLayerIds(comp, {QStringLiteral("A"), QStringLiteral("B")});
            const auto branchIds = namedLayerIds(comp, {QStringLiteral("Untouched")});
            if (selectedIds.size() != 2 || branchIds.size() != 1) {
                branchResult["error"] = QStringLiteral("Redo-branch fixture is missing expected layers");
                return branchResult;
            }
            const bool firstPrecompose = service->precomposeLayersWithUndo(
                selectedIds, UniString(QStringLiteral("Abandoned Redo Branch")), false, false,
                PrecomposeMode::MoveSelected);
            const auto abandonedOutcome = service->lastPrecomposeOutcome();
            const bool firstUndoAvailable = undo->canUndo();
            if (firstPrecompose && firstUndoAvailable) undo->undo();
            const bool redoBranchCreated = firstPrecompose && firstUndoAvailable && undo->canRedo() &&
                undo->redoCount() > 0 && layers(comp, 0) == originalLayers &&
                ArtifactProjectManager::getInstance().compositionCount() == originalCompositionCount;
            const bool newPrecompose = redoBranchCreated && service->precomposeLayersWithUndo(
                branchIds, UniString(QStringLiteral("Replacement Redo Branch")), false, false,
                PrecomposeMode::MoveSelected);
            const auto replacementOutcome = service->lastPrecomposeOutcome();
            const bool abandonedRedoDiscarded = newPrecompose && !undo->canRedo() && undo->redoCount() == 0 &&
                !abandonedOutcome.precompLayerId.isNil() && !replacementOutcome.precompLayerId.isNil() &&
                !comp->layerById(abandonedOutcome.precompLayerId) &&
                !PreComposeManager::instance().isPrecomposeLayer(abandonedOutcome.precompLayerId) &&
                comp->layerById(replacementOutcome.precompLayerId) &&
                PreComposeManager::instance().isPrecomposeLayer(replacementOutcome.precompLayerId);
            const bool replacementUndoAvailable = undo->canUndo();
            if (replacementUndoAvailable) undo->undo();
            const bool replacementUndoRestored = replacementUndoAvailable && undo->canRedo() &&
                undo->redoCount() == 1 && layers(comp, 0) == originalLayers &&
                !comp->layerById(replacementOutcome.precompLayerId);
            if (undo->canRedo()) undo->redo();
            const bool replacementRedoRestored = comp->id() == rootId &&
                comp->layerById(replacementOutcome.precompLayerId) &&
                PreComposeManager::instance().isPrecomposeLayer(replacementOutcome.precompLayerId) &&
                !comp->layerById(abandonedOutcome.precompLayerId);
            branchResult["redoBranchCreated"] = redoBranchCreated;
            branchResult["abandonedRedoDiscarded"] = abandonedRedoDiscarded;
            branchResult["replacementUndoRestored"] = replacementUndoRestored;
            branchResult["replacementRedoRestored"] = replacementRedoRestored;
            branchResult["passed"] = redoBranchCreated && abandonedRedoDiscarded &&
                replacementUndoRestored && replacementRedoRestored;
            if (!branchResult["passed"].toBool())
                branchResult["error"] = QStringLiteral("New precompose did not replace the existing redo branch consistently");
            refresh();
            branchResult["state"] = state();
            return branchResult;
        } else if (name == "test.keepParentFocused") {
            QJsonObject result;
            const auto root = service->currentComposition().lock();
            auto* selection = ArtifactLayerSelectionManager::instance();
            if (!root || !selection) {
                result["error"] = QStringLiteral("Composition or layer-selection service is unavailable");
                return result;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            const bool created = sourceIds.size() == 2 && service->precomposeLayersWithUndo(
                sourceIds, UniString(QStringLiteral("Keep Parent Focused")), false, false,
                PrecomposeMode::MoveSelected);
            const auto outcome = service->lastPrecomposeOutcome();
            const auto focusedWrapper = [&]() {
                const auto active = selection->activeComposition();
                const auto current = selection->currentLayer();
                const auto selected = selection->selectedLayersInOrder();
                return active && active->id() == rootId && current &&
                    current->id() == outcome.precompLayerId && selected.size() == 1 &&
                    selected.front() && selected.front()->id() == outcome.precompLayerId;
            };
            const bool parentStayedFocused = created && focusedWrapper() &&
                service->currentComposition().lock() == root;
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            const bool childWasCreated = child && child->id() == outcome.childCompId &&
                namedLayerIds(child, {QStringLiteral("A"), QStringLiteral("B")}).size() == 2;
            const bool undoAvailable = created && undo->canUndo();
            if (undoAvailable) undo->undo();
            const auto afterUndo = service->currentComposition().lock();
            const bool undoStayedFocused = undoAvailable && afterUndo && afterUndo->id() == rootId &&
                !service->findComposition(outcome.childCompId).ptr.lock();
            const bool redoAvailable = undoStayedFocused && undo->canRedo();
            if (redoAvailable) undo->redo();
            const auto afterRedo = service->currentComposition().lock();
            const bool redoStayedFocused = redoAvailable && afterRedo && afterRedo->id() == rootId &&
                focusedWrapper() && service->findComposition(outcome.childCompId).ptr.lock();
            result["parentStayedFocused"] = parentStayedFocused;
            result["childWasCreated"] = childWasCreated;
            result["undoStayedFocused"] = undoStayedFocused;
            result["redoStayedFocused"] = redoStayedFocused;
            result["passed"] = parentStayedFocused && childWasCreated && undoStayedFocused &&
                redoStayedFocused;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Precompose with openNewComposition=false changed focus or selection");
            childId_ = outcome.childCompId;
            hasChild_ = true;
            service->changeCurrentComposition(rootId);
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.componentDescriptorsSurviveUnprecompose") {
            QJsonObject result;
            const auto root = service->currentComposition().lock();
            if (!root) {
                result["error"] = QStringLiteral("No root composition");
                return result;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            const auto source = sourceIds.size() == 2 ? root->layerById(sourceIds.front())
                                                      : ArtifactAbstractLayerPtr{};
            if (!source) {
                result["error"] = QStringLiteral("Component-descriptor fixture is missing source layer A/B");
                return result;
            }
            (void)source->layerComponents();
            auto authoredSnapshot = source->componentDescriptorSnapshot();
            authoredSnapshot.insert(QStringLiteral("generators"), QJsonArray{
                QJsonObject{
                    {QStringLiteral("generatorId"), QStringLiteral("test.precompose.generator")},
                    {QStringLiteral("typeId"), QStringLiteral("artifact.test.generator.scope")},
                    {QStringLiteral("version"), 4},
                    {QStringLiteral("enabled"), false},
                    {QStringLiteral("order"), 19},
                    {QStringLiteral("settings"), QJsonObject{
                         {QStringLiteral("seed"), 2718},
                         {QStringLiteral("distribution"), QStringLiteral("radial")}}}}
            });
            authoredSnapshot.insert(QStringLiteral("fields"), QJsonArray{
                QJsonObject{
                    {QStringLiteral("fieldId"), QStringLiteral("test.precompose.field")},
                    {QStringLiteral("typeId"), QStringLiteral("artifact.test.field.scope")},
                    {QStringLiteral("version"), 2},
                    {QStringLiteral("enabled"), true},
                    {QStringLiteral("order"), 11},
                    {QStringLiteral("blendMode"), QStringLiteral("screen")},
                    {QStringLiteral("strength"), 0.5},
                    {QStringLiteral("invert"), true},
                    {QStringLiteral("settings"), QJsonObject{
                         {QStringLiteral("axis"), QStringLiteral("z")},
                         {QStringLiteral("threshold"), 0.25}}}}
            });
            authoredSnapshot.insert(QStringLiteral("cloneModifiers"), QJsonArray{
                QJsonObject{
                    {QStringLiteral("modifierId"), QStringLiteral("test.precompose.modifier")},
                    {QStringLiteral("typeId"), QStringLiteral("artifact.test.modifier.scope")},
                    {QStringLiteral("version"), 5},
                    {QStringLiteral("enabled"), false},
                    {QStringLiteral("order"), 31},
                    {QStringLiteral("settings"), QJsonObject{
                         {QStringLiteral("seed"), 42},
                         {QStringLiteral("amount"), 0.75}}}}
            });
            authoredSnapshot.insert(QStringLiteral("clonerTransforms"), QJsonArray{
                QJsonObject{
                    {QStringLiteral("name"), QStringLiteral("Precompose Transform")},
                    {QStringLiteral("enabled"), false},
                    {QStringLiteral("positionX"), 12.0},
                    {QStringLiteral("positionY"), -3.0},
                    {QStringLiteral("positionZ"), 2.5},
                    {QStringLiteral("rotationX"), 45.0},
                    {QStringLiteral("rotationY"), -90.0},
                    {QStringLiteral("rotationZ"), 180.0},
                    {QStringLiteral("scaleX"), 1.5},
                    {QStringLiteral("scaleY"), 0.5},
                    {QStringLiteral("scaleZ"), 2.0}}
            });
            QJsonArray componentGraph = authoredSnapshot.value(QStringLiteral("componentGraph")).toArray();
            const QJsonObject authoredDescriptor{
                {QStringLiteral("componentId"), QStringLiteral("test.precompose.component")},
                {QStringLiteral("typeId"), QStringLiteral("artifact.test.precompose.scope")},
                {QStringLiteral("version"), 3},
                {QStringLiteral("enabled"), false},
                {QStringLiteral("phase"), QStringLiteral("dynamics")},
                {QStringLiteral("scope"), QStringLiteral("layer")},
                {QStringLiteral("order"), 23},
                {QStringLiteral("requires"), QJsonArray{QStringLiteral("artifact.layer.source")} },
                {QStringLiteral("settings"), QJsonObject{
                     {QStringLiteral("label"), QStringLiteral("Precompose fixture")},
                     {QStringLiteral("gain"), 1.75},
                     {QStringLiteral("nested"), QJsonObject{
                          {QStringLiteral("enabled"), true},
                          {QStringLiteral("samples"), QJsonArray{2, 5, 8}}}}}}
            };
            componentGraph.append(authoredDescriptor);
            authoredSnapshot.insert(QStringLiteral("componentGraph"), componentGraph);
            const bool authored = source->restoreComponentDescriptorSnapshot(authoredSnapshot);
            const auto expectedSnapshot = source->componentDescriptorSnapshot();
            const bool descriptorAuthored = authored && expectedSnapshot == authoredSnapshot;
            const auto matchesDescriptorGraph = [&](const ArtifactAbstractLayerPtr& layer) {
                return layer && layer == source &&
                    layer->componentDescriptorSnapshot() == expectedSnapshot;
            };
            const bool precomposed = descriptorAuthored &&
                service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Component Descriptor Retention")),
                    false, false, PrecomposeMode::MoveSelected);
            const auto outcome = service->lastPrecomposeOutcome();
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            const auto childSource = child ? child->layerById(sourceIds.front())
                                           : ArtifactAbstractLayerPtr{};
            const bool childDescriptorPreserved = precomposed && childSource == source &&
                matchesDescriptorGraph(childSource);
            const bool unpacked = childDescriptorPreserved &&
                service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const bool unpackDescriptorPreserved = unpacked &&
                root->layerById(sourceIds.front()) == source && matchesDescriptorGraph(source) &&
                child && !child->layerById(sourceIds.front());
            const bool undoAvailable = unpacked && undo->canUndo();
            if (undoAvailable) undo->undo();
            const bool undoDescriptorPreserved = undoAvailable &&
                root->layerById(outcome.precompLayerId) && child &&
                child->layerById(sourceIds.front()) == source && matchesDescriptorGraph(source);
            const bool redoAvailable = undoDescriptorPreserved && undo->canRedo();
            if (redoAvailable) undo->redo();
            const bool redoDescriptorPreserved = redoAvailable &&
                root->layerById(sourceIds.front()) == source &&
                !root->layerById(outcome.precompLayerId) && matchesDescriptorGraph(source) &&
                child && !child->layerById(sourceIds.front());
            result["authored"] = descriptorAuthored;
            result["childDescriptorPreserved"] = childDescriptorPreserved;
            result["unpackDescriptorPreserved"] = unpackDescriptorPreserved;
            result["undoDescriptorPreserved"] = undoDescriptorPreserved;
            result["redoDescriptorPreserved"] = redoDescriptorPreserved;
            result["passed"] = descriptorAuthored && childDescriptorPreserved &&
                unpackDescriptorPreserved && undoDescriptorPreserved && redoDescriptorPreserved;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Layer component descriptor graph changed across precompose/unprecompose/undo/redo");
            childId_ = outcome.childCompId;
            hasChild_ = true;
            service->changeCurrentComposition(rootId);
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.matteReferencesSurviveUnprecompose") {
            QJsonObject result;
            const auto root = service->currentComposition().lock();
            if (!root) {
                result["error"] = QStringLiteral("No root composition");
                return result;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            const auto source = sourceIds.size() == 2 ? root->layerById(sourceIds.front())
                                                      : ArtifactAbstractLayerPtr{};
            if (!source) {
                result["error"] = QStringLiteral("Matte-reference fixture is missing source layer A/B");
                return result;
            }
            LayerMatteReference first;
            first.sourceLayerId = Id(sourceIds[1].toString());
            first.sourceAssetPath = QStringLiteral("asset://matte-first");
            first.enabled = false;
            first.type = MatteType::Luma;
            first.blendMode = MatteBlendMode::Subtract;
            first.fitMode = MatteFitMode::Fill;
            first.opacity = 0.35f;
            first.invert = true;
            LayerMatteReference second;
            second.sourceLayerId = Id(sourceIds[1].toString());
            second.sourceAssetPath = QStringLiteral("asset://matte-second");
            second.enabled = true;
            second.type = MatteType::InverseAlpha;
            second.blendMode = MatteBlendMode::Difference;
            second.fitMode = MatteFitMode::Original;
            second.opacity = 0.8f;
            second.invert = false;
            source->setMatteReferences({first, second});
            const auto matchesMattes = [&](const ArtifactAbstractLayerPtr& layer) {
                if (!layer || layer != source || layer->matteReferenceCount() != 2) return false;
                const auto references = layer->matteReferences();
                return references.size() == 2 &&
                    references[0].id.toString() == first.id.toString() &&
                    references[0].sourceLayerId.toString() == sourceIds[1].toString() &&
                    references[0].sourceAssetPath == first.sourceAssetPath &&
                    !references[0].enabled && references[0].type == MatteType::Luma &&
                    references[0].blendMode == MatteBlendMode::Subtract &&
                    references[0].fitMode == MatteFitMode::Fill &&
                    std::abs(references[0].opacity - 0.35f) <= 1.0e-6f &&
                    references[0].invert &&
                    references[1].id.toString() == second.id.toString() &&
                    references[1].sourceLayerId.toString() == sourceIds[1].toString() &&
                    references[1].sourceAssetPath == second.sourceAssetPath &&
                    references[1].enabled && references[1].type == MatteType::InverseAlpha &&
                    references[1].blendMode == MatteBlendMode::Difference &&
                    references[1].fitMode == MatteFitMode::Original &&
                    std::abs(references[1].opacity - 0.8f) <= 1.0e-6f &&
                    !references[1].invert;
            };
            const bool authored = matchesMattes(source);
            const bool precomposed = authored && service->precomposeLayersWithUndo(
                sourceIds, UniString(QStringLiteral("Matte Reference Retention")), false, false,
                PrecomposeMode::MoveSelected);
            const auto outcome = service->lastPrecomposeOutcome();
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            const auto childSource = child ? child->layerById(sourceIds.front())
                                           : ArtifactAbstractLayerPtr{};
            const bool childMattesPreserved = precomposed && childSource == source &&
                matchesMattes(childSource);
            const bool unpacked = childMattesPreserved &&
                service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const bool unpackMattesPreserved = unpacked && root->layerById(sourceIds.front()) == source &&
                matchesMattes(source) && child && !child->layerById(sourceIds.front());
            const bool undoAvailable = unpacked && undo->canUndo();
            if (undoAvailable) undo->undo();
            const bool undoMattesPreserved = undoAvailable && root->layerById(outcome.precompLayerId) &&
                child && child->layerById(sourceIds.front()) == source && matchesMattes(source);
            const bool redoAvailable = undoMattesPreserved && undo->canRedo();
            if (redoAvailable) undo->redo();
            const bool redoMattesPreserved = redoAvailable &&
                root->layerById(sourceIds.front()) == source &&
                !root->layerById(outcome.precompLayerId) && matchesMattes(source) && child &&
                !child->layerById(sourceIds.front());
            result["authored"] = authored;
            result["childMattesPreserved"] = childMattesPreserved;
            result["unpackMattesPreserved"] = unpackMattesPreserved;
            result["undoMattesPreserved"] = undoMattesPreserved;
            result["redoMattesPreserved"] = redoMattesPreserved;
            result["passed"] = authored && childMattesPreserved && unpackMattesPreserved &&
                undoMattesPreserved && redoMattesPreserved;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Layer matte reference identity, order, or settings changed across precompose/unprecompose/undo/redo");
            childId_ = outcome.childCompId;
            hasChild_ = true;
            service->changeCurrentComposition(rootId);
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.maskSurvivesUnprecompose") {
            QJsonObject result;
            const auto root = service->currentComposition().lock();
            if (!root) {
                result["error"] = QStringLiteral("No root composition");
                return result;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            const auto source = sourceIds.size() == 2 ? root->layerById(sourceIds.front())
                                                      : ArtifactAbstractLayerPtr{};
            if (!source) {
                result["error"] = QStringLiteral("Mask-retention fixture is missing source layer A");
                return result;
            }
            LayerMask mask;
            mask.setEnabled(false);
            mask.setLocked(true);
            mask.setColor(FloatColor(0.2f, 0.4f, 0.7f, 0.9f));
            MaskPath path;
            path.setName(UniString(QStringLiteral("Cutout")));
            path.setClosed(false);
            path.setOpacity(0.65f);
            path.setInverted(true);
            path.setMode(MaskMode::Subtract);
            path.addVertex({QPointF(12.0, 18.0), QPointF(-2.0, 1.0), QPointF(3.0, -1.0)});
            path.addVertex({QPointF(48.0, 21.0), QPointF(-4.0, 2.0), QPointF(1.0, 3.0)});
            MaskPathKeyframeSnapshot firstKey;
            firstKey.frame = 12;
            firstKey.vertices = {
                {QPointF(14.0, 20.0), QPointF(-1.0, 2.0), QPointF(4.0, -2.0)},
                {QPointF(52.0, 24.0), QPointF(-3.0, 1.0), QPointF(2.0, 4.0)}};
            firstKey.closed = true;
            firstKey.opacity = 0.4f;
            firstKey.feather = 3.5f;
            firstKey.falloff = MaskFeatherFalloff::Linear;
            firstKey.inverted = false;
            firstKey.mode = MaskMode::Add;
            firstKey.name = UniString(QStringLiteral("Cutout Start"));
            path.setAnimationKeyframe(firstKey.frame, firstKey);
            MaskPathKeyframeSnapshot secondKey;
            secondKey.frame = 28;
            secondKey.vertices = {
                {QPointF(19.0, 26.0), QPointF(-5.0, 2.0), QPointF(2.0, -3.0)},
                {QPointF(58.0, 30.0), QPointF(-2.0, 3.0), QPointF(4.0, 1.0)}};
            secondKey.closed = true;
            secondKey.opacity = 0.9f;
            secondKey.feather = 6.25f;
            secondKey.falloff = MaskFeatherFalloff::Sharp;
            secondKey.inverted = true;
            secondKey.mode = MaskMode::Intersect;
            secondKey.name = UniString(QStringLiteral("Cutout End"));
            path.setAnimationKeyframe(secondKey.frame, secondKey);
            mask.addMaskPath(path);
            source->addMask(mask);
            const auto matchesMask = [&](const ArtifactAbstractLayerPtr& layer) {
                if (!layer || layer != source || layer->maskCount() != 1) return false;
                const auto stored = layer->mask(0);
                if (stored.isEnabled() || !stored.isLocked() || stored.maskPathCount() != 1 ||
                    !stored.color().approximatelyEqual(FloatColor(0.2f, 0.4f, 0.7f, 0.9f)))
                    return false;
                const auto storedPath = stored.maskPath(0);
                const auto keys = storedPath.animationKeyframes();
                return storedPath.name().toQString() == QStringLiteral("Cutout") &&
                    !storedPath.isClosed() && storedPath.isInverted() &&
                    storedPath.mode() == MaskMode::Subtract &&
                    std::abs(storedPath.opacity() - 0.65f) <= 1.0e-6f &&
                    storedPath.vertexCount() == 2 &&
                    storedPath.vertex(0).position == QPointF(12.0, 18.0) &&
                    storedPath.vertex(0).inTangent == QPointF(-2.0, 1.0) &&
                    storedPath.vertex(0).outTangent == QPointF(3.0, -1.0) &&
                    storedPath.vertex(1).position == QPointF(48.0, 21.0) &&
                    storedPath.vertex(1).inTangent == QPointF(-4.0, 2.0) &&
                    storedPath.vertex(1).outTangent == QPointF(1.0, 3.0) &&
                    storedPath.hasAnimationKeyframes() && keys.size() == 2 &&
                    keys[0].frame == 12 && keys[0].name.toQString() == QStringLiteral("Cutout Start") &&
                    keys[0].closed && std::abs(keys[0].opacity - 0.4f) <= 1.0e-6f &&
                    std::abs(keys[0].feather - 3.5f) <= 1.0e-6f &&
                    keys[0].falloff == MaskFeatherFalloff::Linear && !keys[0].inverted &&
                    keys[0].mode == MaskMode::Add && keys[0].vertices.size() == 2 &&
                    keys[0].vertices[0].position == QPointF(14.0, 20.0) &&
                    keys[0].vertices[0].inTangent == QPointF(-1.0, 2.0) &&
                    keys[0].vertices[0].outTangent == QPointF(4.0, -2.0) &&
                    keys[1].frame == 28 && keys[1].name.toQString() == QStringLiteral("Cutout End") &&
                    keys[1].closed && std::abs(keys[1].opacity - 0.9f) <= 1.0e-6f &&
                    std::abs(keys[1].feather - 6.25f) <= 1.0e-6f &&
                    keys[1].falloff == MaskFeatherFalloff::Sharp && keys[1].inverted &&
                    keys[1].mode == MaskMode::Intersect && keys[1].vertices.size() == 2 &&
                    keys[1].vertices[1].position == QPointF(58.0, 30.0) &&
                    keys[1].vertices[1].inTangent == QPointF(-2.0, 3.0) &&
                    keys[1].vertices[1].outTangent == QPointF(4.0, 1.0);
            };
            const bool authored = matchesMask(source);
            const bool precomposed = authored && service->precomposeLayersWithUndo(
                sourceIds, UniString(QStringLiteral("Mask Retention")), false, false,
                PrecomposeMode::MoveSelected);
            const auto outcome = service->lastPrecomposeOutcome();
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            const auto childSource = child ? child->layerById(sourceIds.front())
                                           : ArtifactAbstractLayerPtr{};
            const bool childMaskPreserved = precomposed && childSource == source &&
                matchesMask(childSource);
            const bool unpacked = childMaskPreserved &&
                service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const bool unpackMaskPreserved = unpacked && root->layerById(sourceIds.front()) == source &&
                matchesMask(source) && child && !child->layerById(sourceIds.front());
            const bool undoAvailable = unpacked && undo->canUndo();
            if (undoAvailable) undo->undo();
            const bool undoMaskPreserved = undoAvailable && root->layerById(outcome.precompLayerId) &&
                child && child->layerById(sourceIds.front()) == source && matchesMask(source);
            const bool redoAvailable = undoMaskPreserved && undo->canRedo();
            if (redoAvailable) undo->redo();
            const bool redoMaskPreserved = redoAvailable && root->layerById(sourceIds.front()) == source &&
                !root->layerById(outcome.precompLayerId) && matchesMask(source) && child &&
                !child->layerById(sourceIds.front());
            result["authored"] = authored;
            result["childMaskPreserved"] = childMaskPreserved;
            result["unpackMaskPreserved"] = unpackMaskPreserved;
            result["undoMaskPreserved"] = undoMaskPreserved;
            result["redoMaskPreserved"] = redoMaskPreserved;
            result["passed"] = authored && childMaskPreserved && unpackMaskPreserved &&
                undoMaskPreserved && redoMaskPreserved;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Layer mask settings or path geometry changed across precompose/unprecompose/undo/redo");
            childId_ = outcome.childCompId;
            hasChild_ = true;
            service->changeCurrentComposition(rootId);
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.effectSurvivesUnprecompose") {
            QJsonObject result;
            const auto root = service->currentComposition().lock();
            if (!root) {
                result["error"] = QStringLiteral("No root composition");
                return result;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            const auto source = sourceIds.size() == 2 ? root->layerById(sourceIds.front())
                                                      : ArtifactAbstractLayerPtr{};
            const auto effect = makeShared<BrightnessEffect>();
            effect->setBrightness(0.375f);
            if (!source) {
                result["error"] = QStringLiteral("Effect-retention fixture is missing source layer A");
                return result;
            }
            source->addEffect(effect);
            const auto hasEffect = [&](const ArtifactAbstractLayerPtr& layer) {
                if (!layer || layer != source || layer->effectCount() != 1) return false;
                const auto effects = layer->getEffects();
                return effects.size() == 1 && effects.front() == effect &&
                    std::abs(effect->brightness() - 0.375f) <= 1.0e-6f;
            };
            const bool authored = hasEffect(source);
            const bool precomposed = authored && service->precomposeLayersWithUndo(
                sourceIds, UniString(QStringLiteral("Effect Retention")), false, false,
                PrecomposeMode::MoveSelected);
            const auto outcome = service->lastPrecomposeOutcome();
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            const auto childSource = child ? child->layerById(sourceIds.front())
                                           : ArtifactAbstractLayerPtr{};
            const bool childEffectPreserved = precomposed && childSource == source &&
                hasEffect(childSource);
            const bool unpacked = childEffectPreserved &&
                service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const bool unpackEffectPreserved = unpacked && root->layerById(sourceIds.front()) == source &&
                hasEffect(source) && child && !child->layerById(sourceIds.front());
            const bool undoAvailable = unpacked && undo->canUndo();
            if (undoAvailable) undo->undo();
            const bool undoEffectPreserved = undoAvailable &&
                root->layerById(outcome.precompLayerId) && child &&
                child->layerById(sourceIds.front()) == source && hasEffect(source);
            const bool redoAvailable = undoEffectPreserved && undo->canRedo();
            if (redoAvailable) undo->redo();
            const bool redoEffectPreserved = redoAvailable && root->layerById(sourceIds.front()) == source &&
                !root->layerById(outcome.precompLayerId) && hasEffect(source) && child &&
                !child->layerById(sourceIds.front());
            result["authored"] = authored;
            result["childEffectPreserved"] = childEffectPreserved;
            result["unpackEffectPreserved"] = unpackEffectPreserved;
            result["undoEffectPreserved"] = undoEffectPreserved;
            result["redoEffectPreserved"] = redoEffectPreserved;
            result["brightness"] = effect->brightness();
            result["passed"] = authored && childEffectPreserved && unpackEffectPreserved &&
                undoEffectPreserved && redoEffectPreserved;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Layer effect identity or parameters changed across precompose/unprecompose/undo/redo");
            childId_ = outcome.childCompId;
            hasChild_ = true;
            service->changeCurrentComposition(rootId);
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.compositionEffectStaysOnParent") {
            QJsonObject result;
            const auto root = service->currentComposition().lock();
            if (!root) {
                result["error"] = QStringLiteral("No root composition");
                return result;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            const auto effect = makeShared<BrightnessEffect>();
            effect->setBrightness(-0.25f);
            root->addEffect(effect);
            const auto parentEffectPreserved = [&]() {
                const auto effects = root->getEffects();
                return root->effectCount() == 1 && effects.size() == 1 &&
                    effects.front() == effect &&
                    std::abs(effect->brightness() + 0.25f) <= 1.0e-6f;
            };
            const bool authored = parentEffectPreserved();
            const bool precomposed = authored && sourceIds.size() == 2 &&
                service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Composition Effect Scope")), false, false,
                    PrecomposeMode::MoveSelected);
            const auto outcome = service->lastPrecomposeOutcome();
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            const bool scopedToParent = precomposed && parentEffectPreserved() && child &&
                child->effectCount() == 0;
            const bool unpacked = scopedToParent &&
                service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const bool unpackPreservedScope = unpacked && parentEffectPreserved() && child &&
                child->effectCount() == 0 && !child->layerById(sourceIds.front());
            const bool undoAvailable = unpacked && undo->canUndo();
            if (undoAvailable) undo->undo();
            const bool undoPreservedScope = undoAvailable && parentEffectPreserved() && child &&
                child->effectCount() == 0 && child->layerById(sourceIds.front());
            const bool redoAvailable = undoPreservedScope && undo->canRedo();
            if (redoAvailable) undo->redo();
            const bool redoPreservedScope = redoAvailable && parentEffectPreserved() && child &&
                child->effectCount() == 0 && !child->layerById(sourceIds.front());
            result["authored"] = authored;
            result["scopedToParent"] = scopedToParent;
            result["unpackPreservedScope"] = unpackPreservedScope;
            result["undoPreservedScope"] = undoPreservedScope;
            result["redoPreservedScope"] = redoPreservedScope;
            result["passed"] = authored && scopedToParent && unpackPreservedScope &&
                undoPreservedScope && redoPreservedScope;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Composition effect moved to the child or changed across precompose/unprecompose/undo/redo");
            childId_ = outcome.childCompId;
            hasChild_ = true;
            service->changeCurrentComposition(rootId);
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.childCompositionEffectStaysWithChild") {
            QJsonObject result;
            const auto root = service->currentComposition().lock();
            if (!root) {
                result["error"] = QStringLiteral("No root composition");
                return result;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2 || !service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Child Effect Persistence")), false, false,
                    PrecomposeMode::MoveSelected)) {
                result["error"] = QStringLiteral("Could not create child-effect persistence fixture");
                return result;
            }
            const auto outcome = service->lastPrecomposeOutcome();
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            const auto matchesChildEffect = [](const ArtifactCompositionPtr& composition) {
                if (!composition || composition->effectCount() != 1) return false;
                const auto effects = composition->getEffects();
                const auto brightness = effects.size() == 1
                    ? dynamicPointerCast<BrightnessEffect>(effects.front())
                    : SharedPtr<BrightnessEffect>{};
                return brightness && std::abs(brightness->brightness() - 0.4375f) <= 1.0e-6f;
            };
            const auto childEffect = makeShared<BrightnessEffect>();
            childEffect->setBrightness(0.4375f);
            if (child) child->addEffect(childEffect);
            QTemporaryDir tempDir;
            if (!child || !tempDir.isValid()) {
                result["error"] = QStringLiteral("Could not prepare child-effect save/reload fixture");
                return result;
            }
            const auto filePath = tempDir.filePath(QStringLiteral("child-composition-effect.artifact"));
            auto& projects = ArtifactProjectManager::getInstance();
            auto& precompManager = PreComposeManager::instance();
            const auto save = projects.saveToFile(filePath);
            const bool mappingCleared = save.success && precompManager.unregisterPrecompLayer(
                rootId, outcome.precompLayerId, outcome.childCompId) &&
                !precompManager.isPrecomposeLayer(outcome.precompLayerId);
            const bool loaded = mappingCleared && projects.loadFromFile(filePath);
            if (loaded) service->changeCurrentComposition(rootId);
            const auto reloadedRoot = loaded ? service->findComposition(rootId).ptr.lock()
                                             : ArtifactCompositionPtr{};
            const auto reloadedChild = loaded ? service->findComposition(outcome.childCompId).ptr.lock()
                                              : ArtifactCompositionPtr{};
            const auto reloadedWrapper = reloadedRoot
                ? dynamicPointerCast<ArtifactCompositionLayer>(
                      reloadedRoot->layerById(outcome.precompLayerId))
                : SharedPtr<ArtifactCompositionLayer>{};
            const bool effectRestoredOnChild = reloadedWrapper && reloadedChild &&
                reloadedWrapper->sourceCompositionId() == outcome.childCompId &&
                precompManager.isPrecomposeLayer(outcome.precompLayerId) &&
                matchesChildEffect(reloadedChild) && reloadedRoot->getEffects().empty();
            const bool unpacked = effectRestoredOnChild &&
                service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const bool unpackKeptChildEffect = unpacked && matchesChildEffect(reloadedChild) &&
                reloadedRoot->layerById(sourceIds.front()) && reloadedRoot->layerById(sourceIds.back()) &&
                !reloadedChild->layerById(sourceIds.front()) && !reloadedChild->layerById(sourceIds.back()) &&
                !precompManager.isPrecomposeLayer(outcome.precompLayerId);
            const bool undoAvailable = unpackKeptChildEffect && undo->canUndo();
            if (undoAvailable) undo->undo();
            const bool undoKeptChildEffect = undoAvailable && matchesChildEffect(reloadedChild) &&
                reloadedChild->layerById(sourceIds.front()) && reloadedChild->layerById(sourceIds.back()) &&
                reloadedRoot->layerById(outcome.precompLayerId) &&
                precompManager.isPrecomposeLayer(outcome.precompLayerId);
            const bool redoAvailable = undoKeptChildEffect && undo->canRedo();
            if (redoAvailable) undo->redo();
            const bool redoKeptChildEffect = redoAvailable && matchesChildEffect(reloadedChild) &&
                reloadedRoot->layerById(sourceIds.front()) && reloadedRoot->layerById(sourceIds.back()) &&
                !reloadedChild->layerById(sourceIds.front()) && !precompManager.isPrecomposeLayer(outcome.precompLayerId);
            result["saveSucceeded"] = save.success;
            result["mappingClearedBeforeReload"] = mappingCleared;
            result["loaded"] = loaded;
            result["effectRestoredOnChild"] = effectRestoredOnChild;
            result["unpackKeptChildEffect"] = unpackKeptChildEffect;
            result["undoKeptChildEffect"] = undoKeptChildEffect;
            result["redoKeptChildEffect"] = redoKeptChildEffect;
            result["passed"] = save.success && mappingCleared && loaded && effectRestoredOnChild &&
                unpackKeptChildEffect && undoKeptChildEffect && redoKeptChildEffect;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Child composition effect changed scope across save/reload or unprecompose history");
            childId_ = outcome.childCompId;
            hasChild_ = true;
            service->changeCurrentComposition(rootId);
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.openChildComposition") {
            QJsonObject result;
            if (!comp) {
                result["error"] = QStringLiteral("No current composition");
                return result;
            }
            const auto rootId = comp->id();
            const auto sourceIds = namedLayerIds(comp, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2 || !service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Open Child Composition")), true, false,
                    PrecomposeMode::MoveSelected)) {
                result["error"] = QStringLiteral("Could not precompose and open the child composition");
                return result;
            }
            const auto outcome = service->lastPrecomposeOutcome();
            const auto openedChild = service->currentComposition().lock();
            const bool childOpened = openedChild && openedChild->id() == outcome.childCompId &&
                namedLayerIds(openedChild, {QStringLiteral("A"), QStringLiteral("B")}).size() == 2;
            const bool rootWrapperCreated = comp->layerById(outcome.precompLayerId) &&
                ArtifactCore::dynamicPointerCast<ArtifactCompositionLayer>(
                    comp->layerById(outcome.precompLayerId));
            const bool undoAvailable = undo->canUndo();
            if (undoAvailable) undo->undo();
            const auto afterUndo = service->currentComposition().lock();
            const bool undoReturnedToRoot = undoAvailable && afterUndo && afterUndo->id() == rootId &&
                namedLayerIds(afterUndo, {QStringLiteral("A"), QStringLiteral("B")}).size() == 2 &&
                !service->findComposition(outcome.childCompId).ptr.lock();
            const bool redoAvailable = undo->canRedo();
            if (redoAvailable) undo->redo();
            const auto afterRedo = service->currentComposition().lock();
            const auto redoneChild = service->findComposition(outcome.childCompId).ptr.lock();
            bool parentReferenceRestored = false;
            for (const auto& reference : PreComposeManager::instance().getPrecompLayersForChild(
                     outcome.childCompId)) {
                parentReferenceRestored = parentReferenceRestored ||
                    (reference.parentCompId == rootId &&
                     reference.precompLayerId == outcome.precompLayerId);
            }
            const bool redoKeptRootFocused = redoAvailable && afterRedo && afterRedo->id() == rootId;
            const bool redoRestoredChild = redoneChild && afterRedo &&
                namedLayerIds(redoneChild, {QStringLiteral("A"), QStringLiteral("B")}).size() == 2 &&
                parentReferenceRestored && afterRedo->layerById(outcome.precompLayerId);
            result["rootCompositionId"] = rootId.toString();
            result["childCompositionId"] = outcome.childCompId.toString();
            result["childOpened"] = childOpened;
            result["rootWrapperCreated"] = bool(rootWrapperCreated);
            result["undoReturnedToRoot"] = undoReturnedToRoot;
            result["redoKeptRootFocused"] = redoKeptRootFocused;
            result["redoRestoredChild"] = redoRestoredChild;
            result["passed"] = childOpened && rootWrapperCreated && undoReturnedToRoot &&
                redoKeptRootFocused && redoRestoredChild;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Open-child navigation or undo/redo focus contract changed");
            service->changeCurrentComposition(rootId);
            childId_ = outcome.childCompId;
            hasChild_ = true;
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.matchWorkspaceDuration") {
            QJsonObject result;
            if (!comp) {
                result["error"] = QStringLiteral("No current composition");
                return result;
            }
            const auto rootId = comp->id();
            const FrameRange workspaceRange(30, 270);
            if (workspaceRange.end() > comp->frameRange().end()) {
                result["error"] = QStringLiteral("Fixture composition is shorter than the target workspace range");
                return result;
            }
            comp->setWorkAreaRange(workspaceRange);
            const auto sourceIds = namedLayerIds(comp, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2 || !service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Workspace Duration")), true, true,
                    PrecomposeMode::MoveSelected)) {
                result["error"] = QStringLiteral("Could not precompose using workspace duration");
                return result;
            }
            const auto outcome = service->lastPrecomposeOutcome();
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            const auto wrapper = ArtifactCore::dynamicPointerCast<ArtifactCompositionLayer>(
                comp->layerById(outcome.precompLayerId));
            const bool childDurationMatched = child && child->frameRange().start() == 0 &&
                child->frameRange().duration() == workspaceRange.duration();
            const bool childWorkAreaNormalized = child && child->workAreaRange().start() == 0 &&
                child->workAreaRange().duration() == workspaceRange.duration();
            const auto activeChild = service->currentComposition().lock();
            const bool childOpened = child && activeChild && activeChild->id() == child->id();
            const bool wrapperTimingPreserved = wrapper &&
                wrapper->inPoint().framePosition() == 40 &&
                wrapper->outPoint().framePosition() == 200 &&
                wrapper->startTime().framePosition() == 40;
            result["workspaceDuration"] = workspaceRange.duration();
            result["childDuration"] = child ? child->frameRange().duration() : -1;
            result["childWorkAreaDuration"] = child ? child->workAreaRange().duration() : -1;
            result["childDurationMatched"] = childDurationMatched;
            result["childWorkAreaNormalized"] = childWorkAreaNormalized;
            result["childOpened"] = childOpened;
            result["wrapperTimingPreserved"] = wrapperTimingPreserved;
            result["passed"] = childDurationMatched && childWorkAreaNormalized && childOpened &&
                wrapperTimingPreserved;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Precompose child did not match workspace duration or wrapper timing");
            childId_ = outcome.childCompId;
            hasChild_ = true;
            refresh();
            result["state"] = state();
            service->changeCurrentComposition(rootId);
            return result;
        } else if (name == "test.childDurationPropagates") {
            QJsonObject result;
            if (!comp) {
                result["error"] = QStringLiteral("No current composition");
                return result;
            }
            const auto rootId = comp->id();
            const auto sourceIds = namedLayerIds(comp, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2 || !service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Duration Propagation")), false, false,
                    PrecomposeMode::MoveSelected)) {
                result["error"] = QStringLiteral("Could not create duration propagation fixture");
                return result;
            }
            const auto outcome = service->lastPrecomposeOutcome();
            auto wrapper = ArtifactCore::dynamicPointerCast<ArtifactCompositionLayer>(
                comp->layerById(outcome.precompLayerId));
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            if (!wrapper || !child) {
                result["error"] = QStringLiteral("Duration propagation fixture is missing its wrapper or child");
                return result;
            }
            service->changeCurrentComposition(outcome.childCompId);
            child->setFrameRange(FrameRange(0, 90));
            const int64_t shortenedOut = wrapper->outPoint().framePosition();
            const auto activeAfterShrink = service->currentComposition().lock();
            const bool shrinkFollowed = shortenedOut == 130 &&
                wrapper->inPoint().framePosition() == 40 &&
                wrapper->startTime().framePosition() == 40 &&
                activeAfterShrink && activeAfterShrink->id() == outcome.childCompId;
            child->setFrameRange(FrameRange(0, 240));
            const int64_t expandedOut = wrapper->outPoint().framePosition();
            const auto activeAfterExpansion = service->currentComposition().lock();
            const bool expansionFollowed = expandedOut == 280 &&
                wrapper->inPoint().framePosition() == 40 &&
                wrapper->startTime().framePosition() == 40 &&
                activeAfterExpansion && activeAfterExpansion->id() == outcome.childCompId;
            const bool activeChildStayed = shrinkFollowed && expansionFollowed;
            bool parentReferencePreserved = false;
            for (const auto& reference : PreComposeManager::instance().getPrecompLayersForChild(
                     outcome.childCompId)) {
                parentReferencePreserved = parentReferencePreserved ||
                    (reference.parentCompId == rootId &&
                     reference.precompLayerId == outcome.precompLayerId);
            }
            result["shortenedChildOut"] = shortenedOut;
            result["expandedChildOut"] = expandedOut;
            result["shrinkFollowed"] = shrinkFollowed;
            result["expansionFollowed"] = expansionFollowed;
            result["activeChildStayed"] = activeChildStayed;
            result["parentReferencePreserved"] = parentReferencePreserved;
            result["passed"] = shrinkFollowed && expansionFollowed && activeChildStayed &&
                parentReferencePreserved;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Child duration change did not update the parent window from its fixed in point");
            childId_ = outcome.childCompId;
            hasChild_ = true;
            service->changeCurrentComposition(rootId);
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.parentDurationPropagatesToChild") {
            QJsonObject result;
            const auto root = service->currentComposition().lock();
            if (!root) {
                result["error"] = QStringLiteral("No root composition");
                return result;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2 || !service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Parent Duration Propagation")), false, false,
                    PrecomposeMode::MoveSelected)) {
                result["error"] = QStringLiteral("Could not create parent-duration propagation fixture");
                return result;
            }
            const auto outcome = service->lastPrecomposeOutcome();
            const auto wrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                root->layerById(outcome.precompLayerId));
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            if (!wrapper || !child) {
                result["error"] = QStringLiteral("Parent-duration fixture is missing its wrapper or child");
                return result;
            }
            root->setFrameRange(FrameRange(0, 180));
            const bool firstShrinkFollowed = root->frameRange().duration() == 180 &&
                child->frameRange().duration() == 180 &&
                wrapper->inPoint().framePosition() == 40 &&
                wrapper->startTime().framePosition() == 40 &&
                wrapper->outPoint().framePosition() == 220;
            root->setFrameRange(FrameRange(0, 75));
            const bool secondShrinkFollowed = root->frameRange().duration() == 75 &&
                child->frameRange().duration() == 75 &&
                wrapper->inPoint().framePosition() == 40 &&
                wrapper->startTime().framePosition() == 40 &&
                wrapper->outPoint().framePosition() == 115;
            root->setFrameRange(FrameRange(0, 360));
            const bool expansionFollowed = root->frameRange().duration() == 360 &&
                child->frameRange().duration() == 360 &&
                wrapper->inPoint().framePosition() == 40 &&
                wrapper->startTime().framePosition() == 40 &&
                wrapper->outPoint().framePosition() == 400;
            result["firstShrinkFollowed"] = firstShrinkFollowed;
            result["secondShrinkFollowed"] = secondShrinkFollowed;
            result["expansionFollowed"] = expansionFollowed;
            result["passed"] = firstShrinkFollowed && secondShrinkFollowed && expansionFollowed;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Parent duration changes did not propagate to the child and its wrapper");
            childId_ = outcome.childCompId;
            hasChild_ = true;
            service->changeCurrentComposition(rootId);
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.sharedChildDurationPropagates") {
            QJsonObject result;
            const auto root = service->currentComposition().lock();
            if (!root) {
                result["error"] = QStringLiteral("No root composition");
                return result;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2 || !service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Shared Duration Child")), false, false,
                    PrecomposeMode::MoveSelected)) {
                result["error"] = QStringLiteral("Could not create shared-child duration fixture");
                return result;
            }
            const auto outcome = service->lastPrecomposeOutcome();
            const auto firstWrapper = ArtifactCore::dynamicPointerCast<ArtifactCompositionLayer>(
                root->layerById(outcome.precompLayerId));
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            if (!firstWrapper || !child) {
                result["error"] = QStringLiteral("Shared-child duration fixture is missing its first wrapper or child");
                return result;
            }
            ArtifactCompositionInitParams secondParentParams;
            secondParentParams.setCompositionName(UniString(QStringLiteral("Second Duration Parent")));
            secondParentParams.setResolution(640, 360);
            secondParentParams.setFrameRate(24.0);
            secondParentParams.setDurationFrames(300);
            service->createComposition(secondParentParams);
            const auto secondParent = service->currentComposition().lock();
            if (!secondParent || !service->addCompositionLayerToCurrentCompositionWithUndo(
                    outcome.childCompId)) {
                result["error"] = QStringLiteral("Could not add the shared child to the second parent");
                service->changeCurrentComposition(rootId);
                return result;
            }
            auto* selection = ArtifactLayerSelectionManager::instance();
            const auto secondWrapper = selection
                ? ArtifactCore::dynamicPointerCast<ArtifactCompositionLayer>(selection->currentLayer())
                : SharedPtr<ArtifactCompositionLayer>{};
            auto& manager = PreComposeManager::instance();
            const auto references = manager.getPrecompLayersForChild(outcome.childCompId);
            const bool bothParentsRegistered = secondWrapper && secondWrapper->id() != outcome.precompLayerId &&
                references.size() == 2 &&
                std::any_of(references.begin(), references.end(), [&](const auto& reference) {
                    return reference.parentCompId == rootId &&
                        reference.precompLayerId == outcome.precompLayerId;
                }) &&
                std::any_of(references.begin(), references.end(), [&](const auto& reference) {
                    return reference.parentCompId == secondParent->id() &&
                        reference.precompLayerId == secondWrapper->id();
                });
            child->setFrameRange(FrameRange(0, 90));
            const bool shrinkFollowed = firstWrapper->outPoint().framePosition() == 130 &&
                secondWrapper->outPoint().framePosition() == 90 &&
                firstWrapper->inPoint().framePosition() == 40 &&
                secondWrapper->inPoint().framePosition() == 0 &&
                firstWrapper->startTime().framePosition() == 40 &&
                secondWrapper->startTime().framePosition() == 0;
            child->setFrameRange(FrameRange(0, 240));
            const bool expansionFollowed = firstWrapper->outPoint().framePosition() == 280 &&
                secondWrapper->outPoint().framePosition() == 240 &&
                firstWrapper->inPoint().framePosition() == 40 &&
                secondWrapper->inPoint().framePosition() == 0 &&
                firstWrapper->startTime().framePosition() == 40 &&
                secondWrapper->startTime().framePosition() == 0;
            QTemporaryDir tempDir;
            if (!tempDir.isValid() || !secondWrapper) {
                result["error"] = QStringLiteral("Could not prepare shared-child save/reload fixture");
                return result;
            }
            const auto filePath = tempDir.filePath(QStringLiteral("shared-child-duration.artifact"));
            const auto save = ArtifactProjectManager::getInstance().saveToFile(filePath);
            const auto childId = outcome.childCompId;
            const auto firstWrapperId = outcome.precompLayerId;
            const auto secondWrapperId = secondWrapper->id();
            const auto secondParentId = secondParent->id();
            const bool mappingsCleared = save.success &&
                manager.unregisterPrecompLayer(rootId, firstWrapperId, childId) &&
                manager.unregisterPrecompLayer(secondParentId, secondWrapperId, childId) &&
                manager.getPrecompLayersForChild(childId).isEmpty();
            const bool loaded = mappingsCleared &&
                ArtifactProjectManager::getInstance().loadFromFile(filePath);
            if (loaded) service->changeCurrentComposition(rootId);
            const auto reloadedRoot = loaded ? service->findComposition(rootId).ptr.lock()
                                             : decltype(root){};
            const auto reloadedSecondParent = loaded ? service->findComposition(secondParentId).ptr.lock()
                                                     : decltype(root){};
            const auto reloadedChild = loaded ? service->findComposition(childId).ptr.lock()
                                              : decltype(root){};
            const auto reloadedFirstWrapper = reloadedRoot
                ? ArtifactCore::dynamicPointerCast<ArtifactCompositionLayer>(reloadedRoot->layerById(firstWrapperId))
                : SharedPtr<ArtifactCompositionLayer>{};
            const auto reloadedSecondWrapper = reloadedSecondParent
                ? ArtifactCore::dynamicPointerCast<ArtifactCompositionLayer>(reloadedSecondParent->layerById(secondWrapperId))
                : SharedPtr<ArtifactCompositionLayer>{};
            const auto restoredReferences = manager.getPrecompLayersForChild(childId);
            const auto hasRestoredReference = [&](const CompositionID& parentId, const LayerID& wrapperId) {
                return std::any_of(restoredReferences.begin(), restoredReferences.end(), [&](const auto& reference) {
                    return reference.parentCompId == parentId && reference.precompLayerId == wrapperId;
                });
            };
            const bool saveReloadRestored = save.success && mappingsCleared && loaded &&
                reloadedRoot && reloadedSecondParent && reloadedChild &&
                reloadedChild->frameRange().duration() == 240 && restoredReferences.size() == 2 &&
                hasRestoredReference(rootId, firstWrapperId) &&
                hasRestoredReference(secondParentId, secondWrapperId) &&
                reloadedFirstWrapper && reloadedSecondWrapper &&
                reloadedFirstWrapper->inPoint().framePosition() == 40 &&
                reloadedFirstWrapper->startTime().framePosition() == 40 &&
                reloadedFirstWrapper->outPoint().framePosition() == 280 &&
                reloadedSecondWrapper->inPoint().framePosition() == 0 &&
                reloadedSecondWrapper->startTime().framePosition() == 0 &&
                reloadedSecondWrapper->outPoint().framePosition() == 240;
            service->changeCurrentComposition(rootId);
            const bool firstParentDeleted = saveReloadRestored &&
                service->unprecomposeLayerWithUndo(firstWrapperId, false);
            const auto referencesAfterDelete = manager.getPrecompLayersForChild(childId);
            const bool sharedChildSurvivedDelete = firstParentDeleted &&
                referencesAfterDelete.size() == 1 &&
                referencesAfterDelete.front().parentCompId == secondParentId &&
                referencesAfterDelete.front().precompLayerId == secondWrapperId &&
                !reloadedRoot->layerById(firstWrapperId) && reloadedRoot->layerById(secondWrapperId) &&
                reloadedChild && reloadedChild->layerById(sourceIds.front()) &&
                reloadedChild->layerById(sourceIds.back()) && manager.isPrecomposeLayer(secondWrapperId);
            const bool deleteUndoAvailable = sharedChildSurvivedDelete && undo->canUndo();
            if (deleteUndoAvailable) undo->undo();
            const auto refsAfterDeleteUndo = manager.getPrecompLayersForChild(childId);
            const bool deleteUndoRestored = deleteUndoAvailable && refsAfterDeleteUndo.size() == 2 &&
                hasRestoredReference(rootId, firstWrapperId) &&
                hasRestoredReference(secondParentId, secondWrapperId) &&
                reloadedRoot->layerById(firstWrapperId) && reloadedRoot->layerById(secondWrapperId) &&
                reloadedChild->layerById(sourceIds.front());
            const bool deleteRedoAvailable = deleteUndoRestored && undo->canRedo();
            if (deleteRedoAvailable) undo->redo();
            const auto refsAfterDeleteRedo = manager.getPrecompLayersForChild(childId);
            const bool deleteRedoRestored = deleteRedoAvailable && refsAfterDeleteRedo.size() == 1 &&
                refsAfterDeleteRedo.front().parentCompId == secondParentId &&
                refsAfterDeleteRedo.front().precompLayerId == secondWrapperId &&
                !reloadedRoot->layerById(firstWrapperId) && reloadedRoot->layerById(secondWrapperId) &&
                reloadedChild->layerById(sourceIds.front());
            const bool restoreDeleteUndoAvailable = deleteRedoRestored && undo->canUndo();
            if (restoreDeleteUndoAvailable) undo->undo();
            const bool deleteUndoRestoredForUnpack = restoreDeleteUndoAvailable &&
                manager.getPrecompLayersForChild(childId).size() == 2 &&
                reloadedRoot->layerById(firstWrapperId) && reloadedRoot->layerById(secondWrapperId);
            const bool firstParentUnpacked = deleteUndoRestoredForUnpack &&
                service->unprecomposeLayerWithUndo(firstWrapperId, true);
            const auto referencesAfterFirstUnpack = manager.getPrecompLayersForChild(childId);
            const bool otherParentReferencePreserved = firstParentUnpacked &&
                referencesAfterFirstUnpack.size() == 1 &&
                referencesAfterFirstUnpack.front().parentCompId == secondParentId &&
                referencesAfterFirstUnpack.front().precompLayerId == secondWrapperId &&
                !manager.isPrecomposeLayer(firstWrapperId) && manager.isPrecomposeLayer(secondWrapperId) &&
                reloadedRoot->layerById(sourceIds.front()) && reloadedRoot->layerById(sourceIds.back()) &&
                reloadedSecondParent->layerById(secondWrapperId) &&
                reloadedChild->layerById(sourceIds.front()) && reloadedChild->layerById(sourceIds.back());
            const bool firstUnpackUndoAvailable = otherParentReferencePreserved && undo->canUndo();
            if (firstUnpackUndoAvailable) undo->undo();
            const auto referencesAfterUndo = manager.getPrecompLayersForChild(childId);
            const bool firstUnpackUndoRestored = firstUnpackUndoAvailable &&
                referencesAfterUndo.size() == 2 && hasRestoredReference(rootId, firstWrapperId) &&
                hasRestoredReference(secondParentId, secondWrapperId) &&
                manager.isPrecomposeLayer(firstWrapperId) && manager.isPrecomposeLayer(secondWrapperId) &&
                reloadedRoot->layerById(firstWrapperId) && !reloadedRoot->layerById(sourceIds.front()) &&
                reloadedChild->layerById(sourceIds.front());
            const bool firstUnpackRedoAvailable = firstUnpackUndoRestored && undo->canRedo();
            if (firstUnpackRedoAvailable) undo->redo();
            const auto referencesAfterRedo = manager.getPrecompLayersForChild(childId);
            const bool firstUnpackRedoRestored = firstUnpackRedoAvailable &&
                referencesAfterRedo.size() == 1 &&
                referencesAfterRedo.front().parentCompId == secondParentId &&
                referencesAfterRedo.front().precompLayerId == secondWrapperId &&
                reloadedRoot->layerById(sourceIds.front()) && reloadedRoot->layerById(sourceIds.back()) &&
                reloadedChild->layerById(sourceIds.front()) && reloadedChild->layerById(sourceIds.back());
            result["bothParentsRegistered"] = bothParentsRegistered;
            result["shrinkFollowed"] = shrinkFollowed;
            result["expansionFollowed"] = expansionFollowed;
            result["saveSucceeded"] = save.success;
            result["mappingsClearedBeforeReload"] = mappingsCleared;
            result["loaded"] = loaded;
            result["saveReloadRestored"] = saveReloadRestored;
            result["firstParentDeleted"] = firstParentDeleted;
            result["sharedChildSurvivedDelete"] = sharedChildSurvivedDelete;
            result["deleteUndoRestored"] = deleteUndoRestored;
            result["deleteRedoRestored"] = deleteRedoRestored;
            result["firstParentUnpacked"] = firstParentUnpacked;
            result["otherParentReferencePreserved"] = otherParentReferencePreserved;
            result["firstUnpackUndoRestored"] = firstUnpackUndoRestored;
            result["firstUnpackRedoRestored"] = firstUnpackRedoRestored;
            result["passed"] = bothParentsRegistered && shrinkFollowed && expansionFollowed &&
                saveReloadRestored && otherParentReferencePreserved && firstUnpackUndoRestored &&
                firstUnpackRedoRestored && sharedChildSurvivedDelete && deleteUndoRestored &&
                deleteRedoRestored;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Shared child duration propagation or save/reload restoration failed");
            childId_ = outcome.childCompId;
            hasChild_ = true;
            service->changeCurrentComposition(rootId);
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.sharedParentDurationPropagates") {
            QJsonObject result;
            const auto root = service->currentComposition().lock();
            if (!root) {
                result["error"] = QStringLiteral("No root composition");
                return result;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2 || !service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Shared Parent Duration Child")), false, false,
                    PrecomposeMode::MoveSelected)) {
                result["error"] = QStringLiteral("Could not create same-parent shared-child fixture");
                return result;
            }
            const auto first = service->lastPrecomposeOutcome();
            service->changeCurrentComposition(rootId);
            const bool secondAdded = service->addCompositionLayerToCurrentCompositionWithUndo(
                first.childCompId);
            LayerID secondWrapperId;
            if (secondAdded) for (const auto& layer : root->allLayer()) {
                const auto wrapper = ArtifactCore::dynamicPointerCast<ArtifactCompositionLayer>(layer);
                if (wrapper && wrapper->sourceCompositionId() == first.childCompId &&
                    wrapper->id() != first.precompLayerId) {
                    secondWrapperId = wrapper->id();
                    break;
                }
            }
            auto& manager = PreComposeManager::instance();
            const auto references = manager.getPrecompLayersForChild(first.childCompId);
            const bool sameParentRegistered = secondAdded && !secondWrapperId.isNil() &&
                references.size() == 2 &&
                std::any_of(references.begin(), references.end(), [&](const auto& reference) {
                    return reference.parentCompId == rootId &&
                        reference.precompLayerId == first.precompLayerId;
                }) &&
                std::any_of(references.begin(), references.end(), [&](const auto& reference) {
                    return reference.parentCompId == rootId &&
                        reference.precompLayerId == secondWrapperId;
                });
            const auto firstWrapper = ArtifactCore::dynamicPointerCast<ArtifactCompositionLayer>(
                root->layerById(first.precompLayerId));
            const auto secondWrapper = ArtifactCore::dynamicPointerCast<ArtifactCompositionLayer>(
                root->layerById(secondWrapperId));
            const auto child = service->findComposition(first.childCompId).ptr.lock();
            if (!sameParentRegistered || !firstWrapper || !secondWrapper || !child) {
                result["error"] = QStringLiteral("Same-parent shared-child fixture is incomplete");
                return result;
            }
            child->setFrameRange(FrameRange(0, 90));
            const bool shrinkFollowed = firstWrapper->inPoint().framePosition() == 40 &&
                firstWrapper->startTime().framePosition() == 40 &&
                firstWrapper->outPoint().framePosition() == 130 &&
                secondWrapper->inPoint().framePosition() == 0 &&
                secondWrapper->startTime().framePosition() == 0 &&
                secondWrapper->outPoint().framePosition() == 90;
            child->setFrameRange(FrameRange(0, 240));
            const bool expansionFollowed = firstWrapper->inPoint().framePosition() == 40 &&
                firstWrapper->startTime().framePosition() == 40 &&
                firstWrapper->outPoint().framePosition() == 280 &&
                secondWrapper->inPoint().framePosition() == 0 &&
                secondWrapper->startTime().framePosition() == 0 &&
                secondWrapper->outPoint().framePosition() == 240;
            result["sameParentRegistered"] = sameParentRegistered;
            result["shrinkFollowed"] = shrinkFollowed;
            result["expansionFollowed"] = expansionFollowed;
            result["passed"] = sameParentRegistered && shrinkFollowed && expansionFollowed;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Shared child duration changes did not update every wrapper in one parent");
            childId_ = first.childCompId;
            hasChild_ = true;
            service->changeCurrentComposition(rootId);
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.internalParentRestored") {
            QJsonObject result;
            if (!comp) {
                result["error"] = QStringLiteral("No current composition");
                return result;
            }
            const auto sourceIds = namedLayerIds(comp, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2) {
                result["error"] = QStringLiteral("Internal-parent fixture is missing A/B layers");
                return result;
            }
            const auto aId = sourceIds[0];
            const auto bId = sourceIds[1];
            const auto a = comp->layerById(aId);
            const auto b = comp->layerById(bId);
            if (!a || !b) {
                result["error"] = QStringLiteral("Internal-parent fixture layers could not be resolved");
                return result;
            }
            b->setParentById(aId);
            const bool parentAuthored = b->parentLayerId() == aId;
            const bool precomposed = parentAuthored && service->precomposeLayersWithUndo(
                sourceIds, UniString(QStringLiteral("Internal Parent")), false, false,
                PrecomposeMode::MoveSelected);
            if (!precomposed) {
                result["error"] = QStringLiteral("Could not precompose the internal-parent fixture");
                return result;
            }
            const auto outcome = service->lastPrecomposeOutcome();
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            const auto movedB = child ? child->layerById(bId) : ArtifactAbstractLayerPtr{};
            const bool childParentPreserved = child && child->layerById(aId) && movedB &&
                movedB->parentLayerId() == aId;
            const bool unpacked = service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const auto unpackedB = comp->layerById(bId);
            const bool unpackParentPreserved = unpacked && unpackedB &&
                comp->layerById(aId) && unpackedB->parentLayerId() == aId;
            const bool undoAvailable = undo->canUndo();
            if (undoAvailable) undo->undo();
            const auto undoChild = service->findComposition(outcome.childCompId).ptr.lock();
            const auto undoB = undoChild ? undoChild->layerById(bId) : ArtifactAbstractLayerPtr{};
            const bool undoParentPreserved = undoAvailable && undoB &&
                undoChild->layerById(aId) && undoB->parentLayerId() == aId;
            const bool redoAvailable = undo->canRedo();
            if (redoAvailable) undo->redo();
            const auto redoB = comp->layerById(bId);
            const bool redoParentPreserved = redoAvailable && redoB &&
                comp->layerById(aId) && redoB->parentLayerId() == aId;
            result["parentAuthored"] = parentAuthored;
            result["childParentPreserved"] = childParentPreserved;
            result["unpackParentPreserved"] = unpackParentPreserved;
            result["undoParentPreserved"] = undoParentPreserved;
            result["redoParentPreserved"] = redoParentPreserved;
            result["passed"] = childParentPreserved && unpackParentPreserved &&
                undoParentPreserved && redoParentPreserved;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Internal layer parenting changed across precompose/unpack/undo/redo");
            childId_ = outcome.childCompId;
            hasChild_ = true;
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.transitiveCycleRejected") {
            QJsonObject cycleResult;
            auto root = service->currentComposition().lock();
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
            const auto nestedSourceIds = namedLayerIds(child, {QStringLiteral("A"), QStringLiteral("B")});
            if (nestedSourceIds.size() != 2 || !service->precomposeLayersWithUndo(
                    nestedSourceIds, UniString(QStringLiteral("Cycle Grandchild")), false, false,
                    PrecomposeMode::MoveSelected)) {
                service->changeCurrentComposition(rootId);
                cycleResult["error"] = QStringLiteral("Could not create cycle grandchild fixture");
                return cycleResult;
            }
            const auto inner = service->lastPrecomposeOutcome();
            service->changeCurrentComposition(inner.childCompId);
            const auto grandchild = service->currentComposition().lock();
            if (!grandchild) {
                service->changeCurrentComposition(rootId);
                cycleResult["error"] = QStringLiteral("Could not activate cycle grandchild fixture");
                return cycleResult;
            }
            ArtifactCompositionLayerInitParams params;
            params.setName(UniString(QStringLiteral("Deep Transitive Cycle Candidate")));
            service->addLayerToCurrentComposition(params, false);
            const auto candidateIds = namedLayerIds(grandchild, {QStringLiteral("Deep Transitive Cycle Candidate")});
            const auto candidate = candidateIds.isEmpty()
                ? SharedPtr<ArtifactCompositionLayer>{}
                : dynamicPointerCast<ArtifactCompositionLayer>(grandchild->layerById(candidateIds.front()));
            if (!candidate) {
                service->changeCurrentComposition(rootId);
                cycleResult["error"] = QStringLiteral("Could not create deep composition-layer fixture");
                return cycleResult;
            }
            candidate->setCompositionId(rootId);
            const auto layerCountBefore = grandchild->allLayer().size();
            const auto compositionCountBefore = ArtifactProjectManager::getInstance().compositionCount();
            const auto undoCountBefore = undo->undoCount();
            const bool rejected = !service->precomposeLayersWithUndo(
                {candidate->id()}, UniString(QStringLiteral("Rejected Transitive Cycle")),
                false, false, PrecomposeMode::MoveSelected);
            const auto outcome = service->lastPrecomposeOutcome();
            const bool unchanged = grandchild->allLayer().size() == layerCountBefore &&
                grandchild->layerById(candidate->id()) &&
                !PreComposeManager::instance().isPrecomposeLayer(candidate->id()) &&
                outcome.precompLayerId.isNil() && outcome.childCompId.isNil() &&
                ArtifactProjectManager::getInstance().compositionCount() == compositionCountBefore &&
                undo->undoCount() == undoCountBefore;
            const bool removed = service->removeLayerFromComposition(grandchild->id(), candidate->id());
            service->changeCurrentComposition(rootId);
            childId_ = inner.childCompId;
            hasChild_ = true;
            cycleResult["transitiveCycleRejected"] = rejected;
            cycleResult["threeLevelCycleRejected"] = rejected;
            cycleResult["stateUnchanged"] = unchanged;
            cycleResult["fixtureCleaned"] = removed;
            cycleResult["passed"] = rejected && unchanged && removed;
            if (!cycleResult["passed"].toBool())
                cycleResult["error"] = QStringLiteral("Three-level transitive cycle was accepted or mutated the project");
            refresh();
            cycleResult["state"] = state();
            return cycleResult;
        } else if (name == "test.nestedExposedOverrideMaterialized") {
            QJsonObject overrideResult;
            auto root = service->currentComposition().lock();
            if (!root) {
                overrideResult["error"] = QStringLiteral("No root composition");
                return overrideResult;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2 || !service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Inner Exposed Override")), false, false,
                    PrecomposeMode::MoveSelected)) {
                overrideResult["error"] = QStringLiteral("Could not create nested exposed-override fixture");
                return overrideResult;
            }
            const auto inner = service->lastPrecomposeOutcome();
            auto innerChild = service->findComposition(inner.childCompId).ptr.lock();
            auto innerWrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                root->layerById(inner.precompLayerId));
            auto sourceLayer = innerChild
                ? innerChild->layerById(sourceIds.front()) : ArtifactAbstractLayerPtr{};
            if (!innerChild || !innerWrapper || !sourceLayer) {
                overrideResult["error"] = QStringLiteral("Inner exposed-override fixture is incomplete");
                return overrideResult;
            }
            ArtifactCore::ExposedPropertyRegistry registry;
            ArtifactCore::ExposedPropertyBinding binding;
            binding.id = QStringLiteral("test.nested.opacity");
            binding.label = QStringLiteral("Nested Test Opacity");
            binding.targetLayerId = sourceIds.front().toString();
            binding.internalPath = QStringLiteral("layer.opacity");
            binding.defaultValue = sourceLayer->opacity();
            const float overrideValue = 0.73f;
            const bool bindingAdded = registry.add(binding);
            innerWrapper->setExposedProperties(registry);
            const bool overrideAccepted = bindingAdded && innerWrapper->setExposedPropertyOverride(
                binding.id, QVariant::fromValue(static_cast<double>(overrideValue)));
            const auto retainsOverride = [&](const SharedPtr<ArtifactCompositionLayer>& layer) {
                return layer && layer->exposedProperties().contains(binding.id) &&
                    layer->hasExposedPropertyOverride(binding.id) &&
                    std::abs(layer->effectiveExposedPropertyValue(binding.id).toFloat() - overrideValue) < 1.0e-5f;
            };
            const bool overrideDeferred = overrideAccepted && retainsOverride(innerWrapper) &&
                std::abs(sourceLayer->opacity() - binding.defaultValue.toFloat()) < 1.0e-5f;
            const bool outerCreated = overrideDeferred && service->precomposeLayersWithUndo(
                {inner.precompLayerId}, UniString(QStringLiteral("Outer Exposed Override")), false, false,
                PrecomposeMode::MoveSelected);
            const auto outer = service->lastPrecomposeOutcome();
            auto outerWrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                root->layerById(outer.precompLayerId));
            auto outerChild = service->findComposition(outer.childCompId).ptr.lock();
            auto nestedWrapper = outerChild
                ? dynamicPointerCast<ArtifactCompositionLayer>(outerChild->layerById(inner.precompLayerId))
                : SharedPtr<ArtifactCompositionLayer>{};
            ArtifactCore::ExposedPropertyRegistry outerRegistry;
            ArtifactCore::ExposedPropertyBinding outerBinding;
            outerBinding.id = QStringLiteral("test.outer.wrapper.opacity");
            outerBinding.label = QStringLiteral("Outer Wrapper Opacity");
            outerBinding.targetLayerId = inner.precompLayerId.toString();
            outerBinding.internalPath = QStringLiteral("layer.opacity");
            outerBinding.defaultValue = nestedWrapper ? nestedWrapper->opacity() : 1.0f;
            const float outerOverrideValue = 0.36f;
            const bool outerBindingAdded = outerCreated && outerWrapper && nestedWrapper &&
                outerRegistry.add(outerBinding);
            if (outerWrapper) outerWrapper->setExposedProperties(outerRegistry);
            const bool outerOverrideAccepted = outerBindingAdded && outerWrapper->setExposedPropertyOverride(
                outerBinding.id, QVariant::fromValue(static_cast<double>(outerOverrideValue)));
            const bool outerOverrideDeferred = outerOverrideAccepted &&
                outerWrapper->hasExposedPropertyOverride(outerBinding.id) &&
                std::abs(outerWrapper->effectiveExposedPropertyValue(outerBinding.id).toFloat() -
                         outerOverrideValue) < 1.0e-5f &&
                std::abs(nestedWrapper->opacity() - outerBinding.defaultValue.toFloat()) < 1.0e-5f;
            const bool nestedOverrideInMemory = outerCreated && nestedWrapper == innerWrapper &&
                retainsOverride(nestedWrapper) &&
                std::abs(sourceLayer->opacity() - binding.defaultValue.toFloat()) < 1.0e-5f &&
                outerOverrideDeferred;
            bool saveReloadPreserved = false;
            bool outerUnpacked = false;
            QTemporaryDir tempDir;
            if (nestedOverrideInMemory && tempDir.isValid()) {
                auto& projects = ArtifactProjectManager::getInstance();
                auto& manager = PreComposeManager::instance();
                const auto filePath = tempDir.filePath(QStringLiteral("nested-exposed-override.artifact"));
                const auto save = projects.saveToFile(filePath);
                const bool innerMappingCleared = save.success && manager.unregisterPrecompLayer(
                    outer.childCompId, inner.precompLayerId, inner.childCompId);
                const bool outerMappingCleared = save.success && manager.unregisterPrecompLayer(
                    rootId, outer.precompLayerId, outer.childCompId);
                const bool mappingsCleared = innerMappingCleared && outerMappingCleared &&
                    !manager.isPrecomposeLayer(inner.precompLayerId) &&
                    !manager.isPrecomposeLayer(outer.precompLayerId) &&
                    manager.getPrecompLayersForChild(outer.childCompId).isEmpty() &&
                    manager.getPrecompLayersForChild(inner.childCompId).isEmpty();
                const bool loaded = mappingsCleared && projects.loadFromFile(filePath);
                if (loaded) {
                    service->changeCurrentComposition(rootId);
                    root = service->findComposition(rootId).ptr.lock();
                    outerChild = service->findComposition(outer.childCompId).ptr.lock();
                    innerChild = service->findComposition(inner.childCompId).ptr.lock();
                    nestedWrapper = outerChild
                        ? dynamicPointerCast<ArtifactCompositionLayer>(
                              outerChild->layerById(inner.precompLayerId))
                        : SharedPtr<ArtifactCompositionLayer>{};
                    outerWrapper = root
                        ? dynamicPointerCast<ArtifactCompositionLayer>(root->layerById(outer.precompLayerId))
                        : SharedPtr<ArtifactCompositionLayer>{};
                    innerWrapper = nestedWrapper;
                    sourceLayer = innerChild
                        ? innerChild->layerById(sourceIds.front()) : ArtifactAbstractLayerPtr{};
                }
                bool outerMappingRestored = false;
                for (const auto& reference : manager.getPrecompLayersForChild(outer.childCompId)) {
                    outerMappingRestored = outerMappingRestored ||
                        (reference.parentCompId == rootId &&
                         reference.precompLayerId == outer.precompLayerId);
                }
                bool innerMappingRestored = false;
                for (const auto& reference : manager.getPrecompLayersForChild(inner.childCompId)) {
                    innerMappingRestored = innerMappingRestored ||
                        (reference.parentCompId == outer.childCompId &&
                         reference.precompLayerId == inner.precompLayerId);
                }
                saveReloadPreserved = save.success && mappingsCleared && loaded && root &&
                    outerChild && innerChild && outerWrapper && nestedWrapper && sourceLayer &&
                    retainsOverride(nestedWrapper) && outerMappingRestored && innerMappingRestored &&
                    outerWrapper->hasExposedPropertyOverride(outerBinding.id) &&
                    std::abs(outerWrapper->effectiveExposedPropertyValue(outerBinding.id).toFloat() -
                             outerOverrideValue) < 1.0e-5f &&
                    std::abs(nestedWrapper->opacity() - outerBinding.defaultValue.toFloat()) < 1.0e-5f &&
                    std::abs(sourceLayer->opacity() - binding.defaultValue.toFloat()) < 1.0e-5f;
                outerUnpacked = saveReloadPreserved &&
                    service->unprecomposeLayerWithUndo(outer.precompLayerId, true);
            }
            const auto restoredInner = outerUnpacked
                ? dynamicPointerCast<ArtifactCompositionLayer>(root->layerById(inner.precompLayerId))
                : SharedPtr<ArtifactCompositionLayer>{};
            const bool innerOverridePreserved = outerUnpacked && restoredInner == innerWrapper &&
                retainsOverride(restoredInner) &&
                std::abs(restoredInner->opacity() - outerOverrideValue) < 1.0e-5f &&
                std::abs(sourceLayer->opacity() - binding.defaultValue.toFloat()) < 1.0e-5f;
            const bool innerUnpacked = innerOverridePreserved &&
                service->unprecomposeLayerWithUndo(inner.precompLayerId, true);
            const auto restoredSource = root
                ? root->layerById(sourceIds.front()) : ArtifactAbstractLayerPtr{};
            const bool overrideMaterialized = innerUnpacked && restoredSource == sourceLayer &&
                std::abs(restoredSource->opacity() - overrideValue) < 1.0e-5f;
            const bool undoAvailable = innerUnpacked && undo->canUndo();
            if (undoAvailable) undo->undo();
            const auto undoChild = service->findComposition(inner.childCompId).ptr.lock();
            const auto undoSource = undoChild ? undoChild->layerById(sourceIds.front()) : ArtifactAbstractLayerPtr{};
            const auto undoWrapper = root
                ? dynamicPointerCast<ArtifactCompositionLayer>(root->layerById(inner.precompLayerId))
                : SharedPtr<ArtifactCompositionLayer>{};
            const bool undoPreservedOverride = undoAvailable && undoSource == sourceLayer &&
                undoWrapper == innerWrapper && retainsOverride(undoWrapper) &&
                std::abs(undoSource->opacity() - overrideValue) < 1.0e-5f;
            const bool redoAvailable = undoAvailable && undo->canRedo();
            if (redoAvailable) undo->redo();
            const auto redoSource = root
                ? root->layerById(sourceIds.front()) : ArtifactAbstractLayerPtr{};
            const bool redoPreservedOverride = redoAvailable && redoSource == sourceLayer &&
                std::abs(redoSource->opacity() - overrideValue) < 1.0e-5f &&
                !PreComposeManager::instance().isPrecomposeLayer(inner.precompLayerId);
            overrideResult["bindingAdded"] = bindingAdded;
            overrideResult["overrideDeferred"] = overrideDeferred;
            overrideResult["outerOverrideDeferred"] = outerOverrideDeferred;
            overrideResult["nestedOverridePreserved"] = nestedOverrideInMemory;
            overrideResult["saveReloadPreserved"] = saveReloadPreserved;
            overrideResult["outerUnpacked"] = outerUnpacked;
            overrideResult["innerOverridePreserved"] = innerOverridePreserved;
            overrideResult["overrideMaterialized"] = overrideMaterialized;
            overrideResult["undoPreservedOverride"] = undoPreservedOverride;
            overrideResult["redoPreservedOverride"] = redoPreservedOverride;
            overrideResult["passed"] = bindingAdded && overrideDeferred && outerOverrideDeferred && nestedOverrideInMemory &&
                saveReloadPreserved && outerUnpacked && innerOverridePreserved && overrideMaterialized &&
                undoPreservedOverride && redoPreservedOverride;
            if (!overrideResult["passed"].toBool())
                overrideResult["error"] = QStringLiteral("Nested Master Property override changed before or after final unpack/undo/redo");
            service->changeCurrentComposition(rootId);
            childId_ = outer.childCompId;
            hasChild_ = true;
            refresh();
            overrideResult["state"] = state();
            return overrideResult;
        } else if (name == "test.exposedOverrideMaterialized") {
            QJsonObject overrideResult;
            const auto root = service->currentComposition().lock();
            if (!root) {
                overrideResult["error"] = QStringLiteral("No root composition");
                return overrideResult;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2 || !service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Exposed Override Fixture")), false, false,
                    PrecomposeMode::MoveSelected)) {
                overrideResult["error"] = QStringLiteral("Could not create exposed-override fixture");
                return overrideResult;
            }
            const auto outcome = service->lastPrecomposeOutcome();
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            const auto wrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                root->layerById(outcome.precompLayerId));
            const auto childLayer = child
                ? child->layerById(sourceIds.front()) : ArtifactAbstractLayerPtr{};
            if (!child || !wrapper || !childLayer) {
                overrideResult["error"] = QStringLiteral("Exposed-override fixture is missing its child layer or wrapper");
                return overrideResult;
            }
            ArtifactCore::ExposedPropertyRegistry registry;
            ArtifactCore::ExposedPropertyBinding binding;
            binding.id = QStringLiteral("test.opacity");
            binding.label = QStringLiteral("Test Opacity");
            binding.targetLayerId = sourceIds.front().toString();
            binding.internalPath = QStringLiteral("layer.opacity");
            binding.defaultValue = childLayer->opacity();
            const bool bindingAdded = registry.add(binding);
            wrapper->setExposedProperties(registry);
            const float overrideValue = 0.81f;
            const bool overrideAccepted = bindingAdded && wrapper->setExposedPropertyOverride(
                binding.id, QVariant::fromValue(static_cast<double>(overrideValue)));
            const float originalChildOpacity = childLayer->opacity();
            const bool overrideWasDeferred = overrideAccepted &&
                std::abs(originalChildOpacity - 0.42f) < 1.0e-5f &&
                wrapper->hasExposedPropertyOverride(binding.id) &&
                std::abs(wrapper->effectiveExposedPropertyValue(binding.id).toFloat() - overrideValue) < 1.0e-5f;
            const bool unpacked = service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const auto restoredLayer = root->layerById(sourceIds.front());
            const bool unpackMaterialized = unpacked && restoredLayer == childLayer &&
                std::abs(restoredLayer->opacity() - overrideValue) < 1.0e-5f;
            const bool undoAvailable = undo->canUndo();
            if (undoAvailable) undo->undo();
            const auto undoChild = service->findComposition(outcome.childCompId).ptr.lock();
            const auto undoLayer = undoChild ? undoChild->layerById(sourceIds.front()) : ArtifactAbstractLayerPtr{};
            const bool undoPreservedMaterializedValue = undoAvailable && undoChild && undoLayer == childLayer &&
                root->layerById(outcome.precompLayerId) &&
                PreComposeManager::instance().isPrecomposeLayer(outcome.precompLayerId) &&
                std::abs(undoLayer->opacity() - overrideValue) < 1.0e-5f;
            const bool redoAvailable = undo->canRedo();
            if (redoAvailable) undo->redo();
            const auto redoLayer = root->layerById(sourceIds.front());
            const bool redoPreservedMaterializedValue = redoAvailable && redoLayer == childLayer &&
                !PreComposeManager::instance().isPrecomposeLayer(outcome.precompLayerId) &&
                std::abs(redoLayer->opacity() - overrideValue) < 1.0e-5f;
            service->changeCurrentComposition(rootId);
            childId_ = outcome.childCompId;
            hasChild_ = true;
            overrideResult["bindingAdded"] = bindingAdded;
            overrideResult["overrideAccepted"] = overrideAccepted;
            overrideResult["overrideWasDeferred"] = overrideWasDeferred;
            overrideResult["unpackMaterialized"] = unpackMaterialized;
            overrideResult["undoPreservedMaterializedValue"] = undoPreservedMaterializedValue;
            overrideResult["redoPreservedMaterializedValue"] = redoPreservedMaterializedValue;
            overrideResult["passed"] = bindingAdded && overrideAccepted && overrideWasDeferred &&
                unpackMaterialized && undoPreservedMaterializedValue && redoPreservedMaterializedValue;
            if (!overrideResult["passed"].toBool())
                overrideResult["error"] = QStringLiteral("Master Property override was not materialized consistently across unprecompose/undo/redo");
            refresh();
            overrideResult["state"] = state();
            return overrideResult;
        } else if (name == "test.missingChildReferenceAtomic") {
            QJsonObject missingResult;
            const auto root = service->currentComposition().lock();
            if (!root) {
                missingResult["error"] = QStringLiteral("No root composition");
                return missingResult;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2 || !service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Missing Child Reference Fixture")), false, false,
                    PrecomposeMode::MoveSelected)) {
                missingResult["error"] = QStringLiteral("Could not create missing-child-reference fixture");
                return missingResult;
            }
            const auto outcome = service->lastPrecomposeOutcome();
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            const auto wrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                root->layerById(outcome.precompLayerId));
            if (!child || !wrapper || wrapper->sourceCompositionId() != outcome.childCompId) {
                missingResult["error"] = QStringLiteral("Missing-child-reference fixture is incomplete");
                return missingResult;
            }
            childId_ = outcome.childCompId;
            hasChild_ = true;
            wrapper->setCompositionId(CompositionID());
            const auto brokenReferenceLayers = state()["layers"].toArray();
            const auto compositionCountBeforeUnwrap = ArtifactProjectManager::getInstance().compositionCount();
            const auto undoCountBeforeUnwrap = undo->undoCount();
            const bool rejected = !service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const bool parentStatePreserved = state()["layers"].toArray() == brokenReferenceLayers;
            const bool compositionPreserved = service->findComposition(outcome.childCompId).ptr.lock() == child &&
                !child->allLayer().isEmpty() &&
                ArtifactProjectManager::getInstance().compositionCount() == compositionCountBeforeUnwrap;
            const bool wrapperPreserved = root->layerById(outcome.precompLayerId).get() == wrapper.get() &&
                wrapper->sourceCompositionId().isNil() &&
                PreComposeManager::instance().isPrecomposeLayer(outcome.precompLayerId);
            const bool undoHistoryPreserved = undo->undoCount() == undoCountBeforeUnwrap;
            const bool outcomeCleared = service->lastUnprecomposePrecompLayerId().isNil() &&
                service->lastUnprecomposeChildCompId().isNil() &&
                service->lastUnprecomposeMovedLayerIds().isEmpty();
            wrapper->setCompositionId(outcome.childCompId);
            const bool fixtureRestored = wrapper->sourceCompositionId() == outcome.childCompId &&
                service->findComposition(outcome.childCompId).ptr.lock() == child;
            service->changeCurrentComposition(rootId);
            missingResult["unwrapRejected"] = rejected;
            missingResult["parentStatePreserved"] = parentStatePreserved;
            missingResult["compositionPreserved"] = compositionPreserved;
            missingResult["wrapperPreserved"] = wrapperPreserved;
            missingResult["undoHistoryPreserved"] = undoHistoryPreserved;
            missingResult["outcomeCleared"] = outcomeCleared;
            missingResult["fixtureRestored"] = fixtureRestored;
            missingResult["passed"] = rejected && parentStatePreserved && compositionPreserved &&
                wrapperPreserved && undoHistoryPreserved && outcomeCleared && fixtureRestored;
            if (!missingResult["passed"].toBool())
                missingResult["error"] = QStringLiteral("Unprecompose mutated a broken child reference or failed to restore the fixture");
            refresh();
            missingResult["state"] = state();
            return missingResult;
        } else if (name == "test.emptyChildUnwrapRejected") {
            QJsonObject emptyResult;
            const auto root = service->currentComposition().lock();
            if (!root) {
                emptyResult["error"] = QStringLiteral("No root composition");
                return emptyResult;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2 || !service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Empty Child Fixture")), false, false,
                    PrecomposeMode::MoveSelected)) {
                emptyResult["error"] = QStringLiteral("Could not create empty-child precompose fixture");
                return emptyResult;
            }
            const auto outcome = service->lastPrecomposeOutcome();
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            if (!child || child->allLayer().isEmpty()) {
                emptyResult["error"] = QStringLiteral("Precompose fixture child is missing its source layers");
                return emptyResult;
            }
            QVector<LayerID> childLayerIds;
            childLayerIds.reserve(child->allLayer().size());
            for (const auto& layer : child->allLayer()) {
                if (layer) childLayerIds.push_back(layer->id());
            }
            bool childEmptied = !childLayerIds.isEmpty();
            for (const auto& id : childLayerIds) {
                childEmptied = service->removeLayerFromComposition(child->id(), id) && childEmptied;
            }
            const auto parentLayersBeforeUnwrap = state()["layers"].toArray();
            const auto compositionCountBeforeUnwrap = ArtifactProjectManager::getInstance().compositionCount();
            const auto undoCountBeforeUnwrap = undo->undoCount();
            const bool rejected = !service->unprecomposeLayerWithUndo(outcome.precompLayerId, false);
            const auto wrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                root->layerById(outcome.precompLayerId));
            const bool wrapperPreserved = wrapper &&
                wrapper->sourceCompositionId() == outcome.childCompId &&
                PreComposeManager::instance().isPrecomposeLayer(outcome.precompLayerId);
            const bool childPreserved = service->findComposition(outcome.childCompId).ptr.lock() == child &&
                child->allLayer().isEmpty() &&
                ArtifactProjectManager::getInstance().compositionCount() == compositionCountBeforeUnwrap;
            const bool parentStatePreserved = state()["layers"].toArray() == parentLayersBeforeUnwrap;
            const bool undoHistoryPreserved = undo->undoCount() == undoCountBeforeUnwrap;
            const bool outcomeCleared = service->lastUnprecomposePrecompLayerId().isNil() &&
                service->lastUnprecomposeChildCompId().isNil() &&
                service->lastUnprecomposeMovedLayerIds().isEmpty();
            service->changeCurrentComposition(rootId);
            childId_ = outcome.childCompId;
            hasChild_ = true;
            emptyResult["childEmptied"] = childEmptied && child->allLayer().isEmpty();
            emptyResult["unwrapRejected"] = rejected;
            emptyResult["wrapperPreserved"] = wrapperPreserved;
            emptyResult["childPreserved"] = childPreserved;
            emptyResult["parentStatePreserved"] = parentStatePreserved;
            emptyResult["undoHistoryPreserved"] = undoHistoryPreserved;
            emptyResult["outcomeCleared"] = outcomeCleared;
            emptyResult["passed"] = childEmptied && rejected && wrapperPreserved && childPreserved &&
                parentStatePreserved && undoHistoryPreserved && outcomeCleared;
            if (!emptyResult["passed"].toBool())
                emptyResult["error"] = QStringLiteral("Unprecompose consumed or mutated an empty child fixture");
            refresh();
            emptyResult["state"] = state();
            return emptyResult;
        } else if (name == "test.emptyMoveAllRejectedAtomic") {
            QJsonObject emptyResult;
            const auto root = service->currentComposition().lock();
            const auto project = service->getCurrentProjectSharedPtr();
            if (!root || !project) {
                emptyResult["error"] = QStringLiteral("Empty MoveAll fixture is unavailable");
                return emptyResult;
            }
            const auto rootId = root->id();
            const auto originalLayers = root->allLayer();
            if (originalLayers.isEmpty()) {
                emptyResult["error"] = QStringLiteral("Root fixture has no layers to remove");
                return emptyResult;
            }
            const auto compositionCountBefore = ArtifactProjectManager::getInstance().compositionCount();
            const auto undoCountBefore = undo->undoCount();
            const auto redoCountBefore = undo->redoCount();
            bool emptied = true;
            for (const auto& layer : originalLayers) {
                emptied = layer && project->removeLayerFromComposition(rootId, layer->id()) && emptied;
            }
            const bool emptyFixtureReady = emptied && root->allLayer().isEmpty();
            const bool rejected = emptyFixtureReady && !service->precomposeLayersWithUndo(
                {}, UniString(QStringLiteral("Empty Move All")), false, false,
                PrecomposeMode::MoveAllAttributes);
            const auto outcome = service->lastPrecomposeOutcome();
            const bool rejectionAtomic = rejected && root->allLayer().isEmpty() &&
                ArtifactProjectManager::getInstance().compositionCount() == compositionCountBefore &&
                undo->undoCount() == undoCountBefore && undo->redoCount() == redoCountBefore &&
                outcome.precompLayerId.isNil() && outcome.childCompId.isNil();
            bool fixtureRestored = true;
            for (const auto& layer : originalLayers) {
                if (!layer) {
                    fixtureRestored = false;
                    continue;
                }
                if (!root->containsLayerById(layer->id()) &&
                    !project->addLayerToComposition(rootId, layer).success) {
                    fixtureRestored = false;
                    continue;
                }
                layer->setComposition(root.get());
            }
            service->selectLayer(LayerID());
            const auto restoredLayers = root->allLayer();
            bool originalOrderRestored = restoredLayers.size() == originalLayers.size();
            for (int i = 0; originalOrderRestored && i < originalLayers.size(); ++i) {
                originalOrderRestored = restoredLayers[i] &&
                    restoredLayers[i]->id() == originalLayers[i]->id();
            }
            emptyResult["emptyFixtureReady"] = emptyFixtureReady;
            emptyResult["rejected"] = rejected;
            emptyResult["rejectionAtomic"] = rejectionAtomic;
            emptyResult["fixtureRestored"] = fixtureRestored && originalOrderRestored;
            emptyResult["passed"] = emptyFixtureReady && rejected && rejectionAtomic &&
                fixtureRestored && originalOrderRestored;
            if (!emptyResult["passed"].toBool())
                emptyResult["error"] = QStringLiteral("Empty MoveAll precompose mutated state or failed to restore its test fixture");
            refresh();
            emptyResult["state"] = state();
            return emptyResult;
        } else if (name == "test.unpackDeleteRedo") {
            QJsonObject redoResult;
            const auto root = service->currentComposition().lock();
            if (!root) {
                redoResult["error"] = QStringLiteral("No root composition");
                return redoResult;
            }
            const auto rootId = root->id();
            const auto originalLayers = state()["layers"].toArray();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2 || !service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Delete Redo Fixture")), false, false,
                    PrecomposeMode::MoveSelected)) {
                redoResult["error"] = QStringLiteral("Could not create delete-redo precompose fixture");
                return redoResult;
            }
            const auto outcome = service->lastPrecomposeOutcome();
            childId_ = outcome.childCompId;
            hasChild_ = true;
            const auto compositionCountBeforeDelete = ArtifactProjectManager::getInstance().compositionCount();
            const bool unpacked = service->unprecomposeLayerWithUndo(outcome.precompLayerId, false);
            const bool deletedAfterUnpack = unpacked &&
                !service->findComposition(outcome.childCompId).ptr.lock() &&
                ArtifactProjectManager::getInstance().compositionCount() + 1 == compositionCountBeforeDelete &&
                state()["layers"].toArray() == originalLayers;
            const bool undoAvailable = undo->canUndo();
            if (undoAvailable) undo->undo();
            const bool restoredByUndo = undoAvailable &&
                service->findComposition(outcome.childCompId).ptr.lock() &&
                ArtifactProjectManager::getInstance().compositionCount() == compositionCountBeforeDelete &&
                root->layerById(outcome.precompLayerId) &&
                PreComposeManager::instance().isPrecomposeLayer(outcome.precompLayerId);
            const bool redoAvailable = undo->canRedo();
            if (redoAvailable) undo->redo();
            const bool deletedByRedo = redoAvailable &&
                !service->findComposition(outcome.childCompId).ptr.lock() &&
                ArtifactProjectManager::getInstance().compositionCount() + 1 == compositionCountBeforeDelete &&
                !root->layerById(outcome.precompLayerId) &&
                !PreComposeManager::instance().isPrecomposeLayer(outcome.precompLayerId) &&
                state()["layers"].toArray() == originalLayers;
            service->changeCurrentComposition(rootId);
            redoResult["deletedAfterUnpack"] = deletedAfterUnpack;
            redoResult["restoredByUndo"] = restoredByUndo;
            redoResult["deletedByRedo"] = deletedByRedo;
            redoResult["passed"] = deletedAfterUnpack && restoredByUndo && deletedByRedo;
            if (!redoResult["passed"].toBool())
                redoResult["error"] = QStringLiteral("Delete-unpack Undo/Redo did not restore the child and source layers consistently");
            refresh();
            redoResult["state"] = state();
            return redoResult;
        } else if (name == "test.singleLayerRoundTrip") {
            QJsonObject result;
            const auto root = service->currentComposition().lock();
            if (!root) {
                result["error"] = QStringLiteral("No root composition");
                return result;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A")});
            if (sourceIds.size() != 1) {
                result["error"] = QStringLiteral("Single-layer fixture is missing A");
                return result;
            }
            const auto sourceId = sourceIds.front();
            const auto originalLayer = root->layerById(sourceId);
            const auto originalLayerIds = [&]() {
                QVector<LayerID> ids;
                for (const auto& layer : root->allLayer()) if (layer) ids.append(layer->id());
                return ids;
            }();
            const int sourceIndex = originalLayerIds.indexOf(sourceId);
            const bool precomposed = originalLayer && service->precomposeLayersWithUndo(
                {sourceId}, UniString(QStringLiteral("Single Layer Fixture")), false, false,
                PrecomposeMode::MoveSelected);
            const auto outcome = service->lastPrecomposeOutcome();
            const auto child = precomposed
                ? service->findComposition(outcome.childCompId).ptr.lock()
                : ArtifactCompositionPtr{};
            const auto childLayers = child
                ? child->allLayer() : decltype(child->allLayer()){};
            QVector<LayerID> wrappedRootIds;
            for (const auto& layer : root->allLayer()) if (layer) wrappedRootIds.append(layer->id());
            auto expectedWrappedIds = originalLayerIds;
            if (sourceIndex >= 0 && !outcome.precompLayerId.isNil())
                expectedWrappedIds[sourceIndex] = outcome.precompLayerId;
            const bool wrappedCorrectly = precomposed && child && childLayers.size() == 1 &&
                childLayers.front() && childLayers.front()->id() == sourceId &&
                childLayers.front().get() == originalLayer.get() &&
                root->layerById(outcome.precompLayerId) &&
                sourceIndex >= 0 && wrappedRootIds == expectedWrappedIds &&
                !root->layerById(sourceId) &&
                PreComposeManager::instance().getSourceCompositionId(outcome.precompLayerId) ==
                    outcome.childCompId;
            const bool undoAvailable = undo->canUndo();
            if (undoAvailable) undo->undo();
            const bool undoRestored = undoAvailable &&
                !service->findComposition(outcome.childCompId).ptr.lock() &&
                !root->layerById(outcome.precompLayerId) &&
                root->layerById(sourceId).get() == originalLayer.get() &&
                [&]() {
                    QVector<LayerID> ids;
                    for (const auto& layer : root->allLayer()) if (layer) ids.append(layer->id());
                    return ids == originalLayerIds;
                }() && !PreComposeManager::instance().isPrecomposeLayer(outcome.precompLayerId);
            const bool redoAvailable = undo->canRedo();
            if (redoAvailable) undo->redo();
            const auto redoneChild = service->findComposition(outcome.childCompId).ptr.lock();
            const auto redoneLayer = redoneChild ? redoneChild->layerById(sourceId)
                                                 : ArtifactAbstractLayerPtr{};
            const bool redoRestored = redoAvailable && redoneChild && redoneLayer &&
                redoneLayer.get() == originalLayer.get() && root->layerById(outcome.precompLayerId) &&
                PreComposeManager::instance().getSourceCompositionId(outcome.precompLayerId) ==
                    outcome.childCompId && !root->layerById(sourceId);
            result["wrappedCorrectly"] = wrappedCorrectly;
            result["undoRestored"] = undoRestored;
            result["redoRestored"] = redoRestored;
            result["passed"] = wrappedCorrectly && undoRestored && redoRestored;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Single-layer precompose changed layer identity, order, or registration across undo/redo");
            childId_ = outcome.childCompId;
            hasChild_ = !childId_.isNil();
            service->changeCurrentComposition(rootId);
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.layerNotesRoundTrip") {
            QJsonObject result;
            const auto root = service->currentComposition().lock();
            if (!root) {
                result["error"] = QStringLiteral("No root composition");
                return result;
            }
            const auto rootId = root->id();
            const auto ids = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B"),
                                                   QStringLiteral("Untouched")});
            if (ids.size() != 3) {
                result["error"] = QStringLiteral("Layer-note fixture is missing A, B, or Untouched");
                return result;
            }
            const QString noteA = QStringLiteral("A memo / 子");
            const QString noteB = QStringLiteral("B memo\nline 2");
            const QString noteUntouched = QStringLiteral("Keep this note");
            const auto layerA = root->layerById(ids[0]);
            const auto layerB = root->layerById(ids[1]);
            const auto untouched = root->layerById(ids[2]);
            if (!layerA || !layerB || !untouched) {
                result["error"] = QStringLiteral("Layer-note fixture layers could not be resolved");
                return result;
            }
            layerA->setLayerNote(noteA);
            layerB->setLayerNote(noteB);
            untouched->setLayerNote(noteUntouched);
            const bool precomposed = service->precomposeLayersWithUndo(
                {ids[0], ids[1]}, UniString(QStringLiteral("Layer Notes Fixture")), false, false,
                PrecomposeMode::MoveSelected);
            const auto outcome = service->lastPrecomposeOutcome();
            const auto child = precomposed
                ? service->findComposition(outcome.childCompId).ptr.lock() : ArtifactCompositionPtr{};
            const auto childA = child ? child->layerById(ids[0]) : ArtifactAbstractLayerPtr{};
            const auto childB = child ? child->layerById(ids[1]) : ArtifactAbstractLayerPtr{};
            const bool notesMoved = childA && childB && childA->layerNote() == noteA &&
                childB->layerNote() == noteB && untouched->layerNote() == noteUntouched;
            const bool unpacked = precomposed &&
                service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const bool notesUnpacked = unpacked && root->layerById(ids[0]) && root->layerById(ids[1]) &&
                root->layerById(ids[0])->layerNote() == noteA &&
                root->layerById(ids[1])->layerNote() == noteB &&
                untouched->layerNote() == noteUntouched;
            const bool undoAvailable = undo->canUndo();
            if (undoAvailable) undo->undo();
            const auto undoChild = service->findComposition(outcome.childCompId).ptr.lock();
            const bool undoRestored = undoAvailable && undoChild &&
                undoChild->layerById(ids[0]) && undoChild->layerById(ids[1]) &&
                undoChild->layerById(ids[0])->layerNote() == noteA &&
                undoChild->layerById(ids[1])->layerNote() == noteB &&
                root->layerById(outcome.precompLayerId) && untouched->layerNote() == noteUntouched;
            const bool redoAvailable = undo->canRedo();
            if (redoAvailable) undo->redo();
            const bool redoRestored = redoAvailable && root->layerById(ids[0]) && root->layerById(ids[1]) &&
                root->layerById(ids[0])->layerNote() == noteA &&
                root->layerById(ids[1])->layerNote() == noteB &&
                untouched->layerNote() == noteUntouched;
            result["notesMoved"] = notesMoved;
            result["notesUnpacked"] = notesUnpacked;
            result["undoRestored"] = undoRestored;
            result["redoRestored"] = redoRestored;
            result["passed"] = notesMoved && notesUnpacked && undoRestored && redoRestored;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Layer notes changed across precompose/unprecompose/undo/redo");
            childId_ = outcome.childCompId;
            hasChild_ = !childId_.isNil();
            service->changeCurrentComposition(rootId);
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.layerFlagsAndBlendRoundTrip") {
            QJsonObject result;
            const auto root = service->currentComposition().lock();
            if (!root) {
                result["error"] = QStringLiteral("No root composition");
                return result;
            }
            const auto rootId = root->id();
            const auto ids = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B"),
                                                   QStringLiteral("Untouched")});
            if (ids.size() != 3) {
                result["error"] = QStringLiteral("Layer flag fixture is missing A, B, or Untouched");
                return result;
            }
            const auto layerA = root->layerById(ids[0]);
            const auto layerB = root->layerById(ids[1]);
            const auto untouched = root->layerById(ids[2]);
            if (!layerA || !layerB || !untouched) {
                result["error"] = QStringLiteral("Layer flag fixture layers could not be resolved");
                return result;
            }
            layerA->setVisible(false);
            layerA->setLocked(true);
            layerA->setSolo(true);
            layerA->setBlendMode(LAYER_BLEND_TYPE::BLEND_SCREEN);
            layerB->setVisible(true);
            layerB->setLocked(false);
            layerB->setSolo(false);
            layerB->setBlendMode(LAYER_BLEND_TYPE::BLEND_MULTIPLY);
            untouched->setVisible(true);
            untouched->setLocked(false);
            untouched->setSolo(false);
            untouched->setBlendMode(LAYER_BLEND_TYPE::BLEND_NORMAL);
            const auto matches = [](const ArtifactCompositionPtr& composition, LayerID id,
                                    bool visible, bool locked, bool solo,
                                    LAYER_BLEND_TYPE blendMode) {
                const auto layer = composition ? composition->layerById(id)
                                               : ArtifactAbstractLayerPtr{};
                return layer && layer->isVisible() == visible && layer->isLocked() == locked &&
                    layer->isSolo() == solo && layer->layerBlendType() == blendMode;
            };
            const bool precomposed = service->precomposeLayersWithUndo(
                {ids[0], ids[1]}, UniString(QStringLiteral("Layer Flags Fixture")), false, false,
                PrecomposeMode::MoveSelected);
            const auto outcome = service->lastPrecomposeOutcome();
            const auto child = precomposed
                ? service->findComposition(outcome.childCompId).ptr.lock() : ArtifactCompositionPtr{};
            const bool flagsMoved = matches(child, ids[0], false, true, true,
                                             LAYER_BLEND_TYPE::BLEND_SCREEN) &&
                matches(child, ids[1], true, false, false, LAYER_BLEND_TYPE::BLEND_MULTIPLY) &&
                matches(root, ids[2], true, false, false, LAYER_BLEND_TYPE::BLEND_NORMAL);
            const bool unpacked = precomposed &&
                service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const bool flagsUnpacked = unpacked &&
                matches(root, ids[0], false, true, true, LAYER_BLEND_TYPE::BLEND_SCREEN) &&
                matches(root, ids[1], true, false, false, LAYER_BLEND_TYPE::BLEND_MULTIPLY) &&
                matches(root, ids[2], true, false, false, LAYER_BLEND_TYPE::BLEND_NORMAL);
            const bool undoAvailable = undo->canUndo();
            if (undoAvailable) undo->undo();
            const auto undoChild = service->findComposition(outcome.childCompId).ptr.lock();
            const bool undoRestored = undoAvailable &&
                matches(undoChild, ids[0], false, true, true, LAYER_BLEND_TYPE::BLEND_SCREEN) &&
                matches(undoChild, ids[1], true, false, false, LAYER_BLEND_TYPE::BLEND_MULTIPLY) &&
                matches(root, ids[2], true, false, false, LAYER_BLEND_TYPE::BLEND_NORMAL) &&
                root->layerById(outcome.precompLayerId);
            const bool redoAvailable = undo->canRedo();
            if (redoAvailable) undo->redo();
            const bool redoRestored = redoAvailable &&
                matches(root, ids[0], false, true, true, LAYER_BLEND_TYPE::BLEND_SCREEN) &&
                matches(root, ids[1], true, false, false, LAYER_BLEND_TYPE::BLEND_MULTIPLY) &&
                matches(root, ids[2], true, false, false, LAYER_BLEND_TYPE::BLEND_NORMAL);
            result["flagsMoved"] = flagsMoved;
            result["flagsUnpacked"] = flagsUnpacked;
            result["undoRestored"] = undoRestored;
            result["redoRestored"] = redoRestored;
            result["passed"] = flagsMoved && flagsUnpacked && undoRestored && redoRestored;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Layer flags or blend modes changed across precompose/unprecompose/undo/redo");
            childId_ = outcome.childCompId;
            hasChild_ = !childId_.isNil();
            service->changeCurrentComposition(rootId);
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.sharedChildReferences") {
            QJsonObject result;
            auto root = service->currentComposition().lock();
            if (!root) {
                result["error"] = QStringLiteral("No root composition");
                return result;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2 || !service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Shared Child Fixture")), false, false,
                    PrecomposeMode::MoveSelected)) {
                result["error"] = QStringLiteral("Could not create shared-child fixture");
                return result;
            }
            const auto first = service->lastPrecomposeOutcome();
            const bool addedSecond = service->addCompositionLayerToCurrentCompositionWithUndo(
                first.childCompId);
            LayerID secondWrapperId;
            if (addedSecond) {
                for (const auto& layer : root->allLayer()) {
                    const auto compositionLayer = dynamicPointerCast<ArtifactCompositionLayer>(layer);
                    if (compositionLayer && compositionLayer->sourceCompositionId() == first.childCompId &&
                        compositionLayer->id() != first.precompLayerId) {
                        secondWrapperId = compositionLayer->id();
                        break;
                    }
                }
            }
            auto& manager = PreComposeManager::instance();
            const auto hasReference = [&](LayerID id) {
                for (const auto& reference : manager.getPrecompLayersForChild(first.childCompId)) {
                    if (reference.parentCompId == rootId && reference.precompLayerId == id) return true;
                }
                return false;
            };
            const bool twoReferencesCreated = addedSecond && !secondWrapperId.isNil() &&
                manager.getPrecompLayersForChild(first.childCompId).size() == 2 &&
                hasReference(first.precompLayerId) && hasReference(secondWrapperId);
            QTemporaryDir tempDir;
            const bool tempReady = twoReferencesCreated && tempDir.isValid();
            const auto filePath = tempReady
                ? tempDir.filePath(QStringLiteral("shared-child-references.artifact")) : QString{};
            const auto save = tempReady ? ArtifactProjectManager::getInstance().saveToFile(filePath)
                                        : ArtifactProjectExporterResult{};
            const bool firstMappingCleared = save.success && manager.unregisterPrecompLayer(
                rootId, first.precompLayerId, first.childCompId);
            const bool secondMappingCleared = firstMappingCleared && manager.unregisterPrecompLayer(
                rootId, secondWrapperId, first.childCompId);
            const bool mappingsCleared = secondMappingCleared &&
                manager.getPrecompLayersForChild(first.childCompId).isEmpty();
            const bool loaded = mappingsCleared && ArtifactProjectManager::getInstance().loadFromFile(filePath);
            if (loaded) service->changeCurrentComposition(rootId);
            root = loaded ? service->findComposition(rootId).ptr.lock() : ArtifactCompositionPtr{};
            const auto childAfterReload = loaded
                ? service->findComposition(first.childCompId).ptr.lock() : ArtifactCompositionPtr{};
            const auto restoredReferences = manager.getPrecompLayersForChild(first.childCompId);
            const bool sharedChildSaveReload = save.success && mappingsCleared && loaded && root &&
                childAfterReload && restoredReferences.size() == 2 && hasReference(first.precompLayerId) &&
                hasReference(secondWrapperId) && root->layerById(first.precompLayerId) &&
                root->layerById(secondWrapperId) && childAfterReload->layerById(sourceIds.front()) &&
                childAfterReload->layerById(sourceIds.back());
            const bool dissolved = sharedChildSaveReload &&
                service->unprecomposeLayerWithUndo(first.precompLayerId, true);
            const auto childAfterUnpack = service->findComposition(first.childCompId).ptr.lock();
            const bool childDataPreserved = childAfterUnpack &&
                childAfterUnpack->layerById(sourceIds.front()) &&
                childAfterUnpack->layerById(sourceIds.back());
            const bool remainingReferencePreserved = dissolved &&
                manager.getPrecompLayersForChild(first.childCompId).size() == 1 &&
                !manager.isPrecomposeLayer(first.precompLayerId) &&
                manager.isPrecomposeLayer(secondWrapperId) && hasReference(secondWrapperId) &&
                root->layerById(secondWrapperId) && childDataPreserved;
            const bool undoAvailable = undo->canUndo();
            if (undoAvailable) undo->undo();
            const bool undoRestoredBoth = undoAvailable &&
                manager.getPrecompLayersForChild(first.childCompId).size() == 2 &&
                hasReference(first.precompLayerId) && hasReference(secondWrapperId) &&
                root->layerById(first.precompLayerId) && root->layerById(secondWrapperId);
            const bool redoAvailable = undo->canRedo();
            if (redoAvailable) undo->redo();
            const auto childAfterRedo = service->findComposition(first.childCompId).ptr.lock();
            const bool redoChildDataPreserved = childAfterRedo &&
                childAfterRedo->layerById(sourceIds.front()) &&
                childAfterRedo->layerById(sourceIds.back());
            const bool redoPreservedRemaining = redoAvailable &&
                manager.getPrecompLayersForChild(first.childCompId).size() == 1 &&
                !manager.isPrecomposeLayer(first.precompLayerId) &&
                manager.isPrecomposeLayer(secondWrapperId) && hasReference(secondWrapperId) &&
                root->layerById(secondWrapperId) && redoChildDataPreserved;
            service->changeCurrentComposition(rootId);
            const auto compositionCountBeforeFinalDelete =
                ArtifactProjectManager::getInstance().compositionCount();
            const bool finalReferenceDeleted = redoPreservedRemaining &&
                service->unprecomposeLayerWithUndo(secondWrapperId, false);
            const bool finalChildRemoved = finalReferenceDeleted &&
                !service->findComposition(first.childCompId).ptr.lock() &&
                ArtifactProjectManager::getInstance().compositionCount() + 1 ==
                    compositionCountBeforeFinalDelete &&
                !root->layerById(secondWrapperId) &&
                manager.getPrecompLayersForChild(first.childCompId).isEmpty();
            const bool finalDeleteUndoAvailable = finalReferenceDeleted && undo->canUndo();
            if (finalDeleteUndoAvailable) undo->undo();
            const auto restoredFinalChild = service->findComposition(first.childCompId).ptr.lock();
            const bool finalDeleteUndoRestored = finalDeleteUndoAvailable && restoredFinalChild &&
                root->layerById(secondWrapperId) && manager.isPrecomposeLayer(secondWrapperId) &&
                manager.getPrecompLayersForChild(first.childCompId).size() == 1 &&
                hasReference(secondWrapperId) &&
                restoredFinalChild->layerById(sourceIds.front()) &&
                restoredFinalChild->layerById(sourceIds.back());
            const bool finalDeleteRedoAvailable = finalDeleteUndoRestored && undo->canRedo();
            if (finalDeleteRedoAvailable) undo->redo();
            const bool finalDeleteRedoRemoved = finalDeleteRedoAvailable &&
                !service->findComposition(first.childCompId).ptr.lock() &&
                !root->layerById(secondWrapperId) &&
                manager.getPrecompLayersForChild(first.childCompId).isEmpty() &&
                ArtifactProjectManager::getInstance().compositionCount() + 1 ==
                    compositionCountBeforeFinalDelete;
            result["twoReferencesCreated"] = twoReferencesCreated;
            result["saveSucceeded"] = save.success;
            result["mappingsClearedBeforeReload"] = mappingsCleared;
            result["loaded"] = loaded;
            result["sharedChildSaveReload"] = sharedChildSaveReload;
            result["childDataPreserved"] = childDataPreserved;
            result["remainingReferencePreserved"] = remainingReferencePreserved;
            result["undoRestoredBoth"] = undoRestoredBoth;
            result["redoChildDataPreserved"] = redoChildDataPreserved;
            result["redoPreservedRemaining"] = redoPreservedRemaining;
            result["finalChildRemoved"] = finalChildRemoved;
            result["finalDeleteUndoRestored"] = finalDeleteUndoRestored;
            result["finalDeleteRedoRemoved"] = finalDeleteRedoRemoved;
            result["passed"] = twoReferencesCreated && sharedChildSaveReload && remainingReferencePreserved &&
                undoRestoredBoth && redoPreservedRemaining && finalChildRemoved &&
                finalDeleteUndoRestored && finalDeleteRedoRemoved;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Unprecomposing one instance changed shared child data or another reference");
            childId_ = first.childCompId;
            hasChild_ = true;
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.sharedInstanceOverridesRoundTrip") {
            QJsonObject result;
            const auto root = service->currentComposition().lock();
            if (!root) {
                result["error"] = QStringLiteral("No root composition");
                return result;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2 || !service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Shared Override Child")), false, false,
                    PrecomposeMode::MoveSelected)) {
                result["error"] = QStringLiteral("Could not create shared-override fixture");
                return result;
            }
            const auto outcome = service->lastPrecomposeOutcome();
            service->changeCurrentComposition(rootId);
            const bool secondAdded = service->addCompositionLayerToCurrentCompositionWithUndo(
                outcome.childCompId);
            LayerID secondWrapperId;
            if (secondAdded) for (const auto& layer : root->allLayer()) {
                const auto candidate = dynamicPointerCast<ArtifactCompositionLayer>(layer);
                if (candidate && candidate->sourceCompositionId() == outcome.childCompId &&
                    candidate->id() != outcome.precompLayerId) {
                    secondWrapperId = candidate->id();
                    break;
                }
            }
            const auto firstWrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                root->layerById(outcome.precompLayerId));
            const auto secondWrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                root->layerById(secondWrapperId));
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            const auto childLayer = child ? child->layerById(sourceIds.front()) : ArtifactAbstractLayerPtr{};
            if (!secondAdded || !firstWrapper || !secondWrapper || !childLayer) {
                result["error"] = QStringLiteral("Shared-override wrapper or child layer is unavailable");
                return result;
            }
            ArtifactCore::ExposedPropertyRegistry registry;
            ArtifactCore::ExposedPropertyBinding binding;
            binding.id = QStringLiteral("test.shared.opacity");
            binding.label = QStringLiteral("Shared Instance Opacity");
            binding.targetLayerId = sourceIds.front().toString();
            binding.internalPath = QStringLiteral("layer.opacity");
            binding.defaultValue = childLayer->opacity();
            const bool bindingAdded = registry.add(binding);
            firstWrapper->setExposedProperties(registry);
            secondWrapper->setExposedProperties(registry);
            const float firstValue = 0.81f;
            const float secondValue = 0.23f;
            const bool firstOverrideSet = bindingAdded && firstWrapper->setExposedPropertyOverride(
                binding.id, QVariant::fromValue(static_cast<double>(firstValue)));
            const bool secondOverrideSet = bindingAdded && secondWrapper->setExposedPropertyOverride(
                binding.id, QVariant::fromValue(static_cast<double>(secondValue)));
            const float childBaseOpacity = childLayer->opacity();
            const bool overridesIsolated = firstOverrideSet && secondOverrideSet &&
                firstWrapper->hasExposedPropertyOverride(binding.id) &&
                secondWrapper->hasExposedPropertyOverride(binding.id) &&
                std::abs(firstWrapper->effectiveExposedPropertyValue(binding.id).toFloat() - firstValue) < 1.0e-5f &&
                std::abs(secondWrapper->effectiveExposedPropertyValue(binding.id).toFloat() - secondValue) < 1.0e-5f &&
                std::abs(childBaseOpacity - 0.42f) < 1.0e-5f;
            QTemporaryDir tempDir;
            if (!tempDir.isValid()) {
                result["error"] = QStringLiteral("Temporary directory unavailable for shared-override round trip");
                return result;
            }
            const auto filePath = tempDir.filePath(QStringLiteral("shared-instance-overrides.artifact"));
            const auto save = ArtifactProjectManager::getInstance().saveToFile(filePath);
            auto& manager = PreComposeManager::instance();
            const bool firstMappingCleared = save.success && manager.unregisterPrecompLayer(
                rootId, outcome.precompLayerId, outcome.childCompId);
            const bool secondMappingCleared = firstMappingCleared && manager.unregisterPrecompLayer(
                rootId, secondWrapperId, outcome.childCompId);
            const bool mappingsCleared = secondMappingCleared &&
                manager.getPrecompLayersForChild(outcome.childCompId).isEmpty();
            const bool loaded = mappingsCleared &&
                ArtifactProjectManager::getInstance().loadFromFile(filePath);
            if (loaded) service->changeCurrentComposition(rootId);
            const auto reloadedRoot = loaded ? service->findComposition(rootId).ptr.lock()
                                             : decltype(root){};
            const auto reloadedChild = loaded ? service->findComposition(outcome.childCompId).ptr.lock()
                                              : decltype(root){};
            const auto reloadedFirstWrapper = reloadedRoot
                ? dynamicPointerCast<ArtifactCompositionLayer>(reloadedRoot->layerById(outcome.precompLayerId))
                : SharedPtr<ArtifactCompositionLayer>{};
            const auto reloadedSecondWrapper = reloadedRoot
                ? dynamicPointerCast<ArtifactCompositionLayer>(reloadedRoot->layerById(secondWrapperId))
                : SharedPtr<ArtifactCompositionLayer>{};
            const auto reloadedChildLayer = reloadedChild
                ? reloadedChild->layerById(sourceIds.front()) : ArtifactAbstractLayerPtr{};
            const auto restoredReferences = manager.getPrecompLayersForChild(outcome.childCompId);
            const auto hasRestoredReference = [&](const LayerID& wrapperId) {
                return std::any_of(restoredReferences.begin(), restoredReferences.end(), [&](const auto& reference) {
                    return reference.parentCompId == rootId && reference.precompLayerId == wrapperId;
                });
            };
            const bool saveReloadPreserved = save.success && mappingsCleared && loaded &&
                reloadedRoot && reloadedChild && reloadedChildLayer && restoredReferences.size() == 2 &&
                hasRestoredReference(outcome.precompLayerId) && hasRestoredReference(secondWrapperId) &&
                reloadedFirstWrapper && reloadedSecondWrapper &&
                reloadedFirstWrapper->hasExposedPropertyOverride(binding.id) &&
                reloadedSecondWrapper->hasExposedPropertyOverride(binding.id) &&
                std::abs(reloadedFirstWrapper->effectiveExposedPropertyValue(binding.id).toFloat() - firstValue) < 1.0e-5f &&
                std::abs(reloadedSecondWrapper->effectiveExposedPropertyValue(binding.id).toFloat() - secondValue) < 1.0e-5f &&
                std::abs(reloadedChildLayer->opacity() - childBaseOpacity) < 1.0e-5f;
            result["bindingAdded"] = bindingAdded;
            result["overridesIsolated"] = overridesIsolated;
            result["saveSucceeded"] = save.success;
            result["mappingsClearedBeforeReload"] = mappingsCleared;
            result["loaded"] = loaded;
            result["saveReloadPreserved"] = saveReloadPreserved;
            result["passed"] = overridesIsolated && saveReloadPreserved;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Shared wrapper overrides or their independent values changed across save/reload");
            childId_ = outcome.childCompId;
            hasChild_ = true;
            service->changeCurrentComposition(rootId);
            refresh();
            result["state"] = state();
            return result;
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
            const auto sourceBId = idsB.front();
            const auto externalParentId = idsParent.front();
            const auto sourceLayer = parent->layerById(sourceId);
            const auto sourceLayerB = parent->layerById(sourceBId);
            if (!sourceLayer || !sourceLayerB) {
                parentResult["error"] = QStringLiteral("External-parent source layer is missing");
                return parentResult;
            }
            sourceLayer->setParentById(externalParentId);
            sourceLayerB->setParentById(externalParentId);
            const bool authoredParent = sourceLayer->parentLayerId() == externalParentId &&
                sourceLayerB->parentLayerId() == externalParentId;
            const QVector<LayerID> selected{sourceId, sourceBId};
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
            const auto movedSourceB = child ? child->layerById(sourceBId) : ArtifactAbstractLayerPtr{};
            const bool externalParentStored = wrapper &&
                wrapper->restoreExternalParent(sourceId) == externalParentId &&
                wrapper->restoreExternalParent(sourceBId) == externalParentId;
            const bool childParentCleared = movedSource && movedSourceB &&
                movedSource->parentLayerId().isNil() && movedSourceB->parentLayerId().isNil();
            const bool unpacked = service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const auto restoredSource = service->currentComposition().lock()
                ? service->currentComposition().lock()->layerById(sourceId)
                : ArtifactAbstractLayerPtr{};
            const auto restoredSourceB = service->currentComposition().lock()
                ? service->currentComposition().lock()->layerById(sourceBId)
                : ArtifactAbstractLayerPtr{};
            const bool unpackRestoredParent = unpacked && restoredSource &&
                restoredSourceB && restoredSource->parentLayerId() == externalParentId &&
                restoredSourceB->parentLayerId() == externalParentId;
            const bool undoAvailable = undo->canUndo();
            if (undoAvailable) undo->undo();
            const auto undoWrapper = service->currentComposition().lock()
                ? dynamicPointerCast<ArtifactCompositionLayer>(
                    service->currentComposition().lock()->layerById(outcome.precompLayerId))
                : SharedPtr<ArtifactCompositionLayer>{};
            const bool undoRestoredMetadata = undoAvailable && undoWrapper &&
                undoWrapper->restoreExternalParent(sourceId) == externalParentId &&
                undoWrapper->restoreExternalParent(sourceBId) == externalParentId;
            const bool redoAvailable = undo->canRedo();
            if (redoAvailable) undo->redo();
            const auto redoSource = service->currentComposition().lock()
                ? service->currentComposition().lock()->layerById(sourceId)
                : ArtifactAbstractLayerPtr{};
            const auto redoSourceB = service->currentComposition().lock()
                ? service->currentComposition().lock()->layerById(sourceBId)
                : ArtifactAbstractLayerPtr{};
            const bool redoRestoredParent = redoAvailable && redoSource &&
                redoSourceB && redoSource->parentLayerId() == externalParentId &&
                redoSourceB->parentLayerId() == externalParentId;
            parentResult["externalParentStoredOnWrapper"] = externalParentStored;
            parentResult["externalParentClearedInChild"] = childParentCleared;
            parentResult["bothExternalParentLinksRestored"] = unpackRestoredParent;
            parentResult["unpackRestoredExternalParent"] = unpackRestoredParent;
            parentResult["undoRestoredMetadata"] = undoRestoredMetadata;
            parentResult["undoRestoredBothMetadata"] = undoRestoredMetadata;
            parentResult["redoRestoredBothExternalParents"] = redoRestoredParent;
            parentResult["redoRestoredExternalParent"] = redoRestoredParent;
            parentResult["passed"] = externalParentStored && childParentCleared &&
                unpackRestoredParent && undoRestoredMetadata && redoRestoredParent;
            if (!parentResult["passed"].toBool())
                parentResult["error"] = QStringLiteral("External parent relationship changed across precompose/undo/redo");
            refresh();
            parentResult["state"] = state();
            return parentResult;
        } else if (name == "test.missingExternalParentUnwrapAtomic") {
            QJsonObject result;
            auto root = service->currentComposition().lock();
            if (!root) {
                result["error"] = QStringLiteral("No root composition");
                return result;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            const auto parentIds = namedLayerIds(root, {QStringLiteral("Untouched")});
            if (sourceIds.size() != 2 || parentIds.size() != 1) {
                result["error"] = QStringLiteral("Missing-external-parent fixture layers are unavailable");
                return result;
            }
            const auto sourceId = sourceIds.front();
            const auto externalParentId = parentIds.front();
            auto source = root->layerById(sourceId);
            if (!source) {
                result["error"] = QStringLiteral("Missing-external-parent source layer is unavailable");
                return result;
            }
            source->setParentById(externalParentId);
            const bool parentAuthored = source->parentLayerId() == externalParentId;
            const bool precomposed = parentAuthored && service->precomposeLayersWithUndo(
                sourceIds, UniString(QStringLiteral("Missing External Parent")), false, false,
                PrecomposeMode::MoveSelected);
            if (!precomposed) {
                result["error"] = QStringLiteral("Could not create missing-external-parent fixture");
                return result;
            }
            const auto outcome = service->lastPrecomposeOutcome();
            auto wrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                root->layerById(outcome.precompLayerId));
            auto child = service->findComposition(outcome.childCompId).ptr.lock();
            auto childSource = child ? child->layerById(sourceId) : ArtifactAbstractLayerPtr{};
            const bool metadataStored = wrapper &&
                wrapper->restoreExternalParent(sourceId) == externalParentId && childSource &&
                childSource->parentLayerId().isNil();
            const bool parentDeleted = metadataStored &&
                service->removeLayerFromComposition(rootId, externalParentId) &&
                !root->layerById(externalParentId);
            QTemporaryDir tempDir;
            const auto filePath = tempDir.filePath(QStringLiteral("missing-external-parent.artifact"));
            const auto save = tempDir.isValid()
                ? ArtifactProjectManager::getInstance().saveToFile(filePath)
                : ArtifactProjectExporterResult{};
            auto& precompManager = PreComposeManager::instance();
            const bool mappingCleared = save.success && precompManager.unregisterPrecompLayer(
                rootId, outcome.precompLayerId, outcome.childCompId) &&
                !precompManager.isPrecomposeLayer(outcome.precompLayerId);
            const bool loaded = mappingCleared && ArtifactProjectManager::getInstance().loadFromFile(filePath);
            if (loaded) {
                service->changeCurrentComposition(rootId);
                root = service->findComposition(rootId).ptr.lock();
                child = service->findComposition(outcome.childCompId).ptr.lock();
                if (!root || !child) {
                    result["saveSucceeded"] = save.success;
                    result["mappingClearedBeforeReload"] = mappingCleared;
                    result["loaded"] = loaded;
                    result["error"] = QStringLiteral("Missing-parent composition fixture was not restored by reload");
                    return result;
                }
                wrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                    root->layerById(outcome.precompLayerId));
                source = child->layerById(sourceId);
                childSource = source;
            }
            const bool metadataRestoredAfterReload = loaded && wrapper && source &&
                wrapper->restoreExternalParent(sourceId) == externalParentId &&
                source->parentLayerId().isNil() && !root->layerById(externalParentId) &&
                precompManager.isPrecomposeLayer(outcome.precompLayerId);
            const bool unpacked = metadataRestoredAfterReload &&
                service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const auto unpackedSource = root->layerById(sourceId);
            const bool unpackKeptParentCleared = unpacked && unpackedSource == source &&
                unpackedSource->parentLayerId().isNil() && !root->layerById(externalParentId) &&
                !precompManager.isPrecomposeLayer(outcome.precompLayerId);
            const bool undoAvailable = unpacked && undo->canUndo();
            if (undoAvailable) undo->undo();
            const auto undoWrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                root->layerById(outcome.precompLayerId));
            const bool undoRestoredMetadata = undoAvailable && undoWrapper &&
                undoWrapper->restoreExternalParent(sourceId) == externalParentId &&
                precompManager.isPrecomposeLayer(outcome.precompLayerId) &&
                !root->layerById(externalParentId);
            const bool redoAvailable = undoRestoredMetadata && undo->canRedo();
            if (redoAvailable) undo->redo();
            const auto redoSource = root->layerById(sourceId);
            const bool redoKeptParentCleared = redoAvailable && redoSource == source &&
                redoSource->parentLayerId().isNil() && !root->layerById(externalParentId) &&
                !precompManager.isPrecomposeLayer(outcome.precompLayerId);
            result["parentAuthored"] = parentAuthored;
            result["metadataStored"] = metadataStored;
            result["parentDeleted"] = parentDeleted;
            result["saveSucceeded"] = save.success;
            result["mappingClearedBeforeReload"] = mappingCleared;
            result["loaded"] = loaded;
            result["metadataRestoredAfterReload"] = metadataRestoredAfterReload;
            result["unpackKeptParentCleared"] = unpackKeptParentCleared;
            result["undoRestoredMetadata"] = undoRestoredMetadata;
            result["redoKeptParentCleared"] = redoKeptParentCleared;
            result["passed"] = parentAuthored && metadataStored && parentDeleted && save.success &&
                mappingCleared && loaded && metadataRestoredAfterReload &&
                unpackKeptParentCleared && undoRestoredMetadata && redoKeptParentCleared;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Unprecompose restored a deleted external parent or lost its undo metadata");
            childId_ = outcome.childCompId;
            hasChild_ = true;
            service->changeCurrentComposition(rootId);
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.selectionTransitions") {
            QJsonObject selectionResult;
            const auto root = service->currentComposition().lock();
            auto* selection = ArtifactLayerSelectionManager::instance();
            if (!root || !selection) {
                selectionResult["error"] = QStringLiteral("Composition or layer-selection service is unavailable");
                return selectionResult;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2 || !service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Selection Transition Fixture")), false, false,
                    PrecomposeMode::MoveSelected)) {
                selectionResult["error"] = QStringLiteral("Could not create selection-transition fixture");
                return selectionResult;
            }
            const auto outcome = service->lastPrecomposeOutcome();
            const auto matchesSelection = [&](const LayerID& expectedLayerId) {
                const auto active = selection->activeComposition();
                const auto current = selection->currentLayer();
                const auto selected = selection->selectedLayersInOrder();
                return active && active->id() == rootId && current &&
                    current->id() == expectedLayerId && selected.size() == 1 &&
                    selected.front() && selected.front()->id() == expectedLayerId;
            };
            const bool precomposeSelectedWrapper = matchesSelection(outcome.precompLayerId) &&
                root->layerById(outcome.precompLayerId);
            const bool unpacked = service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const bool unpackSelectedFirstSource = unpacked && matchesSelection(sourceIds.front()) &&
                root->layerById(sourceIds.front());
            const bool undoAvailable = undo->canUndo();
            if (undoAvailable) undo->undo();
            const bool undoSelectedWrapper = undoAvailable && matchesSelection(outcome.precompLayerId) &&
                root->layerById(outcome.precompLayerId) &&
                PreComposeManager::instance().isPrecomposeLayer(outcome.precompLayerId);
            const bool redoAvailable = undo->canRedo();
            if (redoAvailable) undo->redo();
            const bool redoSelectedFirstSource = redoAvailable && matchesSelection(sourceIds.front()) &&
                root->layerById(sourceIds.front()) &&
                !PreComposeManager::instance().isPrecomposeLayer(outcome.precompLayerId);
            service->changeCurrentComposition(rootId);
            childId_ = outcome.childCompId;
            hasChild_ = true;
            selectionResult["precomposeSelectedWrapper"] = precomposeSelectedWrapper;
            selectionResult["unpackSelectedFirstSource"] = unpackSelectedFirstSource;
            selectionResult["undoSelectedWrapper"] = undoSelectedWrapper;
            selectionResult["redoSelectedFirstSource"] = redoSelectedFirstSource;
            selectionResult["passed"] = precomposeSelectedWrapper && unpackSelectedFirstSource &&
                undoSelectedWrapper && redoSelectedFirstSource;
            if (!selectionResult["passed"].toBool())
                selectionResult["error"] = QStringLiteral("Layer selection or active composition changed unexpectedly across precompose/unprecompose/undo/redo");
            refresh();
            selectionResult["state"] = state();
            return selectionResult;
        } else if (name == "test.childEditsSurviveUnwrap") {
            QJsonObject editResult;
            const auto root = service->currentComposition().lock();
            if (!root) {
                editResult["error"] = QStringLiteral("No root composition");
                return editResult;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2 || !service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Child Edit Fixture")), false, false,
                    PrecomposeMode::MoveSelected)) {
                editResult["error"] = QStringLiteral("Could not create child-edit fixture");
                return editResult;
            }
            const auto outcome = service->lastPrecomposeOutcome();
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            auto layerA = child ? child->layerById(sourceIds[0]) : ArtifactAbstractLayerPtr{};
            auto layerB = child ? child->layerById(sourceIds[1]) : ArtifactAbstractLayerPtr{};
            if (!child || !layerA || !layerB) {
                editResult["error"] = QStringLiteral("Could not resolve child layers for editing");
                return editResult;
            }
            constexpr float expectedOpacityA = 0.28f;
            constexpr float expectedOpacityB = 0.73f;
            layerA->setPosition3D({135.0f, -42.0f, 7.5f});
            layerA->setRotation3D(QVector3D(17.0f, -23.0f, 38.0f));
            layerA->transform3D().setCurrentScale(1.5f, 0.75f);
            layerA->transform3D().setCurrentAnchor(11.0f, -6.0f, 2.0f);
            layerA->setOpacity(expectedOpacityA);
            layerB->setPosition3D({-18.0f, 64.0f, 2.0f});
            layerB->setRotation3D(QVector3D(-31.0f, 12.0f, 91.0f));
            layerB->transform3D().setCurrentScale(0.625f, 1.875f);
            layerB->transform3D().setCurrentAnchor(-4.0f, 9.0f, -3.0f);
            layerB->setOpacity(expectedOpacityB);
            const auto matchesEditedValues = [&](const ArtifactAbstractLayerPtr& a,
                                                 const ArtifactAbstractLayerPtr& b) {
                if (!a || !b) return false;
                const auto positionA = a->position3D();
                const auto positionB = b->position3D();
                const auto rotationA = a->rotation3D();
                const auto rotationB = b->rotation3D();
                const auto& transformA = a->transform3D();
                const auto& transformB = b->transform3D();
                return std::abs(positionA.x - 135.0f) < 1.0e-5f &&
                    std::abs(positionA.y - (-42.0f)) < 1.0e-5f &&
                    std::abs(positionA.z - 7.5f) < 1.0e-5f &&
                    std::abs(positionB.x - (-18.0f)) < 1.0e-5f &&
                    std::abs(positionB.y - 64.0f) < 1.0e-5f &&
                    std::abs(positionB.z - 2.0f) < 1.0e-5f &&
                    std::abs(rotationA.x() - 17.0f) < 1.0e-5f &&
                    std::abs(rotationA.y() - (-23.0f)) < 1.0e-5f &&
                    std::abs(rotationA.z() - 38.0f) < 1.0e-5f &&
                    std::abs(rotationB.x() - (-31.0f)) < 1.0e-5f &&
                    std::abs(rotationB.y() - 12.0f) < 1.0e-5f &&
                    std::abs(rotationB.z() - 91.0f) < 1.0e-5f &&
                    std::abs(transformA.scaleX() - 1.5f) < 1.0e-5f &&
                    std::abs(transformA.scaleY() - 0.75f) < 1.0e-5f &&
                    std::abs(transformA.anchorX() - 11.0f) < 1.0e-5f &&
                    std::abs(transformA.anchorY() - (-6.0f)) < 1.0e-5f &&
                    std::abs(transformA.anchorZ() - 2.0f) < 1.0e-5f &&
                    std::abs(transformB.scaleX() - 0.625f) < 1.0e-5f &&
                    std::abs(transformB.scaleY() - 1.875f) < 1.0e-5f &&
                    std::abs(transformB.anchorX() - (-4.0f)) < 1.0e-5f &&
                    std::abs(transformB.anchorY() - 9.0f) < 1.0e-5f &&
                    std::abs(transformB.anchorZ() - (-3.0f)) < 1.0e-5f &&
                    std::abs(a->opacity() - expectedOpacityA) < 1.0e-5f &&
                    std::abs(b->opacity() - expectedOpacityB) < 1.0e-5f;
            };
            const bool editsAuthored = matchesEditedValues(layerA, layerB);
            const bool unpacked = service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const auto unpackedA = root->layerById(sourceIds[0]);
            const auto unpackedB = root->layerById(sourceIds[1]);
            const bool unpackedValuesPreserved = unpacked && unpackedA == layerA && unpackedB == layerB &&
                matchesEditedValues(unpackedA, unpackedB);
            const bool undoAvailable = undo->canUndo();
            if (undoAvailable) undo->undo();
            const auto undoChild = service->findComposition(outcome.childCompId).ptr.lock();
            const auto undoA = undoChild ? undoChild->layerById(sourceIds[0]) : ArtifactAbstractLayerPtr{};
            const auto undoB = undoChild ? undoChild->layerById(sourceIds[1]) : ArtifactAbstractLayerPtr{};
            const bool undoValuesPreserved = undoAvailable && undoChild && undoA == layerA && undoB == layerB &&
                root->layerById(outcome.precompLayerId) &&
                PreComposeManager::instance().isPrecomposeLayer(outcome.precompLayerId) &&
                matchesEditedValues(undoA, undoB);
            const bool redoAvailable = undo->canRedo();
            if (redoAvailable) undo->redo();
            const auto redoA = root->layerById(sourceIds[0]);
            const auto redoB = root->layerById(sourceIds[1]);
            const bool redoValuesPreserved = redoAvailable && redoA == layerA && redoB == layerB &&
                !PreComposeManager::instance().isPrecomposeLayer(outcome.precompLayerId) &&
                matchesEditedValues(redoA, redoB);
            service->changeCurrentComposition(rootId);
            childId_ = outcome.childCompId;
            hasChild_ = true;
            editResult["editsAuthored"] = editsAuthored;
            editResult["unpackedValuesPreserved"] = unpackedValuesPreserved;
            editResult["undoValuesPreserved"] = undoValuesPreserved;
            editResult["redoValuesPreserved"] = redoValuesPreserved;
            editResult["passed"] = editsAuthored && unpackedValuesPreserved &&
                undoValuesPreserved && redoValuesPreserved;
            if (!editResult["passed"].toBool())
                editResult["error"] = QStringLiteral("Child-edited transform or opacity changed across unprecompose/undo/redo");
            refresh();
            editResult["state"] = state();
            return editResult;
        } else if (name == "test.externalParentSaveReload") {
            QJsonObject roundTrip;
            auto& projects = ArtifactProjectManager::getInstance();
            auto& precompManager = PreComposeManager::instance();
            const auto parent = service->currentComposition().lock();
            if (!parent) {
                roundTrip["error"] = QStringLiteral("No parent composition");
                return roundTrip;
            }
            service->addLayerToCurrentComposition(
                ArtifactNullLayerInitParams(QStringLiteral("Untouched Two")), false);
            const auto parentId = parent->id();
            const auto sourceIds = namedLayerIds(parent, {QStringLiteral("A")});
            const auto selectedB = namedLayerIds(parent, {QStringLiteral("B")});
            const auto externalParentIds = namedLayerIds(parent, {QStringLiteral("Untouched")});
            const auto secondExternalParentIds = namedLayerIds(parent, {QStringLiteral("Untouched Two")});
            if (sourceIds.size() != 1 || selectedB.size() != 1 || externalParentIds.size() != 1 ||
                secondExternalParentIds.size() != 1) {
                roundTrip["error"] = QStringLiteral("External-parent save fixture layers are missing");
                return roundTrip;
            }
            const auto sourceId = sourceIds.front();
            const auto externalParentId = externalParentIds.front();
            const auto sourceBId = selectedB.front();
            const auto secondExternalParentId = secondExternalParentIds.front();
            const auto source = parent->layerById(sourceId);
            const auto sourceB = parent->layerById(sourceBId);
            if (!source || !sourceB) {
                roundTrip["error"] = QStringLiteral("External-parent save fixture source is missing");
                return roundTrip;
            }
            source->setParentById(externalParentId);
            sourceB->setParentById(secondExternalParentId);
            if (source->parentLayerId() != externalParentId ||
                sourceB->parentLayerId() != secondExternalParentId || !service->precomposeLayersWithUndo(
                    {sourceId, sourceBId}, UniString(QStringLiteral("External Parent Round Trip")),
                    false, false, PrecomposeMode::MoveSelected)) {
                roundTrip["error"] = QStringLiteral("Could not precompose the external-parent save fixture");
                return roundTrip;
            }
            const auto outcome = service->lastPrecomposeOutcome();
            const auto wrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                parent->layerById(outcome.precompLayerId));
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            const auto movedSource = child ? child->layerById(sourceId) : ArtifactAbstractLayerPtr{};
            const auto movedSourceB = child ? child->layerById(sourceBId) : ArtifactAbstractLayerPtr{};
            if (!wrapper || !child || !movedSource ||
                wrapper->restoreExternalParent(sourceId) != externalParentId ||
                wrapper->restoreExternalParent(sourceBId) != secondExternalParentId ||
                !movedSource->parentLayerId().isNil() || !movedSourceB ||
                !movedSourceB->parentLayerId().isNil()) {
                roundTrip["error"] = QStringLiteral("External-parent metadata was not authored in the fixture");
                return roundTrip;
            }
            QTemporaryDir tempDir;
            if (!tempDir.isValid()) {
                roundTrip["error"] = QStringLiteral("Temporary directory unavailable");
                return roundTrip;
            }
            const auto filePath = tempDir.filePath(QStringLiteral("external-parent-roundtrip.artifact"));
            const auto save = projects.saveToFile(filePath);
            roundTrip["saved"] = save.success;
            if (!save.success) {
                roundTrip["error"] = save.errorMessage;
                return roundTrip;
            }
            const bool mappingCleared = precompManager.unregisterPrecompLayer(
                parentId, outcome.precompLayerId, outcome.childCompId) &&
                !precompManager.isPrecomposeLayer(outcome.precompLayerId) &&
                precompManager.getParentComposition(outcome.childCompId).isNil();
            roundTrip["mappingClearedBeforeReload"] = mappingCleared;
            if (!mappingCleared) {
                roundTrip["error"] = QStringLiteral("Could not isolate the external-parent mapping before reload");
                return roundTrip;
            }
            const bool loaded = projects.loadFromFile(filePath);
            roundTrip["loaded"] = loaded;
            if (loaded) service->changeCurrentComposition(parentId);
            const auto reloadedParent = service->findComposition(parentId).ptr.lock();
            const auto reloadedChild = service->findComposition(outcome.childCompId).ptr.lock();
            const auto reloadedWrapper = reloadedParent
                ? dynamicPointerCast<ArtifactCompositionLayer>(
                      reloadedParent->layerById(outcome.precompLayerId))
                : SharedPtr<ArtifactCompositionLayer>{};
            const auto reloadedSource = reloadedChild
                ? reloadedChild->layerById(sourceId) : ArtifactAbstractLayerPtr{};
            const auto reloadedSourceB = reloadedChild
                ? reloadedChild->layerById(sourceBId) : ArtifactAbstractLayerPtr{};
            const bool metadataPersisted = reloadedWrapper &&
                reloadedWrapper->restoreExternalParent(sourceId) == externalParentId &&
                reloadedWrapper->restoreExternalParent(sourceBId) == secondExternalParentId;
            const bool sourceParentRemainsCleared = reloadedSource && reloadedSourceB &&
                reloadedSource->parentLayerId().isNil() && reloadedSourceB->parentLayerId().isNil();
            bool runtimeMappingRestored = false;
            for (const auto& ref : precompManager.getPrecompLayersForChild(outcome.childCompId)) {
                runtimeMappingRestored = runtimeMappingRestored ||
                    (ref.parentCompId == parentId && ref.precompLayerId == outcome.precompLayerId);
            }
            const bool unpacked = loaded && reloadedWrapper &&
                service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const auto restoredSource = reloadedParent ? reloadedParent->layerById(sourceId)
                                                       : ArtifactAbstractLayerPtr{};
            const auto restoredSourceB = reloadedParent ? reloadedParent->layerById(sourceBId)
                                                        : ArtifactAbstractLayerPtr{};
            const bool externalParentRestored = unpacked && restoredSource &&
                restoredSource->parentLayerId() == externalParentId &&
                reloadedParent->layerById(externalParentId) && restoredSourceB &&
                restoredSourceB->parentLayerId() == secondExternalParentId &&
                reloadedParent->layerById(secondExternalParentId);
            service->changeCurrentComposition(parentId);
            childId_ = outcome.childCompId;
            hasChild_ = true;
            roundTrip["metadataPersisted"] = metadataPersisted;
            roundTrip["sourceParentRemainsCleared"] = sourceParentRemainsCleared;
            roundTrip["runtimeMappingRestored"] = runtimeMappingRestored;
            roundTrip["externalParentRestoredAfterUnpack"] = externalParentRestored;
            roundTrip["passed"] = save.success && loaded && mappingCleared && metadataPersisted &&
                sourceParentRemainsCleared && runtimeMappingRestored && externalParentRestored;
            if (!roundTrip["passed"].toBool())
                roundTrip["error"] = QStringLiteral("External-parent metadata did not survive save/reload and unprecompose");
            refresh();
            roundTrip["state"] = state();
            return roundTrip;
        } else if (name == "test.rationalFrameRatePreserved") {
            QJsonObject rateResult;
            auto parent = service->currentComposition().lock();
            if (!parent) {
                rateResult["error"] = QStringLiteral("No current composition");
                return rateResult;
            }
            const auto rootId = parent->id();
            const auto exactRate = FrameRate::fromRational(30000, 1001);
            parent->setFrameRate(exactRate);
            const auto hasExactRate = [](const ArtifactCore::FrameRate& rate) {
                return rate.hasExactRational() && rate.numerator() == 30000 &&
                    rate.denominator() == 1001;
            };
            const auto sourceIds = namedLayerIds(parent, {QStringLiteral("A"), QStringLiteral("B")});
            const bool selectedPrecomposed = sourceIds.size() == 2 && hasExactRate(parent->frameRate()) &&
                service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Rational Selected Rate")), false, false,
                    PrecomposeMode::MoveSelected);
            const auto selectedOutcome = service->lastPrecomposeOutcome();
            const auto selectedChild = service->findComposition(selectedOutcome.childCompId).ptr.lock();
            const auto selectedRatePreserved = selectedPrecomposed && selectedChild &&
                hasExactRate(selectedChild->frameRate());
            bool roundTripRatePreserved = false;
            bool selectedUnpacked = false;
            QTemporaryDir tempDir;
            if (selectedRatePreserved && tempDir.isValid()) {
                auto& projects = ArtifactProjectManager::getInstance();
                const auto filePath = tempDir.filePath(QStringLiteral("rational-frame-rate.artifact"));
                const auto save = projects.saveToFile(filePath);
                const bool mappingCleared = save.success &&
                    PreComposeManager::instance().unregisterPrecompLayer(
                        rootId, selectedOutcome.precompLayerId, selectedOutcome.childCompId) &&
                    !PreComposeManager::instance().isPrecomposeLayer(selectedOutcome.precompLayerId) &&
                    PreComposeManager::instance().getParentComposition(selectedOutcome.childCompId).isNil();
                const bool loaded = mappingCleared && projects.loadFromFile(filePath);
                if (loaded) {
                    service->changeCurrentComposition(rootId);
                    parent = service->findComposition(rootId).ptr.lock();
                }
                const auto reloadedChild = loaded
                    ? service->findComposition(selectedOutcome.childCompId).ptr.lock()
                    : ArtifactCompositionPtr{};
                bool mappingRestored = false;
                for (const auto& reference : PreComposeManager::instance().getPrecompLayersForChild(
                         selectedOutcome.childCompId)) {
                    mappingRestored = mappingRestored ||
                        (reference.parentCompId == rootId &&
                         reference.precompLayerId == selectedOutcome.precompLayerId);
                }
                const auto reloadedWrapper = parent
                    ? dynamicPointerCast<ArtifactCompositionLayer>(
                          parent->layerById(selectedOutcome.precompLayerId))
                    : SharedPtr<ArtifactCompositionLayer>{};
                roundTripRatePreserved = save.success && mappingCleared && loaded && parent &&
                    hasExactRate(parent->frameRate()) && reloadedChild &&
                    hasExactRate(reloadedChild->frameRate()) && mappingRestored && reloadedWrapper;
                selectedUnpacked = roundTripRatePreserved &&
                    service->unprecomposeLayerWithUndo(selectedOutcome.precompLayerId, true);
            }
            const bool parentRateAfterSelected = parent && hasExactRate(parent->frameRate());
            QVector<LayerID> allIds;
            if (parent) for (const auto& layer : parent->allLayer())
                if (layer) allIds.append(layer->id());
            const bool allPrecomposed = parentRateAfterSelected && selectedUnpacked && !allIds.isEmpty() &&
                service->precomposeLayersWithUndo(
                    allIds, UniString(QStringLiteral("Rational All Rate")), false, false,
                    PrecomposeMode::MoveAllAttributes);
            const auto allOutcome = service->lastPrecomposeOutcome();
            const auto allChild = service->findComposition(allOutcome.childCompId).ptr.lock();
            const bool allRatePreserved = allPrecomposed && allChild &&
                hasExactRate(allChild->frameRate());
            const bool allUnpacked = allPrecomposed &&
                service->unprecomposeLayerWithUndo(allOutcome.precompLayerId, true);
            const bool undoAvailable = allUnpacked && undo->canUndo();
            if (undoAvailable) undo->undo();
            const auto undoChild = service->findComposition(allOutcome.childCompId).ptr.lock();
            const bool undoRatePreserved = undoAvailable && parent && undoChild &&
                hasExactRate(parent->frameRate()) && hasExactRate(undoChild->frameRate()) &&
                PreComposeManager::instance().isPrecomposeLayer(allOutcome.precompLayerId);
            const bool redoAvailable = undoAvailable && undo->canRedo();
            if (redoAvailable) undo->redo();
            const bool redoRestored = redoAvailable && parent && hasExactRate(parent->frameRate()) &&
                !PreComposeManager::instance().isPrecomposeLayer(allOutcome.precompLayerId);
            rateResult["selectedRatePreserved"] = selectedRatePreserved;
            rateResult["roundTripRatePreserved"] = roundTripRatePreserved;
            rateResult["parentRateAfterSelected"] = parentRateAfterSelected;
            rateResult["allRatePreserved"] = allRatePreserved;
            rateResult["allUnpacked"] = allUnpacked;
            rateResult["undoRatePreserved"] = undoRatePreserved;
            rateResult["redoRestored"] = redoRestored;
            rateResult["passed"] = selectedRatePreserved && roundTripRatePreserved && selectedUnpacked &&
                parentRateAfterSelected && allRatePreserved && allUnpacked &&
                undoRatePreserved && redoRestored;
            if (!rateResult["passed"].toBool())
                rateResult["error"] = QStringLiteral("Exact rational frame rate changed across selected/all precompose or undo/redo");
            service->changeCurrentComposition(rootId);
            childId_ = allOutcome.childCompId;
            hasChild_ = true;
            refresh();
            rateResult["state"] = state();
            return rateResult;
        } else if (name == "test.negativeTimingRoundTrip") {
            QJsonObject timingResult;
            const auto parent = service->currentComposition().lock();
            const auto idsA = namedLayerIds(parent, {QStringLiteral("A")});
            const auto idsB = namedLayerIds(parent, {QStringLiteral("B")});
            if (!parent || idsA.size() != 1 || idsB.size() != 1) {
                timingResult["error"] = QStringLiteral("Negative-timing fixture layers are missing");
                return timingResult;
            }
            const auto rootId = parent->id();
            const auto sourceA = parent->layerById(idsA.front());
            const auto sourceB = parent->layerById(idsB.front());
            if (!sourceA || !sourceB) {
                timingResult["error"] = QStringLiteral("Negative-timing fixture source layers are unavailable");
                return timingResult;
            }
            sourceA->setInPoint(FramePosition(-24));
            sourceA->setOutPoint(FramePosition(136));
            sourceA->setStartTime(FramePosition(-17));
            sourceB->setInPoint(FramePosition(12));
            sourceB->setOutPoint(FramePosition(96));
            sourceB->setStartTime(FramePosition(5));
            auto propertyA = sourceA->getProperty(QStringLiteral("transform.position.x"));
            auto propertyB = sourceB->getProperty(QStringLiteral("transform.position.x"));
            if (!propertyA || !propertyB) {
                timingResult["error"] = QStringLiteral("Negative-timing animation properties are unavailable");
                return timingResult;
            }
            propertyA->clearKeyFrames();
            propertyA->setAnimatable(true);
            const auto scaleA = sourceA->keyframeTimeScale();
            const RationalTime negativeKey(-12, scaleA);
            const RationalTime positiveKey(24, scaleA);
            propertyA->addKeyFrame(negativeKey, 2.5, InterpolationType::Linear);
            propertyA->addKeyFrame(positiveKey, -1.25, InterpolationType::Constant);
            const auto originalKeysA = keyframes(propertyA);
            propertyB->clearKeyFrames();
            propertyB->setAnimatable(true);
            const auto scaleB = sourceB->keyframeTimeScale();
            const RationalTime keyB0(20, scaleB);
            const RationalTime keyB1(60, scaleB);
            propertyB->addKeyFrame(keyB0, 4.0, InterpolationType::Smooth);
            propertyB->addKeyFrame(keyB1, 8.0, InterpolationType::Linear);
            const auto originalKeysB = keyframes(propertyB);
            const double originalSampleA = propertyA->evaluateValue(
                RationalTime(0, scaleA)).toDouble();
            const double originalSampleB = propertyB->evaluateValue(
                RationalTime(40, scaleB)).toDouble();
            const auto hasTiming = [](const ArtifactAbstractLayerPtr& layer,
                                      int64_t inFrame, int64_t outFrame, int64_t startFrame) {
                return layer && layer->inPoint().framePosition() == inFrame &&
                    layer->outPoint().framePosition() == outFrame &&
                    layer->startTime().framePosition() == startFrame;
            };
            const QVector<LayerID> selected{idsA.front(), idsB.front()};
            const bool precomposed = service->precomposeLayersWithUndo(
                selected, UniString(QStringLiteral("Negative Timing")), false, false,
                PrecomposeMode::MoveSelected);
            const auto outcome = service->lastPrecomposeOutcome();
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            const auto wrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                parent->layerById(outcome.precompLayerId));
            const auto childA = child ? child->layerById(idsA.front()) : ArtifactAbstractLayerPtr{};
            const auto childB = child ? child->layerById(idsB.front()) : ArtifactAbstractLayerPtr{};
            const auto childPropertyA = childA ? childA->getProperty(QStringLiteral("transform.position.x"))
                                               : SharedPtr<AbstractProperty>{};
            const auto childPropertyB = childB ? childB->getProperty(QStringLiteral("transform.position.x"))
                                               : SharedPtr<AbstractProperty>{};
            const auto movedKeysA = childPropertyA ? keyframes(childPropertyA) : QJsonArray{};
            const auto movedKeysB = childPropertyB ? keyframes(childPropertyB) : QJsonArray{};
            const bool childTimingNormalized = precomposed && child &&
                hasTiming(childA, 0, 160, 7) && hasTiming(childB, 36, 120, 29) &&
                wrapper && hasTiming(wrapper, -24, 136, -24);
            const bool childKeysShifted = movedKeysA.size() == 2 && movedKeysB.size() == 2 &&
                movedKeysA[0].toObject()["timeValue"].toInt() == 12 &&
                movedKeysA[1].toObject()["timeValue"].toInt() == 48 &&
                movedKeysB[0].toObject()["timeValue"].toInt() == 44 &&
                movedKeysB[1].toObject()["timeValue"].toInt() == 84 &&
                movedKeysA[0].toObject()["value"].toDouble() == 2.5 &&
                movedKeysA[1].toObject()["value"].toDouble() == -1.25 &&
                movedKeysB[0].toObject()["value"].toDouble() == 4.0 &&
                movedKeysB[1].toObject()["value"].toDouble() == 8.0;
            const bool childSamplesPreserved = childPropertyA && childPropertyB &&
                std::abs(childPropertyA->evaluateValue(RationalTime(
                    24, childA->keyframeTimeScale())).toDouble() - originalSampleA) < 1.0e-6 &&
                std::abs(childPropertyB->evaluateValue(RationalTime(
                    64, childB->keyframeTimeScale())).toDouble() - originalSampleB) < 1.0e-6;
            const bool unpacked = precomposed && service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const auto restored = service->currentComposition().lock();
            const auto restoredA = restored ? restored->layerById(idsA.front()) : ArtifactAbstractLayerPtr{};
            const auto restoredB = restored ? restored->layerById(idsB.front()) : ArtifactAbstractLayerPtr{};
            const auto restoredPropertyA = restoredA ? restoredA->getProperty(QStringLiteral("transform.position.x"))
                                                      : SharedPtr<AbstractProperty>{};
            const auto restoredPropertyB = restoredB ? restoredB->getProperty(QStringLiteral("transform.position.x"))
                                                      : SharedPtr<AbstractProperty>{};
            const bool unpackRestored = unpacked && hasTiming(restoredA, -24, 136, -17) &&
                hasTiming(restoredB, 12, 96, 5) && restoredPropertyA && restoredPropertyB &&
                keyframes(restoredPropertyA) == originalKeysA && keyframes(restoredPropertyB) == originalKeysB;
            const bool unpackSamplesPreserved = unpacked && restoredPropertyA && restoredPropertyB &&
                std::abs(restoredPropertyA->evaluateValue(RationalTime(
                    0, restoredA->keyframeTimeScale())).toDouble() - originalSampleA) < 1.0e-6 &&
                std::abs(restoredPropertyB->evaluateValue(RationalTime(
                    40, restoredB->keyframeTimeScale())).toDouble() - originalSampleB) < 1.0e-6;
            const bool undoAvailable = undo->canUndo();
            if (undoAvailable) undo->undo();
            const auto undoChild = service->findComposition(outcome.childCompId).ptr.lock();
            const auto undoA = undoChild ? undoChild->layerById(idsA.front()) : ArtifactAbstractLayerPtr{};
            const auto undoB = undoChild ? undoChild->layerById(idsB.front()) : ArtifactAbstractLayerPtr{};
            const auto undoPropertyA = undoA
                ? undoA->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const auto undoPropertyB = undoB
                ? undoB->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const bool undoRestored = undoAvailable && undoChild &&
                PreComposeManager::instance().isPrecomposeLayer(outcome.precompLayerId) &&
                hasTiming(undoA, 0, 160, 7) && hasTiming(undoB, 36, 120, 29);
            const bool undoSamplesPreserved = undoRestored && undoPropertyA && undoPropertyB &&
                std::abs(undoPropertyA->evaluateValue(RationalTime(
                    24, undoA->keyframeTimeScale())).toDouble() - originalSampleA) < 1.0e-6 &&
                std::abs(undoPropertyB->evaluateValue(RationalTime(
                    64, undoB->keyframeTimeScale())).toDouble() - originalSampleB) < 1.0e-6;
            const bool redoAvailable = undo->canRedo();
            if (redoAvailable) undo->redo();
            const auto redoParent = service->currentComposition().lock();
            const auto redoA = redoParent ? redoParent->layerById(idsA.front()) : ArtifactAbstractLayerPtr{};
            const auto redoB = redoParent ? redoParent->layerById(idsB.front()) : ArtifactAbstractLayerPtr{};
            const auto redoPropertyA = redoA ? redoA->getProperty(QStringLiteral("transform.position.x"))
                                              : SharedPtr<AbstractProperty>{};
            const auto redoPropertyB = redoB ? redoB->getProperty(QStringLiteral("transform.position.x"))
                                              : SharedPtr<AbstractProperty>{};
            const bool redoRestored = redoAvailable && redoParent && redoParent->id() == rootId &&
                hasTiming(redoA, -24, 136, -17) && hasTiming(redoB, 12, 96, 5) &&
                redoPropertyA && redoPropertyB && keyframes(redoPropertyA) == originalKeysA &&
                keyframes(redoPropertyB) == originalKeysB;
            const bool redoSamplesPreserved = redoRestored &&
                std::abs(redoPropertyA->evaluateValue(RationalTime(
                    0, redoA->keyframeTimeScale())).toDouble() - originalSampleA) < 1.0e-6 &&
                std::abs(redoPropertyB->evaluateValue(RationalTime(
                    40, redoB->keyframeTimeScale())).toDouble() - originalSampleB) < 1.0e-6;
            timingResult["childTimingNormalized"] = childTimingNormalized;
            timingResult["childKeysShifted"] = childKeysShifted;
            timingResult["childSamplesPreserved"] = childSamplesPreserved;
            timingResult["unpackRestored"] = unpackRestored;
            timingResult["unpackSamplesPreserved"] = unpackSamplesPreserved;
            timingResult["undoRestored"] = undoRestored;
            timingResult["undoSamplesPreserved"] = undoSamplesPreserved;
            timingResult["redoRestored"] = redoRestored;
            timingResult["redoSamplesPreserved"] = redoSamplesPreserved;
            timingResult["passed"] = childTimingNormalized && childKeysShifted &&
                childSamplesPreserved && unpackRestored && unpackSamplesPreserved &&
                undoRestored && undoSamplesPreserved && redoRestored && redoSamplesPreserved;
            if (!timingResult["passed"].toBool())
                timingResult["error"] = QStringLiteral("Negative timing or key offsets changed across precompose/unprecompose/undo/redo");
            childId_ = outcome.childCompId;
            hasChild_ = true;
            refresh();
            timingResult["state"] = state();
            return timingResult;
        } else if (name == "test.animatedTiming") {
            QJsonObject animationResult;
            const auto parent = service->currentComposition().lock();
            const auto idsA = namedLayerIds(parent, {QStringLiteral("A")});
            const auto idsB = namedLayerIds(parent, {QStringLiteral("B")});
            const auto idsUntouched = namedLayerIds(parent, {QStringLiteral("Untouched")});
            if (!parent || idsA.size() != 1 || idsB.size() != 1 || idsUntouched.size() != 1) {
                animationResult["error"] = QStringLiteral("Animated precompose fixture layers are missing");
                return animationResult;
            }
            const auto exactAnimationRate = FrameRate::fromRational(30000, 1001);
            parent->setFrameRate(exactAnimationRate);
            const auto source = parent->layerById(idsA.front());
            const auto sourceB = parent->layerById(idsB.front());
            auto property = source ? source->getProperty(QStringLiteral("transform.position.x"))
                                   : SharedPtr<AbstractProperty>{};
            auto propertyB = sourceB ? sourceB->getProperty(QStringLiteral("transform.position.x"))
                                     : SharedPtr<AbstractProperty>{};
            if (!property || !propertyB) {
                animationResult["error"] = QStringLiteral("Position X animation property is unavailable");
                return animationResult;
            }
            const bool rationalTimebaseAuthored = parent->frameRate().hasExactRational() &&
                parent->frameRate().numerator() == 30000 &&
                parent->frameRate().denominator() == 1001 &&
                source->keyframeTimeScale() == 30 && sourceB->keyframeTimeScale() == 30;
            property->clearKeyFrames();
            property->setAnimatable(true);
            const auto keyScale = source->keyframeTimeScale();
            const RationalTime firstTime(40, keyScale);
            const RationalTime secondTime(100, keyScale);
            property->addKeyFrame(firstTime, 1.5, InterpolationType::Bezier,
                                  0.2f, 0.3f, 0.7f, 0.8f, true);
            property->setKeyFrameAnchorAt(firstTime, KeyFrame::Anchor::LockToIn);
            property->setKeyFrameColorLabelAt(firstTime, KeyFrame::ColorLabel::Blue);
            property->setKeyFrameSoftAt(firstTime, true);
            property->addKeyFrame(secondTime, -2.25, InterpolationType::Constant);
            const auto originalKeys = keyframes(property);
            propertyB->clearKeyFrames();
            propertyB->setAnimatable(true);
            const auto keyScaleB = sourceB->keyframeTimeScale();
            const RationalTime firstTimeB(70, keyScaleB);
            const RationalTime secondTimeB(130, keyScaleB);
            propertyB->addKeyFrame(firstTimeB, 3.75, InterpolationType::Smooth);
            propertyB->setKeyFrameAnchorAt(firstTimeB, KeyFrame::Anchor::LockToIn);
            propertyB->addKeyFrame(secondTimeB, 9.25, InterpolationType::Linear);
            const auto originalKeysB = keyframes(propertyB);
            const QStringList animationChannelPaths{
                QStringLiteral("transform.rotation.x"),
                QStringLiteral("transform.scale.x"),
                QStringLiteral("transform.anchor.x"),
                QStringLiteral("layer.opacity")};
            QVector<QJsonArray> originalAnimationKeysA;
            QVector<QJsonArray> originalAnimationKeysB;
            for (qsizetype index = 0; index < animationChannelPaths.size(); ++index) {
                const auto channelA = source->getProperty(animationChannelPaths[index]);
                const auto channelB = sourceB->getProperty(animationChannelPaths[index]);
                if (!channelA || !channelB) {
                    animationResult["error"] = QStringLiteral("An animation channel is unavailable");
                    return animationResult;
                }
                const bool opacityChannel =
                    animationChannelPaths[index] == QStringLiteral("layer.opacity");
                channelA->clearKeyFrames();
                channelA->setAnimatable(true);
                if (opacityChannel) {
                    channelA->addKeyFrame(firstTime, 0.2, InterpolationType::Bezier);
                    channelA->addKeyFrame(secondTime, 0.8, InterpolationType::Constant);
                } else {
                    channelA->addKeyFrame(firstTime, 10.0 + static_cast<double>(index),
                                         InterpolationType::Bezier);
                    channelA->addKeyFrame(secondTime, 20.0 + static_cast<double>(index),
                                         InterpolationType::Constant);
                }
                channelB->clearKeyFrames();
                channelB->setAnimatable(true);
                if (opacityChannel) {
                    channelB->addKeyFrame(firstTimeB, 0.35, InterpolationType::Smooth);
                    channelB->addKeyFrame(secondTimeB, 0.9, InterpolationType::Linear);
                } else {
                    channelB->addKeyFrame(firstTimeB, 30.0 + static_cast<double>(index),
                                         InterpolationType::Smooth);
                    channelB->addKeyFrame(secondTimeB, 40.0 + static_cast<double>(index),
                                         InterpolationType::Linear);
                }
                originalAnimationKeysA.append(keyframes(channelA));
                originalAnimationKeysB.append(keyframes(channelB));
            }
            const QStringList sampledAnimationPaths{
                QStringLiteral("transform.position.x"),
                animationChannelPaths[0], animationChannelPaths[1], animationChannelPaths[2],
                animationChannelPaths[3]};
            const auto sampleChannels = [&](const ArtifactAbstractLayerPtr& layer, int frame) {
                QVector<double> samples;
                if (!layer) return samples;
                const RationalTime sampleTime(frame, layer->keyframeTimeScale());
                for (const auto& path : sampledAnimationPaths) {
                    const auto channel = layer->getProperty(path);
                    if (!channel) return QVector<double>{};
                    samples.append(channel->evaluateValue(sampleTime).toDouble());
                }
                const auto transform = layer->transform3D().snapshotAt(sampleTime);
                samples.append(transform.positionX);
                samples.append(transform.positionY);
                samples.append(transform.positionZ);
                samples.append(transform.rotation);
                samples.append(transform.rotationX);
                samples.append(transform.rotationY);
                samples.append(transform.rotationZ);
                samples.append(transform.scaleX);
                samples.append(transform.scaleY);
                samples.append(transform.scaleZ);
                samples.append(transform.anchorX);
                samples.append(transform.anchorY);
                samples.append(transform.anchorZ);
                return samples;
            };
            const auto sampledChannelsMatch = [&](const ArtifactAbstractLayerPtr& layer, int frame,
                                                  const QVector<double>& expected) {
                if (!layer || expected.size() != sampledAnimationPaths.size() + 13) return false;
                const auto actual = sampleChannels(layer, frame);
                if (actual.size() != expected.size()) return false;
                for (qsizetype index = 0; index < expected.size(); ++index)
                    if (!std::isfinite(actual[index]) ||
                        std::abs(actual[index] - expected[index]) > 1.0e-6) return false;
                return true;
            };
            const auto originalSamplesA = sampleChannels(source, 70);
            const auto originalSamplesB = sampleChannels(sourceB, 100);
            const auto channelKeysMatch = [](const ArtifactCompositionPtr& composition,
                                             LayerID layerId, const QStringList& paths,
                                             const QVector<QJsonArray>& expected) {
                if (!composition || paths.size() != expected.size()) return false;
                const auto layer = composition->layerById(layerId);
                if (!layer) return false;
                for (qsizetype index = 0; index < paths.size(); ++index) {
                    const auto property = layer->getProperty(paths[index]);
                    if (!property || keyframes(property) != expected[index]) return false;
                }
                return true;
            };
            const auto channelKeysShifted = [](const ArtifactAbstractLayerPtr& layer,
                                               const QStringList& paths,
                                               const QVector<QJsonArray>& expected,
                                               int64_t frameOffset) {
                if (!layer || paths.size() != expected.size()) return false;
                for (qsizetype channel = 0; channel < paths.size(); ++channel) {
                    const auto property = layer->getProperty(paths[channel]);
                    if (!property) return false;
                    const auto actual = keyframes(property);
                    if (actual.size() != expected[channel].size()) return false;
                    for (qsizetype keyIndex = 0; keyIndex < actual.size(); ++keyIndex) {
                        auto expectedKey = expected[channel][keyIndex].toObject();
                        expectedKey["timeValue"] = expectedKey["timeValue"].toInt() +
                            static_cast<int>(frameOffset);
                        if (actual[keyIndex].toObject() != expectedKey) return false;
                    }
                }
                return true;
            };
            const QVector<LayerID> selected{idsA.front(), idsB.front()};
            const bool precomposed = service->precomposeLayersWithUndo(
                selected, UniString(QStringLiteral("Animated Precompose")), false, false,
                PrecomposeMode::MoveSelected);
            if (!precomposed) {
                animationResult["error"] = QStringLiteral("Could not precompose animated layers");
                return animationResult;
            }
            const auto outcome = service->lastPrecomposeOutcome();
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            const auto movedSource = child ? child->layerById(idsA.front()) : ArtifactAbstractLayerPtr{};
            const auto childProperty = movedSource
                ? movedSource->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const auto movedKeys = childProperty ? keyframes(childProperty) : QJsonArray{};
            const auto movedSourceB = child ? child->layerById(idsB.front()) : ArtifactAbstractLayerPtr{};
            const auto childPropertyB = movedSourceB
                ? movedSourceB->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const auto movedKeysB = childPropertyB ? keyframes(childPropertyB) : QJsonArray{};
            const auto movedSourceChannelsA = movedSource && channelKeysShifted(
                movedSource, animationChannelPaths, originalAnimationKeysA, -40);
            const auto movedSourceChannelsB = movedSourceB && channelKeysShifted(
                movedSourceB, animationChannelPaths, originalAnimationKeysB, -40);
            const bool animationChannelsRetimed = movedSourceChannelsA && movedSourceChannelsB;
            const bool selectedChildSamplesPreserved =
                sampledChannelsMatch(movedSource, 30, originalSamplesA) &&
                sampledChannelsMatch(movedSourceB, 60, originalSamplesB);
            const bool rationalChildTimebasePreserved = child &&
                child->frameRate().hasExactRational() &&
                child->frameRate().numerator() == 30000 &&
                child->frameRate().denominator() == 1001 &&
                movedSource && movedSource->keyframeTimeScale() == 30 &&
                movedSourceB && movedSourceB->keyframeTimeScale() == 30;
            const bool childKeysRetimed = childProperty && movedKeys.size() == 2 &&
                movedKeys[0].toObject()["timeValue"].toInt() == 0 &&
                movedKeys[0].toObject()["timeScale"].toDouble() == static_cast<double>(keyScale) &&
                movedKeys[1].toObject()["timeValue"].toInt() == 100 &&
                movedKeys[1].toObject()["timeScale"].toDouble() == static_cast<double>(keyScale) &&
                movedKeys[0].toObject()["value"].toDouble() == 1.5 &&
                movedKeys[1].toObject()["value"].toDouble() == -2.25 &&
                movedKeys[0].toObject()["anchor"].toInt() == static_cast<int>(KeyFrame::Anchor::LockToIn) &&
                movedKeys[0].toObject()["interpolation"].toInt() == static_cast<int>(InterpolationType::Bezier) &&
                movedKeys[0].toObject()["cp1x"].toDouble() == 0.2 &&
                movedKeys[0].toObject()["cp1y"].toDouble() == 0.3 &&
                movedKeys[0].toObject()["cp2x"].toDouble() == 0.7 &&
                movedKeys[0].toObject()["cp2y"].toDouble() == 0.8 &&
                movedKeys[0].toObject()["roving"].toBool() &&
                movedKeys[0].toObject()["colorLabel"].toInt() == static_cast<int>(KeyFrame::ColorLabel::Blue) &&
                movedKeys[0].toObject()["soft"].toBool() &&
                movedKeys[1].toObject()["interpolation"].toInt() == static_cast<int>(InterpolationType::Constant);
            const bool offsetChildKeysRetimed = childPropertyB && movedKeysB.size() == 2 &&
                movedKeysB[0].toObject()["timeValue"].toInt() == 30 &&
                movedKeysB[0].toObject()["timeScale"].toDouble() == static_cast<double>(keyScaleB) &&
                movedKeysB[1].toObject()["timeValue"].toInt() == 130 &&
                movedKeysB[1].toObject()["timeScale"].toDouble() == static_cast<double>(keyScaleB) &&
                movedKeysB[0].toObject()["value"].toDouble() == 3.75 &&
                movedKeysB[1].toObject()["value"].toDouble() == 9.25 &&
                movedKeysB[0].toObject()["anchor"].toInt() == static_cast<int>(KeyFrame::Anchor::LockToIn) &&
                movedKeysB[0].toObject()["interpolation"].toInt() == static_cast<int>(InterpolationType::Smooth) &&
                movedKeysB[1].toObject()["interpolation"].toInt() == static_cast<int>(InterpolationType::Linear);
            const bool unpacked = service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const auto restored = service->currentComposition().lock();
            const auto restoredSource = restored ? restored->layerById(idsA.front()) : ArtifactAbstractLayerPtr{};
            const auto restoredProperty = restoredSource
                ? restoredSource->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const auto restoredSourceB = restored ? restored->layerById(idsB.front()) : ArtifactAbstractLayerPtr{};
            const auto restoredPropertyB = restoredSourceB
                ? restoredSourceB->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const bool unpackKeysRestored = unpacked && restoredProperty &&
                keyframes(restoredProperty) == originalKeys && restoredPropertyB &&
                keyframes(restoredPropertyB) == originalKeysB;
            const bool animationChannelsUnpacked = unpacked &&
                channelKeysMatch(restored, idsA.front(), animationChannelPaths, originalAnimationKeysA) &&
                channelKeysMatch(restored, idsB.front(), animationChannelPaths, originalAnimationKeysB);
            const bool selectedUnpackSamplesPreserved = unpacked &&
                sampledChannelsMatch(restoredSource, 70, originalSamplesA) &&
                sampledChannelsMatch(restoredSourceB, 100, originalSamplesB);
            const bool undoAvailable = undo->canUndo();
            if (undoAvailable) undo->undo();
            const auto afterUnpackUndoChild = service->findComposition(outcome.childCompId).ptr.lock();
            const auto afterUnpackUndoA = afterUnpackUndoChild
                ? afterUnpackUndoChild->layerById(idsA.front()) : ArtifactAbstractLayerPtr{};
            const auto afterUnpackUndoB = afterUnpackUndoChild
                ? afterUnpackUndoChild->layerById(idsB.front()) : ArtifactAbstractLayerPtr{};
            const bool selectedUndoSamplesPreserved = undoAvailable &&
                sampledChannelsMatch(afterUnpackUndoA, 30, originalSamplesA) &&
                sampledChannelsMatch(afterUnpackUndoB, 60, originalSamplesB);
            const bool redoAvailable = undo->canRedo();
            if (redoAvailable) undo->redo();
            const auto afterRedo = service->currentComposition().lock();
            const auto redoSource = afterRedo ? afterRedo->layerById(idsA.front()) : ArtifactAbstractLayerPtr{};
            const auto redoProperty = redoSource
                ? redoSource->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const auto redoSourceB = afterRedo ? afterRedo->layerById(idsB.front()) : ArtifactAbstractLayerPtr{};
            const auto redoPropertyB = redoSourceB
                ? redoSourceB->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const bool redoKeysRestored = redoAvailable && redoProperty &&
                keyframes(redoProperty) == originalKeys && redoPropertyB &&
                keyframes(redoPropertyB) == originalKeysB;
            const bool animationChannelsRedoPreserved = redoAvailable &&
                channelKeysMatch(afterRedo, idsA.front(), animationChannelPaths, originalAnimationKeysA) &&
                channelKeysMatch(afterRedo, idsB.front(), animationChannelPaths, originalAnimationKeysB);
            const bool selectedRedoSamplesPreserved = redoAvailable &&
                sampledChannelsMatch(redoSource, 70, originalSamplesA) &&
                sampledChannelsMatch(redoSourceB, 100, originalSamplesB);
            QVector<LayerID> allLayerIds;
            if (afterRedo) for (const auto& layer : afterRedo->allLayer())
                if (layer) allLayerIds.append(layer->id());
            const auto hasTiming = [](const ArtifactAbstractLayerPtr& layer,
                                      int64_t inFrame, int64_t outFrame, int64_t startFrame) {
                return layer && layer->inPoint().framePosition() == inFrame &&
                    layer->outPoint().framePosition() == outFrame &&
                    layer->startTime().framePosition() == startFrame;
            };
            const bool allPrecomposed = redoKeysRestored && !allLayerIds.isEmpty() &&
                service->precomposeLayersWithUndo(
                    allLayerIds, UniString(QStringLiteral("Animated Move All")), false, false,
                    PrecomposeMode::MoveAllAttributes);
            if (!allPrecomposed) {
                animationResult["error"] = QStringLiteral("Could not precompose animated layers in MoveAllAttributes mode");
                return animationResult;
            }
            const auto allOutcome = service->lastPrecomposeOutcome();
            const auto allChild = service->findComposition(allOutcome.childCompId).ptr.lock();
            const auto allSourceA = allChild ? allChild->layerById(idsA.front()) : ArtifactAbstractLayerPtr{};
            const auto allSourceB = allChild ? allChild->layerById(idsB.front()) : ArtifactAbstractLayerPtr{};
            const auto allSourceUntouched = allChild
                ? allChild->layerById(idsUntouched.front()) : ArtifactAbstractLayerPtr{};
            const bool allModeChildTimingPreserved =
                hasTiming(allSourceA, 40, 200, 7) && hasTiming(allSourceB, 70, 160, 9) &&
                hasTiming(allSourceUntouched, 25, 220, 3);
            const bool allModeRationalTimebasePreserved = allChild &&
                allChild->frameRate().hasExactRational() &&
                allChild->frameRate().numerator() == 30000 &&
                allChild->frameRate().denominator() == 1001 &&
                allSourceA && allSourceA->keyframeTimeScale() == 30 &&
                allSourceB && allSourceB->keyframeTimeScale() == 30;
            const auto allPropertyA = allSourceA
                ? allSourceA->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const auto allPropertyB = allSourceB
                ? allSourceB->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const bool allModeChildKeysPreserved = allPropertyA && allPropertyB &&
                keyframes(allPropertyA) == originalKeys && keyframes(allPropertyB) == originalKeysB;
            const bool allModeChildSamplesPreserved =
                sampledChannelsMatch(allSourceA, 70, originalSamplesA) &&
                sampledChannelsMatch(allSourceB, 100, originalSamplesB);
            const bool allModeChildAnimationKeysPreserved =
                channelKeysMatch(allChild, idsA.front(), animationChannelPaths, originalAnimationKeysA) &&
                channelKeysMatch(allChild, idsB.front(), animationChannelPaths, originalAnimationKeysB);
            const bool allUnpacked = service->unprecomposeLayerWithUndo(allOutcome.precompLayerId, true);
            const auto allRestored = service->currentComposition().lock();
            const auto allRestoredA = allRestored ? allRestored->layerById(idsA.front()) : ArtifactAbstractLayerPtr{};
            const auto allRestoredB = allRestored ? allRestored->layerById(idsB.front()) : ArtifactAbstractLayerPtr{};
            const auto allRestoredUntouched = allRestored
                ? allRestored->layerById(idsUntouched.front()) : ArtifactAbstractLayerPtr{};
            const auto allRestoredPropertyA = allRestoredA
                ? allRestoredA->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const auto allRestoredPropertyB = allRestoredB
                ? allRestoredB->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const bool allModeUnpackKeysPreserved = allUnpacked && allRestoredPropertyA &&
                allRestoredPropertyB && keyframes(allRestoredPropertyA) == originalKeys &&
                keyframes(allRestoredPropertyB) == originalKeysB;
            const bool allModeUnpackAnimationKeysPreserved = allUnpacked &&
                channelKeysMatch(allRestored, idsA.front(), animationChannelPaths, originalAnimationKeysA) &&
                channelKeysMatch(allRestored, idsB.front(), animationChannelPaths, originalAnimationKeysB);
            const bool allModeUnpackTimingPreserved = allUnpacked &&
                hasTiming(allRestoredA, 40, 200, 7) && hasTiming(allRestoredB, 70, 160, 9) &&
                hasTiming(allRestoredUntouched, 25, 220, 3);
            const bool allModeUnpackSamplesPreserved = allUnpacked &&
                sampledChannelsMatch(allRestoredA, 70, originalSamplesA) &&
                sampledChannelsMatch(allRestoredB, 100, originalSamplesB);
            const bool allUndoAvailable = undo->canUndo();
            if (allUndoAvailable) undo->undo();
            const bool allRedoAvailable = undo->canRedo();
            if (allRedoAvailable) undo->redo();
            const auto allAfterRedo = service->currentComposition().lock();
            const auto allRedoA = allAfterRedo ? allAfterRedo->layerById(idsA.front()) : ArtifactAbstractLayerPtr{};
            const auto allRedoB = allAfterRedo ? allAfterRedo->layerById(idsB.front()) : ArtifactAbstractLayerPtr{};
            const auto allRedoUntouched = allAfterRedo
                ? allAfterRedo->layerById(idsUntouched.front()) : ArtifactAbstractLayerPtr{};
            const auto allRedoPropertyA = allRedoA
                ? allRedoA->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const auto allRedoPropertyB = allRedoB
                ? allRedoB->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const bool allModeRedoKeysPreserved = allRedoAvailable && allRedoPropertyA &&
                allRedoPropertyB && keyframes(allRedoPropertyA) == originalKeys &&
                keyframes(allRedoPropertyB) == originalKeysB;
            const bool allModeRedoAnimationKeysPreserved = allRedoAvailable &&
                channelKeysMatch(allAfterRedo, idsA.front(), animationChannelPaths, originalAnimationKeysA) &&
                channelKeysMatch(allAfterRedo, idsB.front(), animationChannelPaths, originalAnimationKeysB);
            const bool allModeRedoTimingPreserved = allRedoAvailable &&
                hasTiming(allRedoA, 40, 200, 7) && hasTiming(allRedoB, 70, 160, 9) &&
                hasTiming(allRedoUntouched, 25, 220, 3);
            const bool allModeRedoSamplesPreserved = allRedoAvailable &&
                sampledChannelsMatch(allRedoA, 70, originalSamplesA) &&
                sampledChannelsMatch(allRedoB, 100, originalSamplesB);
            animationResult["originalKeys"] = originalKeys;
            animationResult["originalOffsetKeys"] = originalKeysB;
            animationResult["precomposedKeys"] = movedKeys;
            animationResult["precomposedOffsetKeys"] = movedKeysB;
            animationResult["childKeysRetimed"] = childKeysRetimed;
            animationResult["offsetChildKeysRetimed"] = offsetChildKeysRetimed;
            animationResult["animationChannelsRetimed"] = animationChannelsRetimed;
            animationResult["rationalTimebaseAuthored"] = rationalTimebaseAuthored;
            animationResult["rationalChildTimebasePreserved"] = rationalChildTimebasePreserved;
            animationResult["selectedChildSamplesPreserved"] = selectedChildSamplesPreserved;
            animationResult["animationChannelsUnpacked"] = animationChannelsUnpacked;
            animationResult["selectedUnpackSamplesPreserved"] = selectedUnpackSamplesPreserved;
            animationResult["selectedUndoSamplesPreserved"] = selectedUndoSamplesPreserved;
            animationResult["animationChannelsRedoPreserved"] = animationChannelsRedoPreserved;
            animationResult["selectedRedoSamplesPreserved"] = selectedRedoSamplesPreserved;
            animationResult["unpackRestoredKeys"] = unpackKeysRestored;
            animationResult["redoRestoredKeys"] = redoKeysRestored;
            animationResult["allModeChildKeysPreserved"] = allModeChildKeysPreserved;
            animationResult["allModeChildAnimationKeysPreserved"] = allModeChildAnimationKeysPreserved;
            animationResult["allModeChildTimingPreserved"] = allModeChildTimingPreserved;
            animationResult["allModeRationalTimebasePreserved"] = allModeRationalTimebasePreserved;
            animationResult["allModeChildSamplesPreserved"] = allModeChildSamplesPreserved;
            animationResult["allModeUnpackKeysPreserved"] = allModeUnpackKeysPreserved;
            animationResult["allModeUnpackAnimationKeysPreserved"] = allModeUnpackAnimationKeysPreserved;
            animationResult["allModeUnpackTimingPreserved"] = allModeUnpackTimingPreserved;
            animationResult["allModeUnpackSamplesPreserved"] = allModeUnpackSamplesPreserved;
            animationResult["allModeRedoKeysPreserved"] = allModeRedoKeysPreserved;
            animationResult["allModeRedoAnimationKeysPreserved"] = allModeRedoAnimationKeysPreserved;
            animationResult["allModeRedoTimingPreserved"] = allModeRedoTimingPreserved;
            animationResult["allModeRedoSamplesPreserved"] = allModeRedoSamplesPreserved;
            animationResult["passed"] = rationalTimebaseAuthored && rationalChildTimebasePreserved &&
                childKeysRetimed && offsetChildKeysRetimed &&
                animationChannelsRetimed && selectedChildSamplesPreserved &&
                animationChannelsUnpacked && selectedUnpackSamplesPreserved &&
                selectedUndoSamplesPreserved && animationChannelsRedoPreserved &&
                selectedRedoSamplesPreserved &&
                unpackKeysRestored &&
                undoAvailable && redoKeysRestored && allModeChildKeysPreserved &&
                allModeChildAnimationKeysPreserved &&
                allModeChildTimingPreserved && allModeRationalTimebasePreserved &&
                allModeChildSamplesPreserved &&
                allModeUnpackKeysPreserved &&
                allModeUnpackAnimationKeysPreserved && allModeUnpackTimingPreserved &&
                allModeUnpackSamplesPreserved &&
                allUndoAvailable && allModeRedoKeysPreserved && allModeRedoAnimationKeysPreserved &&
                allModeRedoTimingPreserved && allModeRedoSamplesPreserved;
            if (!animationResult["passed"].toBool())
                animationResult["error"] = QStringLiteral("Animated keyframes changed across precompose/unpack/redo");
            refresh();
            animationResult["state"] = state();
            return animationResult;
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
            LayerID originalAId;
            LayerID originalBId;
            if (middle) for (const auto& layer : middle->allLayer()) {
                if (!layer) continue;
                if (layer->layerName() == QStringLiteral("A")) originalAId = layer->id();
                if (layer->layerName() == QStringLiteral("B")) originalBId = layer->id();
            }
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
        } else if (name == "test.nestedExternalParentRestored") {
            QJsonObject result;
            const auto root = service->currentComposition().lock();
            if (!root) {
                result["error"] = QStringLiteral("No root composition");
                return result;
            }
            const auto rootId = root->id();
            service->addLayerToCurrentComposition(
                ArtifactNullLayerInitParams(QStringLiteral("Untouched Two")), false);
            const auto idsA = namedLayerIds(root, {QStringLiteral("A")});
            const auto idsB = namedLayerIds(root, {QStringLiteral("B")});
            const auto externalIds = namedLayerIds(root, {QStringLiteral("Untouched")});
            const auto secondExternalIds = namedLayerIds(root, {QStringLiteral("Untouched Two")});
            if (idsA.size() != 1 || idsB.size() != 1 || externalIds.size() != 1 ||
                secondExternalIds.size() != 1) {
                result["error"] = QStringLiteral("Nested external-parent fixture layers are missing");
                return result;
            }
            const auto sourceId = idsA.front();
            const auto sourceBId = idsB.front();
            const auto externalParentId = externalIds.front();
            const auto secondExternalParentId = secondExternalIds.front();
            const auto source = root->layerById(sourceId);
            const auto sourceB = root->layerById(sourceBId);
            if (!source || !sourceB) {
                result["error"] = QStringLiteral("Nested external-parent source is unavailable");
                return result;
            }
            source->setParentById(externalParentId);
            sourceB->setParentById(secondExternalParentId);
            const QVector<LayerID> selected{sourceId, sourceBId};
            const bool innerCreated = source->parentLayerId() == externalParentId &&
                sourceB->parentLayerId() == secondExternalParentId &&
                service->precomposeLayersWithUndo(
                    selected, UniString(QStringLiteral("Inner External Parent")), false, false,
                    PrecomposeMode::MoveSelected);
            const auto inner = service->lastPrecomposeOutcome();
            const auto innerWrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                root->layerById(inner.precompLayerId));
            const bool innerMetadataAuthored = innerCreated && innerWrapper &&
                innerWrapper->restoreExternalParent(sourceId) == externalParentId &&
                innerWrapper->restoreExternalParent(sourceBId) == secondExternalParentId;
            const bool outerCreated = innerMetadataAuthored && service->precomposeLayersWithUndo(
                {inner.precompLayerId}, UniString(QStringLiteral("Outer External Parent")),
                false, false, PrecomposeMode::MoveSelected);
            const auto outer = service->lastPrecomposeOutcome();
            const auto outerChild = service->findComposition(outer.childCompId).ptr.lock();
            const auto nestedWrapper = outerChild
                ? dynamicPointerCast<ArtifactCompositionLayer>(outerChild->layerById(inner.precompLayerId))
                : SharedPtr<ArtifactCompositionLayer>{};
            const auto hierarchy = PreComposeManager::instance().getCompositionHierarchy(inner.childCompId);
            const bool nestedMetadataPreserved = outerCreated && nestedWrapper &&
                nestedWrapper->restoreExternalParent(sourceId) == externalParentId &&
                nestedWrapper->restoreExternalParent(sourceBId) == secondExternalParentId &&
                hierarchy.size() == 3 && hierarchy[0] == rootId &&
                hierarchy[1] == outer.childCompId && hierarchy[2] == inner.childCompId;
            const bool outerUnpacked = nestedMetadataPreserved &&
                service->unprecomposeLayerWithUndo(outer.precompLayerId, true);
            const auto restoredInnerWrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                root->layerById(inner.precompLayerId));
            const bool innerMappingRestored = outerUnpacked && restoredInnerWrapper &&
                restoredInnerWrapper->restoreExternalParent(sourceId) == externalParentId &&
                restoredInnerWrapper->restoreExternalParent(sourceBId) == secondExternalParentId &&
                PreComposeManager::instance().isPrecomposeLayer(inner.precompLayerId) &&
                PreComposeManager::instance().getParentComposition(inner.childCompId) == rootId;
            const bool innerUnpacked = innerMappingRestored &&
                service->unprecomposeLayerWithUndo(inner.precompLayerId, true);
            const auto restoredSource = root->layerById(sourceId);
            const auto restoredSourceB = root->layerById(sourceBId);
            const bool externalParentARestored = innerUnpacked && restoredSource &&
                restoredSource->parentLayerId() == externalParentId && root->layerById(externalParentId);
            const bool externalParentBRestored = innerUnpacked && restoredSourceB &&
                restoredSourceB->parentLayerId() == secondExternalParentId &&
                root->layerById(secondExternalParentId);
            const bool externalParentRestored = externalParentARestored && externalParentBRestored;
            const bool undoAvailable = innerUnpacked && undo->canUndo();
            if (undoAvailable) undo->undo();
            const auto undoWrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                root->layerById(inner.precompLayerId));
            const bool undoRestoredMetadata = undoAvailable && undoWrapper &&
                undoWrapper->restoreExternalParent(sourceId) == externalParentId &&
                undoWrapper->restoreExternalParent(sourceBId) == secondExternalParentId &&
                PreComposeManager::instance().isPrecomposeLayer(inner.precompLayerId);
            const bool redoAvailable = undoAvailable && undo->canRedo();
            if (redoAvailable) undo->redo();
            const auto redoSource = root->layerById(sourceId);
            const auto redoSourceB = root->layerById(sourceBId);
            const bool redoRestoredParent = redoAvailable && redoSource &&
                redoSource->parentLayerId() == externalParentId &&
                redoSourceB && redoSourceB->parentLayerId() == secondExternalParentId &&
                !PreComposeManager::instance().isPrecomposeLayer(inner.precompLayerId);
            result["innerMetadataAuthored"] = innerMetadataAuthored;
            result["nestedMetadataPreserved"] = nestedMetadataPreserved;
            result["outerUnpacked"] = outerUnpacked;
            result["innerMappingRestored"] = innerMappingRestored;
            result["distinctExternalParents"] = externalParentId != secondExternalParentId;
            result["externalParentARestored"] = externalParentARestored;
            result["externalParentBRestored"] = externalParentBRestored;
            result["externalParentRestored"] = externalParentRestored;
            result["undoRestoredMetadata"] = undoRestoredMetadata;
            result["redoRestoredParent"] = redoRestoredParent;
            result["hierarchy"] = QJsonArray{rootId.toString(), outer.childCompId.toString(),
                inner.childCompId.toString()};
            result["passed"] = externalParentId != secondExternalParentId &&
                innerMetadataAuthored && nestedMetadataPreserved && outerUnpacked &&
                innerMappingRestored && externalParentRestored && undoRestoredMetadata &&
                redoRestoredParent;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Nested precompose did not preserve external-parent metadata through unpack and undo/redo");
            service->changeCurrentComposition(rootId);
            childId_ = outer.childCompId;
            hasChild_ = true;
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.reparentExistingPrecompose") {
            QJsonObject result;
            auto root = service->currentComposition().lock();
            if (!root) {
                result["error"] = QStringLiteral("No root composition");
                return result;
            }
            const auto rootId = root->id();
            const auto sourceIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            if (sourceIds.size() != 2 || !service->precomposeLayersWithUndo(
                    sourceIds, UniString(QStringLiteral("Existing Precompose")), false, false,
                    PrecomposeMode::MoveSelected)) {
                result["error"] = QStringLiteral("Could not create initial precompose");
                return result;
            }
            const auto existing = service->lastPrecomposeOutcome();
            auto existingWrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                root->layerById(existing.precompLayerId));
            const auto existingChild = service->findComposition(existing.childCompId).ptr.lock();
            if (!existingWrapper || !existingChild) {
                result["error"] = QStringLiteral("Initial precompose wrapper or child is unavailable");
                return result;
            }
            existingWrapper->setInPoint(FramePosition(0));
            existingWrapper->setOutPoint(FramePosition(120));
            existingWrapper->setStartTime(FramePosition(0));
            existingWrapper->setPosition3D({23.0f, -14.0f, 6.0f});
            existingWrapper->setRotation3D(QVector3D(19.0f, -33.0f, 47.0f));
            existingWrapper->transform3D().setCurrentScale(1.35f, 0.65f);
            existingWrapper->transform3D().setCurrentAnchor(8.0f, -11.0f, 2.5f);
            existingWrapper->setOpacity(0.58f);
            const auto rotationChannel = existingWrapper->getProperty(QStringLiteral("transform.rotation.x"));
            if (!rotationChannel) {
                result["error"] = QStringLiteral("Existing precompose wrapper rotation channel is unavailable");
                return result;
            }
            rotationChannel->clearKeyFrames();
            rotationChannel->setAnimatable(true);
            rotationChannel->addKeyFrame(
                RationalTime(12, existingWrapper->keyframeTimeScale()), 17.0,
                InterpolationType::Bezier, 0.1f, 0.2f, 0.8f, 0.9f, true);
            rotationChannel->addKeyFrame(
                RationalTime(36, existingWrapper->keyframeTimeScale()), 49.0,
                InterpolationType::Constant);
            const auto expectedWrapperKeys = keyframes(rotationChannel);
            const auto wrapperTransformMatches = [&](const SharedPtr<ArtifactCompositionLayer>& layer) {
                if (!layer) return false;
                const auto position = layer->position3D();
                const auto rotation = layer->rotation3D();
                const auto& transform = layer->transform3D();
                const auto channel = layer->getProperty(QStringLiteral("transform.rotation.x"));
                return std::abs(position.x - 23.0f) < 1.0e-5f &&
                    std::abs(position.y - (-14.0f)) < 1.0e-5f &&
                    std::abs(position.z - 6.0f) < 1.0e-5f &&
                    std::abs(rotation.x() - 19.0f) < 1.0e-5f &&
                    std::abs(rotation.y() - (-33.0f)) < 1.0e-5f &&
                    std::abs(rotation.z() - 47.0f) < 1.0e-5f &&
                    std::abs(transform.scaleX() - 1.35f) < 1.0e-5f &&
                    std::abs(transform.scaleY() - 0.65f) < 1.0e-5f &&
                    std::abs(transform.anchorX() - 8.0f) < 1.0e-5f &&
                    std::abs(transform.anchorY() - (-11.0f)) < 1.0e-5f &&
                    std::abs(transform.anchorZ() - 2.5f) < 1.0e-5f &&
                    std::abs(layer->opacity() - 0.58f) < 1.0e-5f &&
                    layer->inPoint().framePosition() == 0 &&
                    layer->outPoint().framePosition() == 120 &&
                    layer->startTime().framePosition() == 0 &&
                    channel && keyframes(channel) == expectedWrapperKeys;
            };
            const bool wrapperTransformAuthored = wrapperTransformMatches(existingWrapper);
            if (!wrapperTransformAuthored) {
                result["error"] = QStringLiteral("Could not prepare existing-wrapper transform fixture");
                return result;
            }
            if (!service->precomposeLayersWithUndo(
                    {existing.precompLayerId}, UniString(QStringLiteral("Precompose Wrapper")),
                    false, false, PrecomposeMode::MoveSelected)) {
                result["error"] = QStringLiteral("Could not wrap existing precompose layer");
                return result;
            }
            const auto wrapper = service->lastPrecomposeOutcome();
            auto wrapperChild = service->findComposition(wrapper.childCompId).ptr.lock();
            auto nestedExistingWrapper = wrapperChild
                ? dynamicPointerCast<ArtifactCompositionLayer>(wrapperChild->layerById(existing.precompLayerId))
                : SharedPtr<ArtifactCompositionLayer>{};
            const bool wrapperTransformPreservedInNestedChild = nestedExistingWrapper == existingWrapper &&
                wrapperTransformMatches(nestedExistingWrapper);
            const auto& manager = PreComposeManager::instance();
            const auto hasParentReference = [&](CompositionID expectedParent) {
                for (const auto& reference : manager.getPrecompLayersForChild(existing.childCompId)) {
                    if (reference.parentCompId == expectedParent &&
                        reference.precompLayerId == existing.precompLayerId) return true;
                }
                return false;
            };
            const auto hierarchyAfterMove = manager.getCompositionHierarchy(existing.childCompId);
            const bool parentReferenceUpdated = hasParentReference(wrapper.childCompId);
            const bool hierarchyUpdated = hierarchyAfterMove.size() == 3 &&
                hierarchyAfterMove[0] == rootId && hierarchyAfterMove[1] == wrapper.childCompId &&
                hierarchyAfterMove[2] == existing.childCompId;
            const bool undoAvailable = undo->canUndo();
            if (undoAvailable) undo->undo();
            const auto hierarchyAfterUndo = manager.getCompositionHierarchy(existing.childCompId);
            const auto undoWrapperTransform = dynamicPointerCast<ArtifactCompositionLayer>(
                root->layerById(existing.precompLayerId));
            const bool undoRestoredParent = undoAvailable && hasParentReference(rootId) &&
                hierarchyAfterUndo.size() == 2 && hierarchyAfterUndo[0] == rootId &&
                hierarchyAfterUndo[1] == existing.childCompId && undoWrapperTransform == existingWrapper &&
                wrapperTransformMatches(undoWrapperTransform);
            const bool redoAvailable = undo->canRedo();
            if (redoAvailable) undo->redo();
            const auto hierarchyAfterRedo = manager.getCompositionHierarchy(existing.childCompId);
            const auto redoWrapperChild = service->findComposition(wrapper.childCompId).ptr.lock();
            const auto redoNestedExistingWrapper = redoWrapperChild
                ? dynamicPointerCast<ArtifactCompositionLayer>(
                      redoWrapperChild->layerById(existing.precompLayerId))
                : SharedPtr<ArtifactCompositionLayer>{};
            const bool redoRestoredParent = redoAvailable && hasParentReference(wrapper.childCompId) &&
                hierarchyAfterRedo.size() == 3 && hierarchyAfterRedo[0] == rootId &&
                hierarchyAfterRedo[1] == wrapper.childCompId &&
                hierarchyAfterRedo[2] == existing.childCompId &&
                redoNestedExistingWrapper == existingWrapper &&
                wrapperTransformMatches(redoNestedExistingWrapper);
            QTemporaryDir tempDir;
            const auto filePath = tempDir.filePath(QStringLiteral("reparented-precompose.artifact"));
            const auto save = tempDir.isValid()
                ? ArtifactProjectManager::getInstance().saveToFile(filePath)
                : ArtifactProjectExporterResult{};
            const bool mappingsCleared = save.success &&
                manager.unregisterPrecompLayer(rootId, wrapper.precompLayerId, wrapper.childCompId) &&
                manager.unregisterPrecompLayer(wrapper.childCompId, existing.precompLayerId,
                                               existing.childCompId) &&
                manager.getPrecompLayersForChild(wrapper.childCompId).isEmpty() &&
                manager.getPrecompLayersForChild(existing.childCompId).isEmpty();
            const bool loaded = mappingsCleared && ArtifactProjectManager::getInstance().loadFromFile(filePath);
            if (loaded) {
                service->changeCurrentComposition(rootId);
                root = service->findComposition(rootId).ptr.lock();
                wrapperChild = service->findComposition(wrapper.childCompId).ptr.lock();
                if (!root || !wrapperChild) {
                    result["saveSucceeded"] = save.success;
                    result["mappingsClearedBeforeReload"] = mappingsCleared;
                    result["loaded"] = loaded;
                    result["error"] = QStringLiteral("Reparented compositions were not restored by project reload");
                    return result;
                }
            }
            const auto reloadedExistingChild = loaded
                ? service->findComposition(existing.childCompId).ptr.lock() : ArtifactCompositionPtr{};
            const auto reloadedOuterWrapper = root
                ? dynamicPointerCast<ArtifactCompositionLayer>(root->layerById(wrapper.precompLayerId))
                : SharedPtr<ArtifactCompositionLayer>{};
            if (loaded) {
                nestedExistingWrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                    wrapperChild->layerById(existing.precompLayerId));
                existingWrapper = nestedExistingWrapper;
            }
            bool outerReferenceRestored = false;
            for (const auto& reference : manager.getPrecompLayersForChild(wrapper.childCompId))
                outerReferenceRestored = outerReferenceRestored ||
                    (reference.parentCompId == rootId &&
                     reference.precompLayerId == wrapper.precompLayerId);
            const auto hierarchyAfterReload = manager.getCompositionHierarchy(existing.childCompId);
            const bool reparentSaveReloadRestored = save.success && mappingsCleared && loaded &&
                root && wrapperChild && reloadedExistingChild && reloadedOuterWrapper &&
                reloadedOuterWrapper->sourceCompositionId() == wrapper.childCompId &&
                nestedExistingWrapper && nestedExistingWrapper->sourceCompositionId() == existing.childCompId &&
                outerReferenceRestored && hasParentReference(wrapper.childCompId) &&
                hierarchyAfterReload.size() == 3 && hierarchyAfterReload[0] == rootId &&
                hierarchyAfterReload[1] == wrapper.childCompId &&
                hierarchyAfterReload[2] == existing.childCompId &&
                wrapperTransformMatches(nestedExistingWrapper);
            const bool outerUnpacked = redoRestoredParent && reparentSaveReloadRestored &&
                service->unprecomposeLayerWithUndo(wrapper.precompLayerId, true);
            const auto unpackedExistingWrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                root->layerById(existing.precompLayerId));
            const auto hierarchyAfterUnpack = manager.getCompositionHierarchy(existing.childCompId);
            const bool unpackRestoredOriginalWrapper = outerUnpacked &&
                unpackedExistingWrapper == existingWrapper && wrapperTransformMatches(unpackedExistingWrapper) &&
                hasParentReference(rootId) && hierarchyAfterUnpack.size() == 2 &&
                hierarchyAfterUnpack[0] == rootId && hierarchyAfterUnpack[1] == existing.childCompId;
            const bool unpackUndoAvailable = outerUnpacked && undo->canUndo();
            if (unpackUndoAvailable) undo->undo();
            const auto hierarchyAfterUnpackUndo = manager.getCompositionHierarchy(existing.childCompId);
            const bool unpackUndoRestoredNested = unpackUndoAvailable &&
                hierarchyAfterUnpackUndo.size() == 3 && hierarchyAfterUnpackUndo[0] == rootId &&
                hierarchyAfterUnpackUndo[1] == wrapper.childCompId &&
                hierarchyAfterUnpackUndo[2] == existing.childCompId &&
                hasParentReference(wrapper.childCompId) && wrapperTransformMatches(nestedExistingWrapper);
            const bool unpackRedoAvailable = unpackUndoRestoredNested && undo->canRedo();
            if (unpackRedoAvailable) undo->redo();
            const auto finalExistingWrapper = dynamicPointerCast<ArtifactCompositionLayer>(
                root->layerById(existing.precompLayerId));
            const auto hierarchyAfterUnpackRedo = manager.getCompositionHierarchy(existing.childCompId);
            const bool unpackRedoRestoredOriginal = unpackRedoAvailable &&
                finalExistingWrapper == existingWrapper && wrapperTransformMatches(finalExistingWrapper) &&
                hasParentReference(rootId) && hierarchyAfterUnpackRedo.size() == 2 &&
                hierarchyAfterUnpackRedo[0] == rootId &&
                hierarchyAfterUnpackRedo[1] == existing.childCompId;
            result["existingChildId"] = existing.childCompId.toString();
            result["wrapperChildId"] = wrapper.childCompId.toString();
            result["wrapperTransformAuthored"] = wrapperTransformAuthored;
            result["wrapperTransformPreservedInNestedChild"] = wrapperTransformPreservedInNestedChild;
            result["parentReferenceUpdated"] = parentReferenceUpdated;
            result["hierarchyUpdated"] = hierarchyUpdated;
            result["undoRestoredParent"] = undoRestoredParent;
            result["redoRestoredParent"] = redoRestoredParent;
            result["saveSucceeded"] = save.success;
            result["mappingsClearedBeforeReload"] = mappingsCleared;
            result["loaded"] = loaded;
            result["reparentSaveReloadRestored"] = reparentSaveReloadRestored;
            result["unpackRestoredOriginalWrapper"] = unpackRestoredOriginalWrapper;
            result["unpackUndoRestoredNested"] = unpackUndoRestoredNested;
            result["unpackRedoRestoredOriginal"] = unpackRedoRestoredOriginal;
            result["undoAvailable"] = undoAvailable;
            result["redoAvailable"] = redoAvailable;
            result["hierarchy"] = QJsonArray{rootId.toString(),
                hierarchyAfterRedo.size() > 1 ? hierarchyAfterRedo[1].toString() : QString(),
                hierarchyAfterRedo.size() > 2 ? hierarchyAfterRedo[2].toString() : QString()};
            result["passed"] = wrapperTransformAuthored && wrapperTransformPreservedInNestedChild &&
                parentReferenceUpdated && hierarchyUpdated && undoRestoredParent && redoRestoredParent &&
                reparentSaveReloadRestored &&
                unpackRestoredOriginalWrapper && unpackUndoRestoredNested && unpackRedoRestoredOriginal;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Existing-precompose reparenting or its undo/redo did not restore the runtime hierarchy");
            childId_ = existing.childCompId;
            hasChild_ = true;
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.unprecomposeNested") {
            QJsonObject result;
            const auto root = service->currentComposition().lock();
            if (!root) {
                result["error"] = QStringLiteral("No root composition");
                return result;
            }
            const auto rootId = root->id();
            const auto outerIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            if (outerIds.size() != 2 || !service->precomposeLayersWithUndo(
                    outerIds, UniString(QStringLiteral("Nested Unpack Outer")), false, false,
                    PrecomposeMode::MoveSelected)) {
                result["error"] = QStringLiteral("Could not create outer composition fixture");
                return result;
            }
            const auto outer = service->lastPrecomposeOutcome();
            service->changeCurrentComposition(outer.childCompId);
            const auto middle = service->currentComposition().lock();
            const auto innerIds = namedLayerIds(middle, {QStringLiteral("A"), QStringLiteral("B")});
            if (!middle || innerIds.size() != 2 || !service->precomposeLayersWithUndo(
                    innerIds, UniString(QStringLiteral("Nested Unpack Inner")), false, false,
                    PrecomposeMode::MoveSelected)) {
                result["error"] = QStringLiteral("Could not create inner composition fixture");
                service->changeCurrentComposition(rootId);
                return result;
            }
            const auto inner = service->lastPrecomposeOutcome();
            const auto matchesRestoredLayers = [&]() {
                const auto parent = service->currentComposition().lock();
                if (!parent || parent->id() != outer.childCompId) return false;
                ArtifactAbstractLayerPtr a;
                ArtifactAbstractLayerPtr b;
                for (const auto& layer : parent->allLayer()) {
                    if (!layer) continue;
                    if (layer->layerName() == QStringLiteral("A")) a = layer;
                    if (layer->layerName() == QStringLiteral("B")) b = layer;
                }
                return a && b && a->id() == originalAId && b->id() == originalBId &&
                    a->inPoint().framePosition() == 0 && a->outPoint().framePosition() == 160 &&
                    a->startTime().framePosition() == 7 &&
                    b->inPoint().framePosition() == 30 && b->outPoint().framePosition() == 120 &&
                    b->startTime().framePosition() == 9;
            };
            const auto& manager = PreComposeManager::instance();
            const bool dissolved = service->unprecomposeLayerWithUndo(inner.precompLayerId, true);
            const auto refsAfterUnpack = manager.getPrecompLayersForChild(inner.childCompId);
            const bool nestingRemoved = refsAfterUnpack.isEmpty() &&
                !manager.isPrecomposeLayer(inner.precompLayerId);
            const bool restoredTiming = dissolved && matchesRestoredLayers();
            const bool undoAvailable = undo->canUndo();
            if (undoAvailable) undo->undo();
            bool innerReferenceRestored = false;
            for (const auto& reference : manager.getPrecompLayersForChild(inner.childCompId)) {
                innerReferenceRestored = innerReferenceRestored ||
                    (reference.parentCompId == outer.childCompId &&
                     reference.precompLayerId == inner.precompLayerId);
            }
            const auto undoHierarchy = manager.getCompositionHierarchy(inner.childCompId);
            const bool undoRestoredNesting = undoAvailable && innerReferenceRestored &&
                undoHierarchy.size() == 3 && undoHierarchy[0] == rootId &&
                undoHierarchy[1] == outer.childCompId && undoHierarchy[2] == inner.childCompId;
            const bool redoAvailable = undo->canRedo();
            if (redoAvailable) undo->redo();
            const bool redoRemovedNesting = redoAvailable &&
                manager.getPrecompLayersForChild(inner.childCompId).isEmpty() &&
                !manager.isPrecomposeLayer(inner.precompLayerId) && matchesRestoredLayers();
            result["outerChildId"] = outer.childCompId.toString();
            result["innerChildId"] = inner.childCompId.toString();
            result["dissolved"] = dissolved;
            result["nestingRemoved"] = nestingRemoved;
            result["restoredTiming"] = restoredTiming;
            result["undoRestoredNesting"] = undoRestoredNesting;
            result["redoRemovedNesting"] = redoRemovedNesting;
            result["passed"] = dissolved && nestingRemoved && restoredTiming &&
                undoRestoredNesting && redoRemovedNesting;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Nested unpack changed layer timing or runtime nesting across undo/redo");
            service->changeCurrentComposition(rootId);
            childId_ = outer.childCompId;
            hasChild_ = true;
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.nonContiguousSelectionOrder") {
            QJsonObject result;
            if (!comp) {
                result["error"] = QStringLiteral("No current composition");
                return result;
            }
            const auto originalState = layers(comp, 0);
            LayerID aId;
            LayerID untouchedId;
            for (const auto& layer : comp->allLayer()) {
                if (!layer) continue;
                if (layer->layerName() == QStringLiteral("A")) aId = layer->id();
                if (layer->layerName() == QStringLiteral("Untouched")) untouchedId = layer->id();
            }
            if (aId.isNil() || untouchedId.isNil()) {
                result["error"] = QStringLiteral("Noncontiguous fixture layers are missing");
                return result;
            }
            const auto sourceA = comp->layerById(aId);
            const auto sourceUntouched = comp->layerById(untouchedId);
            const auto positionA = sourceA
                ? sourceA->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const auto opacityUntouched = sourceUntouched
                ? sourceUntouched->getProperty(QStringLiteral("layer.opacity"))
                : SharedPtr<AbstractProperty>{};
            if (!sourceA || !sourceUntouched || !positionA || !opacityUntouched) {
                result["error"] = QStringLiteral("Noncontiguous animation fixture properties are unavailable");
                return result;
            }
            positionA->clearKeyFrames();
            positionA->setAnimatable(true);
            positionA->addKeyFrame(RationalTime(40, sourceA->keyframeTimeScale()), 4.0,
                                   InterpolationType::Bezier);
            positionA->addKeyFrame(RationalTime(100, sourceA->keyframeTimeScale()), 16.0,
                                   InterpolationType::Constant);
            opacityUntouched->clearKeyFrames();
            opacityUntouched->setAnimatable(true);
            opacityUntouched->addKeyFrame(RationalTime(30, sourceUntouched->keyframeTimeScale()),
                                          0.25, InterpolationType::Smooth);
            opacityUntouched->addKeyFrame(RationalTime(90, sourceUntouched->keyframeTimeScale()),
                                          0.85, InterpolationType::Linear);
            const auto originalPositionKeys = keyframes(positionA);
            const auto originalOpacityKeys = keyframes(opacityUntouched);
            const double originalPositionSample = positionA->evaluateValue(
                RationalTime(70, sourceA->keyframeTimeScale())).toDouble();
            const double originalOpacitySample = opacityUntouched->evaluateValue(
                RationalTime(60, sourceUntouched->keyframeTimeScale())).toDouble();
            const auto keysShifted = [](const QJsonArray& actual, const QJsonArray& expected,
                                        int64_t offset) {
                if (actual.size() != expected.size()) return false;
                for (qsizetype index = 0; index < actual.size(); ++index) {
                    auto expectedKey = expected[index].toObject();
                    expectedKey["timeValue"] = expectedKey["timeValue"].toInt() +
                        static_cast<int>(offset);
                    if (actual[index].toObject() != expectedKey) return false;
                }
                return true;
            };
            const bool precomposed = service->precomposeLayersWithUndo(
                    {untouchedId, aId}, UniString(QStringLiteral("Noncontiguous Selection")),
                    false, false, PrecomposeMode::MoveSelected);
            if (!precomposed) {
                result["error"] = QStringLiteral("Could not precompose the noncontiguous fixture selection");
                return result;
            }
            const auto outcome = service->lastPrecomposeOutcome();
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            const auto childA = child ? child->layerById(aId) : ArtifactAbstractLayerPtr{};
            const auto childUntouched = child ? child->layerById(untouchedId) : ArtifactAbstractLayerPtr{};
            const auto childPositionA = childA
                ? childA->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const auto childOpacityUntouched = childUntouched
                ? childUntouched->getProperty(QStringLiteral("layer.opacity"))
                : SharedPtr<AbstractProperty>{};
            const bool childOrderPreserved = child && child->allLayer().size() == 2 &&
                child->allLayer()[0]->id() == aId && child->allLayer()[1]->id() == untouchedId;
            const bool childKeysRetimed = childPositionA && childOpacityUntouched &&
                keysShifted(keyframes(childPositionA), originalPositionKeys, -25) &&
                keysShifted(keyframes(childOpacityUntouched), originalOpacityKeys, -25);
            const bool childSamplesPreserved = childPositionA && childOpacityUntouched &&
                std::abs(childPositionA->evaluateValue(RationalTime(
                    45, childA->keyframeTimeScale())).toDouble() - originalPositionSample) < 1.0e-6 &&
                std::abs(childOpacityUntouched->evaluateValue(RationalTime(
                    35, childUntouched->keyframeTimeScale())).toDouble() - originalOpacitySample) < 1.0e-6;
            const auto precomposedState = layers(comp, 0);
            const bool unpacked = service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const bool originalOrderRestored = unpacked && layers(comp, 0) == originalState;
            const auto unpackedA = comp->layerById(aId);
            const auto unpackedUntouched = comp->layerById(untouchedId);
            const auto unpackedPositionA = unpackedA
                ? unpackedA->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const auto unpackedOpacityUntouched = unpackedUntouched
                ? unpackedUntouched->getProperty(QStringLiteral("layer.opacity"))
                : SharedPtr<AbstractProperty>{};
            const bool unpackedKeysRestored = unpackedPositionA && unpackedOpacityUntouched &&
                keyframes(unpackedPositionA) == originalPositionKeys &&
                keyframes(unpackedOpacityUntouched) == originalOpacityKeys;
            const bool unpackedSamplesPreserved = unpackedPositionA && unpackedOpacityUntouched &&
                std::abs(unpackedPositionA->evaluateValue(RationalTime(
                    70, unpackedA->keyframeTimeScale())).toDouble() - originalPositionSample) < 1.0e-6 &&
                std::abs(unpackedOpacityUntouched->evaluateValue(RationalTime(
                    60, unpackedUntouched->keyframeTimeScale())).toDouble() - originalOpacitySample) < 1.0e-6;
            const bool undoAvailable = undo->canUndo();
            if (undoAvailable) undo->undo();
            const bool undoRestoredPrecompose = undoAvailable && layers(comp, 0) == precomposedState;
            const auto undoChildPosition = childA
                ? childA->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const auto undoChildOpacity = childUntouched
                ? childUntouched->getProperty(QStringLiteral("layer.opacity"))
                : SharedPtr<AbstractProperty>{};
            const bool undoAnimationPreserved = undoAvailable && undoChildPosition && undoChildOpacity &&
                keysShifted(keyframes(undoChildPosition), originalPositionKeys, -25) &&
                keysShifted(keyframes(undoChildOpacity), originalOpacityKeys, -25) &&
                std::abs(undoChildPosition->evaluateValue(RationalTime(
                    45, childA->keyframeTimeScale())).toDouble() - originalPositionSample) < 1.0e-6 &&
                std::abs(undoChildOpacity->evaluateValue(RationalTime(
                    35, childUntouched->keyframeTimeScale())).toDouble() - originalOpacitySample) < 1.0e-6;
            const bool redoAvailable = undo->canRedo();
            if (redoAvailable) undo->redo();
            const bool redoRestoredOriginalOrder = redoAvailable && layers(comp, 0) == originalState;
            const auto redoA = comp->layerById(aId);
            const auto redoUntouched = comp->layerById(untouchedId);
            const auto redoPositionA = redoA
                ? redoA->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const auto redoOpacityUntouched = redoUntouched
                ? redoUntouched->getProperty(QStringLiteral("layer.opacity"))
                : SharedPtr<AbstractProperty>{};
            const bool redoAnimationRestored = redoRestoredOriginalOrder && redoPositionA &&
                redoOpacityUntouched && keyframes(redoPositionA) == originalPositionKeys &&
                keyframes(redoOpacityUntouched) == originalOpacityKeys &&
                std::abs(redoPositionA->evaluateValue(RationalTime(
                    70, redoA->keyframeTimeScale())).toDouble() - originalPositionSample) < 1.0e-6 &&
                std::abs(redoOpacityUntouched->evaluateValue(RationalTime(
                    60, redoUntouched->keyframeTimeScale())).toDouble() - originalOpacitySample) < 1.0e-6;
            result["childOrderPreserved"] = childOrderPreserved;
            result["childKeysRetimed"] = childKeysRetimed;
            result["childSamplesPreserved"] = childSamplesPreserved;
            result["originalOrderRestored"] = originalOrderRestored;
            result["unpackedKeysRestored"] = unpackedKeysRestored;
            result["unpackedSamplesPreserved"] = unpackedSamplesPreserved;
            result["undoRestoredPrecompose"] = undoRestoredPrecompose;
            result["undoAnimationPreserved"] = undoAnimationPreserved;
            result["redoRestoredOriginalOrder"] = redoRestoredOriginalOrder;
            result["redoAnimationRestored"] = redoAnimationRestored;
            result["passed"] = childOrderPreserved && childKeysRetimed && childSamplesPreserved &&
                originalOrderRestored && unpackedKeysRestored && unpackedSamplesPreserved &&
                undoRestoredPrecompose && undoAnimationPreserved && redoRestoredOriginalOrder &&
                redoAnimationRestored;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Noncontiguous precompose/unpack changed animation or layer ordering");
            childId_ = outcome.childCompId;
            hasChild_ = true;
            refresh();
            result["state"] = state();
            return result;
        } else if (name == "test.nestedSaveReload") {
            QJsonObject roundTrip;
            auto& projects = ArtifactProjectManager::getInstance();
            auto& manager = PreComposeManager::instance();
            const auto root = service->currentComposition().lock();
            if (!root) {
                roundTrip["error"] = QStringLiteral("No root composition");
                return roundTrip;
            }
            const auto rootId = root->id();
            const auto outerIds = namedLayerIds(root, {QStringLiteral("A"), QStringLiteral("B")});
            if (outerIds.size() != 2 || !service->precomposeLayersWithUndo(
                    outerIds, UniString(QStringLiteral("Round Trip Outer")), false, false,
                    PrecomposeMode::MoveSelected)) {
                roundTrip["error"] = QStringLiteral("Could not create outer round-trip composition");
                return roundTrip;
            }
            const auto outer = service->lastPrecomposeOutcome();
            service->changeCurrentComposition(outer.childCompId);
            const auto middle = service->currentComposition().lock();
            const auto innerIds = namedLayerIds(middle, {QStringLiteral("A"), QStringLiteral("B")});
            if (!middle || innerIds.size() != 2 || !service->precomposeLayersWithUndo(
                    innerIds, UniString(QStringLiteral("Round Trip Inner")), false, false,
                    PrecomposeMode::MoveSelected)) {
                service->changeCurrentComposition(rootId);
                roundTrip["error"] = QStringLiteral("Could not create inner round-trip composition");
                return roundTrip;
            }
            const auto inner = service->lastPrecomposeOutcome();
            service->changeCurrentComposition(rootId);
            const auto expectedTree = layers(root, 0);
            QTemporaryDir tempDir;
            if (!tempDir.isValid()) {
                roundTrip["error"] = QStringLiteral("Could not create temporary round-trip directory");
                return roundTrip;
            }
            const auto filePath = tempDir.filePath(QStringLiteral("nested-precompose-roundtrip.artifact"));
            const auto save = projects.saveToFile(filePath);
            if (!save.success) {
                roundTrip["error"] = save.errorMessage;
                return roundTrip;
            }
            const bool innerMappingCleared = manager.unregisterPrecompLayer(
                middle->id(), inner.precompLayerId, inner.childCompId);
            const bool outerMappingCleared = manager.unregisterPrecompLayer(
                rootId, outer.precompLayerId, outer.childCompId);
            const bool allMappingsCleared = innerMappingCleared && outerMappingCleared &&
                !manager.isPrecomposeLayer(inner.precompLayerId) &&
                !manager.isPrecomposeLayer(outer.precompLayerId) &&
                manager.getPrecompLayersForChild(outer.childCompId).isEmpty() &&
                manager.getPrecompLayersForChild(inner.childCompId).isEmpty();
            roundTrip["allMappingsClearedBeforeReload"] = allMappingsCleared;
            if (!allMappingsCleared) {
                roundTrip["error"] = QStringLiteral("Could not isolate both runtime nesting registrations");
                return roundTrip;
            }
            const bool loaded = projects.loadFromFile(filePath);
            roundTrip["loaded"] = loaded;
            if (!loaded) {
                roundTrip["error"] = QStringLiteral("Could not reload nested composition project");
                return roundTrip;
            }
            service->changeCurrentComposition(rootId);
            const auto reloadedRoot = service->findComposition(rootId).ptr.lock();
            const auto reloadedMiddle = service->findComposition(outer.childCompId).ptr.lock();
            const auto reloadedGrandchild = service->findComposition(inner.childCompId).ptr.lock();
            bool outerReverseReference = false;
            for (const auto& reference : manager.getPrecompLayersForChild(outer.childCompId)) {
                outerReverseReference = outerReverseReference ||
                    (reference.parentCompId == rootId &&
                     reference.precompLayerId == outer.precompLayerId);
            }
            bool innerReverseReference = false;
            for (const auto& reference : manager.getPrecompLayersForChild(inner.childCompId)) {
                innerReverseReference = innerReverseReference ||
                    (reference.parentCompId == outer.childCompId &&
                     reference.precompLayerId == inner.precompLayerId);
            }
            const auto middleNesting = manager.getCompositionNesting(outer.childCompId);
            const auto grandchildNesting = manager.getCompositionNesting(inner.childCompId);
            const auto hierarchy = manager.getCompositionHierarchy(inner.childCompId);
            const bool runtimeHierarchyRestored = reloadedMiddle && reloadedGrandchild &&
                manager.isPrecomposeLayer(outer.precompLayerId) &&
                manager.getSourceCompositionId(outer.precompLayerId) == outer.childCompId &&
                manager.isPrecomposeLayer(inner.precompLayerId) &&
                manager.getSourceCompositionId(inner.precompLayerId) == inner.childCompId &&
                outerReverseReference && innerReverseReference &&
                middleNesting.parentCompositionId == rootId &&
                middleNesting.parentLayerId == outer.precompLayerId &&
                grandchildNesting.parentCompositionId == outer.childCompId &&
                grandchildNesting.parentLayerId == inner.precompLayerId &&
                hierarchy.size() == 3 && hierarchy[0] == rootId &&
                hierarchy[1] == outer.childCompId && hierarchy[2] == inner.childCompId;
            const auto reloadedTree = reloadedRoot ? layers(reloadedRoot, 0) : QJsonArray{};
            const bool hierarchyDataPreserved = reloadedRoot && reloadedMiddle &&
                reloadedGrandchild && reloadedTree == expectedTree;
            const double timeInRoot = NestedTimeUtils::convertTime(5.0, inner.childCompId, rootId);
            const double timeRoundTrip = NestedTimeUtils::convertTime(
                timeInRoot, rootId, inner.childCompId);
            const bool timeConversionRestored = timeInRoot == 45.0 && timeRoundTrip == 5.0;
            const auto reloadedOuterWrapper = reloadedRoot
                ? dynamicPointerCast<ArtifactCompositionLayer>(reloadedRoot->layerById(outer.precompLayerId))
                : SharedPtr<ArtifactCompositionLayer>{};
            const auto reloadedInnerWrapper = reloadedMiddle
                ? dynamicPointerCast<ArtifactCompositionLayer>(reloadedMiddle->layerById(inner.precompLayerId))
                : SharedPtr<ArtifactCompositionLayer>{};
            bool nestedDurationShrinkPropagated = false;
            bool nestedDurationExpansionPropagated = false;
            if (reloadedRoot && reloadedMiddle && reloadedGrandchild &&
                reloadedOuterWrapper && reloadedInnerWrapper) {
                const auto outerIn = reloadedOuterWrapper->inPoint().framePosition();
                const auto outerStart = reloadedOuterWrapper->startTime().framePosition();
                const auto innerIn = reloadedInnerWrapper->inPoint().framePosition();
                const auto innerStart = reloadedInnerWrapper->startTime().framePosition();
                reloadedRoot->setFrameRange(FrameRange(0, 180));
                nestedDurationShrinkPropagated = reloadedRoot->frameRange().duration() == 180 &&
                    reloadedMiddle->frameRange().duration() == 180 &&
                    reloadedGrandchild->frameRange().duration() == 180 &&
                    reloadedOuterWrapper->inPoint().framePosition() == outerIn &&
                    reloadedOuterWrapper->startTime().framePosition() == outerStart &&
                    reloadedOuterWrapper->outPoint().framePosition() == outerIn + 180 &&
                    reloadedInnerWrapper->inPoint().framePosition() == innerIn &&
                    reloadedInnerWrapper->startTime().framePosition() == innerStart &&
                    reloadedInnerWrapper->outPoint().framePosition() == innerIn + 180;
                reloadedRoot->setFrameRange(FrameRange(0, 300));
                nestedDurationExpansionPropagated = reloadedRoot->frameRange().duration() == 300 &&
                    reloadedMiddle->frameRange().duration() == 300 &&
                    reloadedGrandchild->frameRange().duration() == 300 &&
                    reloadedOuterWrapper->inPoint().framePosition() == outerIn &&
                    reloadedOuterWrapper->startTime().framePosition() == outerStart &&
                    reloadedOuterWrapper->outPoint().framePosition() == outerIn + 300 &&
                    reloadedInnerWrapper->inPoint().framePosition() == innerIn &&
                    reloadedInnerWrapper->startTime().framePosition() == innerStart &&
                    reloadedInnerWrapper->outPoint().framePosition() == innerIn + 300;
            }
            const auto middleContainsUnpackedLayers = [&]() {
                if (!reloadedMiddle) return false;
                const auto a = reloadedMiddle->layerById(innerIds.front());
                const auto b = reloadedMiddle->layerById(innerIds.back());
                return a && b && a->layerName() == QStringLiteral("A") &&
                    b->layerName() == QStringLiteral("B") &&
                    a->inPoint().framePosition() == 0 && a->outPoint().framePosition() == 160 &&
                    a->startTime().framePosition() == 7 &&
                    b->inPoint().framePosition() == 30 && b->outPoint().framePosition() == 120 &&
                    b->startTime().framePosition() == 9;
            };
            service->changeCurrentComposition(outer.childCompId);
            const bool nestedUnpackedAfterReload = runtimeHierarchyRestored &&
                service->unprecomposeLayerWithUndo(inner.precompLayerId, true) &&
                middleContainsUnpackedLayers() && !reloadedMiddle->layerById(inner.precompLayerId) &&
                reloadedGrandchild->allLayer().empty() &&
                manager.getPrecompLayersForChild(inner.childCompId).isEmpty() &&
                manager.getPrecompLayersForChild(outer.childCompId).size() == 1;
            const bool nestedUnpackUndoAvailable = nestedUnpackedAfterReload && undo->canUndo();
            if (nestedUnpackUndoAvailable) undo->undo();
            const bool nestedUnpackUndoRestored = nestedUnpackUndoAvailable &&
                manager.isPrecomposeLayer(inner.precompLayerId) &&
                reloadedMiddle->layerById(inner.precompLayerId) &&
                reloadedGrandchild->layerById(innerIds.front()) &&
                manager.getCompositionHierarchy(inner.childCompId).size() == 3;
            const bool nestedUnpackRedoAvailable = nestedUnpackUndoRestored && undo->canRedo();
            if (nestedUnpackRedoAvailable) undo->redo();
            const bool nestedUnpackRedoRestored = nestedUnpackRedoAvailable &&
                !manager.isPrecomposeLayer(inner.precompLayerId) &&
                middleContainsUnpackedLayers() && !reloadedMiddle->layerById(inner.precompLayerId) &&
                reloadedGrandchild->allLayer().empty() &&
                manager.getPrecompLayersForChild(inner.childCompId).isEmpty();
            roundTrip["hierarchyDataPreserved"] = hierarchyDataPreserved;
            roundTrip["runtimeHierarchyRestored"] = runtimeHierarchyRestored;
            roundTrip["timeConversionRestored"] = timeConversionRestored;
            roundTrip["nestedDurationShrinkPropagated"] = nestedDurationShrinkPropagated;
            roundTrip["nestedDurationExpansionPropagated"] = nestedDurationExpansionPropagated;
            roundTrip["nestedUnpackedAfterReload"] = nestedUnpackedAfterReload;
            roundTrip["nestedUnpackUndoRestored"] = nestedUnpackUndoRestored;
            roundTrip["nestedUnpackRedoRestored"] = nestedUnpackRedoRestored;
            roundTrip["timeInRoot"] = timeInRoot;
            roundTrip["passed"] = hierarchyDataPreserved && runtimeHierarchyRestored &&
                timeConversionRestored && nestedUnpackedAfterReload && nestedUnpackUndoRestored &&
                nestedUnpackRedoRestored && nestedDurationShrinkPropagated &&
                nestedDurationExpansionPropagated;
            if (!roundTrip["passed"].toBool())
                roundTrip["error"] = QStringLiteral("Nested save/reload lost composition data or runtime hierarchy");
            childId_ = inner.childCompId;
            hasChild_ = true;
            service->changeCurrentComposition(rootId);
            refresh();
            roundTrip["state"] = state();
            return roundTrip;
        } else if (name == "test.saveReload") {
            QJsonObject roundTrip;
            auto& projects = ArtifactProjectManager::getInstance();
            const auto parent = service->currentComposition().lock();
            const auto originalParentId = parent ? parent->id() : CompositionID{};
            const auto originalChildId = childId_;
            SharedPtr<ArtifactCompositionLayer> originalPrecompLayer;
            if (parent) for (const auto& layer : parent->allLayer()) {
                auto candidate = dynamicPointerCast<ArtifactCompositionLayer>(layer);
                if (candidate && candidate->sourceCompositionId() == originalChildId) {
                    originalPrecompLayer = candidate;
                    break;
                }
            }
            QTemporaryDir tempDir;
            if (!parent || originalChildId.isNil() || !originalPrecompLayer || !tempDir.isValid()) {
                roundTrip["error"] = QStringLiteral("Precompose fixture or temporary directory unavailable");
                return roundTrip;
            }
            const auto originalChild = service->findComposition(originalChildId).ptr.lock();
            const auto sourceIds = namedLayerIds(originalChild, {QStringLiteral("A")});
            const auto sourceLayer = sourceIds.isEmpty()
                ? ArtifactAbstractLayerPtr{} : originalChild->layerById(sourceIds.front());
            const auto peerIds = namedLayerIds(originalChild, {QStringLiteral("B")});
            const auto peerLayer = peerIds.isEmpty()
                ? ArtifactAbstractLayerPtr{} : originalChild->layerById(peerIds.front());
            const auto untouchedChildIds = namedLayerIds(originalChild, {QStringLiteral("Untouched")});
            const bool untouchedIsChild = !untouchedChildIds.isEmpty();
            const auto untouchedParentIds = untouchedIsChild
                ? QVector<LayerID>{} : namedLayerIds(parent, {QStringLiteral("Untouched")});
            const auto untouchedLayer = untouchedIsChild
                ? originalChild->layerById(untouchedChildIds.front())
                : (untouchedParentIds.isEmpty() ? ArtifactAbstractLayerPtr{}
                                                : parent->layerById(untouchedParentIds.front()));
            if (!sourceLayer || !peerLayer || !untouchedLayer) {
                roundTrip["error"] = QStringLiteral("Precompose layers unavailable for metadata persistence");
                return roundTrip;
            }
            const auto parentEffectCountBefore = parent->getEffects().size();
            const auto persistedParentEffect = makeShared<BrightnessEffect>();
            persistedParentEffect->setBrightness(-0.21875f);
            parent->addEffect(persistedParentEffect);
            sourceLayer->setLayerNote(QStringLiteral("Persisted A / 注記"));
            sourceLayer->setVisible(false);
            sourceLayer->setLocked(true);
            sourceLayer->setSolo(true);
            sourceLayer->setBlendMode(LAYER_BLEND_TYPE::BLEND_SCREEN);
            sourceLayer->setRotation3D(QVector3D(13.0f, -27.0f, 41.0f));
            sourceLayer->transform3D().setCurrentScale(1.25f, 0.8f);
            sourceLayer->transform3D().setCurrentAnchor(7.0f, -5.0f, 3.0f);
            const auto persistedEffect = makeShared<BrightnessEffect>();
            persistedEffect->setBrightness(0.3125f);
            sourceLayer->addEffect(persistedEffect);
            LayerMask persistedMask;
            persistedMask.setEnabled(false);
            persistedMask.setLocked(true);
            persistedMask.setColor(FloatColor(0.25f, 0.5f, 0.75f, 1.0f));
            MaskPath persistedMaskPath;
            persistedMaskPath.setName(UniString(QStringLiteral("Saved Cutout")));
            persistedMaskPath.setClosed(true);
            persistedMaskPath.setOpacity(0.625f);
            persistedMaskPath.setMode(MaskMode::Difference);
            persistedMaskPath.addVertex(
                {QPointF(5.0, 9.0), QPointF(-1.0, 0.5), QPointF(2.0, -0.5)});
            persistedMaskPath.addVertex(
                {QPointF(25.0, 29.0), QPointF(-2.0, 1.0), QPointF(1.0, 2.0)});
            MaskPathKeyframeSnapshot persistedMaskKey;
            persistedMaskKey.vertices = {
                {QPointF(7.0, 11.0), QPointF(-1.5, 0.5), QPointF(2.5, -0.5)},
                {QPointF(28.0, 31.0), QPointF(-2.5, 1.0), QPointF(1.5, 2.0)}};
            persistedMaskKey.closed = false;
            persistedMaskKey.opacity = 0.375f;
            persistedMaskKey.feather = 2.5f;
            persistedMaskKey.falloff = MaskFeatherFalloff::Smooth;
            persistedMaskKey.mode = MaskMode::Subtract;
            persistedMaskKey.name = UniString(QStringLiteral("Saved Cutout Key"));
            persistedMaskPath.setAnimationKeyframe(24, persistedMaskKey);
            persistedMask.addMaskPath(persistedMaskPath);
            sourceLayer->addMask(persistedMask);
            LayerMatteReference persistedMatte;
            persistedMatte.sourceLayerId = Id(peerLayer->id().toString());
            persistedMatte.sourceAssetPath = QStringLiteral("asset://saved-matte");
            persistedMatte.enabled = true;
            persistedMatte.type = MatteType::Luma;
            persistedMatte.blendMode = MatteBlendMode::Intersect;
            persistedMatte.fitMode = MatteFitMode::Fit;
            persistedMatte.opacity = 0.75f;
            persistedMatte.invert = true;
            sourceLayer->setMatteReferences({persistedMatte});
            (void)sourceLayer->layerComponents();
            auto expectedComponentSnapshot = sourceLayer->componentDescriptorSnapshot();
            QJsonArray persistedComponentGraph =
                expectedComponentSnapshot.value(QStringLiteral("componentGraph")).toArray();
            persistedComponentGraph.append(QJsonObject{
                {QStringLiteral("componentId"), QStringLiteral("test.precompose.persisted")},
                {QStringLiteral("typeId"), QStringLiteral("artifact.test.persisted.scope")},
                {QStringLiteral("version"), 2},
                {QStringLiteral("enabled"), true},
                {QStringLiteral("phase"), QStringLiteral("render")},
                {QStringLiteral("scope"), QStringLiteral("layer")},
                {QStringLiteral("order"), 7},
                {QStringLiteral("requires"), QJsonArray{}},
                {QStringLiteral("settings"), QJsonObject{
                     {QStringLiteral("mode"), QStringLiteral("persistent")},
                     {QStringLiteral("weight"), 0.5}}}});
            expectedComponentSnapshot.insert(QStringLiteral("componentGraph"), persistedComponentGraph);
            const bool componentSnapshotAuthored =
                sourceLayer->restoreComponentDescriptorSnapshot(expectedComponentSnapshot) &&
                sourceLayer->componentDescriptorSnapshot() == expectedComponentSnapshot;
            if (!componentSnapshotAuthored) {
                roundTrip["error"] = QStringLiteral("Could not prepare component descriptor save/reload fixture");
                return roundTrip;
            }
            peerLayer->setLayerNote(QStringLiteral("Persisted B\nsecond line"));
            peerLayer->setVisible(true);
            peerLayer->setLocked(false);
            peerLayer->setSolo(false);
            peerLayer->setBlendMode(LAYER_BLEND_TYPE::BLEND_MULTIPLY);
            peerLayer->setRotation3D(QVector3D(-9.0f, 22.0f, -35.0f));
            peerLayer->transform3D().setCurrentScale(0.7f, 1.6f);
            peerLayer->transform3D().setCurrentAnchor(-2.0f, 8.0f, -4.0f);
            untouchedLayer->setLayerNote(QStringLiteral("Parent note survives reload"));
            const QStringList animatedPropertyPaths{
                QStringLiteral("transform.position.x"),
                QStringLiteral("transform.position.y"),
                QStringLiteral("transform.position.z"),
                QStringLiteral("transform.rotation.x"),
                QStringLiteral("transform.rotation.y"),
                QStringLiteral("transform.rotation.z"),
                QStringLiteral("transform.scale.x"),
                QStringLiteral("transform.scale.y"),
                QStringLiteral("transform.anchor.x"),
                QStringLiteral("transform.anchor.y"),
                QStringLiteral("transform.anchor.z"),
                QStringLiteral("layer.opacity")};
            QVector<QJsonArray> sourceTransformKeys;
            QVector<QJsonArray> peerTransformKeys;
            QVector<double> sourceAnimationSamples;
            QVector<double> peerAnimationSamples;
            for (qsizetype index = 0; index < animatedPropertyPaths.size(); ++index) {
                const auto sourceChannel = sourceLayer->getProperty(animatedPropertyPaths[index]);
                const auto peerChannel = peerLayer->getProperty(animatedPropertyPaths[index]);
                if (!sourceChannel || !peerChannel) {
                    roundTrip["error"] = QStringLiteral("Animation channel unavailable for save/reload");
                    return roundTrip;
                }
                sourceChannel->clearKeyFrames();
                sourceChannel->setAnimatable(true);
                sourceChannel->addKeyFrame(
                    RationalTime(12, sourceLayer->keyframeTimeScale()),
                    5.0 + static_cast<double>(index), InterpolationType::Bezier,
                    0.1f, 0.2f, 0.8f, 0.9f, true);
                sourceChannel->addKeyFrame(
                    RationalTime(36, sourceLayer->keyframeTimeScale()),
                    15.0 + static_cast<double>(index), InterpolationType::Constant);
                peerChannel->clearKeyFrames();
                peerChannel->setAnimatable(true);
                peerChannel->addKeyFrame(
                    RationalTime(18, peerLayer->keyframeTimeScale()),
                    25.0 + static_cast<double>(index), InterpolationType::Smooth);
                peerChannel->addKeyFrame(
                    RationalTime(42, peerLayer->keyframeTimeScale()),
                    35.0 + static_cast<double>(index), InterpolationType::Linear);
                sourceTransformKeys.append(keyframes(sourceChannel));
                peerTransformKeys.append(keyframes(peerChannel));
                sourceAnimationSamples.append(sourceChannel->evaluateValue(
                    RationalTime(24, sourceLayer->keyframeTimeScale())).toDouble());
                peerAnimationSamples.append(peerChannel->evaluateValue(
                    RationalTime(30, peerLayer->keyframeTimeScale())).toDouble());
            }
            ArtifactCore::ExposedPropertyRegistry exposedProperties;
            ArtifactCore::ExposedPropertyBinding opacityBinding;
            opacityBinding.id = QStringLiteral("roundtrip.opacity");
            opacityBinding.label = QStringLiteral("Round-trip Opacity");
            opacityBinding.targetLayerId = sourceLayer->id().toString();
            opacityBinding.internalPath = QStringLiteral("layer.opacity");
            opacityBinding.defaultValue = sourceLayer->opacity();
            const bool definitionAdded = exposedProperties.add(opacityBinding);
            originalPrecompLayer->setExposedProperties(exposedProperties);
            const float sourceOpacityBeforeSave = sourceLayer->opacity();
            const float overrideValue = 0.67f;
            const bool overrideSet = definitionAdded && originalPrecompLayer->setExposedPropertyOverride(
                opacityBinding.id,
                QVariant::fromValue(static_cast<double>(overrideValue)));
            const auto original = layers(parent, 0);
            if (!overrideSet || !originalPrecompLayer->hasExposedPropertyOverride(opacityBinding.id)) {
                roundTrip["error"] = QStringLiteral("Could not prepare exposed-property persistence fixture");
                return roundTrip;
            }
            const auto filePath = tempDir.filePath(QStringLiteral("precompose-roundtrip.artifact"));
            const auto save = projects.saveToFile(filePath);
            roundTrip["saved"] = save.success;
            if (!save.success) {
                roundTrip["error"] = save.errorMessage;
                return roundTrip;
            }
            auto& precompManager = PreComposeManager::instance();
            const bool mappingCleared = precompManager.unregisterPrecompLayer(
                originalParentId, originalPrecompLayer->id(), originalChildId) &&
                !precompManager.isPrecomposeLayer(originalPrecompLayer->id()) &&
                precompManager.getParentComposition(originalChildId).isNil();
            roundTrip["mappingClearedBeforeReload"] = mappingCleared;
            if (!mappingCleared) {
                roundTrip["error"] = QStringLiteral("Could not isolate runtime precompose registration before reload");
                return roundTrip;
            }
            const bool loaded = projects.loadFromFile(filePath);
            roundTrip["loaded"] = loaded;
            if (loaded) service->changeCurrentComposition(originalParentId);
            const auto reloadedParent = service->findComposition(originalParentId).ptr.lock();
            const auto reloadedChild = service->findComposition(originalChildId).ptr.lock();
            const auto reloaded = reloadedParent ? layers(reloadedParent, 0) : QJsonArray{};
            const auto reloadedPrecompLayer = reloadedParent
                ? dynamicPointerCast<ArtifactCompositionLayer>(
                      reloadedParent->layerById(originalPrecompLayer->id()))
                : SharedPtr<ArtifactCompositionLayer>{};
            const auto reloadedSourceLayer = reloadedChild
                ? reloadedChild->layerById(sourceLayer->id()) : ArtifactAbstractLayerPtr{};
            const auto reloadedPeerLayer = reloadedChild
                ? reloadedChild->layerById(peerLayer->id()) : ArtifactAbstractLayerPtr{};
            const auto reloadedUntouchedHost = untouchedIsChild ? reloadedChild : reloadedParent;
            const auto reloadedUntouchedLayer = reloadedUntouchedHost
                ? reloadedUntouchedHost->layerById(untouchedLayer->id()) : ArtifactAbstractLayerPtr{};
            const auto layerChannelsMatch = [&](const ArtifactAbstractLayerPtr& layer,
                                                const QVector<QJsonArray>& expected) {
                if (!layer || expected.size() != animatedPropertyPaths.size()) return false;
                for (qsizetype index = 0; index < animatedPropertyPaths.size(); ++index) {
                    const auto property = layer->getProperty(animatedPropertyPaths[index]);
                    if (!property || keyframes(property) != expected[index]) return false;
                }
                return true;
            };
            const bool transformKeyframesPreserved =
                layerChannelsMatch(reloadedSourceLayer, sourceTransformKeys) &&
                layerChannelsMatch(reloadedPeerLayer, peerTransformKeys);
            const auto animationSamplesMatch = [&](const ArtifactAbstractLayerPtr& layer,
                                                   const QVector<double>& expected,
                                                   int sampleFrame) {
                if (!layer || expected.size() != animatedPropertyPaths.size()) return false;
                const RationalTime sampleTime(sampleFrame, layer->keyframeTimeScale());
                for (qsizetype index = 0; index < animatedPropertyPaths.size(); ++index) {
                    const auto property = layer->getProperty(animatedPropertyPaths[index]);
                    if (!property || std::abs(property->evaluateValue(sampleTime).toDouble() -
                                              expected[index]) > 1.0e-6) return false;
                }
                return true;
            };
            const bool animationSamplesPreserved =
                animationSamplesMatch(reloadedSourceLayer, sourceAnimationSamples, 24) &&
                animationSamplesMatch(reloadedPeerLayer, peerAnimationSamples, 30);
            const auto reloadedEffects = reloadedSourceLayer
                ? reloadedSourceLayer->getEffects()
                : std::vector<SharedPtr<ArtifactAbstractEffect>>{};
            const auto reloadedBrightness = reloadedEffects.size() == 1
                ? dynamicPointerCast<BrightnessEffect>(reloadedEffects.front())
                : SharedPtr<BrightnessEffect>{};
            const bool layerEffectPersisted = reloadedBrightness &&
                std::abs(reloadedBrightness->brightness() - 0.3125f) <= 1.0e-6f;
            bool layerMaskPersisted = false;
            if (reloadedSourceLayer && reloadedSourceLayer->maskCount() == 1) {
                const auto mask = reloadedSourceLayer->mask(0);
                if (mask.maskPathCount() == 1) {
                    const auto path = mask.maskPath(0);
                    const auto keys = path.animationKeyframes();
                    layerMaskPersisted = !mask.isEnabled() && mask.isLocked() &&
                        mask.color().approximatelyEqual(FloatColor(0.25f, 0.5f, 0.75f, 1.0f)) &&
                        path.name().toQString() == QStringLiteral("Saved Cutout") &&
                        path.isClosed() && path.mode() == MaskMode::Difference &&
                        std::abs(path.opacity() - 0.625f) <= 1.0e-6f &&
                        path.vertexCount() == 2 &&
                        path.vertex(0).position == QPointF(5.0, 9.0) &&
                        path.vertex(0).outTangent == QPointF(2.0, -0.5) &&
                        path.hasAnimationKeyframes() && keys.size() == 1 &&
                        keys[0].frame == 24 && keys[0].name.toQString() ==
                            QStringLiteral("Saved Cutout Key") && !keys[0].closed &&
                        std::abs(keys[0].opacity - 0.375f) <= 1.0e-6f &&
                        std::abs(keys[0].feather - 2.5f) <= 1.0e-6f &&
                        keys[0].falloff == MaskFeatherFalloff::Smooth &&
                        keys[0].mode == MaskMode::Subtract && keys[0].vertices.size() == 2 &&
                        keys[0].vertices[1].position == QPointF(28.0, 31.0) &&
                        keys[0].vertices[1].inTangent == QPointF(-2.5, 1.0);
                }
            }
            const auto reloadedMattes = reloadedSourceLayer
                ? reloadedSourceLayer->matteReferences()
                : std::vector<LayerMatteReference>{};
            const bool layerMattePersisted = reloadedMattes.size() == 1 &&
                reloadedMattes.front().id.toString() == persistedMatte.id.toString() &&
                reloadedMattes.front().sourceLayerId.toString() == peerLayer->id().toString() &&
                reloadedMattes.front().sourceAssetPath == persistedMatte.sourceAssetPath &&
                reloadedMattes.front().enabled &&
                reloadedMattes.front().type == MatteType::Luma &&
                reloadedMattes.front().blendMode == MatteBlendMode::Intersect &&
                reloadedMattes.front().fitMode == MatteFitMode::Fit &&
                std::abs(reloadedMattes.front().opacity - 0.75f) <= 1.0e-6f &&
                reloadedMattes.front().invert;
            const bool componentDescriptorsPersisted = reloadedSourceLayer &&
                reloadedSourceLayer->componentDescriptorSnapshot() == expectedComponentSnapshot;
            const auto reloadedParentEffects = reloadedParent ? reloadedParent->getEffects()
                                                               : std::vector<SharedPtr<ArtifactAbstractEffect>>{};
            const auto reloadedParentBrightness = std::find_if(
                reloadedParentEffects.begin(), reloadedParentEffects.end(), [](const auto& effect) {
                    const auto brightness = dynamicPointerCast<BrightnessEffect>(effect);
                    return brightness && std::abs(brightness->brightness() + 0.21875f) <= 1.0e-6f;
                });
            const bool compositionEffectPersistedOnParent =
                reloadedParentEffects.size() == parentEffectCountBefore + 1 &&
                reloadedParentBrightness != reloadedParentEffects.end() && reloadedChild &&
                reloadedChild->effectCount() == 0;
            const auto hasPersistedAuxiliaryState = [&](const ArtifactAbstractLayerPtr& layer) {
                if (!layer || layer->componentDescriptorSnapshot() != expectedComponentSnapshot ||
                    layer->maskCount() != 1 || layer->matteReferences().size() != 1) return false;
                const auto effects = layer->getEffects();
                const auto brightness = effects.size() == 1
                    ? dynamicPointerCast<BrightnessEffect>(effects.front())
                    : SharedPtr<BrightnessEffect>{};
                const auto mask = layer->mask(0);
                const auto path = mask.maskPathCount() == 1 ? mask.maskPath(0) : MaskPath{};
                const auto keys = path.animationKeyframes();
                const auto mattes = layer->matteReferences();
                return brightness && std::abs(brightness->brightness() - 0.3125f) <= 1.0e-6f &&
                    !mask.isEnabled() && mask.isLocked() && path.name().toQString() ==
                        QStringLiteral("Saved Cutout") && keys.size() == 1 && keys.front().frame == 24 &&
                    mattes.front().sourceLayerId.toString() == peerLayer->id().toString() &&
                    mattes.front().type == MatteType::Luma &&
                    mattes.front().blendMode == MatteBlendMode::Intersect &&
                    std::abs(mattes.front().opacity - 0.75f) <= 1.0e-6f && mattes.front().invert;
            };
            const auto reloadedBinding = reloadedPrecompLayer
                ? reloadedPrecompLayer->exposedProperties().binding(opacityBinding.id)
                : ArtifactCore::ExposedPropertyBinding{};
            const bool masterPropertyDefinitionPreserved = reloadedPrecompLayer &&
                reloadedPrecompLayer->exposedProperties().contains(opacityBinding.id) &&
                reloadedBinding.targetLayerId == sourceLayer->id().toString() &&
                reloadedBinding.internalPath == QStringLiteral("layer.opacity") &&
                std::abs(reloadedBinding.defaultValue.toFloat() - sourceOpacityBeforeSave) < 1.0e-5f;
            const bool masterPropertyOverridePreserved = reloadedPrecompLayer &&
                reloadedPrecompLayer->hasExposedPropertyOverride(opacityBinding.id) &&
                std::abs(reloadedPrecompLayer->effectiveExposedPropertyValue(
                             opacityBinding.id).toFloat() - overrideValue) < 1.0e-5f;
            const bool overrideStillDeferred = reloadedSourceLayer &&
                std::abs(reloadedSourceLayer->opacity() - sourceOpacityBeforeSave) < 1.0e-5f;
            const auto sourceRotation = reloadedSourceLayer ? reloadedSourceLayer->rotation3D() : QVector3D{};
            const auto peerRotation = reloadedPeerLayer ? reloadedPeerLayer->rotation3D() : QVector3D{};
            const bool layerMetadataPreserved = reloadedSourceLayer && reloadedPeerLayer &&
                reloadedUntouchedLayer &&
                reloadedSourceLayer->layerNote() == QStringLiteral("Persisted A / 注記") &&
                !reloadedSourceLayer->isVisible() && reloadedSourceLayer->isLocked() &&
                reloadedSourceLayer->isSolo() &&
                reloadedSourceLayer->layerBlendType() == LAYER_BLEND_TYPE::BLEND_SCREEN &&
                std::abs(sourceRotation.x() - 13.0f) < 1.0e-5f &&
                std::abs(sourceRotation.y() - (-27.0f)) < 1.0e-5f &&
                std::abs(sourceRotation.z() - 41.0f) < 1.0e-5f &&
                std::abs(reloadedSourceLayer->transform3D().scaleX() - 1.25f) < 1.0e-5f &&
                std::abs(reloadedSourceLayer->transform3D().scaleY() - 0.8f) < 1.0e-5f &&
                std::abs(reloadedSourceLayer->transform3D().anchorX() - 7.0f) < 1.0e-5f &&
                std::abs(reloadedSourceLayer->transform3D().anchorY() - (-5.0f)) < 1.0e-5f &&
                std::abs(reloadedSourceLayer->transform3D().anchorZ() - 3.0f) < 1.0e-5f &&
                reloadedPeerLayer->layerNote() == QStringLiteral("Persisted B\nsecond line") &&
                reloadedPeerLayer->isVisible() && !reloadedPeerLayer->isLocked() &&
                !reloadedPeerLayer->isSolo() &&
                reloadedPeerLayer->layerBlendType() == LAYER_BLEND_TYPE::BLEND_MULTIPLY &&
                std::abs(peerRotation.x() - (-9.0f)) < 1.0e-5f &&
                std::abs(peerRotation.y() - 22.0f) < 1.0e-5f &&
                std::abs(peerRotation.z() - (-35.0f)) < 1.0e-5f &&
                std::abs(reloadedPeerLayer->transform3D().scaleX() - 0.7f) < 1.0e-5f &&
                std::abs(reloadedPeerLayer->transform3D().scaleY() - 1.6f) < 1.0e-5f &&
                std::abs(reloadedPeerLayer->transform3D().anchorX() - (-2.0f)) < 1.0e-5f &&
                std::abs(reloadedPeerLayer->transform3D().anchorY() - 8.0f) < 1.0e-5f &&
                std::abs(reloadedPeerLayer->transform3D().anchorZ() - (-4.0f)) < 1.0e-5f &&
                reloadedUntouchedLayer->layerNote() == QStringLiteral("Parent note survives reload");
            const bool parentChildLink = reloaded == original && bool(reloadedChild);
            const auto nesting = precompManager.getCompositionNesting(originalChildId);
            bool reverseParentLink = false;
            for (const auto& ref : precompManager.getPrecompLayersForChild(originalChildId))
                reverseParentLink = reverseParentLink ||
                    (ref.parentCompId == originalParentId &&
                     ref.precompLayerId == originalPrecompLayer->id());
            const bool runtimeNestingRestored =
                precompManager.isPrecomposeLayer(originalPrecompLayer->id()) &&
                precompManager.getSourceCompositionId(originalPrecompLayer->id()) == originalChildId &&
                nesting.parentCompositionId == originalParentId &&
                nesting.parentLayerId == originalPrecompLayer->id() &&
                precompManager.getCompositionHierarchy(originalChildId).size() == 2 &&
                reverseParentLink;
            const bool auxiliaryStateUnpacked = loaded && reloadedPrecompLayer &&
                service->unprecomposeLayerWithUndo(originalPrecompLayer->id(), true) &&
                hasPersistedAuxiliaryState(reloadedParent->layerById(sourceLayer->id()));
            const auto compositionEffectScopeMatches = [&]() {
                const auto parentEffects = reloadedParent->getEffects();
                const auto child = service->findComposition(originalChildId).ptr.lock();
                if (!child) return false;
                const auto childEffects = child->getEffects();
                const auto authoredEffect = std::find_if(parentEffects.begin(), parentEffects.end(),
                    [](const auto& effect) {
                        const auto brightness = dynamicPointerCast<BrightnessEffect>(effect);
                        return brightness && std::abs(brightness->brightness() + 0.21875f) <= 1.0e-6f;
                    });
                return parentEffects.size() == parentEffectCountBefore + 1 &&
                    authoredEffect != parentEffects.end() && childEffects.empty();
            };
            const bool compositionEffectScopeAfterUnpack = auxiliaryStateUnpacked &&
                compositionEffectScopeMatches();
            const bool unpackUndoAvailable = compositionEffectScopeAfterUnpack && undo->canUndo();
            if (unpackUndoAvailable) undo->undo();
            const auto undoChild = service->findComposition(originalChildId).ptr.lock();
            const bool unpackUndoRestored = unpackUndoAvailable &&
                hasPersistedAuxiliaryState(undoChild ? undoChild->layerById(sourceLayer->id())
                                                     : ArtifactAbstractLayerPtr{}) &&
                compositionEffectScopeMatches() &&
                precompManager.isPrecomposeLayer(originalPrecompLayer->id()) &&
                reloadedParent->layerById(originalPrecompLayer->id());
            const bool unpackRedoAvailable = unpackUndoRestored && undo->canRedo();
            if (unpackRedoAvailable) undo->redo();
            const bool unpackRedoRestored = unpackRedoAvailable &&
                hasPersistedAuxiliaryState(reloadedParent->layerById(sourceLayer->id())) &&
                compositionEffectScopeMatches() &&
                !precompManager.isPrecomposeLayer(originalPrecompLayer->id());
            roundTrip["parentChildLinkPreserved"] = parentChildLink;
            roundTrip["layerStatePreserved"] = reloaded == original;
            roundTrip["runtimeNestingRestored"] = runtimeNestingRestored;
            roundTrip["masterPropertyDefinitionPreserved"] = masterPropertyDefinitionPreserved;
            roundTrip["masterPropertyOverridePreserved"] = masterPropertyOverridePreserved;
            roundTrip["overrideStillDeferred"] = overrideStillDeferred;
            roundTrip["layerMetadataPreserved"] = layerMetadataPreserved;
            roundTrip["transformKeyframesPreserved"] = transformKeyframesPreserved;
            roundTrip["animationSamplesPreserved"] = animationSamplesPreserved;
            roundTrip["layerEffectPersisted"] = layerEffectPersisted;
            roundTrip["layerMaskPersisted"] = layerMaskPersisted;
            roundTrip["layerMattePersisted"] = layerMattePersisted;
            roundTrip["componentDescriptorsPersisted"] = componentDescriptorsPersisted;
            roundTrip["compositionEffectPersistedOnParent"] = compositionEffectPersistedOnParent;
            roundTrip["compositionEffectScopeAfterUnpack"] = compositionEffectScopeAfterUnpack;
            roundTrip["compositionEffectScopeAfterUndoRedo"] = unpackUndoRestored && unpackRedoRestored;
            roundTrip["auxiliaryStateUnpackedAfterReload"] = auxiliaryStateUnpacked;
            roundTrip["unpackUndoRestoredChild"] = unpackUndoRestored;
            roundTrip["unpackRedoRestoredParent"] = unpackRedoRestored;
            roundTrip["passed"] = save.success && loaded && parentChildLink && runtimeNestingRestored &&
                masterPropertyDefinitionPreserved && masterPropertyOverridePreserved && overrideStillDeferred &&
                layerMetadataPreserved && transformKeyframesPreserved && animationSamplesPreserved &&
                layerEffectPersisted &&
                layerMaskPersisted && layerMattePersisted && componentDescriptorsPersisted &&
                compositionEffectPersistedOnParent && auxiliaryStateUnpacked &&
                compositionEffectScopeAfterUnpack && unpackUndoRestored && unpackRedoRestored;
            if (!roundTrip["passed"].toBool())
                roundTrip["error"] = QStringLiteral("Precompose project round-trip lost parent/child data or runtime nesting registration");
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
            const bool moveAll = name == "test.precomposeAll";
            const QVector<LayerID> requestedIds = moveAll ? QVector<LayerID>()
                : (ids.size() == 2 ? QVector<LayerID>{ids[1], ids[0], ids[1]} : ids);
            ok = (moveAll || !ids.isEmpty()) && service->precomposeLayersWithUndo(requestedIds,
                UniString(QStringLiteral("Nested A+B")), false, false,
                moveAll ? PrecomposeMode::MoveAllAttributes : PrecomposeMode::MoveSelected);
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
        if (name == "test.precomposeAll") result["emptySelectionSubmitted"] = true;
        if (ok && (name == "test.precompose" || name == "test.precomposeAll")) {
            const auto child = service->findComposition(childId_).ptr.lock();
            const auto parentSize = comp ? comp->settings().compositionSize() : QSize();
            const auto childSize = child ? child->settings().compositionSize() : QSize();
            const auto parentColor = comp ? comp->backgroundColor() : FloatColor();
            const auto childColor = child ? child->backgroundColor() : FloatColor();
            const bool settingsCopied = comp && child &&
                parentSize == childSize &&
                comp->frameRate().framerate() == child->frameRate().framerate() &&
                parentColor.r() == childColor.r() && parentColor.g() == childColor.g() &&
                parentColor.b() == childColor.b() && parentColor.a() == childColor.a();
            result["childSettingsCopied"] = settingsCopied;
            result["childResolution"] = QJsonArray{childSize.width(), childSize.height()};
            result["childFrameRate"] = child ? child->frameRate().framerate() : -1.0;
            result["childFrameRangeDuration"] = child ? child->frameRange().duration() : -1;
            result["parentFrameRangeDuration"] = comp ? comp->frameRange().duration() : -1;
            result["childBackgroundColor"] = QJsonArray{childColor.r(), childColor.g(),
                                                         childColor.b(), childColor.a()};
        }
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
    static QJsonArray keyframes(const SharedPtr<AbstractProperty>& property)
    {
        QJsonArray result;
        if (!property) return result;
        for (const auto& key : property->getKeyFrames()) {
            result.append(QJsonObject{{"timeValue", static_cast<double>(key.time.value())},
                {"timeScale", static_cast<double>(key.time.scale())},
                {"value", key.value.toDouble()},
                {"interpolation", static_cast<int>(key.interpolation)},
                {"cp1x", static_cast<double>(key.cp1_x)},
                {"cp1y", static_cast<double>(key.cp1_y)},
                {"cp2x", static_cast<double>(key.cp2_x)},
                {"cp2y", static_cast<double>(key.cp2_y)},
                {"roving", key.roving}, {"anchor", static_cast<int>(key.anchor)},
                {"colorLabel", static_cast<int>(key.colorLabel)}, {"soft", key.soft}});
        }
        return result;
    }

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
            const auto rotation = layer->rotation3D();
            const auto& transform = layer->transform3D();
            QJsonObject entry{{"id", layer->id().toString()}, {"name", layer->layerName()},
                {"startFrame", double(layer->startTime().framePosition())},
                {"inFrame", double(layer->inPoint().framePosition())},
                {"outFrame", double(layer->outPoint().framePosition())},
                {"opacity", layer->opacity()},
                {"layerNote", layer->layerNote()},
                {"visible", layer->isVisible()},
                {"locked", layer->isLocked()},
                {"solo", layer->isSolo()},
                {"blendMode", static_cast<int>(layer->layerBlendType())},
                {"rotation", QJsonArray{rotation.x(), rotation.y(), rotation.z()}},
                {"scale", QJsonArray{transform.scaleX(), transform.scaleY(), transform.scaleZ()}},
                {"anchor", QJsonArray{transform.anchorX(), transform.anchorY(), transform.anchorZ()}},
                {"position", QJsonArray{position.x, position.y, position.z}}};
            auto compositionLayer = dynamicPointerCast<ArtifactCompositionLayer>(layer);
            const auto childId = compositionLayer ? compositionLayer->sourceCompositionId()
                                                  : CompositionID{};
            if (!childId.isNil()) {
                entry["childId"] = childId.toString();
                auto child = ArtifactProjectService::instance()->findComposition(childId).ptr.lock();
                entry["childExists"] = bool(child);
                if (child) {
                    entry["children"] = layers(child, depth + 1);
                    const auto childSize = child->settings().compositionSize();
                    const auto childColor = child->backgroundColor();
                    entry["childComposition"] = QJsonObject{
                        {"resolution", QJsonArray{childSize.width(), childSize.height()}},
                        {"frameRate", child->frameRate().framerate()},
                        {"frameRangeDuration", child->frameRange().duration()},
                        {"backgroundColor", QJsonArray{childColor.r(), childColor.g(),
                                                        childColor.b(), childColor.a()}}};
                }
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
        ArtifactCompositionInitParams compositionParams;
        compositionParams.setCompositionName(UniString(QStringLiteral("Precompose Fixture")));
        compositionParams.setResolution(1280, 720);
        compositionParams.setFrameRate(24.0);
        compositionParams.setDurationFrames(300);
        compositionParams.setBackgroundColor(FloatColor(0.125f, 0.25f, 0.5f, 1.0f));
        service->createComposition(compositionParams);
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
            for (const auto* name : {"test.state", "test.reset", "test.guiKeyboardRoundTrip", "test.cycleRejected", "test.invalidRequestsAtomic", "test.undoBudgetRejectsStructureChanges", "test.redoBranchDiscarded", "test.keepParentFocused", "test.componentDescriptorsSurviveUnprecompose", "test.matteReferencesSurviveUnprecompose", "test.maskSurvivesUnprecompose", "test.effectSurvivesUnprecompose", "test.compositionEffectStaysOnParent", "test.childCompositionEffectStaysWithChild", "test.openChildComposition", "test.matchWorkspaceDuration", "test.childDurationPropagates", "test.parentDurationPropagatesToChild", "test.sharedChildDurationPropagates", "test.sharedParentDurationPropagates", "test.internalParentRestored", "test.transitiveCycleRejected", "test.exposedOverrideMaterialized", "test.nestedExposedOverrideMaterialized", "test.missingChildReferenceAtomic", "test.emptyChildUnwrapRejected", "test.emptyMoveAllRejectedAtomic", "test.unpackDeleteRedo", "test.layerNotesRoundTrip", "test.layerFlagsAndBlendRoundTrip", "test.sharedChildReferences", "test.sharedInstanceOverridesRoundTrip", "test.singleLayerRoundTrip", "test.externalParentRestored", "test.missingExternalParentUnwrapAtomic", "test.selectionTransitions", "test.childEditsSurviveUnwrap", "test.externalParentSaveReload", "test.rationalFrameRatePreserved", "test.negativeTimingRoundTrip", "test.animatedTiming", "test.precompose", "test.precomposeAll", "test.saveReload", "test.nestedHierarchy", "test.nestedSaveReload", "test.nestedExternalParentRestored", "test.reparentExistingPrecompose", "test.unprecomposeNested", "test.nonContiguousSelectionOrder",
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
