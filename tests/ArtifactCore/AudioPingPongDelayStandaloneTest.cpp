#include <gtest/gtest.h>

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <initializer_list>
#include <limits>
#include <utility>

import Audio.Effect.PingPongDelay;
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
    segment.channelData.push_back(std::move(left));
    segment.channelData.push_back(std::move(right));
    return segment;
}
}

TEST(AudioPingPongDelayStandaloneTest, ExposesExpectedDefaults)
{
    const AudioPingPongDelay delay;
    EXPECT_EQ(toStdString(delay.getName()), "Ping-Pong Delay");
    EXPECT_EQ(toStdString(delay.effectType()), "ping_pong_delay");
    EXPECT_FLOAT_EQ(delay.getTimeMs(), 375.0f);
    EXPECT_FLOAT_EQ(delay.getFeedback(), 0.4f);
    EXPECT_FLOAT_EQ(delay.getMix(), 0.35f);
    EXPECT_EQ(delay.latencySamples(), 0);
    EXPECT_GT(delay.tailSamples(), 0);
}

TEST(AudioPingPongDelayStandaloneTest, SettersClampAndRestoreDefaultsForNonFinite)
{
    AudioPingPongDelay delay;
    delay.setTimeMs(9999.0f);
    delay.setFeedback(2.0f);
    EXPECT_FLOAT_EQ(delay.getTimeMs(), 2000.0f);
    EXPECT_FLOAT_EQ(delay.getFeedback(), 0.95f);

    delay.setTimeMs(std::numeric_limits<float>::quiet_NaN());
    delay.setMix(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(delay.getTimeMs(), 375.0f);
    EXPECT_FLOAT_EQ(delay.getMix(), 0.35f);
}

TEST(AudioPingPongDelayStandaloneTest, TailEstimateGrowsWithFeedbackAndIsZeroWhenDisabled)
{
    AudioPingPongDelay delay;
    delay.setTimeMs(100.0f);
    delay.setFeedback(0.0f);
    EXPECT_EQ(delay.tailSamples(), 0);

    delay.setFeedback(0.5f);
    const qint64 halfFeedbackTail = delay.tailSamples();
    const double oneDelayInSamples = 100.0 * 0.001 * 44100.0;
    const qint64 expectedHalfFeedbackTail = static_cast<qint64>(
        oneDelayInSamples * std::log(0.001) / std::log(0.5));
    EXPECT_EQ(halfFeedbackTail, expectedHalfFeedbackTail);

    delay.setFeedback(0.9f);
    EXPECT_GT(delay.tailSamples(), halfFeedbackTail);
}

TEST(AudioPingPongDelayStandaloneTest, ZeroMixPreservesDrySignalExactly)
{
    AudioPingPongDelay delay;
    delay.setMix(0.0f);
    auto segment = makeMono(48000, QVector<float>{1.0f, -0.5f, 0.25f, 0.0f});

    delay.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{1.0f, -0.5f, 0.25f, 0.0f}));
}

TEST(AudioPingPongDelayStandaloneTest, FullWetImpulseProducesEchoTail)
{
    AudioPingPongDelay delay;
    delay.setTimeMs(100.0f);
    delay.setFeedback(0.5f);
    delay.setMix(1.0f);
    // 100ms @48kHz = 4800 samples。2 エコーぶん確保する。
    QVector<float> impulse(15000, 0.0f);
    impulse[0] = 1.0f;
    auto segment = makeMono(48000, std::move(impulse));

    delay.process(segment);

    // 遅延読み出しが空のため先頭は無音。
    EXPECT_FLOAT_EQ(segment.channelData[0][0], 0.0f);
    bool hasEcho = false;
    for (const float sample : segment.channelData[0])
        hasEcho = hasEcho || (sample != 0.0f);
    EXPECT_TRUE(hasEcho);
}

TEST(AudioPingPongDelayStandaloneTest, StereoImpulseCrossFeedsAlternatingEchoes)
{
    AudioPingPongDelay delay;
    delay.setTimeMs(1.0f);
    delay.setFeedback(0.5f);
    delay.setMix(1.0f);

    AudioSegment segment;
    segment.sampleRate = 4000;
    segment.layout = AudioChannelLayout::Stereo;
    segment.channelData = {
        QVector<float>{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                       0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        QVector<float>(12, 0.0f)};

    delay.process(segment);

    EXPECT_FLOAT_EQ(segment.channelData[0][0], 0.0f);
    EXPECT_FLOAT_EQ(segment.channelData[1][0], 0.0f);
    EXPECT_FLOAT_EQ(segment.channelData[0][4], 1.0f);
    EXPECT_FLOAT_EQ(segment.channelData[1][4], 0.0f);
    EXPECT_FLOAT_EQ(segment.channelData[0][8], 0.0f);
    EXPECT_FLOAT_EQ(segment.channelData[1][8], 0.5f);
}

TEST(AudioPingPongDelayStandaloneTest, CrossFeedEchoesContinueAcrossBlockBoundaries)
{
    AudioPingPongDelay contiguous;
    AudioPingPongDelay segmented;
    for (auto* delay : {&contiguous, &segmented}) {
        delay->setTimeMs(1.0f);
        delay->setFeedback(0.5f);
        delay->setMix(1.0f);
    }

    QVector<float> leftInput(12, 0.0f);
    QVector<float> rightInput(12, 0.0f);
    leftInput[0] = 1.0f;
    auto whole = makeStereo(4000, leftInput, rightInput);
    contiguous.process(whole);

    QVector<float> splitLeft;
    QVector<float> splitRight;
    splitLeft.reserve(12);
    splitRight.reserve(12);
    for (int start = 0; start < 12; start += 4) {
        auto block = makeStereo(4000, leftInput.mid(start, 4), rightInput.mid(start, 4));
        segmented.process(block);
        splitLeft += block.channelData[0];
        splitRight += block.channelData[1];
    }

    EXPECT_EQ(splitLeft, whole.channelData[0]);
    EXPECT_EQ(splitRight, whole.channelData[1]);
    EXPECT_FLOAT_EQ(splitLeft[4], 1.0f);
    EXPECT_FLOAT_EQ(splitRight[8], 0.5f);
}

TEST(AudioPingPongDelayStandaloneTest, BypassPreservesInputSamples)
{
    AudioPingPongDelay delay;
    delay.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    delay.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioPingPongDelayStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioPingPongDelay delay;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    auto segment = makeMono(48000, QVector<float>{nan, nan, nan});

    delay.process(segment);

    for (const float sample : segment.channelData[0])
        EXPECT_TRUE(std::isfinite(sample));
}

TEST(AudioPingPongDelayStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioPingPongDelay delay;
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    delay.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioPingPongDelayStandaloneTest, JsonRoundTripPreservesSettingsAndBypass)
{
    AudioPingPongDelay source;
    source.setTimeMs(500.0f);
    source.setFeedback(0.7f);
    source.setMix(0.6f);
    source.setBypass(true);

    AudioPingPongDelay restored;
    restored.fromJson(source.toJson());

    EXPECT_FLOAT_EQ(restored.getTimeMs(), 500.0f);
    EXPECT_FLOAT_EQ(restored.getFeedback(), 0.7f);
    EXPECT_FLOAT_EQ(restored.getMix(), 0.6f);
    EXPECT_TRUE(restored.isBypassed());
}
