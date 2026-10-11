#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.ParametricEQ;
import Audio.Segment;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
AudioSegment makeMono(int sampleRate, QVector<float> samples)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = AudioChannelLayout::Mono;
    segment.startFrame = 41;
    segment.channelData.push_back(std::move(samples));
    return segment;
}

QVector<float> constantSamples(float value, int count)
{
    return QVector<float>(count, value);
}
}

TEST(AudioParametricEQStandaloneTest, ExposesExpectedIdentityAndFourDefaultBands)
{
    const AudioParametricEQ eq;
    EXPECT_EQ(toStdString(eq.getName()), "Parametric EQ");
    EXPECT_EQ(toStdString(eq.effectType()), "parametric_eq");
    ASSERT_EQ(eq.getBandCount(), 4);
    EXPECT_FLOAT_EQ(eq.getBand(0).frequency, 100.0f);
    EXPECT_FLOAT_EQ(eq.getBand(1).frequency, 500.0f);
    EXPECT_FLOAT_EQ(eq.getBand(2).frequency, 2000.0f);
    EXPECT_FLOAT_EQ(eq.getBand(3).frequency, 8000.0f);
    for (int index = 0; index < eq.getBandCount(); ++index) {
        const auto band = eq.getBand(index);
        EXPECT_FLOAT_EQ(band.gainDb, 0.0f);
        EXPECT_FLOAT_EQ(band.qFactor, 1.0f);
        EXPECT_TRUE(band.enabled);
    }
}

TEST(AudioParametricEQStandaloneTest, SetBandClampsValuesAndReplacesNonFiniteValues)
{
    AudioParametricEQ eq;
    eq.setBand(0, {30000.0f, 60.0f, 20.0f, false});
    auto band = eq.getBand(0);
    EXPECT_FLOAT_EQ(band.frequency, 24000.0f);
    EXPECT_FLOAT_EQ(band.gainDb, 48.0f);
    EXPECT_FLOAT_EQ(band.qFactor, 10.0f);
    EXPECT_FALSE(band.enabled);

    eq.setBand(1, {std::numeric_limits<float>::quiet_NaN(),
                   std::numeric_limits<float>::infinity(),
                   std::numeric_limits<float>::quiet_NaN(), true});
    band = eq.getBand(1);
    EXPECT_FLOAT_EQ(band.frequency, 1000.0f);
    EXPECT_FLOAT_EQ(band.gainDb, 0.0f);
    EXPECT_FLOAT_EQ(band.qFactor, 1.0f);
}

TEST(AudioParametricEQStandaloneTest, InvalidBandIndicesDoNotChangeExistingBands)
{
    AudioParametricEQ eq;
    const auto original = eq.getBand(0);
    eq.setBand(-1, {200.0f, 3.0f, 2.0f, false});
    eq.setBand(eq.getBandCount(), {200.0f, 3.0f, 2.0f, false});
    EXPECT_FLOAT_EQ(eq.getBand(0).frequency, original.frequency);
    EXPECT_FLOAT_EQ(eq.getBand(0).gainDb, original.gainDb);
    EXPECT_FLOAT_EQ(eq.getBand(0).qFactor, original.qFactor);
    EXPECT_EQ(eq.getBand(0).enabled, original.enabled);
    EXPECT_FLOAT_EQ(eq.getBand(-1).frequency, 1000.0f);
}

TEST(AudioParametricEQStandaloneTest, BandCountIsClampedToSupportedRange)
{
    AudioParametricEQ eq;
    eq.setBandCount(-2);
    EXPECT_EQ(eq.getBandCount(), 0);
    eq.setBandCount(80);
    EXPECT_EQ(eq.getBandCount(), 64);
    eq.setBandCount(3);
    EXPECT_EQ(eq.getBandCount(), 3);
}

TEST(AudioParametricEQStandaloneTest, ParameterIdsUpdateOnlyTheirBandProperty)
{
    AudioParametricEQ eq;
    eq.setParameterValue(String("band1.frequency"), 750.0f);
    eq.setParameterValue(String("band1.gainDb"), 6.0f);
    eq.setParameterValue(String("band1.qFactor"), 2.5f);
    const auto band = eq.getBand(1);
    EXPECT_FLOAT_EQ(band.frequency, 750.0f);
    EXPECT_FLOAT_EQ(band.gainDb, 6.0f);
    EXPECT_FLOAT_EQ(band.qFactor, 2.5f);

    eq.setParameterValue(String("band1.gainDb"), 100.0f);
    EXPECT_FLOAT_EQ(eq.getBand(1).gainDb, 48.0f);
    eq.setParameterValue(String("band1.gainDb"), std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(eq.getBand(1).gainDb, 48.0f);
}

TEST(AudioParametricEQStandaloneTest, ParameterMetadataContainsThreeEntriesPerBand)
{
    const AudioParametricEQ eq;
    const auto parameters = eq.getParameters();
    ASSERT_EQ(parameters.size(), 12u);
    EXPECT_EQ(toStdString(parameters[0].id), "band0.frequency");
    EXPECT_EQ(toStdString(parameters[1].id), "band0.gainDb");
    EXPECT_EQ(toStdString(parameters[2].id), "band0.qFactor");
    EXPECT_FLOAT_EQ(parameters[1].minValue, -48.0f);
    EXPECT_FLOAT_EQ(parameters[1].maxValue, 48.0f);
}

TEST(AudioParametricEQStandaloneTest, ZeroGainBandPreservesConstantSignal)
{
    AudioParametricEQ eq;
    auto segment = makeMono(48000, constantSamples(0.25f, 24));

    eq.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_NEAR(sample, 0.25f, 1e-5f);
}

TEST(AudioParametricEQStandaloneTest, DisabledBandDoesNotAffectItsInput)
{
    AudioParametricEQ eq;
    eq.setBand(0, {1000.0f, 24.0f, 1.0f, false});
    auto segment = makeMono(48000, constantSamples(0.25f, 24));

    eq.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_NEAR(sample, 0.25f, 1e-5f);
}

TEST(AudioParametricEQStandaloneTest, BypassPreservesInputSamplesExactly)
{
    AudioParametricEQ eq;
    eq.setBand(0, {1000.0f, 18.0f, 1.0f, true});
    eq.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.125f, -0.75f, 1.0f});

    eq.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.125f, -0.75f, 1.0f}));
}

TEST(AudioParametricEQStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioParametricEQ eq;
    eq.setBandCount(1);
    auto segment = makeMono(48000, QVector<float>{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()});

    eq.process(segment);

    for (const float sample : segment.channelData[0]) {
        EXPECT_TRUE(std::isfinite(sample));
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}

TEST(AudioParametricEQStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioParametricEQ eq;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    eq.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioParametricEQStandaloneTest, JsonRoundTripPreservesBandsAndBypass)
{
    AudioParametricEQ source;
    source.setBandCount(2);
    source.setBand(0, {120.0f, 4.5f, 1.2f, true});
    source.setBand(1, {6400.0f, -3.0f, 2.4f, false});
    source.setBypass(true);

    AudioParametricEQ restored;
    restored.fromJson(source.toJson());

    ASSERT_EQ(restored.getBandCount(), 2);
    EXPECT_FLOAT_EQ(restored.getBand(0).frequency, 120.0f);
    EXPECT_FLOAT_EQ(restored.getBand(0).gainDb, 4.5f);
    EXPECT_FLOAT_EQ(restored.getBand(0).qFactor, 1.2f);
    EXPECT_TRUE(restored.getBand(0).enabled);
    EXPECT_FLOAT_EQ(restored.getBand(1).frequency, 6400.0f);
    EXPECT_FLOAT_EQ(restored.getBand(1).gainDb, -3.0f);
    EXPECT_FLOAT_EQ(restored.getBand(1).qFactor, 2.4f);
    EXPECT_FALSE(restored.getBand(1).enabled);
    EXPECT_TRUE(restored.isBypassed());
}

