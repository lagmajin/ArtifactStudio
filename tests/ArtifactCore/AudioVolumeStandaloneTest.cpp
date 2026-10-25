#include <gtest/gtest.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <array>
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

TEST(AudioVolumeStandaloneTest, ExtremeFiniteDecibelsRemainWithinLinearBounds)
{
    const float extreme = std::numeric_limits<float>::max();

    EXPECT_FLOAT_EQ(AudioVolume::fromDecibels(extreme).volume(), 2.0f);
    EXPECT_FLOAT_EQ(AudioVolume::fromDecibels(-extreme).volume(), 0.0f);
}

TEST(AudioVolumeStandaloneTest, DecibelsObjectConversionPreservesRepresentableLevels)
{
    const AudioVolume source(0.5f);
    const auto db = source.toDecibelsObject();
    EXPECT_NEAR(db.decibels(), -6.0206f, 1e-4f);
    EXPECT_NEAR(AudioVolume::fromDecibelsObject(db).volume(), 0.5f, 1e-5f);
}

TEST(AudioVolumeStandaloneTest, DecibelsObjectConversionPreservesMuteAndMaximumEndpoints)
{
    const auto muteDb = AudioVolume(0.0f).toDecibelsObject();
    EXPECT_FLOAT_EQ(muteDb.decibels(), -60.0f);
    EXPECT_FLOAT_EQ(AudioVolume::fromDecibelsObject(muteDb).volume(), 0.0f);

    const auto maximumDb = AudioVolume(2.0f).toDecibelsObject();
    EXPECT_NEAR(maximumDb.decibels(), 6.0206f, 1e-4f);
    EXPECT_NEAR(AudioVolume::fromDecibelsObject(maximumDb).volume(), 2.0f, 1e-5f);
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

TEST(AudioVolumeStandaloneTest, InterpolationTreatsNaNAndNegativeInfinityAsStart)
{
    const AudioVolume from(0.2f);
    const AudioVolume to(1.2f);

    EXPECT_FLOAT_EQ(from.interpolate(
                        to, std::numeric_limits<float>::quiet_NaN()).volume(), 0.2f);
    EXPECT_FLOAT_EQ(from.interpolate(
                        to, -std::numeric_limits<float>::infinity()).volume(), 0.2f);
}

TEST(AudioVolumeStandaloneTest, InterpolationResultIsIndependentOfItsInputs)
{
    const AudioVolume from(0.2f);
    const AudioVolume to(1.2f);
    AudioVolume result = from.interpolate(to, 0.5f);

    result.setVolume(1.8f);

    EXPECT_FLOAT_EQ(result.volume(), 1.8f);
    EXPECT_FLOAT_EQ(from.volume(), 0.2f);
    EXPECT_FLOAT_EQ(to.volume(), 1.2f);
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

TEST(AudioVolumeStandaloneTest, MutedPredicateUsesExclusiveEpsilonBoundary)
{
    constexpr float threshold = 1.0e-6f;
    const float justBelow = std::nextafter(threshold, 0.0f);
    const float justAbove = std::nextafter(
        threshold, std::numeric_limits<float>::infinity());

    EXPECT_TRUE(AudioVolume(justBelow).isMuted());
    EXPECT_FALSE(AudioVolume(threshold).isMuted());
    EXPECT_FALSE(AudioVolume(justAbove).isMuted());
}

TEST(AudioVolumeStandaloneTest, NormalizedPredicateDistinguishesAdjacentEpsilonSides)
{
    constexpr float threshold = 1.0e-6f;
    const float roundedBoundary = 1.0f + threshold;
    const float justInside = std::nextafter(roundedBoundary, 1.0f);
    const float justOutside = std::nextafter(
        roundedBoundary, std::numeric_limits<float>::infinity());

    EXPECT_TRUE(AudioVolume(justInside).isNormalized());
    EXPECT_FALSE(AudioVolume(justOutside).isNormalized());
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

TEST(AudioVolumeStandaloneTest, PresetNamesCoverEveryPresetAndUnknownFallback)
{
    using Preset = AudioVolume::Preset;
    const std::array<std::pair<Preset, QString>, 6> expected{{
        {Preset::Mute, QStringLiteral("Mute")},
        {Preset::VeryQuiet, QStringLiteral("Very Quiet")},
        {Preset::Quiet, QStringLiteral("Quiet")},
        {Preset::Normal, QStringLiteral("Normal")},
        {Preset::Loud, QStringLiteral("Loud")},
        {Preset::Maximum, QStringLiteral("Maximum")}}};

    for (const auto& [preset, name] : expected) {
        EXPECT_EQ(AudioVolume::getPresetName(preset).toQString(), name);
    }
    EXPECT_EQ(AudioVolume::getPresetName(static_cast<Preset>(255)).toQString(),
              QStringLiteral("Unknown"));
}

TEST(AudioVolumeStandaloneTest, UnknownPresetFallsBackToUnityVolume)
{
    const auto fallback = AudioVolume::createPreset(
        static_cast<AudioVolume::Preset>(255));

    EXPECT_FLOAT_EQ(fallback.volume(), 1.0f);
    EXPECT_TRUE(fallback.isNormalized());
}

TEST(AudioVolumeStandaloneTest, SerializeDeserializePreservesNonDefaultVolume)
{
    const AudioVolume source(0.42f);

    const AudioVolume restored = AudioVolume::deserialize(source.serialize());

    EXPECT_FLOAT_EQ(restored.volume(), 0.42f);
}

TEST(AudioVolumeStandaloneTest, SerializeEmitsVolumeAndMutedFieldsAsJson)
{
    const AudioVolume source(0.42f);
    const QJsonDocument serialized = QJsonDocument::fromJson(
        source.serialize().toQString().toUtf8());

    ASSERT_TRUE(serialized.isObject());
    const QJsonObject object = serialized.object();
    ASSERT_TRUE(object.contains(QStringLiteral("volume")));
    ASSERT_TRUE(object.contains(QStringLiteral("muted")));
    EXPECT_DOUBLE_EQ(object.value(QStringLiteral("volume")).toDouble(), 0.42);
    EXPECT_FALSE(object.value(QStringLiteral("muted")).toBool());
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

TEST(AudioVolumeStandaloneTest, EqualsDirectlyUsesSymmetricEpsilonComparison)
{
    const AudioVolume baseline(1.0f);
    const AudioVolume near(1.0000005f);
    const AudioVolume outside(1.000002f);

    EXPECT_TRUE(baseline.equals(near));
    EXPECT_TRUE(near.equals(baseline));
    EXPECT_FALSE(baseline.equals(outside));
    EXPECT_FALSE(outside.equals(baseline));
}

TEST(AudioVolumeStandaloneTest, OrderingSeparatesValuesJustOutsideEpsilon)
{
    const AudioVolume baseline(1.0f);
    const AudioVolume inside(1.0000005f);
    const AudioVolume outside(1.000002f);

    EXPECT_FALSE(baseline < inside);
    EXPECT_FALSE(inside > baseline);
    EXPECT_TRUE(baseline < outside);
    EXPECT_TRUE(outside > baseline);
}

TEST(AudioVolumeStandaloneTest, ArithmeticOperatorsClampTheirResults)
{
    const AudioVolume base(0.75f);
    EXPECT_FLOAT_EQ((base + AudioVolume(1.5f)).volume(), 2.0f);
    EXPECT_FLOAT_EQ((base - AudioVolume(1.0f)).volume(), 0.0f);
    EXPECT_FLOAT_EQ((base * 2.0f).volume(), 1.5f);
    EXPECT_FLOAT_EQ((base / 0.0f).volume(), 0.0f);
}

TEST(AudioVolumeStandaloneTest, ValueDivisionUsesZeroFallbackForInvalidDenominators)
{
    const AudioVolume base(0.75f);
    EXPECT_FLOAT_EQ((base / 0.5e-6f).volume(), 0.0f);
    EXPECT_FLOAT_EQ((base / std::numeric_limits<float>::infinity()).volume(), 0.0f);
    EXPECT_FLOAT_EQ((base / -std::numeric_limits<float>::infinity()).volume(), 0.0f);
    EXPECT_FLOAT_EQ((base / std::numeric_limits<float>::quiet_NaN()).volume(), 0.0f);
}

TEST(AudioVolumeStandaloneTest, ValueDivisionUsesStrictEpsilonDenominatorBoundary)
{
    constexpr float threshold = 1.0e-6f;
    const AudioVolume base(0.75f);
    const float justBelow = std::nextafter(threshold, 0.0f);
    const float justAbove = std::nextafter(
        threshold, std::numeric_limits<float>::infinity());

    EXPECT_FLOAT_EQ((base / justBelow).volume(), 0.0f);
    EXPECT_FLOAT_EQ((base / threshold).volume(), 2.0f);
    EXPECT_FLOAT_EQ((base / justAbove).volume(), 2.0f);
}

TEST(AudioVolumeStandaloneTest, ValueDivisionClampsNegativeFiniteDenominatorsToMute)
{
    const AudioVolume base(0.75f);

    EXPECT_FLOAT_EQ((base / -0.5f).volume(), 0.0f);
    EXPECT_FLOAT_EQ((base / -2.0f).volume(), 0.0f);
}

TEST(AudioVolumeStandaloneTest, ValueMultiplicationClampsNegativeFiniteResultsToMute)
{
    const AudioVolume base(0.75f);

    EXPECT_FLOAT_EQ((base * -0.5f).volume(), 0.0f);
    EXPECT_FLOAT_EQ((base * -2.0f).volume(), 0.0f);
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

TEST(AudioVolumeStandaloneTest, CompoundAddAndSubtractSupportSelfAliasing)
{
    AudioVolume value(0.75f);

    value += value;
    EXPECT_FLOAT_EQ(value.volume(), 1.5f);

    value -= value;
    EXPECT_FLOAT_EQ(value.volume(), 0.0f);
}

TEST(AudioVolumeStandaloneTest, CompoundDivisionIgnoresNonFiniteDenominators)
{
    AudioVolume value(0.75f);
    value /= std::numeric_limits<float>::infinity();
    EXPECT_FLOAT_EQ(value.volume(), 0.75f);

    value /= -std::numeric_limits<float>::infinity();
    EXPECT_FLOAT_EQ(value.volume(), 0.75f);

    value /= std::numeric_limits<float>::quiet_NaN();
    EXPECT_FLOAT_EQ(value.volume(), 0.75f);
}

TEST(AudioVolumeStandaloneTest, CompoundDivisionUsesStrictEpsilonDenominatorBoundary)
{
    constexpr float threshold = 1.0e-6f;
    const float justBelow = std::nextafter(threshold, 0.0f);
    const float justAbove = std::nextafter(
        threshold, std::numeric_limits<float>::infinity());
    AudioVolume value(0.75f);

    value /= justBelow;
    EXPECT_FLOAT_EQ(value.volume(), 0.75f);
    value /= threshold;
    EXPECT_FLOAT_EQ(value.volume(), 0.75f);
    value /= justAbove;
    EXPECT_FLOAT_EQ(value.volume(), 2.0f);
}

TEST(AudioVolumeStandaloneTest, CompoundDivisionClampsNegativeFiniteResultsToMute)
{
    AudioVolume value(0.75f);
    value /= -0.5f;

    EXPECT_FLOAT_EQ(value.volume(), 0.0f);
}

TEST(AudioVolumeStandaloneTest, CompoundMultiplicationFallsBackToUnityOnOverflow)
{
    AudioVolume value(0.75f);
    value *= std::numeric_limits<float>::max();

    EXPECT_FLOAT_EQ(value.volume(), 1.0f);
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

TEST(AudioVolumeStandaloneTest, SelfCopyAndMoveAssignmentPreserveTheVolume)
{
    AudioVolume value(0.65f);

    value = value;
    EXPECT_FLOAT_EQ(value.volume(), 0.65f);

    value = std::move(value);
    EXPECT_FLOAT_EQ(value.volume(), 0.65f);
}

