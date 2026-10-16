#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

#include <cstddef>
#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.PlateReverb;
import Audio.Segment;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
AudioSegment makeMono(int sampleRate, QVector<float> samples)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = AudioChannelLayout::Mono;
    segment.startFrame = 100;
    segment.channelData.push_back(std::move(samples));
    return segment;
}

AudioSegment makeStereo(int sampleRate, QVector<float> left, QVector<float> right)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = AudioChannelLayout::Stereo;
    segment.startFrame = 100;
    segment.channelData.push_back(std::move(left));
    segment.channelData.push_back(std::move(right));
    return segment;
}

bool hasTail(const QVector<float>& data, qsizetype from)
{
    for (qsizetype frame = from; frame < data.size(); ++frame) {
        if (data[frame] != 0.0f) return true;
    }
    return false;
}
}

TEST(AudioPlateReverbStandaloneTest, ExposesExpectedDefaultIdentityAndParameters)
{
    const AudioPlateReverb plate;
    EXPECT_EQ(toStdString(plate.getName()), "Plate Reverb");
    EXPECT_EQ(toStdString(plate.effectType()), "plate_reverb");
    EXPECT_FLOAT_EQ(plate.getPreDelayMs(), 10.0f);
    EXPECT_FLOAT_EQ(plate.getDecay(), 0.75f);
    EXPECT_FLOAT_EQ(plate.getDamping(), 0.4f);
    EXPECT_FLOAT_EQ(plate.getDiffusion(), 0.7f);
    EXPECT_FLOAT_EQ(plate.getSize(), 1.0f);
    EXPECT_FLOAT_EQ(plate.getModDepth(), 0.3f);
    EXPECT_FLOAT_EQ(plate.getModRate(), 0.8f);
    EXPECT_FLOAT_EQ(plate.getWidth(), 0.8f);
    EXPECT_FLOAT_EQ(plate.getMix(), 0.3f);
    EXPECT_FLOAT_EQ(plate.getParameterValue(String("decay")), 0.75f);
    EXPECT_FLOAT_EQ(plate.getParameterValue(String("mix")), 0.3f);
    EXPECT_EQ(plate.latencySamples(), 0);
    EXPECT_GT(plate.tailSamples(), 0);
}

TEST(AudioPlateReverbStandaloneTest, ParameterMetadataAndIdRoutingStayConsistent)
{
    AudioPlateReverb plate;
    const auto parameters = plate.getParameters();
    ASSERT_EQ(parameters.size(), 9u);

    for (std::size_t index = 0; index < parameters.size(); ++index) {
        const auto& parameter = parameters[index];
        EXPECT_FALSE(parameter.id.isEmpty());
        EXPECT_LE(parameter.minValue, parameter.maxValue);
        EXPECT_GE(parameter.defaultValue, parameter.minValue);
        EXPECT_LE(parameter.defaultValue, parameter.maxValue);
        EXPECT_FLOAT_EQ(parameter.value, plate.getParameterValue(parameter.id));
        for (std::size_t previous = 0; previous < index; ++previous)
            EXPECT_NE(parameters[previous].id, parameter.id);

        const auto before = plate.getParameters();
        float probe = (parameter.minValue + parameter.maxValue) * 0.5f;
        if (probe == parameter.value)
            probe = parameter.value == parameter.minValue
                ? parameter.maxValue : parameter.minValue;
        plate.setParameterValue(parameter.id, probe);

        EXPECT_FLOAT_EQ(plate.getParameterValue(parameter.id), probe);
        const auto after = plate.getParameters();
        for (std::size_t other = 0; other < before.size(); ++other) {
            if (other == index) continue;
            EXPECT_FLOAT_EQ(after[other].value, before[other].value)
                << "Setting parameter " << toStdString(parameter.id)
                << " changed " << toStdString(before[other].id);
        }
    }

    const QJsonObject beforeUnknownId = plate.toJson();
    plate.setParameterValue(String("unknown_parameter"), 0.5f);
    EXPECT_EQ(plate.toJson(), beforeUnknownId);
    EXPECT_FLOAT_EQ(plate.getParameterValue(String("unknown_parameter")), 0.0f);
}

TEST(AudioPlateReverbStandaloneTest, SettersClampParametersAndRestoreDefaultsForNonFiniteValues)
{
    AudioPlateReverb plate;
    plate.setDecay(2.0f);
    plate.setMix(-1.0f);
    plate.setSize(5.0f);
    plate.setModRate(99.0f);
    EXPECT_FLOAT_EQ(plate.getDecay(), 0.99f);
    EXPECT_FLOAT_EQ(plate.getMix(), 0.0f);
    EXPECT_FLOAT_EQ(plate.getSize(), 2.0f);
    EXPECT_FLOAT_EQ(plate.getModRate(), 5.0f);

    plate.setDecay(std::numeric_limits<float>::infinity());
    plate.setMix(std::numeric_limits<float>::quiet_NaN());
    plate.setSize(std::numeric_limits<float>::quiet_NaN());
    plate.setPreDelayMs(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(plate.getDecay(), 0.75f);
    EXPECT_FLOAT_EQ(plate.getMix(), 0.3f);
    EXPECT_FLOAT_EQ(plate.getSize(), 1.0f);
    EXPECT_FLOAT_EQ(plate.getPreDelayMs(), 10.0f);
}

TEST(AudioPlateReverbStandaloneTest, ZeroMixPreservesDrySignalExactly)
{
    AudioPlateReverb plate;
    plate.setMix(0.0f);
    auto segment = makeMono(48000, QVector<float>{1.0f, -0.5f, 0.25f, 0.0f});

    plate.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{1.0f, -0.5f, 0.25f, 0.0f}));
}

TEST(AudioPlateReverbStandaloneTest, FullWetImpulseProducesAReverbTail)
{
    AudioPlateReverb plate;
    plate.setDecay(0.99f);
    plate.setMix(1.0f);
    QVector<float> impulse(6000, 0.0f);
    impulse[0] = 1.0f;
    auto segment = makeMono(48000, std::move(impulse));

    plate.process(segment);

    // Pre-delay(10ms)があるため先頭は無音。
    EXPECT_FLOAT_EQ(segment.channelData[0][0], 0.0f);
    EXPECT_TRUE(hasTail(segment.channelData[0], 1));
}

TEST(AudioPlateReverbStandaloneTest, ProcessingInBlocksMatchesOneContinuousBlock)
{
    QVector<float> input(8192, 0.0f);
    input[0] = 1.0f;
    input[1023] = 0.35f;
    input[4095] = -0.25f;

    AudioPlateReverb continuous;
    continuous.setDecay(0.92f);
    continuous.setMix(1.0f);
    auto continuousSegment = makeMono(48000, input);
    continuous.process(continuousSegment);

    AudioPlateReverb blocked;
    blocked.setDecay(0.92f);
    blocked.setMix(1.0f);
    constexpr int boundaries[] = {0, 1, 137, 1024, 4097, 8192};
    QVector<float> blockedOutput;
    blockedOutput.reserve(input.size());
    for (std::size_t block = 1; block < 6; ++block) {
        const int start = boundaries[block - 1];
        const int length = boundaries[block] - start;
        auto segment = makeMono(48000, input.mid(start, length));
        blocked.process(segment);
        for (const float sample : segment.channelData[0])
            blockedOutput.push_back(sample);
    }

    EXPECT_EQ(blockedOutput, continuousSegment.channelData[0]);
}

TEST(AudioPlateReverbStandaloneTest, ZeroDecayProducesSilenceForFullWetImpulse)
{
    AudioPlateReverb plate;
    plate.setDecay(0.0f);
    plate.setMix(1.0f);
    QVector<float> impulse(6000, 0.0f);
    impulse[0] = 1.0f;
    auto segment = makeMono(48000, std::move(impulse));

    plate.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_FLOAT_EQ(sample, 0.0f);
    EXPECT_EQ(plate.tailSamples(), 0);
}

TEST(AudioPlateReverbStandaloneTest, StereoImpulseProducesDifferentTailsPerChannel)
{
    AudioPlateReverb plate;
    plate.setDecay(0.99f);
    plate.setMix(1.0f);
    QVector<float> left(6000, 0.0f);
    QVector<float> right(6000, 0.0f);
    left[0] = 1.0f;
    auto segment = makeStereo(48000, std::move(left), std::move(right));

    plate.process(segment);

    EXPECT_TRUE(hasTail(segment.channelData[0], 1));
    EXPECT_TRUE(hasTail(segment.channelData[1], 1));
    bool differs = false;
    for (qsizetype i = 0; i < segment.channelData[0].size(); ++i) {
        if (segment.channelData[0][i] != segment.channelData[1][i]) {
            differs = true;
            break;
        }
    }
    EXPECT_TRUE(differs);
}

TEST(AudioPlateReverbStandaloneTest, TailGrowsWithDecay)
{
    AudioPlateReverb plate;
    plate.setDecay(0.2f);
    const qint64 shortTail = plate.tailSamples();
    plate.setDecay(0.99f);
    const qint64 longTail = plate.tailSamples();
    EXPECT_GT(shortTail, 0);
    EXPECT_GT(longTail, shortTail);
}

TEST(AudioPlateReverbStandaloneTest, BypassPreservesWetAndDryInputSamples)
{
    AudioPlateReverb plate;
    plate.setMix(1.0f);
    plate.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    plate.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioPlateReverbStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioPlateReverb plate;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    auto segment = makeMono(48000, QVector<float>{nan, infinity, -infinity});

    plate.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
}

TEST(AudioPlateReverbStandaloneTest, ExtremeFiniteStereoInputCannotPoisonOutputOrTail)
{
    const float nearLimit = std::numeric_limits<float>::max() * 0.95f;
    QVector<float> left(4096, nearLimit);
    QVector<float> right(4096, nearLimit);
    for (qsizetype frame = 1; frame < left.size(); frame += 2) {
        left[frame] = -nearLimit;
        right[frame] = nearLimit;
    }

    AudioPlateReverb plate;
    plate.setDecay(0.99f);
    auto segment = makeStereo(48000, std::move(left), std::move(right));
    plate.process(segment);
    for (int channel = 0; channel < 2; ++channel) {
        for (qsizetype frame = 0; frame < segment.channelData[channel].size(); ++frame)
            EXPECT_TRUE(std::isfinite(segment.channelData[channel][frame]))
                << "Non-finite output at channel " << channel << ", frame " << frame;
    }

    auto silence = makeStereo(48000, QVector<float>(2048, 0.0f),
                              QVector<float>(2048, 0.0f));
    plate.process(silence);
    for (int channel = 0; channel < 2; ++channel) {
        for (const float sample : silence.channelData[channel])
            EXPECT_TRUE(std::isfinite(sample));
    }
}

TEST(AudioPlateReverbStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioPlateReverb plate;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    plate.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioPlateReverbStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioPlateReverb source;
    source.setPreDelayMs(25.0f);
    source.setDecay(0.9f);
    source.setDamping(0.6f);
    source.setDiffusion(0.5f);
    source.setSize(1.5f);
    source.setModDepth(0.7f);
    source.setModRate(1.2f);
    source.setWidth(0.4f);
    source.setMix(0.65f);
    source.setBypass(true);

    AudioPlateReverb restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getPreDelayMs(), 25.0f);
    EXPECT_FLOAT_EQ(restored.getDecay(), 0.9f);
    EXPECT_FLOAT_EQ(restored.getDamping(), 0.6f);
    EXPECT_FLOAT_EQ(restored.getDiffusion(), 0.5f);
    EXPECT_FLOAT_EQ(restored.getSize(), 1.5f);
    EXPECT_FLOAT_EQ(restored.getModDepth(), 0.7f);
    EXPECT_FLOAT_EQ(restored.getModRate(), 1.2f);
    EXPECT_FLOAT_EQ(restored.getWidth(), 0.4f);
    EXPECT_FLOAT_EQ(restored.getMix(), 0.65f);
    EXPECT_TRUE(restored.isBypassed());
}
