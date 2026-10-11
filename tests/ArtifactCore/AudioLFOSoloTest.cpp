#include <gtest/gtest.h>

#include <cmath>
#include <limits>

import Audio.DSP.LFO;

using ArtifactCore::Audio::DSP::LFO;

TEST(AudioLFOSoloTest, DefaultOscillatorProducesZeroUntilConfigured)
{
    LFO lfo;
    for (int i = 0; i < 5; ++i)
        EXPECT_FLOAT_EQ(lfo.process(), 0.0f);
}

TEST(AudioLFOSoloTest, OneHertzAtFourSamplesPerSecondProducesSineCycle)
{
    LFO lfo;
    lfo.initialize(1.0f, 4.0f);

    EXPECT_NEAR(lfo.process(), 0.0f, 1e-6f);
    EXPECT_NEAR(lfo.process(), 1.0f, 1e-6f);
    EXPECT_NEAR(lfo.process(), 0.0f, 1e-6f);
    EXPECT_NEAR(lfo.process(), -1.0f, 1e-6f);
    EXPECT_NEAR(lfo.process(), 0.0f, 1e-6f);
}

TEST(AudioLFOSoloTest, InitializationResetsPhase)
{
    LFO lfo;
    lfo.initialize(1.0f, 4.0f);
    lfo.process();
    lfo.process();
    lfo.initialize(1.0f, 4.0f);

    EXPECT_NEAR(lfo.process(), 0.0f, 1e-6f);
    EXPECT_NEAR(lfo.process(), 1.0f, 1e-6f);
}

TEST(AudioLFOSoloTest, ZeroFrequencyHoldsCurrentSineValue)
{
    LFO lfo;
    lfo.initialize(1.0f, 4.0f);
    EXPECT_NEAR(lfo.process(), 0.0f, 1e-6f);
    lfo.setFrequency(0.0f);

    EXPECT_NEAR(lfo.process(), 1.0f, 1e-6f);
    EXPECT_NEAR(lfo.process(), 1.0f, 1e-6f);
}

TEST(AudioLFOSoloTest, NegativeFrequencyRunsTheCycleInReverse)
{
    LFO lfo;
    lfo.initialize(-1.0f, 4.0f);

    EXPECT_NEAR(lfo.process(), 0.0f, 1e-6f);
    EXPECT_NEAR(lfo.process(), -1.0f, 1e-6f);
    EXPECT_NEAR(lfo.process(), 0.0f, 1e-6f);
    EXPECT_NEAR(lfo.process(), 1.0f, 1e-6f);
}

TEST(AudioLFOSoloTest, NonFiniteFrequencyProducesStationaryZeroOutput)
{
    LFO lfo;
    lfo.initialize(std::numeric_limits<float>::quiet_NaN(), 4.0f);
    for (int i = 0; i < 8; ++i)
        EXPECT_FLOAT_EQ(lfo.process(), 0.0f);

    lfo.setFrequency(std::numeric_limits<float>::infinity());
    for (int i = 0; i < 4; ++i)
        EXPECT_FLOAT_EQ(lfo.process(), 0.0f);
}

TEST(AudioLFOSoloTest, InvalidSampleRateUsesTheDefaultRate)
{
    LFO lfo;
    lfo.initialize(12000.0f, 0.0f);

    EXPECT_NEAR(lfo.process(), 0.0f, 1e-6f);
    EXPECT_NEAR(lfo.process(), 1.0f, 1e-6f);
    EXPECT_NEAR(lfo.process(), 0.0f, 1e-5f);
    EXPECT_NEAR(lfo.process(), -1.0f, 1e-5f);
}

TEST(AudioLFOSoloTest, VeryHighFrequencyKeepsOutputFinite)
{
    LFO lfo;
    lfo.initialize(1.0e30f, 48000.0f);

    for (int i = 0; i < 100; ++i)
        EXPECT_TRUE(std::isfinite(lfo.process()));
}

TEST(AudioLFOSoloTest, LongRunRemainsBoundedAndFinite)
{
    LFO lfo;
    lfo.initialize(0.73f, 44100.0f);

    for (int i = 0; i < 10000; ++i) {
        const float sample = lfo.process();
        EXPECT_TRUE(std::isfinite(sample));
        EXPECT_GE(sample, -1.0f);
        EXPECT_LE(sample, 1.0f);
    }
}

