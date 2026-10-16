#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.SpringReverb;
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

TEST(AudioSpringReverbStandaloneTest, ExposesExpectedDefaults)
{
    const AudioSpringReverb spring;
    EXPECT_EQ(toStdString(spring.getName()), "Spring Reverb");
    EXPECT_EQ(toStdString(spring.effectType()), "spring_reverb");
    EXPECT_FLOAT_EQ(spring.getDecay(), 0.8f);
    EXPECT_FLOAT_EQ(spring.getBrightness(), 0.5f);
    EXPECT_FLOAT_EQ(spring.getTension(), 1.0f);
    EXPECT_FLOAT_EQ(spring.getMix(), 0.3f);
    EXPECT_EQ(spring.latencySamples(), 0);
    EXPECT_GT(spring.tailSamples(), 0);
}

TEST(AudioSpringReverbStandaloneTest, SettersClampAndRestoreDefaultsForNonFinite)
{
    AudioSpringReverb spring;
    spring.setDecay(99.0f);
    spring.setTension(0.0f);
    spring.setMix(-1.0f);
    EXPECT_FLOAT_EQ(spring.getDecay(), 0.99f);
    EXPECT_FLOAT_EQ(spring.getTension(), 0.5f);
    EXPECT_FLOAT_EQ(spring.getMix(), 0.0f);

    spring.setDecay(std::numeric_limits<float>::quiet_NaN());
    spring.setBrightness(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(spring.getDecay(), 0.8f);
    EXPECT_FLOAT_EQ(spring.getBrightness(), 0.5f);
}

TEST(AudioSpringReverbStandaloneTest, ZeroMixPreservesDrySignalExactly)
{
    AudioSpringReverb spring;
    spring.setMix(0.0f);
    auto segment = makeMono(48000, QVector<float>{1.0f, -0.5f, 0.25f, 0.0f});

    spring.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{1.0f, -0.5f, 0.25f, 0.0f}));
}

TEST(AudioSpringReverbStandaloneTest, FullWetImpulseProducesAReverbTail)
{
    AudioSpringReverb spring;
    spring.setDecay(0.9f);
    spring.setMix(1.0f);
    // 最長ループ 31ms * 1.5 + 系列遅延ぶん確保する。
    QVector<float> impulse(6000, 0.0f);
    impulse[0] = 1.0f;
    auto segment = makeMono(48000, std::move(impulse));

    spring.process(segment);

    // ループ読み出しが空のため先頭は無音。
    EXPECT_FLOAT_EQ(segment.channelData[0][0], 0.0f);
    bool hasTail = false;
    for (const float sample : segment.channelData[0])
        hasTail = hasTail || (sample != 0.0f);
    EXPECT_TRUE(hasTail);
}

TEST(AudioSpringReverbStandaloneTest, BypassPreservesInputSamples)
{
    AudioSpringReverb spring;
    spring.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    spring.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioSpringReverbStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioSpringReverb spring;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    auto segment = makeMono(48000, QVector<float>{nan, infinity, -infinity});

    spring.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
}

TEST(AudioSpringReverbStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioSpringReverb spring;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    spring.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioSpringReverbStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioSpringReverb source;
    source.setDecay(0.9f);
    source.setBrightness(0.8f);
    source.setTension(1.2f);
    source.setMix(0.6f);
    source.setBypass(true);

    AudioSpringReverb restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getDecay(), 0.9f);
    EXPECT_FLOAT_EQ(restored.getBrightness(), 0.8f);
    EXPECT_FLOAT_EQ(restored.getTension(), 1.2f);
    EXPECT_FLOAT_EQ(restored.getMix(), 0.6f);
    EXPECT_TRUE(restored.isBypassed());
}
