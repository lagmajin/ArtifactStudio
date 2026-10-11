#include <gtest/gtest.h>

#include <QVector>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Analyze;
import Audio.Segment;

using namespace ArtifactCore;

namespace {
AudioSegment makeMono(QVector<float> samples, int sampleRate = 48000)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = AudioChannelLayout::Mono;
    segment.channelData.push_back(std::move(samples));
    return segment;
}
}

TEST(AudioAnalyzerStandaloneTest, DefaultFFTSizeIsPowerOfTwo)
{
    const AudioAnalyzer analyzer;
    EXPECT_EQ(analyzer.getFFTSize(), 1024);
}

TEST(AudioAnalyzerStandaloneTest, ConstructorNormalizesRequestedFFTSize)
{
    EXPECT_EQ(AudioAnalyzer(0).getFFTSize(), 2);
    EXPECT_EQ(AudioAnalyzer(3).getFFTSize(), 4);
    EXPECT_EQ(AudioAnalyzer(8).getFFTSize(), 8);
    EXPECT_EQ(AudioAnalyzer(9).getFFTSize(), 16);
}

TEST(AudioAnalyzerStandaloneTest, SetterNormalizesFFTSizeAndKeepsValidPowerOfTwo)
{
    AudioAnalyzer analyzer(8);
    analyzer.setFFTSize(5);
    EXPECT_EQ(analyzer.getFFTSize(), 8);
    analyzer.setFFTSize(-1);
    EXPECT_EQ(analyzer.getFFTSize(), 2);
    analyzer.setFFTSize(17);
    EXPECT_EQ(analyzer.getFFTSize(), 32);
}

TEST(AudioAnalyzerStandaloneTest, EmptySegmentReturnsZeroMetricsAndNoSpectrum)
{
    AudioAnalyzer analyzer(8);
    AudioSegment segment;

    const auto result = analyzer.analyze(segment);

    EXPECT_FLOAT_EQ(result.rms, 0.0f);
    EXPECT_FLOAT_EQ(result.peak, 0.0f);
    EXPECT_TRUE(result.spectrum.empty());
    EXPECT_FLOAT_EQ(result.lowIntensity, 0.0f);
    EXPECT_FLOAT_EQ(result.midIntensity, 0.0f);
    EXPECT_FLOAT_EQ(result.highIntensity, 0.0f);
}

TEST(AudioAnalyzerStandaloneTest, MonoInputProducesExpectedRmsAndPeak)
{
    AudioAnalyzer analyzer(8);
    const auto result = analyzer.analyze(makeMono(QVector<float>{1.0f, -1.0f, 1.0f, -1.0f}));

    EXPECT_FLOAT_EQ(result.rms, 1.0f);
    EXPECT_FLOAT_EQ(result.peak, 1.0f);
    EXPECT_EQ(result.spectrum.size(), 5u);
}

TEST(AudioAnalyzerStandaloneTest, StereoMetricsUseAllChannelsAndSpectrumUsesTheirMix)
{
    AudioAnalyzer analyzer(8);
    AudioSegment segment;
    segment.sampleRate = 48000;
    segment.layout = AudioChannelLayout::Stereo;
    segment.channelData.push_back(QVector<float>{0.5f, 0.5f, 0.5f, 0.5f});
    segment.channelData.push_back(QVector<float>{-0.5f, -0.5f, -0.5f, -0.5f});

    const auto result = analyzer.analyze(segment);

    EXPECT_FLOAT_EQ(result.rms, 0.5f);
    EXPECT_FLOAT_EQ(result.peak, 0.5f);
    ASSERT_EQ(result.spectrum.size(), 5u);
    for (const float bin : result.spectrum)
        EXPECT_FLOAT_EQ(bin, 0.0f);
}

TEST(AudioAnalyzerStandaloneTest, SpectrumHasHalfSizePlusOneFiniteMagnitudeBins)
{
    AudioAnalyzer analyzer(16);
    const auto result = analyzer.analyze(makeMono(QVector<float>{
        0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 0.75f, 0.5f, 0.25f}));

    ASSERT_EQ(result.spectrum.size(), 9u);
    for (const float bin : result.spectrum) {
        EXPECT_TRUE(std::isfinite(bin));
        EXPECT_GE(bin, 0.0f);
    }
}

TEST(AudioAnalyzerStandaloneTest, NonFiniteInputIsExcludedFromPeakAndSanitizedForFft)
{
    AudioAnalyzer analyzer(8);
    const auto result = analyzer.analyze(makeMono(QVector<float>{
        1.0f, std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), -1.0f}));

    EXPECT_NEAR(result.rms, std::sqrt(0.5f), 1e-6f);
    EXPECT_FLOAT_EQ(result.peak, 1.0f);
    for (const float bin : result.spectrum)
        EXPECT_TRUE(std::isfinite(bin));
}

TEST(AudioAnalyzerStandaloneTest, InvalidSampleRateKeepsBandIntensitiesAtZero)
{
    AudioAnalyzer analyzer(8);
    const auto result = analyzer.analyze(makeMono(QVector<float>{0.5f, -0.5f}, 0));

    EXPECT_FLOAT_EQ(result.rms, 0.5f);
    EXPECT_FLOAT_EQ(result.peak, 0.5f);
    EXPECT_FLOAT_EQ(result.lowIntensity, 0.0f);
    EXPECT_FLOAT_EQ(result.midIntensity, 0.0f);
    EXPECT_FLOAT_EQ(result.highIntensity, 0.0f);
}

TEST(AudioAnalyzerStandaloneTest, ShortInputIsZeroPaddedToConfiguredFFTSize)
{
    AudioAnalyzer analyzer(16);
    const auto result = analyzer.analyze(makeMono(QVector<float>{1.0f}));

    EXPECT_EQ(result.spectrum.size(), 9u);
    EXPECT_FLOAT_EQ(result.rms, 1.0f);
    EXPECT_FLOAT_EQ(result.peak, 1.0f);
}

TEST(AudioAnalyzerStandaloneTest, RMSAndPeakUseAllFramesWhenFFTTruncatesInput)
{
    AudioAnalyzer analyzer(2);
    const auto result = analyzer.analyze(makeMono(QVector<float>{1.0f, 0.0f, 0.0f, 1.0f}));

    EXPECT_FLOAT_EQ(result.rms, std::sqrt(0.5f));
    EXPECT_FLOAT_EQ(result.peak, 1.0f);
    EXPECT_EQ(result.spectrum.size(), 2u);
}

TEST(AudioAnalyzerStandaloneTest, AllEmptyChannelsReturnEmptyAnalysis)
{
    AudioAnalyzer analyzer(8);
    AudioSegment segment;
    segment.sampleRate = 48000;
    segment.layout = AudioChannelLayout::Stereo;
    segment.channelData.push_back(QVector<float>{});
    segment.channelData.push_back(QVector<float>{});

    const auto result = analyzer.analyze(segment);

    EXPECT_FLOAT_EQ(result.rms, 0.0f);
    EXPECT_FLOAT_EQ(result.peak, 0.0f);
    EXPECT_TRUE(result.spectrum.empty());
}

