#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.Reverb;
import Audio.Segment;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
AudioSegment makeMono(int sampleRate, QVector<float> samples)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = AudioChannelLayout::Mono;
    segment.startFrame = 100;
    segment.channelData.push_back(std::move(samples));
    return segment;
}
}

TEST(AudioReverbStandaloneTest, ExposesExpectedDefaultIdentityAndParameters)
{
    const AudioReverb reverb;
    EXPECT_EQ(toStdString(reverb.getName()), "Reverb");
    EXPECT_FLOAT_EQ(reverb.getDecay(), 0.5f);
    EXPECT_FLOAT_EQ(reverb.getMix(), 0.3f);
    EXPECT_FLOAT_EQ(reverb.getParameterValue(String("decay")), 0.5f);
    EXPECT_FLOAT_EQ(reverb.getParameterValue(String("mix")), 0.3f);
    EXPECT_FLOAT_EQ(reverb.getParameterValue(String("size")), 0.7f);
}

TEST(AudioReverbStandaloneTest, SettersClampParametersAndRestoreDefaultsForNonFiniteValues)
{
    AudioReverb reverb;
    reverb.setDecay(2.0f);
    reverb.setMix(-1.0f);
    reverb.setSize(-3.0f);
    EXPECT_FLOAT_EQ(reverb.getDecay(), 1.0f);
    EXPECT_FLOAT_EQ(reverb.getMix(), 0.0f);
    EXPECT_FLOAT_EQ(reverb.getParameterValue(String("size")), 0.0f);

    reverb.setDecay(std::numeric_limits<float>::infinity());
    reverb.setMix(std::numeric_limits<float>::quiet_NaN());
    reverb.setSize(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(reverb.getDecay(), 0.5f);
    EXPECT_FLOAT_EQ(reverb.getMix(), 0.3f);
    EXPECT_FLOAT_EQ(reverb.getParameterValue(String("size")), 0.7f);
}

TEST(AudioReverbStandaloneTest, ZeroMixPreservesDrySignalExactly)
{
    AudioReverb reverb;
    reverb.setMix(0.0f);
    auto segment = makeMono(48000, QVector<float>{1.0f, -0.5f, 0.25f, 0.0f});

    reverb.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{1.0f, -0.5f, 0.25f, 0.0f}));
}

TEST(AudioReverbStandaloneTest, FullWetImpulseProducesAReverbTail)
{
    AudioReverb reverb;
    reverb.setDecay(1.0f);
    reverb.setMix(1.0f);
    QVector<float> impulse(1300, 0.0f);
    impulse[0] = 1.0f;
    auto segment = makeMono(48000, std::move(impulse));

    reverb.process(segment);

    EXPECT_FLOAT_EQ(segment.channelData[0][0], 0.0f);
    bool hasTail = false;
    for (qsizetype frame = 1; frame < segment.channelData[0].size(); ++frame)
        hasTail = hasTail || segment.channelData[0][frame] != 0.0f;
    EXPECT_TRUE(hasTail);
}

TEST(AudioReverbStandaloneTest, ZeroDecayDoesNotStoreAnImpulseForLaterOutput)
{
    AudioReverb reverb;
    reverb.setDecay(0.0f);
    reverb.setMix(1.0f);
    QVector<float> impulse(1300, 0.0f);
    impulse[0] = 1.0f;
    auto segment = makeMono(48000, std::move(impulse));

    reverb.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_FLOAT_EQ(sample, 0.0f);
}

TEST(AudioReverbStandaloneTest, BypassPreservesWetAndDryInputSamples)
{
    AudioReverb reverb;
    reverb.setMix(1.0f);
    reverb.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    reverb.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioReverbStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioReverb reverb;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    auto segment = makeMono(48000, QVector<float>{nan, infinity, -infinity});

    reverb.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
}

TEST(AudioReverbStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioReverb reverb;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    reverb.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioReverbStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioReverb source;
    source.setDecay(0.85f);
    source.setMix(0.65f);
    source.setSize(0.2f);
    source.setBypass(true);

    AudioReverb restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getDecay(), 0.85f);
    EXPECT_FLOAT_EQ(restored.getMix(), 0.65f);
    EXPECT_FLOAT_EQ(restored.getParameterValue(String("size")), 0.2f);
    EXPECT_TRUE(restored.isBypassed());
}
