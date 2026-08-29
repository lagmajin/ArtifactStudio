#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <memory>

import Audio.Modulation.Router;
import Audio.Modulation.Modulator;

using namespace ArtifactCore::Audio::Modulation;

namespace {
class ConstantSource final : public IModulatorSource {
public:
    explicit ConstantSource(float value) : value_(value) {}
    void setSampleRate(float) override {}
    void reset() override {}
    float process(std::uint32_t) override { return value_; }
    bool bipolar() const override { return true; }
private:
    float value_;
};
}

TEST(AudioModulationRouterTest, PropertyPathMappingAddsToBaseValue) {
    ModulationRouter router;
    router.setSmoothingTime(0.0f);
    const auto sourceId = router.addSource(std::make_unique<ConstantSource>(0.5f));
    const auto mapping = ModulationAssignment::forPropertyPath(
        sourceId, "transform.rotation", 20.0f);

    ASSERT_TRUE(router.addAssignment(mapping));
    router.process(64);

    EXPECT_EQ(router.assignments().size(), 1u);
    EXPECT_TRUE(router.hasTarget(mapping.targetId));
    EXPECT_FLOAT_EQ(router.targetValue(mapping.targetId, 10.0f), 20.0f);
}

TEST(AudioModulationRouterTest, MultiplyMappingsPreserveAnimatedBaseValue) {
    ModulationRouter router;
    router.setSmoothingTime(0.0f);
    const auto sourceId = router.addSource(std::make_unique<ConstantSource>(0.5f));
    const auto mapping = ModulationAssignment::forPropertyPath(
        sourceId, "effect.blur.amount", 0.5f, ModulationMixMode::Multiply);

    ASSERT_TRUE(router.addAssignment(mapping));
    router.process(64);

    // A 0.5 source at 0.5 depth produces a 1.25 multiplier.
    EXPECT_FLOAT_EQ(router.targetValue(mapping.targetId, 8.0f), 10.0f);
}

TEST(AudioModulationRouterTest, ReaddingSameMappingUpdatesInsteadOfDuplicating) {
    ModulationRouter router;
    const auto sourceId = router.addSource(std::make_unique<ConstantSource>(1.0f));
    auto mapping = ModulationAssignment::forPropertyPath(sourceId, "opacity", 0.1f);
    ASSERT_TRUE(router.addAssignment(mapping));
    mapping.depth = 0.25f;
    ASSERT_TRUE(router.addAssignment(mapping));

    const auto mappings = router.assignments();
    ASSERT_EQ(mappings.size(), 1u);
    EXPECT_FLOAT_EQ(mappings.front().depth, 0.25f);
}

TEST(AudioModulationRouterTest, FrameClockIsIdempotentAndSeeksDeterministically) {
    ModulationRouter router;
    router.setSmoothingTime(0.0f);
    auto lfo = std::make_unique<LfoSource>();
    lfo->setFrequency(1.0f);
    const auto sourceId = router.addSource(std::move(lfo));
    const auto mapping = ModulationAssignment::forPropertyPath(sourceId, "transform.rotation");
    ASSERT_TRUE(router.addAssignment(mapping));

    router.processAtFrame(0, 4.0f);
    EXPECT_NEAR(router.targetModulation(mapping.targetId), 0.0f, 0.0001f);
    router.processAtFrame(0, 4.0f);
    EXPECT_NEAR(router.targetModulation(mapping.targetId), 0.0f, 0.0001f);
    router.processAtFrame(1, 4.0f);
    EXPECT_NEAR(router.targetModulation(mapping.targetId), 1.0f, 0.0001f);
    router.processAtFrame(0, 4.0f);
    router.processAtFrame(1, 4.0f);
    EXPECT_NEAR(router.targetModulation(mapping.targetId), 1.0f, 0.0001f);
}

TEST(AudioModulationRouterTest, RandomSourceReplaysAfterBackwardSeek) {
    ModulationRouter router;
    router.setSmoothingTime(0.0f);
    auto random = std::make_unique<RandomSource>();
    random->setSeed(42u);
    random->setRate(1.0f);
    const auto sourceId = router.addSource(std::move(random));
    const auto mapping = ModulationAssignment::forPropertyPath(sourceId, "opacity");
    ASSERT_TRUE(router.addAssignment(mapping));

    router.processAtFrame(4, 4.0f);
    const float firstPass = router.targetModulation(mapping.targetId);
    router.processAtFrame(1, 4.0f);
    router.processAtFrame(4, 4.0f);
    EXPECT_FLOAT_EQ(router.targetModulation(mapping.targetId), firstPass);
}

TEST(AudioModulationRouterTest, SourceDefinitionsRestoreSourceIdsAndSettings) {
    ModulationRouter router;
    auto lfo = std::make_unique<LfoSource>(LfoWaveform::Square);
    lfo->setFrequency(3.5f);
    lfo->setPhase(0.25f);
    lfo->setPulseWidth(0.3f);
    lfo->setUnipolar(true);
    const auto lfoId = router.addSource(std::move(lfo));

    auto random = std::make_unique<RandomSource>();
    random->setRate(7.0f);
    random->setSmoothing(0.15f);
    random->setSeed(1234u);
    const auto randomId = router.addSource(std::move(random));
    const auto mapping = ModulationAssignment::forPropertyPath(
        randomId, "layer.example.layer.opacity", 0.25f);
    ASSERT_TRUE(router.addAssignment(mapping));

    const auto definitions = router.sourceDefinitions();
    ASSERT_EQ(definitions.size(), 2u);

    ModulationRouter restored;
    restored.restoreSources(definitions);
    ASSERT_TRUE(restored.addAssignment(mapping));

    const auto restoredDefinitions = restored.sourceDefinitions();
    ASSERT_EQ(restoredDefinitions.size(), 2u);
    EXPECT_EQ(restoredDefinitions[0].id, lfoId);
    EXPECT_EQ(restoredDefinitions[0].type, ModulatorSourceType::Lfo);
    EXPECT_EQ(restoredDefinitions[0].waveform, LfoWaveform::Square);
    EXPECT_FLOAT_EQ(restoredDefinitions[0].frequency, 3.5f);
    EXPECT_FLOAT_EQ(restoredDefinitions[0].phaseOffset, 0.25f);
    EXPECT_TRUE(restoredDefinitions[0].unipolar);
    EXPECT_EQ(restoredDefinitions[1].id, randomId);
    EXPECT_EQ(restoredDefinitions[1].type, ModulatorSourceType::Random);
    EXPECT_EQ(restoredDefinitions[1].seed, 1234u);
    EXPECT_FLOAT_EQ(restoredDefinitions[1].rate, 7.0f);
    EXPECT_EQ(restored.assignments().size(), 1u);
}

TEST(AudioModulationRouterTest, MappingRejectsMissingSource) {
    ModulationRouter router;
    EXPECT_FALSE(router.addAssignment(ModulationAssignment::forPropertyPath(
        99u, "layer.example.layer.opacity", 1.0f)));
    EXPECT_TRUE(router.assignments().empty());
}

TEST(AudioModulationRouterTest, MacroSourceMapsAndRestoresItsValue) {
    ModulationRouter router;
    router.setSmoothingTime(0.0f);
    auto macro = std::make_unique<MacroSource>();
    macro->setValue(0.75f);
    const auto sourceId = router.addSource(std::move(macro));
    const auto assignment = ModulationAssignment::forPropertyPath(
        sourceId, "effect.example.amount", 4.0f);
    ASSERT_TRUE(router.addAssignment(assignment));
    router.process(1);
    EXPECT_FLOAT_EQ(router.targetValue(assignment.targetId, 1.0f), 4.0f);

    const auto definitions = router.sourceDefinitions();
    ASSERT_EQ(definitions.size(), 1u);
    EXPECT_EQ(definitions.front().type, ModulatorSourceType::Macro);
    EXPECT_FLOAT_EQ(definitions.front().macroValue, 0.75f);

    ModulationRouter restored;
    restored.restoreSources(definitions);
    const auto restoredDefinitions = restored.sourceDefinitions();
    ASSERT_EQ(restoredDefinitions.size(), 1u);
    EXPECT_EQ(restoredDefinitions.front().type, ModulatorSourceType::Macro);
    EXPECT_FLOAT_EQ(restoredDefinitions.front().macroValue, 0.75f);
}

TEST(AudioModulationRouterTest, SourceIdWrapSkipsReservedZero) {
    ModulationRouter router;
    ModulationSourceDefinition maximum;
    maximum.id = std::numeric_limits<std::uint32_t>::max();
    maximum.type = ModulatorSourceType::Macro;
    router.restoreSources({maximum});

    const auto nextId = router.addSource(std::make_unique<MacroSource>());
    EXPECT_EQ(nextId, 1u);
    EXPECT_NE(nextId, 0u);
}

TEST(AudioModulationRouterTest, SnapshotReplacesPriorRouterState) {
    ModulationRouter source;
    source.setSmoothingTime(0.1f);
    auto random = std::make_unique<RandomSource>();
    random->setSeed(99u);
    const auto sourceId = source.addSource(std::move(random));
    const auto assignment = ModulationAssignment::forPropertyPath(
        sourceId, "effect.example.amount", 0.5f);
    ASSERT_TRUE(source.addAssignment(assignment));

    ModulationRouter target;
    const auto staleId = target.addSource(std::make_unique<LfoSource>());
    ASSERT_TRUE(target.addAssignment(ModulationAssignment::forPropertyPath(
        staleId, "layer.stale.layer.opacity", 1.0f)));
    target.restoreSnapshot(source.snapshot());

    const auto definitions = target.sourceDefinitions();
    ASSERT_EQ(definitions.size(), 1u);
    EXPECT_EQ(definitions.front().id, sourceId);
    EXPECT_EQ(definitions.front().seed, 99u);
    const auto assignments = target.assignments();
    ASSERT_EQ(assignments.size(), 1u);
    EXPECT_EQ(assignments.front().targetPath, "effect.example.amount");
    EXPECT_FLOAT_EQ(target.smoothingTime(), 0.1f);
}
