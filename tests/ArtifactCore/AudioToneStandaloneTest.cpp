#include <gtest/gtest.h>

#include <QJsonObject>
#include <QString>
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

TEST(AudioToneStandaloneTest, ParameterSnapshotReflectsUpdatedSettings)
{
    AudioTone tone;
    tone.setFrequency(880.0f);
    tone.setAmplitude(0.65f);
    tone.setWaveType(AudioTone::WaveType::Sawtooth);

    const auto parameters = tone.getParameters();

    ASSERT_EQ(parameters.size(), 3u);
    EXPECT_FLOAT_EQ(parameters[0].value, 880.0f);
    EXPECT_FLOAT_EQ(parameters[1].value, 0.65f);
    EXPECT_FLOAT_EQ(parameters[2].value, 2.0f);
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

TEST(AudioToneStandaloneTest, StereoChannelsRenderTheSameSynchronizedTone)
{
    AudioTone tone;
    tone.setFrequency(1.0f);
    tone.setAmplitude(1.0f);
    AudioSegment segment;
    segment.sampleRate = 4;
    segment.layout = AudioChannelLayout::Stereo;
    segment.channelData = {QVector<float>(3, 0.0f), QVector<float>(3, 0.0f)};

    tone.process(segment);

    EXPECT_EQ(segment.channelData[0], segment.channelData[1]);
    EXPECT_NEAR(segment.channelData[0][0], 0.0f, 1e-6f);
    EXPECT_NEAR(segment.channelData[0][1], 1.0f, 1e-6f);
    EXPECT_NEAR(segment.channelData[0][2], 0.0f, 1e-6f);
}

TEST(AudioToneStandaloneTest, ChannelsBeyondStereoRemainUntouched)
{
    AudioTone tone;
    tone.setFrequency(1.0f);
    tone.setAmplitude(1.0f);
    AudioSegment segment;
    segment.sampleRate = 4;
    segment.layout = AudioChannelLayout::Surround51;
    segment.channelData = {
        QVector<float>(2, 0.0f),
        QVector<float>(2, 0.0f),
        QVector<float>{0.25f, -0.5f}};

    tone.process(segment);

    EXPECT_EQ(segment.channelData[2], (QVector<float>{0.25f, -0.5f}));
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

TEST(AudioToneStandaloneTest, ProcessingClampsFiniteFrequencyToSupportedRange)
{
    AudioTone negativeFrequency;
    AudioTone zeroFrequency;
    negativeFrequency.setFrequency(-1.0f);
    zeroFrequency.setFrequency(0.0f);
    negativeFrequency.setAmplitude(1.0f);
    zeroFrequency.setAmplitude(1.0f);
    auto negativeSegment = makeMono(44100, QVector<float>(16, 0.0f));
    auto zeroSegment = makeMono(44100, QVector<float>(16, 0.0f));

    negativeFrequency.process(negativeSegment);
    zeroFrequency.process(zeroSegment);
    EXPECT_EQ(negativeSegment.channelData[0], zeroSegment.channelData[0]);

    AudioTone aboveMaximum;
    AudioTone maximumFrequency;
    aboveMaximum.setFrequency(120000.0f);
    maximumFrequency.setFrequency(96000.0f);
    aboveMaximum.setAmplitude(1.0f);
    maximumFrequency.setAmplitude(1.0f);
    auto aboveMaximumSegment = makeMono(44100, QVector<float>(16, 0.0f));
    auto maximumSegment = makeMono(44100, QVector<float>(16, 0.0f));

    aboveMaximum.process(aboveMaximumSegment);
    maximumFrequency.process(maximumSegment);
    EXPECT_EQ(aboveMaximumSegment.channelData[0], maximumSegment.channelData[0]);
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

TEST(AudioToneStandaloneTest, JsonWaveTypeClampsToSupportedEnumRange)
{
    AudioTone belowRange;
    QJsonObject lowValue;
    lowValue.insert(QStringLiteral("wave_type"), -3);
    belowRange.fromJson(lowValue);
    EXPECT_FLOAT_EQ(belowRange.getParameterValue(String("wave_type")), 0.0f);

    AudioTone aboveRange;
    QJsonObject highValue;
    highValue.insert(QStringLiteral("wave_type"), 12);
    aboveRange.fromJson(highValue);
    EXPECT_FLOAT_EQ(aboveRange.getParameterValue(String("wave_type")), 3.0f);
}

TEST(AudioToneStandaloneTest, InvalidJsonWaveTypeFallsBackToSine)
{
    AudioTone tone;
    tone.setWaveType(AudioTone::WaveType::Sawtooth);
    QJsonObject malformed;
    malformed.insert(QStringLiteral("wave_type"), QStringLiteral("noise"));

    tone.fromJson(malformed);

    EXPECT_FLOAT_EQ(tone.getParameterValue(String("wave_type")), 0.0f);
}

TEST(AudioToneStandaloneTest, MissingJsonSettingsRestoreToneDefaults)
{
    AudioTone tone;
    tone.setFrequency(880.0f);
    tone.setAmplitude(0.65f);
    tone.setWaveType(AudioTone::WaveType::Noise);

    tone.fromJson(QJsonObject{});

    EXPECT_FLOAT_EQ(tone.getFrequency(), 440.0f);
    EXPECT_FLOAT_EQ(tone.getAmplitude(), 0.2f);
    EXPECT_FLOAT_EQ(tone.getParameterValue(String("wave_type")), 0.0f);
}

TEST(AudioToneStandaloneTest, WrongJsonNumericSettingTypesRestoreToneDefaults)
{
    AudioTone tone;
    tone.setFrequency(880.0f);
    tone.setAmplitude(0.65f);
    QJsonObject malformed;
    malformed.insert(QStringLiteral("frequency"), QStringLiteral("fast"));
    malformed.insert(QStringLiteral("amplitude"), QStringLiteral("loud"));

    tone.fromJson(malformed);

    EXPECT_FLOAT_EQ(tone.getFrequency(), 440.0f);
    EXPECT_FLOAT_EQ(tone.getAmplitude(), 0.2f);
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

TEST(AudioToneStandaloneTest, ZeroFrameInputDoesNotAdvanceOscillatorPhase)
{
    AudioTone afterEmpty;
    afterEmpty.setFrequency(1.0f);
    afterEmpty.setAmplitude(1.0f);
    auto empty = makeMono(4, QVector<float>{});
    afterEmpty.process(empty);

    AudioTone fresh;
    fresh.setFrequency(1.0f);
    fresh.setAmplitude(1.0f);
    auto nextAfterEmpty = makeMono(4, QVector<float>{0.0f});
    auto freshInput = makeMono(4, QVector<float>{0.0f});
    afterEmpty.process(nextAfterEmpty);
    fresh.process(freshInput);

    EXPECT_EQ(nextAfterEmpty.channelData[0], freshInput.channelData[0]);
}

TEST(AudioToneStandaloneTest, ZeroAmplitudeStillAdvancesOscillatorPhase)
{
    AudioTone tone;
    tone.setFrequency(1.0f);
    tone.setAmplitude(0.0f);
    auto silentFrame = makeMono(4, QVector<float>{0.0f});
    tone.process(silentFrame);

    tone.setAmplitude(1.0f);
    auto audibleFrame = makeMono(4, QVector<float>{0.0f});
    tone.process(audibleFrame);

    EXPECT_NEAR(audibleFrame.channelData[0][0], 1.0f, 1e-6f);
}

TEST(AudioToneStandaloneTest, OscillatorPhaseContinuesAcrossAdjacentSegments)
{
    AudioTone tone;
    tone.setFrequency(1.0f);
    tone.setAmplitude(1.0f);

    auto first = makeMono(4, QVector<float>{0.0f});
    auto second = makeMono(4, QVector<float>{0.0f});
    tone.process(first);
    tone.process(second);

    EXPECT_NEAR(first.channelData[0][0], 0.0f, 1e-6f);
    EXPECT_NEAR(second.channelData[0][0], 1.0f, 1e-6f);
}

TEST(AudioToneStandaloneTest, NonFiniteInputSamplesAreReplacedBeforeToneIsAdded)
{
    AudioTone tone;
    tone.setFrequency(1.0f);
    tone.setAmplitude(1.0f);
    auto segment = makeMono(4, QVector<float>{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()});

    tone.process(segment);

    ASSERT_EQ(segment.channelData[0].size(), 3);
    EXPECT_FLOAT_EQ(segment.channelData[0][0], 0.0f);
    EXPECT_FLOAT_EQ(segment.channelData[0][1], 1.0f);
    EXPECT_NEAR(segment.channelData[0][2], 0.0f, 1e-6f);
    for (const float sample : segment.channelData[0]) {
        EXPECT_TRUE(std::isfinite(sample));
    }
}

TEST(AudioToneStandaloneTest, FiniteMixOverflowSaturatesAtFloatLimits)
{
    const float maximum = std::numeric_limits<float>::max();
    AudioTone positiveTone;
    positiveTone.setFrequency(1.0f);
    positiveTone.setAmplitude(maximum);
    auto positive = makeMono(4, QVector<float>{0.0f, maximum});
    positiveTone.process(positive);

    AudioTone negativeTone;
    negativeTone.setFrequency(1.0f);
    negativeTone.setAmplitude(-maximum);
    auto negative = makeMono(4, QVector<float>{0.0f, -maximum});
    negativeTone.process(negative);

    EXPECT_TRUE(std::isfinite(positive.channelData[0][1]));
    EXPECT_FLOAT_EQ(positive.channelData[0][1], maximum);
    EXPECT_TRUE(std::isfinite(negative.channelData[0][1]));
    EXPECT_FLOAT_EQ(negative.channelData[0][1], -maximum);
}
