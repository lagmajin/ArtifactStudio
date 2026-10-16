#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>

#include <cstddef>
#include <limits>
#include <utility>

import Audio.Effect.Delay;
import Audio.Segment;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
AudioSegment makeMono(int sampleRate, QVector<float> samples)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = AudioChannelLayout::Mono;
    segment.startFrame = 50;
    segment.channelData.push_back(std::move(samples));
    return segment;
}
}

TEST(AudioDelayStandaloneTest, ExposesExpectedDefaultsAndParameters)
{
    const AudioDelay delay;
    EXPECT_EQ(toStdString(delay.getName()), "Delay");
    EXPECT_EQ(toStdString(delay.effectType()), "delay");
    EXPECT_FLOAT_EQ(delay.getDelayTimeMs(), 300.0f);
    EXPECT_FLOAT_EQ(delay.getFeedback(), 0.3f);
    EXPECT_FLOAT_EQ(delay.getMix(), 0.4f);
    EXPECT_FLOAT_EQ(delay.getParameterValue(String("delay_ms")), 300.0f);
    EXPECT_FLOAT_EQ(delay.getParameterValue(String("feedback")), 0.3f);
    EXPECT_FLOAT_EQ(delay.getParameterValue(String("mix")), 0.4f);
}

TEST(AudioDelayStandaloneTest, ParameterMetadataAndIdRoutingStayConsistent)
{
    AudioDelay delay;
    const auto parameters = delay.getParameters();
    ASSERT_EQ(parameters.size(), 3u);

    for (std::size_t index = 0; index < parameters.size(); ++index) {
        const auto& parameter = parameters[index];
        EXPECT_FALSE(parameter.id.isEmpty());
        EXPECT_LE(parameter.minValue, parameter.maxValue);
        EXPECT_GE(parameter.defaultValue, parameter.minValue);
        EXPECT_LE(parameter.defaultValue, parameter.maxValue);
        EXPECT_FLOAT_EQ(parameter.value, delay.getParameterValue(parameter.id));
        for (std::size_t previous = 0; previous < index; ++previous)
            EXPECT_NE(parameters[previous].id, parameter.id);

        const auto before = delay.getParameters();
        float probe = (parameter.minValue + parameter.maxValue) * 0.5f;
        if (probe == parameter.value)
            probe = parameter.value == parameter.minValue
                ? parameter.maxValue : parameter.minValue;
        delay.setParameterValue(parameter.id, probe);

        EXPECT_FLOAT_EQ(delay.getParameterValue(parameter.id), probe);
        const auto after = delay.getParameters();
        for (std::size_t other = 0; other < before.size(); ++other) {
            if (other == index) continue;
            EXPECT_FLOAT_EQ(after[other].value, before[other].value)
                << "Setting parameter " << toStdString(parameter.id)
                << " changed " << toStdString(before[other].id);
        }
    }

    const QJsonObject beforeUnknownId = delay.toJson();
    delay.setParameterValue(String("unknown_parameter"), 0.5f);
    EXPECT_EQ(delay.toJson(), beforeUnknownId);
    EXPECT_FLOAT_EQ(delay.getParameterValue(String("unknown_parameter")), 0.0f);
}

TEST(AudioDelayStandaloneTest, SettersClampFiniteValuesAndRestoreDefaultsForNonFiniteValues)
{
    AudioDelay delay;
    delay.setDelayTimeMs(-5.0f);
    delay.setFeedback(2.0f);
    delay.setMix(-1.0f);
    EXPECT_FLOAT_EQ(delay.getDelayTimeMs(), 1.0f);
    EXPECT_FLOAT_EQ(delay.getFeedback(), 0.99f);
    EXPECT_FLOAT_EQ(delay.getMix(), 0.0f);

    delay.setDelayTimeMs(std::numeric_limits<float>::quiet_NaN());
    delay.setFeedback(std::numeric_limits<float>::infinity());
    delay.setMix(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(delay.getDelayTimeMs(), 300.0f);
    EXPECT_FLOAT_EQ(delay.getFeedback(), 0.3f);
    EXPECT_FLOAT_EQ(delay.getMix(), 0.4f);
}

TEST(AudioDelayStandaloneTest, ImpulseAppearsAfterConfiguredDelayInFrames)
{
    AudioDelay delay;
    delay.setDelayTimeMs(1.0f);
    delay.setFeedback(0.0f);
    delay.setMix(1.0f);
    auto segment = makeMono(1000, QVector<float>{1.0f, 0.0f, 0.0f});

    delay.process(segment);

    ASSERT_EQ(segment.channelData[0].size(), 3);
    EXPECT_FLOAT_EQ(segment.channelData[0][0], 0.0f);
    EXPECT_FLOAT_EQ(segment.channelData[0][1], 1.0f);
    EXPECT_FLOAT_EQ(segment.channelData[0][2], 0.0f);
}

TEST(AudioDelayStandaloneTest, ZeroMixKeepsTheDrySignalUnchanged)
{
    AudioDelay delay;
    delay.setDelayTimeMs(1.0f);
    delay.setFeedback(0.5f);
    delay.setMix(0.0f);
    auto segment = makeMono(1000, QVector<float>{0.25f, -0.5f, 0.75f});

    delay.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioDelayStandaloneTest, BypassLeavesSamplesUnchanged)
{
    AudioDelay delay;
    delay.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f});

    delay.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioDelayStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioDelay delay;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    delay.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioDelayStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioDelay source;
    source.setDelayTimeMs(125.0f);
    source.setFeedback(0.65f);
    source.setMix(0.8f);
    source.setBypass(true);

    AudioDelay restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getDelayTimeMs(), 125.0f);
    EXPECT_FLOAT_EQ(restored.getFeedback(), 0.65f);
    EXPECT_FLOAT_EQ(restored.getMix(), 0.8f);
    EXPECT_TRUE(restored.isBypassed());
}
