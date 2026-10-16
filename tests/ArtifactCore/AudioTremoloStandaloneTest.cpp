#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.Tremolo;
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

TEST(AudioTremoloStandaloneTest, ExposesExpectedDefaults)
{
    const AudioTremolo tremolo;
    EXPECT_EQ(toStdString(tremolo.getName()), "Tremolo");
    EXPECT_EQ(toStdString(tremolo.effectType()), "tremolo");
    EXPECT_FLOAT_EQ(tremolo.getRate(), 5.0f);
    EXPECT_FLOAT_EQ(tremolo.getDepth(), 0.8f);
    EXPECT_FLOAT_EQ(tremolo.getShape(), 0.0f);
    EXPECT_FLOAT_EQ(tremolo.getWidth(), 0.0f);
    EXPECT_FLOAT_EQ(tremolo.getMix(), 1.0f);
}

TEST(AudioTremoloStandaloneTest, SettersClampAndRestoreDefaultsForNonFinite)
{
    AudioTremolo tremolo;
    tremolo.setRate(99.0f);
    tremolo.setDepth(-1.0f);
    EXPECT_FLOAT_EQ(tremolo.getRate(), 20.0f);
    EXPECT_FLOAT_EQ(tremolo.getDepth(), 0.0f);

    tremolo.setRate(std::numeric_limits<float>::quiet_NaN());
    tremolo.setMix(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(tremolo.getRate(), 5.0f);
    EXPECT_FLOAT_EQ(tremolo.getMix(), 1.0f);
}

TEST(AudioTremoloStandaloneTest, ZeroDepthLeavesSignalUnchanged)
{
    AudioTremolo tremolo;
    tremolo.setDepth(0.0f);
    auto segment = makeMono(48000, QVector<float>{1.0f, -0.5f, 0.25f, 0.0f});

    tremolo.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{1.0f, -0.5f, 0.25f, 0.0f}));
}

TEST(AudioTremoloStandaloneTest, DepthModulatesConstantSignal)
{
    AudioTremolo tremolo;
    tremolo.setRate(5.0f);
    tremolo.setDepth(0.8f);
    auto segment = makeMono(48000, QVector<float>(4800, 1.0f));

    tremolo.process(segment);

    bool differs = false;
    for (const float sample : segment.channelData[0]) {
        differs = differs || (sample != 1.0f);
        EXPECT_TRUE(std::isfinite(sample));
        EXPECT_GE(sample, 0.0f);
        EXPECT_LE(sample, 1.0f);
    }
    EXPECT_TRUE(differs);
}

TEST(AudioTremoloStandaloneTest, BypassPreservesInputSamples)
{
    AudioTremolo tremolo;
    tremolo.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    tremolo.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioTremoloStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioTremolo tremolo;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    auto segment = makeMono(48000, QVector<float>{nan, nan, nan});

    tremolo.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
}

TEST(AudioTremoloStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioTremolo tremolo;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    tremolo.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioTremoloStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioTremolo source;
    source.setRate(8.0f);
    source.setDepth(0.5f);
    source.setShape(1.0f);
    source.setWidth(1.0f);
    source.setMix(0.7f);
    source.setBypass(true);

    AudioTremolo restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getRate(), 8.0f);
    EXPECT_FLOAT_EQ(restored.getDepth(), 0.5f);
    EXPECT_FLOAT_EQ(restored.getShape(), 1.0f);
    EXPECT_FLOAT_EQ(restored.getWidth(), 1.0f);
    EXPECT_FLOAT_EQ(restored.getMix(), 0.7f);
    EXPECT_TRUE(restored.isBypassed());
}
