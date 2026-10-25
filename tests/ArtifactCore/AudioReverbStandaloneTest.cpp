#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>

#include <cstddef>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <utility>

import Audio.Effect.Reverb;
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
}

TEST(AudioReverbStandaloneTest, ExposesExpectedDefaultIdentityAndParameters)
{
    const AudioReverb reverb;
    EXPECT_EQ(toStdString(reverb.getName()), "Reverb");
    EXPECT_FLOAT_EQ(reverb.getDecay(), 0.5f);
    EXPECT_FLOAT_EQ(reverb.getMix(), 0.3f);
    EXPECT_FLOAT_EQ(reverb.getParameterValue(String("decay")), 0.5f);
    EXPECT_FLOAT_EQ(reverb.getParameterValue(String("mix")), 0.3f);
    EXPECT_FLOAT_EQ(reverb.getParameterValue(String("size")), 0.7f);
}

TEST(AudioReverbStandaloneTest, ParameterDescriptorsMatchIdGettersAndSetters)
{
    AudioReverb reverb;
    const auto parameters = reverb.getParameters();
    ASSERT_EQ(parameters.size(), 3u);

    for (std::size_t index = 0; index < parameters.size(); ++index) {
        const auto& parameter = parameters[index];
        EXPECT_FALSE(parameter.id.isEmpty());
        EXPECT_LE(parameter.minValue, parameter.maxValue);
        EXPECT_GE(parameter.defaultValue, parameter.minValue);
        EXPECT_LE(parameter.defaultValue, parameter.maxValue);
        EXPECT_FLOAT_EQ(parameter.value, reverb.getParameterValue(parameter.id));
        for (std::size_t previous = 0; previous < index; ++previous)
            EXPECT_NE(parameters[previous].id, parameter.id);

        reverb.setParameterValue(parameter.id, parameter.maxValue);
        EXPECT_FLOAT_EQ(reverb.getParameterValue(parameter.id), parameter.maxValue);
    }

    const QJsonObject beforeUnknownId = reverb.toJson();
    reverb.setParameterValue(String("unknown_parameter"), 0.5f);
    EXPECT_EQ(reverb.toJson(), beforeUnknownId);
    EXPECT_FLOAT_EQ(reverb.getParameterValue(String("unknown_parameter")), 0.0f);
}

TEST(AudioReverbStandaloneTest, SettersClampParametersAndRestoreDefaultsForNonFiniteValues)
{
    AudioReverb reverb;
    reverb.setDecay(2.0f);
    reverb.setMix(-1.0f);
    reverb.setSize(-3.0f);
    EXPECT_FLOAT_EQ(reverb.getDecay(), 1.0f);
    EXPECT_FLOAT_EQ(reverb.getMix(), 0.0f);
    EXPECT_FLOAT_EQ(reverb.getParameterValue(String("size")), 0.0f);

    reverb.setDecay(std::numeric_limits<float>::infinity());
    reverb.setMix(std::numeric_limits<float>::quiet_NaN());
    reverb.setSize(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(reverb.getDecay(), 0.5f);
    EXPECT_FLOAT_EQ(reverb.getMix(), 0.3f);
    EXPECT_FLOAT_EQ(reverb.getParameterValue(String("size")), 0.7f);
}

TEST(AudioReverbStandaloneTest, ZeroMixPreservesDrySignalExactly)
{
    AudioReverb reverb;
    reverb.setMix(0.0f);
    auto segment = makeMono(48000, QVector<float>{1.0f, -0.5f, 0.25f, 0.0f});

    reverb.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{1.0f, -0.5f, 0.25f, 0.0f}));
}

TEST(AudioReverbStandaloneTest, ZeroMixStillAccumulatesWetHistory)
{
    AudioReverb mutedWet;
    AudioReverb audibleWet;
    for (auto* reverb : {&mutedWet, &audibleWet})
        reverb->setDecay(0.9f);
    mutedWet.setMix(0.0f);
    audibleWet.setMix(1.0f);

    QVector<float> impulse(1500, 0.0f);
    impulse[0] = 1.0f;
    auto mutedExcitation = makeMono(48000, impulse);
    auto audibleExcitation = makeMono(48000, impulse);
    mutedWet.process(mutedExcitation);
    audibleWet.process(audibleExcitation);
    EXPECT_EQ(mutedExcitation.channelData[0], impulse);

    mutedWet.setMix(1.0f);
    auto mutedTail = makeMono(48000, QVector<float>(1500, 0.0f));
    auto audibleTail = makeMono(48000, QVector<float>(1500, 0.0f));
    mutedWet.process(mutedTail);
    audibleWet.process(audibleTail);

    EXPECT_EQ(mutedTail.channelData[0], audibleTail.channelData[0]);
}

TEST(AudioReverbStandaloneTest, FullWetImpulseProducesAReverbTail)
{
    AudioReverb reverb;
    reverb.setDecay(1.0f);
    reverb.setMix(1.0f);
    QVector<float> impulse(1300, 0.0f);
    impulse[0] = 1.0f;
    auto segment = makeMono(48000, std::move(impulse));

    reverb.process(segment);

    EXPECT_FLOAT_EQ(segment.channelData[0][0], 0.0f);
    bool hasTail = false;
    for (qsizetype frame = 1; frame < segment.channelData[0].size(); ++frame)
        hasTail = hasTail || segment.channelData[0][frame] != 0.0f;
    EXPECT_TRUE(hasTail);
}

TEST(AudioReverbStandaloneTest, ProcessingInBlocksPreservesDelayHistory)
{
    QVector<float> input(1500, 0.0f);
    input[0] = 1.0f;

    AudioReverb wholeBlockReverb;
    wholeBlockReverb.setDecay(0.9f);
    wholeBlockReverb.setMix(1.0f);
    auto wholeBlock = makeMono(48000, input);
    wholeBlockReverb.process(wholeBlock);

    AudioReverb segmentedReverb;
    segmentedReverb.setDecay(0.9f);
    segmentedReverb.setMix(1.0f);
    QVector<float> segmentedOutput;
    segmentedOutput.reserve(input.size());
    for (int start = 0; start < input.size(); start += 300) {
        auto block = makeMono(48000, input.mid(start, 300));
        segmentedReverb.process(block);
        segmentedOutput += block.channelData[0];
    }

    ASSERT_EQ(segmentedOutput.size(), wholeBlock.channelData[0].size());
    EXPECT_EQ(segmentedOutput, wholeBlock.channelData[0]);
}

TEST(AudioReverbStandaloneTest, StereoChannelsDoNotLeakReverbIntoEachOther)
{
    QVector<float> left(1300, 0.0f);
    left[0] = 1.0f;
    auto segment = makeStereo(48000, std::move(left), QVector<float>(1300, 0.0f));
    AudioReverb reverb;
    reverb.setDecay(0.9f);
    reverb.setMix(1.0f);

    reverb.process(segment);

    bool leftHasTail = false;
    for (const float sample : segment.channelData[0])
        leftHasTail = leftHasTail || sample != 0.0f;
    EXPECT_TRUE(leftHasTail);
    for (const float sample : segment.channelData[1])
        EXPECT_FLOAT_EQ(sample, 0.0f);
}

TEST(AudioReverbStandaloneTest, ChannelsBeyondStereoRemainUntouched)
{
    AudioSegment segment;
    segment.sampleRate = 48000;
    segment.layout = AudioChannelLayout::Surround51;
    segment.channelData = {
        QVector<float>(1300, 0.0f), QVector<float>(1300, 0.0f),
        QVector<float>(1300, 0.125f), QVector<float>(1300, -0.25f),
        QVector<float>(1300, 0.375f), QVector<float>(1300, -0.5f)};
    segment.channelData[0][0] = 1.0f;
    segment.channelData[1][0] = -1.0f;
    const auto untouchedChannels = QVector<QVector<float>>{
        segment.channelData[2], segment.channelData[3],
        segment.channelData[4], segment.channelData[5]};

    AudioReverb reverb;
    reverb.setDecay(0.9f);
    reverb.setMix(1.0f);
    reverb.process(segment);

    ASSERT_EQ(segment.channelData.size(), 6);
    for (int channel = 0; channel < 4; ++channel)
        EXPECT_EQ(segment.channelData[channel + 2], untouchedChannels[channel]);
}

TEST(AudioReverbStandaloneTest, SizeParameterChangesWetImpulseResponse)
{
    QVector<float> impulse(4096, 0.0f);
    impulse[0] = 1.0f;

    AudioReverb compact;
    compact.setDecay(0.9f);
    compact.setMix(1.0f);
    compact.setSize(0.0f);
    auto compactSegment = makeMono(48000, impulse);
    compact.process(compactSegment);

    AudioReverb large;
    large.setDecay(0.9f);
    large.setMix(1.0f);
    large.setSize(1.0f);
    auto largeSegment = makeMono(48000, impulse);
    large.process(largeSegment);

    EXPECT_NE(compactSegment.channelData[0], largeSegment.channelData[0]);
}

TEST(AudioReverbStandaloneTest, DelayHistoryIsIsolatedBetweenInstances)
{
    AudioReverb excited;
    excited.setDecay(0.9f);
    excited.setMix(1.0f);
    QVector<float> impulse(1300, 0.0f);
    impulse[0] = 1.0f;
    auto excitedSegment = makeMono(48000, std::move(impulse));
    excited.process(excitedSegment);

    AudioReverb independent;
    independent.setDecay(0.9f);
    independent.setMix(1.0f);
    auto silence = makeMono(48000, QVector<float>(1024, 0.0f));
    independent.process(silence);

    EXPECT_EQ(silence.channelData[0], QVector<float>(1024, 0.0f));
}

TEST(AudioReverbStandaloneTest, ZeroDecayDoesNotStoreAnImpulseForLaterOutput)
{
    AudioReverb reverb;
    reverb.setDecay(0.0f);
    reverb.setMix(1.0f);
    QVector<float> impulse(1300, 0.0f);
    impulse[0] = 1.0f;
    auto segment = makeMono(48000, std::move(impulse));

    reverb.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_FLOAT_EQ(sample, 0.0f);

    reverb.setDecay(1.0f);
    auto followupSilence = makeMono(48000, QVector<float>(1300, 0.0f));
    reverb.process(followupSilence);
    for (const float sample : followupSilence.channelData[0])
        EXPECT_FLOAT_EQ(sample, 0.0f);
}

TEST(AudioReverbStandaloneTest, BypassPreservesWetAndDryInputSamples)
{
    AudioReverb reverb;
    reverb.setMix(1.0f);
    reverb.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    reverb.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioReverbStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioReverb reverb;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    auto segment = makeMono(48000, QVector<float>{nan, infinity, -infinity});

    reverb.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
}

TEST(AudioReverbStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioReverb reverb;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    reverb.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioReverbStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioReverb source;
    source.setDecay(0.85f);
    source.setMix(0.65f);
    source.setSize(0.2f);
    source.setBypass(true);

    AudioReverb restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getDecay(), 0.85f);
    EXPECT_FLOAT_EQ(restored.getMix(), 0.65f);
    EXPECT_FLOAT_EQ(restored.getParameterValue(String("size")), 0.2f);
    EXPECT_TRUE(restored.isBypassed());
}
