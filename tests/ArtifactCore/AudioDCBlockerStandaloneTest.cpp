#include <gtest/gtest.h>

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.DCBlocker;
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

TEST(AudioDCBlockerStandaloneTest, ExposesExpectedDefaults)
{
    const AudioDCBlocker dc;
    EXPECT_EQ(toStdString(dc.getName()), "DC Blocker");
    EXPECT_EQ(toStdString(dc.effectType()), "dc_blocker");
    EXPECT_FLOAT_EQ(dc.getFrequency(), 10.0f);
    EXPECT_FLOAT_EQ(dc.getMix(), 1.0f);
}

TEST(AudioDCBlockerStandaloneTest, SettersClampAndRestoreDefaultsForNonFinite)
{
    AudioDCBlocker dc;
    dc.setFrequency(9999.0f);
    dc.setMix(-1.0f);
    EXPECT_FLOAT_EQ(dc.getFrequency(), 100.0f);
    EXPECT_FLOAT_EQ(dc.getMix(), 0.0f);

    dc.setFrequency(std::numeric_limits<float>::quiet_NaN());
    dc.setMix(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(dc.getFrequency(), 10.0f);
    EXPECT_FLOAT_EQ(dc.getMix(), 1.0f);
}

TEST(AudioDCBlockerStandaloneTest, SettersClampFiniteValuesAtBothRangeEnds)
{
    AudioDCBlocker dc;
    dc.setFrequency(1.0f);
    EXPECT_FLOAT_EQ(dc.getFrequency(), 2.0f);
    dc.setFrequency(2.0f);
    EXPECT_FLOAT_EQ(dc.getFrequency(), 2.0f);
    dc.setFrequency(100.0f);
    EXPECT_FLOAT_EQ(dc.getFrequency(), 100.0f);
    dc.setFrequency(101.0f);
    EXPECT_FLOAT_EQ(dc.getFrequency(), 100.0f);

    dc.setMix(-0.1f);
    EXPECT_FLOAT_EQ(dc.getMix(), 0.0f);
    dc.setMix(0.0f);
    EXPECT_FLOAT_EQ(dc.getMix(), 0.0f);
    dc.setMix(1.0f);
    EXPECT_FLOAT_EQ(dc.getMix(), 1.0f);
    dc.setMix(1.1f);
    EXPECT_FLOAT_EQ(dc.getMix(), 1.0f);
}

TEST(AudioDCBlockerStandaloneTest, ExposesAndUpdatesAutomationParametersById)
{
    AudioDCBlocker dc;
    const auto parameters = dc.getParameters();

    ASSERT_EQ(parameters.size(), 2u);
    EXPECT_EQ(toStdString(parameters[0].id), "frequency");
    EXPECT_FLOAT_EQ(parameters[0].minValue, 2.0f);
    EXPECT_FLOAT_EQ(parameters[0].maxValue, 100.0f);
    EXPECT_FLOAT_EQ(parameters[0].defaultValue, 10.0f);
    EXPECT_FLOAT_EQ(parameters[0].value, 10.0f);
    EXPECT_EQ(toStdString(parameters[1].id), "mix");
    EXPECT_FLOAT_EQ(parameters[1].minValue, 0.0f);
    EXPECT_FLOAT_EQ(parameters[1].maxValue, 1.0f);
    EXPECT_FLOAT_EQ(parameters[1].defaultValue, 1.0f);
    EXPECT_FLOAT_EQ(parameters[1].value, 1.0f);

    dc.setParameterValue(String("frequency"), 120.0f);
    dc.setParameterValue(String("mix"), -0.5f);
    EXPECT_FLOAT_EQ(dc.getParameterValue(String("frequency")), 100.0f);
    EXPECT_FLOAT_EQ(dc.getParameterValue(String("mix")), 0.0f);

    dc.setParameterValue(String("unknown"), 55.0f);
    EXPECT_FLOAT_EQ(dc.getParameterValue(String("unknown")), 0.0f);
    EXPECT_FLOAT_EQ(dc.getFrequency(), 100.0f);
    EXPECT_FLOAT_EQ(dc.getMix(), 0.0f);
}

TEST(AudioDCBlockerStandaloneTest, ParameterSnapshotReflectsUpdatedSettings)
{
    AudioDCBlocker dc;
    dc.setFrequency(35.0f);
    dc.setMix(0.4f);

    const auto parameters = dc.getParameters();

    ASSERT_EQ(parameters.size(), 2u);
    EXPECT_FLOAT_EQ(parameters[0].value, 35.0f);
    EXPECT_FLOAT_EQ(parameters[1].value, 0.4f);
}

TEST(AudioDCBlockerStandaloneTest, ZeroMixPreservesDrySignalExactly)
{
    AudioDCBlocker dc;
    dc.setMix(0.0f);
    auto segment = makeMono(48000, QVector<float>{1.0f, -0.5f, 0.25f, 0.0f});

    dc.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{1.0f, -0.5f, 0.25f, 0.0f}));
}

TEST(AudioDCBlockerStandaloneTest, RemovesConstantDcOffset)
{
    AudioDCBlocker dc;
    dc.setFrequency(10.0f);
    dc.setMix(1.0f);
    auto segment = makeMono(48000, QVector<float>(4800, 0.5f));

    dc.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
    // 十分な時間の後は直流がほぼ消える。
    EXPECT_LT(std::abs(segment.channelData[0].last()), 0.05f);
}

TEST(AudioDCBlockerStandaloneTest, ProcessingInBlocksPreservesFilterStateContinuity)
{
    AudioDCBlocker wholeBlockFilter;
    AudioDCBlocker splitBlockFilter;
    const QVector<float> input{
        0.75f, -0.25f, 0.5f, 0.0f, -1.0f, 0.125f,
        0.875f, 0.25f, -0.5f, 0.0f, 1.0f};
    auto whole = makeMono(48000, input);
    wholeBlockFilter.process(whole);

    const QVector<QVector<float>> blocks{
        QVector<float>{0.75f, -0.25f, 0.5f},
        QVector<float>{0.0f, -1.0f, 0.125f, 0.875f, 0.25f},
        QVector<float>{-0.5f, 0.0f, 1.0f}};
    QVector<float> splitOutput;
    splitOutput.reserve(input.size());
    for (const auto& samples : blocks) {
        auto segment = makeMono(48000, samples);
        splitBlockFilter.process(segment);
        splitOutput += segment.channelData[0];
    }

    EXPECT_EQ(splitOutput, whole.channelData[0]);
}

TEST(AudioDCBlockerStandaloneTest, BypassPreservesInputSamples)
{
    AudioDCBlocker dc;
    dc.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    dc.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioDCBlockerStandaloneTest, BypassDoesNotAdvanceFilterHistory)
{
    AudioDCBlocker afterBypass;
    afterBypass.setBypass(true);
    auto bypassed = makeMono(48000, QVector<float>{0.75f, -0.25f, 0.5f});
    afterBypass.process(bypassed);
    afterBypass.setBypass(false);

    AudioDCBlocker fresh;
    auto nextAfterBypass = makeMono(48000, QVector<float>{0.25f, 0.0f});
    auto freshInput = makeMono(48000, QVector<float>{0.25f, 0.0f});
    afterBypass.process(nextAfterBypass);
    fresh.process(freshInput);

    EXPECT_EQ(nextAfterBypass.channelData[0], freshInput.channelData[0]);
}

TEST(AudioDCBlockerStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioDCBlocker dc;
    const float infinity = std::numeric_limits<float>::infinity();
    auto segment = makeMono(48000, QVector<float>{infinity, infinity, infinity});

    dc.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
}

TEST(AudioDCBlockerStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioDCBlocker dc;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    dc.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioDCBlockerStandaloneTest, StereoChannelsKeepIndependentFilterHistory)
{
    AudioDCBlocker stereoEffect;
    AudioDCBlocker leftReferenceEffect;
    AudioDCBlocker rightReferenceEffect;
    AudioSegment stereo;
    stereo.sampleRate = 48000;
    stereo.layout = AudioChannelLayout::Stereo;
    stereo.channelData = {
        QVector<float>{1.0f, 0.0f, 0.0f, 0.0f},
        QVector<float>{0.0f, 1.0f, 0.0f, 0.0f}};
    auto leftReference = makeMono(48000, QVector<float>{1.0f, 0.0f, 0.0f, 0.0f});
    auto rightReference = makeMono(48000, QVector<float>{0.0f, 1.0f, 0.0f, 0.0f});

    stereoEffect.process(stereo);
    leftReferenceEffect.process(leftReference);
    rightReferenceEffect.process(rightReference);

    EXPECT_EQ(stereo.channelData[0], leftReference.channelData[0]);
    EXPECT_EQ(stereo.channelData[1], rightReference.channelData[0]);
}

TEST(AudioDCBlockerStandaloneTest, LeavesChannelsBeyondStereoUntouched)
{
    AudioDCBlocker dc;
    AudioSegment segment;
    segment.sampleRate = 48000;
    segment.layout = AudioChannelLayout::Surround51;
    segment.channelData = {
        QVector<float>{0.5f, 0.5f, 0.5f},
        QVector<float>{-0.5f, -0.5f, -0.5f},
        QVector<float>{0.25f, 0.5f, 0.75f},
        QVector<float>{-0.25f, -0.5f, -0.75f},
        QVector<float>{0.1f, 0.2f, 0.3f},
        QVector<float>{-0.1f, -0.2f, -0.3f}};
    const QVector<QVector<float>> untouched{
        segment.channelData[2], segment.channelData[3],
        segment.channelData[4], segment.channelData[5]};

    dc.process(segment);

    for (int channel = 0; channel < 4; ++channel)
        EXPECT_EQ(segment.channelData[channel + 2], untouched[channel]) << channel;
    EXPECT_NE(segment.channelData[0], (QVector<float>{0.5f, 0.5f, 0.5f}));
}

TEST(AudioDCBlockerStandaloneTest, ZeroMixDoesNotAdvanceFilterHistory)
{
    AudioDCBlocker afterDryRun;
    afterDryRun.setMix(0.0f);
    auto dry = makeMono(48000, QVector<float>{0.75f, -0.25f, 0.5f});
    afterDryRun.process(dry);
    afterDryRun.setMix(1.0f);

    AudioDCBlocker fresh;
    auto nextAfterDryRun = makeMono(48000, QVector<float>{0.25f, 0.0f});
    auto freshInput = makeMono(48000, QVector<float>{0.25f, 0.0f});
    afterDryRun.process(nextAfterDryRun);
    fresh.process(freshInput);

    EXPECT_EQ(nextAfterDryRun.channelData[0], freshInput.channelData[0]);
}

TEST(AudioDCBlockerStandaloneTest, InvalidSampleRateDoesNotAdvanceFilterHistory)
{
    AudioDCBlocker afterInvalidRun;
    auto invalidRate = makeMono(0, QVector<float>{0.75f, -0.25f, 0.5f});
    afterInvalidRun.process(invalidRate);

    AudioDCBlocker fresh;
    auto nextAfterInvalidRun = makeMono(48000, QVector<float>{0.25f, 0.0f});
    auto freshInput = makeMono(48000, QVector<float>{0.25f, 0.0f});
    afterInvalidRun.process(nextAfterInvalidRun);
    fresh.process(freshInput);

    EXPECT_EQ(nextAfterInvalidRun.channelData[0], freshInput.channelData[0]);
}

TEST(AudioDCBlockerStandaloneTest, ZeroFrameInputDoesNotAdvanceFilterHistory)
{
    AudioDCBlocker afterEmptyRun;
    auto empty = makeMono(48000, QVector<float>{});
    afterEmptyRun.process(empty);

    AudioDCBlocker fresh;
    auto nextAfterEmptyRun = makeMono(48000, QVector<float>{0.25f, 0.0f});
    auto freshInput = makeMono(48000, QVector<float>{0.25f, 0.0f});
    afterEmptyRun.process(nextAfterEmptyRun);
    fresh.process(freshInput);

    EXPECT_EQ(nextAfterEmptyRun.channelData[0], freshInput.channelData[0]);
}

TEST(AudioDCBlockerStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioDCBlocker source;
    source.setFrequency(30.0f);
    source.setMix(0.8f);
    source.setBypass(true);

    AudioDCBlocker restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getFrequency(), 30.0f);
    EXPECT_FLOAT_EQ(restored.getMix(), 0.8f);
    EXPECT_TRUE(restored.isBypassed());
}

TEST(AudioDCBlockerStandaloneTest, MissingJsonSettingsRestoreDocumentedDefaults)
{
    AudioDCBlocker dc;
    dc.setFrequency(30.0f);
    dc.setMix(0.25f);

    dc.fromJson(QJsonObject{});

    EXPECT_FLOAT_EQ(dc.getFrequency(), 10.0f);
    EXPECT_FLOAT_EQ(dc.getMix(), 1.0f);
}

TEST(AudioDCBlockerStandaloneTest, WrongJsonSettingTypesRestoreDocumentedDefaults)
{
    AudioDCBlocker dc;
    dc.setFrequency(30.0f);
    dc.setMix(0.25f);
    QJsonObject malformed;
    malformed.insert(QStringLiteral("frequency"), QStringLiteral("high"));
    malformed.insert(QStringLiteral("mix"), QStringLiteral("wet"));

    dc.fromJson(malformed);

    EXPECT_FLOAT_EQ(dc.getFrequency(), 10.0f);
    EXPECT_FLOAT_EQ(dc.getMix(), 1.0f);
}

TEST(AudioDCBlockerStandaloneTest, JsonSettingsAreClampedToSupportedRanges)
{
    QJsonObject outOfRange;
    outOfRange.insert(QStringLiteral("frequency"), 200.0);
    outOfRange.insert(QStringLiteral("mix"), -0.5);
    AudioDCBlocker dc;

    dc.fromJson(outOfRange);

    EXPECT_FLOAT_EQ(dc.getFrequency(), 100.0f);
    EXPECT_FLOAT_EQ(dc.getMix(), 0.0f);
}
