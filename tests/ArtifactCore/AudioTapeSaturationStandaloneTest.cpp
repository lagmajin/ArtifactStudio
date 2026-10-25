#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <initializer_list>
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

TEST(AudioTapeSaturationStandaloneTest, WarmthReducesHighFrequencyOutputLevel)
{
    AudioTapeSaturation cool;
    AudioTapeSaturation warm;
    for (auto* tape : {&cool, &warm}) {
        tape->setDriveDb(0.0f);
        tape->setOutputDb(0.0f);
        tape->setMix(1.0f);
    }
    cool.setWarmth(0.0f);
    warm.setWarmth(1.0f);

    const QVector<float> input = makeSine(10000.0f, 48000, 4800, 0.8f);
    auto coolSegment = makeMono(48000, input);
    auto warmSegment = makeMono(48000, input);
    cool.process(coolSegment);
    warm.process(warmSegment);

    double coolEnergy = 0.0;
    double warmEnergy = 0.0;
    for (qsizetype frame = 256; frame < input.size(); ++frame) {
        const double coolSample = coolSegment.channelData[0][frame];
        const double warmSample = warmSegment.channelData[0][frame];
        coolEnergy += coolSample * coolSample;
        warmEnergy += warmSample * warmSample;
    }

    EXPECT_GT(coolEnergy, 0.0);
    EXPECT_LT(warmEnergy, coolEnergy);
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

TEST(AudioTapeSaturationStandaloneTest, ExposesParameterMetadataAndIdBasedControls)
{
    AudioTapeSaturation tape;
    const auto parameters = tape.getParameters();
    ASSERT_EQ(parameters.size(), 4u);
    EXPECT_EQ(toStdString(parameters[0].id), "drive_db");
    EXPECT_EQ(toStdString(parameters[1].id), "warmth");
    EXPECT_EQ(toStdString(parameters[2].id), "output_db");
    EXPECT_EQ(toStdString(parameters[3].id), "mix");
    EXPECT_FLOAT_EQ(parameters[0].minValue, 0.0f);
    EXPECT_FLOAT_EQ(parameters[0].maxValue, 24.0f);
    EXPECT_FLOAT_EQ(parameters[0].defaultValue, 6.0f);
    EXPECT_FLOAT_EQ(parameters[1].minValue, 0.0f);
    EXPECT_FLOAT_EQ(parameters[1].maxValue, 1.0f);
    EXPECT_FLOAT_EQ(parameters[1].defaultValue, 0.4f);
    EXPECT_FLOAT_EQ(parameters[2].minValue, -12.0f);
    EXPECT_FLOAT_EQ(parameters[2].maxValue, 12.0f);
    EXPECT_FLOAT_EQ(parameters[2].defaultValue, -6.0f);
    EXPECT_FLOAT_EQ(parameters[3].minValue, 0.0f);
    EXPECT_FLOAT_EQ(parameters[3].maxValue, 1.0f);
    EXPECT_FLOAT_EQ(parameters[3].defaultValue, 1.0f);

    tape.setParameterValue("drive_db", 18.0f);
    tape.setParameterValue("warmth", 0.8f);
    tape.setParameterValue("output_db", 3.0f);
    tape.setParameterValue("mix", 0.25f);
    EXPECT_FLOAT_EQ(tape.getParameterValue("drive_db"), 18.0f);
    EXPECT_FLOAT_EQ(tape.getParameterValue("warmth"), 0.8f);
    EXPECT_FLOAT_EQ(tape.getParameterValue("output_db"), 3.0f);
    EXPECT_FLOAT_EQ(tape.getParameterValue("mix"), 0.25f);
    tape.setParameterValue("unknown", 9.0f);
    EXPECT_FLOAT_EQ(tape.getParameterValue("unknown"), 0.0f);
    EXPECT_FLOAT_EQ(tape.getDriveDb(), 18.0f);
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
