#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.Expander;
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

TEST(AudioExpanderStandaloneTest, ExposesExpectedDefaults)
{
    const AudioExpander expander;
    EXPECT_EQ(toStdString(expander.getName()), "Expander");
    EXPECT_EQ(toStdString(expander.effectType()), "expander");
    EXPECT_FLOAT_EQ(expander.getThresholdDb(), -40.0f);
    EXPECT_FLOAT_EQ(expander.getRatio(), 2.0f);
    EXPECT_FLOAT_EQ(expander.getAttackMs(), 10.0f);
    EXPECT_FLOAT_EQ(expander.getReleaseMs(), 150.0f);
}

TEST(AudioExpanderStandaloneTest, SettersClampAndRestoreDefaultsForNonFinite)
{
    AudioExpander expander;
    expander.setThresholdDb(99.0f);
    expander.setRatio(99.0f);
    EXPECT_FLOAT_EQ(expander.getThresholdDb(), 0.0f);
    EXPECT_FLOAT_EQ(expander.getRatio(), 10.0f);

    expander.setThresholdDb(std::numeric_limits<float>::quiet_NaN());
    expander.setAttackMs(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(expander.getThresholdDb(), -40.0f);
    EXPECT_FLOAT_EQ(expander.getAttackMs(), 10.0f);
}

TEST(AudioExpanderStandaloneTest, UnityRatioLeavesSignalUnchanged)
{
    AudioExpander expander;
    expander.setRatio(1.0f);
    auto segment = makeMono(48000, QVector<float>{1.0f, -0.5f, 0.25f, 0.001f});

    expander.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{1.0f, -0.5f, 0.25f, 0.001f}));
}

TEST(AudioExpanderStandaloneTest, AttenuatesSignalBelowThreshold)
{
    AudioExpander expander;
    expander.setThresholdDb(-40.0f);
    expander.setRatio(4.0f);
    // -60dB の微小信号は抑えられる。
    auto segment = makeMono(48000, QVector<float>(4800, 0.001f));

    expander.process(segment);

    for (const float sample : segment.channelData[0]) {
        EXPECT_TRUE(std::isfinite(sample));
        EXPECT_LE(std::abs(sample), 0.001f);
    }
    EXPECT_LT(std::abs(segment.channelData[0].last()), 0.001f);
}

TEST(AudioExpanderStandaloneTest, BypassPreservesInputSamples)
{
    AudioExpander expander;
    expander.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    expander.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioExpanderStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioExpander expander;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    auto segment = makeMono(48000, QVector<float>{nan, nan, nan});

    expander.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
}

TEST(AudioExpanderStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioExpander expander;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    expander.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioExpanderStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioExpander source;
    source.setThresholdDb(-20.0f);
    source.setRatio(3.0f);
    source.setAttackMs(5.0f);
    source.setReleaseMs(300.0f);
    source.setBypass(true);

    AudioExpander restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getThresholdDb(), -20.0f);
    EXPECT_FLOAT_EQ(restored.getRatio(), 3.0f);
    EXPECT_FLOAT_EQ(restored.getAttackMs(), 5.0f);
    EXPECT_FLOAT_EQ(restored.getReleaseMs(), 300.0f);
    EXPECT_TRUE(restored.isBypassed());
}
