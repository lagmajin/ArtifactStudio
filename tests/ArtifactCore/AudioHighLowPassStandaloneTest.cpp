#include <gtest/gtest.h>

#include <QVector>

#include <cmath>
#include <limits>
#include <utility>

import Audio.Effect.HighLowPass;
import Audio.Segment;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
AudioSegment makeMono(int sampleRate, QVector<float> samples)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = AudioChannelLayout::Mono;
    segment.channelData.push_back(std::move(samples));
    return segment;
}
}

TEST(AudioHighLowPassStandaloneTest, DefaultsToDisabledFilters)
{
    const AudioHighLowPass filter;
    EXPECT_EQ(toStdString(filter.getName()), "High-Low Pass");
    EXPECT_FLOAT_EQ(filter.getLowPassFreq(), 0.0f);
    EXPECT_FLOAT_EQ(filter.getHighPassFreq(), 0.0f);
}

TEST(AudioHighLowPassStandaloneTest, DisabledFiltersPassFiniteSamplesUnchanged)
{
    AudioHighLowPass filter;
    auto segment = makeMono(48000, QVector<float>{0.25f, -0.5f, 0.75f});

    filter.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f, 0.75f}));
}

TEST(AudioHighLowPassStandaloneTest, LowPassImpulseResponseFollowsOnePoleRecurrence)
{
    AudioHighLowPass filter;
    constexpr float cutoffHz = 1000.0f;
    constexpr float sampleRate = 4000.0f;
    filter.setLowPassFreq(cutoffHz);
    auto segment = makeMono(static_cast<int>(sampleRate),
                            QVector<float>{1.0f, 0.0f, 0.0f, 0.0f});

    filter.process(segment);

    const float coefficient = std::exp(-2.0f * 3.14159265f * cutoffHz / sampleRate);
    EXPECT_NEAR(segment.channelData[0][0], 1.0f, 1e-6f);
    EXPECT_NEAR(segment.channelData[0][1], coefficient, 1e-6f);
    EXPECT_NEAR(segment.channelData[0][2], coefficient * coefficient, 1e-6f);
    EXPECT_NEAR(segment.channelData[0][3], coefficient * coefficient * coefficient, 1e-6f);
}

TEST(AudioHighLowPassStandaloneTest, FilterStateContinuesAcrossAdjacentBlocks)
{
    AudioHighLowPass wholeBlockFilter;
    AudioHighLowPass splitBlockFilter;
    wholeBlockFilter.setLowPassFreq(800.0f);
    splitBlockFilter.setLowPassFreq(800.0f);
    auto whole = makeMono(48000, QVector<float>{1.0f, 0.5f, 0.0f, -0.5f, -1.0f, 0.25f});
    auto first = makeMono(48000, QVector<float>{1.0f, 0.5f, 0.0f});
    auto second = makeMono(48000, QVector<float>{-0.5f, -1.0f, 0.25f});

    wholeBlockFilter.process(whole);
    splitBlockFilter.process(first);
    splitBlockFilter.process(second);

    ASSERT_EQ(whole.channelData[0].size(), 6);
    for (int frame = 0; frame < 3; ++frame)
        EXPECT_NEAR(whole.channelData[0][frame], first.channelData[0][frame], 1e-6f);
    for (int frame = 0; frame < 3; ++frame)
        EXPECT_NEAR(whole.channelData[0][frame + 3], second.channelData[0][frame], 1e-6f);
}

TEST(AudioHighLowPassStandaloneTest, HighPassRejectsSteadyDcAfterItsTransient)
{
    AudioHighLowPass filter;
    filter.setHighPassFreq(1000.0f);
    auto segment = makeMono(48000, QVector<float>(128, 1.0f));

    filter.process(segment);

    ASSERT_EQ(segment.channelData[0].size(), 128);
    EXPECT_NEAR(segment.channelData[0][0], 0.0f, 1e-6f);
    EXPECT_LT(std::abs(segment.channelData[0].back()), 1e-3f);
}

TEST(AudioHighLowPassStandaloneTest, BypassPreservesInputSamples)
{
    AudioHighLowPass filter;
    filter.setLowPassFreq(500.0f);
    filter.setBypass(true);
    auto segment = makeMono(48000, QVector<float>{0.25f, 0.5f, 0.75f});

    filter.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, 0.5f, 0.75f}));
}

TEST(AudioHighLowPassStandaloneTest, NonFiniteInputSamplesAreReplacedWithSilence)
{
    AudioHighLowPass filter;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    auto segment = makeMono(48000, QVector<float>{nan, infinity, -infinity});

    filter.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.0f, 0.0f, 0.0f}));
}

TEST(AudioHighLowPassStandaloneTest, InvalidSampleRateLeavesInputUnchanged)
{
    AudioHighLowPass filter;
    filter.setLowPassFreq(1000.0f);
    auto segment = makeMono(0, QVector<float>{0.25f, -0.5f});

    filter.process(segment);

    EXPECT_EQ(segment.channelData[0], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioHighLowPassStandaloneTest, FrequencySettersClampFiniteValuesAndReplaceNonFiniteValues)
{
    AudioHighLowPass filter;
    filter.setLowPassFreq(50000.0f);
    filter.setHighPassFreq(-10.0f);
    EXPECT_FLOAT_EQ(filter.getLowPassFreq(), 24000.0f);
    EXPECT_FLOAT_EQ(filter.getHighPassFreq(), 0.0f);

    filter.setLowPassFreq(std::numeric_limits<float>::infinity());
    filter.setHighPassFreq(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(filter.getLowPassFreq(), 0.0f);
    EXPECT_FLOAT_EQ(filter.getHighPassFreq(), 0.0f);
}
