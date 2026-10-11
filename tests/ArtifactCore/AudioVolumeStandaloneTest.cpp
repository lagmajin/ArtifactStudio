#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Volume;

using namespace ArtifactCore;

TEST(AudioVolumeStandaloneTest, DefaultsToUnityAndIsNormalized)
{
    const AudioVolume volume;
    EXPECT_FLOAT_EQ(volume.volume(), 1.0f);
    EXPECT_FLOAT_EQ(volume.toDecibels(), 0.0f);
    EXPECT_TRUE(volume.isNormalized());
    EXPECT_TRUE(volume.isValid());
}

TEST(AudioVolumeStandaloneTest, ConstructorAndSetterClampAndSanitizeValues)
{
    AudioVolume above(3.0f);
    AudioVolume below(-1.0f);
    EXPECT_FLOAT_EQ(above.volume(), 2.0f);
    EXPECT_FLOAT_EQ(below.volume(), 0.0f);
    above.setVolume(std::numeric_limits<float>::infinity());
    below.setVolume(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(above.volume(), 1.0f);
    EXPECT_FLOAT_EQ(below.volume(), 1.0f);
}

TEST(AudioVolumeStandaloneTest, ConvertsLinearVolumeToDecibelsIncludingSilence)
{
    EXPECT_FLOAT_EQ(AudioVolume(1.0f).toDecibels(), 0.0f);
    EXPECT_NEAR(AudioVolume(0.5f).toDecibels(), -6.0206f, 1e-4f);
    EXPECT_FLOAT_EQ(AudioVolume(0.0f).toDecibels(),
                    -std::numeric_limits<float>::infinity());
    EXPECT_NEAR(AudioVolume(2.0f).toDecibels(), 6.0206f, 1e-4f);
}

TEST(AudioVolumeStandaloneTest, ConvertsDecibelsToBoundedLinearVolume)
{
    EXPECT_FLOAT_EQ(AudioVolume::fromDecibels(0.0f).volume(), 1.0f);
    EXPECT_NEAR(AudioVolume::fromDecibels(-6.0206f).volume(), 0.5f, 1e-4f);
    EXPECT_FLOAT_EQ(AudioVolume::fromDecibels(-std::numeric_limits<float>::infinity()).volume(), 0.0f);
    EXPECT_FLOAT_EQ(AudioVolume::fromDecibels(std::numeric_limits<float>::infinity()).volume(), 2.0f);
    EXPECT_FLOAT_EQ(AudioVolume::fromDecibels(std::numeric_limits<float>::quiet_NaN()).volume(), 1.0f);
    EXPECT_FLOAT_EQ(AudioVolume::fromDecibels(100.0f).volume(), 2.0f);
}

TEST(AudioVolumeStandaloneTest, DecibelsObjectConversionPreservesRepresentableLevels)
{
    const AudioVolume source(0.5f);
    const auto db = source.toDecibelsObject();
    EXPECT_NEAR(db.decibels(), -6.0206f, 1e-4f);
    EXPECT_NEAR(AudioVolume::fromDecibelsObject(db).volume(), 0.5f, 1e-5f);
}

TEST(AudioVolumeStandaloneTest, InterpolationClampsFractionAndSanitizesNonFinite)
{
    const AudioVolume from(0.2f);
    const AudioVolume to(1.2f);
    EXPECT_FLOAT_EQ(from.interpolate(to, 0.0f).volume(), 0.2f);
    EXPECT_FLOAT_EQ(from.interpolate(to, 0.5f).volume(), 0.7f);
    EXPECT_FLOAT_EQ(from.interpolate(to, 1.0f).volume(), 1.2f);
    EXPECT_FLOAT_EQ(from.interpolate(to, -1.0f).volume(), 0.2f);
    EXPECT_FLOAT_EQ(from.interpolate(to, 2.0f).volume(), 1.2f);
    EXPECT_FLOAT_EQ(from.interpolate(to, std::numeric_limits<float>::infinity()).volume(), 0.2f);
}

TEST(AudioVolumeStandaloneTest, LinearFadeUsesVolumeInterpolation)
{
    EXPECT_FLOAT_EQ(AudioVolume::linearFade(AudioVolume(0.0f), AudioVolume(2.0f), 0.25f).volume(), 0.5f);
}

TEST(AudioVolumeStandaloneTest, ValidMutedAndNormalizedPredicatesRespectTheirRanges)
{
    EXPECT_TRUE(AudioVolume(0.5f).isValid());
    EXPECT_FALSE(AudioVolume(1.5f).isValid());
    EXPECT_TRUE(AudioVolume(0.0f).isMuted());
    EXPECT_TRUE(AudioVolume(0.0000001f).isMuted());
    EXPECT_FALSE(AudioVolume(0.01f).isMuted());
    EXPECT_TRUE(AudioVolume(1.0f).isNormalized());
    EXPECT_FALSE(AudioVolume(1.01f).isNormalized());
}

TEST(AudioVolumeStandaloneTest, ClampRestrictsTheStoredLevelToNormalizedRange)
{
    AudioVolume loud(1.5f);
    loud.clamp();
    EXPECT_FLOAT_EQ(loud.volume(), 1.0f);
    AudioVolume invalid(std::numeric_limits<float>::quiet_NaN());
    invalid.clamp();
    EXPECT_FLOAT_EQ(invalid.volume(), 1.0f);
}

TEST(AudioVolumeStandaloneTest, PresetsMatchTheirLinearLevels)
{
    EXPECT_FLOAT_EQ(AudioVolume::createPreset(AudioVolume::Preset::Mute).volume(), 0.0f);
    EXPECT_FLOAT_EQ(AudioVolume::createPreset(AudioVolume::Preset::VeryQuiet).volume(), 0.1f);
    EXPECT_FLOAT_EQ(AudioVolume::createPreset(AudioVolume::Preset::Quiet).volume(), 0.3f);
    EXPECT_FLOAT_EQ(AudioVolume::createPreset(AudioVolume::Preset::Normal).volume(), 1.0f);
    EXPECT_FLOAT_EQ(AudioVolume::createPreset(AudioVolume::Preset::Loud).volume(), 1.5f);
    EXPECT_FLOAT_EQ(AudioVolume::createPreset(AudioVolume::Preset::Maximum).volume(), 2.0f);
}

TEST(AudioVolumeStandaloneTest, ComparisonsUseEpsilonEquality)
{
    const AudioVolume a(1.0f);
    const AudioVolume near(1.0000005f);
    const AudioVolume louder(1.5f);
    EXPECT_TRUE(a == near);
    EXPECT_FALSE(a != near);
    EXPECT_TRUE(a < louder);
    EXPECT_TRUE(a <= near);
    EXPECT_TRUE(louder > a);
    EXPECT_TRUE(louder >= a);
}

TEST(AudioVolumeStandaloneTest, ArithmeticOperatorsClampTheirResults)
{
    const AudioVolume base(0.75f);
    EXPECT_FLOAT_EQ((base + AudioVolume(1.5f)).volume(), 2.0f);
    EXPECT_FLOAT_EQ((base - AudioVolume(1.0f)).volume(), 0.0f);
    EXPECT_FLOAT_EQ((base * 2.0f).volume(), 1.5f);
    EXPECT_FLOAT_EQ((base / 0.0f).volume(), 0.0f);
}

TEST(AudioVolumeStandaloneTest, CompoundArithmeticMutatesWithinSupportedRange)
{
    AudioVolume value(0.5f);
    value += AudioVolume(0.25f);
    EXPECT_FLOAT_EQ(value.volume(), 0.75f);
    value -= AudioVolume(1.0f);
    EXPECT_FLOAT_EQ(value.volume(), 0.0f);
    value *= 3.0f;
    EXPECT_FLOAT_EQ(value.volume(), 0.0f);
    value = AudioVolume(1.5f);
    value /= 2.0f;
    EXPECT_FLOAT_EQ(value.volume(), 0.75f);
    value /= 0.0f;
    EXPECT_FLOAT_EQ(value.volume(), 0.75f);
}

TEST(AudioVolumeStandaloneTest, CopyConstructionAndAssignmentAreIndependent)
{
    AudioVolume source(0.4f);
    AudioVolume copied(source);
    AudioVolume assigned;
    assigned = source;
    source.setVolume(1.2f);

    EXPECT_FLOAT_EQ(copied.volume(), 0.4f);
    EXPECT_FLOAT_EQ(assigned.volume(), 0.4f);
    EXPECT_FLOAT_EQ(source.volume(), 1.2f);
}

TEST(AudioVolumeStandaloneTest, MoveConstructionAndAssignmentTransferOwnership)
{
    AudioVolume source(0.4f);
    AudioVolume moved(std::move(source));
    EXPECT_FLOAT_EQ(moved.volume(), 0.4f);

    AudioVolume target(1.5f);
    target = std::move(moved);
    EXPECT_FLOAT_EQ(target.volume(), 0.4f);
}

