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
