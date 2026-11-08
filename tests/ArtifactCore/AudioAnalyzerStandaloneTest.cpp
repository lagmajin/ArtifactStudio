#include <gtest/gtest.h>

#include <QVector>

#include <algorithm>
#include <cmath>
#include <iterator>
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

TEST(AudioAnalyzerStandaloneTest, ResizingFFTRebuildsWindowUsedByNextAnalysis)
{
    AudioAnalyzer analyzer(8);
    const auto segment = makeMono(QVector<float>{
        0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 0.75f, 0.5f, 0.25f});

    const auto initial = analyzer.analyze(segment);
    ASSERT_EQ(initial.spectrum.size(), 5u);

    analyzer.setFFTSize(16);
    const auto expanded = analyzer.analyze(segment);
    EXPECT_EQ(expanded.spectrum.size(), 9u);
    EXPECT_FLOAT_EQ(expanded.rms, initial.rms);
    EXPECT_FLOAT_EQ(expanded.peak, initial.peak);

    analyzer.setFFTSize(8);
    const auto restored = analyzer.analyze(segment);
    EXPECT_EQ(restored.spectrum.size(), 5u);
    EXPECT_EQ(restored.spectrum, initial.spectrum);
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

TEST(AudioAnalyzerStandaloneTest, MismatchedChannelLengthsIgnoreEveryChannelTailAfterShortest)
{
    AudioSegment mismatched;
    mismatched.sampleRate = 48000;
    mismatched.layout = AudioChannelLayout::Stereo;
    mismatched.channelData.push_back(QVector<float>{0.5f, 0.25f, 500.0f, -500.0f});
    mismatched.channelData.push_back(QVector<float>{-0.25f, 0.5f});

    AudioSegment explicitlyTrimmed;
    explicitlyTrimmed.sampleRate = mismatched.sampleRate;
    explicitlyTrimmed.layout = mismatched.layout;
    explicitlyTrimmed.channelData.push_back(QVector<float>{0.5f, 0.25f});
    explicitlyTrimmed.channelData.push_back(QVector<float>{-0.25f, 0.5f});

    ASSERT_EQ(mismatched.frameCount(), 2);
    AudioAnalyzer analyzer(8);
    const auto actual = analyzer.analyze(mismatched);
    const auto expected = analyzer.analyze(explicitlyTrimmed);

    EXPECT_FLOAT_EQ(actual.rms, expected.rms);
    EXPECT_FLOAT_EQ(actual.peak, expected.peak);
    EXPECT_EQ(actual.spectrum, expected.spectrum);
    EXPECT_FLOAT_EQ(actual.lowIntensity, expected.lowIntensity);
    EXPECT_FLOAT_EQ(actual.midIntensity, expected.midIntensity);
    EXPECT_FLOAT_EQ(actual.highIntensity, expected.highIntensity);
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

TEST(AudioAnalyzerStandaloneTest, BinCenteredTonePeaksAtItsFrequencyWithHammingGain)
{
    constexpr int fftSize = 128;
    constexpr int toneBin = 11;
    constexpr float amplitude = 0.8f;
    constexpr double pi = 3.14159265358979323846;
    QVector<float> samples;
    samples.reserve(fftSize);
    for (int frame = 0; frame < fftSize; ++frame) {
        const double phase = 2.0 * pi * toneBin * frame / fftSize;
        samples.push_back(amplitude * static_cast<float>(std::sin(phase)));
    }

    AudioAnalyzer analyzer(fftSize);
    const auto result = analyzer.analyze(makeMono(std::move(samples)));

    ASSERT_EQ(result.spectrum.size(), fftSize / 2 + 1);
    const auto peak = std::max_element(result.spectrum.begin(), result.spectrum.end());
    ASSERT_NE(peak, result.spectrum.end());
    EXPECT_EQ(std::distance(result.spectrum.begin(), peak), toneBin);

    // Hamming coherent gain is sum(window) / N = 0.54 - 0.46 / N.
    const float expectedPeak = amplitude * (0.54f - 0.46f / fftSize);
    EXPECT_NEAR(result.spectrum[toneBin], expectedPeak, 1e-3f);
    EXPECT_NEAR(result.rms, amplitude / std::sqrt(2.0f), 1e-6f);
    EXPECT_NEAR(result.peak, amplitude, 1e-6f);
}

TEST(AudioAnalyzerStandaloneTest,
     DCAndNyquistBinsUseTheSameOneSidedAmplitudeScaleAsInteriorBins)
{
    constexpr int fftSize = 128;
    constexpr float amplitude = 0.75f;
    constexpr int nyquistBin = fftSize / 2;
    QVector<float> dcSamples;
    QVector<float> nyquistSamples;
    dcSamples.reserve(fftSize);
    nyquistSamples.reserve(fftSize);
    for (int frame = 0; frame < fftSize; ++frame) {
        dcSamples.push_back(amplitude);
        nyquistSamples.push_back(frame % 2 == 0 ? amplitude : -amplitude);
    }

    AudioAnalyzer analyzer(fftSize);
    const auto dc = analyzer.analyze(makeMono(std::move(dcSamples)));
    const auto nyquist = analyzer.analyze(makeMono(std::move(nyquistSamples)));
    ASSERT_EQ(dc.spectrum.size(), static_cast<std::size_t>(nyquistBin + 1));
    ASSERT_EQ(nyquist.spectrum.size(), static_cast<std::size_t>(nyquistBin + 1));

    // Hamming coherent gain is sum(window) / N. DC and Nyquist are not
    // mirrored bins, so one-sided amplitude normalization must not double them.
    const float expectedAmplitude = amplitude * (0.54f - 0.46f / fftSize);
    EXPECT_NEAR(dc.spectrum.front(), expectedAmplitude, 1e-3f);
    EXPECT_NEAR(nyquist.spectrum.back(), expectedAmplitude, 1e-3f);
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

TEST(AudioAnalyzerStandaloneTest, MaximumFiniteInputKeepsAnalysisMetricsFinite)
{
    AudioAnalyzer analyzer(8);
    const float maximum = std::numeric_limits<float>::max();
    const auto result = analyzer.analyze(makeMono(QVector<float>{maximum, maximum}));

    EXPECT_TRUE(std::isfinite(result.rms));
    EXPECT_FLOAT_EQ(result.rms, maximum);
    EXPECT_TRUE(std::isfinite(result.peak));
    EXPECT_FLOAT_EQ(result.peak, maximum);
    ASSERT_EQ(result.spectrum.size(), 5u);
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

TEST(AudioAnalyzerStandaloneTest, TruncatedFFTUsesLeadingWindowButMetricsIncludeLatePeak)
{
    AudioAnalyzer analyzer(8);
    const auto full = analyzer.analyze(makeMono(
        QVector<float>{0.25f, -0.5f, 0.75f, -1.0f, 0.5f, 0.0f, 0.25f, -0.5f, 4.0f}));
    const auto fftWindowOnly = analyzer.analyze(makeMono(
        QVector<float>{0.25f, -0.5f, 0.75f, -1.0f, 0.5f, 0.0f, 0.25f, -0.5f}));

    EXPECT_NEAR(full.rms, std::sqrt(18.4375f / 9.0f), 1e-6f);
    EXPECT_FLOAT_EQ(full.peak, 4.0f);
    EXPECT_EQ(full.spectrum, fftWindowOnly.spectrum);
    EXPECT_TRUE(std::any_of(full.spectrum.begin(), full.spectrum.end(),
                            [](float bin) { return bin > 0.0f; }));
}

TEST(AudioAnalyzerStandaloneTest, BandIntensitiesAverageBinsClampedToNyquist)
{
    AudioAnalyzer analyzer(8);
    const auto result = analyzer.analyze(makeMono(QVector<float>{
        0.0f, 1.0f, -0.5f, 0.25f, -0.75f, 0.5f, -0.25f, 0.125f}, 8000));

    ASSERT_EQ(result.spectrum.size(), 5u);
    const float expectedLow = result.spectrum[0];
    const float expectedMid = (result.spectrum[0] + result.spectrum[1] + result.spectrum[2]) / 3.0f;
    const float expectedHigh = (result.spectrum[2] + result.spectrum[3] + result.spectrum[4]) / 3.0f;
    EXPECT_FLOAT_EQ(result.lowIntensity, expectedLow);
    EXPECT_FLOAT_EQ(result.midIntensity, expectedMid);
    EXPECT_FLOAT_EQ(result.highIntensity, expectedHigh);
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

