#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <initializer_list>
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

TEST(AudioTransientShaperStandaloneTest, StereoChannelsKeepIndependentEnvelopeState)
{
    AudioTransientShaper stereoShaper;
    AudioTransientShaper leftReference;
    AudioTransientShaper rightReference;
    for (auto* shaper : {&stereoShaper, &leftReference, &rightReference}) {
        shaper->setAttack(1.0f);
        shaper->setSustain(-0.5f);
        shaper->setSpeedMs(5.0f);
    }

    const QVector<float> leftInput{1.0f, 0.0f, 0.0f, 0.25f, 0.0f};
    const QVector<float> rightInput{0.0f, 0.5f, 0.0f, 0.0f, 0.75f};
    AudioSegment stereo;
    stereo.sampleRate = 1000;
    stereo.layout = AudioChannelLayout::Stereo;
    stereo.channelData = {leftInput, rightInput};
    auto left = makeMono(1000, leftInput);
    auto right = makeMono(1000, rightInput);

    stereoShaper.process(stereo);
    leftReference.process(left);
    rightReference.process(right);

    EXPECT_EQ(stereo.channelData[0], left.channelData[0]);
    EXPECT_EQ(stereo.channelData[1], right.channelData[0]);
}

TEST(AudioTransientShaperStandaloneTest, ProcessingInBlocksPreservesEnvelopeStateContinuity)
{
    AudioTransientShaper contiguous;
    AudioTransientShaper blocked;
    for (auto* shaper : {&contiguous, &blocked}) {
        shaper->setAttack(0.8f);
        shaper->setSustain(-0.4f);
        shaper->setSpeedMs(8.0f);
    }

    const QVector<float> input{1.0f, 0.0f, 0.0f, -0.25f, 0.5f, 0.0f, 0.75f};
    auto whole = makeMono(1000, input);
    auto first = makeMono(1000, input.mid(0, 2));
    auto second = makeMono(1000, input.mid(2, 3));
    auto third = makeMono(1000, input.mid(5, 2));
    contiguous.process(whole);
    blocked.process(first);
    blocked.process(second);
    blocked.process(third);

    EXPECT_EQ(first.channelData[0], whole.channelData[0].mid(0, 2));
    EXPECT_EQ(second.channelData[0], whole.channelData[0].mid(2, 3));
    EXPECT_EQ(third.channelData[0], whole.channelData[0].mid(5, 2));
}

TEST(AudioTransientShaperStandaloneTest, ChannelsBeyondStereoRemainUnchanged)
{
    AudioTransientShaper shaper;
    shaper.setAttack(1.0f);
    shaper.setSustain(-0.5f);
    AudioSegment segment;
    segment.sampleRate = 1000;
    segment.layout = AudioChannelLayout::Surround51;
    segment.channelData = {
        QVector<float>{1.0f, 0.0f},
        QVector<float>{0.0f, 0.5f},
        QVector<float>{0.125f, -0.25f},
        QVector<float>{0.0f, 0.25f},
        QVector<float>{-0.5f, 0.75f},
        QVector<float>{0.875f, -1.0f}};
    const QVector<float> untouchedChannel = segment.channelData[2];
    const QVector<float> untouchedLfe = segment.channelData[3];
    const QVector<float> untouchedLeftSurround = segment.channelData[4];
    const QVector<float> untouchedRightSurround = segment.channelData[5];

    shaper.process(segment);

    EXPECT_EQ(segment.channelData[2], untouchedChannel);
    EXPECT_EQ(segment.channelData[3], untouchedLfe);
    EXPECT_EQ(segment.channelData[4], untouchedLeftSurround);
    EXPECT_EQ(segment.channelData[5], untouchedRightSurround);
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
