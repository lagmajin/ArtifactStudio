#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.Expander;
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

AudioSegment makeStereo(int sampleRate, QVector<float> left, QVector<float> right)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = AudioChannelLayout::Stereo;
    segment.startFrame = 0;
    segment.channelData.push_back(std::move(left));
    segment.channelData.push_back(std::move(right));
    return segment;
}
}

TEST(AudioExpanderStandaloneTest, ExposesExpectedDefaults)
{
    const AudioExpander expander;
    EXPECT_EQ(toStdString(expander.getName()), "Expander");
    EXPECT_EQ(toStdString(expander.effectType()), "expander");
    EXPECT_FLOAT_EQ(expander.getThresholdDb(), -40.0f);
    EXPECT_FLOAT_EQ(expander.getRatio(), 2.0f);
    EXPECT_FLOAT_EQ(expander.getAttackMs(), 10.0f);
    EXPECT_FLOAT_EQ(expander.getReleaseMs(), 150.0f);
}

TEST(AudioExpanderStandaloneTest, SettersClampAndRestoreDefaultsForNonFinite)
{
    AudioExpander expander;
    expander.setThresholdDb(99.0f);
    expander.setRatio(99.0f);
    EXPECT_FLOAT_EQ(expander.getThresholdDb(), 0.0f);
    EXPECT_FLOAT_EQ(expander.getRatio(), 10.0f);

    expander.setThresholdDb(std::numeric_limits<float>::quiet_NaN());
    expander.setAttackMs(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(expander.getThresholdDb(), -40.0f);
    EXPECT_FLOAT_EQ(expander.getAttackMs(), 10.0f);
}

TEST(AudioExpanderStandaloneTest, ParameterIdsExposeAndUpdateAllControls)
{
    AudioExpander expander;
    const auto parameters = expander.getParameters();
    ASSERT_EQ(parameters.size(), 4u);
    EXPECT_EQ(toStdString(parameters[0].id), "threshold_db");
    EXPECT_EQ(toStdString(parameters[1].id), "ratio");
    EXPECT_EQ(toStdString(parameters[2].id), "attack_ms");
    EXPECT_EQ(toStdString(parameters[3].id), "release_ms");
    EXPECT_FLOAT_EQ(parameters[0].minValue, -60.0f);
    EXPECT_FLOAT_EQ(parameters[0].maxValue, 0.0f);
    EXPECT_FLOAT_EQ(parameters[0].defaultValue, -40.0f);
    EXPECT_FLOAT_EQ(parameters[1].minValue, 1.0f);
    EXPECT_FLOAT_EQ(parameters[1].maxValue, 10.0f);
    EXPECT_FLOAT_EQ(parameters[1].defaultValue, 2.0f);
    EXPECT_FLOAT_EQ(parameters[2].minValue, 0.1f);
    EXPECT_FLOAT_EQ(parameters[2].maxValue, 100.0f);
    EXPECT_FLOAT_EQ(parameters[2].defaultValue, 10.0f);
    EXPECT_FLOAT_EQ(parameters[3].minValue, 10.0f);
    EXPECT_FLOAT_EQ(parameters[3].maxValue, 1000.0f);
    EXPECT_FLOAT_EQ(parameters[3].defaultValue, 150.0f);

    expander.setParameterValue("threshold_db", -30.0f);
    expander.setParameterValue("ratio", 3.5f);
    expander.setParameterValue("attack_ms", 5.0f);
    expander.setParameterValue("release_ms", 300.0f);
    EXPECT_FLOAT_EQ(expander.getParameterValue("threshold_db"), -30.0f);
    EXPECT_FLOAT_EQ(expander.getParameterValue("ratio"), 3.5f);
    EXPECT_FLOAT_EQ(expander.getParameterValue("attack_ms"), 5.0f);
    EXPECT_FLOAT_EQ(expander.getParameterValue("release_ms"), 300.0f);
    expander.setParameterValue("unknown", 99.0f);
    EXPECT_FLOAT_EQ(expander.getThresholdDb(), -30.0f);
}

TEST(AudioExpanderStandaloneTest, UnityRatioLeavesSignalUnchanged)
{
    AudioExpander expander;
    expander.setRatio(1.0f);
    auto segment = makeMono(48000, QVector<float>{1.0f, -0.5f, 0.25f, 0.001f});

    expander.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{1.0f, -0.5f, 0.25f, 0.001f}));
}

TEST(AudioExpanderStandaloneTest, UnityRatioBlockDoesNotAdvanceEnvelopeState)
{
    AudioExpander withUnityBlock;
    AudioExpander uninterrupted;
    for (auto* expander : {&withUnityBlock, &uninterrupted}) {
        expander->setThresholdDb(-20.0f);
        expander->setRatio(4.0f);
        expander->setAttackMs(0.1f);
        expander->setReleaseMs(10.0f);
    }

    auto primeWithUnityBlock = makeMono(48000, QVector<float>{1.0f});
    auto primeWithoutUnityBlock = primeWithUnityBlock;
    withUnityBlock.process(primeWithUnityBlock);
    uninterrupted.process(primeWithoutUnityBlock);

    withUnityBlock.setRatio(1.0f);
    auto unityBlock = makeMono(48000, QVector<float>(2000, 0.0f));
    withUnityBlock.process(unityBlock);
    withUnityBlock.setRatio(4.0f);

    auto nextWithUnityBlock = makeMono(48000, QVector<float>(16, 0.05f));
    auto nextWithoutUnityBlock = nextWithUnityBlock;
    withUnityBlock.process(nextWithUnityBlock);
    uninterrupted.process(nextWithoutUnityBlock);

    EXPECT_EQ(nextWithUnityBlock.channelData[0],
              nextWithoutUnityBlock.channelData[0]);
}

TEST(AudioExpanderStandaloneTest, AttenuatesSignalBelowThreshold)
{
    AudioExpander expander;
    expander.setThresholdDb(-40.0f);
    expander.setRatio(4.0f);
    // -60dB の微小信号は抑えられる。
    auto segment = makeMono(48000, QVector<float>(4800, 0.001f));

    expander.process(segment);

    for (const float sample : segment.channelData[0]) {
        EXPECT_TRUE(std::isfinite(sample));
        EXPECT_LE(std::abs(sample), 0.001f);
    }
    EXPECT_LT(std::abs(segment.channelData[0].last()), 0.001f);
}

TEST(AudioExpanderStandaloneTest, PositiveAndNegativeInputsReceiveSymmetricGain)
{
    AudioExpander positiveExpander;
    AudioExpander negativeExpander;
    for (auto* expander : {&positiveExpander, &negativeExpander}) {
        expander->setThresholdDb(-40.0f);
        expander->setRatio(4.0f);
        expander->setAttackMs(0.1f);
        expander->setReleaseMs(10.0f);
    }
    auto positive = makeMono(1000, QVector<float>{0.001f, 0.002f, 0.01f, 0.1f});
    auto negative = makeMono(1000, QVector<float>{-0.001f, -0.002f, -0.01f, -0.1f});

    positiveExpander.process(positive);
    negativeExpander.process(negative);

    ASSERT_EQ(positive.channelData[0].size(), negative.channelData[0].size());
    for (qsizetype frame = 0; frame < positive.channelData[0].size(); ++frame)
        EXPECT_FLOAT_EQ(positive.channelData[0][frame], -negative.channelData[0][frame]);
}

TEST(AudioExpanderStandaloneTest, ReleaseKeepsSmallSignalOpenAfterPeak)
{
    AudioExpander afterPeak;
    AudioExpander fresh;
    for (auto* expander : {&afterPeak, &fresh}) {
        expander->setThresholdDb(-40.0f);
        expander->setRatio(4.0f);
        expander->setAttackMs(0.1f);
        expander->setReleaseMs(1000.0f);
    }

    auto loud = makeMono(1000, QVector<float>{0.5f});
    auto lowAfterPeak = makeMono(1000, QVector<float>{0.001f});
    auto lowFromSilence = makeMono(1000, QVector<float>{0.001f});
    afterPeak.process(loud);
    afterPeak.process(lowAfterPeak);
    fresh.process(lowFromSilence);

    EXPECT_NEAR(lowAfterPeak.channelData[0][0], 0.001f, 0.000001f);
    EXPECT_LT(lowFromSilence.channelData[0][0], 0.00001f);
}

TEST(AudioExpanderStandaloneTest, ReleaseEventuallyReturnsSmallSignalBelowThreshold)
{
    AudioExpander expander;
    expander.setThresholdDb(-40.0f);
    expander.setRatio(4.0f);
    expander.setAttackMs(0.1f);
    expander.setReleaseMs(1000.0f);

    auto loud = makeMono(1000, QVector<float>{0.5f});
    auto quietTail = makeMono(1000, QVector<float>(12000, 0.001f));
    expander.process(loud);
    expander.process(quietTail);

    EXPECT_LT(std::abs(quietTail.channelData[0].last()), 0.00001f);
}

TEST(AudioExpanderStandaloneTest, ProcessingInBlocksPreservesEnvelopeContinuity)
{
    AudioExpander contiguous;
    AudioExpander blocked;
    for (auto* expander : {&contiguous, &blocked}) {
        expander->setThresholdDb(-40.0f);
        expander->setRatio(4.0f);
        expander->setAttackMs(0.1f);
        expander->setReleaseMs(150.0f);
    }

    const QVector<float> input{
        0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
        0.001f, 0.001f, 0.001f, 0.001f, 0.001f, 0.001f};
    auto whole = makeMono(48000, input);
    contiguous.process(whole);

    auto first = makeMono(48000, QVector<float>{0.5f, 0.5f, 0.5f});
    auto second = makeMono(48000, QVector<float>{0.5f, 0.5f, 0.5f, 0.001f});
    auto third = makeMono(48000, QVector<float>{0.001f, 0.001f, 0.001f, 0.001f, 0.001f});
    blocked.process(first);
    blocked.process(second);
    blocked.process(third);

    int outputIndex = 0;
    for (const auto* block : {&first, &second, &third}) {
        for (const float sample : block->channelData[0]) {
            EXPECT_FLOAT_EQ(sample, whole.channelData[0][outputIndex]);
            ++outputIndex;
        }
    }
    EXPECT_EQ(outputIndex, input.size());
}

TEST(AudioExpanderStandaloneTest, KeepsEnvelopeStateIndependentBetweenStereoChannels)
{
    AudioExpander expander;
    expander.setThresholdDb(-40.0f);
    expander.setRatio(4.0f);
    expander.setAttackMs(0.1f);
    expander.setReleaseMs(1000.0f);

    auto mixedLevels = makeStereo(
        48000, QVector<float>(4800, 0.001f), QVector<float>(4800, 0.5f));
    expander.process(mixedLevels);
    EXPECT_LT(std::abs(mixedLevels.channelData[0].last()), 0.00001f);
    EXPECT_NEAR(mixedLevels.channelData[1].last(), 0.5f, 0.0001f);

    auto quietLevels = makeStereo(
        48000, QVector<float>(4800, 0.001f), QVector<float>(4800, 0.001f));
    expander.process(quietLevels);
    EXPECT_LT(std::abs(quietLevels.channelData[0].last()), 0.00001f);
    EXPECT_TRUE(std::isfinite(quietLevels.channelData[1].last()));
}

TEST(AudioExpanderStandaloneTest, BypassPreservesInputSamples)
{
    AudioExpander expander;
    expander.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    expander.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioExpanderStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioExpander expander;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    auto segment = makeMono(48000, QVector<float>{nan, nan, nan});

    expander.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
}

TEST(AudioExpanderStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioExpander expander;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    expander.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioExpanderStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioExpander source;
    source.setThresholdDb(-20.0f);
    source.setRatio(3.0f);
    source.setAttackMs(5.0f);
    source.setReleaseMs(300.0f);
    source.setBypass(true);

    AudioExpander restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getThresholdDb(), -20.0f);
    EXPECT_FLOAT_EQ(restored.getRatio(), 3.0f);
    EXPECT_FLOAT_EQ(restored.getAttackMs(), 5.0f);
    EXPECT_FLOAT_EQ(restored.getReleaseMs(), 300.0f);
    EXPECT_TRUE(restored.isBypassed());
}
