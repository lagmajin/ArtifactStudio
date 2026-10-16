#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.DCBlocker;
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

TEST(AudioDCBlockerStandaloneTest, ExposesExpectedDefaults)
{
    const AudioDCBlocker dc;
    EXPECT_EQ(toStdString(dc.getName()), "DC Blocker");
    EXPECT_EQ(toStdString(dc.effectType()), "dc_blocker");
    EXPECT_FLOAT_EQ(dc.getFrequency(), 10.0f);
    EXPECT_FLOAT_EQ(dc.getMix(), 1.0f);
}

TEST(AudioDCBlockerStandaloneTest, SettersClampAndRestoreDefaultsForNonFinite)
{
    AudioDCBlocker dc;
    dc.setFrequency(9999.0f);
    dc.setMix(-1.0f);
    EXPECT_FLOAT_EQ(dc.getFrequency(), 100.0f);
    EXPECT_FLOAT_EQ(dc.getMix(), 0.0f);

    dc.setFrequency(std::numeric_limits<float>::quiet_NaN());
    dc.setMix(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(dc.getFrequency(), 10.0f);
    EXPECT_FLOAT_EQ(dc.getMix(), 1.0f);
}

TEST(AudioDCBlockerStandaloneTest, ZeroMixPreservesDrySignalExactly)
{
    AudioDCBlocker dc;
    dc.setMix(0.0f);
    auto segment = makeMono(48000, QVector<float>{1.0f, -0.5f, 0.25f, 0.0f});

    dc.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{1.0f, -0.5f, 0.25f, 0.0f}));
}

TEST(AudioDCBlockerStandaloneTest, RemovesConstantDcOffset)
{
    AudioDCBlocker dc;
    dc.setFrequency(10.0f);
    dc.setMix(1.0f);
    auto segment = makeMono(48000, QVector<float>(4800, 0.5f));

    dc.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
    // 十分な時間の後は直流がほぼ消える。
    EXPECT_LT(std::abs(segment.channelData[0].last()), 0.05f);
}

TEST(AudioDCBlockerStandaloneTest, BypassPreservesInputSamples)
{
    AudioDCBlocker dc;
    dc.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    dc.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioDCBlockerStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioDCBlocker dc;
    const float infinity = std::numeric_limits<float>::infinity();
    auto segment = makeMono(48000, QVector<float>{infinity, infinity, infinity});

    dc.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
}

TEST(AudioDCBlockerStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioDCBlocker dc;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    dc.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioDCBlockerStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioDCBlocker source;
    source.setFrequency(30.0f);
    source.setMix(0.8f);
    source.setBypass(true);

    AudioDCBlocker restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getFrequency(), 30.0f);
    EXPECT_FLOAT_EQ(restored.getMix(), 0.8f);
    EXPECT_TRUE(restored.isBypassed());
}
