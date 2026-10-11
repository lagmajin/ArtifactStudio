#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.Chorus;
import Audio.Segment;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
AudioSegment makeMono(int sampleRate, QVector<float> samples)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = AudioChannelLayout::Mono;
    segment.startFrame = 27;
    segment.channelData.push_back(std::move(samples));
    return segment;
}
}

TEST(AudioChorusStandaloneTest, ExposesExpectedDefaultIdentityAndParameters)
{
    const AudioChorus chorus;
    EXPECT_EQ(toStdString(chorus.getName()), "Chorus");
    EXPECT_EQ(chorus.getMode(), AudioChorus::Mode::Chorus);
    EXPECT_FLOAT_EQ(chorus.getRate(), 1.5f);
    EXPECT_FLOAT_EQ(chorus.getDepth(), 0.5f);
    EXPECT_FLOAT_EQ(chorus.getParameterValue(String("feedback")), 0.3f);
    EXPECT_FLOAT_EQ(chorus.getParameterValue(String("delay_ms")), 20.0f);
    EXPECT_EQ(chorus.getParameters().size(), 4u);
}

TEST(AudioChorusStandaloneTest, SettersClampFiniteValuesAndRestoreDefaultsForNonFinite)
{
    AudioChorus chorus;
    chorus.setRate(20.0f);
    chorus.setDepth(-1.0f);
    chorus.setFeedback(2.0f);
    chorus.setDelayMs(0.0f);
    EXPECT_FLOAT_EQ(chorus.getRate(), 10.0f);
    EXPECT_FLOAT_EQ(chorus.getDepth(), 0.0f);
    EXPECT_FLOAT_EQ(chorus.getParameterValue(String("feedback")), 0.99f);
    EXPECT_FLOAT_EQ(chorus.getParameterValue(String("delay_ms")), 1.0f);

    chorus.setRate(std::numeric_limits<float>::quiet_NaN());
    chorus.setDepth(std::numeric_limits<float>::infinity());
    chorus.setFeedback(-std::numeric_limits<float>::infinity());
    chorus.setDelayMs(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(chorus.getRate(), 1.5f);
    EXPECT_FLOAT_EQ(chorus.getDepth(), 0.5f);
    EXPECT_FLOAT_EQ(chorus.getParameterValue(String("feedback")), 0.3f);
    EXPECT_FLOAT_EQ(chorus.getParameterValue(String("delay_ms")), 20.0f);
}

TEST(AudioChorusStandaloneTest, ShortBlockContainsOnlyTheDocumentedDryBlendBeforeDelay)
{
    AudioChorus chorus;
    chorus.setDelayMs(20.0f);
    chorus.setDepth(0.0f);
    auto segment = makeMono(48000, QVector<float>{1.0f, -0.5f, 0.25f});

    chorus.process(segment);

    EXPECT_FLOAT_EQ(segment.channelData[0][0], 0.7f);
    EXPECT_FLOAT_EQ(segment.channelData[0][1], -0.35f);
    EXPECT_FLOAT_EQ(segment.channelData[0][2], 0.175f);
}

TEST(AudioChorusStandaloneTest, BypassPreservesInputSamplesExactly)
{
    AudioChorus chorus;
    chorus.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.125f, -0.75f, 1.0f});

    chorus.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.125f, -0.75f, 1.0f}));
}

TEST(AudioChorusStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioChorus chorus;
    auto segment = makeMono(48000, QVector<float>{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()});

    chorus.process(segment);

    for (const float sample : segment.channelData[0]) {
        EXPECT_TRUE(std::isfinite(sample));
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}

TEST(AudioChorusStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioChorus chorus;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    chorus.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioChorusStandaloneTest, EmptyBlockIsAccepted)
{
    AudioChorus chorus;
    auto segment = makeMono(48000, {});

    EXPECT_NO_THROW(chorus.process(segment));
    EXPECT_TRUE(segment.channelData[0].isEmpty());
}

TEST(AudioChorusStandaloneTest, JsonRoundTripPreservesModeParametersAndBypass)
{
    AudioChorus source;
    source.setMode(AudioChorus::Mode::Flanger);
    source.setRate(4.25f);
    source.setDepth(0.8f);
    source.setFeedback(-0.4f);
    source.setDelayMs(7.5f);
    source.setBypass(true);

    AudioChorus restored;
    restored.fromJson(source.toJson());

    EXPECT_EQ(restored.getMode(), AudioChorus::Mode::Flanger);
    EXPECT_FLOAT_EQ(restored.getRate(), 4.25f);
    EXPECT_FLOAT_EQ(restored.getDepth(), 0.8f);
    EXPECT_FLOAT_EQ(restored.getParameterValue(String("feedback")), -0.4f);
    EXPECT_FLOAT_EQ(restored.getParameterValue(String("delay_ms")), 7.5f);
    EXPECT_TRUE(restored.isBypassed());
}

