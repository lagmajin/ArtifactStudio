#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.Tone;
import Audio.Segment;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
AudioSegment makeMono(int sampleRate, QVector<float> samples)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = AudioChannelLayout::Mono;
    segment.channelData.push_back(std::move(samples));
    return segment;
}
}

TEST(AudioToneStandaloneTest, ExposesExpectedDefaultIdentityAndParameters)
{
    const AudioTone tone;
    EXPECT_EQ(toStdString(tone.getName()), "Tone");
    EXPECT_FLOAT_EQ(tone.getFrequency(), 440.0f);
    EXPECT_FLOAT_EQ(tone.getAmplitude(), 0.2f);
    EXPECT_FALSE(tone.isBypassed());
    EXPECT_EQ(tone.getParameterValue(String("frequency")), 440.0f);
    EXPECT_EQ(tone.getParameterValue(String("amplitude")), 0.2f);
    EXPECT_EQ(tone.getParameterValue(String("wave_type")), 0.0f);
}

TEST(AudioToneStandaloneTest, SineWaveUsesSampleRateAndAddsToExistingSamples)
{
    AudioTone tone;
    tone.setFrequency(1.0f);
    tone.setAmplitude(1.0f);
    auto segment = makeMono(4, QVector<float>{1.0f, 1.0f, 1.0f, 1.0f});

    tone.process(segment);

    ASSERT_EQ(segment.channelData[0].size(), 4);
    EXPECT_NEAR(segment.channelData[0][0], 1.0f, 1e-6f);
    EXPECT_NEAR(segment.channelData[0][1], 2.0f, 1e-6f);
    EXPECT_NEAR(segment.channelData[0][2], 1.0f, 1e-6f);
    EXPECT_NEAR(segment.channelData[0][3], 0.0f, 1e-6f);
}

TEST(AudioToneStandaloneTest, SquareAndSawtoothWaveformsFollowTheirPhaseShapes)
{
    AudioTone square;
    square.setFrequency(1.0f);
    square.setAmplitude(1.0f);
    square.setWaveType(AudioTone::WaveType::Square);
    auto squareSegment = makeMono(4, QVector<float>(4, 0.0f));
    square.process(squareSegment);
    EXPECT_EQ(squareSegment.channelData[0], (QVector<float>{-1.0f, 1.0f, -1.0f, -1.0f}));

    AudioTone sawtooth;
    sawtooth.setFrequency(1.0f);
    sawtooth.setAmplitude(1.0f);
    sawtooth.setWaveType(AudioTone::WaveType::Sawtooth);
    auto sawtoothSegment = makeMono(4, QVector<float>(4, 0.0f));
    sawtooth.process(sawtoothSegment);
    EXPECT_EQ(sawtoothSegment.channelData[0], (QVector<float>{0.0f, 0.5f, -1.0f, -0.5f}));
}

TEST(AudioToneStandaloneTest, NoiseWaveformStaysWithinConfiguredAmplitude)
{
    AudioTone tone;
    constexpr float amplitude = 0.25f;
    tone.setAmplitude(amplitude);
    tone.setWaveType(AudioTone::WaveType::Noise);
    auto segment = makeMono(48000, QVector<float>(512, 0.0f));

    tone.process(segment);

    for (const float sample : segment.channelData[0]) {
        EXPECT_GE(sample, -amplitude);
        EXPECT_LE(sample, amplitude);
    }
}

TEST(AudioToneStandaloneTest, BypassPreservesInputAndDoesNotAdvanceOscillatorPhase)
{
    AudioTone tone;
    tone.setFrequency(1.0f);
    tone.setAmplitude(1.0f);
    tone.setBypass(true);
    auto bypassed = makeMono(4, QVector<float>{5.0f});

    tone.process(bypassed);
    EXPECT_EQ(bypassed.channelData[0], (QVector<float>{5.0f}));

    tone.setBypass(false);
    auto active = makeMono(4, QVector<float>{0.0f});
    tone.process(active);
    EXPECT_NEAR(active.channelData[0][0], 0.0f, 1e-6f);
}

TEST(AudioToneStandaloneTest, NonFiniteFrequencyAndAmplitudeSettersRestoreDefaults)
{
    AudioTone tone;
    tone.setFrequency(std::numeric_limits<float>::quiet_NaN());
    tone.setAmplitude(std::numeric_limits<float>::infinity());

    EXPECT_FLOAT_EQ(tone.getFrequency(), 440.0f);
    EXPECT_FLOAT_EQ(tone.getAmplitude(), 0.2f);
}

TEST(AudioToneStandaloneTest, ParameterSetterClampsWaveTypeAndIgnoresNonFiniteType)
{
    AudioTone tone;
    tone.setParameterValue(String("wave_type"), 3.7f);
    EXPECT_FLOAT_EQ(tone.getParameterValue(String("wave_type")), 3.0f);

    tone.setParameterValue(String("wave_type"),
                           std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(tone.getParameterValue(String("wave_type")), 3.0f);
    EXPECT_FLOAT_EQ(tone.getParameterValue(String("unknown")), 0.0f);
}

TEST(AudioToneStandaloneTest, JsonRoundTripPreservesToneParametersAndBypass)
{
    AudioTone source;
    source.setFrequency(1234.0f);
    source.setAmplitude(0.75f);
    source.setWaveType(AudioTone::WaveType::Square);
    source.setBypass(true);
    const QJsonObject json = source.toJson();

    AudioTone restored;
    restored.fromJson(json);

    EXPECT_FLOAT_EQ(restored.getFrequency(), 1234.0f);
    EXPECT_FLOAT_EQ(restored.getAmplitude(), 0.75f);
    EXPECT_FLOAT_EQ(restored.getParameterValue(String("wave_type")), 1.0f);
    EXPECT_TRUE(restored.isBypassed());
}

TEST(AudioToneStandaloneTest, NonPositiveSampleRateLeavesInputAndOscillatorUntouched)
{
    AudioTone tone;
    tone.setFrequency(1.0f);
    tone.setAmplitude(1.0f);
    auto invalidRate = makeMono(0, QVector<float>{5.0f});

    tone.process(invalidRate);

    EXPECT_EQ(invalidRate.channelData[0], (QVector<float>{5.0f}));
    auto validRate = makeMono(4, QVector<float>{0.0f});
    tone.process(validRate);
    EXPECT_NEAR(validRate.channelData[0][0], 0.0f, 1e-6f);
}
