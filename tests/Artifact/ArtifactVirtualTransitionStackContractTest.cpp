#include <gtest/gtest.h>

#include <QApplication>
#include <QCoreApplication>
#include <QJsonArray>
#include <QImage>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QJsonObject>
#include <QPointF>
#include <QWidget>

#include <array>
#include <cstdint>

import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Layer.Factory;
import Artifact.Layer.InitParams;
import Artifact.Layers.SolidImage;
import Artifact.Timeline.TrackPainterView;
import Artifact.Render.SoftwareCompositor;
import Color.Float;
import Core.ArtifactArray;
import Frame.Range;
import Layer.Blend;
import Utils.Id;
import Utils.String.UniString;

namespace {

QApplication* ensureTestApplication()
{
    if (auto* existing = qobject_cast<QApplication*>(QCoreApplication::instance())) {
        return existing;
    }
    if (QCoreApplication::instance()) {
        return nullptr;
    }

    static int argc = 1;
    static char applicationName[] = "ArtifactVirtualTransitionStackContractTest";
    static char* argv[] = {applicationName, nullptr};
    static QApplication application(argc, argv);
    return &application;
}

Artifact::CompositionTimelineTransition makeTransition(
    const QString& id, const QString& left, const QString& right,
    qint64 start, qint64 end)
{
    Artifact::CompositionTimelineTransition transition;
    transition.id = id;
    transition.name = QStringLiteral("Virtual Transition");
    transition.kind = QStringLiteral("Crossfade");
    transition.easing = QStringLiteral("Ease In-Out");
    transition.leftClipName = left;
    transition.rightClipName = right;
    transition.stackPosition = 2;
    transition.range = ArtifactCore::FrameRange(start, end);
    return transition;
}

} // namespace

TEST(ArtifactVirtualTransitionStackContractTest,
     SoftwareCrossfadeTileInterpolatesPremultipliedAlpha)
{
    QImage canvas(2, 1, QImage::Format_RGBA8888);
    canvas.fill(Qt::transparent);
    const std::array<std::uint8_t, 8> left = {
        0, 0, 128, 128,
        0, 0, 0, 0};
    const std::array<std::uint8_t, 8> right = {
        128, 0, 0, 128,
        0, 0, 0, 0};

    ASSERT_TRUE(Artifact::SoftwareRender::blendCrossfadeTile(
        canvas, left.data(), 8, right.data(), 8, 0, 0, 2, 1, 0.5f,
        1.0f, 1.0f, ArtifactCore::BlendMode::Normal));

    const QColor midpoint = canvas.pixelColor(0, 0);
    EXPECT_NEAR(midpoint.red(), 188, 1);
    EXPECT_NEAR(midpoint.green(), 0, 1);
    EXPECT_NEAR(midpoint.blue(), 188, 1);
    EXPECT_NEAR(midpoint.alpha(), 128, 1);
    EXPECT_EQ(canvas.pixelColor(1, 0).alpha(), 0);
}

TEST(ArtifactVirtualTransitionStackContractTest,
     EvaluatesAllConcurrentDisjointTransitionsIntoReusableOutput)
{
    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(QStringLiteral("ConcurrentTransitions")),
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 1.0f));
    params.setResolution(16, 16);
    params.setFrameRate(24.0);
    params.setDurationFrames(100);
    Artifact::ArtifactAbstractComposition composition(
        ArtifactCore::CompositionID(), params);
    Artifact::ArtifactLayerFactory factory;

    ArtifactCore::ArtifactArray<Artifact::ArtifactAbstractLayerPtr> layers;
    for (const auto& name : {QStringLiteral("A"), QStringLiteral("B"),
                             QStringLiteral("C"), QStringLiteral("D")}) {
        Artifact::ArtifactSolidLayerInitParams layerParams(name);
        layerParams.setWidth(16);
        layerParams.setHeight(16);
        const auto result = factory.createLayer(layerParams);
        ASSERT_TRUE(result.success);
        ASSERT_TRUE(composition.appendLayerTop(result.layer).success);
        layers.append(result.layer);
    }

    auto first = makeTransition(QStringLiteral("first"), QStringLiteral("A"),
                                QStringLiteral("B"), 10, 20);
    first.easing = QStringLiteral("Linear");
    first.leftLayerId = layers[0]->id();
    first.rightLayerId = layers[1]->id();
    composition.addTimelineTransition(first);

    auto second = makeTransition(QStringLiteral("second"), QStringLiteral("C"),
                                 QStringLiteral("D"), 12, 22);
    second.easing = QStringLiteral("Linear");
    second.leftLayerId = layers[2]->id();
    second.rightLayerId = layers[3]->id();
    composition.addTimelineTransition(second);

    ArtifactCore::ArtifactArray<
        Artifact::CompositionTimelineTransitionEvaluation> evaluations;
    evaluations.reserve(2);
    composition.evaluateTimelineTransitionsAtFrame(15, evaluations);

    ASSERT_EQ(evaluations.size(), 2u);
    EXPECT_NEAR(evaluations[0].progress, 0.5, 1e-9);
    EXPECT_NEAR(evaluations[1].progress, 0.3, 1e-9);
    EXPECT_NEAR(
        Artifact::CompositionTimelineTransitionEvaluation::
            combinedOpacityMultiplierForLayer(evaluations, layers[0]->id()),
        0.5, 1e-9);
    EXPECT_NEAR(
        Artifact::CompositionTimelineTransitionEvaluation::
            combinedOpacityMultiplierForLayer(evaluations, layers[2]->id()),
        0.7, 1e-9);
    EXPECT_DOUBLE_EQ(
        Artifact::CompositionTimelineTransitionEvaluation::
            combinedOpacityMultiplierForLayer(evaluations, layers[1]->id()),
        0.5);
    EXPECT_DOUBLE_EQ(
        Artifact::CompositionTimelineTransitionEvaluation::
            combinedOpacityMultiplierForLayer(evaluations, layers[3]->id()),
        0.3);

    composition.evaluateTimelineTransitionsAtFrame(0, evaluations);
    EXPECT_TRUE(evaluations.isEmpty());
    EXPECT_EQ(evaluations.capacity(), 2u);
}

TEST(ArtifactVirtualTransitionStackContractTest,
     TransitionJsonRoundTripPreservesStackPositionAndLegacyDefaults)
{
    const auto original = makeTransition(
        QStringLiteral("transition-1"), QStringLiteral("A"),
        QStringLiteral("B"), 10, 20);
    auto withSourceIds = original;
    withSourceIds.leftLayerId = ArtifactCore::LayerID(
        QStringLiteral("00000000-0000-0000-0000-00000000000a"));
    withSourceIds.rightLayerId = ArtifactCore::LayerID(
        QStringLiteral("00000000-0000-0000-0000-00000000000b"));
    const auto restored = Artifact::CompositionTimelineTransition::fromJson(
        withSourceIds.toJson());

    EXPECT_EQ(restored.id, original.id);
    EXPECT_EQ(restored.leftClipName, original.leftClipName);
    EXPECT_EQ(restored.rightClipName, original.rightClipName);
    EXPECT_EQ(restored.stackPosition, 2);
    EXPECT_EQ(restored.leftLayerId, withSourceIds.leftLayerId);
    EXPECT_EQ(restored.rightLayerId, withSourceIds.rightLayerId);
    EXPECT_EQ(restored.leftLayerStackIndex, -1);
    EXPECT_EQ(restored.rightLayerStackIndex, -1);
    EXPECT_EQ(restored.range.start(), 10);
    EXPECT_EQ(restored.range.end(), 20);

    QJsonObject legacy;
    legacy.insert(QStringLiteral("id"), QStringLiteral("legacy"));
    legacy.insert(QStringLiteral("leftClipName"), QStringLiteral("A"));
    legacy.insert(QStringLiteral("rightClipName"), QStringLiteral("B"));
    const auto legacyRestored =
        Artifact::CompositionTimelineTransition::fromJson(legacy);
    EXPECT_EQ(legacyRestored.stackPosition, -1);
    EXPECT_TRUE(legacyRestored.leftLayerId.isNil());
    EXPECT_TRUE(legacyRestored.rightLayerId.isNil());
}

TEST(ArtifactVirtualTransitionStackContractTest,
     CompositionRejectsOverlappingTransitionsSharingEitherSource)
{
    Artifact::ArtifactCompositionInitParams params;
    Artifact::ArtifactAbstractComposition composition(
        ArtifactCore::CompositionID(), params);

    composition.addTimelineTransition(makeTransition(
        QStringLiteral("first"), QStringLiteral("A"), QStringLiteral("B"),
        10, 20));
    composition.addTimelineTransition(makeTransition(
        QStringLiteral("second"), QStringLiteral("B"), QStringLiteral("C"),
        20, 30));
    EXPECT_EQ(composition.timelineTransitions().size(), 1);

    composition.addTimelineTransition(makeTransition(
        QStringLiteral("adjacent"), QStringLiteral("B"), QStringLiteral("C"),
        21, 30));
    ASSERT_EQ(composition.timelineTransitions().size(), 2);
    EXPECT_FALSE(composition.setTimelineTransitionRange(
        QStringLiteral("adjacent"), ArtifactCore::FrameRange(20, 30)));

    auto renamedSourceConflict = makeTransition(
        QStringLiteral("renamed-conflict"), QStringLiteral("Renamed B"),
        QStringLiteral("D"), 15, 25);
    renamedSourceConflict.leftLayerId = ArtifactCore::LayerID(
        QStringLiteral("00000000-0000-0000-0000-00000000000b"));
    renamedSourceConflict.rightLayerId = ArtifactCore::LayerID(
        QStringLiteral("00000000-0000-0000-0000-00000000000d"));
    auto idBackedFirst = makeTransition(
        QStringLiteral("id-backed-first"), QStringLiteral("A"),
        QStringLiteral("B"), 10, 20);
    idBackedFirst.leftLayerId = ArtifactCore::LayerID(
        QStringLiteral("00000000-0000-0000-0000-00000000000a"));
    idBackedFirst.rightLayerId = renamedSourceConflict.leftLayerId;
    Artifact::ArtifactAbstractComposition idBackedComposition(
        ArtifactCore::CompositionID(), params);
    idBackedComposition.addTimelineTransition(idBackedFirst);
    idBackedComposition.addTimelineTransition(renamedSourceConflict);
    EXPECT_EQ(idBackedComposition.timelineTransitions().size(), 1);
    auto independentTransition = makeTransition(
        QStringLiteral("independent"), QStringLiteral("C"),
        QStringLiteral("D"), 15, 25);
    independentTransition.leftLayerId = ArtifactCore::LayerID(
        QStringLiteral("00000000-0000-0000-0000-00000000000c"));
    independentTransition.rightLayerId = renamedSourceConflict.rightLayerId;
    idBackedComposition.addTimelineTransition(independentTransition);
    ASSERT_EQ(idBackedComposition.timelineTransitions().size(), 2);
    EXPECT_FALSE(idBackedComposition.setTimelineTransitionSources(
        QStringLiteral("independent"), idBackedFirst.leftLayerId,
        independentTransition.rightLayerId));
}

TEST(ArtifactVirtualTransitionStackContractTest,
     StableSourceIdsSurviveRenameAndLegacyNamesMustBeUnique)
{
    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(QStringLiteral("TransitionSources")),
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 1.0f));
    params.setResolution(16, 16);
    params.setFrameRate(24.0);
    params.setDurationFrames(100);
    Artifact::ArtifactAbstractComposition composition(
        ArtifactCore::CompositionID(), params);

    Artifact::ArtifactLayerFactory factory;
    Artifact::ArtifactSolidLayerInitParams aParams(QStringLiteral("A"));
    aParams.setWidth(16);
    aParams.setHeight(16);
    Artifact::ArtifactSolidLayerInitParams bParams(QStringLiteral("B"));
    bParams.setWidth(16);
    bParams.setHeight(16);
    Artifact::ArtifactSolidLayerInitParams duplicateBParams(
        QStringLiteral("B"));
    duplicateBParams.setWidth(16);
    duplicateBParams.setHeight(16);
    const auto a = factory.createLayer(aParams);
    const auto b = factory.createLayer(bParams);
    const auto duplicateB = factory.createLayer(duplicateBParams);
    ASSERT_TRUE(a.success);
    ASSERT_TRUE(b.success);
    ASSERT_TRUE(duplicateB.success);
    ASSERT_TRUE(composition.appendLayerTop(a.layer).success);
    ASSERT_TRUE(composition.appendLayerTop(b.layer).success);

    auto stable = makeTransition(QStringLiteral("stable"),
                                 QStringLiteral("A"), QStringLiteral("B"),
                                 10, 20);
    stable.leftLayerId = a.layer->id();
    stable.rightLayerId = b.layer->id();
    composition.addTimelineTransition(stable);
    a.layer->setLayerName(QStringLiteral("Renamed A"));
    const auto resolvedStableA =
        composition.resolveTimelineTransitionSource(stable, true);
    ASSERT_TRUE(resolvedStableA);
    EXPECT_EQ(resolvedStableA->id(), a.layer->id());
    const auto middleEvaluation =
        composition.evaluateTimelineTransitionAtFrame(15);
    ASSERT_TRUE(middleEvaluation.active);
    EXPECT_TRUE(middleEvaluation.isSourceLayer(a.layer->id()));
    EXPECT_TRUE(middleEvaluation.isSourceLayer(b.layer->id()));
    EXPECT_DOUBLE_EQ(
        middleEvaluation.opacityMultiplierForLayer(a.layer->id()), 0.5);
    EXPECT_DOUBLE_EQ(
        middleEvaluation.opacityMultiplierForLayer(b.layer->id()), 0.5);
    EXPECT_DOUBLE_EQ(middleEvaluation.opacityMultiplierForLayer(
                         ArtifactCore::LayerID::Nil()), 1.0);

    auto cutEvaluation = middleEvaluation;
    cutEvaluation.hardCut = true;
    cutEvaluation.progress = 0.49;
    EXPECT_DOUBLE_EQ(
        cutEvaluation.opacityMultiplierForLayer(a.layer->id()), 1.0);
    EXPECT_DOUBLE_EQ(
        cutEvaluation.opacityMultiplierForLayer(b.layer->id()), 0.0);
    cutEvaluation.progress = 0.5;
    EXPECT_DOUBLE_EQ(
        cutEvaluation.opacityMultiplierForLayer(a.layer->id()), 0.0);
    EXPECT_DOUBLE_EQ(
        cutEvaluation.opacityMultiplierForLayer(b.layer->id()), 1.0);

    auto legacy = makeTransition(QStringLiteral("legacy-unique"),
                                 QStringLiteral("Renamed A"),
                                 QStringLiteral("B"), 30, 40);
    const auto resolvedLegacyA =
        composition.resolveTimelineTransitionSource(legacy, true);
    const auto resolvedLegacyB =
        composition.resolveTimelineTransitionSource(legacy, false);
    ASSERT_TRUE(resolvedLegacyA);
    ASSERT_TRUE(resolvedLegacyB);
    EXPECT_EQ(resolvedLegacyA->id(), a.layer->id());
    EXPECT_EQ(resolvedLegacyB->id(), b.layer->id());

    QJsonObject legacyCompositionJson = composition.toJson().object();
    QJsonObject legacyTransitionJson = legacy.toJson();
    legacyTransitionJson.remove(QStringLiteral("leftLayerId"));
    legacyTransitionJson.remove(QStringLiteral("rightLayerId"));
    QJsonArray legacyTransitions;
    legacyTransitions.append(legacyTransitionJson);
    legacyCompositionJson.insert(QStringLiteral("timelineTransitions"),
                                 legacyTransitions);
    const auto restoredLegacyComposition =
        Artifact::ArtifactAbstractComposition::fromJson(
            QJsonDocument(legacyCompositionJson));
    ASSERT_TRUE(restoredLegacyComposition);
    const auto migratedLegacy = restoredLegacyComposition->timelineTransitionById(
        QStringLiteral("legacy-unique"));
    ASSERT_TRUE(migratedLegacy.has_value());
    EXPECT_EQ(migratedLegacy->leftLayerId, a.layer->id());
    EXPECT_EQ(migratedLegacy->rightLayerId, b.layer->id());
    const auto migratedEvaluation =
        restoredLegacyComposition->evaluateTimelineTransitionAtFrame(35);
    ASSERT_TRUE(migratedEvaluation.active);
    EXPECT_EQ(migratedEvaluation.leftLayerStackIndex, 0);
    EXPECT_EQ(migratedEvaluation.rightLayerStackIndex, 1);
    EXPECT_EQ(migratedEvaluation.insertionIndex, 1);

    ASSERT_TRUE(composition.appendLayerTop(duplicateB.layer).success);
    EXPECT_FALSE(composition.resolveTimelineTransitionSource(legacy, false));
    EXPECT_FALSE(composition.setTimelineTransitionSources(
        QStringLiteral("stable"), a.layer->id(), a.layer->id()));
    ASSERT_TRUE(composition.setTimelineTransitionSources(
        QStringLiteral("stable"), a.layer->id(), duplicateB.layer->id()));
    const auto updatedStable = composition.timelineTransitionById(
        QStringLiteral("stable"));
    ASSERT_TRUE(updatedStable.has_value());
    EXPECT_EQ(updatedStable->rightLayerId, duplicateB.layer->id());

    composition.moveLayerToIndex(duplicateB.layer->id(), 0);
    const auto reorderedEvaluation =
        composition.evaluateTimelineTransitionAtFrame(15);
    ASSERT_TRUE(reorderedEvaluation.active);
    EXPECT_EQ(reorderedEvaluation.leftLayerStackIndex, 1);
    EXPECT_EQ(reorderedEvaluation.rightLayerStackIndex, 0);
    EXPECT_EQ(reorderedEvaluation.insertionIndex, 2);
    ASSERT_TRUE(composition.setTimelineTransitionStackPosition(
        QStringLiteral("stable"), 0));
    EXPECT_EQ(composition.evaluateTimelineTransitionAtFrame(15).insertionIndex,
              0);

    Artifact::ArtifactSolidLayerInitParams insertedParams(
        QStringLiteral("Inserted"));
    insertedParams.setWidth(16);
    insertedParams.setHeight(16);
    const auto inserted = factory.createLayer(insertedParams);
    ASSERT_TRUE(inserted.success);
    ASSERT_TRUE(composition.appendLayerBottom(inserted.layer).success);
    const auto afterInsert = composition.evaluateTimelineTransitionAtFrame(15);
    EXPECT_EQ(afterInsert.leftLayerStackIndex, 2);
    EXPECT_EQ(afterInsert.rightLayerStackIndex, 1);
    EXPECT_EQ(afterInsert.insertionIndex, 0);

    composition.removeLayer(inserted.layer->id());
    const auto afterRemove = composition.evaluateTimelineTransitionAtFrame(15);
    EXPECT_EQ(afterRemove.leftLayerStackIndex, 1);
    EXPECT_EQ(afterRemove.rightLayerStackIndex, 0);
}

TEST(ArtifactVirtualTransitionStackContractTest,
     StackPositionAndEasedProgressAreEditableAndPersisted)
{
    Artifact::ArtifactCompositionInitParams params;
    Artifact::ArtifactAbstractComposition composition(
        ArtifactCore::CompositionID(), params);
    auto sourceTransition = makeTransition(
        QStringLiteral("transition"), QStringLiteral("A"),
        QStringLiteral("B"), 10, 20);
    sourceTransition.leftLayerId = ArtifactCore::LayerID(
        QStringLiteral("00000000-0000-0000-0000-00000000000a"));
    sourceTransition.rightLayerId = ArtifactCore::LayerID(
        QStringLiteral("00000000-0000-0000-0000-00000000000b"));
    composition.addTimelineTransition(sourceTransition);

    EXPECT_TRUE(composition.setTimelineTransitionStackPosition(
        QStringLiteral("transition"), 1));
    EXPECT_FALSE(composition.setTimelineTransitionStackPosition(
        QStringLiteral("transition"), -2));
    EXPECT_DOUBLE_EQ(composition.timelineTransitionProgressAtFrame(10), 0.0);
    EXPECT_NEAR(composition.timelineTransitionProgressAtFrame(15), 0.5, 1e-9);
    EXPECT_LT(composition.timelineTransitionProgressAtFrame(12), 0.5);
    EXPECT_GT(composition.timelineTransitionProgressAtFrame(18), 0.5);
    EXPECT_DOUBLE_EQ(composition.timelineTransitionProgressAtFrame(20), 1.0);

    Artifact::CompositionTimelineTransition duplicate;
    ASSERT_TRUE(composition.duplicateTimelineTransition(
        QStringLiteral("transition"), &duplicate));
    EXPECT_EQ(duplicate.range.start(), 21);
    EXPECT_EQ(duplicate.range.end(), 31);

    const auto json = composition.toJson();
    const auto restored = Artifact::ArtifactAbstractComposition::fromJson(json);
    ASSERT_TRUE(restored);
    const auto transition = restored->timelineTransitionById(
        QStringLiteral("transition"));
    ASSERT_TRUE(transition.has_value());
    EXPECT_EQ(transition->stackPosition, 1);
    EXPECT_EQ(transition->leftLayerId, ArtifactCore::LayerID(
                  QStringLiteral("00000000-0000-0000-0000-00000000000a")));
    EXPECT_EQ(transition->rightLayerId, ArtifactCore::LayerID(
                  QStringLiteral("00000000-0000-0000-0000-00000000000b")));
    EXPECT_EQ(transition->easing, QStringLiteral("Ease In-Out"));
}

TEST(ArtifactVirtualTransitionStackContractTest,
     FrameEvaluationCarriesEditedStackPosition)
{
    Artifact::ArtifactCompositionInitParams params;
    Artifact::ArtifactAbstractComposition composition(
        ArtifactCore::CompositionID(), params);
    auto transition = makeTransition(
        QStringLiteral("render-order"), QStringLiteral("A"),
        QStringLiteral("B"), 10, 20);
    transition.leftLayerId = ArtifactCore::LayerID(
        QStringLiteral("00000000-0000-0000-0000-00000000000a"));
    transition.rightLayerId = ArtifactCore::LayerID(
        QStringLiteral("00000000-0000-0000-0000-00000000000b"));
    composition.addTimelineTransition(transition);

    const auto before = composition.evaluateTimelineTransitionAtFrame(15);
    ASSERT_TRUE(before.active);
    ASSERT_TRUE(composition.setTimelineTransitionStackPosition(
        QStringLiteral("render-order"), 0));
    const auto moved = composition.evaluateTimelineTransitionAtFrame(15);

    // This covers model-to-evaluation propagation only. Renderer insertion
    // order remains a separate, still-unimplemented contract.
    EXPECT_NE(before.stackPosition, moved.stackPosition);
}

TEST(ArtifactVirtualTransitionStackContractTest,
     TimelineShowsCompactMarkerUntilTransitionIsSelected)
{
    ASSERT_NE(ensureTestApplication(), nullptr)
        << "Existing application type: "
        << (QCoreApplication::instance()
                ? QCoreApplication::instance()->metaObject()->className()
                : "none");

    Artifact::ArtifactTimelineTrackPainterView view;
    view.resize(240, 40);
    view.setTrackCount(1);
    view.setTrackHeights({32});
    view.setDurationFrames(100.0);
    view.setPixelsPerFrame(2.0);

    Artifact::ArtifactTimelineTrackPainterView::TrackClipVisual transition;
    transition.clipId = QStringLiteral("transition");
    transition.trackIndex = 0;
    transition.startFrame = 20.0;
    transition.durationFrame = 40.0;
    transition.kind = Artifact::ArtifactTimelineTrackPainterView::TrackClipVisual::Kind::Transition;
    transition.fillColor = QColor(245, 174, 32);
    transition.title = QStringLiteral("Crossfade → Plate B");
    view.setClips({transition});

    QImage compactMarker(240, 40, QImage::Format_ARGB32_Premultiplied);
    compactMarker.fill(Qt::transparent);
    view.render(&compactMarker);

    transition.selected = true;
    view.setClips({transition});
    QImage selectedRange(240, 40, QImage::Format_ARGB32_Premultiplied);
    selectedRange.fill(Qt::transparent);
    view.render(&selectedRange);

    EXPECT_NE(compactMarker.pixelColor(60, 16),
              selectedRange.pixelColor(60, 16));
}

TEST(ArtifactVirtualTransitionStackContractTest,
     TimelineTransitionMarkerParticipatesInExistingHoverHitTest)
{
    ASSERT_NE(ensureTestApplication(), nullptr)
        << "Existing application type: "
        << (QCoreApplication::instance()
                ? QCoreApplication::instance()->metaObject()->className()
                : "none");

    Artifact::ArtifactTimelineTrackPainterView view;
    view.resize(240, 40);
    view.setPixelsPerFrame(2.0);
    Artifact::ArtifactTimelineTrackPainterView::TrackClipVisual transition;
    transition.clipId = QStringLiteral("transition-hover");
    transition.trackIndex = 0;
    transition.title = QStringLiteral("A → B");
    transition.startFrame = 10;
    transition.durationFrame = 20;
    transition.kind = Artifact::ArtifactTimelineTrackPainterView::
        TrackClipVisual::Kind::Transition;
    view.setClips({transition});

    // The compact transition marker is centered at
    // (startFrame + durationFrame / 2) * pixelsPerFrame = 40 px.
    QMouseEvent move(QEvent::MouseMove, QPointF(40, 16),
                     Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&view, &move);

    EXPECT_EQ(view.hoveredClip().clipId, transition.clipId);
    EXPECT_EQ(view.hoveredClip().kind,
              Artifact::ArtifactTimelineTrackPainterView::
                  TrackClipVisual::Kind::Transition);
}

TEST(ArtifactVirtualTransitionStackContractTest,
     DrawPlanPlacesTransitionCompositeAtInsertionIndex)
{
    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(QStringLiteral("DrawPlanContract")),
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 1.0f));
    params.setResolution(16, 16);
    params.setFrameRate(24.0);
    params.setDurationFrames(100);
    Artifact::ArtifactAbstractComposition composition(
        ArtifactCore::CompositionID(), params);
    Artifact::ArtifactLayerFactory factory;

    ArtifactCore::ArtifactArray<Artifact::ArtifactAbstractLayerPtr> layers;
    for (const auto& name : {QStringLiteral("A"), QStringLiteral("B"),
                             QStringLiteral("C"), QStringLiteral("D")}) {
        Artifact::ArtifactSolidLayerInitParams layerParams(name);
        layerParams.setWidth(16);
        layerParams.setHeight(16);
        const auto result = factory.createLayer(layerParams);
        ASSERT_TRUE(result.success);
        ASSERT_TRUE(composition.appendLayerTop(result.layer).success);
        layers.append(result.layer);
    }

    auto atZero = makeTransition(QStringLiteral("at-zero"), QStringLiteral("C"),
                                 QStringLiteral("D"), 10, 20);
    atZero.leftLayerId = layers[2]->id();
    atZero.rightLayerId = layers[3]->id();
    atZero.stackPosition = 0;
    composition.addTimelineTransition(atZero);

    auto atTwo = makeTransition(QStringLiteral("at-two"), QStringLiteral("A"),
                                QStringLiteral("B"), 10, 20);
    atTwo.leftLayerId = layers[0]->id();
    atTwo.rightLayerId = layers[1]->id();
    atTwo.stackPosition = 2;
    composition.addTimelineTransition(atTwo);

    ArtifactCore::ArtifactArray<
        Artifact::CompositionTimelineTransitionEvaluation> evaluations;
    ArtifactCore::ArtifactArray<
        Artifact::CompositionTimelineTransitionDrawStep> steps;
    evaluations.reserve(2);
    steps.reserve(6);
    composition.buildTimelineTransitionDrawPlan(15, evaluations, steps);

    ASSERT_EQ(evaluations.size(), 2u);
    ASSERT_EQ(steps.size(), 6u);
    // Each transition stages both sources first, then emits its composite at
    // the insertion point. This lets renderers reuse one bounded A/B target
    // pair while still suppressing each source's natural stack draw.
    EXPECT_EQ(steps[0].kind,
              Artifact::CompositionTimelineTransitionDrawStep::Kind::Layer);
    EXPECT_EQ(steps[0].layerIndex, 2);
    EXPECT_EQ(steps[1].kind,
              Artifact::CompositionTimelineTransitionDrawStep::Kind::Layer);
    EXPECT_EQ(steps[1].layerIndex, 3);
    EXPECT_EQ(steps[2].kind,
              Artifact::CompositionTimelineTransitionDrawStep::Kind::TransitionComposite);
    EXPECT_EQ(steps[2].layerIndex, 0);
    EXPECT_TRUE(evaluations[steps[2].evaluationIndex].isSourceLayer(layers[3]->id()));
    EXPECT_EQ(steps[3].kind,
              Artifact::CompositionTimelineTransitionDrawStep::Kind::Layer);
    EXPECT_EQ(steps[3].layerIndex, 0);
    EXPECT_EQ(steps[4].kind,
              Artifact::CompositionTimelineTransitionDrawStep::Kind::Layer);
    EXPECT_EQ(steps[4].layerIndex, 1);
    EXPECT_EQ(steps[5].kind,
              Artifact::CompositionTimelineTransitionDrawStep::Kind::TransitionComposite);
    EXPECT_EQ(steps[5].layerIndex, 2);
    EXPECT_TRUE(evaluations[steps[5].evaluationIndex].isSourceLayer(layers[0]->id()));
    EXPECT_EQ(evaluations.capacity(), 2u);
    EXPECT_EQ(steps.capacity(), 6u);

    ASSERT_TRUE(composition.setTimelineTransitionStackPosition(
        QStringLiteral("at-two"), 1));
    composition.buildTimelineTransitionDrawPlan(15, evaluations, steps);
    ASSERT_EQ(steps.size(), 6u);
    bool sawCompositeAtOne = false;
    for (const auto& step : steps) {
        if (step.kind !=
            Artifact::CompositionTimelineTransitionDrawStep::Kind::TransitionComposite) {
            continue;
        }
        if (step.layerIndex == 1 &&
            evaluations[step.evaluationIndex].isSourceLayer(layers[0]->id())) {
            sawCompositeAtOne = true;
        }
    }
    EXPECT_TRUE(sawCompositeAtOne);
}

