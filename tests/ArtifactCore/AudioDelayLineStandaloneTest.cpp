#include <gtest/gtest.h>

#include <cmath>
#include <limits>

import Audio.DSP.DelayLine;

using ArtifactCore::Audio::DSP::FractionalDelayLine;

TEST(AudioDelayLineStandaloneTest, UninitializedLineReadsSilenceAndIgnoresWrites)
{
    FractionalDelayLine line;
    line.write(1.0f);
    EXPECT_FLOAT_EQ(line.read(1.0f), 0.0f);
    EXPECT_FLOAT_EQ(line.readSeconds(0.5f), 0.0f);
}

TEST(AudioDelayLineStandaloneTest, OneSampleDelayReturnsMostRecentlyWrittenSample)
{
    FractionalDelayLine line;
    line.initialize(1.0f, 4.0f);
    line.write(0.75f);

    EXPECT_FLOAT_EQ(line.read(1.0f), 0.75f);
}

TEST(AudioDelayLineStandaloneTest, FractionalDelayLinearlyInterpolatesAdjacentSamples)
{
    FractionalDelayLine line;
    line.initialize(1.0f, 4.0f);
    line.write(1.0f);
    line.write(2.0f);

    EXPECT_FLOAT_EQ(line.read(1.5f), 1.5f);
}

TEST(AudioDelayLineStandaloneTest, SecondsBasedReadUsesConfiguredSampleRate)
{
    FractionalDelayLine line;
    line.initialize(1.0f, 4.0f);
    line.write(-0.25f);

    EXPECT_FLOAT_EQ(line.readSeconds(0.25f), line.read(1.0f));
    EXPECT_FLOAT_EQ(line.readSeconds(0.25f), -0.25f);
}

TEST(AudioDelayLineStandaloneTest, RingBufferWrapPreservesRecentHistory)
{
    FractionalDelayLine line;
    line.initialize(0.25f, 4.0f); // Five slots after guard space.
    for (float sample = 1.0f; sample <= 7.0f; sample += 1.0f)
        line.write(sample);

    EXPECT_FLOAT_EQ(line.read(1.0f), 7.0f);
    EXPECT_FLOAT_EQ(line.read(2.0f), 6.0f);
}

TEST(AudioDelayLineStandaloneTest, ClearErasesHistoryAndResetsWritePosition)
{
    FractionalDelayLine line;
    line.initialize(1.0f, 4.0f);
    line.write(1.0f);
    line.write(2.0f);
    line.clear();
    EXPECT_FLOAT_EQ(line.read(1.0f), 0.0f);

    line.write(9.0f);
    EXPECT_FLOAT_EQ(line.read(1.0f), 9.0f);
}

TEST(AudioDelayLineStandaloneTest, ReinitializeDiscardsOldSamples)
{
    FractionalDelayLine line;
    line.initialize(1.0f, 4.0f);
    line.write(0.5f);
    line.initialize(0.5f, 4.0f);

    EXPECT_FLOAT_EQ(line.read(1.0f), 0.0f);
}

TEST(AudioDelayLineStandaloneTest, NonFiniteDelayIsTreatedAsZero)
{
    FractionalDelayLine line;
    line.initialize(1.0f, 4.0f);

    EXPECT_FLOAT_EQ(line.read(std::numeric_limits<float>::quiet_NaN()), 0.0f);
    EXPECT_FLOAT_EQ(line.read(std::numeric_limits<float>::infinity()), 0.0f);
}

TEST(AudioDelayLineStandaloneTest, NegativeDelayClampsToZero)
{
    FractionalDelayLine line;
    line.initialize(1.0f, 4.0f);
    line.write(3.0f);

    EXPECT_FLOAT_EQ(line.read(-2.0f), 0.0f);
}

TEST(AudioDelayLineStandaloneTest, InvalidInitializationUsesSafeFallbackSampleRate)
{
    FractionalDelayLine line;
    line.initialize(-1.0f, std::numeric_limits<float>::quiet_NaN());
    line.write(4.0f);

    EXPECT_FLOAT_EQ(line.readSeconds(1.0f / 48000.0f), 4.0f);
}

