#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.Phaser;
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

TEST(AudioPhaserStandaloneTest, ExposesExpectedDefaults)
{
    const AudioPhaser phaser;
    EXPECT_EQ(toStdString(phaser.getName()), "Phaser");
    EXPECT_EQ(toStdString(phaser.effectType()), "phaser");
    EXPECT_FLOAT_EQ(phaser.getRate(), 0.5f);
    EXPECT_FLOAT_EQ(phaser.getDepth(), 0.6f);
    EXPECT_FLOAT_EQ(phaser.getCenter(), 800.0f);
    EXPECT_FLOAT_EQ(phaser.getFeedback(), 0.4f);
    EXPECT_FLOAT_EQ(phaser.getStages(), 4.0f);
    EXPECT_FLOAT_EQ(phaser.getMix(), 0.5f);
}

TEST(AudioPhaserStandaloneTest, SettersClampAndRestoreDefaultsForNonFinite)
{
    AudioPhaser phaser;
    phaser.setRate(99.0f);
    phaser.setStages(99.0f);
    phaser.setMix(-1.0f);
    EXPECT_FLOAT_EQ(phaser.getRate(), 10.0f);
    EXPECT_FLOAT_EQ(phaser.getStages(), 8.0f);
    EXPECT_FLOAT_EQ(phaser.getMix(), 0.0f);

    phaser.setRate(std::numeric_limits<float>::quiet_NaN());
    phaser.setCenter(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(phaser.getRate(), 0.5f);
    EXPECT_FLOAT_EQ(phaser.getCenter(), 800.0f);
}

TEST(AudioPhaserStandaloneTest, ZeroMixPreservesDrySignalExactly)
{
    AudioPhaser phaser;
    phaser.setMix(0.0f);
    auto segment = makeMono(48000, QVector<float>{1.0f, -0.5f, 0.25f, 0.0f});

    phaser.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{1.0f, -0.5f, 0.25f, 0.0f}));
}

TEST(AudioPhaserStandaloneTest, FullWetProcessingAudiblyChangesSignal)
{
    AudioPhaser phaser;
    phaser.setMix(1.0f);
    auto segment = makeMono(48000, QVector<float>{1.0f, 1.0f, 1.0f, 1.0f});

    phaser.process(segment);

    bool differs = false;
    for (const float sample : segment.channelData[0]) {
        differs = differs || (sample != 1.0f);
        EXPECT_TRUE(std::isfinite(sample));
    }
    EXPECT_TRUE(differs);
}

TEST(AudioPhaserStandaloneTest, BypassPreservesInputSamples)
{
    AudioPhaser phaser;
    phaser.setMix(1.0f);
    phaser.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    phaser.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioPhaserStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioPhaser phaser;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    auto segment = makeMono(48000, QVector<float>{nan, infinity, -infinity});

    phaser.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
}

TEST(AudioPhaserStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioPhaser phaser;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    phaser.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioPhaserStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioPhaser source;
    source.setRate(2.5f);
    source.setDepth(0.9f);
    source.setCenter(1200.0f);
    source.setFeedback(0.7f);
    source.setStages(6.0f);
    source.setMix(0.8f);
    source.setBypass(true);

    AudioPhaser restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getRate(), 2.5f);
    EXPECT_FLOAT_EQ(restored.getDepth(), 0.9f);
    EXPECT_FLOAT_EQ(restored.getCenter(), 1200.0f);
    EXPECT_FLOAT_EQ(restored.getFeedback(), 0.7f);
    EXPECT_FLOAT_EQ(restored.getStages(), 6.0f);
    EXPECT_FLOAT_EQ(restored.getMix(), 0.8f);
    EXPECT_TRUE(restored.isBypassed());
}
