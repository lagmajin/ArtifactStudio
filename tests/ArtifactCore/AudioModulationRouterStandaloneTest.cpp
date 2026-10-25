#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>

import Audio.Modulation.Modulator;
import Audio.Modulation.Router;

using namespace ArtifactCore::Audio::Modulation;

TEST(AudioModulationRouterStandaloneTest, PropertyPathHashIsStableAndDistinguishesPaths)
{
    EXPECT_EQ(modulationTargetId("transform.opacity"),
              modulationTargetId("transform.opacity"));
    EXPECT_NE(modulationTargetId("transform.opacity"),
              modulationTargetId("transform.scale"));
    EXPECT_NE(modulationTargetId(""), 0u);
}

TEST(AudioModulationRouterStandaloneTest, PropertyAssignmentKeepsPathAndDerivedId)
{
    const auto assignment = ModulationAssignment::forPropertyPath(7u, "layer.opacity", 0.25f);
    EXPECT_EQ(assignment.sourceId, 7u);
    EXPECT_EQ(assignment.targetPath, "layer.opacity");
    EXPECT_EQ(assignment.targetId, modulationTargetId("layer.opacity"));
    EXPECT_FLOAT_EQ(assignment.depth, 0.25f);
}

TEST(AudioModulationRouterStandaloneTest, EmptyRouterRejectsNullSourceAndUnknownAssignments)
{
    ModulationRouter router;
    EXPECT_TRUE(router.empty());
    EXPECT_EQ(router.addSource(std::unique_ptr<IModulatorSource>{}), 0u);
    EXPECT_FALSE(router.addAssignment(
        ModulationAssignment::forPropertyPath(12u, "opacity")));
}

TEST(AudioModulationRouterStandaloneTest, AddsSourcesAndProvidesMutableLookup)
{
    ModulationRouter router;
    const auto first = router.addSource(std::make_unique<ConstantSource>(1.0f));
    const auto second = router.addSource(std::make_unique<ConstantSource>(2.0f));
    ASSERT_NE(first, 0u);
    EXPECT_EQ(second, first + 1u);
    ASSERT_NE(router.source(first), nullptr);
    EXPECT_FLOAT_EQ(router.source(first)->process(0), 1.0f);
    EXPECT_EQ(router.source(999u), nullptr);
}

TEST(AudioModulationRouterStandaloneTest, RejectsAssignmentsWithZeroSourceOrTargetId)
{
    ModulationRouter router;
    const auto source = router.addSource(std::make_unique<ConstantSource>(1.0f));
    auto assignment = ModulationAssignment::forPropertyPath(source, "opacity");
    assignment.sourceId = 0;
    EXPECT_FALSE(router.addAssignment(assignment));
    assignment.sourceId = source;
    assignment.targetId = 0;
    EXPECT_FALSE(router.addAssignment(assignment));
}

TEST(AudioModulationRouterStandaloneTest, AddModeSumsSourceContributions)
{
    ModulationRouter router;
    router.setSmoothingTime(0.0f);
    const auto a = router.addSource(std::make_unique<ConstantSource>(0.25f));
    const auto b = router.addSource(std::make_unique<ConstantSource>(0.5f));
    const auto target = modulationTargetId("effect.amount");
    ASSERT_TRUE(router.addAssignment({a, target, "effect.amount", 2.0f, true,
                                      ModulationMixMode::Add}));
    ASSERT_TRUE(router.addAssignment({b, target, "effect.amount", 0.5f, true,
                                      ModulationMixMode::Add}));
    router.process(1);
    EXPECT_FLOAT_EQ(router.targetModulation(target), 0.75f);
    EXPECT_FLOAT_EQ(router.targetValue(target, 0.2f), 0.95f);
}

TEST(AudioModulationRouterStandaloneTest, MultiplyModeScalesTheAdditiveBaseResult)
{
    ModulationRouter router;
    router.setSmoothingTime(0.0f);
    const auto source = router.addSource(std::make_unique<ConstantSource>(0.5f));
    const auto target = modulationTargetId("effect.blur");
    ASSERT_TRUE(router.addAssignment({source, target, "effect.blur", 0.5f, true,
                                      ModulationMixMode::Multiply}));
    router.process(1);
    EXPECT_FLOAT_EQ(router.targetValue(target, 8.0f), 10.0f);
}

TEST(AudioModulationRouterStandaloneTest, AddAndMultiplyAssignmentsCoexistForOneTarget)
{
    ModulationRouter router;
    router.setSmoothingTime(0.0f);
    const auto source = router.addSource(std::make_unique<ConstantSource>(0.5f));
    const auto target = modulationTargetId("effect.amount");
    ASSERT_TRUE(router.addAssignment({source, target, "effect.amount", 2.0f, true,
                                      ModulationMixMode::Add}));
    ASSERT_TRUE(router.addAssignment({source, target, "effect.amount", 1.0f, true,
                                      ModulationMixMode::Multiply}));

    router.process(1);

    ASSERT_EQ(router.assignments().size(), 2u);
    EXPECT_FLOAT_EQ(router.targetModulation(target), 1.0f);
    EXPECT_FLOAT_EQ(router.targetValue(target, 2.0f), 4.5f);
}

TEST(AudioModulationRouterStandaloneTest, DisabledAssignmentsDoNotContribute)
{
    ModulationRouter router;
    router.setSmoothingTime(0.0f);
    const auto source = router.addSource(std::make_unique<ConstantSource>(1.0f));
    const auto target = modulationTargetId("opacity");
    ASSERT_TRUE(router.addAssignment({source, target, "opacity", 1.0f, false,
                                      ModulationMixMode::Add}));
    router.process(1);
    EXPECT_FLOAT_EQ(router.targetModulation(target), 0.0f);
    EXPECT_FLOAT_EQ(router.targetValue(target, 0.6f), 0.6f);
}

TEST(AudioModulationRouterStandaloneTest, ReaddingSameModeUpdatesExistingAssignment)
{
    ModulationRouter router;
    const auto source = router.addSource(std::make_unique<ConstantSource>(1.0f));
    auto assignment = ModulationAssignment::forPropertyPath(source, "opacity", 0.1f);
    ASSERT_TRUE(router.addAssignment(assignment));
    assignment.targetPath = "renamed.opacity";
    assignment.depth = 0.25f;
    ASSERT_TRUE(router.addAssignment(assignment));
    const auto assignments = router.assignments();
    ASSERT_EQ(assignments.size(), 1u);
    EXPECT_EQ(assignments[0].targetPath, "renamed.opacity");
    EXPECT_FLOAT_EQ(assignments[0].depth, 0.25f);
}

TEST(AudioModulationRouterStandaloneTest, NonFiniteDepthIsSanitizedToZero)
{
    ModulationRouter router;
    router.setSmoothingTime(0.0f);
    const auto source = router.addSource(std::make_unique<ConstantSource>(1.0f));
    const auto target = modulationTargetId("opacity");
    auto assignment = ModulationAssignment::forPropertyPath(
        source, "opacity", std::numeric_limits<float>::infinity());
    ASSERT_TRUE(router.addAssignment(assignment));
    router.process(1);
    EXPECT_FLOAT_EQ(router.targetModulation(target), 0.0f);
    EXPECT_TRUE(router.setDepth(source, target, std::numeric_limits<float>::quiet_NaN()));
    router.process(1);
    EXPECT_FLOAT_EQ(router.targetModulation(target), 0.0f);
}

TEST(AudioModulationRouterStandaloneTest, AssignmentControlsCanBeChangedAndRemoved)
{
    ModulationRouter router;
    router.setSmoothingTime(0.0f);
    const auto source = router.addSource(std::make_unique<ConstantSource>(1.0f));
    const auto target = modulationTargetId("opacity");
    ASSERT_TRUE(router.addAssignment({source, target, "opacity", 1.0f, true,
                                      ModulationMixMode::Add}));
    EXPECT_TRUE(router.setEnabled(source, target, false));
    router.process(1);
    EXPECT_FLOAT_EQ(router.targetModulation(target), 0.0f);
    EXPECT_FALSE(router.setEnabled(999u, target, true));
    EXPECT_TRUE(router.setEnabled(source, target, true));
    router.process(1);
    EXPECT_FLOAT_EQ(router.targetModulation(target), 1.0f);
    router.removeAssignment(source, target);
    EXPECT_TRUE(router.empty());
    router.process(1);
    EXPECT_FALSE(router.hasTarget(target));
    EXPECT_FLOAT_EQ(router.targetModulation(target), 0.0f);
    EXPECT_FLOAT_EQ(router.targetValue(target, 0.6f), 0.6f);
}

TEST(AudioModulationRouterStandaloneTest, RemovingSourceAlsoRemovesItsAssignments)
{
    ModulationRouter router;
    const auto source = router.addSource(std::make_unique<ConstantSource>(1.0f));
    const auto target = modulationTargetId("opacity");
    ASSERT_TRUE(router.addAssignment({source, target, "opacity", 1.0f, true,
                                      ModulationMixMode::Add}));
    EXPECT_TRUE(router.removeSource(source));
    EXPECT_FALSE(router.removeSource(source));
    EXPECT_TRUE(router.empty());
    EXPECT_FALSE(router.hasTarget(target));
}

TEST(AudioModulationRouterStandaloneTest, RemovingAssignmentsByTargetAndClearAreScoped)
{
    ModulationRouter router;
    const auto source = router.addSource(std::make_unique<ConstantSource>(1.0f));
    const auto opacity = modulationTargetId("opacity");
    const auto scale = modulationTargetId("scale");
    ASSERT_TRUE(router.addAssignment({source, opacity, "opacity", 1.0f, true,
                                      ModulationMixMode::Add}));
    ASSERT_TRUE(router.addAssignment({source, scale, "scale", 1.0f, true,
                                      ModulationMixMode::Add}));
    router.removeAssignmentsForTarget(opacity);
    router.process(1);
    EXPECT_FALSE(router.hasTarget(opacity));
    EXPECT_TRUE(router.hasTarget(scale));
    router.clearAssignments();
    EXPECT_TRUE(router.empty());
}

TEST(AudioModulationRouterStandaloneTest, SmoothingTimeControlsContributionRamp)
{
    ModulationRouter router;
    router.setSampleRate(1.0f);
    router.setSmoothingTime(1.0f);
    const auto source = router.addSource(std::make_unique<ConstantSource>(1.0f));
    const auto target = modulationTargetId("opacity");
    ASSERT_TRUE(router.addAssignment({source, target, "opacity", 1.0f, true,
                                      ModulationMixMode::Add}));
    router.process(1);
    EXPECT_NEAR(router.targetModulation(target), 1.0f - std::exp(-1.0f), 1e-6f);
    router.setSmoothingTime(-1.0f);
    EXPECT_FLOAT_EQ(router.smoothingTime(), 0.0f);
}

TEST(AudioModulationRouterStandaloneTest, FrameClockIsIdempotentAndReplaysAfterBackwardSeek)
{
    ModulationRouter router;
    router.setSmoothingTime(0.0f);
    auto lfo = std::make_unique<LfoSource>();
    lfo->setFrequency(1.0f);
    const auto source = router.addSource(std::move(lfo));
    const auto target = modulationTargetId("rotation");
    ASSERT_TRUE(router.addAssignment({source, target, "rotation", 1.0f, true,
                                      ModulationMixMode::Add}));
    router.processAtFrame(2, 4.0f);
    const float atTwo = router.targetModulation(target);
    router.processAtFrame(2, 4.0f);
    EXPECT_FLOAT_EQ(router.targetModulation(target), atTwo);
    router.processAtFrame(1, 4.0f);
    router.processAtFrame(2, 4.0f);
    EXPECT_FLOAT_EQ(router.targetModulation(target), atTwo);
}

TEST(AudioModulationRouterStandaloneTest, SnapshotRestoresSourcesAssignmentsAndSmoothing)
{
    ModulationRouter original;
    original.setSmoothingTime(0.0f);
    const auto source = original.addSource(std::make_unique<ConstantSource>(0.5f));
    const auto target = modulationTargetId("opacity");
    ASSERT_TRUE(original.addAssignment({source, target, "opacity", 0.5f, true,
                                       ModulationMixMode::Add}));
    const auto snapshot = original.snapshot();

    ModulationRouter restored;
    restored.restoreSnapshot(snapshot);
    EXPECT_FLOAT_EQ(restored.smoothingTime(), 0.0f);
    ASSERT_EQ(restored.sourceDefinitions().size(), 1u);
    ASSERT_EQ(restored.assignments().size(), 1u);
    restored.process(1);
    EXPECT_FLOAT_EQ(restored.targetModulation(target), 0.25f);
}

TEST(AudioModulationRouterStandaloneTest, SnapshotPreservesAssignmentModesAndEnabledState)
{
    ModulationRouter original;
    original.setSmoothingTime(0.0f);
    const auto source = original.addSource(std::make_unique<ConstantSource>(0.5f));
    const auto target = modulationTargetId("effect.amount");
    ASSERT_TRUE(original.addAssignment({source, target, "effect.amount", 2.0f, false,
                                        ModulationMixMode::Add}));
    ASSERT_TRUE(original.addAssignment({source, target, "effect.amount", 0.5f, true,
                                        ModulationMixMode::Multiply}));

    ModulationRouter restored;
    restored.restoreSnapshot(original.snapshot());

    const auto assignments = restored.assignments();
    ASSERT_EQ(assignments.size(), 2u);
    EXPECT_EQ(assignments[0].mode, ModulationMixMode::Add);
    EXPECT_FALSE(assignments[0].enabled);
    EXPECT_EQ(assignments[1].mode, ModulationMixMode::Multiply);
    EXPECT_TRUE(assignments[1].enabled);
    restored.process(1);
    EXPECT_FLOAT_EQ(restored.targetModulation(target), 0.0f);
    EXPECT_FLOAT_EQ(restored.targetValue(target, 4.0f), 5.0f);
}

TEST(AudioModulationRouterStandaloneTest, SnapshotRestoresLfoConfigurationAndDeterministicOutput)
{
    ModulationRouter original;
    original.setSmoothingTime(0.0f);
    auto lfo = std::make_unique<LfoSource>(LfoWaveform::Square);
    lfo->setFrequency(1.0f);
    lfo->setPhase(0.25f);
    lfo->setPulseWidth(0.4f);
    lfo->setUnipolar(true);
    const auto source = original.addSource(std::move(lfo));
    const auto target = modulationTargetId("effect.amount");
    ASSERT_TRUE(original.addAssignment({source, target, "effect.amount", 0.5f, true,
                                      ModulationMixMode::Add}));

    ModulationRouter restored;
    restored.restoreSnapshot(original.snapshot());

    const auto definitions = restored.sourceDefinitions();
    ASSERT_EQ(definitions.size(), 1u);
    EXPECT_EQ(definitions[0].id, source);
    EXPECT_EQ(definitions[0].type, ModulatorSourceType::Lfo);
    EXPECT_EQ(definitions[0].waveform, LfoWaveform::Square);
    EXPECT_FLOAT_EQ(definitions[0].frequency, 1.0f);
    EXPECT_FLOAT_EQ(definitions[0].phaseOffset, 0.25f);
    EXPECT_FLOAT_EQ(definitions[0].pulseWidth, 0.4f);
    EXPECT_TRUE(definitions[0].unipolar);

    restored.process(0);
    EXPECT_FLOAT_EQ(restored.targetModulation(target), 0.5f);
}

TEST(AudioModulationRouterStandaloneTest, SnapshotRestoresAdsrParametersButResetsGateState)
{
    ModulationRouter original;
    original.setSmoothingTime(0.0f);
    auto adsr = std::make_unique<AdsrSource>();
    adsr->setAttack(0.2f);
    adsr->setDecay(0.4f);
    adsr->setSustain(0.3f);
    adsr->setRelease(0.6f);
    const auto source = original.addSource(std::move(adsr));
    const auto target = modulationTargetId("effect.amount");
    ASSERT_TRUE(original.addAssignment({source, target, "effect.amount", 1.0f, true,
                                      ModulationMixMode::Add}));
    auto* originalAdsr = dynamic_cast<AdsrSource*>(original.source(source));
    ASSERT_NE(originalAdsr, nullptr);
    originalAdsr->gate(true);
    original.process(2400);

    ModulationRouter restored;
    restored.restoreSnapshot(original.snapshot());

    const auto definitions = restored.sourceDefinitions();
    ASSERT_EQ(definitions.size(), 1u);
    EXPECT_EQ(definitions[0].type, ModulatorSourceType::Adsr);
    EXPECT_FLOAT_EQ(definitions[0].attack, 0.2f);
    EXPECT_FLOAT_EQ(definitions[0].decay, 0.4f);
    EXPECT_FLOAT_EQ(definitions[0].sustain, 0.3f);
    EXPECT_FLOAT_EQ(definitions[0].release, 0.6f);
    auto* restoredAdsr = dynamic_cast<AdsrSource*>(restored.source(source));
    ASSERT_NE(restoredAdsr, nullptr);
    EXPECT_EQ(restoredAdsr->stage(), AdsrSource::Stage::Idle);

    restoredAdsr->gate(true);
    restored.process(2400);
    EXPECT_NEAR(restored.targetModulation(target), 0.25f, 1.0e-6f);
}

TEST(AudioModulationRouterStandaloneTest, SnapshotRestoresRandomConfigurationAndSeededSequence)
{
    ModulationRouter original;
    original.setSmoothingTime(0.0f);
    auto random = std::make_unique<RandomSource>();
    random->setRate(7.0f);
    random->setSmoothing(0.0f);
    random->setSeed(1234u);
    random->setUnipolar(true);
    const auto source = original.addSource(std::move(random));
    const auto target = modulationTargetId("effect.amount");
    ASSERT_TRUE(original.addAssignment({source, target, "effect.amount", 1.0f, true,
                                      ModulationMixMode::Add}));
    const auto snapshot = original.snapshot();

    ModulationRouter firstRestore;
    ModulationRouter secondRestore;
    firstRestore.restoreSnapshot(snapshot);
    secondRestore.restoreSnapshot(snapshot);

    const auto definitions = firstRestore.sourceDefinitions();
    ASSERT_EQ(definitions.size(), 1u);
    EXPECT_EQ(definitions[0].type, ModulatorSourceType::Random);
    EXPECT_FLOAT_EQ(definitions[0].rate, 7.0f);
    EXPECT_FLOAT_EQ(definitions[0].smoothing, 0.0f);
    EXPECT_EQ(definitions[0].seed, 1234u);
    EXPECT_TRUE(definitions[0].unipolar);

    firstRestore.process(1);
    secondRestore.process(1);
    const float firstValue = firstRestore.targetModulation(target);
    EXPECT_FLOAT_EQ(firstValue, secondRestore.targetModulation(target));
    EXPECT_GE(firstValue, 0.0f);
    EXPECT_LE(firstValue, 1.0f);
}

TEST(AudioModulationRouterStandaloneTest, SnapshotRestoresNoiseConfigurationAndSeededSequence)
{
    ModulationRouter original;
    original.setSmoothingTime(0.0f);
    auto noise = std::make_unique<NoiseSource>();
    noise->setRate(5.0f);
    noise->setSeed(9876u);
    noise->setUnipolar(true);
    const auto source = original.addSource(std::move(noise));
    const auto target = modulationTargetId("effect.amount");
    ASSERT_TRUE(original.addAssignment({source, target, "effect.amount", 1.0f, true,
                                      ModulationMixMode::Add}));
    const auto snapshot = original.snapshot();

    ModulationRouter firstRestore;
    ModulationRouter secondRestore;
    firstRestore.restoreSnapshot(snapshot);
    secondRestore.restoreSnapshot(snapshot);

    const auto definitions = firstRestore.sourceDefinitions();
    ASSERT_EQ(definitions.size(), 1u);
    EXPECT_EQ(definitions[0].type, ModulatorSourceType::Noise);
    EXPECT_FLOAT_EQ(definitions[0].rate, 5.0f);
    EXPECT_EQ(definitions[0].seed, 9876u);
    EXPECT_TRUE(definitions[0].unipolar);

    firstRestore.process(10000);
    secondRestore.process(10000);
    const float firstValue = firstRestore.targetModulation(target);
    EXPECT_FLOAT_EQ(firstValue, secondRestore.targetModulation(target));
    EXPECT_GE(firstValue, 0.0f);
    EXPECT_LE(firstValue, 1.0f);
}

TEST(AudioModulationRouterStandaloneTest, SnapshotRestoresStepsConfigurationAndSeededSequence)
{
    ModulationRouter original;
    original.setSmoothingTime(0.0f);
    auto steps = std::make_unique<StepsSource>();
    steps->setStepCount(7u);
    steps->setRate(12.0f);
    steps->setSeed(4321u);
    steps->setUnipolar(true);
    const auto source = original.addSource(std::move(steps));
    const auto target = modulationTargetId("effect.amount");
    ASSERT_TRUE(original.addAssignment({source, target, "effect.amount", 1.0f, true,
                                      ModulationMixMode::Add}));
    const auto snapshot = original.snapshot();

    ModulationRouter firstRestore;
    ModulationRouter secondRestore;
    firstRestore.restoreSnapshot(snapshot);
    secondRestore.restoreSnapshot(snapshot);

    const auto definitions = firstRestore.sourceDefinitions();
    ASSERT_EQ(definitions.size(), 1u);
    EXPECT_EQ(definitions[0].type, ModulatorSourceType::Steps);
    EXPECT_EQ(definitions[0].stepCount, 7u);
    EXPECT_FLOAT_EQ(definitions[0].rate, 12.0f);
    EXPECT_EQ(definitions[0].seed, 4321u);
    EXPECT_TRUE(definitions[0].unipolar);

    for (const std::uint32_t frames : {0u, 4000u, 4000u, 8000u}) {
        firstRestore.process(frames);
        secondRestore.process(frames);
        const float value = firstRestore.targetModulation(target);
        EXPECT_FLOAT_EQ(value, secondRestore.targetModulation(target));
        EXPECT_GE(value, 0.0f);
        EXPECT_LE(value, 1.0f);
    }
}

TEST(AudioModulationRouterStandaloneTest, TargetValueReturnsBaseForUnknownTargets)
{
    ModulationRouter router;
    EXPECT_FALSE(router.hasTarget(99u));
    EXPECT_FLOAT_EQ(router.targetModulation(99u), 0.0f);
    EXPECT_FLOAT_EQ(router.targetValue(99u, 12.5f), 12.5f);
}
