#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.BitCrusher;
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

TEST(AudioBitCrusherStandaloneTest, ExposesExpectedDefaults)
{
    const AudioBitCrusher crusher;
    EXPECT_EQ(toStdString(crusher.getName()), "Bit Crusher");
    EXPECT_EQ(toStdString(crusher.effectType()), "bit_crusher");
    EXPECT_FLOAT_EQ(crusher.getBits(), 8.0f);
    EXPECT_FLOAT_EQ(crusher.getDownsample(), 1.0f);
    EXPECT_FLOAT_EQ(crusher.getMix(), 1.0f);
}

TEST(AudioBitCrusherStandaloneTest, SettersClampAndRestoreDefaultsForNonFinite)
{
    AudioBitCrusher crusher;
    crusher.setBits(0.0f);
    crusher.setDownsample(999.0f);
    EXPECT_FLOAT_EQ(crusher.getBits(), 2.0f);
    EXPECT_FLOAT_EQ(crusher.getDownsample(), 40.0f);

    crusher.setBits(std::numeric_limits<float>::quiet_NaN());
    crusher.setMix(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(crusher.getBits(), 8.0f);
    EXPECT_FLOAT_EQ(crusher.getMix(), 1.0f);
}

TEST(AudioBitCrusherStandaloneTest, TransparentSettingsLeaveSignalUnchanged)
{
    AudioBitCrusher crusher;
    crusher.setBits(16.0f);
    crusher.setDownsample(1.0f);
    auto segment = makeMono(48000, QVector<float>{0.5f, -0.25f, 0.125f, 0.0f});

    crusher.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.5f, -0.25f, 0.125f, 0.0f}));
}

TEST(AudioBitCrusherStandaloneTest, LowBitDepthQuantizesSignal)
{
    AudioBitCrusher crusher;
    crusher.setBits(2.0f);
    crusher.setDownsample(1.0f);
    crusher.setMix(1.0f);
    auto segment = makeMono(48000, makeSine(440.0f, 48000, 480, 0.5f));
    const QVector<float> before = segment.channelData[0];

    crusher.process(segment);

    bool differs = false;
    for (qsizetype i = 0; i < before.size(); ++i) {
        EXPECT_TRUE(std::isfinite(segment.channelData[0][i]));
        differs = differs || (segment.channelData[0][i] != before[i]);
    }
    EXPECT_TRUE(differs);
}

TEST(AudioBitCrusherStandaloneTest, BypassPreservesInputSamples)
{
    AudioBitCrusher crusher;
    crusher.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    crusher.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioBitCrusherStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioBitCrusher crusher;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    auto segment = makeMono(48000, QVector<float>{nan, nan, nan});

    crusher.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
}

TEST(AudioBitCrusherStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioBitCrusher source;
    source.setBits(4.0f);
    source.setDownsample(8.0f);
    source.setMix(0.7f);
    source.setBypass(true);

    AudioBitCrusher restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getBits(), 4.0f);
    EXPECT_FLOAT_EQ(restored.getDownsample(), 8.0f);
    EXPECT_FLOAT_EQ(restored.getMix(), 0.7f);
    EXPECT_TRUE(restored.isBypassed());
}
