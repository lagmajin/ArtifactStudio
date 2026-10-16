#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.DeEsser;
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

QVector<float> makeSine(float freq, int sampleRate, int frames, float amp)
{
    QVector<float> data(frames);
    for (int i = 0; i < frames; ++i) {
        data[i] = amp * std::sin(2.0f * 3.14159265f * freq * i / sampleRate);
    }
    return data;
}
}

TEST(AudioDeEsserStandaloneTest, ExposesExpectedDefaults)
{
    const AudioDeEsser deesser;
    EXPECT_EQ(toStdString(deesser.getName()), "De-Esser");
    EXPECT_EQ(toStdString(deesser.effectType()), "de_esser");
    EXPECT_FLOAT_EQ(deesser.getFrequency(), 6500.0f);
    EXPECT_FLOAT_EQ(deesser.getThresholdDb(), -30.0f);
    EXPECT_FLOAT_EQ(deesser.getReduction(), 0.6f);
}

TEST(AudioDeEsserStandaloneTest, SettersClampAndRestoreDefaultsForNonFinite)
{
    AudioDeEsser deesser;
    deesser.setFrequency(99999.0f);
    deesser.setReduction(-1.0f);
    EXPECT_FLOAT_EQ(deesser.getFrequency(), 12000.0f);
    EXPECT_FLOAT_EQ(deesser.getReduction(), 0.0f);

    deesser.setFrequency(std::numeric_limits<float>::quiet_NaN());
    deesser.setThresholdDb(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(deesser.getFrequency(), 6500.0f);
    EXPECT_FLOAT_EQ(deesser.getThresholdDb(), -30.0f);
}

TEST(AudioDeEsserStandaloneTest, ZeroReductionLeavesSignalUnchanged)
{
    AudioDeEsser deesser;
    deesser.setReduction(0.0f);
    auto segment = makeMono(48000, makeSine(8000.0f, 48000, 64, 1.0f));
    const QVector<float> before = segment.channelData[0];

    deesser.process(segment);

    EXPECT_EQ(segment.channelData[0], before);
}

TEST(AudioDeEsserStandaloneTest, AttenuatesLoudHighFrequencyContent)
{
    AudioDeEsser deesser;
    deesser.setFrequency(6500.0f);
    deesser.setThresholdDb(-30.0f);
    deesser.setReduction(1.0f);
    auto segment = makeMono(48000, makeSine(8000.0f, 48000, 4800, 1.0f));
    const QVector<float> before = segment.channelData[0];

    deesser.process(segment);

    bool differs = false;
    for (qsizetype i = 0; i < before.size(); ++i) {
        EXPECT_TRUE(std::isfinite(segment.channelData[0][i]));
        differs = differs || (segment.channelData[0][i] != before[i]);
    }
    EXPECT_TRUE(differs);
}

TEST(AudioDeEsserStandaloneTest, BypassPreservesInputSamples)
{
    AudioDeEsser deesser;
    deesser.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    deesser.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioDeEsserStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioDeEsser deesser;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    auto segment = makeMono(48000, QVector<float>{nan, nan, nan});

    deesser.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
}

TEST(AudioDeEsserStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioDeEsser deesser;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    deesser.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioDeEsserStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioDeEsser source;
    source.setFrequency(8000.0f);
    source.setThresholdDb(-20.0f);
    source.setReduction(0.8f);
    source.setBypass(true);

    AudioDeEsser restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getFrequency(), 8000.0f);
    EXPECT_FLOAT_EQ(restored.getThresholdDb(), -20.0f);
    EXPECT_FLOAT_EQ(restored.getReduction(), 0.8f);
    EXPECT_TRUE(restored.isBypassed());
}
