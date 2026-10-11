#include <gtest/gtest.h>

#include <cmath>
#include <initializer_list>
#include <limits>

import Audio.Modulation.Modulator;

using namespace ArtifactCore::Audio::Modulation;

TEST(AudioModulatorsStandaloneTest, LfoSineAdvancesBySampleRateAndFrequency)
{
    LfoSource lfo;
    lfo.setSampleRate(4.0f);
    lfo.setFrequency(1.0f);
    EXPECT_NEAR(lfo.process(1), 0.0f, 1e-6f);
    EXPECT_NEAR(lfo.process(1), 1.0f, 1e-6f);
    EXPECT_NEAR(lfo.process(1), 0.0f, 1e-6f);
    EXPECT_NEAR(lfo.process(1), -1.0f, 1e-6f);
}

TEST(AudioModulatorsStandaloneTest, LfoTriangleSawAndSquareWaveformsUseExpectedPhase)
{
    LfoSource triangle(LfoWaveform::Triangle);
    triangle.setSampleRate(4.0f);
    triangle.setFrequency(1.0f);
    EXPECT_FLOAT_EQ(triangle.process(1), -1.0f);
    EXPECT_FLOAT_EQ(triangle.process(1), 0.0f);
    EXPECT_FLOAT_EQ(triangle.process(1), 1.0f);

    LfoSource saw(LfoWaveform::Sawtooth);
    saw.setSampleRate(4.0f);
    saw.setFrequency(1.0f);
    EXPECT_FLOAT_EQ(saw.process(1), -1.0f);
    EXPECT_FLOAT_EQ(saw.process(1), -0.5f);

    LfoSource square(LfoWaveform::Square);
    square.setSampleRate(4.0f);
    square.setFrequency(1.0f);
    square.setPulseWidth(0.25f);
    EXPECT_FLOAT_EQ(square.process(1), 1.0f);
    EXPECT_FLOAT_EQ(square.process(1), -1.0f);
}

TEST(AudioModulatorsStandaloneTest, LfoPhaseOffsetAndResetAreDeterministic)
{
    LfoSource lfo;
    lfo.setSampleRate(4.0f);
    lfo.setFrequency(1.0f);
    lfo.setPhase(0.75f);
    EXPECT_NEAR(lfo.process(1), -1.0f, 1e-6f);
    lfo.process(1);
    lfo.reset();
    EXPECT_NEAR(lfo.process(1), -1.0f, 1e-6f);
}

TEST(AudioModulatorsStandaloneTest, LfoWrapsPhaseAndIgnoresNonFinitePhaseInput)
{
    LfoSource lfo;
    lfo.setSampleRate(4.0f);
    lfo.setFrequency(1.0f);
    lfo.setPhase(0.75f);
    EXPECT_NEAR(lfo.process(2), -1.0f, 1e-6f);
    EXPECT_NEAR(lfo.phase(), 0.25f, 1e-6f);
    lfo.setPhase(std::numeric_limits<float>::infinity());
    EXPECT_NEAR(lfo.phaseOffset(), 0.75f, 1e-6f);
}

TEST(AudioModulatorsStandaloneTest, LfoClampsFrequencyAndSanitizesSampleRate)
{
    LfoSource lfo;
    lfo.setSampleRate(8.0f);
    lfo.setFrequency(20.0f);
    EXPECT_FLOAT_EQ(lfo.frequency(), 4.0f);
    lfo.setFrequency(-2.0f);
    EXPECT_FLOAT_EQ(lfo.frequency(), 0.0f);
    lfo.setSampleRate(0.0f);
    lfo.setFrequency(1.0f);
    EXPECT_FLOAT_EQ(lfo.frequency(), 1.0f);
}

TEST(AudioModulatorsStandaloneTest, LfoUnipolarModeMapsWaveformToZeroThroughOne)
{
    LfoSource lfo(LfoWaveform::Triangle);
    lfo.setUnipolar(true);
    EXPECT_FALSE(lfo.bipolar());
    EXPECT_FLOAT_EQ(lfo.process(0), 0.0f);
    lfo.setPhase(0.5f);
    EXPECT_FLOAT_EQ(lfo.process(0), 1.0f);
}

TEST(AudioModulatorsStandaloneTest, AdsrTraversesAttackDecaySustainAndRelease)
{
    AdsrSource adsr;
    adsr.setSampleRate(1000.0f);
    adsr.setAttack(0.004f);
    adsr.setDecay(0.004f);
    adsr.setSustain(0.5f);
    adsr.setRelease(0.004f);
    adsr.gate(true);
    EXPECT_EQ(adsr.stage(), AdsrSource::Stage::Attack);
    EXPECT_FLOAT_EQ(adsr.process(2), 0.5f);
    EXPECT_FLOAT_EQ(adsr.process(2), 1.0f);
    EXPECT_EQ(adsr.stage(), AdsrSource::Stage::Decay);
    EXPECT_FLOAT_EQ(adsr.process(2), 0.5f);
    EXPECT_EQ(adsr.stage(), AdsrSource::Stage::Sustain);
    EXPECT_FLOAT_EQ(adsr.process(5), 0.5f);
    adsr.gate(false);
    EXPECT_EQ(adsr.stage(), AdsrSource::Stage::Release);
    EXPECT_FLOAT_EQ(adsr.process(2), 0.0f);
    EXPECT_EQ(adsr.stage(), AdsrSource::Stage::Idle);
}

TEST(AudioModulatorsStandaloneTest, AdsrZeroFramesDoNotAdvanceEnvelope)
{
    AdsrSource adsr;
    adsr.setSampleRate(1000.0f);
    adsr.gate(true);
    EXPECT_FLOAT_EQ(adsr.process(0), 0.0f);
    EXPECT_EQ(adsr.stage(), AdsrSource::Stage::Attack);
}

TEST(AudioModulatorsStandaloneTest, AdsrResetReturnsToIdleAtZero)
{
    AdsrSource adsr;
    adsr.setSampleRate(1000.0f);
    adsr.gate(true);
    adsr.process(100);
    adsr.reset();
    EXPECT_FLOAT_EQ(adsr.process(0), 0.0f);
    EXPECT_EQ(adsr.stage(), AdsrSource::Stage::Idle);
}

TEST(AudioModulatorsStandaloneTest, AdsrParametersClampOrRejectInvalidValues)
{
    AdsrSource adsr;
    adsr.setAttack(-1.0f);
    adsr.setDecay(std::numeric_limits<float>::infinity());
    adsr.setRelease(std::numeric_limits<float>::quiet_NaN());
    adsr.setSustain(2.0f);
    EXPECT_FLOAT_EQ(adsr.attack(), 0.000001f);
    EXPECT_FLOAT_EQ(adsr.decay(), 0.000001f);
    EXPECT_FLOAT_EQ(adsr.release(), 0.000001f);
    EXPECT_FLOAT_EQ(adsr.sustain(), 1.0f);
    adsr.setSustain(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(adsr.sustain(), 0.0f);
}

TEST(AudioModulatorsStandaloneTest, MacroSourceClampsNormalizedValuesAndIgnoresTime)
{
    MacroSource macro;
    macro.setValue(1.5f);
    EXPECT_FLOAT_EQ(macro.process(0), 1.0f);
    EXPECT_FLOAT_EQ(macro.process(100000), 1.0f);
    EXPECT_FALSE(macro.bipolar());
    macro.setValue(-0.5f);
    EXPECT_FLOAT_EQ(macro.value(), 0.0f);
}

TEST(AudioModulatorsStandaloneTest, MacroSourceMapsNonFiniteValueToZero)
{
    MacroSource macro;
    macro.setValue(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(macro.value(), 0.0f);
}

TEST(AudioModulatorsStandaloneTest, ConstantSourcePreservesFiniteUnboundedOffset)
{
    ConstantSource constant(-3.5f);
    EXPECT_FLOAT_EQ(constant.process(123), -3.5f);
    constant.setValue(2.25f);
    EXPECT_FLOAT_EQ(constant.value(), 2.25f);
    EXPECT_FALSE(constant.bipolar());
}

TEST(AudioModulatorsStandaloneTest, ConstantSourceSanitizesNonFiniteValues)
{
    ConstantSource constant;
    constant.setValue(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(constant.process(0), 0.0f);
}

TEST(AudioModulatorsStandaloneTest, RandomSourceResetReplaysTheSeededSequence)
{
    RandomSource random;
    random.setSeed(42u);
    random.setSampleRate(100.0f);
    random.setRate(10.0f);
    random.reset();
    const float first = random.process(10);
    random.process(30);
    random.reset();
    EXPECT_FLOAT_EQ(random.process(10), first);
}

TEST(AudioModulatorsStandaloneTest, RandomSourceProducesFiniteBipolarAndUnipolarValues)
{
    RandomSource random;
    random.setSeed(9u);
    random.setRate(0.0f);
    random.reset();
    const float bipolar = random.process(1);
    EXPECT_TRUE(std::isfinite(bipolar));
    EXPECT_GE(bipolar, -1.0f);
    EXPECT_LE(bipolar, 1.0f);

    random.setUnipolar(true);
    random.reset();
    const float unipolar = random.process(1);
    EXPECT_FALSE(random.bipolar());
    EXPECT_GE(unipolar, 0.0f);
    EXPECT_LE(unipolar, 1.0f);
}

TEST(AudioModulatorsStandaloneTest, RandomSourceSanitizesRateSmoothingAndZeroSeed)
{
    RandomSource random;
    random.setRate(5000.0f);
    EXPECT_FLOAT_EQ(random.rate(), 1000.0f);
    random.setRate(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(random.rate(), 0.0f);
    random.setSmoothing(-1.0f);
    EXPECT_FLOAT_EQ(random.smoothing(), 0.0f);
    random.setSeed(0u);
    EXPECT_NE(random.seed(), 0u);
}

TEST(AudioModulatorsStandaloneTest, NoiseSourceReplaysSeededValuesAfterReset)
{
    NoiseSource noise;
    noise.setSeed(123u);
    noise.setRate(1.0f);
    noise.setSampleRate(100.0f);
    noise.reset();
    const float first = noise.process(25);
    const float second = noise.process(25);
    noise.reset();
    EXPECT_FLOAT_EQ(noise.process(25), first);
    EXPECT_FLOAT_EQ(noise.process(25), second);
}

TEST(AudioModulatorsStandaloneTest, NoiseSourceInterpolatesWithinBipolarAndUnipolarBounds)
{
    NoiseSource noise;
    noise.setSeed(321u);
    noise.setRate(2.0f);
    noise.setSampleRate(100.0f);
    noise.reset();
    const float bipolar = noise.process(25);
    EXPECT_GE(bipolar, -1.0f);
    EXPECT_LE(bipolar, 1.0f);
    noise.setUnipolar(true);
    noise.reset();
    const float unipolar = noise.process(25);
    EXPECT_FALSE(noise.bipolar());
    EXPECT_GE(unipolar, 0.0f);
    EXPECT_LE(unipolar, 1.0f);
}

TEST(AudioModulatorsStandaloneTest, StepsSourceIsDeterministicForSeedAndPhase)
{
    StepsSource first;
    StepsSource second;
    for (auto* source : {&first, &second}) {
        source->setSeed(777u);
        source->setStepCount(4u);
        source->setRate(2.0f);
        source->setSampleRate(100.0f);
        source->reset();
    }
    EXPECT_FLOAT_EQ(first.process(25), second.process(25));
    EXPECT_FLOAT_EQ(first.process(25), second.process(25));
    first.reset();
    EXPECT_FLOAT_EQ(first.process(25), second.process(25));
}

TEST(AudioModulatorsStandaloneTest, StepsSourceClampsStepCountRateAndSeed)
{
    StepsSource steps;
    steps.setStepCount(0u);
    EXPECT_EQ(steps.stepCount(), 1u);
    steps.setStepCount(100u);
    EXPECT_EQ(steps.stepCount(), 32u);
    steps.setRate(5000.0f);
    EXPECT_FLOAT_EQ(steps.rate(), 1000.0f);
    steps.setRate(-1.0f);
    EXPECT_FLOAT_EQ(steps.rate(), 0.0f);
    steps.setSeed(0u);
    EXPECT_NE(steps.seed(), 0u);
}

TEST(AudioModulatorsStandaloneTest, StepsSourceOutputsStayInSelectedPolarityRange)
{
    StepsSource steps;
    steps.setSeed(88u);
    steps.setRate(0.0f);
    steps.reset();
    const float bipolar = steps.process(100);
    EXPECT_GE(bipolar, -1.0f);
    EXPECT_LE(bipolar, 1.0f);
    steps.setUnipolar(true);
    const float unipolar = steps.process(0);
    EXPECT_FALSE(steps.bipolar());
    EXPECT_GE(unipolar, 0.0f);
    EXPECT_LE(unipolar, 1.0f);
}
