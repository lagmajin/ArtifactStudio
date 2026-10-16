#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.Compressor;
import Audio.Segment;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
AudioSegment makeMono(int sampleRate, QVector<float> samples)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = AudioChannelLayout::Mono;
    segment.startFrame = 123;
    segment.channelData.push_back(std::move(samples));
    return segment;
}

AudioSegment makeStereo(int sampleRate, QVector<float> left, QVector<float> right)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = AudioChannelLayout::Stereo;
    segment.channelData.push_back(std::move(left));
    segment.channelData.push_back(std::move(right));
    return segment;
}

void configureFastCompressor(AudioCompressor& compressor)
{
    compressor.setThreshold(-6.0f);
    compressor.setRatio(4.0f);
    compressor.setAttack(0.1f);
    compressor.setRelease(1.0f);
}
}

TEST(AudioCompressorStandaloneTest, ExposesExpectedDefaultsAndParameterValues)
{
    const AudioCompressor compressor;
    EXPECT_EQ(toStdString(compressor.getName()), "Compressor");
    EXPECT_EQ(toStdString(compressor.effectType()), "compressor");
    EXPECT_FLOAT_EQ(compressor.getParameterValue(String("threshold")), -20.0f);
    EXPECT_FLOAT_EQ(compressor.getParameterValue(String("ratio")), 4.0f);
    EXPECT_FLOAT_EQ(compressor.getParameterValue(String("attack_ms")), 10.0f);
    EXPECT_FLOAT_EQ(compressor.getParameterValue(String("release_ms")), 100.0f);
    EXPECT_FLOAT_EQ(compressor.getParameterValue(String("sidechain")), 0.0f);
    EXPECT_FLOAT_EQ(compressor.getGainReduction(), 1.0f);
}

TEST(AudioCompressorStandaloneTest, SettersClampFiniteValuesAndReplaceNonFiniteValues)
{
    AudioCompressor compressor;
    compressor.setThreshold(-200.0f);
    compressor.setRatio(100.0f);
    compressor.setAttack(0.0f);
    compressor.setRelease(9000.0f);
    EXPECT_FLOAT_EQ(compressor.getParameterValue(String("threshold")), -120.0f);
    EXPECT_FLOAT_EQ(compressor.getParameterValue(String("ratio")), 20.0f);
    EXPECT_FLOAT_EQ(compressor.getParameterValue(String("attack_ms")), 0.1f);
    EXPECT_FLOAT_EQ(compressor.getParameterValue(String("release_ms")), 5000.0f);

    compressor.setThreshold(std::numeric_limits<float>::infinity());
    compressor.setRatio(std::numeric_limits<float>::quiet_NaN());
    compressor.setAttack(std::numeric_limits<float>::infinity());
    compressor.setRelease(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(compressor.getParameterValue(String("threshold")), -20.0f);
    EXPECT_FLOAT_EQ(compressor.getParameterValue(String("ratio")), 4.0f);
    EXPECT_FLOAT_EQ(compressor.getParameterValue(String("attack_ms")), 10.0f);
    EXPECT_FLOAT_EQ(compressor.getParameterValue(String("release_ms")), 100.0f);
}

TEST(AudioCompressorStandaloneTest, RatioOneLeavesAboveThresholdAudioUnchanged)
{
    AudioCompressor compressor;
    configureFastCompressor(compressor);
    compressor.setRatio(1.0f);
    auto segment = makeMono(1000, QVector<float>{1.0f, 0.8f});

    compressor.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{1.0f, 0.8f}));
    EXPECT_FLOAT_EQ(compressor.getGainReduction(), 1.0f);
}

TEST(AudioCompressorStandaloneTest, ReducesAboveThresholdAndLeavesBelowThresholdSignalAlone)
{
    AudioCompressor compressor;
    configureFastCompressor(compressor);
    auto loud = makeMono(1000, QVector<float>{1.0f});
    compressor.process(loud);
    EXPECT_GT(loud.channelData[0][0], 0.0f);
    EXPECT_LT(loud.channelData[0][0], 1.0f);
    EXPECT_LT(compressor.getGainReduction(), 1.0f);

    AudioCompressor belowThreshold;
    configureFastCompressor(belowThreshold);
    auto quiet = makeMono(1000, QVector<float>{0.25f});
    belowThreshold.process(quiet);
    EXPECT_FLOAT_EQ(quiet.channelData[0][0], 0.25f);
    EXPECT_FLOAT_EQ(belowThreshold.getGainReduction(), 1.0f);
}

TEST(AudioCompressorStandaloneTest, ProcessingInBlocksMatchesContinuousEnvelope)
{
    const QVector<float> input{1.0f, 1.0f, 0.2f, 0.2f, 0.0f, 0.0f,
                               0.8f, 0.8f, 0.0f, 0.0f, 0.5f, 0.5f};

    AudioCompressor continuous;
    configureFastCompressor(continuous);
    auto continuousSegment = makeMono(1000, input);
    continuous.process(continuousSegment);

    AudioCompressor blocked;
    configureFastCompressor(blocked);
    constexpr int boundaries[] = {0, 1, 4, 7, 12};
    QVector<float> blockedOutput;
    blockedOutput.reserve(input.size());
    for (int block = 1; block < 5; ++block) {
        const int start = boundaries[block - 1];
        const int length = boundaries[block] - start;
        auto segment = makeMono(1000, input.mid(start, length));
        blocked.process(segment);
        for (const float sample : segment.channelData[0])
            blockedOutput.push_back(sample);
    }

    EXPECT_EQ(blockedOutput, continuousSegment.channelData[0]);
}

TEST(AudioCompressorStandaloneTest, LoudestStereoChannelControlsGainForBothChannels)
{
    AudioCompressor compressor;
    configureFastCompressor(compressor);
    auto segment = makeStereo(1000, QVector<float>{1.0f}, QVector<float>{0.25f});

    compressor.process(segment);

    EXPECT_GT(segment.channelData[0][0], 0.0f);
    EXPECT_LT(segment.channelData[0][0], 1.0f);
    EXPECT_LT(segment.channelData[1][0], 0.25f);
    EXPECT_LT(compressor.getGainReduction(), 1.0f);
}

TEST(AudioCompressorStandaloneTest, EnabledSidechainControlsProgramSignalLevel)
{
    AudioCompressor compressor;
    configureFastCompressor(compressor);
    compressor.setSideChain(true);
    auto program = makeMono(1000, QVector<float>{0.25f});
    auto sideChain = makeMono(1000, QVector<float>{1.0f});

    compressor.process(program, &sideChain);

    EXPECT_GT(program.channelData[0][0], 0.0f);
    EXPECT_LT(program.channelData[0][0], 0.25f);
    EXPECT_LT(compressor.getGainReduction(), 1.0f);
}

TEST(AudioCompressorStandaloneTest, MissingSidechainFallsBackToProgramDetector)
{
    AudioCompressor compressor;
    configureFastCompressor(compressor);
    compressor.setSideChain(true);
    auto program = makeMono(1000, QVector<float>{1.0f});

    compressor.process(program, nullptr);

    EXPECT_LT(program.channelData[0][0], 1.0f);
    EXPECT_LT(compressor.getGainReduction(), 1.0f);
}

TEST(AudioCompressorStandaloneTest, BypassPreservesSamplesAndResetsReportedGainReduction)
{
    AudioCompressor compressor;
    configureFastCompressor(compressor);
    auto loud = makeMono(1000, QVector<float>{1.0f});
    compressor.process(loud);
    ASSERT_LT(compressor.getGainReduction(), 1.0f);

    compressor.setBypass(true);
    auto bypassed = makeMono(1000, QVector<float>{0.75f});
    compressor.process(bypassed);

    EXPECT_EQ(bypassed.channelData[0], (QVector<float>{0.75f}));
    EXPECT_FLOAT_EQ(compressor.getGainReduction(), 1.0f);
}

TEST(AudioCompressorStandaloneTest, NonFiniteSamplesBecomeSilence)
{
    AudioCompressor compressor;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    auto segment = makeMono(48000, QVector<float>{nan, infinity, -infinity});

    compressor.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.0f, 0.0f, 0.0f}));
    EXPECT_FLOAT_EQ(compressor.getGainReduction(), 1.0f);
}

TEST(AudioCompressorStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioCompressor source;
    source.setThreshold(-12.0f);
    source.setRatio(7.0f);
    source.setAttack(25.0f);
    source.setRelease(250.0f);
    source.setSideChain(true);
    source.setBypass(true);

    AudioCompressor restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getParameterValue(String("threshold")), -12.0f);
    EXPECT_FLOAT_EQ(restored.getParameterValue(String("ratio")), 7.0f);
    EXPECT_FLOAT_EQ(restored.getParameterValue(String("attack_ms")), 25.0f);
    EXPECT_FLOAT_EQ(restored.getParameterValue(String("release_ms")), 250.0f);
    EXPECT_FLOAT_EQ(restored.getParameterValue(String("sidechain")), 1.0f);
    EXPECT_TRUE(restored.isBypassed());
}
