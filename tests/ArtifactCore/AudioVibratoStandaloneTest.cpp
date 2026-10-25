#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <initializer_list>
#include <limits>
#include <utility>

import Audio.Effect.Vibrato;
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

TEST(AudioVibratoStandaloneTest, ExposesExpectedDefaults)
{
    const AudioVibrato vibrato;
    EXPECT_EQ(toStdString(vibrato.getName()), "Vibrato");
    EXPECT_EQ(toStdString(vibrato.effectType()), "vibrato");
    EXPECT_FLOAT_EQ(vibrato.getRate(), 6.0f);
    EXPECT_FLOAT_EQ(vibrato.getDepthMs(), 2.0f);
    EXPECT_FLOAT_EQ(vibrato.getBaseMs(), 5.0f);
    EXPECT_FLOAT_EQ(vibrato.getMix(), 1.0f);
    EXPECT_GT(vibrato.latencySamples(), 0);
    EXPECT_EQ(vibrato.tailSamples(), 0);
}

TEST(AudioVibratoStandaloneTest, SettersClampAndRestoreDefaultsForNonFinite)
{
    AudioVibrato vibrato;
    vibrato.setRate(99.0f);
    vibrato.setBaseMs(0.0f);
    EXPECT_FLOAT_EQ(vibrato.getRate(), 15.0f);
    EXPECT_FLOAT_EQ(vibrato.getBaseMs(), 3.0f);

    vibrato.setDepthMs(std::numeric_limits<float>::quiet_NaN());
    vibrato.setMix(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(vibrato.getDepthMs(), 2.0f);
    EXPECT_FLOAT_EQ(vibrato.getMix(), 1.0f);
}

TEST(AudioVibratoStandaloneTest, ParameterApiExposesAndUpdatesEveryControl)
{
    AudioVibrato vibrato;
    const auto parameters = vibrato.getParameters();
    ASSERT_EQ(parameters.size(), 4u);
    EXPECT_EQ(toStdString(parameters[0].id), "rate");
    EXPECT_FLOAT_EQ(parameters[0].minValue, 1.0f);
    EXPECT_FLOAT_EQ(parameters[0].maxValue, 15.0f);
    EXPECT_FLOAT_EQ(parameters[0].defaultValue, 6.0f);
    EXPECT_EQ(toStdString(parameters[1].id), "depth_ms");
    EXPECT_FLOAT_EQ(parameters[1].minValue, 0.0f);
    EXPECT_FLOAT_EQ(parameters[1].maxValue, 10.0f);
    EXPECT_FLOAT_EQ(parameters[1].defaultValue, 2.0f);
    EXPECT_EQ(toStdString(parameters[2].id), "base_ms");
    EXPECT_FLOAT_EQ(parameters[2].minValue, 3.0f);
    EXPECT_FLOAT_EQ(parameters[2].maxValue, 10.0f);
    EXPECT_FLOAT_EQ(parameters[2].defaultValue, 5.0f);
    EXPECT_EQ(toStdString(parameters[3].id), "mix");
    EXPECT_FLOAT_EQ(parameters[3].minValue, 0.0f);
    EXPECT_FLOAT_EQ(parameters[3].maxValue, 1.0f);
    EXPECT_FLOAT_EQ(parameters[3].defaultValue, 1.0f);

    vibrato.setParameterValue(String("rate"), 12.0f);
    vibrato.setParameterValue(String("depth_ms"), 4.0f);
    vibrato.setParameterValue(String("base_ms"), 8.0f);
    vibrato.setParameterValue(String("mix"), 0.25f);
    vibrato.setParameterValue(String("unknown"), 99.0f);
    EXPECT_FLOAT_EQ(vibrato.getParameterValue(String("rate")), 12.0f);
    EXPECT_FLOAT_EQ(vibrato.getParameterValue(String("depth_ms")), 4.0f);
    EXPECT_FLOAT_EQ(vibrato.getParameterValue(String("base_ms")), 8.0f);
    EXPECT_FLOAT_EQ(vibrato.getParameterValue(String("mix")), 0.25f);
    EXPECT_FLOAT_EQ(vibrato.getParameterValue(String("unknown")), 0.0f);
}

TEST(AudioVibratoStandaloneTest, ZeroMixPreservesDrySignalExactly)
{
    AudioVibrato vibrato;
    vibrato.setMix(0.0f);
    auto segment = makeMono(48000, QVector<float>{1.0f, -0.5f, 0.25f, 0.0f});

    vibrato.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{1.0f, -0.5f, 0.25f, 0.0f}));
}

TEST(AudioVibratoStandaloneTest, FullWetImpulseStartsSilentDueToDelay)
{
    AudioVibrato vibrato;
    vibrato.setMix(1.0f);
    QVector<float> impulse(4800, 0.0f);
    impulse[0] = 1.0f;
    auto segment = makeMono(48000, std::move(impulse));

    vibrato.process(segment);

    EXPECT_FLOAT_EQ(segment.channelData[0][0], 0.0f);
    bool hasWet = false;
    for (const float sample : segment.channelData[0])
        hasWet = hasWet || (sample != 0.0f);
    EXPECT_TRUE(hasWet);
}

TEST(AudioVibratoStandaloneTest, LfoPhaseContinuesAcrossAdjacentSegments)
{
    AudioVibrato contiguous;
    AudioVibrato segmented;
    for (auto* vibrato : {&contiguous, &segmented}) {
        vibrato->setRate(12.0f);
        vibrato->setDepthMs(4.0f);
        vibrato->setBaseMs(5.0f);
        vibrato->setMix(1.0f);
    }

    QVector<float> input;
    input.reserve(64);
    for (int frame = 0; frame < 64; ++frame)
        input.push_back(std::sin(2.0f * 3.14159265f * 37.0f * frame / 1000.0f));

    auto whole = makeMono(1000, input);
    contiguous.process(whole);

    QVector<float> segmentedOutput;
    segmentedOutput.reserve(input.size());
    for (int start = 0; start < input.size(); start += 8) {
        auto block = makeMono(1000, input.mid(start, 8));
        segmented.process(block);
        segmentedOutput += block.channelData[0];
    }

    ASSERT_EQ(segmentedOutput.size(), whole.channelData[0].size());
    EXPECT_EQ(segmentedOutput, whole.channelData[0]);
}

TEST(AudioVibratoStandaloneTest, BypassPreservesInputSamples)
{
    AudioVibrato vibrato;
    vibrato.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    vibrato.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioVibratoStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioVibrato vibrato;
    const float infinity = std::numeric_limits<float>::infinity();
    auto segment = makeMono(48000, QVector<float>{infinity, infinity, infinity});

    vibrato.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
}

TEST(AudioVibratoStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioVibrato vibrato;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    vibrato.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioVibratoStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioVibrato source;
    source.setRate(10.0f);
    source.setDepthMs(4.0f);
    source.setBaseMs(8.0f);
    source.setMix(0.5f);
    source.setBypass(true);

    AudioVibrato restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getRate(), 10.0f);
    EXPECT_FLOAT_EQ(restored.getDepthMs(), 4.0f);
    EXPECT_FLOAT_EQ(restored.getBaseMs(), 8.0f);
    EXPECT_FLOAT_EQ(restored.getMix(), 0.5f);
    EXPECT_TRUE(restored.isBypassed());
}
