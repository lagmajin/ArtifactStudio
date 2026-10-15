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
import Property.Abstract;
import Math.Interpolate;
import Frame.Position;
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
        } else if (name == "test.animatedTiming") {
            QJsonObject animationResult;
            const auto parent = service->currentComposition().lock();
            const auto idsA = namedLayerIds(parent, {QStringLiteral("A")});
            const auto idsB = namedLayerIds(parent, {QStringLiteral("B")});
            if (!parent || idsA.size() != 1 || idsB.size() != 1) {
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
            const auto allRestoredPropertyA = allRestoredA
                ? allRestoredA->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const auto allRestoredPropertyB = allRestoredB
                ? allRestoredB->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const bool allModeUnpackKeysPreserved = allUnpacked && allRestoredPropertyA &&
                allRestoredPropertyB && keyframes(allRestoredPropertyA) == originalKeys &&
                keyframes(allRestoredPropertyB) == originalKeysB;
            const bool allUndoAvailable = undo->canUndo();
            if (allUndoAvailable) undo->undo();
            const bool allRedoAvailable = undo->canRedo();
            if (allRedoAvailable) undo->redo();
            const auto allAfterRedo = service->currentComposition().lock();
            const auto allRedoA = allAfterRedo ? allAfterRedo->layerById(idsA.front()) : ArtifactAbstractLayerPtr{};
            const auto allRedoB = allAfterRedo ? allAfterRedo->layerById(idsB.front()) : ArtifactAbstractLayerPtr{};
            const auto allRedoPropertyA = allRedoA
                ? allRedoA->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const auto allRedoPropertyB = allRedoB
                ? allRedoB->getProperty(QStringLiteral("transform.position.x"))
                : SharedPtr<AbstractProperty>{};
            const bool allModeRedoKeysPreserved = allRedoAvailable && allRedoPropertyA &&
                allRedoPropertyB && keyframes(allRedoPropertyA) == originalKeys &&
                keyframes(allRedoPropertyB) == originalKeysB;
            animationResult["originalKeys"] = originalKeys;
            animationResult["originalOffsetKeys"] = originalKeysB;
            animationResult["precomposedKeys"] = movedKeys;
            animationResult["precomposedOffsetKeys"] = movedKeysB;
            animationResult["childKeysRetimed"] = childKeysRetimed;
            animationResult["offsetChildKeysRetimed"] = offsetChildKeysRetimed;
            animationResult["unpackRestoredKeys"] = unpackKeysRestored;
            animationResult["redoRestoredKeys"] = redoKeysRestored;
            animationResult["allModeChildKeysPreserved"] = allModeChildKeysPreserved;
            animationResult["allModeUnpackKeysPreserved"] = allModeUnpackKeysPreserved;
            animationResult["allModeRedoKeysPreserved"] = allModeRedoKeysPreserved;
            animationResult["passed"] = childKeysRetimed && offsetChildKeysRetimed && unpackKeysRestored &&
                undoAvailable && redoKeysRestored && allModeChildKeysPreserved &&
                allModeUnpackKeysPreserved && allUndoAvailable && allModeRedoKeysPreserved;
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
        } else if (name == "test.saveReload") {
            QJsonObject roundTrip;
            auto& projects = ArtifactProjectManager::getInstance();
            const auto parent = service->currentComposition().lock();
            const auto original = parent ? layers(parent, 0) : QJsonArray{};
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
            roundTrip["passed"] = save.success && loaded && parentChildLink && runtimeNestingRestored;
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
            for (const auto* name : {"test.state", "test.reset", "test.cycleRejected", "test.transitiveCycleRejected", "test.externalParentRestored", "test.animatedTiming", "test.precompose", "test.precomposeAll", "test.saveReload", "test.nestedHierarchy", "test.reparentExistingPrecompose", "test.unprecomposeNested",
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
