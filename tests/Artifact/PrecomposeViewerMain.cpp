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
#include <QSize>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimerEvent>
#include <QVariant>
#include <QVBoxLayout>
#include <QVector>
#include <cmath>
#include <cstdio>
#include <exception>

import Artifact.Test.PreCompose;
import Artifact.Service.Project;
import Artifact.Project.Manager;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Layer.Abstract;
import Artifact.Layer.InitParams;
import Artifact.Layer.Composition;
import Artifact.Layers.Selection.Manager;
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
            child->setFrameRange(FrameRange(0, 90));
            const int64_t shortenedOut = wrapper->outPoint().framePosition();
            const bool shrinkFollowed = shortenedOut == 130 &&
                wrapper->inPoint().framePosition() == 40 &&
                wrapper->startTime().framePosition() == 40;
            child->setFrameRange(FrameRange(0, 240));
            const int64_t expandedOut = wrapper->outPoint().framePosition();
            const bool expansionFollowed = expandedOut == 280 &&
                wrapper->inPoint().framePosition() == 40 &&
                wrapper->startTime().framePosition() == 40;
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
            result["parentReferencePreserved"] = parentReferencePreserved;
            result["passed"] = shrinkFollowed && expansionFollowed && parentReferencePreserved;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Child duration change did not update the parent window from its fixed in point");
            childId_ = outcome.childCompId;
            hasChild_ = true;
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
            auto outerChild = service->findComposition(outer.childCompId).ptr.lock();
            auto nestedWrapper = outerChild
                ? dynamicPointerCast<ArtifactCompositionLayer>(outerChild->layerById(inner.precompLayerId))
                : SharedPtr<ArtifactCompositionLayer>{};
            const bool nestedOverrideInMemory = outerCreated && nestedWrapper == innerWrapper &&
                retainsOverride(nestedWrapper) &&
                std::abs(sourceLayer->opacity() - binding.defaultValue.toFloat()) < 1.0e-5f;
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
                    outerChild && innerChild && nestedWrapper && sourceLayer &&
                    retainsOverride(nestedWrapper) && outerMappingRestored && innerMappingRestored &&
                    std::abs(sourceLayer->opacity() - binding.defaultValue.toFloat()) < 1.0e-5f;
                outerUnpacked = saveReloadPreserved &&
                    service->unprecomposeLayerWithUndo(outer.precompLayerId, true);
            }
            const auto restoredInner = outerUnpacked
                ? dynamicPointerCast<ArtifactCompositionLayer>(root->layerById(inner.precompLayerId))
                : SharedPtr<ArtifactCompositionLayer>{};
            const bool innerOverridePreserved = outerUnpacked && restoredInner == innerWrapper &&
                retainsOverride(restoredInner) &&
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
            overrideResult["nestedOverridePreserved"] = nestedOverrideInMemory;
            overrideResult["saveReloadPreserved"] = saveReloadPreserved;
            overrideResult["outerUnpacked"] = outerUnpacked;
            overrideResult["innerOverridePreserved"] = innerOverridePreserved;
            overrideResult["overrideMaterialized"] = overrideMaterialized;
            overrideResult["undoPreservedOverride"] = undoPreservedOverride;
            overrideResult["redoPreservedOverride"] = redoPreservedOverride;
            overrideResult["passed"] = bindingAdded && overrideDeferred && nestedOverrideInMemory &&
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
        } else if (name == "test.sharedChildReferences") {
            QJsonObject result;
            const auto root = service->currentComposition().lock();
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
            const bool dissolved = twoReferencesCreated &&
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
            result["twoReferencesCreated"] = twoReferencesCreated;
            result["childDataPreserved"] = childDataPreserved;
            result["remainingReferencePreserved"] = remainingReferencePreserved;
            result["undoRestoredBoth"] = undoRestoredBoth;
            result["redoChildDataPreserved"] = redoChildDataPreserved;
            result["redoPreservedRemaining"] = redoPreservedRemaining;
            result["passed"] = twoReferencesCreated && remainingReferencePreserved &&
                undoRestoredBoth && redoPreservedRemaining;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Unprecomposing one instance changed shared child data or another reference");
            childId_ = first.childCompId;
            hasChild_ = true;
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
            layerA->setOpacity(expectedOpacityA);
            layerB->setPosition3D({-18.0f, 64.0f, 2.0f});
            layerB->setOpacity(expectedOpacityB);
            const auto matchesEditedValues = [&](const ArtifactAbstractLayerPtr& a,
                                                 const ArtifactAbstractLayerPtr& b) {
                if (!a || !b) return false;
                const auto positionA = a->position3D();
                const auto positionB = b->position3D();
                return std::abs(positionA.x - 135.0f) < 1.0e-5f &&
                    std::abs(positionA.y - (-42.0f)) < 1.0e-5f &&
                    std::abs(positionA.z - 7.5f) < 1.0e-5f &&
                    std::abs(positionB.x - (-18.0f)) < 1.0e-5f &&
                    std::abs(positionB.y - 64.0f) < 1.0e-5f &&
                    std::abs(positionB.z - 2.0f) < 1.0e-5f &&
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
            const bool undoAvailable = undo->canUndo();
            if (undoAvailable) undo->undo();
            const auto undoChild = service->findComposition(outcome.childCompId).ptr.lock();
            const auto undoA = undoChild ? undoChild->layerById(idsA.front()) : ArtifactAbstractLayerPtr{};
            const auto undoB = undoChild ? undoChild->layerById(idsB.front()) : ArtifactAbstractLayerPtr{};
            const bool undoRestored = undoAvailable && undoChild &&
                PreComposeManager::instance().isPrecomposeLayer(outcome.precompLayerId) &&
                hasTiming(undoA, 0, 160, 7) && hasTiming(undoB, 36, 120, 29);
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
            timingResult["childTimingNormalized"] = childTimingNormalized;
            timingResult["childKeysShifted"] = childKeysShifted;
            timingResult["unpackRestored"] = unpackRestored;
            timingResult["undoRestored"] = undoRestored;
            timingResult["redoRestored"] = redoRestored;
            timingResult["passed"] = childTimingNormalized && childKeysShifted && unpackRestored &&
                undoRestored && redoRestored;
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
            const bool undoAvailable = undo->canUndo();
            if (undoAvailable) undo->undo();
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
            const auto allPropertyA = allSourceA
                ? allSourceA->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const auto allPropertyB = allSourceB
                ? allSourceB->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const bool allModeChildKeysPreserved = allPropertyA && allPropertyB &&
                keyframes(allPropertyA) == originalKeys && keyframes(allPropertyB) == originalKeysB;
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
            const bool allModeUnpackTimingPreserved = allUnpacked &&
                hasTiming(allRestoredA, 40, 200, 7) && hasTiming(allRestoredB, 70, 160, 9) &&
                hasTiming(allRestoredUntouched, 25, 220, 3);
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
            const bool allModeRedoTimingPreserved = allRedoAvailable &&
                hasTiming(allRedoA, 40, 200, 7) && hasTiming(allRedoB, 70, 160, 9) &&
                hasTiming(allRedoUntouched, 25, 220, 3);
            animationResult["originalKeys"] = originalKeys;
            animationResult["originalOffsetKeys"] = originalKeysB;
            animationResult["precomposedKeys"] = movedKeys;
            animationResult["precomposedOffsetKeys"] = movedKeysB;
            animationResult["childKeysRetimed"] = childKeysRetimed;
            animationResult["offsetChildKeysRetimed"] = offsetChildKeysRetimed;
            animationResult["unpackRestoredKeys"] = unpackKeysRestored;
            animationResult["redoRestoredKeys"] = redoKeysRestored;
            animationResult["allModeChildKeysPreserved"] = allModeChildKeysPreserved;
            animationResult["allModeChildTimingPreserved"] = allModeChildTimingPreserved;
            animationResult["allModeUnpackKeysPreserved"] = allModeUnpackKeysPreserved;
            animationResult["allModeUnpackTimingPreserved"] = allModeUnpackTimingPreserved;
            animationResult["allModeRedoKeysPreserved"] = allModeRedoKeysPreserved;
            animationResult["allModeRedoTimingPreserved"] = allModeRedoTimingPreserved;
            animationResult["passed"] = childKeysRetimed && offsetChildKeysRetimed && unpackKeysRestored &&
                undoAvailable && redoKeysRestored && allModeChildKeysPreserved &&
                allModeChildTimingPreserved && allModeUnpackKeysPreserved &&
                allModeUnpackTimingPreserved && allUndoAvailable && allModeRedoKeysPreserved &&
                allModeRedoTimingPreserved;
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
            const auto root = service->currentComposition().lock();
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
            if (!service->precomposeLayersWithUndo(
                    {existing.precompLayerId}, UniString(QStringLiteral("Precompose Wrapper")),
                    false, false, PrecomposeMode::MoveSelected)) {
                result["error"] = QStringLiteral("Could not wrap existing precompose layer");
                return result;
            }
            const auto wrapper = service->lastPrecomposeOutcome();
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
            const bool undoRestoredParent = undoAvailable && hasParentReference(rootId) &&
                hierarchyAfterUndo.size() == 2 && hierarchyAfterUndo[0] == rootId &&
                hierarchyAfterUndo[1] == existing.childCompId;
            const bool redoAvailable = undo->canRedo();
            if (redoAvailable) undo->redo();
            const auto hierarchyAfterRedo = manager.getCompositionHierarchy(existing.childCompId);
            const bool redoRestoredParent = redoAvailable && hasParentReference(wrapper.childCompId) &&
                hierarchyAfterRedo.size() == 3 && hierarchyAfterRedo[0] == rootId &&
                hierarchyAfterRedo[1] == wrapper.childCompId &&
                hierarchyAfterRedo[2] == existing.childCompId;
            result["existingChildId"] = existing.childCompId.toString();
            result["wrapperChildId"] = wrapper.childCompId.toString();
            result["parentReferenceUpdated"] = parentReferenceUpdated;
            result["hierarchyUpdated"] = hierarchyUpdated;
            result["undoRestoredParent"] = undoRestoredParent;
            result["redoRestoredParent"] = redoRestoredParent;
            result["undoAvailable"] = undoAvailable;
            result["redoAvailable"] = redoAvailable;
            result["hierarchy"] = QJsonArray{rootId.toString(),
                hierarchyAfterRedo.size() > 1 ? hierarchyAfterRedo[1].toString() : QString(),
                hierarchyAfterRedo.size() > 2 ? hierarchyAfterRedo[2].toString() : QString()};
            result["passed"] = parentReferenceUpdated && hierarchyUpdated && undoRestoredParent &&
                redoRestoredParent;
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
            if (aId.isNil() || untouchedId.isNil() || !service->precomposeLayersWithUndo(
                    {untouchedId, aId}, UniString(QStringLiteral("Noncontiguous Selection")),
                    false, false, PrecomposeMode::MoveSelected)) {
                result["error"] = QStringLiteral("Could not precompose the noncontiguous fixture selection");
                return result;
            }
            const auto outcome = service->lastPrecomposeOutcome();
            const auto child = service->findComposition(outcome.childCompId).ptr.lock();
            const bool childOrderPreserved = child && child->allLayer().size() == 2 &&
                child->allLayer()[0]->id() == aId && child->allLayer()[1]->id() == untouchedId;
            const auto precomposedState = layers(comp, 0);
            const bool unpacked = service->unprecomposeLayerWithUndo(outcome.precompLayerId, true);
            const bool originalOrderRestored = unpacked && layers(comp, 0) == originalState;
            const bool undoAvailable = undo->canUndo();
            if (undoAvailable) undo->undo();
            const bool undoRestoredPrecompose = undoAvailable && layers(comp, 0) == precomposedState;
            const bool redoAvailable = undo->canRedo();
            if (redoAvailable) undo->redo();
            const bool redoRestoredOriginalOrder = redoAvailable && layers(comp, 0) == originalState;
            result["childOrderPreserved"] = childOrderPreserved;
            result["originalOrderRestored"] = originalOrderRestored;
            result["undoRestoredPrecompose"] = undoRestoredPrecompose;
            result["redoRestoredOriginalOrder"] = redoRestoredOriginalOrder;
            result["passed"] = childOrderPreserved && originalOrderRestored &&
                undoRestoredPrecompose && redoRestoredOriginalOrder;
            if (!result["passed"].toBool())
                result["error"] = QStringLiteral("Noncontiguous precompose/unpack changed source layer ordering");
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
            roundTrip["hierarchyDataPreserved"] = hierarchyDataPreserved;
            roundTrip["runtimeHierarchyRestored"] = runtimeHierarchyRestored;
            roundTrip["timeConversionRestored"] = timeConversionRestored;
            roundTrip["timeInRoot"] = timeInRoot;
            roundTrip["passed"] = hierarchyDataPreserved && runtimeHierarchyRestored &&
                timeConversionRestored;
            if (!roundTrip["passed"].toBool())
                roundTrip["error"] = QStringLiteral("Nested save/reload lost composition data or runtime hierarchy");
            childId_ = inner.childCompId;
            hasChild_ = true;
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
            if (!sourceLayer) {
                roundTrip["error"] = QStringLiteral("Precompose source layer unavailable for exposed-property persistence");
                return roundTrip;
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
            roundTrip["parentChildLinkPreserved"] = parentChildLink;
            roundTrip["layerStatePreserved"] = reloaded == original;
            roundTrip["runtimeNestingRestored"] = runtimeNestingRestored;
            roundTrip["masterPropertyDefinitionPreserved"] = masterPropertyDefinitionPreserved;
            roundTrip["masterPropertyOverridePreserved"] = masterPropertyOverridePreserved;
            roundTrip["overrideStillDeferred"] = overrideStillDeferred;
            roundTrip["passed"] = save.success && loaded && parentChildLink && runtimeNestingRestored &&
                masterPropertyDefinitionPreserved && masterPropertyOverridePreserved && overrideStillDeferred;
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
            for (const auto* name : {"test.state", "test.reset", "test.cycleRejected", "test.invalidRequestsAtomic", "test.redoBranchDiscarded", "test.openChildComposition", "test.matchWorkspaceDuration", "test.childDurationPropagates", "test.internalParentRestored", "test.transitiveCycleRejected", "test.exposedOverrideMaterialized", "test.nestedExposedOverrideMaterialized", "test.missingChildReferenceAtomic", "test.emptyChildUnwrapRejected", "test.emptyMoveAllRejectedAtomic", "test.unpackDeleteRedo", "test.sharedChildReferences", "test.singleLayerRoundTrip", "test.externalParentRestored", "test.selectionTransitions", "test.childEditsSurviveUnwrap", "test.externalParentSaveReload", "test.rationalFrameRatePreserved", "test.negativeTimingRoundTrip", "test.animatedTiming", "test.precompose", "test.precomposeAll", "test.saveReload", "test.nestedHierarchy", "test.nestedSaveReload", "test.nestedExternalParentRestored", "test.reparentExistingPrecompose", "test.unprecomposeNested", "test.nonContiguousSelectionOrder",
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
