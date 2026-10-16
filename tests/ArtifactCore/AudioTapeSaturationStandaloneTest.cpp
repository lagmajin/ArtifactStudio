#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.TapeSaturation;
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

TEST(AudioTapeSaturationStandaloneTest, ExposesExpectedDefaults)
{
    const AudioTapeSaturation tape;
    EXPECT_EQ(toStdString(tape.getName()), "Tape Saturation");
    EXPECT_EQ(toStdString(tape.effectType()), "tape_saturation");
    EXPECT_FLOAT_EQ(tape.getDriveDb(), 6.0f);
    EXPECT_FLOAT_EQ(tape.getWarmth(), 0.4f);
    EXPECT_FLOAT_EQ(tape.getOutputDb(), -6.0f);
    EXPECT_FLOAT_EQ(tape.getMix(), 1.0f);
}

TEST(AudioTapeSaturationStandaloneTest, SettersClampAndRestoreDefaultsForNonFinite)
{
    AudioTapeSaturation tape;
    tape.setDriveDb(99.0f);
    tape.setOutputDb(-99.0f);
    EXPECT_FLOAT_EQ(tape.getDriveDb(), 24.0f);
    EXPECT_FLOAT_EQ(tape.getOutputDb(), -12.0f);

    tape.setDriveDb(std::numeric_limits<float>::quiet_NaN());
    tape.setWarmth(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(tape.getDriveDb(), 6.0f);
    EXPECT_FLOAT_EQ(tape.getWarmth(), 0.4f);
}

TEST(AudioTapeSaturationStandaloneTest, ZeroMixPreservesDrySignalExactly)
{
    AudioTapeSaturation tape;
    tape.setMix(0.0f);
    auto segment = makeMono(48000, QVector<float>{1.0f, -0.5f, 0.25f, 0.0f});

    tape.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{1.0f, -0.5f, 0.25f, 0.0f}));
}

TEST(AudioTapeSaturationStandaloneTest, DriveAudiblyChangesSine)
{
    AudioTapeSaturation tape;
    tape.setDriveDb(12.0f);
    tape.setWarmth(0.0f);
    tape.setOutputDb(0.0f);
    tape.setMix(1.0f);
    auto segment = makeMono(48000, makeSine(440.0f, 48000, 480, 0.5f));
    const QVector<float> before = segment.channelData[0];

    tape.process(segment);

    bool differs = false;
    for (qsizetype i = 0; i < before.size(); ++i) {
        EXPECT_TRUE(std::isfinite(segment.channelData[0][i]));
        differs = differs || (segment.channelData[0][i] != before[i]);
    }
    EXPECT_TRUE(differs);
}

TEST(AudioTapeSaturationStandaloneTest, BypassPreservesInputSamples)
{
    AudioTapeSaturation tape;
    tape.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    tape.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioTapeSaturationStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioTapeSaturation tape;
    const float infinity = std::numeric_limits<float>::infinity();
    auto segment = makeMono(48000, QVector<float>{infinity, infinity, infinity});

    tape.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
}

TEST(AudioTapeSaturationStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioTapeSaturation tape;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    tape.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioTapeSaturationStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioTapeSaturation source;
    source.setDriveDb(18.0f);
    source.setWarmth(0.9f);
    source.setOutputDb(3.0f);
    source.setMix(0.6f);
    source.setBypass(true);

    AudioTapeSaturation restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getDriveDb(), 18.0f);
    EXPECT_FLOAT_EQ(restored.getWarmth(), 0.9f);
    EXPECT_FLOAT_EQ(restored.getOutputDb(), 3.0f);
    EXPECT_FLOAT_EQ(restored.getMix(), 0.6f);
    EXPECT_TRUE(restored.isBypassed());
}
