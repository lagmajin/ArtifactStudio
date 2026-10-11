#include <gtest/gtest.h>

#include <QVector>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.Spectrum;
import Audio.Segment;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
AudioSegment makeMono(int sampleRate, QVector<float> samples, qint64 startFrame = 0)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = AudioChannelLayout::Mono;
    segment.startFrame = startFrame;
    segment.channelData.push_back(std::move(samples));
    return segment;
}

QVector<float> constantSamples(float value, int count)
{
    return QVector<float>(count, value);
}
}

TEST(AudioSpectrumStandaloneTest, ExposesExpectedDefaultIdentityAndBuffers)
{
    const AudioSpectrum spectrum;
    EXPECT_EQ(toStdString(spectrum.getName()), "Audio Spectrum");
    EXPECT_EQ(spectrum.getBins(), 64);
    EXPECT_EQ(spectrum.getSpectrum().size(), 64u);
    EXPECT_EQ(spectrum.getWaveform().size(), 1024u);
    EXPECT_FALSE(spectrum.measurement().hasLoudness());
    EXPECT_TRUE(spectrum.measurement().isApproximate);
}

TEST(AudioSpectrumStandaloneTest, BinCountClampsToSupportedRange)
{
    AudioSpectrum spectrum;
    spectrum.setBins(0);
    EXPECT_EQ(spectrum.getBins(), 1);
    spectrum.setBins(-20);
    EXPECT_EQ(spectrum.getBins(), 1);
    spectrum.setBins((1 << 20) + 1);
    EXPECT_EQ(spectrum.getBins(), 1 << 20);
    spectrum.setBins(12);
    EXPECT_EQ(spectrum.getBins(), 12);
}

TEST(AudioSpectrumStandaloneTest, MonoConstantSignalProducesExpectedLevelAndPeak)
{
    AudioSpectrum spectrum;
    spectrum.setBins(8);
    auto segment = makeMono(48000, constantSamples(0.5f, 16));

    spectrum.process(segment);

    const float expectedLufs = static_cast<float>(-0.691 + 20.0 * std::log10(0.5));
    EXPECT_NEAR(spectrum.getMomentaryLufs(), expectedLufs, 1e-4f);
    EXPECT_NEAR(spectrum.getShortTermLufs(), expectedLufs, 1e-4f);
    EXPECT_NEAR(spectrum.getIntegratedLufs(), expectedLufs, 1e-4f);
    EXPECT_NEAR(spectrum.getPeakDb(), 20.0f * std::log10(0.5f), 1e-4f);
    EXPECT_NEAR(spectrum.getTruePeakDb(), spectrum.getPeakDb(), 1e-5f);
    EXPECT_EQ(spectrum.getWaveform().size(), 16u);
    EXPECT_FLOAT_EQ(spectrum.getWaveform()[0], 0.5f);
    EXPECT_EQ(spectrum.getSpectrum().size(), 8u);
}

TEST(AudioSpectrumStandaloneTest, ImpulseRetainsSamplePeakAndHasFiniteSpectrum)
{
    AudioSpectrum spectrum;
    spectrum.setBins(4);
    auto segment = makeMono(48000, QVector<float>{0.0f, 1.0f, 0.0f, 0.0f});

    spectrum.process(segment);

    EXPECT_NEAR(spectrum.getPeakDb(), 0.0f, 1e-5f);
    EXPECT_NEAR(spectrum.getTruePeakDb(), 0.0f, 1e-5f);
    ASSERT_EQ(spectrum.getSpectrum().size(), 4u);
    for (const float bin : spectrum.getSpectrum())
        EXPECT_TRUE(std::isfinite(bin));
}

TEST(AudioSpectrumStandaloneTest, StereoLoudnessUsesMeanChannelEnergy)
{
    AudioSpectrum spectrum;
    spectrum.setBins(4);
    AudioSegment segment;
    segment.sampleRate = 48000;
    segment.layout = AudioChannelLayout::Stereo;
    segment.channelData.push_back(constantSamples(0.5f, 8));
    segment.channelData.push_back(constantSamples(0.0f, 8));

    spectrum.process(segment);

    const float expected = static_cast<float>(-0.691 + 10.0 * std::log10(0.125));
    EXPECT_NEAR(spectrum.getMomentaryLufs(), expected, 1e-4f);
    EXPECT_NEAR(spectrum.getPeakDb(), 20.0f * std::log10(0.5f), 1e-4f);
    EXPECT_FLOAT_EQ(spectrum.getWaveform()[0], 0.25f);
}

TEST(AudioSpectrumStandaloneTest, NonFiniteSamplesDoNotContaminateMeasurements)
{
    AudioSpectrum spectrum;
    spectrum.setBins(4);
    auto segment = makeMono(48000, QVector<float>{
        0.5f, std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), -0.5f});

    spectrum.process(segment);

    EXPECT_TRUE(std::isfinite(spectrum.getMomentaryLufs()));
    EXPECT_TRUE(std::isfinite(spectrum.getPeakDb()));
    for (const float value : spectrum.getSpectrum())
        EXPECT_TRUE(std::isfinite(value));
    for (const float value : spectrum.getWaveform())
        EXPECT_TRUE(std::isfinite(value));
}

TEST(AudioSpectrumStandaloneTest, EmptyBlockClearsAnalysisAndLoudnessState)
{
    AudioSpectrum spectrum;
    spectrum.setBins(6);
    auto nonEmpty = makeMono(48000, constantSamples(0.5f, 8));
    spectrum.process(nonEmpty);
    ASSERT_TRUE(spectrum.measurement().hasLoudness());

    auto empty = makeMono(48000, {});
    spectrum.process(empty);

    EXPECT_EQ(spectrum.getSpectrum().size(), 6u);
    EXPECT_EQ(spectrum.getWaveform().size(), 1024u);
    EXPECT_FALSE(spectrum.measurement().hasLoudness());
    EXPECT_FALSE(spectrum.measurement().hasTruePeak());
    for (const float bin : spectrum.getSpectrum()) EXPECT_FLOAT_EQ(bin, 0.0f);
    for (const float sample : spectrum.getWaveform()) EXPECT_FLOAT_EQ(sample, 0.0f);
}

TEST(AudioSpectrumStandaloneTest, ResetClearsLoudnessWithoutChangingBinCount)
{
    AudioSpectrum spectrum;
    spectrum.setBins(10);
    auto segment = makeMono(48000, constantSamples(0.25f, 8));
    spectrum.process(segment);
    ASSERT_TRUE(spectrum.measurement().hasLoudness());

    spectrum.resetLoudnessMeasurement();

    EXPECT_EQ(spectrum.getBins(), 10);
    EXPECT_FALSE(spectrum.measurement().hasLoudness());
    EXPECT_FALSE(spectrum.measurement().hasTruePeak());
    EXPECT_FLOAT_EQ(spectrum.getLoudnessRangeLufs(), 0.0f);
}

TEST(AudioSpectrumStandaloneTest, DiscontinuousSegmentsRestartIntegratedWindow)
{
    AudioSpectrum spectrum;
    spectrum.setBins(4);
    auto first = makeMono(48000, constantSamples(0.5f, 8), 100);
    spectrum.process(first);
    auto discontinuous = makeMono(48000, constantSamples(0.25f, 8), 1000);

    spectrum.process(discontinuous);

    const float expected = static_cast<float>(-0.691 + 20.0 * std::log10(0.25));
    EXPECT_NEAR(spectrum.getIntegratedLufs(), expected, 1e-4f);
}

TEST(AudioSpectrumStandaloneTest, NormalizationTargetsRequestedLoudness)
{
    AudioSpectrum spectrum;
    spectrum.setBins(4);
    auto segment = makeMono(48000, constantSamples(0.25f, 8));
    constexpr float targetLufs = -6.0f;

    ASSERT_TRUE(spectrum.normalizeToTargetLufs(segment, targetLufs));

    const float expectedGain = std::pow(10.0f, spectrum.normalizationGainDb(targetLufs) / 20.0f);
    for (const float sample : segment.channelData[0])
        EXPECT_NEAR(sample, 0.25f * expectedGain, 1e-5f);
}

TEST(AudioSpectrumStandaloneTest, NonFiniteNormalizationTargetUsesUnityGain)
{
    AudioSpectrum spectrum;
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    EXPECT_FLOAT_EQ(spectrum.normalizationGainDb(-14.0f), 0.0f);
    EXPECT_TRUE(spectrum.normalizeToTargetLufs(segment,
                                              std::numeric_limits<float>::quiet_NaN()));
    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioSpectrumStandaloneTest, BypassLeavesInputAndPreviousMeasurementsUnchanged)
{
    AudioSpectrum spectrum;
    spectrum.setBins(4);
    auto first = makeMono(48000, constantSamples(0.5f, 8));
    spectrum.process(first);
    const float previousLufs = spectrum.getIntegratedLufs();
    spectrum.setBypass(true);
    auto bypassed = makeMono(48000, QVector<float>{0.1f, -0.2f, 0.3f});

    spectrum.process(bypassed);

    EXPECT_EQ(bypassed.channelData[0], (QVector<float>{0.1f, -0.2f, 0.3f}));
    EXPECT_FLOAT_EQ(spectrum.getIntegratedLufs(), previousLufs);
}

