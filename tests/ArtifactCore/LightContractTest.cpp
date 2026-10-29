#include <gtest/gtest.h>

#include <limits>

import Core.Light;
import Float3;
import Math.Vec;

namespace {

using ArtifactCore::Light;
using ArtifactCore::LightType;
using ArtifactCore::Coordinates::WorldVector3;
using ArtifactCore::Units::Degrees;
using ArtifactCore::Units::Pixels;

} // namespace

TEST(LightContractTest, DirectionNormalizesAndZeroInputKeepsLastValidDirection)
{
    Light light;
    light.setDirection(WorldVector3{0.0f, -4.0f, 0.0f});
    EXPECT_FLOAT_EQ(light.direction().x, 0.0f);
    EXPECT_FLOAT_EQ(light.direction().y, -1.0f);
    EXPECT_FLOAT_EQ(light.direction().z, 0.0f);

    light.setDirection(WorldVector3{0.0f, 0.0f, 0.0f});
    EXPECT_FLOAT_EQ(light.direction().y, -1.0f);
}

TEST(LightContractTest, RangeAndAttenuationClampAndRestoreKnownCoefficients)
{
    Light light(LightType::Point);
    light.setRange(Pixels{10.0f});
    EXPECT_FLOAT_EQ(light.attenuationConstant(), 1.0f);
    EXPECT_FLOAT_EQ(light.attenuationLinear(), 0.45f);
    EXPECT_FLOAT_EQ(light.attenuationQuadratic(), 0.75f);

    light.setRange(Pixels{0.0f});
    EXPECT_FLOAT_EQ(light.attenuationConstant(), 1.0f);
    EXPECT_FLOAT_EQ(light.attenuationLinear(), 0.0f);
    EXPECT_FLOAT_EQ(light.attenuationQuadratic(), 0.0f);

    light.setAttenuation(-1.0f, 2.0f, -3.0f);
    EXPECT_FLOAT_EQ(light.attenuationConstant(), 0.0f);
    EXPECT_FLOAT_EQ(light.attenuationLinear(), 2.0f);
    EXPECT_FLOAT_EQ(light.attenuationQuadratic(), 0.0f);
}

TEST(LightContractTest, SpotAreaAndGoboSettingsRespectTheirRanges)
{
    Light light(LightType::Spot);
    light.setCutoff(Degrees{80.0f}, Degrees{20.0f});
    EXPECT_FLOAT_EQ(light.spotInnerCutoff().value, 80.0f);
    EXPECT_FLOAT_EQ(light.spotOuterCutoff().value, 80.0f);
    light.setSpotAngle(Degrees{120.0f});
    EXPECT_FLOAT_EQ(light.spotInnerCutoff().value, 72.0f);
    EXPECT_FLOAT_EQ(light.spotOuterCutoff().value, 90.0f);

    light.setAreaSize(Pixels{-2.0f}, Pixels{0.0f});
    EXPECT_FLOAT_EQ(light.areaWidth().value, 1.0f);
    EXPECT_FLOAT_EQ(light.areaHeight().value, 1.0f);
    light.setAreaShape(-5);
    EXPECT_EQ(light.areaShape(), 0);
    light.setAreaShape(9);
    EXPECT_EQ(light.areaShape(), 1);

    light.setGoboIntensity(1.5f);
    EXPECT_FLOAT_EQ(light.goboIntensity(), 1.0f);
    light.setGoboRotation(Degrees{450.0f});
    EXPECT_FLOAT_EQ(light.goboRotation().value, 90.0f);
    light.setGoboInvert(true);
    EXPECT_TRUE(light.goboInvert());
}

TEST(LightContractTest, RadianceAndShadowSettingsPreserveExplicitValues)
{
    Light light = Light::makeAmbient({0.25f, 0.5f, 0.75f}, 2.0f);
    EXPECT_EQ(light.type(), LightType::Ambient);
    EXPECT_FLOAT_EQ(light.radiance().x, 0.5f);
    EXPECT_FLOAT_EQ(light.radiance().y, 1.0f);
    EXPECT_FLOAT_EQ(light.radiance().z, 1.5f);

    light.setIntensity(-3.0f);
    EXPECT_FLOAT_EQ(light.intensity(), 0.0f);
    light.setShadowSoftness(8.0f);
    EXPECT_FLOAT_EQ(light.shadowSoftness(), 2.0f);
    light.setShadowSoftness(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(light.shadowSoftness(), 0.0f);
    light.setCastsShadows(false);
    light.setEnabled(false);
    EXPECT_FALSE(light.castsShadows());
    EXPECT_FALSE(light.enabled());
}
