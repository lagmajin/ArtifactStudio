#include <gtest/gtest.h>

#include <cmath>
#include <limits>

import Audio.Decibels;

using namespace ArtifactCore;

TEST(AudioDecibelsStandaloneTest, DefaultsToZeroDecibelsAndUnityLinearGain)
{
    const AudioDecibels decibels;
    EXPECT_FLOAT_EQ(decibels.decibels(), 0.0f);
    EXPECT_FLOAT_EQ(decibels.toLinearValue(), 1.0f);
    EXPECT_TRUE(decibels.isNormalized());
    EXPECT_TRUE(decibels.isValid());
}

TEST(AudioDecibelsStandaloneTest, ConstructorAndSetterClampToSupportedRange)
{
    AudioDecibels above(30.0f);
    AudioDecibels below(-80.0f);
    EXPECT_FLOAT_EQ(above.decibels(), 20.0f);
    EXPECT_FLOAT_EQ(below.decibels(), -60.0f);

    above.setDecibels(-100.0f);
    below.setDecibels(100.0f);
    EXPECT_FLOAT_EQ(above.decibels(), -60.0f);
    EXPECT_FLOAT_EQ(below.decibels(), 20.0f);
}

TEST(AudioDecibelsStandaloneTest, NonFiniteDecibelInputsSanitizeToUnity)
{
    AudioDecibels decibels(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(decibels.decibels(), 0.0f);
    decibels.setDecibels(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(decibels.decibels(), 0.0f);
    decibels.setDecibels(-std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(decibels.decibels(), 0.0f);
}

TEST(AudioDecibelsStandaloneTest, ConvertsCommonDecibelValuesToLinearGain)
{
    EXPECT_NEAR(AudioDecibels(-6.0f).toLinearValue(), 0.5011872f, 1e-6f);
    EXPECT_FLOAT_EQ(AudioDecibels(-60.0f).toLinearValue(), 0.0f);
    EXPECT_FLOAT_EQ(AudioDecibels(0.0f).toLinearValue(), 1.0f);
    EXPECT_NEAR(AudioDecibels(6.0f).toLinearValue(), 1.9952623f, 1e-6f);
}

TEST(AudioDecibelsStandaloneTest, ConvertsLinearValuesAndClampsResult)
{
    EXPECT_NEAR(AudioDecibels::fromLinearValue(0.5f).decibels(),
                -6.0206f, 1e-4f);
    EXPECT_FLOAT_EQ(AudioDecibels::fromLinearValue(1.0f).decibels(), 0.0f);
    EXPECT_FLOAT_EQ(AudioDecibels::fromLinearValue(0.0f).decibels(), -60.0f);
    EXPECT_FLOAT_EQ(AudioDecibels::fromLinearValue(-1.0f).decibels(), -60.0f);
    EXPECT_FLOAT_EQ(AudioDecibels::fromLinearValue(100.0f).decibels(), 20.0f);
    EXPECT_FLOAT_EQ(AudioDecibels::fromLinearValue(
                        std::numeric_limits<float>::infinity()).decibels(), 20.0f);
    EXPECT_FLOAT_EQ(AudioDecibels::fromLinearValue(
                        std::numeric_limits<float>::quiet_NaN()).decibels(), 0.0f);
}

TEST(AudioDecibelsStandaloneTest, InterpolationClampsFractionAndDefaultsNonFiniteToStart)
{
    const AudioDecibels start(-20.0f);
    const AudioDecibels target(0.0f);
    EXPECT_FLOAT_EQ(start.interpolate(target, 0.0f).decibels(), -20.0f);
    EXPECT_FLOAT_EQ(start.interpolate(target, 0.25f).decibels(), -15.0f);
    EXPECT_FLOAT_EQ(start.interpolate(target, 1.0f).decibels(), 0.0f);
    EXPECT_FLOAT_EQ(start.interpolate(target, -1.0f).decibels(), -20.0f);
    EXPECT_FLOAT_EQ(start.interpolate(target, 2.0f).decibels(), 0.0f);
    EXPECT_FLOAT_EQ(start.interpolate(target,
                        std::numeric_limits<float>::quiet_NaN()).decibels(), -20.0f);
}

TEST(AudioDecibelsStandaloneTest, LinearFadeUsesTheSameClampedInterpolation)
{
    const AudioDecibels start(-40.0f);
    const AudioDecibels target(20.0f);
    EXPECT_FLOAT_EQ(AudioDecibels::linearFade(start, target, 0.5f).decibels(), -10.0f);
    EXPECT_FLOAT_EQ(AudioDecibels::linearFade(start, target, 4.0f).decibels(), 20.0f);
}

TEST(AudioDecibelsStandaloneTest, PresetsMatchDocumentedLevels)
{
    EXPECT_FLOAT_EQ(AudioDecibels::createPreset(AudioDecibels::Preset::Mute).decibels(), -60.0f);
    EXPECT_FLOAT_EQ(AudioDecibels::createPreset(AudioDecibels::Preset::VeryQuiet).decibels(), -40.0f);
    EXPECT_FLOAT_EQ(AudioDecibels::createPreset(AudioDecibels::Preset::Quiet).decibels(), -20.0f);
    EXPECT_FLOAT_EQ(AudioDecibels::createPreset(AudioDecibels::Preset::Normal).decibels(), 0.0f);
    EXPECT_FLOAT_EQ(AudioDecibels::createPreset(AudioDecibels::Preset::Loud).decibels(), 6.0f);
    EXPECT_FLOAT_EQ(AudioDecibels::createPreset(AudioDecibels::Preset::VeryLoud).decibels(), 12.0f);
    EXPECT_FLOAT_EQ(AudioDecibels::createPreset(AudioDecibels::Preset::Maximum).decibels(), 20.0f);
}

TEST(AudioDecibelsStandaloneTest, MuteSilentAndNormalizedPredicatesUseTheirThresholds)
{
    EXPECT_TRUE(AudioDecibels(-60.0f).isMute());
    EXPECT_TRUE(AudioDecibels(-40.0f).isSilent());
    EXPECT_FALSE(AudioDecibels(-39.9f).isSilent());
    EXPECT_TRUE(AudioDecibels(0.0f).isNormalized());
    EXPECT_FALSE(AudioDecibels(0.01f).isNormalized());
    EXPECT_TRUE(AudioDecibels(-30.0f).isValid());
}

TEST(AudioDecibelsStandaloneTest, ArithmeticOperatorsClampResults)
{
    const AudioDecibels base(10.0f);
    EXPECT_FLOAT_EQ((base + 15.0f).decibels(), 20.0f);
    EXPECT_FLOAT_EQ((base - 80.0f).decibels(), -60.0f);
    EXPECT_FLOAT_EQ((base * 3.0f).decibels(), 20.0f);
    EXPECT_FLOAT_EQ((base / 2.0f).decibels(), 5.0f);
    EXPECT_FLOAT_EQ((base / 0.0f).decibels(), 0.0f);
}

TEST(AudioDecibelsStandaloneTest, CompoundArithmeticMutatesAndClamps)
{
    AudioDecibels value(5.0f);
    value += 10.0f;
    EXPECT_FLOAT_EQ(value.decibels(), 15.0f);
    value -= 40.0f;
    EXPECT_FLOAT_EQ(value.decibels(), -25.0f);
    value *= 3.0f;
    EXPECT_FLOAT_EQ(value.decibels(), -60.0f);
    value /= 2.0f;
    EXPECT_FLOAT_EQ(value.decibels(), -30.0f);
}

TEST(AudioDecibelsStandaloneTest, ComparisonsRespectEpsilonEquality)
{
    const AudioDecibels a(0.0f);
    const AudioDecibels near(0.0000005f);
    const AudioDecibels greater(1.0f);
    EXPECT_TRUE(a == near);
    EXPECT_FALSE(a != near);
    EXPECT_TRUE(a < greater);
    EXPECT_TRUE(a <= near);
    EXPECT_TRUE(greater > a);
    EXPECT_TRUE(greater >= a);
}

TEST(AudioDecibelsStandaloneTest, CopyConstructionAndAssignmentAreIndependent)
{
    AudioDecibels source(-12.0f);
    AudioDecibels copied(source);
    AudioDecibels assigned;
    assigned = source;
    source.setDecibels(3.0f);

    EXPECT_FLOAT_EQ(copied.decibels(), -12.0f);
    EXPECT_FLOAT_EQ(assigned.decibels(), -12.0f);
    EXPECT_FLOAT_EQ(source.decibels(), 3.0f);
}

