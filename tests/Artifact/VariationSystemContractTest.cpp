#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSize>
#include <QString>
#include <QVariant>

#include <utility>

import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Layer.Abstract;
import Artifact.Layer.Factory;
import Artifact.Layer.InitParams;
import Utils.Id;

namespace {

QCoreApplication* ensureApplication()
{
    static int argc = 1;
    static char appName[] = "ArtifactVariationSystemContractTest";
    static char* argv[] = {appName, nullptr};
    static QCoreApplication app(argc, argv);
    return &app;
}

Artifact::ArtifactAbstractLayerPtr makeSolidLayer()
{
    ensureApplication();
    Artifact::ArtifactSolidLayerInitParams params(QStringLiteral("Variant test"));
    params.setWidth(64);
    params.setHeight(64);
    Artifact::ArtifactLayerFactory factory;
    return factory.createNewLayer(params);
}

struct CompositionLayerFixture {
    QCoreApplication* app = ensureApplication();
    Artifact::ArtifactCompositionInitParams params;
    Artifact::ArtifactAbstractComposition composition{
        ArtifactCore::CompositionID{}, params};
    Artifact::ArtifactAbstractLayerPtr layer = makeSolidLayer();
    bool layerAttached = layer && composition.appendLayerTop(layer).success;
};

Artifact::CompositionStateVariant opacityState(
    const QString& stateId, const ArtifactCore::LayerID& layerId,
    double value, double baseline)
{
    Artifact::CompositionStateVariant state;
    state.stateId = stateId;
    state.displayName = stateId;

    Artifact::CompositionStatePropertyOverride overrideItem;
    overrideItem.layerId = layerId;
    overrideItem.propertyPath = QStringLiteral("layer.opacity");
    overrideItem.value = value;
    overrideItem.baselineValue = baseline;
    state.overrides.append(overrideItem);
    return state;
}

} // namespace

TEST(ArtifactLayerVariantContractTest, BaseAndDerivedOpacityRemainIndependent)
{
    auto layer = makeSolidLayer();
    ASSERT_TRUE(layer);
    ASSERT_EQ(layer->getVariants().size(), 1u);
    EXPECT_EQ(layer->getActiveVariantIndex(), 0u);

    layer->setOpacity(0.8f);
    auto* derived = layer->createVariantFromCurrent("Mobile");
    ASSERT_NE(derived, nullptr);
    ASSERT_EQ(layer->getVariants().size(), 2u);

    layer->setActiveVariant(1);
    layer->setOpacity(0.35f);
    EXPECT_FLOAT_EQ(layer->opacity(), 0.35f);
    EXPECT_TRUE(Artifact::HasFlag(
        derived->overrideFlags_, Artifact::VariantOverrideFlags::Opacity));

    layer->setActiveVariant(0);
    EXPECT_FLOAT_EQ(layer->opacity(), 0.8f);
    layer->setActiveVariant(1);
    EXPECT_FLOAT_EQ(layer->opacity(), 0.35f);
}

TEST(ArtifactLayerVariantContractTest, InvalidActivationAndBaseResetAreNoOps)
{
    auto layer = makeSolidLayer();
    ASSERT_TRUE(layer);
    layer->setOpacity(0.6f);
    layer->setActiveVariant(999);
    EXPECT_EQ(layer->getActiveVariantIndex(), 0u);
    layer->resetVariantOverride();
    EXPECT_FLOAT_EQ(layer->opacity(), 0.6f);
}

TEST(ArtifactLayerVariantContractTest, DerivedVariantInheritsBaseUntilItHasAnOverride)
{
    auto layer = makeSolidLayer();
    ASSERT_TRUE(layer);
    layer->setOpacity(0.8f);
    ASSERT_NE(layer->createVariantFromCurrent("Mobile"), nullptr);

    layer->setActiveVariant(1);
    EXPECT_FLOAT_EQ(layer->opacity(), 0.8f);

    layer->setActiveVariant(0);
    layer->setOpacity(0.55f);
    layer->setActiveVariant(1);
    EXPECT_FLOAT_EQ(layer->opacity(), 0.55f);
}

TEST(ArtifactLayerVariantContractTest, ResettingOneOverridePreservesOtherOverrides)
{
    auto layer = makeSolidLayer();
    ASSERT_TRUE(layer);
    layer->setOpacity(0.8f);
    const auto baseBlend = layer->layerBlendType();
    ASSERT_NE(layer->createVariantFromCurrent("Mobile"), nullptr);

    layer->setActiveVariant(1);
    layer->setOpacity(0.35f);
    layer->setBlendMode(ArtifactCore::LAYER_BLEND_TYPE::BLEND_MULTIPLY);
    layer->resetVariantOverride(Artifact::VariantOverrideFlags::Opacity);

    EXPECT_FLOAT_EQ(layer->opacity(), 0.8f);
    EXPECT_EQ(layer->layerBlendType(), ArtifactCore::LAYER_BLEND_TYPE::BLEND_MULTIPLY);
    const auto* active = layer->getActiveVariant();
    ASSERT_NE(active, nullptr);
    EXPECT_FALSE(Artifact::HasFlag(
        active->overrideFlags_, Artifact::VariantOverrideFlags::Opacity));
    EXPECT_TRUE(Artifact::HasFlag(
        active->overrideFlags_, Artifact::VariantOverrideFlags::BlendMode));

    layer->resetVariantOverride(Artifact::VariantOverrideFlags::BlendMode);
    EXPECT_EQ(layer->layerBlendType(), baseBlend);
    EXPECT_EQ(active->overrideFlags_, Artifact::VariantOverrideFlags::None);
}

TEST(ArtifactLayerVariantContractTest, ExtractAndInsertKeepTheActiveVariantIdentity)
{
    auto layer = makeSolidLayer();
    ASSERT_TRUE(layer);
    ASSERT_NE(layer->createVariantFromCurrent("B"), nullptr);
    auto* variantC = layer->createVariantFromCurrent("C");
    ASSERT_NE(variantC, nullptr);
    layer->setActiveVariant(2);

    auto extracted = layer->extractVariant(1);
    ASSERT_NE(extracted, nullptr);
    EXPECT_EQ(layer->getActiveVariantIndex(), 1u);
    EXPECT_EQ(layer->getActiveVariant(), variantC);
    EXPECT_EQ(layer->getVariants().size(), 2u);

    layer->insertVariant(0, std::move(extracted));
    EXPECT_EQ(layer->getActiveVariantIndex(), 2u);
    EXPECT_EQ(layer->getActiveVariant(), variantC);
    EXPECT_EQ(layer->getVariants().size(), 3u);
    EXPECT_EQ(layer->extractVariant(99), nullptr);
}

TEST(ArtifactLayerVariantContractTest, JsonRoundTripPreservesActiveOverrideAndBaseValue)
{
    auto layer = makeSolidLayer();
    ASSERT_TRUE(layer);
    layer->setOpacity(0.8f);
    ASSERT_NE(layer->createVariantFromCurrent("Mobile"), nullptr);
    layer->setActiveVariant(1);
    layer->setOpacity(0.35f);

    const auto restored = Artifact::ArtifactLayerFactory::createFromJson(layer->toJson());
    ASSERT_TRUE(restored);
    ASSERT_EQ(restored->getVariants().size(), 2u);
    EXPECT_EQ(restored->getActiveVariantIndex(), 1u);
    EXPECT_FLOAT_EQ(restored->opacity(), 0.35f);

    restored->setActiveVariant(0);
    EXPECT_FLOAT_EQ(restored->opacity(), 0.8f);
}

TEST(ArtifactCompositionResponsiveVariantContractTest, JsonRoundTripPreservesVariantsAndActiveSelection)
{
    Artifact::ResponsiveLayoutSet layout;
    layout.activeVariantId = QStringLiteral("vertical");
    layout.defaultPolicy = QStringLiteral("manual");

    Artifact::ResponsiveLayoutVariant wide;
    wide.variantId = QStringLiteral("wide");
    wide.displayName = QStringLiteral("Wide");
    wide.baseSize = QSize(1920, 1080);
    wide.aspectRatio = 16.0 / 9.0;

    Artifact::ResponsiveLayoutVariant vertical;
    vertical.variantId = QStringLiteral("vertical");
    vertical.displayName = QStringLiteral("Vertical");
    vertical.baseSize = QSize(1080, 1920);
    vertical.aspectRatio = 9.0 / 16.0;
    vertical.layoutRules.insert(QStringLiteral("safeAreaMode"), QStringLiteral("title"));

    layout.variants.append(wide);
    layout.variants.append(vertical);

    const auto restored = Artifact::ResponsiveLayoutSet::fromJson(layout.toJson());
    ASSERT_EQ(restored.variants.size(), 2);
    EXPECT_EQ(restored.activeVariantId, QStringLiteral("vertical"));
    EXPECT_TRUE(restored.hasVariant(QStringLiteral("wide")));
    EXPECT_EQ(restored.variants.at(1).baseSize, QSize(1080, 1920));
    EXPECT_EQ(restored.variants.at(1).layoutRules.value(QStringLiteral("safeAreaMode")).toString(),
              QStringLiteral("title"));
}

TEST(ArtifactCompositionResponsiveVariantContractTest, CompositionRejectsUnknownActiveVariant)
{
    ensureApplication();
    Artifact::ArtifactCompositionInitParams params;
    Artifact::ArtifactAbstractComposition composition(
        ArtifactCore::CompositionID{}, params);

    Artifact::ResponsiveLayoutSet layout;
    Artifact::ResponsiveLayoutVariant wide;
    wide.variantId = QStringLiteral("wide");
    wide.baseSize = QSize(1920, 1080);
    Artifact::ResponsiveLayoutVariant vertical;
    vertical.variantId = QStringLiteral("vertical");
    vertical.baseSize = QSize(1080, 1920);
    layout.variants.append(wide);
    layout.variants.append(vertical);
    layout.activeVariantId = QStringLiteral("wide");

    composition.setResponsiveLayout(layout);
    EXPECT_EQ(composition.effectiveCompositionSize(), QSize(1920, 1080));
    composition.setActiveResponsiveLayoutVariantId(QStringLiteral("vertical"));
    EXPECT_EQ(composition.activeResponsiveLayoutVariantId(), QStringLiteral("vertical"));
    EXPECT_EQ(composition.effectiveCompositionSize(), QSize(1080, 1920));
    composition.setActiveResponsiveLayoutVariantId(QStringLiteral("missing"));
    EXPECT_EQ(composition.activeResponsiveLayoutVariantId(), QStringLiteral("vertical"));
    EXPECT_EQ(composition.effectiveCompositionSize(), QSize(1080, 1920));
}

TEST(ArtifactCompositionResponsiveVariantContractTest, MissingActiveVariantFallsBackToFirstVariant)
{
    QJsonObject serialized{
        {QStringLiteral("activeVariantId"), QStringLiteral("removed")},
        {QStringLiteral("variants"), QJsonArray{
            QJsonObject{
                {QStringLiteral("variantId"), QStringLiteral("primary")},
                {QStringLiteral("displayName"), QStringLiteral("Primary")},
                {QStringLiteral("width"), 1280},
                {QStringLiteral("height"), 720},
            },
        }},
    };

    const auto restored = Artifact::ResponsiveLayoutSet::fromJson(serialized);
    ASSERT_FALSE(restored.variants.isEmpty());
    EXPECT_EQ(restored.activeVariantId, QStringLiteral("primary"));
}

TEST(ArtifactCompositionResponsiveVariantContractTest, EmptyLayoutRestoresOneDefaultVariant)
{
    ensureApplication();
    Artifact::ArtifactCompositionInitParams params;
    Artifact::ArtifactAbstractComposition composition(
        ArtifactCore::CompositionID{}, params);

    Artifact::ResponsiveLayoutSet layout;
    Artifact::ResponsiveLayoutVariant vertical;
    vertical.variantId = QStringLiteral("vertical");
    vertical.baseSize = QSize(1080, 1920);
    layout.variants.append(vertical);
    layout.activeVariantId = QStringLiteral("vertical");
    composition.setResponsiveLayout(layout);

    composition.setResponsiveLayout(Artifact::ResponsiveLayoutSet{});
    const auto variants = composition.responsiveLayoutVariants();
    ASSERT_EQ(variants.size(), 1);
    EXPECT_FALSE(variants.front().variantId.isEmpty());
    EXPECT_EQ(composition.activeResponsiveLayoutVariantId(), variants.front().variantId);
    EXPECT_EQ(composition.effectiveCompositionSize(), QSize(1080, 1920));
}

TEST(ArtifactCompositionStateVariantContractTest, ActivationAndDeactivationApplyAndRestoreOpacity)
{
    CompositionLayerFixture fixture;
    ASSERT_TRUE(fixture.layerAttached);
    fixture.layer->setOpacity(0.8f);

    auto sale = opacityState(QStringLiteral("sale"), fixture.layer->id(), 0.35, 0.8);
    fixture.composition.setStateVariants({sale});

    EXPECT_TRUE(fixture.composition.setActiveStateVariantId(QStringLiteral(" sale ")));
    EXPECT_EQ(fixture.composition.activeStateVariantId(), QStringLiteral("sale"));
    EXPECT_FLOAT_EQ(fixture.layer->opacity(), 0.35f);

    EXPECT_TRUE(fixture.composition.setActiveStateVariantId(QString{}));
    EXPECT_TRUE(fixture.composition.activeStateVariantId().isEmpty());
    EXPECT_FLOAT_EQ(fixture.layer->opacity(), 0.8f);
}

TEST(ArtifactCompositionStateVariantContractTest, StateTransitionKeepsSharedOverrideUntilNextValueApplies)
{
    CompositionLayerFixture fixture;
    ASSERT_TRUE(fixture.layerAttached);
    fixture.layer->setOpacity(0.8f);

    const auto first = opacityState(QStringLiteral("first"), fixture.layer->id(), 0.25, 0.8);
    const auto second = opacityState(QStringLiteral("second"), fixture.layer->id(), 0.6, 0.8);
    fixture.composition.setStateVariants({first, second});

    ASSERT_TRUE(fixture.composition.setActiveStateVariantId(QStringLiteral("first")));
    EXPECT_FLOAT_EQ(fixture.layer->opacity(), 0.25f);
    ASSERT_TRUE(fixture.composition.setActiveStateVariantId(QStringLiteral("second")));
    EXPECT_FLOAT_EQ(fixture.layer->opacity(), 0.6f);
    ASSERT_TRUE(fixture.composition.setActiveStateVariantId(QString{}));
    EXPECT_FLOAT_EQ(fixture.layer->opacity(), 0.8f);
}

TEST(ArtifactCompositionStateVariantContractTest, DisabledOrUnknownStateIsRejectedAtomically)
{
    CompositionLayerFixture fixture;
    ASSERT_TRUE(fixture.layerAttached);
    fixture.layer->setOpacity(0.8f);

    auto disabled = opacityState(QStringLiteral("disabled"), fixture.layer->id(), 0.1, 0.8);
    disabled.enabled = false;
    fixture.composition.setStateVariants({disabled});

    EXPECT_FALSE(fixture.composition.setActiveStateVariantId(QStringLiteral("disabled")));
    EXPECT_FALSE(fixture.composition.setActiveStateVariantId(QStringLiteral("missing")));
    EXPECT_TRUE(fixture.composition.activeStateVariantId().isEmpty());
    EXPECT_FLOAT_EQ(fixture.layer->opacity(), 0.8f);
}

TEST(ArtifactCompositionStateVariantContractTest, StateIdentifiersAreUniqueAndComparisonPairIsValidated)
{
    CompositionLayerFixture fixture;
    ASSERT_TRUE(fixture.layerAttached);

    Artifact::CompositionStateVariant first;
    first.stateId = QStringLiteral("client");
    first.displayName = QStringLiteral("Client");
    Artifact::CompositionStateVariant duplicate = first;
    Artifact::CompositionStateVariant unnamed;

    fixture.composition.setStateVariants({first, duplicate, unnamed});
    const auto states = fixture.composition.stateVariants();
    ASSERT_EQ(states.size(), 3);
    EXPECT_EQ(states.at(0).stateId, QStringLiteral("client"));
    EXPECT_EQ(states.at(1).stateId, QStringLiteral("client_2"));
    EXPECT_EQ(states.at(2).stateId, QStringLiteral("state"));
    EXPECT_EQ(states.at(2).displayName, QStringLiteral("state"));

    EXPECT_FALSE(fixture.composition.setStateComparisonPair(
        QStringLiteral("client"), QStringLiteral("missing")));
    EXPECT_TRUE(fixture.composition.stateComparisonAId().isEmpty());
    EXPECT_TRUE(fixture.composition.setStateComparisonPair(
        QStringLiteral("client"), QStringLiteral("client_2")));
    EXPECT_EQ(fixture.composition.stateComparisonAId(), QStringLiteral("client"));
    EXPECT_EQ(fixture.composition.stateComparisonBId(), QStringLiteral("client_2"));
}

TEST(ArtifactCompositionStateVariantContractTest, StateAndOverrideJsonRoundTripPreservesValues)
{
    const ArtifactCore::LayerID layerId(
        QStringLiteral("00000000-0000-0000-0000-000000000123"));
    auto state = opacityState(QStringLiteral("mobile"), layerId, 0.4, 0.9);
    state.enabled = false;
    state.overrides.front().enabled = false;

    const auto restored = Artifact::CompositionStateVariant::fromJson(state.toJson());
    ASSERT_EQ(restored.stateId, QStringLiteral("mobile"));
    EXPECT_FALSE(restored.enabled);
    ASSERT_EQ(restored.overrides.size(), 1);
    EXPECT_EQ(restored.overrides.front().layerId, layerId);
    EXPECT_EQ(restored.overrides.front().propertyPath, QStringLiteral("layer.opacity"));
    EXPECT_DOUBLE_EQ(restored.overrides.front().value.toDouble(), 0.4);
    EXPECT_DOUBLE_EQ(restored.overrides.front().baselineValue.toDouble(), 0.9);
    EXPECT_FALSE(restored.overrides.front().enabled);
    EXPECT_TRUE(restored.hasOverride(layerId, QStringLiteral("layer.opacity")));
    EXPECT_FALSE(restored.hasOverride(layerId, QStringLiteral("layer.visible")));
}

TEST(ArtifactCompositionStateVariantContractTest, CompositionJsonRoundTripPreservesActiveStateAndResponsiveLayout)
{
    CompositionLayerFixture fixture;
    ASSERT_TRUE(fixture.layerAttached);
    fixture.layer->setOpacity(0.8f);

    auto state = opacityState(QStringLiteral("mobile"), fixture.layer->id(), 0.4, 0.8);
    auto comparisonState = opacityState(QStringLiteral("desktop"), fixture.layer->id(), 0.9, 0.8);
    fixture.composition.setStateVariants({state, comparisonState});
    ASSERT_TRUE(fixture.composition.setStateComparisonPair(
        QStringLiteral("mobile"), QStringLiteral("desktop")));
    ASSERT_TRUE(fixture.composition.setActiveStateVariantId(QStringLiteral("mobile")));

    Artifact::ResponsiveLayoutSet layout;
    Artifact::ResponsiveLayoutVariant wide;
    wide.variantId = QStringLiteral("wide");
    wide.baseSize = QSize(1920, 1080);
    Artifact::ResponsiveLayoutVariant vertical;
    vertical.variantId = QStringLiteral("vertical");
    vertical.baseSize = QSize(1080, 1920);
    layout.variants.append(wide);
    layout.variants.append(vertical);
    layout.activeVariantId = QStringLiteral("vertical");
    fixture.composition.setResponsiveLayout(layout);

    const auto restored = Artifact::ArtifactAbstractComposition::fromJson(
        fixture.composition.toJson());
    ASSERT_TRUE(restored);
    EXPECT_EQ(restored->activeStateVariantId(), QStringLiteral("mobile"));
    EXPECT_EQ(restored->stateComparisonAId(), QStringLiteral("mobile"));
    EXPECT_EQ(restored->stateComparisonBId(), QStringLiteral("desktop"));
    ASSERT_EQ(restored->stateVariants().size(), 2);
    EXPECT_EQ(restored->activeResponsiveLayoutVariantId(), QStringLiteral("vertical"));
    EXPECT_EQ(restored->effectiveCompositionSize(), QSize(1080, 1920));

    const auto restoredLayer = restored->layerById(fixture.layer->id());
    ASSERT_TRUE(restoredLayer);
    EXPECT_FLOAT_EQ(restoredLayer->opacity(), 0.4f);
}

TEST(ArtifactCompositionStateVariantContractTest, CompositionJsonClearsUnknownActiveAndComparisonIds)
{
    CompositionLayerFixture fixture;
    ASSERT_TRUE(fixture.layerAttached);
    fixture.composition.setStateVariants(
        {opacityState(QStringLiteral("known"), fixture.layer->id(), 0.3, 1.0)});

    auto serialized = fixture.composition.toJson();
    auto object = serialized.object();
    object.insert(QStringLiteral("activeStateVariantId"), QStringLiteral("removed"));
    object.insert(QStringLiteral("stateComparisonAId"), QStringLiteral("known"));
    object.insert(QStringLiteral("stateComparisonBId"), QStringLiteral("removed"));
    serialized.setObject(object);

    const auto restored = Artifact::ArtifactAbstractComposition::fromJson(serialized);
    ASSERT_TRUE(restored);
    EXPECT_TRUE(restored->activeStateVariantId().isEmpty());
    EXPECT_EQ(restored->stateComparisonAId(), QStringLiteral("known"));
    EXPECT_TRUE(restored->stateComparisonBId().isEmpty());
}
