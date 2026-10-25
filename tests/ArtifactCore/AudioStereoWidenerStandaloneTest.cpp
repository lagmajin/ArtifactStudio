#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.StereoWidener;
import Audio.Segment;
import Core.ArtifactString;
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

TEST(AudioStereoWidenerStandaloneTest, ExposesExpectedDefaults)
{
    const AudioStereoWidener widener;
    EXPECT_EQ(toStdString(widener.getName()), "Stereo Widener");
    EXPECT_EQ(toStdString(widener.effectType()), "stereo_widener");
    EXPECT_FLOAT_EQ(widener.getWidth(), 1.2f);
    EXPECT_FLOAT_EQ(widener.getBassMonoFreq(), 120.0f);
    EXPECT_FLOAT_EQ(widener.getTrimDb(), 0.0f);
}

TEST(AudioStereoWidenerStandaloneTest, SettersClampAndRestoreDefaultsForNonFinite)
{
    AudioStereoWidener widener;
    widener.setWidth(99.0f);
    widener.setTrimDb(-99.0f);
    EXPECT_FLOAT_EQ(widener.getWidth(), 2.0f);
    EXPECT_FLOAT_EQ(widener.getTrimDb(), -12.0f);

    widener.setWidth(std::numeric_limits<float>::quiet_NaN());
    widener.setBassMonoFreq(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(widener.getWidth(), 1.2f);
    EXPECT_FLOAT_EQ(widener.getBassMonoFreq(), 120.0f);
}

TEST(AudioStereoWidenerStandaloneTest, ExposesAndUpdatesAutomationParametersById)
{
    AudioStereoWidener widener;
    const auto parameters = widener.getParameters();

    ASSERT_EQ(parameters.size(), 3u);
    EXPECT_EQ(toStdString(parameters[0].id), "width");
    EXPECT_FLOAT_EQ(parameters[0].minValue, 0.0f);
    EXPECT_FLOAT_EQ(parameters[0].maxValue, 2.0f);
    EXPECT_FLOAT_EQ(parameters[0].value, 1.2f);
    EXPECT_EQ(toStdString(parameters[1].id), "bass_mono_freq");
    EXPECT_FLOAT_EQ(parameters[1].minValue, 0.0f);
    EXPECT_FLOAT_EQ(parameters[1].maxValue, 500.0f);
    EXPECT_FLOAT_EQ(parameters[1].value, 120.0f);
    EXPECT_EQ(toStdString(parameters[2].id), "trim_db");
    EXPECT_FLOAT_EQ(parameters[2].minValue, -12.0f);
    EXPECT_FLOAT_EQ(parameters[2].maxValue, 12.0f);
    EXPECT_FLOAT_EQ(parameters[2].value, 0.0f);

    widener.setParameterValue(String("width"), -1.0f);
    widener.setParameterValue(String("bass_mono_freq"), 600.0f);
    widener.setParameterValue(String("trim_db"), -20.0f);
    EXPECT_FLOAT_EQ(widener.getParameterValue(String("width")), 0.0f);
    EXPECT_FLOAT_EQ(widener.getParameterValue(String("bass_mono_freq")), 500.0f);
    EXPECT_FLOAT_EQ(widener.getParameterValue(String("trim_db")), -12.0f);

    widener.setParameterValue(String("unknown"), 4.0f);
    EXPECT_FLOAT_EQ(widener.getParameterValue(String("unknown")), 0.0f);
    EXPECT_FLOAT_EQ(widener.getWidth(), 0.0f);
    EXPECT_FLOAT_EQ(widener.getBassMonoFreq(), 500.0f);
    EXPECT_FLOAT_EQ(widener.getTrimDb(), -12.0f);
}

TEST(AudioStereoWidenerStandaloneTest, MonoInputPassesThroughUnchanged)
{
    AudioStereoWidener widener;
    widener.setWidth(2.0f);
    auto segment = makeMono(48000, QVector<float>{0.5f, -0.25f, 0.125f, 0.0f});

    widener.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.5f, -0.25f, 0.125f, 0.0f}));
}

TEST(AudioStereoWidenerStandaloneTest, WidthExpandsOutOfPhaseStereo)
{
    AudioStereoWidener widener;
    widener.setWidth(2.0f);
    widener.setBassMonoFreq(0.0f);
    widener.setTrimDb(0.0f);
    // L=+1 / R=-1 は Side のみ。width 2 で L=+2 / R=-2 になる。
    auto segment = makeStereo(48000, QVector<float>(64, 1.0f), QVector<float>(64, -1.0f));

    widener.process(segment);

    EXPECT_FLOAT_EQ(segment.channelData[0][0], 2.0f);
    EXPECT_FLOAT_EQ(segment.channelData[1][0], -2.0f);
}

TEST(AudioStereoWidenerStandaloneTest, AdditionalSurroundChannelsRemainUntouched)
{
    AudioStereoWidener widener;
    widener.setWidth(2.0f);
    widener.setBassMonoFreq(0.0f);
    AudioSegment segment;
    segment.sampleRate = 48000;
    segment.layout = AudioChannelLayout::Surround51;
    segment.channelData = {
        QVector<float>{0.75f, -0.25f},
        QVector<float>{-0.5f, 0.5f},
        QVector<float>{0.1f, 0.2f},
        QVector<float>{0.3f, 0.4f},
        QVector<float>{0.5f, 0.6f},
        QVector<float>{0.7f, 0.8f}};
    const QVector<QVector<float>> extraChannels{
        segment.channelData[2], segment.channelData[3],
        segment.channelData[4], segment.channelData[5]};

    widener.process(segment);

    for (int channel = 0; channel < 4; ++channel) {
        EXPECT_EQ(segment.channelData[channel + 2], extraChannels[channel]);
    }
    EXPECT_NE(segment.channelData[0], (QVector<float>{0.75f, -0.25f}));
}

TEST(AudioStereoWidenerStandaloneTest, BassMonoKeepsSteadyLowFrequencySideAtOriginalWidth)
{
    AudioStereoWidener widener;
    widener.setWidth(2.0f);
    widener.setBassMonoFreq(120.0f);
    widener.setTrimDb(0.0f);
    auto segment = makeStereo(
        48000, QVector<float>(4096, 1.0f), QVector<float>(4096, -1.0f));

    widener.process(segment);

    EXPECT_NEAR(segment.channelData[0].last(), 1.0f, 0.0001f);
    EXPECT_NEAR(segment.channelData[1].last(), -1.0f, 0.0001f);
}

TEST(AudioStereoWidenerStandaloneTest, BassMonoStateIsContinuousAcrossSegmentBoundaries)
{
    AudioStereoWidener wholeBlockWidener;
    AudioStereoWidener segmentedWidener;
    for (auto* widener : {&wholeBlockWidener, &segmentedWidener}) {
        widener->setWidth(2.0f);
        widener->setBassMonoFreq(120.0f);
    }

    QVector<float> leftInput;
    QVector<float> rightInput;
    leftInput.reserve(96);
    rightInput.reserve(96);
    for (int frame = 0; frame < 96; ++frame) {
        const float side = (frame % 2 == 0) ? 0.75f : -0.25f;
        leftInput.push_back(side);
        rightInput.push_back(-side);
    }

    auto wholeBlock = makeStereo(48000, leftInput, rightInput);
    wholeBlockWidener.process(wholeBlock);

    QVector<float> segmentedLeft;
    QVector<float> segmentedRight;
    segmentedLeft.reserve(leftInput.size());
    segmentedRight.reserve(rightInput.size());
    constexpr int blockSizes[] = {13, 29, 7, 47};
    int start = 0;
    for (const int blockSize : blockSizes) {
        auto block = makeStereo(48000, leftInput.mid(start, blockSize),
                                rightInput.mid(start, blockSize));
        segmentedWidener.process(block);
        segmentedLeft += block.channelData[0];
        segmentedRight += block.channelData[1];
        start += blockSize;
    }

    EXPECT_EQ(start, leftInput.size());
    EXPECT_EQ(segmentedLeft, wholeBlock.channelData[0]);
    EXPECT_EQ(segmentedRight, wholeBlock.channelData[1]);
}

TEST(AudioStereoWidenerStandaloneTest, MonoPassThroughDoesNotAdvanceStereoFilterState)
{
    AudioStereoWidener withMonoInterruption;
    AudioStereoWidener uninterrupted;
    for (auto* widener : {&withMonoInterruption, &uninterrupted}) {
        widener->setWidth(2.0f);
        widener->setBassMonoFreq(120.0f);
    }

    auto firstWithInterruption = makeStereo(
        48000, QVector<float>{0.75f, -0.25f, 0.5f},
        QVector<float>{-0.75f, 0.25f, -0.5f});
    auto firstWithoutInterruption = firstWithInterruption;
    withMonoInterruption.process(firstWithInterruption);
    uninterrupted.process(firstWithoutInterruption);

    auto mono = makeMono(48000, QVector<float>{0.1f, 0.2f, 0.3f});
    const auto originalMono = mono.channelData[0];
    withMonoInterruption.process(mono);
    EXPECT_EQ(mono.channelData[0], originalMono);

    auto nextWithInterruption = makeStereo(
        48000, QVector<float>{-0.4f, 0.8f}, QVector<float>{0.4f, -0.8f});
    auto nextWithoutInterruption = nextWithInterruption;
    withMonoInterruption.process(nextWithInterruption);
    uninterrupted.process(nextWithoutInterruption);

    EXPECT_EQ(nextWithInterruption.channelData[0], nextWithoutInterruption.channelData[0]);
    EXPECT_EQ(nextWithInterruption.channelData[1], nextWithoutInterruption.channelData[1]);
}

TEST(AudioStereoWidenerStandaloneTest, TrimDbScalesMidSignalAmplitude)
{
    AudioStereoWidener widener;
    widener.setWidth(0.0f);
    widener.setBassMonoFreq(0.0f);
    widener.setTrimDb(-6.0206f);
    auto segment = makeStereo(
        48000, QVector<float>(8, 0.25f), QVector<float>(8, 0.25f));

    widener.process(segment);

    EXPECT_NEAR(segment.channelData[0].last(), 0.125f, 0.00001f);
    EXPECT_NEAR(segment.channelData[1].last(), 0.125f, 0.00001f);
}

TEST(AudioStereoWidenerStandaloneTest, BypassPreservesInputSamples)
{
    AudioStereoWidener widener;
    widener.setBypass(true);
    auto segment = makeStereo(48000, QVector<float>{0.5f}, QVector<float>{-0.5f});

    widener.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.5f}));
    EXPECT_EQ(segment.channelData[1], (QVector<float>{-0.5f}));
}

TEST(AudioStereoWidenerStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioStereoWidener widener;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    auto segment = makeStereo(48000, QVector<float>{nan}, QVector<float>{nan});

    widener.process(segment);

    EXPECT_TRUE(std::isfinite(segment.channelData[0][0]));
    EXPECT_TRUE(std::isfinite(segment.channelData[1][0]));
}

TEST(AudioStereoWidenerStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioStereoWidener widener;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    widener.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioStereoWidenerStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioStereoWidener source;
    source.setWidth(1.8f);
    source.setBassMonoFreq(200.0f);
    source.setTrimDb(-3.0f);
    source.setBypass(true);

    AudioStereoWidener restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getWidth(), 1.8f);
    EXPECT_FLOAT_EQ(restored.getBassMonoFreq(), 200.0f);
    EXPECT_FLOAT_EQ(restored.getTrimDb(), -3.0f);
    EXPECT_TRUE(restored.isBypassed());
}
