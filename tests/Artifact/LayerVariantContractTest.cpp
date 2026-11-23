#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QJsonObject>
#include <QString>

import Artifact.Layer.Factory;
import Artifact.Layer.Abstract;
import Artifact.Layer.InitParams;

namespace {

QCoreApplication* ensureApplication()
{
    static int argc = 1;
    static char appName[] = "ArtifactLayerVariantContractTest";
    static char* argv[] = {appName, nullptr};
    static QCoreApplication app(argc, argv);
    return &app;
}

Artifact::ArtifactAbstractLayerPtr makeLayer()
{
    ensureApplication();
    Artifact::ArtifactSolidLayerInitParams params(QStringLiteral("Variant contract"));
    params.setWidth(32);
    params.setHeight(32);
    Artifact::ArtifactLayerFactory factory;
    return factory.createNewLayer(params);
}

} // namespace

TEST(ArtifactLayerVariantContractTest, StartsWithOneBaseVariant)
{
    auto layer = makeLayer();
    ASSERT_TRUE(layer);

    ASSERT_EQ(layer->getVariants().size(), 1u);
    EXPECT_EQ(layer->getActiveVariantIndex(), 0u);
    EXPECT_NE(layer->getActiveVariant(), nullptr);
}

TEST(ArtifactLayerVariantContractTest, DerivedVariantInheritsBaseUntilEdited)
{
    auto layer = makeLayer();
    ASSERT_TRUE(layer);
    layer->setOpacity(0.72f);

    auto* variant = layer->createVariantFromCurrent("Mobile");
    ASSERT_NE(variant, nullptr);
    ASSERT_EQ(layer->getVariants().size(), 2u);

    layer->setActiveVariant(1);
    EXPECT_FLOAT_EQ(layer->opacity(), 0.72f);
    layer->setOpacity(0.31f);
    EXPECT_FLOAT_EQ(layer->opacity(), 0.31f);

    layer->setActiveVariant(0);
    EXPECT_FLOAT_EQ(layer->opacity(), 0.72f);
}

TEST(ArtifactLayerVariantContractTest, InvalidActivationLeavesCurrentVariantUnchanged)
{
    auto layer = makeLayer();
    ASSERT_TRUE(layer);
    ASSERT_NE(layer->createVariantFromCurrent("Mobile"), nullptr);
    layer->setActiveVariant(1);

    layer->setActiveVariant(99);

    EXPECT_EQ(layer->getActiveVariantIndex(), 1u);
    EXPECT_NE(layer->getActiveVariant(), nullptr);
}

TEST(ArtifactLayerVariantContractTest, JsonRoundTripKeepsActiveVariantAndOpacityOverride)
{
    auto layer = makeLayer();
    ASSERT_TRUE(layer);
    layer->setOpacity(0.8f);
    ASSERT_NE(layer->createVariantFromCurrent("Mobile"), nullptr);
    layer->setActiveVariant(1);
    layer->setOpacity(0.4f);

    auto restored = Artifact::ArtifactLayerFactory::createFromJson(layer->toJson());
    ASSERT_TRUE(restored);
    ASSERT_EQ(restored->getVariants().size(), 2u);
    EXPECT_EQ(restored->getActiveVariantIndex(), 1u);
    EXPECT_FLOAT_EQ(restored->opacity(), 0.4f);

    restored->setActiveVariant(0);
    EXPECT_FLOAT_EQ(restored->opacity(), 0.8f);
}

TEST(ArtifactLayerVariantContractTest, InvalidSerializedActiveIndexFallsBackToBase)
{
    auto layer = makeLayer();
    ASSERT_TRUE(layer);
    ASSERT_NE(layer->createVariantFromCurrent("Mobile"), nullptr);

    auto serialized = layer->toJson();
    serialized.insert(QStringLiteral("activeVariantIndex"), -1);

    auto restored = Artifact::ArtifactLayerFactory::createFromJson(serialized);
    ASSERT_TRUE(restored);
    EXPECT_EQ(restored->getActiveVariantIndex(), 0u);
    EXPECT_NE(restored->getActiveVariant(), nullptr);
}
