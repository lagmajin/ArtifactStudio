#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.TransientShaper;
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

TEST(AudioTransientShaperStandaloneTest, ExposesExpectedDefaults)
{
    const AudioTransientShaper shaper;
    EXPECT_EQ(toStdString(shaper.getName()), "Transient Shaper");
    EXPECT_EQ(toStdString(shaper.effectType()), "transient_shaper");
    EXPECT_FLOAT_EQ(shaper.getAttack(), 0.3f);
    EXPECT_FLOAT_EQ(shaper.getSustain(), 0.0f);
    EXPECT_FLOAT_EQ(shaper.getSpeedMs(), 20.0f);
}

TEST(AudioTransientShaperStandaloneTest, SettersClampAndRestoreDefaultsForNonFinite)
{
    AudioTransientShaper shaper;
    shaper.setAttack(99.0f);
    shaper.setSustain(-99.0f);
    EXPECT_FLOAT_EQ(shaper.getAttack(), 1.0f);
    EXPECT_FLOAT_EQ(shaper.getSustain(), -1.0f);

    shaper.setAttack(std::numeric_limits<float>::quiet_NaN());
    shaper.setSpeedMs(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(shaper.getAttack(), 0.3f);
    EXPECT_FLOAT_EQ(shaper.getSpeedMs(), 20.0f);
}

TEST(AudioTransientShaperStandaloneTest, NeutralSettingsLeaveSignalUnchanged)
{
    AudioTransientShaper shaper;
    shaper.setAttack(0.0f);
    shaper.setSustain(0.0f);
    auto segment = makeMono(48000, QVector<float>{1.0f, -0.5f, 0.25f, 0.001f});

    shaper.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{1.0f, -0.5f, 0.25f, 0.001f}));
}

TEST(AudioTransientShaperStandaloneTest, FullAttackBoostsImpulseTransient)
{
    AudioTransientShaper shaper;
    shaper.setAttack(1.0f);
    shaper.setSustain(0.0f);
    QVector<float> impulse(4800, 0.0f);
    impulse[0] = 1.0f;
    auto segment = makeMono(48000, std::move(impulse));

    shaper.process(segment);

    EXPECT_GT(segment.channelData[0][0], 1.0f);
    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
}

TEST(AudioTransientShaperStandaloneTest, BypassPreservesInputSamples)
{
    AudioTransientShaper shaper;
    shaper.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    shaper.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioTransientShaperStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioTransientShaper shaper;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    auto segment = makeMono(48000, QVector<float>{nan, nan, nan});

    shaper.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
}

TEST(AudioTransientShaperStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioTransientShaper shaper;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    shaper.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioTransientShaperStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioTransientShaper source;
    source.setAttack(-0.5f);
    source.setSustain(0.7f);
    source.setSpeedMs(40.0f);
    source.setBypass(true);

    AudioTransientShaper restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getAttack(), -0.5f);
    EXPECT_FLOAT_EQ(restored.getSustain(), 0.7f);
    EXPECT_FLOAT_EQ(restored.getSpeedMs(), 40.0f);
    EXPECT_TRUE(restored.isBypassed());
}
