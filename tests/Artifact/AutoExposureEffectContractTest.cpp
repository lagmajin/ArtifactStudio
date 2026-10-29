#include <gtest/gtest.h>

#include <QVariant>

import Artifact.Effect.Abstract;
import AutoExposureEffect;
import Utils.String.UniString;

TEST(AutoExposureEffectContractTest, DefaultsMatchThePublishedPropertySurface)
{
    const Artifact::AutoExposureEffect effect;
    const auto properties = effect.getProperties();
    ASSERT_EQ(properties.size(), 5u);
    EXPECT_FLOAT_EQ(effect.middleGray(), 0.18f);
    EXPECT_FLOAT_EQ(effect.minExposure(), -8.0f);
    EXPECT_FLOAT_EQ(effect.maxExposure(), 8.0f);
    EXPECT_FLOAT_EQ(effect.adaptationSpeed(), 1.5f);
    EXPECT_TRUE(effect.isEnabled());
    EXPECT_EQ(properties[0].getName(), QStringLiteral("MiddleGray"));
    EXPECT_EQ(properties[1].getName(), QStringLiteral("MinExposure"));
    EXPECT_EQ(properties[2].getName(), QStringLiteral("MaxExposure"));
    EXPECT_EQ(properties[3].getName(), QStringLiteral("AdaptationSpeed"));
    EXPECT_EQ(properties[4].getName(), QStringLiteral("Enabled"));
}

TEST(AutoExposureEffectContractTest, DirectSettersClampDocumentedRanges)
{
    Artifact::AutoExposureEffect effect;
    effect.setMiddleGray(0.0f);
    EXPECT_FLOAT_EQ(effect.middleGray(), 0.01f);
    effect.setMiddleGray(2.0f);
    EXPECT_FLOAT_EQ(effect.middleGray(), 1.0f);
    effect.setAdaptationSpeed(0.0f);
    EXPECT_FLOAT_EQ(effect.adaptationSpeed(), 0.01f);
    effect.setMinExposure(-12.0f);
    EXPECT_FLOAT_EQ(effect.minExposure(), -12.0f);
    effect.setMaxExposure(12.0f);
    EXPECT_FLOAT_EQ(effect.maxExposure(), 12.0f);
}

TEST(AutoExposureEffectContractTest, PropertyUpdatesUseStableNames)
{
    Artifact::AutoExposureEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString("MiddleGray"), QVariant(0.32));
    effect.setPropertyValue(ArtifactCore::UniString("MinExposure"), QVariant(-3.0));
    effect.setPropertyValue(ArtifactCore::UniString("MaxExposure"), QVariant(5.0));
    effect.setPropertyValue(ArtifactCore::UniString("AdaptationSpeed"), QVariant(2.5));
    effect.setPropertyValue(ArtifactCore::UniString("Enabled"), QVariant(false));
    EXPECT_FLOAT_EQ(effect.middleGray(), 0.32f);
    EXPECT_FLOAT_EQ(effect.minExposure(), -3.0f);
    EXPECT_FLOAT_EQ(effect.maxExposure(), 5.0f);
    EXPECT_FLOAT_EQ(effect.adaptationSpeed(), 2.5f);
    EXPECT_FALSE(effect.isEnabled());

    effect.setPropertyValue(ArtifactCore::UniString("Unknown"), QVariant(1.0));
    EXPECT_FLOAT_EQ(effect.middleGray(), 0.32f);
}
