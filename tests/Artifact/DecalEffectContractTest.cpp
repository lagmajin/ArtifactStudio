#include <gtest/gtest.h>

#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Decal;
import Utils.String.UniString;

TEST(DecalEffectContractTest, DirectSettersClampOpacityAndNormalFade)
{
    Artifact::DecalEffect effect;
    effect.setOpacity(-1.0f);
    EXPECT_FLOAT_EQ(effect.opacity(), 0.0f);
    effect.setOpacity(2.0f);
    EXPECT_FLOAT_EQ(effect.opacity(), 1.0f);

    effect.setNormalFade(-1.0f);
    EXPECT_FLOAT_EQ(effect.normalFade(), 0.0f);
    effect.setNormalFade(2.0f);
    EXPECT_FLOAT_EQ(effect.normalFade(), 1.0f);
}

TEST(DecalEffectContractTest, PropertiesExposeExpectedEditableRanges)
{
    const Artifact::DecalEffect effect;
    const auto properties = effect.getProperties();
    ASSERT_EQ(properties.size(), 2u);
    EXPECT_EQ(properties[0].name(), QStringLiteral("Opacity"));
    EXPECT_DOUBLE_EQ(properties[0].minValue().toDouble(), 0.0);
    EXPECT_DOUBLE_EQ(properties[0].maxValue().toDouble(), 1.0);
    EXPECT_EQ(properties[1].name(), QStringLiteral("NormalFade"));
    EXPECT_DOUBLE_EQ(properties[1].minValue().toDouble(), 0.0);
    EXPECT_DOUBLE_EQ(properties[1].maxValue().toDouble(), 1.0);
}

TEST(DecalEffectContractTest, PropertyValuesAndUnknownNamesAreStable)
{
    Artifact::DecalEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString("Opacity"), QVariant(0.25));
    effect.setPropertyValue(ArtifactCore::UniString("NormalFade"), QVariant(0.75));
    EXPECT_FLOAT_EQ(effect.opacity(), 0.25f);
    EXPECT_FLOAT_EQ(effect.normalFade(), 0.75f);

    effect.setPropertyValue(ArtifactCore::UniString("Unknown"), QVariant(0.0));
    EXPECT_FLOAT_EQ(effect.opacity(), 0.25f);
    EXPECT_FLOAT_EQ(effect.normalFade(), 0.75f);
}
