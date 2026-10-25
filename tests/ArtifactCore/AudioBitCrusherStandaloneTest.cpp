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

TEST(AudioBitCrusherStandaloneTest, ParameterApiExposesRangesAndUpdatesCurrentValues)
{
    AudioBitCrusher crusher;
    crusher.setParameterValue(String("bits"), 4.0f);
    crusher.setParameterValue(String("downsample"), 8.0f);
    crusher.setParameterValue(String("mix"), 0.25f);

    const auto parameters = crusher.getParameters();
    ASSERT_EQ(parameters.size(), 3u);
    EXPECT_EQ(toStdString(parameters[0].id), "bits");
    EXPECT_FLOAT_EQ(parameters[0].minValue, 2.0f);
    EXPECT_FLOAT_EQ(parameters[0].maxValue, 16.0f);
    EXPECT_FLOAT_EQ(parameters[0].defaultValue, 8.0f);
    EXPECT_FLOAT_EQ(parameters[0].value, 4.0f);
    EXPECT_EQ(toStdString(parameters[1].id), "downsample");
    EXPECT_FLOAT_EQ(parameters[1].minValue, 1.0f);
    EXPECT_FLOAT_EQ(parameters[1].maxValue, 40.0f);
    EXPECT_FLOAT_EQ(parameters[1].defaultValue, 1.0f);
    EXPECT_FLOAT_EQ(parameters[1].value, 8.0f);
    EXPECT_EQ(toStdString(parameters[2].id), "mix");
    EXPECT_FLOAT_EQ(parameters[2].minValue, 0.0f);
    EXPECT_FLOAT_EQ(parameters[2].maxValue, 1.0f);
    EXPECT_FLOAT_EQ(parameters[2].defaultValue, 1.0f);
    EXPECT_FLOAT_EQ(parameters[2].value, 0.25f);

    EXPECT_FLOAT_EQ(crusher.getParameterValue(String("bits")), 4.0f);
    EXPECT_FLOAT_EQ(crusher.getParameterValue(String("downsample")), 8.0f);
    EXPECT_FLOAT_EQ(crusher.getParameterValue(String("mix")), 0.25f);
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

TEST(AudioBitCrusherStandaloneTest, MixInterpolatesBetweenDryAndQuantizedSamples)
{
    AudioBitCrusher crusher;
    crusher.setBits(2.0f);
    crusher.setDownsample(1.0f);
    crusher.setMix(0.5f);
    auto segment = makeMono(48000, QVector<float>{0.8f, -0.3f, 0.6f});

    crusher.process(segment);

    EXPECT_NEAR(segment.channelData[0][0], 0.9f, 1e-6f);
    EXPECT_NEAR(segment.channelData[0][1], -0.4f, 1e-6f);
    EXPECT_NEAR(segment.channelData[0][2], 0.55f, 1e-6f);
}

TEST(AudioBitCrusherStandaloneTest, ZeroMixBlockDoesNotAdvanceSampleHoldState)
{
    AudioBitCrusher withMutedBlock;
    AudioBitCrusher uninterrupted;
    for (auto* crusher : {&withMutedBlock, &uninterrupted}) {
        crusher->setBits(16.0f);
        crusher->setDownsample(3.0f);
        crusher->setMix(0.0f);
    }

    auto mutedBlock = makeMono(48000, QVector<float>{0.7f});
    withMutedBlock.process(mutedBlock);
    EXPECT_FLOAT_EQ(mutedBlock.channelData[0][0], 0.7f);
    withMutedBlock.setMix(1.0f);
    uninterrupted.setMix(1.0f);

    auto nextWithMutedBlock = makeMono(
        48000, QVector<float>{0.1f, 0.3f, 0.6f, 0.9f});
    auto nextWithoutMutedBlock = nextWithMutedBlock;
    withMutedBlock.process(nextWithMutedBlock);
    uninterrupted.process(nextWithoutMutedBlock);

    EXPECT_EQ(nextWithMutedBlock.channelData[0],
              nextWithoutMutedBlock.channelData[0]);
}

TEST(AudioBitCrusherStandaloneTest,
     DownsampleHoldPeriodIsStableAcrossAudioSegmentBoundaries)
{
    AudioBitCrusher wholeBufferCrusher;
    wholeBufferCrusher.setBits(16.0f);
    wholeBufferCrusher.setDownsample(3.0f);
    wholeBufferCrusher.setMix(1.0f);
    auto wholeBuffer = makeMono(
        48000, QVector<float>{0.1f, 0.3f, 0.6f, -0.2f, 0.4f, 0.8f});
    wholeBufferCrusher.process(wholeBuffer);

    AudioBitCrusher segmentedCrusher;
    segmentedCrusher.setBits(16.0f);
    segmentedCrusher.setDownsample(3.0f);
    segmentedCrusher.setMix(1.0f);
    auto firstSegment = makeMono(48000, QVector<float>{0.1f, 0.3f});
    auto secondSegment = makeMono(48000, QVector<float>{0.6f, -0.2f, 0.4f, 0.8f});
    segmentedCrusher.process(firstSegment);
    segmentedCrusher.process(secondSegment);

    QVector<float> segmentedOutput = firstSegment.channelData[0];
    segmentedOutput += secondSegment.channelData[0];
    ASSERT_EQ(segmentedOutput.size(), wholeBuffer.channelData[0].size());
    EXPECT_EQ(segmentedOutput, wholeBuffer.channelData[0])
        << "audio block boundaries must not restart the sample-hold LOD phase";
    EXPECT_FLOAT_EQ(wholeBuffer.channelData[0][0],
                    wholeBuffer.channelData[0][1]);
    EXPECT_FLOAT_EQ(wholeBuffer.channelData[0][1],
                    wholeBuffer.channelData[0][2]);
    EXPECT_FLOAT_EQ(wholeBuffer.channelData[0][3],
                    wholeBuffer.channelData[0][4]);
    EXPECT_FLOAT_EQ(wholeBuffer.channelData[0][4],
                    wholeBuffer.channelData[0][5]);
    EXPECT_NE(wholeBuffer.channelData[0][2],
              wholeBuffer.channelData[0][3]);
}

TEST(AudioBitCrusherStandaloneTest, FractionalDownsampleFactorRoundsAtHalfStep)
{
    AudioBitCrusher belowHalf;
    belowHalf.setBits(16.0f);
    belowHalf.setDownsample(2.49f);
    auto twoSampleHold = makeMono(48000,
        QVector<float>{0.1f, 0.3f, 0.5f, 0.7f});
    belowHalf.process(twoSampleHold);
    EXPECT_FLOAT_EQ(twoSampleHold.channelData[0][0],
                    twoSampleHold.channelData[0][1]);
    EXPECT_NE(twoSampleHold.channelData[0][1],
              twoSampleHold.channelData[0][2]);

    AudioBitCrusher atHalf;
    atHalf.setBits(16.0f);
    atHalf.setDownsample(2.5f);
    auto threeSampleHold = makeMono(48000,
        QVector<float>{0.1f, 0.3f, 0.5f, 0.7f});
    atHalf.process(threeSampleHold);
    EXPECT_FLOAT_EQ(threeSampleHold.channelData[0][0],
                    threeSampleHold.channelData[0][1]);
    EXPECT_FLOAT_EQ(threeSampleHold.channelData[0][1],
                    threeSampleHold.channelData[0][2]);
    EXPECT_NE(threeSampleHold.channelData[0][2],
              threeSampleHold.channelData[0][3]);
}

TEST(AudioBitCrusherStandaloneTest,
     DownsampleHoldStateIsIndependentForStereoChannels)
{
    AudioBitCrusher crusher;
    crusher.setBits(16.0f);
    crusher.setDownsample(2.0f);
    crusher.setMix(1.0f);

    AudioSegment segment;
    segment.sampleRate = 48000;
    segment.layout = AudioChannelLayout::Stereo;
    segment.channelData = {
        QVector<float>{0.1f, 0.3f, 0.5f, 0.7f},
        QVector<float>{-0.2f, -0.4f, -0.6f, -0.8f}};
    crusher.process(segment);

    ASSERT_EQ(segment.channelData.size(), 2);
    EXPECT_FLOAT_EQ(segment.channelData[0][0], segment.channelData[0][1]);
    EXPECT_FLOAT_EQ(segment.channelData[0][2], segment.channelData[0][3]);
    EXPECT_FLOAT_EQ(segment.channelData[1][0], segment.channelData[1][1]);
    EXPECT_FLOAT_EQ(segment.channelData[1][2], segment.channelData[1][3]);
    EXPECT_NE(segment.channelData[0][0], segment.channelData[1][0]);
    EXPECT_NE(segment.channelData[0][2], segment.channelData[1][2]);
}

TEST(AudioBitCrusherStandaloneTest, ChannelsBeyondStereoRemainUntouched)
{
    AudioBitCrusher crusher;
    crusher.setBits(2.0f);
    crusher.setMix(1.0f);

    AudioSegment segment;
    segment.sampleRate = 48000;
    segment.layout = AudioChannelLayout::Surround51;
    segment.channelData = {
        QVector<float>{0.3f, 0.7f},
        QVector<float>{-0.3f, -0.7f},
        QVector<float>{0.11f, 0.22f},
        QVector<float>{0.33f, 0.44f},
        QVector<float>{0.55f, 0.66f},
        QVector<float>{0.77f, 0.88f}};
    const QVector<QVector<float>> extraChannels{
        segment.channelData[2], segment.channelData[3],
        segment.channelData[4], segment.channelData[5]};

    crusher.process(segment);

    ASSERT_EQ(segment.channelData.size(), 6);
    for (int channel = 0; channel < 4; ++channel) {
        EXPECT_EQ(segment.channelData[channel + 2], extraChannels[channel]);
    }
    EXPECT_NE(segment.channelData[0], (QVector<float>{0.3f, 0.7f}));
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

TEST(AudioBitCrusherStandaloneTest, DeserializationUsesDefaultsForMissingParameters)
{
    AudioBitCrusher crusher;
    crusher.setBits(3.0f);
    crusher.setDownsample(12.0f);
    crusher.setMix(0.25f);

    crusher.fromJson(QJsonObject{});

    EXPECT_FLOAT_EQ(crusher.getBits(), 8.0f);
    EXPECT_FLOAT_EQ(crusher.getDownsample(), 1.0f);
    EXPECT_FLOAT_EQ(crusher.getMix(), 1.0f);
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
