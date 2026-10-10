#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVariant>

import Project.ProjectVisitor;
import Project.MetadataCollector;
import Composition.ParametricComposition;
import Memory.SharedPtr;
import Utils.Id;

using namespace ArtifactCore;

namespace {

class CountingCollector : public MetadataCollector {
public:
    void reset() override
    {
        projects = 0;
        compositions = 0;
        layers = 0;
        effects = 0;
        properties = 0;
    }

    void onProject(const MetadataNode&) override { ++projects; }
    void onComposition(const MetadataNode&) override { ++compositions; }
    void onLayer(const MetadataNode&) override { ++layers; }
    void onEffect(const MetadataNode&) override { ++effects; }
    void onProperty(const MetadataNode&) override { ++properties; }

    MetadataReport report() const override
    {
        MetadataReport result;
        result.addCount(QStringLiteral("project"), projects);
        result.addCount(QStringLiteral("composition"), compositions);
        result.addCount(QStringLiteral("layer"), layers);
        result.addCount(QStringLiteral("effect"), effects);
        result.addCount(QStringLiteral("property"), properties);
        return result;
    }

    qint64 projects = 0;
    qint64 compositions = 0;
    qint64 layers = 0;
    qint64 effects = 0;
    qint64 properties = 0;
};

ParametricCompositionDefinition makeSampleDefinition()
{
    ParametricCompositionDefinition definition =
        makeDefaultParametricCompositionDefinition(
            QStringLiteral("comp.roundtrip"), QStringLiteral("Roundtrip"));

    ParametricCompositionParameter opacity;
    opacity.key = QStringLiteral("opacity");
    opacity.displayName = QStringLiteral("Opacity");
    opacity.defaultValue = 1.0;
    EXPECT_TRUE(definition.addParameter(opacity));

    ParametricCompositionPublishedControl control;
    control.controlId = QStringLiteral("ctl.opacity");
    control.sourceParameterKey = QStringLiteral("opacity");
    control.displayName = QStringLiteral("Opacity");
    control.defaultValue = 1.0;
    EXPECT_TRUE(definition.addPublishedControl(control));

    ParametricCompositionDataBinding binding;
    binding.columnKey = QStringLiteral("shot");
    binding.targetParameterKey = QStringLiteral("opacity");
    EXPECT_TRUE(definition.addDataBinding(binding));
    return definition;
}

} // namespace

TEST(ProjectVisitorTest, NodeOrderAndDepthSurviveCollect)
{
    ProjectVisitor visitor;
    visitor.beginProject(QStringLiteral("proj-1"), QStringLiteral("Proj"));
    visitor.visitComposition(QStringLiteral("comp-1"), QStringLiteral("Comp"));
    visitor.visitLayer(QStringLiteral("layer-1"), QStringLiteral("Layer"),
                      QStringLiteral("solid"));
    visitor.visitEffect(QStringLiteral("fx-1"), QStringLiteral("Fx"),
                       QStringLiteral("exposure"));
    visitor.visitProperty(QStringLiteral("prop-1"), QStringLiteral("Exposure"),
                         QStringLiteral("double"), 1.5);

    ASSERT_EQ(visitor.nodes().size(), 5);
    EXPECT_EQ(visitor.nodes().at(0).depth, 0);
    EXPECT_EQ(visitor.nodes().at(1).depth, 1);
    EXPECT_EQ(visitor.nodes().at(2).depth, 2);
    EXPECT_EQ(visitor.nodes().at(3).depth, 3);
    EXPECT_EQ(visitor.nodes().at(4).depth, 4);
    EXPECT_EQ(visitor.nodes().at(4).value.toDouble(), 1.5);

    CountingCollector collector;
    visitor.collect({&collector});
    EXPECT_EQ(collector.projects, 1);
    EXPECT_EQ(collector.compositions, 1);
    EXPECT_EQ(collector.layers, 1);
    EXPECT_EQ(collector.effects, 1);
    EXPECT_EQ(collector.properties, 1);

    // collect() resets first: a second pass without new nodes still counts one.
    visitor.collect({&collector});
    EXPECT_EQ(collector.layers, 1);

    visitor.clear();
    EXPECT_TRUE(visitor.nodes().isEmpty());
    visitor.collect({&collector});
    EXPECT_EQ(collector.layers, 0);
}

TEST(ProjectMetadataReportTest, MergeDedupsAndSerializes)
{
    CountingCollector first;
    CountingCollector second;
    ProjectVisitor visitor;
    visitor.beginProject(QStringLiteral("p"), QStringLiteral("P"));
    visitor.visitComposition(QStringLiteral("c"), QStringLiteral("C"));
    visitor.visitLayer(QStringLiteral("l"), QStringLiteral("L"),
                      QStringLiteral("text"));
    const MetadataReport merged =
        MetadataCollectorDriver::collectReport(visitor.nodes(), {&first, &second});
    EXPECT_EQ(merged.count(QStringLiteral("layer")), 2);
    EXPECT_EQ(merged.count(QStringLiteral("missing")), 0);

    MetadataReport manual;
    manual.add(QStringLiteral("tag"), QStringLiteral("night"));
    manual.add(QStringLiteral("tag"), QStringLiteral("night"));
    manual.add(QStringLiteral("tag"), QStringLiteral("day"));
    MetadataReport combined;
    combined.merge(manual);
    combined.merge(manual);
    EXPECT_EQ(combined.values(QStringLiteral("tag")).size(), 2);

    const QJsonObject json = combined.toJson();
    EXPECT_TRUE(json.contains(QStringLiteral("counts")));
    EXPECT_TRUE(json.contains(QStringLiteral("values")));
    EXPECT_EQ(json.value(QStringLiteral("values")).toObject()
                  .value(QStringLiteral("tag")).toArray().size(), 2);

    const QString csv = combined.toCsv();
    EXPECT_TRUE(csv.startsWith(QStringLiteral("kind,key,value")));
    EXPECT_TRUE(csv.contains(QStringLiteral("night")));
}

TEST(ProjectDefinitionTest, DefaultDefinitionValidates)
{
    const ParametricCompositionDefinition definition =
        makeDefaultParametricCompositionDefinition(
            QStringLiteral("comp.default"));
    QString error;
    EXPECT_TRUE(definition.validate(&error));
    EXPECT_TRUE(error.isEmpty());
    EXPECT_EQ(definition.inputSlots().size(), 1);
    EXPECT_EQ(definition.outputSlots().size(), 1);
    EXPECT_TRUE(definition.hasSlot(QStringLiteral("input")));
    EXPECT_TRUE(definition.hasSlot(QStringLiteral("output")));
}

TEST(ProjectDefinitionTest, SlotOwnershipRejectsDuplicatesAndEmpties)
{
    ParametricCompositionDefinition definition =
        makeDefaultParametricCompositionDefinition(
            QStringLiteral("comp.slots"));

    ParametricCompositionSlot duplicate;
    duplicate.slotId = QStringLiteral("input");
    duplicate.role = ParametricCompositionSlotRole::Input;
    EXPECT_FALSE(definition.addSlot(duplicate));

    ParametricCompositionSlot empty;
    empty.role = ParametricCompositionSlotRole::Input;
    EXPECT_FALSE(definition.addSlot(empty));
    EXPECT_FALSE(definition.setSlot(empty));

    // setSlot upserts: unknown id is appended, known id is replaced.
    ParametricCompositionSlot control;
    control.slotId = QStringLiteral("ctl");
    control.role = ParametricCompositionSlotRole::Control;
    EXPECT_TRUE(definition.setSlot(control));
    EXPECT_TRUE(definition.hasSlot(QStringLiteral("ctl")));
    EXPECT_EQ(definition.slotsByRole(ParametricCompositionSlotRole::Control).size(), 1);

    EXPECT_TRUE(definition.removeSlot(QStringLiteral("ctl")));
    EXPECT_FALSE(definition.removeSlot(QStringLiteral("ctl")));
    EXPECT_FALSE(definition.hasSlot(QStringLiteral("ctl")));

    definition.clearSlots();
    EXPECT_TRUE(definition.slotDefinitions().isEmpty());
    EXPECT_FALSE(definition.validate());
}

TEST(ProjectDefinitionTest, ValidateRejectsBadShapes)
{
    QString error;
    ParametricCompositionDefinition noId;
    EXPECT_FALSE(noId.validate(&error));
    EXPECT_FALSE(error.isEmpty());

    // Two input slots violate the exactly-one-input rule.
    ParametricCompositionDefinition twoInputs =
        makeDefaultParametricCompositionDefinition(QStringLiteral("comp.two"));
    ParametricCompositionSlot extra;
    extra.slotId = QStringLiteral("input2");
    extra.role = ParametricCompositionSlotRole::Input;
    EXPECT_TRUE(twoInputs.addSlot(extra));
    EXPECT_FALSE(twoInputs.validate(&error));
    EXPECT_FALSE(error.isEmpty());

    // Non-RGBA value types are rejected.
    ParametricCompositionDefinition nonRgba =
        makeDefaultParametricCompositionDefinition(QStringLiteral("comp.yuv"));
    ParametricCompositionSlot yuv;
    yuv.slotId = QStringLiteral("input");
    yuv.role = ParametricCompositionSlotRole::Input;
    yuv.valueType = QStringLiteral("YUV");
    EXPECT_TRUE(nonRgba.setSlot(yuv));
    EXPECT_FALSE(nonRgba.validate());
}

TEST(ProjectDefinitionTest, JsonRoundTripPreservesDefinition)
{
    const ParametricCompositionDefinition definition = makeSampleDefinition();
    const QJsonObject json = definition.toJson();
    ASSERT_FALSE(json.isEmpty());

    const ParametricCompositionDefinition restored =
        ParametricCompositionDefinition::fromJson(json);
    EXPECT_EQ(restored.definitionId(), QStringLiteral("comp.roundtrip"));
    EXPECT_EQ(restored.displayName(), QStringLiteral("Roundtrip"));
    ASSERT_EQ(restored.slotDefinitions().size(), 2);
    ASSERT_EQ(restored.parameters().size(), 1);
    EXPECT_EQ(restored.parameters().front().key, QStringLiteral("opacity"));
    EXPECT_EQ(restored.parameters().front().defaultValue.toDouble(), 1.0);
    ASSERT_EQ(restored.publishedControls().size(), 1);
    EXPECT_EQ(restored.publishedControls().front().controlId,
              QStringLiteral("ctl.opacity"));
    ASSERT_EQ(restored.dataBindings().size(), 1);
    EXPECT_EQ(restored.dataBindings().front().columnKey, QStringLiteral("shot"));
    EXPECT_TRUE(restored.validate());

    // Second round-trip must be stable.
    const ParametricCompositionDefinition twice =
        ParametricCompositionDefinition::fromJson(restored.toJson());
    EXPECT_EQ(twice.slotDefinitions().size(), 2);
    EXPECT_EQ(twice.parameters().front().key, QStringLiteral("opacity"));
}

TEST(ProjectDefinitionTest, FromJsonSkipsMalformedEntriesAndSupportsLegacySlots)
{
    QJsonObject legacy;
    legacy.insert(QStringLiteral("definitionId"), QStringLiteral("comp.legacy"));
    QJsonObject inputSlot;
    inputSlot.insert(QStringLiteral("slotId"), QStringLiteral("in"));
    inputSlot.insert(QStringLiteral("role"),
                     static_cast<int>(ParametricCompositionSlotRole::Input));
    QJsonObject outputSlot;
    outputSlot.insert(QStringLiteral("slotId"), QStringLiteral("out"));
    outputSlot.insert(QStringLiteral("role"),
                      static_cast<int>(ParametricCompositionSlotRole::Output));
    legacy.insert(QStringLiteral("inputSlots"),
                  QJsonArray{inputSlot, outputSlot,
                             QJsonValue(QStringLiteral("not an object")),
                             QJsonValue(42)});

    const ParametricCompositionDefinition restored =
        ParametricCompositionDefinition::fromJson(legacy);
    EXPECT_EQ(restored.definitionId(), QStringLiteral("comp.legacy"));
    ASSERT_EQ(restored.slotDefinitions().size(), 2);
    EXPECT_TRUE(restored.validate());

    const ParametricCompositionDefinition empty =
        ParametricCompositionDefinition::fromJson(QJsonObject{});
    EXPECT_TRUE(empty.definitionId().isEmpty());
    EXPECT_FALSE(empty.validate());
}

TEST(ProjectInstanceTest, ParameterOverridesHonorDefinition)
{
    ParametricCompositionDefinition definition = makeSampleDefinition();
    ParametricCompositionParameter locked;
    locked.key = QStringLiteral("locked");
    locked.defaultValue = 5.0;
    locked.overridableByInstance = false;
    ASSERT_TRUE(definition.addParameter(locked));

    const SharedPtr<const ParametricCompositionDefinition> shared =
        makeShared<ParametricCompositionDefinition>(definition);
    ParametricCompositionInstance instance(shared);

    EXPECT_EQ(instance.parameterValue(QStringLiteral("opacity"), 0.0).toDouble(), 1.0);
    instance.setParameterOverride(QStringLiteral("opacity"), 0.25);
    EXPECT_EQ(instance.parameterValue(QStringLiteral("opacity"), 0.0).toDouble(), 0.25);
    EXPECT_TRUE(instance.parameterOverrides().contains(QStringLiteral("opacity")));
    instance.clearParameterOverride(QStringLiteral("opacity"));
    EXPECT_EQ(instance.parameterValue(QStringLiteral("opacity"), 0.0).toDouble(), 1.0);

    // Unknown keys fall back, and locked parameters ignore overrides.
    EXPECT_EQ(instance.parameterValue(QStringLiteral("unknown"), 7.0).toDouble(), 7.0);
    instance.setParameterOverride(QStringLiteral("unknown"), 1.0);
    EXPECT_EQ(instance.parameterValue(QStringLiteral("unknown"), 7.0).toDouble(), 7.0);
    instance.setParameterOverride(QStringLiteral("locked"), 9.0);
    EXPECT_FALSE(instance.parameterOverrides().contains(QStringLiteral("locked")));
    EXPECT_EQ(instance.parameterValue(QStringLiteral("locked"), 0.0).toDouble(), 5.0);
    instance.setParameterOverride(QString(), 1.0);
    EXPECT_FALSE(instance.parameterOverrides().contains(QString()));
}

TEST(ProjectInstanceTest, PublishedControlsRouteThroughParameters)
{
    const SharedPtr<const ParametricCompositionDefinition> shared =
        makeShared<ParametricCompositionDefinition>(makeSampleDefinition());
    ParametricCompositionInstance instance(shared);

    EXPECT_EQ(instance.publishedControlValue(QStringLiteral("ctl.opacity"), 0.0).toDouble(),
              1.0);
    EXPECT_EQ(instance.publishedControlValue(QStringLiteral("missing"), 3.0).toDouble(),
              3.0);
    instance.setPublishedControlOverride(QStringLiteral("ctl.opacity"), 0.5);
    EXPECT_EQ(instance.parameterValue(QStringLiteral("opacity"), 0.0).toDouble(), 0.5);
    instance.clearPublishedControlOverride(QStringLiteral("ctl.opacity"));
    EXPECT_EQ(instance.parameterValue(QStringLiteral("opacity"), 0.0).toDouble(), 1.0);

    QVariantMap row{{QStringLiteral("shot"), QStringLiteral("A001")}};
    instance.applyDataRow(row);
    EXPECT_EQ(instance.dataRowValues(), row);
}

TEST(ProjectInstanceTest, JsonRoundTripPreservesOverridesAndBindings)
{
    const SharedPtr<const ParametricCompositionDefinition> shared =
        makeShared<ParametricCompositionDefinition>(makeSampleDefinition());
    ParametricCompositionInstance instance(shared);
    instance.setParameterOverride(QStringLiteral("opacity"), 0.75);
    instance.addInputBinding(ParametricCompositionInputBinding::fromText(
        QStringLiteral("title"), QStringLiteral("input")));
    ASSERT_EQ(instance.inputBindingCount(), 1);
    EXPECT_TRUE(instance.hasAnyInputConnected());

    ParametricCompositionInstance restored =
        ParametricCompositionInstance::fromJson(instance.toJson(), shared);
    EXPECT_EQ(restored.parameterValue(QStringLiteral("opacity"), 0.0).toDouble(), 0.75);
    ASSERT_EQ(restored.inputBindingCount(), 1);
    EXPECT_TRUE(restored.isInputConnected(0));
    EXPECT_TRUE(restored.hasAnyInputConnected());

    // Out-of-range binding access is inert.
    EXPECT_FALSE(restored.isInputConnected(7));
    restored.setInputBinding(7, ParametricCompositionInputBinding());
    restored.removeInputBinding(7);
    EXPECT_EQ(restored.inputBindingCount(), 1);
    restored.clearInputBindings();
    EXPECT_EQ(restored.inputBindingCount(), 0);
    EXPECT_FALSE(restored.hasAnyInputConnected());
}

TEST(ProjectInstanceTest, EvaluateFallsBackToTransparentWithoutInputs)
{
    const SharedPtr<const ParametricCompositionDefinition> shared =
        makeShared<ParametricCompositionDefinition>(
            makeDefaultParametricCompositionDefinition(QStringLiteral("comp.eval")));
    const ParametricCompositionInstance instance(shared);

    ParametricCompositionRenderContext context;
    context.timeSeconds = 2.5;
    const ParametricCompositionEvaluation evaluation =
        instance.evaluate(context, ParametricCompositionInputResolver{});
    EXPECT_FALSE(evaluation.inputResolved);
    EXPECT_TRUE(evaluation.usedTransparentFallback);
    EXPECT_TRUE(evaluation.cacheKey.isValid());
    EXPECT_EQ(evaluation.cacheKey.definitionId, QStringLiteral("comp.eval"));
    EXPECT_EQ(evaluation.cacheKey.timeKey, context.timeKey());
    EXPECT_EQ(context.timeKey(), 2500000);
}

TEST(ProjectBindingTest, ConnectionAndCycleRules)
{
    const ParametricCompositionInputBinding emptyText =
        ParametricCompositionInputBinding::fromText(QString(), QStringLiteral("s"));
    EXPECT_FALSE(emptyText.isConnected());

    const ParametricCompositionInputBinding text =
        ParametricCompositionInputBinding::fromText(QStringLiteral("hi"),
                                                   QStringLiteral("s"));
    EXPECT_TRUE(text.isConnected());
    EXPECT_EQ(text.text, QStringLiteral("hi"));

    const ParametricCompositionInputBinding nilLayer =
        ParametricCompositionInputBinding::fromSourceLayer(
            LayerID(LayerID::Nil()), QStringLiteral("s"));
    EXPECT_FALSE(nilLayer.isConnected());

    const ParametricCompositionInputBinding liveLayer =
        ParametricCompositionInputBinding::fromSourceLayer(
            LayerID(), QStringLiteral("s"), QStringLiteral("comp.self"),
            QStringList{QStringLiteral("comp.upstream")});
    EXPECT_TRUE(liveLayer.isConnected());
    EXPECT_TRUE(liveLayer.wouldCreateCycle(QStringLiteral("comp.self")));
    EXPECT_TRUE(liveLayer.wouldCreateCycle(QStringLiteral("comp.upstream")));
    EXPECT_FALSE(liveLayer.wouldCreateCycle(QStringLiteral("comp.other")));
    EXPECT_FALSE(liveLayer.wouldCreateCycle(QString()));

    QJsonObject json = liveLayer.toJson();
    const ParametricCompositionInputBinding restored =
        ParametricCompositionInputBinding::fromJson(json);
    EXPECT_EQ(restored.sourceDefinitionId, QStringLiteral("comp.self"));
    EXPECT_EQ(restored.upstreamDefinitionIds,
              QStringList{QStringLiteral("comp.upstream")});
    EXPECT_TRUE(restored.isConnected());

    // Image/Matte bindings without pixel payloads disconnect on load.
    QJsonObject imageClaim;
    imageClaim.insert(QStringLiteral("kind"),
                      static_cast<int>(ParametricCompositionSlotKind::Image));
    imageClaim.insert(QStringLiteral("connected"), true);
    imageClaim.insert(QStringLiteral("hasImage"), false);
    EXPECT_FALSE(
        ParametricCompositionInputBinding::fromJson(imageClaim).isConnected());
}

TEST(ProjectBundleTest, BundleRoundTripPreservesProject)
{
    const ParametricCompositionDefinition definition = makeSampleDefinition();
    const SharedPtr<const ParametricCompositionDefinition> shared =
        makeShared<ParametricCompositionDefinition>(definition);
    ParametricCompositionInstance instance(shared);
    instance.setParameterOverride(QStringLiteral("opacity"), 0.5);

    ParametricCompositionBundle bundle;
    bundle.bundleTitle = QStringLiteral("Shot bundle");
    bundle.definition = definition.toJson();
    bundle.instance = instance.toJson();
    bundle.metadata.insert(QStringLiteral("author"), QStringLiteral("test"));

    const ParametricCompositionBundle restored =
        ParametricCompositionBundle::fromJson(bundle.toJson());
    EXPECT_EQ(restored.bundleKind, QStringLiteral("parametric-composition"));
    EXPECT_EQ(restored.bundleTitle, QStringLiteral("Shot bundle"));
    EXPECT_EQ(restored.definition.value(QStringLiteral("definitionId")).toString(),
              QStringLiteral("comp.roundtrip"));
    EXPECT_TRUE(restored.instance.contains(QStringLiteral("parameterOverrides")));
    EXPECT_EQ(restored.metadata.value(QStringLiteral("author")).toString(),
              QStringLiteral("test"));

    const ParametricCompositionDefinition restoredDefinition =
        ParametricCompositionDefinition::fromJson(restored.definition);
    EXPECT_TRUE(restoredDefinition.validate());
    const ParametricCompositionInstance restoredInstance =
        ParametricCompositionInstance::fromJson(restored.instance, shared);
    EXPECT_EQ(restoredInstance.parameterValue(QStringLiteral("opacity"), 0.0).toDouble(),
              0.5);

    const QJsonObject compositionJson =
        parametricCompositionBundleToCompositionJson(restored);
    EXPECT_FALSE(compositionJson.isEmpty());
    EXPECT_TRUE(parametricCompositionBundleToCompositionJson(
                    ParametricCompositionBundle{}).isEmpty());
}
