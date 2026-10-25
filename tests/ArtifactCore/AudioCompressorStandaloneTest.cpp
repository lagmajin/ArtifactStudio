#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>

#include <cstddef>
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

TEST(AudioCompressorStandaloneTest, ParameterDescriptorsMatchIdGettersAndSetters)
{
    AudioCompressor compressor;
    const auto parameters = compressor.getParameters();
    ASSERT_EQ(parameters.size(), 5u);

    for (std::size_t index = 0; index < parameters.size(); ++index) {
        const auto& parameter = parameters[index];
        EXPECT_FALSE(parameter.id.isEmpty());
        EXPECT_LE(parameter.minValue, parameter.maxValue);
        EXPECT_GE(parameter.defaultValue, parameter.minValue);
        EXPECT_LE(parameter.defaultValue, parameter.maxValue);
        EXPECT_FLOAT_EQ(parameter.value, compressor.getParameterValue(parameter.id));
        for (std::size_t previous = 0; previous < index; ++previous)
            EXPECT_NE(parameters[previous].id, parameter.id);

        float probe = (parameter.minValue + parameter.maxValue) * 0.5f;
        if (probe == parameter.value)
            probe = parameter.value == parameter.minValue
                ? parameter.maxValue : parameter.minValue;
        if (parameter.id == "sidechain") probe = 1.0f;
        compressor.setParameterValue(parameter.id, probe);
        EXPECT_FLOAT_EQ(compressor.getParameterValue(parameter.id), probe);
    }

    const QJsonObject beforeUnknownId = compressor.toJson();
    compressor.setParameterValue(String("unknown_parameter"), 0.5f);
    EXPECT_EQ(compressor.toJson(), beforeUnknownId);
    EXPECT_FLOAT_EQ(compressor.getParameterValue(String("unknown_parameter")), 0.0f);
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

TEST(AudioCompressorStandaloneTest, GainReductionReflectsTheCurrentQuietBlock)
{
    AudioCompressor compressor;
    configureFastCompressor(compressor);
    auto loud = makeMono(1000, QVector<float>{1.0f});
    compressor.process(loud);
    ASSERT_LT(compressor.getGainReduction(), 1.0f);

    auto quiet = makeMono(1000, QVector<float>{0.0f});
    compressor.process(quiet);

    EXPECT_FLOAT_EQ(compressor.getGainReduction(), 1.0f);
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

TEST(AudioCompressorStandaloneTest, SampleRateChangeResetsPreviousEnvelopeHistory)
{
    AudioCompressor withHistory;
    configureFastCompressor(withHistory);
    auto loudHistory = makeMono(1000, QVector<float>{1.0f, 1.0f, 1.0f});
    withHistory.process(loudHistory);
    ASSERT_LT(withHistory.getGainReduction(), 1.0f);

    auto afterRateChange = makeMono(2000, QVector<float>{0.25f, 0.2f, 0.15f});
    withHistory.process(afterRateChange);

    AudioCompressor freshAtNewRate;
    configureFastCompressor(freshAtNewRate);
    auto freshOutput = makeMono(2000, QVector<float>{0.25f, 0.2f, 0.15f});
    freshAtNewRate.process(freshOutput);

    EXPECT_EQ(afterRateChange.channelData[0], freshOutput.channelData[0]);
    EXPECT_FLOAT_EQ(withHistory.getGainReduction(), freshAtNewRate.getGainReduction());
}

TEST(AudioCompressorStandaloneTest, InvalidSampleRateAndEmptyFramesResetReportedGainReduction)
{
    AudioCompressor invalidRateCompressor;
    configureFastCompressor(invalidRateCompressor);
    auto loud = makeMono(1000, QVector<float>{1.0f});
    invalidRateCompressor.process(loud);
    ASSERT_LT(invalidRateCompressor.getGainReduction(), 1.0f);

    auto invalidRate = makeMono(0, QVector<float>{0.25f, -0.5f});
    invalidRateCompressor.process(invalidRate);
    EXPECT_EQ(invalidRate.channelData[0], (QVector<float>{0.25f, -0.5f}));
    EXPECT_FLOAT_EQ(invalidRateCompressor.getGainReduction(), 1.0f);

    AudioCompressor emptyFramesCompressor;
    configureFastCompressor(emptyFramesCompressor);
    auto loudAtValidRate = makeMono(1000, QVector<float>{1.0f});
    emptyFramesCompressor.process(loudAtValidRate);
    ASSERT_LT(emptyFramesCompressor.getGainReduction(), 1.0f);

    auto emptyFrames = makeMono(1000, QVector<float>{});
    emptyFramesCompressor.process(emptyFrames);
    EXPECT_FLOAT_EQ(emptyFramesCompressor.getGainReduction(), 1.0f);
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

TEST(AudioCompressorStandaloneTest, DisabledSidechainUsesProgramDetector)
{
    AudioCompressor compressor;
    configureFastCompressor(compressor);
    compressor.setSideChain(false);
    auto program = makeMono(1000, QVector<float>{0.25f});
    auto sideChain = makeMono(1000, QVector<float>{1.0f});

    compressor.process(program, &sideChain);

    EXPECT_FLOAT_EQ(program.channelData[0][0], 0.25f);
    EXPECT_FLOAT_EQ(compressor.getGainReduction(), 1.0f);
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

TEST(AudioCompressorStandaloneTest, ShortSidechainTreatsMissingFramesAsSilence)
{
    const QVector<float> programSamples{0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f};
    auto shortProgram = makeMono(1000, programSamples);
    auto paddedProgram = makeMono(1000, programSamples);
    auto shortSidechain = makeMono(1000, QVector<float>{1.0f, 0.0f});
    auto paddedSidechain = makeMono(1000, QVector<float>{1.0f, 0.0f, 0.0f,
                                                         0.0f, 0.0f, 0.0f});

    AudioCompressor shortInput;
    configureFastCompressor(shortInput);
    shortInput.setSideChain(true);
    shortInput.process(shortProgram, &shortSidechain);

    AudioCompressor explicitSilence;
    configureFastCompressor(explicitSilence);
    explicitSilence.setSideChain(true);
    explicitSilence.process(paddedProgram, &paddedSidechain);

    EXPECT_EQ(shortProgram.channelData[0], paddedProgram.channelData[0]);
    EXPECT_FLOAT_EQ(shortInput.getGainReduction(), explicitSilence.getGainReduction());
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

TEST(AudioCompressorStandaloneTest, ExtremeFiniteStereoInputKeepsOutputAndEnvelopeFinite)
{
    const float nearLimit = std::numeric_limits<float>::max() * 0.95f;
    QVector<float> left(256, nearLimit);
    QVector<float> right(256, -nearLimit);
    for (qsizetype frame = 1; frame < left.size(); frame += 2) {
        left[frame] = -nearLimit;
        right[frame] = nearLimit;
    }

    AudioCompressor compressor;
    configureFastCompressor(compressor);
    auto segment = makeStereo(48000, std::move(left), std::move(right));
    compressor.process(segment);
    for (int channel = 0; channel < 2; ++channel) {
        for (const float sample : segment.channelData[channel])
            EXPECT_TRUE(std::isfinite(sample));
    }
    EXPECT_TRUE(std::isfinite(compressor.getGainReduction()));
    EXPECT_GE(compressor.getGainReduction(), 0.0f);
    EXPECT_LE(compressor.getGainReduction(), 1.0f);

    auto silence = makeStereo(48000, QVector<float>(128, 0.0f),
                              QVector<float>(128, 0.0f));
    compressor.process(silence);
    for (int channel = 0; channel < 2; ++channel) {
        for (const float sample : silence.channelData[channel])
            EXPECT_TRUE(std::isfinite(sample));
    }
    EXPECT_TRUE(std::isfinite(compressor.getGainReduction()));
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

TEST(AudioCompressorStandaloneTest, MissingJsonParametersRestoreDefaults)
{
    AudioCompressor compressor;
    compressor.setThreshold(-12.0f);
    compressor.setRatio(7.0f);
    compressor.setAttack(25.0f);
    compressor.setRelease(250.0f);
    compressor.setSideChain(true);
    compressor.setBypass(true);

    compressor.fromJson(QJsonObject{});

    EXPECT_FLOAT_EQ(compressor.getParameterValue(String("threshold")), -20.0f);
    EXPECT_FLOAT_EQ(compressor.getParameterValue(String("ratio")), 4.0f);
    EXPECT_FLOAT_EQ(compressor.getParameterValue(String("attack_ms")), 10.0f);
    EXPECT_FLOAT_EQ(compressor.getParameterValue(String("release_ms")), 100.0f);
    EXPECT_FLOAT_EQ(compressor.getParameterValue(String("sidechain")), 0.0f);
    EXPECT_FALSE(compressor.isBypassed());
}
