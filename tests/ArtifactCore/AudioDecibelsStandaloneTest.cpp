#include <gtest/gtest.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <array>
#include <cmath>
#include <limits>
#include <utility>

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

TEST(AudioDecibelsStandaloneTest, LinearConversionClampsAtSupportedGainBoundaries)
{
    const float minimumGain = 0.001f;
    const float justBelowMinimum = std::nextafter(minimumGain, 0.0f);
    const float maximumGain = 10.0f;
    const float justAboveMaximum = std::nextafter(
        maximumGain, std::numeric_limits<float>::infinity());

    EXPECT_FLOAT_EQ(AudioDecibels::fromLinearValue(justBelowMinimum).decibels(), -60.0f);
    EXPECT_NEAR(AudioDecibels::fromLinearValue(minimumGain).decibels(), -60.0f, 1.0e-4f);
    EXPECT_NEAR(AudioDecibels::fromLinearValue(maximumGain).decibels(), 20.0f, 1.0e-4f);
    EXPECT_FLOAT_EQ(AudioDecibels::fromLinearValue(justAboveMaximum).decibels(), 20.0f);
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

TEST(AudioDecibelsStandaloneTest, PresetNamesCoverAllValuesAndUnknownFallback)
{
    using Preset = AudioDecibels::Preset;
    const std::array<std::pair<Preset, QString>, 7> expected{{
        {Preset::Mute, QStringLiteral("Mute (- dB)")},
        {Preset::VeryQuiet, QStringLiteral("Very Quiet (-40 dB)")},
        {Preset::Quiet, QStringLiteral("Quiet (-20 dB)")},
        {Preset::Normal, QStringLiteral("Normal (0 dB)")},
        {Preset::Loud, QStringLiteral("Loud (+6 dB)")},
        {Preset::VeryLoud, QStringLiteral("Very Loud (+12 dB)")},
        {Preset::Maximum, QStringLiteral("Maximum (+20 dB)")}}};

    for (const auto& [preset, name] : expected) {
        EXPECT_EQ(AudioDecibels::getPresetName(preset).toQString(), name);
    }
    EXPECT_EQ(AudioDecibels::getPresetName(static_cast<Preset>(255)).toQString(),
              QStringLiteral("Unknown"));
}

TEST(AudioDecibelsStandaloneTest, SerializeDeserializePreservesNonDefaultLevel)
{
    const AudioDecibels source(-12.75f);

    const AudioDecibels restored = AudioDecibels::deserialize(source.serialize());

    EXPECT_FLOAT_EQ(restored.decibels(), -12.75f);
}

TEST(AudioDecibelsStandaloneTest, SerializeEmitsDecibelsFieldAsJson)
{
    const AudioDecibels source(-12.75f);
    const QJsonDocument serialized = QJsonDocument::fromJson(
        source.serialize().toQString().toUtf8());

    ASSERT_TRUE(serialized.isObject());
    const QJsonObject object = serialized.object();
    ASSERT_TRUE(object.contains(QStringLiteral("dB")));
    EXPECT_DOUBLE_EQ(object.value(QStringLiteral("dB")).toDouble(), -12.75);
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

TEST(AudioDecibelsStandaloneTest, NormalizedPredicateUsesExclusiveEpsilonBoundary)
{
    constexpr float threshold = 1.0e-6f;
    const float justInside = std::nextafter(threshold, 0.0f);
    const float justOutside = std::nextafter(
        threshold, std::numeric_limits<float>::infinity());

    EXPECT_TRUE(AudioDecibels(justInside).isNormalized());
    EXPECT_FALSE(AudioDecibels(threshold).isNormalized());
    EXPECT_FALSE(AudioDecibels(justOutside).isNormalized());
    EXPECT_TRUE(AudioDecibels(-justInside).isNormalized());
    EXPECT_FALSE(AudioDecibels(-threshold).isNormalized());
    EXPECT_FALSE(AudioDecibels(-justOutside).isNormalized());
}

TEST(AudioDecibelsStandaloneTest, SilentPredicateUsesInclusiveMinusFortyDecibelBoundary)
{
    const float threshold = -40.0f;
    const float justBelow = std::nextafter(threshold,
                                           -std::numeric_limits<float>::infinity());
    const float justAbove = std::nextafter(threshold,
                                           std::numeric_limits<float>::infinity());

    EXPECT_TRUE(AudioDecibels(justBelow).isSilent());
    EXPECT_TRUE(AudioDecibels(threshold).isSilent());
    EXPECT_FALSE(AudioDecibels(justAbove).isSilent());
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

TEST(AudioDecibelsStandaloneTest, NegativeDivisorsReverseDecibelSign)
{
    const AudioDecibels base(-12.0f);
    EXPECT_FLOAT_EQ((base / -2.0f).decibels(), 6.0f);

    AudioDecibels compound(-12.0f);
    compound /= -2.0f;
    EXPECT_FLOAT_EQ(compound.decibels(), 6.0f);
}

TEST(AudioDecibelsStandaloneTest, InvalidDivisionOperandsUseDefinedFallbacks)
{
    const AudioDecibels base(8.0f);
    EXPECT_FLOAT_EQ((base / 0.5e-6f).decibels(), 0.0f);
    EXPECT_FLOAT_EQ((base / std::numeric_limits<float>::infinity()).decibels(), 0.0f);
    EXPECT_FLOAT_EQ((base / std::numeric_limits<float>::quiet_NaN()).decibels(), 0.0f);

    AudioDecibels compound(8.0f);
    compound /= 0.5e-6f;
    EXPECT_FLOAT_EQ(compound.decibels(), 8.0f);
    compound /= std::numeric_limits<float>::infinity();
    EXPECT_FLOAT_EQ(compound.decibels(), 8.0f);
    compound /= std::numeric_limits<float>::quiet_NaN();
    EXPECT_FLOAT_EQ(compound.decibels(), 8.0f);
}

TEST(AudioDecibelsStandaloneTest, CompoundMultiplicationSanitizesNonFiniteResultsToUnity)
{
    AudioDecibels value(5.0f);
    value *= std::numeric_limits<float>::infinity();
    EXPECT_FLOAT_EQ(value.decibels(), 0.0f);

    value.setDecibels(5.0f);
    value *= -std::numeric_limits<float>::infinity();
    EXPECT_FLOAT_EQ(value.decibels(), 0.0f);

    value.setDecibels(5.0f);
    value *= std::numeric_limits<float>::quiet_NaN();
    EXPECT_FLOAT_EQ(value.decibels(), 0.0f);
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

TEST(AudioDecibelsStandaloneTest, ComparisonsSeparateValuesOutsideEpsilon)
{
    const AudioDecibels baseline(0.0f);
    const AudioDecibels inside(0.75e-6f);
    const AudioDecibels outside(1.25e-6f);

    EXPECT_TRUE(baseline == inside);
    EXPECT_FALSE(baseline < inside);
    EXPECT_FALSE(inside > baseline);

    EXPECT_FALSE(baseline == outside);
    EXPECT_TRUE(baseline != outside);
    EXPECT_TRUE(baseline < outside);
    EXPECT_TRUE(outside > baseline);
    EXPECT_FALSE(outside <= baseline);
    EXPECT_FALSE(baseline >= outside);
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

TEST(AudioDecibelsStandaloneTest, MoveConstructionAndAssignmentTransferOwnership)
{
    AudioDecibels source(-12.5f);
    AudioDecibels moved(std::move(source));
    EXPECT_FLOAT_EQ(moved.decibels(), -12.5f);

    AudioDecibels destination(6.0f);
    AudioDecibels incoming(3.25f);
    destination = std::move(incoming);
    EXPECT_FLOAT_EQ(destination.decibels(), 3.25f);

    destination = std::move(destination);
    EXPECT_FLOAT_EQ(destination.decibels(), 3.25f);
}

