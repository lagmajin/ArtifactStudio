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

TEST(AudioBassTrebleStandaloneTest, BassBoostFavorsLowToneOverHighTone)
{
    AudioBassTreble lowFrequencyEffect;
    AudioBassTreble highFrequencyEffect;
    lowFrequencyEffect.setBass(12.0f);
    highFrequencyEffect.setBass(12.0f);

    constexpr int sampleRate = 48000;
    constexpr int frameCount = 4800;
    QVector<float> lowTone(frameCount);
    QVector<float> highTone(frameCount);
    for (int frame = 0; frame < frameCount; ++frame) {
        lowTone[frame] = 0.5f * std::sin(2.0f * 3.14159265f * 50.0f
                                        * frame / sampleRate);
        highTone[frame] = 0.5f * std::sin(2.0f * 3.14159265f * 12000.0f
                                          * frame / sampleRate);
    }
    auto lowSegment = makeMono(sampleRate, std::move(lowTone));
    auto highSegment = makeMono(sampleRate, std::move(highTone));

    lowFrequencyEffect.process(lowSegment);
    highFrequencyEffect.process(highSegment);

    double lowEnergy = 0.0;
    double highEnergy = 0.0;
    for (int frame = 480; frame < frameCount; ++frame) {
        const double lowSample = lowSegment.channelData[0][frame];
        const double highSample = highSegment.channelData[0][frame];
        lowEnergy += lowSample * lowSample;
        highEnergy += highSample * highSample;
    }

    EXPECT_GT(lowEnergy, highEnergy);
}

TEST(AudioBassTrebleStandaloneTest, ProcessingInBlocksPreservesFilterStateContinuity)
{
    AudioBassTreble contiguous;
    AudioBassTreble blocked;
    contiguous.setBass(9.0f);
    contiguous.setTreble(-5.0f);
    blocked.setBass(9.0f);
    blocked.setTreble(-5.0f);

    const QVector<float> input{
        1.0f, 0.0f, 0.0f, -0.25f, 0.5f, 0.0f, 0.0f, 0.75f,
        -1.0f, 0.0f, 0.125f};
    auto whole = makeMono(48000, input);
    contiguous.process(whole);

    auto first = makeMono(48000, QVector<float>{input[0], input[1], input[2]});
    auto second = makeMono(48000, QVector<float>{input[3], input[4], input[5], input[6]});
    auto third = makeMono(48000, QVector<float>{input[7], input[8], input[9], input[10]});
    blocked.process(first);
    blocked.process(second);
    blocked.process(third);

    int outputIndex = 0;
    for (const auto* block : {&first, &second, &third}) {
        for (const float sample : block->channelData[0]) {
            EXPECT_FLOAT_EQ(sample, whole.channelData[0][outputIndex]);
            ++outputIndex;
        }
    }
    EXPECT_EQ(outputIndex, input.size());
}

TEST(AudioBassTrebleStandaloneTest, SampleRateChangeResetsFilterHistory)
{
    AudioBassTreble withHistory;
    withHistory.setBass(12.0f);
    withHistory.setTreble(-8.0f);
    auto warmup = makeMono(48000, QVector<float>{1.0f, 0.0f, 0.25f});
    withHistory.process(warmup);

    auto changedRate = makeMono(24000, QVector<float>{0.5f, -0.25f, 0.0f, 0.75f});
    auto freshInput = makeMono(24000, changedRate.channelData[0]);
    withHistory.process(changedRate);

    AudioBassTreble fresh;
    fresh.setBass(12.0f);
    fresh.setTreble(-8.0f);
    fresh.process(freshInput);

    EXPECT_EQ(changedRate.channelData[0], freshInput.channelData[0]);
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

TEST(AudioBassTrebleStandaloneTest, UnequalChannelLengthsPreserveLongerChannelTail)
{
    AudioBassTreble effect;
    effect.setBass(12.0f);
    AudioSegment segment;
    segment.sampleRate = 48000;
    segment.layout = AudioChannelLayout::Stereo;
    segment.channelData = {
        QVector<float>{0.25f, 0.5f, 99.0f},
        QVector<float>{-0.25f, -0.5f}};
    ASSERT_EQ(segment.frameCount(), 2);

    effect.process(segment);

    ASSERT_EQ(segment.channelData[0].size(), 3);
    EXPECT_FLOAT_EQ(segment.channelData[0][2], 99.0f);
    EXPECT_EQ(segment.channelData[1].size(), 2);
}

