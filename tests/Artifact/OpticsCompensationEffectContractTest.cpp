#include <gtest/gtest.h>

#include <limits>

#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.OpticsCompensation;
import Utils.String.UniString;

TEST(OpticsCompensationEffectContractTest, DefaultsAndPropertyNamesAreStable)
{
    const Artifact::OpticsCompensationEffect effect;
    const auto properties = effect.getProperties();
    ASSERT_EQ(properties.size(), 4u);
    EXPECT_FLOAT_EQ(effect.centerX(), 0.5f);
    EXPECT_FLOAT_EQ(effect.centerY(), 0.5f);
    EXPECT_FLOAT_EQ(effect.fov(), 45.0f);
    EXPECT_EQ(effect.direction(), 1);
    EXPECT_EQ(properties[0].getName(), QStringLiteral("Center X"));
    EXPECT_EQ(properties[1].getName(), QStringLiteral("Center Y"));
    EXPECT_EQ(properties[2].getName(), QStringLiteral("FOV"));
    EXPECT_EQ(properties[3].getName(), QStringLiteral("Direction"));
}

TEST(OpticsCompensationEffectContractTest, SettersClampAndNormalizeParameters)
{
    Artifact::OpticsCompensationEffect effect;
    effect.setCenterX(-1.0f);
    EXPECT_FLOAT_EQ(effect.centerX(), 0.0f);
    effect.setCenterX(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.centerX(), 0.5f);
    effect.setCenterY(2.0f);
    EXPECT_FLOAT_EQ(effect.centerY(), 1.0f);
    effect.setFov(0.0f);
    EXPECT_FLOAT_EQ(effect.fov(), 1.0f);
    effect.setFov(200.0f);
    EXPECT_FLOAT_EQ(effect.fov(), 180.0f);
    effect.setDirection(0);
    EXPECT_EQ(effect.direction(), 1);
    effect.setDirection(-5);
    EXPECT_EQ(effect.direction(), -1);
}

TEST(OpticsCompensationEffectContractTest, PropertyUpdatesDelegateToClampedSetters)
{
    Artifact::OpticsCompensationEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString("Center X"), QVariant(3.0));
    effect.setPropertyValue(ArtifactCore::UniString("FOV"), QVariant(200.0));
    effect.setPropertyValue(ArtifactCore::UniString("Direction"), QVariant(0));
    EXPECT_FLOAT_EQ(effect.centerX(), 1.0f);
    EXPECT_FLOAT_EQ(effect.fov(), 180.0f);
    EXPECT_EQ(effect.direction(), 1);
}
