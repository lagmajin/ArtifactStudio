#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.SpringReverb;
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

TEST(AudioSpringReverbStandaloneTest, ExposesExpectedDefaults)
{
    const AudioSpringReverb spring;
    EXPECT_EQ(toStdString(spring.getName()), "Spring Reverb");
    EXPECT_EQ(toStdString(spring.effectType()), "spring_reverb");
    EXPECT_FLOAT_EQ(spring.getDecay(), 0.8f);
    EXPECT_FLOAT_EQ(spring.getBrightness(), 0.5f);
    EXPECT_FLOAT_EQ(spring.getTension(), 1.0f);
    EXPECT_FLOAT_EQ(spring.getMix(), 0.3f);
    EXPECT_EQ(spring.latencySamples(), 0);
    EXPECT_GT(spring.tailSamples(), 0);
}

TEST(AudioSpringReverbStandaloneTest, TailEstimateTracksDecayAndTension)
{
    AudioSpringReverb spring;
    spring.setDecay(0.0f);
    EXPECT_EQ(spring.tailSamples(), 0);

    spring.setDecay(0.8f);
    spring.setTension(0.5f);
    const qint64 shortTail = spring.tailSamples();
    spring.setTension(1.5f);
    const qint64 longTensionTail = spring.tailSamples();
    spring.setTension(1.0f);
    spring.setDecay(0.9f);
    const qint64 longerDecayTail = spring.tailSamples();

    EXPECT_GT(shortTail, 0);
    EXPECT_GT(longTensionTail, shortTail);
    EXPECT_GT(longerDecayTail, shortTail * 2);
}

TEST(AudioSpringReverbStandaloneTest, SettersClampAndRestoreDefaultsForNonFinite)
{
    AudioSpringReverb spring;
    spring.setDecay(99.0f);
    spring.setTension(0.0f);
    spring.setMix(-1.0f);
    EXPECT_FLOAT_EQ(spring.getDecay(), 0.99f);
    EXPECT_FLOAT_EQ(spring.getTension(), 0.5f);
    EXPECT_FLOAT_EQ(spring.getMix(), 0.0f);

    spring.setDecay(std::numeric_limits<float>::quiet_NaN());
    spring.setBrightness(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(spring.getDecay(), 0.8f);
    EXPECT_FLOAT_EQ(spring.getBrightness(), 0.5f);
}

TEST(AudioSpringReverbStandaloneTest, ZeroMixPreservesDrySignalExactly)
{
    AudioSpringReverb spring;
    spring.setMix(0.0f);
    auto segment = makeMono(48000, QVector<float>{1.0f, -0.5f, 0.25f, 0.0f});

    spring.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{1.0f, -0.5f, 0.25f, 0.0f}));
}

TEST(AudioSpringReverbStandaloneTest, FullWetImpulseProducesAReverbTail)
{
    AudioSpringReverb spring;
    spring.setDecay(0.9f);
    spring.setMix(1.0f);
    // 最長ループ 31ms * 1.5 + 系列遅延ぶん確保する。
    QVector<float> impulse(6000, 0.0f);
    impulse[0] = 1.0f;
    auto segment = makeMono(48000, std::move(impulse));

    spring.process(segment);

    // ループ読み出しが空のため先頭は無音。
    EXPECT_FLOAT_EQ(segment.channelData[0][0], 0.0f);
    bool hasTail = false;
    for (const float sample : segment.channelData[0])
        hasTail = hasTail || (sample != 0.0f);
    EXPECT_TRUE(hasTail);
}

TEST(AudioSpringReverbStandaloneTest, SampleRateChangeRebuildsDelayState)
{
    AudioSpringReverb reconfigured;
    AudioSpringReverb freshAtNewRate;
    for (auto* spring : {&reconfigured, &freshAtNewRate}) {
        spring->setDecay(0.9f);
        spring->setBrightness(0.7f);
        spring->setTension(1.2f);
        spring->setMix(1.0f);
    }

    auto priorRateSignal = makeMono(48000, QVector<float>(6000, 0.25f));
    reconfigured.process(priorRateSignal);

    QVector<float> newRateInput(4096, 0.0f);
    newRateInput[0] = 1.0f;
    auto afterRateChange = makeMono(44100, newRateInput);
    auto freshInput = makeMono(44100, newRateInput);
    reconfigured.process(afterRateChange);
    freshAtNewRate.process(freshInput);

    EXPECT_EQ(afterRateChange.channelData[0], freshInput.channelData[0]);
}

TEST(AudioSpringReverbStandaloneTest, TensionChangeRebuildsDelayState)
{
    AudioSpringReverb reconfigured;
    AudioSpringReverb freshAtNewTension;
    for (auto* spring : {&reconfigured, &freshAtNewTension}) {
        spring->setDecay(0.9f);
        spring->setBrightness(0.7f);
        spring->setMix(1.0f);
    }
    reconfigured.setTension(0.5f);
    freshAtNewTension.setTension(1.5f);

    auto priorTensionSignal = makeMono(48000, QVector<float>(6000, 0.25f));
    reconfigured.process(priorTensionSignal);
    reconfigured.setTension(1.5f);

    QVector<float> nextInput(4096, 0.0f);
    nextInput[0] = 1.0f;
    auto afterTensionChange = makeMono(48000, nextInput);
    auto freshInput = makeMono(48000, nextInput);
    reconfigured.process(afterTensionChange);
    freshAtNewTension.process(freshInput);

    EXPECT_EQ(afterTensionChange.channelData[0], freshInput.channelData[0]);
}

TEST(AudioSpringReverbStandaloneTest, BypassPreservesInputSamples)
{
    AudioSpringReverb spring;
    spring.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    spring.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioSpringReverbStandaloneTest, BypassDoesNotAdvanceReverbHistory)
{
    AudioSpringReverb afterBypass;
    AudioSpringReverb uninterrupted;
    for (auto* spring : {&afterBypass, &uninterrupted}) {
        spring->setDecay(0.9f);
        spring->setBrightness(0.7f);
        spring->setMix(1.0f);
    }

    QVector<float> excitation(6000, 0.0f);
    excitation[0] = 1.0f;
    auto firstForBypass = makeMono(48000, excitation);
    auto firstUninterrupted = makeMono(48000, excitation);
    afterBypass.process(firstForBypass);
    uninterrupted.process(firstUninterrupted);

    afterBypass.setBypass(true);
    auto bypassed = makeMono(48000, QVector<float>(256, 0.25f));
    afterBypass.process(bypassed);
    afterBypass.setBypass(false);

    auto resumed = makeMono(48000, QVector<float>(4096, 0.0f));
    auto control = makeMono(48000, QVector<float>(4096, 0.0f));
    afterBypass.process(resumed);
    uninterrupted.process(control);

    EXPECT_EQ(resumed.channelData[0], control.channelData[0]);
}

TEST(AudioSpringReverbStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioSpringReverb spring;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    auto segment = makeMono(48000, QVector<float>{nan, infinity, -infinity});

    spring.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
}

TEST(AudioSpringReverbStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioSpringReverb spring;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    spring.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioSpringReverbStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioSpringReverb source;
    source.setDecay(0.9f);
    source.setBrightness(0.8f);
    source.setTension(1.2f);
    source.setMix(0.6f);
    source.setBypass(true);

    AudioSpringReverb restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getDecay(), 0.9f);
    EXPECT_FLOAT_EQ(restored.getBrightness(), 0.8f);
    EXPECT_FLOAT_EQ(restored.getTension(), 1.2f);
    EXPECT_FLOAT_EQ(restored.getMix(), 0.6f);
    EXPECT_TRUE(restored.isBypassed());
}
