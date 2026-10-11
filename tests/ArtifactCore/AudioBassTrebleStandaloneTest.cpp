#include <gtest/gtest.h>

#include <QVector>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.BassTreble;
import Audio.Segment;

using namespace ArtifactCore;

namespace {
AudioSegment makeMono(int sampleRate, QVector<float> samples)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = AudioChannelLayout::Mono;
    segment.startFrame = 13;
    segment.channelData.push_back(std::move(samples));
    return segment;
}

QVector<float> constantSamples(float value, int count)
{
    return QVector<float>(count, value);
}
}

TEST(AudioBassTrebleStandaloneTest, ExposesExpectedDefaultIdentityAndNeutralSettings)
{
    const AudioBassTreble effect;
    EXPECT_EQ(toStdString(effect.getName()), "Bass & Treble");
    EXPECT_FLOAT_EQ(effect.getBass(), 0.0f);
    EXPECT_FLOAT_EQ(effect.getTreble(), 0.0f);
}

TEST(AudioBassTrebleStandaloneTest, SettersClampGainAndReplaceNonFiniteValues)
{
    AudioBassTreble effect;
    effect.setBass(30.0f);
    effect.setTreble(-30.0f);
    EXPECT_FLOAT_EQ(effect.getBass(), 24.0f);
    EXPECT_FLOAT_EQ(effect.getTreble(), -24.0f);

    effect.setBass(std::numeric_limits<float>::quiet_NaN());
    effect.setTreble(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.getBass(), 0.0f);
    EXPECT_FLOAT_EQ(effect.getTreble(), 0.0f);
}

TEST(AudioBassTrebleStandaloneTest, NeutralSettingsPreserveConstantSignal)
{
    AudioBassTreble effect;
    auto segment = makeMono(48000, constantSamples(0.25f, 32));

    effect.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_NEAR(sample, 0.25f, 1e-6f);
}

TEST(AudioBassTrebleStandaloneTest, BassBoostAppliesLinearGainToConstantSignal)
{
    AudioBassTreble effect;
    effect.setBass(6.0f);
    auto segment = makeMono(48000, constantSamples(0.2f, 32));

    effect.process(segment);

    const float expected = 0.2f * (std::pow(10.0f, 6.0f / 20.0f) + 1.0f) * 0.5f;
    for (const float sample : segment.channelData[0])
        EXPECT_NEAR(sample, expected, 1e-5f);
}

TEST(AudioBassTrebleStandaloneTest, TrebleCutAppliesLinearGainToConstantSignal)
{
    AudioBassTreble effect;
    effect.setTreble(-6.0f);
    auto segment = makeMono(48000, constantSamples(0.2f, 32));

    effect.process(segment);

    const float expected = 0.2f * (1.0f + std::pow(10.0f, -6.0f / 20.0f)) * 0.5f;
    for (const float sample : segment.channelData[0])
        EXPECT_NEAR(sample, expected, 1e-5f);
}

TEST(AudioBassTrebleStandaloneTest, BypassPreservesInputSamplesExactly)
{
    AudioBassTreble effect;
    effect.setBass(18.0f);
    effect.setTreble(-12.0f);
    effect.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.125f, -0.75f, 1.0f});

    effect.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.125f, -0.75f, 1.0f}));
}

TEST(AudioBassTrebleStandaloneTest, NonFiniteInputIsReplacedWithSilence)
{
    AudioBassTreble effect;
    auto segment = makeMono(48000, QVector<float>{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()});

    effect.process(segment);

    for (const float sample : segment.channelData[0]) {
        EXPECT_TRUE(std::isfinite(sample));
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}

TEST(AudioBassTrebleStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioBassTreble effect;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    effect.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioBassTrebleStandaloneTest, EmptyBlockIsAccepted)
{
    AudioBassTreble effect;
    auto segment = makeMono(48000, {});

    EXPECT_NO_THROW(effect.process(segment));
    EXPECT_TRUE(segment.channelData[0].isEmpty());
}

