#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.Vibrato;
import Audio.Segment;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
AudioSegment makeMono(int sampleRate, QVector<float> samples)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = AudioChannelLayout::Mono;
    segment.startFrame = 0;
    segment.channelData.push_back(std::move(samples));
    return segment;
}
}

TEST(AudioVibratoStandaloneTest, ExposesExpectedDefaults)
{
    const AudioVibrato vibrato;
    EXPECT_EQ(toStdString(vibrato.getName()), "Vibrato");
    EXPECT_EQ(toStdString(vibrato.effectType()), "vibrato");
    EXPECT_FLOAT_EQ(vibrato.getRate(), 6.0f);
    EXPECT_FLOAT_EQ(vibrato.getDepthMs(), 2.0f);
    EXPECT_FLOAT_EQ(vibrato.getBaseMs(), 5.0f);
    EXPECT_FLOAT_EQ(vibrato.getMix(), 1.0f);
    EXPECT_GT(vibrato.latencySamples(), 0);
    EXPECT_EQ(vibrato.tailSamples(), 0);
}

TEST(AudioVibratoStandaloneTest, SettersClampAndRestoreDefaultsForNonFinite)
{
    AudioVibrato vibrato;
    vibrato.setRate(99.0f);
    vibrato.setBaseMs(0.0f);
    EXPECT_FLOAT_EQ(vibrato.getRate(), 15.0f);
    EXPECT_FLOAT_EQ(vibrato.getBaseMs(), 3.0f);

    vibrato.setDepthMs(std::numeric_limits<float>::quiet_NaN());
    vibrato.setMix(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(vibrato.getDepthMs(), 2.0f);
    EXPECT_FLOAT_EQ(vibrato.getMix(), 1.0f);
}

TEST(AudioVibratoStandaloneTest, ZeroMixPreservesDrySignalExactly)
{
    AudioVibrato vibrato;
    vibrato.setMix(0.0f);
    auto segment = makeMono(48000, QVector<float>{1.0f, -0.5f, 0.25f, 0.0f});

    vibrato.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{1.0f, -0.5f, 0.25f, 0.0f}));
}

TEST(AudioVibratoStandaloneTest, FullWetImpulseStartsSilentDueToDelay)
{
    AudioVibrato vibrato;
    vibrato.setMix(1.0f);
    QVector<float> impulse(4800, 0.0f);
    impulse[0] = 1.0f;
    auto segment = makeMono(48000, std::move(impulse));

    vibrato.process(segment);

    EXPECT_FLOAT_EQ(segment.channelData[0][0], 0.0f);
    bool hasWet = false;
    for (const float sample : segment.channelData[0])
        hasWet = hasWet || (sample != 0.0f);
    EXPECT_TRUE(hasWet);
}

TEST(AudioVibratoStandaloneTest, BypassPreservesInputSamples)
{
    AudioVibrato vibrato;
    vibrato.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    vibrato.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioVibratoStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioVibrato vibrato;
    const float infinity = std::numeric_limits<float>::infinity();
    auto segment = makeMono(48000, QVector<float>{infinity, infinity, infinity});

    vibrato.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
}

TEST(AudioVibratoStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioVibrato vibrato;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    vibrato.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioVibratoStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioVibrato source;
    source.setRate(10.0f);
    source.setDepthMs(4.0f);
    source.setBaseMs(8.0f);
    source.setMix(0.5f);
    source.setBypass(true);

    AudioVibrato restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getRate(), 10.0f);
    EXPECT_FLOAT_EQ(restored.getDepthMs(), 4.0f);
    EXPECT_FLOAT_EQ(restored.getBaseMs(), 8.0f);
    EXPECT_FLOAT_EQ(restored.getMix(), 0.5f);
    EXPECT_TRUE(restored.isBypassed());
}
