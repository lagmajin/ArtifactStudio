#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <utility>
#include <vector>

#include <QVector2D>
#include <QString>

import Physics.System;
import Physics.SoftBody;
import Physics.Fluid;
import Physics.Mpm2D;
import Physics.Cloth3D;
import Physics2D;
import Utils.Id;

using namespace ArtifactCore;

namespace {

class PhysicsLodSettingsScope {
public:
    PhysicsLodSettingsScope()
        : system_(PhysicsSystem::instance())
        , previous_(system_.physicsLODSettings())
    {
    }

    ~PhysicsLodSettingsScope()
    {
        system_.setPhysicsLODSettings(previous_);
    }

    PhysicsSystem& system() const { return system_; }

private:
    PhysicsSystem& system_;
    PhysicsLODSettings previous_;
};

} // namespace

TEST(PhysicsLodSettingsContractTest, FullLevelKeepsExistingDefaultBudgets)
{
    PhysicsLodSettingsScope scope;
    PhysicsLODSettings settings;
    settings.level = PhysicsLODLevel::Full;

    scope.system().setPhysicsLODSettings(settings);
    const PhysicsLODSettings& actual = scope.system().physicsLODSettings();

    EXPECT_EQ(actual.level, PhysicsLODLevel::Full);
    EXPECT_FLOAT_EQ(actual.targetHz, 0.0f);
    EXPECT_EQ(actual.rigidBodySubSteps, 0);
    EXPECT_EQ(actual.softBodyMaxSubSteps, 0);
    EXPECT_EQ(actual.fluidSolverIterations, 0);
    EXPECT_FALSE(actual.applySleepPolicy);
    EXPECT_FALSE(actual.disableContinuousCollision);
}

TEST(PhysicsLodSettingsContractTest,
     UnknownLevelKeepsFullResolutionBudgetsAndPolicies)
{
    PhysicsLodSettingsScope scope;
    PhysicsLODSettings settings;
    settings.level = static_cast<PhysicsLODLevel>(255);

    scope.system().setPhysicsLODSettings(settings);
    const PhysicsLODSettings& actual = scope.system().physicsLODSettings();

    EXPECT_EQ(actual.level, static_cast<PhysicsLODLevel>(255));
    EXPECT_FLOAT_EQ(actual.targetHz, 0.0f);
    EXPECT_EQ(actual.rigidBodySubSteps, 0);
    EXPECT_EQ(actual.softBodyMaxSubSteps, 0);
    EXPECT_EQ(actual.softBodyConstraintIterations, 0);
    EXPECT_EQ(actual.softBodyCollisionIterations, 0);
    EXPECT_FLOAT_EQ(actual.softBodyGridScale, 1.0f);
    EXPECT_EQ(actual.fluidSolverIterations, 0);
    EXPECT_FLOAT_EQ(actual.fluidResolutionScale, 1.0f);
    EXPECT_EQ(actual.materialMaxSubSteps, 0);
    EXPECT_FALSE(actual.disableFracture);
    EXPECT_FLOAT_EQ(actual.fractureShardScale, 1.0f);
    EXPECT_FLOAT_EQ(actual.fractureDebrisScale, 1.0f);
    EXPECT_FALSE(actual.simplifyCollisionMesh);
    EXPECT_FALSE(actual.disableSoftBodySelfCollision);
    EXPECT_FALSE(actual.applySleepPolicy);
    EXPECT_FALSE(actual.disableContinuousCollision);
}

TEST(PhysicsLodSettingsContractTest,
     UnknownLevelUpdatesFluidLikeFullResolutionFallback)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const LayerID layerId;
    auto fluid = system.createFluidSolver(layerId, 12, 12);
    ASSERT_TRUE(fluid);
    FluidSolver2D reference(12, 12);
    fluid->addDensity(6, 6, 1.0f);
    fluid->addVelocity(6, 6, 0.4f, -0.2f);
    reference.addDensity(6, 6, 1.0f);
    reference.addVelocity(6, 6, 0.4f, -0.2f);

    PhysicsLODSettings unknown;
    unknown.level = static_cast<PhysicsLODLevel>(255);
    system.setPhysicsLODSettings(unknown);
    system.update(1.0f / 60.0f, 0.0f, 9.8f, false);
    reference.update(1.0f / 60.0f);

    const FluidSnapshot2D actual = fluid->snapshot();
    const FluidSnapshot2D expected = reference.snapshot();
    ASSERT_EQ(actual.density.size(), expected.density.size());
    ASSERT_EQ(actual.velocityX.size(), expected.velocityX.size());
    ASSERT_EQ(actual.velocityY.size(), expected.velocityY.size());
    EXPECT_EQ(fluid->solverIterations(), reference.solverIterations());
    for (std::size_t index = 0; index < actual.density.size(); ++index) {
        EXPECT_NEAR(actual.density[index], expected.density[index], 1.0e-6f);
        EXPECT_NEAR(actual.velocityX[index], expected.velocityX[index], 1.0e-6f);
        EXPECT_NEAR(actual.velocityY[index], expected.velocityY[index], 1.0e-6f);
    }

    system.unregisterFluidSolver(layerId);
}

TEST(PhysicsLodSettingsContractTest, ReducedLevelFillsUnspecifiedBudgets)
{
    PhysicsLodSettingsScope scope;
    PhysicsLODSettings settings;
    settings.level = PhysicsLODLevel::Reduced;

    scope.system().setPhysicsLODSettings(settings);
    const PhysicsLODSettings& actual = scope.system().physicsLODSettings();

    EXPECT_FLOAT_EQ(actual.targetHz, 30.0f);
    EXPECT_EQ(actual.rigidBodySubSteps, 2);
    EXPECT_EQ(actual.softBodyMaxSubSteps, 4);
    EXPECT_EQ(actual.softBodyConstraintIterations, 3);
    EXPECT_EQ(actual.softBodyCollisionIterations, 1);
    EXPECT_FLOAT_EQ(actual.softBodyGridScale, 0.75f);
    EXPECT_EQ(actual.fluidSolverIterations, 10);
    EXPECT_FLOAT_EQ(actual.fluidResolutionScale, 0.5f);
    EXPECT_EQ(actual.materialMaxSubSteps, 256);
    EXPECT_TRUE(actual.applySleepPolicy);
    EXPECT_FLOAT_EQ(actual.sleepThreshold, 0.5f);
}

TEST(PhysicsLodSettingsContractTest, MinimalLevelAppliesReducedSolverAndCollisionPolicy)
{
    PhysicsLodSettingsScope scope;
    PhysicsLODSettings settings;
    settings.level = PhysicsLODLevel::Minimal;

    scope.system().setPhysicsLODSettings(settings);
    const PhysicsLODSettings& actual = scope.system().physicsLODSettings();

    EXPECT_FLOAT_EQ(actual.targetHz, 15.0f);
    EXPECT_EQ(actual.rigidBodySubSteps, 1);
    EXPECT_EQ(actual.softBodyMaxSubSteps, 2);
    EXPECT_EQ(actual.softBodyConstraintIterations, 1);
    EXPECT_FLOAT_EQ(actual.softBodyGridScale, 0.5f);
    EXPECT_EQ(actual.fluidSolverIterations, 5);
    EXPECT_FLOAT_EQ(actual.fluidResolutionScale, 0.25f);
    EXPECT_EQ(actual.materialMaxSubSteps, 128);
    EXPECT_TRUE(actual.disableSoftBodySelfCollision);
    EXPECT_TRUE(actual.applySleepPolicy);
    EXPECT_FLOAT_EQ(actual.sleepThreshold, 1.0f);
    EXPECT_TRUE(actual.disableContinuousCollision);
    EXPECT_TRUE(actual.simplifyCollisionMesh);
    EXPECT_TRUE(actual.disableFracture);
    EXPECT_FLOAT_EQ(actual.fractureShardScale, 0.5f);
    EXPECT_FLOAT_EQ(actual.fractureDebrisScale, 0.25f);
}

TEST(PhysicsLodSettingsContractTest, ExplicitBudgetsOverrideReducedPresets)
{
    PhysicsLodSettingsScope scope;
    PhysicsLODSettings settings;
    settings.level = PhysicsLODLevel::Reduced;
    settings.targetHz = 24.0f;
    settings.rigidBodySubSteps = 6;
    settings.softBodyGridScale = 0.6f;
    settings.fluidResolutionScale = 0.8f;
    settings.sleepThreshold = 0.25f;

    scope.system().setPhysicsLODSettings(settings);
    const PhysicsLODSettings& actual = scope.system().physicsLODSettings();

    EXPECT_FLOAT_EQ(actual.targetHz, 24.0f);
    EXPECT_EQ(actual.rigidBodySubSteps, 6);
    EXPECT_FLOAT_EQ(actual.softBodyGridScale, 0.6f);
    EXPECT_FLOAT_EQ(actual.fluidResolutionScale, 0.8f);
    EXPECT_FLOAT_EQ(actual.sleepThreshold, 0.25f);
}

TEST(PhysicsLodSettingsContractTest,
     ReturningFromReducedAndMinimalRestoresEveryFullBudgetAndPolicy)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();

    const auto expectFullDefaults = [&] {
        const PhysicsLODSettings& actual = system.physicsLODSettings();
        EXPECT_EQ(actual.level, PhysicsLODLevel::Full);
        EXPECT_FLOAT_EQ(actual.targetHz, 0.0f);
        EXPECT_EQ(actual.rigidBodySubSteps, 0);
        EXPECT_EQ(actual.softBodyMaxSubSteps, 0);
        EXPECT_EQ(actual.softBodyConstraintIterations, 0);
        EXPECT_EQ(actual.softBodyCollisionIterations, 0);
        EXPECT_FLOAT_EQ(actual.softBodyGridScale, 1.0f);
        EXPECT_EQ(actual.fluidSolverIterations, 0);
        EXPECT_FLOAT_EQ(actual.fluidResolutionScale, 1.0f);
        EXPECT_EQ(actual.materialMaxSubSteps, 0);
        EXPECT_FALSE(actual.disableFracture);
        EXPECT_FLOAT_EQ(actual.fractureShardScale, 1.0f);
        EXPECT_FLOAT_EQ(actual.fractureDebrisScale, 1.0f);
        EXPECT_FALSE(actual.simplifyCollisionMesh);
        EXPECT_FALSE(actual.disableSoftBodySelfCollision);
        EXPECT_FALSE(actual.applySleepPolicy);
        EXPECT_FLOAT_EQ(actual.sleepThreshold, 0.0f);
        EXPECT_FALSE(actual.disableContinuousCollision);
    };

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);
    ASSERT_EQ(system.physicsLODSettings().level, PhysicsLODLevel::Reduced);
    system.setPhysicsLODSettings(PhysicsLODSettings{});
    expectFullDefaults();

    PhysicsLODSettings minimal;
    minimal.level = PhysicsLODLevel::Minimal;
    system.setPhysicsLODSettings(minimal);
    ASSERT_EQ(system.physicsLODSettings().level, PhysicsLODLevel::Minimal);
    system.setPhysicsLODSettings(PhysicsLODSettings{});
    expectFullDefaults();
}

TEST(PhysicsLodSettingsContractTest,
     SwitchingBetweenReducedAndMinimalDoesNotLeakPresetPolicies)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);
    ASSERT_EQ(system.physicsLODSettings().level, PhysicsLODLevel::Reduced);
    EXPECT_EQ(system.physicsLODSettings().rigidBodySubSteps, 2);
    EXPECT_EQ(system.physicsLODSettings().fluidSolverIterations, 10);
    EXPECT_FALSE(system.physicsLODSettings().disableContinuousCollision);
    EXPECT_FALSE(system.physicsLODSettings().disableFracture);

    PhysicsLODSettings minimal;
    minimal.level = PhysicsLODLevel::Minimal;
    system.setPhysicsLODSettings(minimal);
    ASSERT_EQ(system.physicsLODSettings().level, PhysicsLODLevel::Minimal);
    EXPECT_EQ(system.physicsLODSettings().rigidBodySubSteps, 1);
    EXPECT_EQ(system.physicsLODSettings().fluidSolverIterations, 5);
    EXPECT_TRUE(system.physicsLODSettings().disableContinuousCollision);
    EXPECT_TRUE(system.physicsLODSettings().disableFracture);
    EXPECT_TRUE(system.physicsLODSettings().simplifyCollisionMesh);

    system.setPhysicsLODSettings(reduced);
    const PhysicsLODSettings& returnedToReduced =
        system.physicsLODSettings();
    EXPECT_EQ(returnedToReduced.level, PhysicsLODLevel::Reduced);
    EXPECT_EQ(returnedToReduced.rigidBodySubSteps, 2);
    EXPECT_EQ(returnedToReduced.fluidSolverIterations, 10);
    EXPECT_FLOAT_EQ(returnedToReduced.softBodyGridScale, 0.75f);
    EXPECT_FALSE(returnedToReduced.disableContinuousCollision);
    EXPECT_FALSE(returnedToReduced.disableFracture);
    EXPECT_FALSE(returnedToReduced.simplifyCollisionMesh);
}

TEST(PhysicsLodSettingsContractTest, FullLevelClampsInvalidManualValues)
{
    PhysicsLodSettingsScope scope;
    PhysicsLODSettings settings;
    settings.level = PhysicsLODLevel::Full;
    settings.targetHz = -10.0f;
    settings.rigidBodySubSteps = -2;
    settings.softBodyMaxSubSteps = -3;
    settings.softBodyConstraintIterations = -4;
    settings.softBodyCollisionIterations = -5;
    settings.softBodyGridScale = 0.1f;
    settings.fluidSolverIterations = -6;
    settings.fluidResolutionScale = 2.0f;
    settings.materialMaxSubSteps = -4;

    scope.system().setPhysicsLODSettings(settings);
    const PhysicsLODSettings& actual = scope.system().physicsLODSettings();

    EXPECT_FLOAT_EQ(actual.targetHz, 0.0f);
    EXPECT_EQ(actual.rigidBodySubSteps, 0);
    EXPECT_EQ(actual.softBodyMaxSubSteps, 0);
    EXPECT_EQ(actual.softBodyConstraintIterations, 0);
    EXPECT_EQ(actual.softBodyCollisionIterations, 0);
    EXPECT_FLOAT_EQ(actual.softBodyGridScale, 0.25f);
    EXPECT_EQ(actual.fluidSolverIterations, 0);
    EXPECT_FLOAT_EQ(actual.fluidResolutionScale, 1.0f);
    EXPECT_EQ(actual.materialMaxSubSteps, 0);
}

TEST(PhysicsLodSettingsContractTest, NonFiniteResolutionScalesStayWithinSupportedRange)
{
    PhysicsLodSettingsScope scope;
    PhysicsLODSettings settings;
    settings.level = PhysicsLODLevel::Full;
    settings.targetHz = std::numeric_limits<float>::quiet_NaN();
    settings.softBodyGridScale = std::numeric_limits<float>::quiet_NaN();
    settings.fluidResolutionScale = std::numeric_limits<float>::quiet_NaN();
    settings.fractureShardScale = std::numeric_limits<float>::quiet_NaN();
    settings.fractureDebrisScale = std::numeric_limits<float>::quiet_NaN();
    settings.sleepThreshold = std::numeric_limits<float>::quiet_NaN();

    scope.system().setPhysicsLODSettings(settings);
    const PhysicsLODSettings& actual = scope.system().physicsLODSettings();

    EXPECT_TRUE(std::isfinite(actual.softBodyGridScale));
    EXPECT_TRUE(std::isfinite(actual.fluidResolutionScale));
    EXPECT_TRUE(std::isfinite(actual.fractureShardScale));
    EXPECT_TRUE(std::isfinite(actual.fractureDebrisScale));
    EXPECT_TRUE(std::isfinite(actual.sleepThreshold));
    EXPECT_FLOAT_EQ(actual.targetHz, 0.0f);
    EXPECT_FLOAT_EQ(actual.softBodyGridScale, 1.0f);
    EXPECT_FLOAT_EQ(actual.fluidResolutionScale, 1.0f);
    EXPECT_FLOAT_EQ(actual.fractureShardScale, 1.0f);
    EXPECT_FLOAT_EQ(actual.fractureDebrisScale, 1.0f);
    EXPECT_FLOAT_EQ(actual.sleepThreshold, 0.0f);

    settings.targetHz = std::numeric_limits<float>::infinity();
    scope.system().setPhysicsLODSettings(settings);
    EXPECT_TRUE(std::isfinite(scope.system().physicsLODSettings().targetHz));
    EXPECT_FLOAT_EQ(scope.system().physicsLODSettings().targetHz, 0.0f);

    settings.softBodyGridScale = std::numeric_limits<float>::infinity();
    settings.fluidResolutionScale = std::numeric_limits<float>::infinity();
    scope.system().setPhysicsLODSettings(settings);
    EXPECT_FLOAT_EQ(scope.system().physicsLODSettings().softBodyGridScale, 1.0f);
    EXPECT_FLOAT_EQ(scope.system().physicsLODSettings().fluidResolutionScale, 1.0f);

    settings.softBodyGridScale = -std::numeric_limits<float>::infinity();
    settings.fluidResolutionScale = -std::numeric_limits<float>::infinity();
    scope.system().setPhysicsLODSettings(settings);
    EXPECT_FLOAT_EQ(scope.system().physicsLODSettings().softBodyGridScale, 0.25f);
    EXPECT_FLOAT_EQ(scope.system().physicsLODSettings().fluidResolutionScale, 0.125f);
}

TEST(PhysicsLodSettingsContractTest, FrozenLevelDoesNotAdvanceRegisteredSoftBodies)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId;
    auto softBody = system.createSoftBodyGrid(
        layerId, 0.0f, 0.0f, 10.0f, 10.0f, 5, 5, 1.0f, 1.0f, false);
    const SoftBodySnapshot before = softBody->snapshot();

    PhysicsLODSettings frozen;
    frozen.level = PhysicsLODLevel::Frozen;
    system.setPhysicsLODSettings(frozen);
    system.advancePhysicsFrame(1.0f / 30.0f, 0.0f, 9.8f, false);

    const SoftBodySnapshot after = softBody->snapshot();
    EXPECT_EQ(after.points.size(), before.points.size());
    for (std::size_t i = 0;
         i < before.points.size() && i < after.points.size(); ++i) {
        EXPECT_FLOAT_EQ(after.points[i].x, before.points[i].x);
        EXPECT_FLOAT_EQ(after.points[i].y, before.points[i].y);
        EXPECT_FLOAT_EQ(after.points[i].prevX, before.points[i].prevX);
        EXPECT_FLOAT_EQ(after.points[i].prevY, before.points[i].prevY);
    }

    system.unregisterSoftBody(layerId);
    system.setPhysicsTimelineSettings(previousTimeline);
}

TEST(PhysicsLodSettingsContractTest,
     NonFiniteFrameDeltasDoNotAdvanceOrPoisonReducedPhysicsLOD)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    struct TimelineCleanup final {
        PhysicsSystem& system;
        PhysicsTimelineSettings previous;
        ~TimelineCleanup() { system.setPhysicsTimelineSettings(previous); }
    } timelineCleanup{system, previousTimeline};

    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);

    const LayerID layerId;
    auto softBody = system.createSoftBodyGrid(
        layerId, 0.0f, 0.0f, 10.0f, 10.0f, 5, 5, 1.0f, 1.0f, false);
    ASSERT_TRUE(softBody);
    struct SoftBodyCleanup final {
        PhysicsSystem& system;
        LayerID layerId;
        ~SoftBodyCleanup() { system.unregisterSoftBody(layerId); }
    } softBodyCleanup{system, layerId};
    softBody->setGravity(0.0f, 1800.0f);
    const SoftBodySnapshot initial = softBody->snapshot();

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    for (const float invalidDelta : {nan, infinity, -infinity, 0.0f, -0.01f}) {
        system.advancePhysicsFrame(invalidDelta, 0.0f, 9.8f, false);
        const SoftBodySnapshot afterInvalidDelta = softBody->snapshot();
        ASSERT_EQ(afterInvalidDelta.points.size(), initial.points.size());
        EXPECT_FLOAT_EQ(afterInvalidDelta.points[12].y, initial.points[12].y)
            << "invalid frame delta=" << invalidDelta
            << " must not advance a fixed-timeline Reduced LOD step";
    }

    system.advancePhysicsFrame(timeline.fixedTimeStep, 0.0f, 9.8f, false);
    const SoftBodySnapshot afterValidDelta = softBody->snapshot();
    ASSERT_EQ(afterValidDelta.points.size(), initial.points.size());
    EXPECT_NE(afterValidDelta.points[12].y, initial.points[12].y)
        << "invalid deltas must not prevent the next valid physics LOD step";
}

TEST(PhysicsLodSettingsContractTest, ReturningFromFrozenResumesSoftBodySimulation)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId;
    auto softBody = system.createSoftBodyGrid(
        layerId, 0.0f, 0.0f, 10.0f, 10.0f, 5, 5, 1.0f, 1.0f, false);
    softBody->setGravity(0.0f, 1800.0f);
    const SoftBodySnapshot before = softBody->snapshot();

    PhysicsLODSettings frozen;
    frozen.level = PhysicsLODLevel::Frozen;
    system.setPhysicsLODSettings(frozen);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    const SoftBodySnapshot frozenState = softBody->snapshot();
    ASSERT_EQ(frozenState.points.size(), before.points.size());
    EXPECT_FLOAT_EQ(frozenState.points[12].y, before.points[12].y);

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    const SoftBodySnapshot resumed = softBody->snapshot();
    ASSERT_EQ(resumed.points.size(), before.points.size());
    EXPECT_NE(resumed.points[12].y, frozenState.points[12].y)
        << "returning from Frozen should resume the registered soft body";

    system.unregisterSoftBody(layerId);
    system.setPhysicsTimelineSettings(previousTimeline);
}

TEST(PhysicsLodSettingsContractTest,
     ReducedSoftBodyKeepsItsStateWhileFrozenAndRestoresAtFull)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId(QStringLiteral("physics-lod-reduced-frozen-softbody"));
    auto softBody = system.createSoftBodyGrid(
        layerId, 0.0f, 0.0f, 10.0f, 10.0f, 9, 9, 1.0f, 1.0f, false);
    ASSERT_TRUE(softBody);
    struct SoftBodyLodCleanup final {
        PhysicsSystem& system;
        LayerID layerId;
        PhysicsTimelineSettings timeline;
        ~SoftBodyLodCleanup()
        {
            system.unregisterSoftBody(layerId);
            system.setPhysicsTimelineSettings(timeline);
        }
    } cleanup{system, layerId, previousTimeline};
    softBody->setGravity(0.0f, 1800.0f);

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    const SoftBodySnapshot reducedState = softBody->snapshot();
    ASSERT_LT(reducedState.points.size(), 9u * 9u);

    PhysicsLODSettings frozen;
    frozen.level = PhysicsLODLevel::Frozen;
    system.setPhysicsLODSettings(frozen);
    system.advancePhysicsFrame(0.25f, 0.0f, 9.8f, false);
    const SoftBodySnapshot frozenState = softBody->snapshot();
    ASSERT_EQ(frozenState.points.size(), reducedState.points.size());
    ASSERT_EQ(frozenState.constraints.size(), reducedState.constraints.size());
    for (std::size_t index = 0; index < reducedState.points.size(); ++index) {
        EXPECT_FLOAT_EQ(frozenState.points[index].x, reducedState.points[index].x);
        EXPECT_FLOAT_EQ(frozenState.points[index].y, reducedState.points[index].y);
        EXPECT_FLOAT_EQ(frozenState.points[index].prevX,
                        reducedState.points[index].prevX);
        EXPECT_FLOAT_EQ(frozenState.points[index].prevY,
                        reducedState.points[index].prevY);
    }

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    const SoftBodySnapshot restoredState = softBody->snapshot();
    EXPECT_EQ(restoredState.points.size(), 9u * 9u);
    EXPECT_EQ(restoredState.gridColumns, 9);
    EXPECT_EQ(restoredState.gridRows, 9);
}

TEST(PhysicsLodSettingsContractTest,
     UnknownLevelContinuesRegisteredSoftBodySimulation)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId(QStringLiteral("physics-lod-unknown-softbody"));
    auto softBody = system.createSoftBodyGrid(
        layerId, 0.0f, 0.0f, 10.0f, 10.0f, 5, 5, 1.0f, 1.0f, false);
    ASSERT_TRUE(softBody);
    struct SoftBodyLodCleanup final {
        PhysicsSystem& system;
        LayerID layerId;
        PhysicsTimelineSettings timeline;
        ~SoftBodyLodCleanup()
        {
            system.unregisterSoftBody(layerId);
            system.setPhysicsTimelineSettings(timeline);
        }
    } cleanup{system, layerId, previousTimeline};
    softBody->setGravity(0.0f, 1800.0f);
    const SoftBodySnapshot before = softBody->snapshot();

    PhysicsLODSettings minimal;
    minimal.level = PhysicsLODLevel::Minimal;
    system.setPhysicsLODSettings(minimal);
    system.advanceSoftBodyFrame(0.2f);
    const SoftBodySnapshot reduced = softBody->snapshot();
    ASSERT_LT(reduced.points.size(), before.points.size());

    PhysicsLODSettings unknown;
    unknown.level = static_cast<PhysicsLODLevel>(255);
    system.setPhysicsLODSettings(unknown);
    EXPECT_FLOAT_EQ(system.physicsLODSettings().softBodyGridScale, 1.0f);
    system.advanceSoftBodyFrame(1.0f / 60.0f);

    const SoftBodySnapshot after = softBody->snapshot();
    ASSERT_EQ(after.points.size(), before.points.size());
    EXPECT_NE(after.points[12].y, before.points[12].y)
        << "an unknown LOD enum should restore Full topology and keep simulation running";

}

TEST(PhysicsLodSettingsContractTest, SoftBodyCatchUpWorkShrinksAcrossLodLevels)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();

    const auto simulate = [&](const PhysicsLODLevel level) {
        const LayerID layerId;
        auto softBody = system.createSoftBodyGrid(
            layerId, 0.0f, 0.0f, 1.0f, 1.0f, 2, 2, 1.0f, 1.0f, false);
        softBody->setGravity(0.0f, 1800.0f);

        PhysicsLODSettings settings;
        settings.level = level;
        system.setPhysicsLODSettings(settings);
        system.advanceSoftBodyFrame(0.2f);
        const int appliedSubsteps = softBody->maxSubsteps();
        const float resultingY = softBody->point(0).y;
        EXPECT_EQ(softBody->gridColumns(), 2)
            << "the tiny grid should retain its topology at every LOD";
        system.unregisterSoftBody(layerId);
        return std::pair{appliedSubsteps, resultingY};
    };

    const auto full = simulate(PhysicsLODLevel::Full);
    const auto reduced = simulate(PhysicsLODLevel::Reduced);
    const auto minimal = simulate(PhysicsLODLevel::Minimal);

    EXPECT_EQ(full.first, 8);
    EXPECT_EQ(reduced.first, 4);
    EXPECT_EQ(minimal.first, 2);
    EXPECT_GT(full.second, reduced.second);
    EXPECT_GT(reduced.second, minimal.second);
}

TEST(PhysicsLodSettingsContractTest, ReturningToFullRestoresSoftBodySelfCollision)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const LayerID layerId;
    auto softBody = system.createSoftBodyGrid(
        layerId, 0.0f, 0.0f, 10.0f, 10.0f, 5, 5, 1.0f, 1.0f, false);
    softBody->setSelfCollisionEnabled(true);

    PhysicsLODSettings minimal;
    minimal.level = PhysicsLODLevel::Minimal;
    system.setPhysicsLODSettings(minimal);
    system.advanceSoftBodyFrame(1.0f / 60.0f);
    EXPECT_FALSE(softBody->isSelfCollisionEnabled());

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    system.advanceSoftBodyFrame(1.0f / 60.0f);
    EXPECT_TRUE(softBody->isSelfCollisionEnabled())
        << "returning to Full should restore the authored self-collision setting";

    system.unregisterSoftBody(layerId);
}

TEST(PhysicsLodSettingsContractTest, FrozenLevelDoesNotAdvanceRegisteredFluidSolver)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId;
    auto fluid = system.createFluidSolver(layerId, 8, 8);
    fluid->addDensity(3, 4, 2.0f);
    fluid->addVelocity(3, 4, 1.5f, -0.75f);
    const FluidSnapshot2D before = fluid->snapshot();

    PhysicsLODSettings frozen;
    frozen.level = PhysicsLODLevel::Frozen;
    system.setPhysicsLODSettings(frozen);
    system.advancePhysicsFrame(1.0f / 30.0f, 0.0f, 9.8f, false);

    const FluidSnapshot2D after = fluid->snapshot();
    ASSERT_EQ(after.density.size(), before.density.size());
    ASSERT_EQ(after.densityPrev.size(), before.densityPrev.size());
    ASSERT_EQ(after.velocityX.size(), before.velocityX.size());
    ASSERT_EQ(after.velocityY.size(), before.velocityY.size());
    ASSERT_EQ(after.velocityXPrev.size(), before.velocityXPrev.size());
    ASSERT_EQ(after.velocityYPrev.size(), before.velocityYPrev.size());
    ASSERT_EQ(after.curl.size(), before.curl.size());
    for (std::size_t i = 0; i < before.density.size(); ++i) {
        EXPECT_FLOAT_EQ(after.density[i], before.density[i]);
        EXPECT_FLOAT_EQ(after.densityPrev[i], before.densityPrev[i]);
        EXPECT_FLOAT_EQ(after.velocityX[i], before.velocityX[i]);
        EXPECT_FLOAT_EQ(after.velocityY[i], before.velocityY[i]);
        EXPECT_FLOAT_EQ(after.velocityXPrev[i], before.velocityXPrev[i]);
        EXPECT_FLOAT_EQ(after.velocityYPrev[i], before.velocityYPrev[i]);
        EXPECT_FLOAT_EQ(after.curl[i], before.curl[i]);
    }

    system.unregisterFluidSolver(layerId);
    system.setPhysicsTimelineSettings(previousTimeline);
}

TEST(PhysicsLodSettingsContractTest, ReturningFromFrozenResumesFluidSimulation)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId;
    auto fluid = system.createFluidSolver(layerId, 8, 8);
    fluid->addDensity(3, 4, 2.0f);
    fluid->addVelocity(3, 4, 1.5f, -0.75f);

    PhysicsLODSettings frozen;
    frozen.level = PhysicsLODLevel::Frozen;
    system.setPhysicsLODSettings(frozen);
    const FluidSnapshot2D frozenBefore = fluid->snapshot();
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    const FluidSnapshot2D frozenAfter = fluid->snapshot();
    EXPECT_EQ(frozenAfter.density.size(), frozenBefore.density.size());
    for (std::size_t index = 0;
         index < std::min(frozenAfter.density.size(), frozenBefore.density.size());
         ++index) {
        EXPECT_FLOAT_EQ(frozenAfter.density[index], frozenBefore.density[index]);
    }

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    const FluidSnapshot2D resumed = fluid->snapshot();
    EXPECT_EQ(resumed.density.size(), frozenAfter.density.size());
    bool simulationAdvanced = false;
    for (std::size_t index = 0;
         index < std::min(resumed.density.size(), frozenAfter.density.size());
         ++index) {
        simulationAdvanced = simulationAdvanced ||
            resumed.density[index] != frozenAfter.density[index];
    }
    EXPECT_TRUE(simulationAdvanced)
        << "returning from Frozen to Full should resume the fluid solver";

    system.unregisterFluidSolver(layerId);
    system.setPhysicsTimelineSettings(previousTimeline);
}

TEST(PhysicsLodSettingsContractTest, FrozenLevelDoesNotAdvanceRegisteredMaterialSolver)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId;
    auto material = system.createMaterialSolver(layerId);
    material->addParticlesGrid(0.0f, 0.0f, 16.0f, 12.0f, 4, 3);
    material->setGravity(0.0f, -980.0f);
    const MpmSnapshot2D before = material->snapshot();
    ASSERT_FALSE(before.particles.empty());

    PhysicsLODSettings frozen;
    frozen.level = PhysicsLODLevel::Frozen;
    system.setPhysicsLODSettings(frozen);
    system.advancePhysicsFrame(1.0f / 30.0f, 0.0f, 9.8f, false);

    const MpmSnapshot2D after = material->snapshot();
    ASSERT_EQ(after.particles.size(), before.particles.size());
    ASSERT_EQ(after.fracturedIndices.size(), before.fracturedIndices.size());
    EXPECT_FLOAT_EQ(after.accumulatedTime, before.accumulatedTime);
    for (std::size_t i = 0; i < before.particles.size(); ++i) {
        const auto& expected = before.particles[i];
        const auto& actual = after.particles[i];
        EXPECT_FLOAT_EQ(actual.pos.x, expected.pos.x);
        EXPECT_FLOAT_EQ(actual.pos.y, expected.pos.y);
        EXPECT_FLOAT_EQ(actual.vel.x, expected.vel.x);
        EXPECT_FLOAT_EQ(actual.vel.y, expected.vel.y);
        EXPECT_FLOAT_EQ(actual.mass, expected.mass);
        EXPECT_FLOAT_EQ(actual.F.m00, expected.F.m00);
        EXPECT_FLOAT_EQ(actual.F.m01, expected.F.m01);
        EXPECT_FLOAT_EQ(actual.F.m10, expected.F.m10);
        EXPECT_FLOAT_EQ(actual.F.m11, expected.F.m11);
        EXPECT_FLOAT_EQ(actual.Fp.m00, expected.Fp.m00);
        EXPECT_FLOAT_EQ(actual.Fp.m01, expected.Fp.m01);
        EXPECT_FLOAT_EQ(actual.Fp.m10, expected.Fp.m10);
        EXPECT_FLOAT_EQ(actual.Fp.m11, expected.Fp.m11);
        EXPECT_FLOAT_EQ(actual.C.m00, expected.C.m00);
        EXPECT_FLOAT_EQ(actual.C.m01, expected.C.m01);
        EXPECT_FLOAT_EQ(actual.C.m10, expected.C.m10);
        EXPECT_FLOAT_EQ(actual.C.m11, expected.C.m11);
        EXPECT_FLOAT_EQ(actual.plasticStrain, expected.plasticStrain);
        EXPECT_EQ(actual.active, expected.active);
    }
    for (std::size_t i = 0; i < before.fracturedIndices.size(); ++i) {
        EXPECT_EQ(after.fracturedIndices[i], before.fracturedIndices[i]);
    }

    system.removeMaterialSolver(layerId);
    system.setPhysicsTimelineSettings(previousTimeline);
}

TEST(PhysicsLodSettingsContractTest,
     ReturningFromFrozenResumesRegisteredMaterialSolver)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId(QStringLiteral("physics-lod-material-resume-probe"));
    struct MaterialSolverCleanup final {
        PhysicsSystem& system;
        LayerID layerId;
        PhysicsTimelineSettings timeline;
        ~MaterialSolverCleanup()
        {
            system.removeMaterialSolver(layerId);
            system.setPhysicsTimelineSettings(timeline);
        }
    } cleanup{system, layerId, previousTimeline};

    auto material = system.createMaterialSolver(layerId);
    ASSERT_TRUE(material);
    material->addParticlesGrid(0.0f, 0.0f, 16.0f, 12.0f, 4, 3);
    material->setGravity(0.0f, -980.0f);
    const MpmSnapshot2D initial = material->snapshot();
    ASSERT_FALSE(initial.particles.empty());

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    const MpmSnapshot2D running = material->snapshot();
    ASSERT_NE(running.particles[0].pos.y, initial.particles[0].pos.y)
        << "the material solver should advance before entering Frozen";

    PhysicsLODSettings frozen;
    frozen.level = PhysicsLODLevel::Frozen;
    system.setPhysicsLODSettings(frozen);
    system.advancePhysicsFrame(0.25f, 0.0f, 9.8f, false);
    const MpmSnapshot2D stopped = material->snapshot();
    ASSERT_FLOAT_EQ(stopped.particles[0].pos.y, running.particles[0].pos.y);
    EXPECT_FLOAT_EQ(stopped.particles[0].vel.y, running.particles[0].vel.y);

    system.setPhysicsLODSettings(full);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    const MpmSnapshot2D resumed = material->snapshot();
    EXPECT_NE(resumed.particles[0].pos.y, stopped.particles[0].pos.y)
        << "returning to Full should resume the registered material solver";
}

TEST(PhysicsLodSettingsContractTest,
     FrozenLevelStopsAndFullResumesGranularMaterialSolver)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    constexpr std::uint64_t simulationWorldId = 42017ULL;
    const LayerID layerId(QStringLiteral("physics-lod-granular-frozen-probe"));
    auto solver = system.createGranularMaterialGrid(
        simulationWorldId, layerId, 0.0f, 0.0f, 4.0f, 4.0f,
        -4.0f, -4.0f, 12.0f, 12.0f, 3, 3,
        1000.0f, 0.2f, 0.4f, 1.0f, 980.0f, 0.1f, 2.0f);
    ASSERT_NE(solver, nullptr);
    ASSERT_FALSE(solver->particles().empty());

    const float initialY = solver->particles()[0].pos.y;
    system.advancePhysicsFrame(1.0f / 30.0f, 0.0f, 9.8f, false,
                               simulationWorldId);
    const float beforeFrozenY = solver->particles()[0].pos.y;
    ASSERT_NE(beforeFrozenY, initialY)
        << "granular material should advance at Full quality";

    PhysicsLODSettings frozen;
    frozen.level = PhysicsLODLevel::Frozen;
    system.setPhysicsLODSettings(frozen);
    system.advancePhysicsFrame(1.0f / 30.0f, 0.0f, 9.8f, false,
                               simulationWorldId);
    EXPECT_FLOAT_EQ(solver->particles()[0].pos.y, beforeFrozenY)
        << "Frozen quality should preserve granular particle positions";

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    system.advancePhysicsFrame(1.0f / 30.0f, 0.0f, 9.8f, false,
                               simulationWorldId);
    EXPECT_NE(solver->particles()[0].pos.y, beforeFrozenY)
        << "returning to Full should resume granular material simulation";

    system.unregisterGranularMaterialSolver(simulationWorldId, layerId);
}

TEST(PhysicsLodSettingsContractTest, FrozenLevelStopsRigidWorldAndFullResumesIt)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId;
    auto world = system.createRigidWorld(layerId);
    const std::vector<QVector2D> triangle{
        {-0.5f, -0.5f}, {0.5f, -0.5f}, {0.0f, 0.5f}};
    auto body = world->addPolygonBody(
        0.0f, 0.0f, triangle, true, 1.0f);
    const QVector2D initialPosition = body->position();

    PhysicsLODSettings frozen;
    frozen.level = PhysicsLODLevel::Frozen;
    system.setPhysicsLODSettings(frozen);
    system.advancePhysicsFrame(1.0f / 30.0f, 0.0f, 9.8f, false);
    EXPECT_EQ(body->position(), initialPosition);

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_NE(body->position(), initialPosition)
        << "returning from Frozen should resume the rigid world";

    system.unregisterRigidWorld(layerId);
    system.setPhysicsTimelineSettings(previousTimeline);
}

TEST(PhysicsLodSettingsContractTest, ReducedTargetRateAccumulatesRigidWorldDeltas)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);

    const LayerID layerId;
    auto world = system.createRigidWorld(layerId);
    const std::vector<QVector2D> triangle{
        {-0.5f, -0.5f}, {0.5f, -0.5f}, {0.0f, 0.5f}};
    auto body = world->addPolygonBody(
        0.0f, 0.0f, triangle, true, 1.0f);
    const QVector2D initialPosition = body->position();

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);
    const float frameDelta = 1.0f / 60.0f;
    const float reducedInterval = 1.0f / system.physicsLODSettings().targetHz;
    system.update(frameDelta, 0.0f, 9.8f, false);
    EXPECT_EQ(body->position(), initialPosition)
        << "a single 60Hz delta should not advance a 30Hz Reduced world";

    system.update(frameDelta, 0.0f, 9.8f, false);
    EXPECT_NE(body->position(), initialPosition)
        << "the second delta should advance the accumulated Reduced interval";
    EXPECT_NEAR(reducedInterval, frameDelta * 2.0f, 1.0e-6f);

    system.unregisterRigidWorld(layerId);
}

TEST(PhysicsLodSettingsContractTest,
     ReducedRigidBodySubstepBudgetChangesContactTrajectory)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const CompositionID compositionId;
    const std::vector<QVector2D> floorPolygon{
        {-8.0f, -0.25f}, {8.0f, -0.25f},
        {8.0f, 0.25f}, {-8.0f, 0.25f}};
    const std::vector<QVector2D> bodyPolygon{
        {-0.5f, -0.5f}, {0.5f, -0.5f},
        {0.5f, 0.5f}, {-0.5f, 0.5f}};

    const auto simulateContact = [&](const PhysicsLODLevel level) {
        PhysicsLODSettings settings;
        settings.level = level;
        system.setPhysicsLODSettings(settings);

        auto world = system.createCompositionRigidWorld(compositionId);
        if (!world) {
            ADD_FAILURE() << "could not create the composition rigid world";
            return QVector2D{};
        }
        world->setGravity(0.0f, 500.0f);
        world->addPolygonBody(0.0f, 3.0f, floorPolygon, false);
        auto body = world->addPolygonBody(
            0.0f, 2.0f, bodyPolygon, true, 1.0f, 0.0f, 0.0f);
        if (!body) {
            ADD_FAILURE() << "could not create the dynamic contact body";
            system.unregisterCompositionRigidWorld(compositionId);
            return QVector2D{};
        }
        body->setLinearVelocity(QVector2D(0.0f, 15.0f));

        system.updateCompositionRigidWorld(compositionId, 1.0f / 30.0f);
        const QVector2D position = body->position();
        system.unregisterCompositionRigidWorld(compositionId);
        return position;
    };

    const QVector2D fullPosition = simulateContact(PhysicsLODLevel::Full);
    const QVector2D reducedPosition = simulateContact(PhysicsLODLevel::Reduced);

    EXPECT_TRUE(std::isfinite(fullPosition.x()));
    EXPECT_TRUE(std::isfinite(fullPosition.y()));
    EXPECT_TRUE(std::isfinite(reducedPosition.x()));
    EXPECT_TRUE(std::isfinite(reducedPosition.y()));
    EXPECT_GT((fullPosition - reducedPosition).length(), 1.0e-5f)
        << "the Reduced rigid-body substep budget should affect the same contact solve";
}

TEST(PhysicsLodSettingsContractTest,
     MinimalLODDisablesBulletCollisionWhileFullPreservesIt)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const std::vector<QVector2D> wallPolygon{
        {-0.05f, -2.0f}, {0.05f, -2.0f},
        {0.05f, 2.0f}, {-0.05f, 2.0f}};
    const std::vector<QVector2D> bodyPolygon{
        {-0.1f, -0.1f}, {0.1f, -0.1f},
        {0.1f, 0.1f}, {-0.1f, 0.1f}};
    const auto simulateWallCrossing = [&](const PhysicsLODLevel level) {
        PhysicsLODSettings settings;
        settings.level = level;
        system.setPhysicsLODSettings(settings);
        const LayerID layerId;
        auto world = system.createRigidWorld(layerId);
        if (!world) {
            ADD_FAILURE() << "could not create the rigid world";
            return std::numeric_limits<float>::quiet_NaN();
        }
        world->setGravity(0.0f, 0.0f);
        world->addPolygonBody(0.0f, 0.0f, wallPolygon, false);
        auto body = world->addPolygonBody(
            -1.0f, 0.0f, bodyPolygon, true, 1.0f, 0.0f, 0.0f);
        if (!body) {
            ADD_FAILURE() << "could not create the moving body";
            system.unregisterRigidWorld(layerId);
            return std::numeric_limits<float>::quiet_NaN();
        }
        body->setContinuousCollision(true);
        body->setLinearVelocity(QVector2D(100.0f, 0.0f));

        system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 0.0f, false);
        const float positionX = body->position().x();
        system.unregisterRigidWorld(layerId);
        return positionX;
    };

    const float fullPositionX = simulateWallCrossing(PhysicsLODLevel::Full);
    const float minimalPositionX = simulateWallCrossing(PhysicsLODLevel::Minimal);
    system.setPhysicsTimelineSettings(previousTimeline);

    EXPECT_TRUE(std::isfinite(fullPositionX));
    EXPECT_TRUE(std::isfinite(minimalPositionX));
    EXPECT_LT(fullPositionX, 0.0f)
        << "Full should preserve the body's enabled bullet collision at the wall";
    EXPECT_GT(minimalPositionX, 0.0f)
        << "Minimal should disable bullet collision and allow this fast body to cross";
}

TEST(PhysicsLodSettingsContractTest,
     ReturningFromMinimalRestoresAuthoredBulletCollisionAcrossHigherLevels)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId;
    auto world = system.createRigidWorld(layerId);
    ASSERT_TRUE(world);
    world->setGravity(0.0f, 0.0f);
    const std::vector<QVector2D> wallPolygon{
        {-0.05f, -2.0f}, {0.05f, -2.0f},
        {0.05f, 2.0f}, {-0.05f, 2.0f}};
    const std::vector<QVector2D> bodyPolygon{
        {-0.1f, -0.1f}, {0.1f, -0.1f},
        {0.1f, 0.1f}, {-0.1f, 0.1f}};
    world->addPolygonBody(0.0f, 0.0f, wallPolygon, false);
    auto body = world->addPolygonBody(
        -1.0f, 0.0f, bodyPolygon, true, 1.0f, 0.0f, 0.0f);
    ASSERT_TRUE(body);
    body->setContinuousCollision(true);

    PhysicsLODSettings minimal;
    minimal.level = PhysicsLODLevel::Minimal;
    system.setPhysicsLODSettings(minimal);
    body->setTransform(QVector2D(-1.0f, 0.0f), 0.0f);
    body->setLinearVelocity(QVector2D(100.0f, 0.0f));
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 0.0f, false);
    const float minimalPositionX = body->position().x();

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);
    body->setTransform(QVector2D(-1.0f, 0.0f), 0.0f);
    body->setLinearVelocity(QVector2D(100.0f, 0.0f));
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 0.0f, false);
    const float restoredReducedPositionX = body->position().x();

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    body->setTransform(QVector2D(-1.0f, 0.0f), 0.0f);
    body->setLinearVelocity(QVector2D(100.0f, 0.0f));
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 0.0f, false);
    const float restoredFullPositionX = body->position().x();

    system.unregisterRigidWorld(layerId);
    system.setPhysicsTimelineSettings(previousTimeline);

    EXPECT_GT(minimalPositionX, 0.0f)
        << "Minimal should disable the authored bullet collision";
    EXPECT_LT(restoredReducedPositionX, 0.0f)
        << "returning to Reduced should restore the body's authored bullet collision";
    EXPECT_LT(restoredFullPositionX, 0.0f)
        << "returning to Full should restore the body's authored bullet collision";
}

TEST(PhysicsLodSettingsContractTest, FrozenLevelStopsCompositionRigidPlayback)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const CompositionID compositionId;
    auto world = system.createCompositionRigidWorld(compositionId);
    const std::vector<QVector2D> triangle{
        {-0.5f, -0.5f}, {0.5f, -0.5f}, {0.0f, 0.5f}};
    auto body = world->addPolygonBody(
        0.0f, 0.0f, triangle, true, 1.0f);
    const QVector2D initialPosition = body->position();

    PhysicsLODSettings frozen;
    frozen.level = PhysicsLODLevel::Frozen;
    system.setPhysicsLODSettings(frozen);
    system.updateCompositionRigidWorld(compositionId, 1.0f / 30.0f);
    EXPECT_EQ(body->position(), initialPosition);

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    system.updateCompositionRigidWorld(compositionId, 1.0f / 60.0f);
    EXPECT_NE(body->position(), initialPosition)
        << "Full LOD should resume the composition playback-owned world";

    system.unregisterCompositionRigidWorld(compositionId);
}

TEST(PhysicsLodSettingsContractTest,
     ReducedLODDoesNotThrottleCompositionOwnedRigidFixedClock)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const CompositionID compositionId;
    auto world = system.createCompositionRigidWorld(compositionId);
    ASSERT_TRUE(world);
    const std::vector<QVector2D> triangle{
        {-0.5f, -0.5f}, {0.5f, -0.5f}, {0.0f, 0.5f}};
    auto body = world->addPolygonBody(
        0.0f, 0.0f, triangle, true, 1.0f);
    ASSERT_TRUE(body);

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);
    const QVector2D initialPosition = body->position();
    constexpr float fixedDelta = 1.0f / 60.0f;

    system.updateCompositionRigidWorld(compositionId, fixedDelta);
    const QVector2D afterFirstTick = body->position();
    EXPECT_NE(afterFirstTick, initialPosition)
        << "a composition-owned fixed tick must run even below Reduced targetHz";

    system.updateCompositionRigidWorld(compositionId, fixedDelta);
    EXPECT_NE(body->position(), afterFirstTick)
        << "each fixed-clock tick should advance the Reduced composition world";

    system.unregisterCompositionRigidWorld(compositionId);
}

TEST(PhysicsLodSettingsContractTest, ReducedGridResolutionRestoresAtFullLevel)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId;
    auto softBody = system.createSoftBodyGrid(
        layerId, 0.0f, 0.0f, 10.0f, 10.0f, 9, 9);
    const SoftBodySnapshot fullResolutionBeforeLod = softBody->snapshot();
    ASSERT_EQ(std::count_if(
                  fullResolutionBeforeLod.points.begin(),
                  fullResolutionBeforeLod.points.end(),
                  [](const SoftBodyPoint& point) { return point.isPinned; }),
              9);

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    const SoftBodySnapshot reducedSnapshot = softBody->snapshot();
    EXPECT_EQ(reducedSnapshot.gridColumns, 7);
    EXPECT_EQ(reducedSnapshot.gridRows, 7);
    EXPECT_EQ(reducedSnapshot.points.size(), 49u);
    EXPECT_EQ(std::count_if(reducedSnapshot.points.begin(),
                            reducedSnapshot.points.end(),
                            [](const SoftBodyPoint& point) {
                                return point.isPinned;
                            }),
              reducedSnapshot.gridColumns);
    for (int index = 0; index < static_cast<int>(reducedSnapshot.points.size());
         ++index) {
        EXPECT_EQ(reducedSnapshot.points[static_cast<std::size_t>(index)].isPinned,
                  index < reducedSnapshot.gridColumns);
    }
    EXPECT_EQ(softBody->maxSubsteps(), 4);

    PhysicsLODSettings minimal;
    minimal.level = PhysicsLODLevel::Minimal;
    system.setPhysicsLODSettings(minimal);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    const SoftBodySnapshot minimalSnapshot = softBody->snapshot();
    EXPECT_EQ(minimalSnapshot.gridColumns, 5);
    EXPECT_EQ(minimalSnapshot.gridRows, 5);
    EXPECT_EQ(minimalSnapshot.points.size(), 25u);
    EXPECT_EQ(std::count_if(minimalSnapshot.points.begin(),
                            minimalSnapshot.points.end(),
                            [](const SoftBodyPoint& point) {
                                return point.isPinned;
                            }),
              minimalSnapshot.gridColumns);
    for (int index = 0; index < static_cast<int>(minimalSnapshot.points.size());
         ++index) {
        EXPECT_EQ(minimalSnapshot.points[static_cast<std::size_t>(index)].isPinned,
                  index < minimalSnapshot.gridColumns);
    }
    EXPECT_EQ(softBody->maxSubsteps(), 2);

    system.setPhysicsLODSettings(reduced);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    const SoftBodySnapshot restoredReducedSnapshot = softBody->snapshot();
    EXPECT_EQ(restoredReducedSnapshot.gridColumns, 7);
    EXPECT_EQ(restoredReducedSnapshot.gridRows, 7);
    EXPECT_EQ(restoredReducedSnapshot.points.size(), 49u);
    EXPECT_EQ(std::count_if(
                  restoredReducedSnapshot.points.begin(),
                  restoredReducedSnapshot.points.end(),
                  [](const SoftBodyPoint& point) { return point.isPinned; }),
              restoredReducedSnapshot.gridColumns);
    for (int index = 0;
         index < static_cast<int>(restoredReducedSnapshot.points.size());
         ++index) {
        EXPECT_EQ(restoredReducedSnapshot.points[static_cast<std::size_t>(index)].isPinned,
                  index < restoredReducedSnapshot.gridColumns);
    }
    EXPECT_EQ(softBody->maxSubsteps(), 4);

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    const SoftBodySnapshot fullSnapshot = softBody->snapshot();
    EXPECT_EQ(fullSnapshot.gridColumns, 9);
    EXPECT_EQ(fullSnapshot.gridRows, 9);
    EXPECT_EQ(fullSnapshot.points.size(), 81u);
    EXPECT_EQ(std::count_if(fullSnapshot.points.begin(),
                            fullSnapshot.points.end(),
                            [](const SoftBodyPoint& point) {
                                return point.isPinned;
                            }),
              std::count_if(fullResolutionBeforeLod.points.begin(),
                            fullResolutionBeforeLod.points.end(),
                            [](const SoftBodyPoint& point) {
                                return point.isPinned;
                            }));
    ASSERT_EQ(fullSnapshot.points.size(),
              fullResolutionBeforeLod.points.size());
    for (std::size_t index = 0; index < fullSnapshot.points.size(); ++index) {
        EXPECT_EQ(fullSnapshot.points[index].isPinned,
                  fullResolutionBeforeLod.points[index].isPinned);
    }
    EXPECT_EQ(softBody->maxSubsteps(), 8)
        << "returning to Full should restore the solver's default substep budget";

    system.unregisterSoftBody(layerId);
    system.setPhysicsTimelineSettings(previousTimeline);
}

TEST(PhysicsLodSettingsContractTest,
     ReducedConstraintBudgetChangesSoftBodySolveOnTheSameGrid)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId;
    const auto simulate = [&](PhysicsLODSettings settings) {
        system.setPhysicsLODSettings(settings);
        auto softBody = system.createSoftBodyGrid(
            layerId, 0.0f, 0.0f, 10.0f, 10.0f, 9, 9);
        softBody->setGravity(0.0f, 1800.0f);
        system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 0.0f, false);
        const SoftBodySnapshot snapshot = softBody->snapshot();
        system.unregisterSoftBody(layerId);
        return snapshot;
    };

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    const SoftBodySnapshot fullSnapshot = simulate(full);

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    reduced.softBodyGridScale = 0.99f;
    const SoftBodySnapshot reducedSnapshot = simulate(reduced);
    system.setPhysicsTimelineSettings(previousTimeline);

    ASSERT_EQ(fullSnapshot.gridColumns, 9);
    ASSERT_EQ(fullSnapshot.gridRows, 9);
    ASSERT_EQ(reducedSnapshot.gridColumns, fullSnapshot.gridColumns);
    ASSERT_EQ(reducedSnapshot.gridRows, fullSnapshot.gridRows);
    ASSERT_EQ(reducedSnapshot.points.size(), fullSnapshot.points.size());

    float maximumPositionDelta = 0.0f;
    for (std::size_t index = 0; index < fullSnapshot.points.size(); ++index) {
        const auto& fullPoint = fullSnapshot.points[index];
        const auto& reducedPoint = reducedSnapshot.points[index];
        EXPECT_EQ(reducedPoint.isPinned, fullPoint.isPinned);
        maximumPositionDelta = std::max(
            maximumPositionDelta,
            std::abs(reducedPoint.x - fullPoint.x));
        maximumPositionDelta = std::max(
            maximumPositionDelta,
            std::abs(reducedPoint.y - fullPoint.y));
    }
    EXPECT_GT(maximumPositionDelta, 1.0e-5f)
        << "Reduced LOD should apply its lower constraint-iteration budget "
           "while preserving the same grid topology";
}

TEST(PhysicsLodSettingsContractTest,
     ReturningFromReducedRestoresSoftBodyConstraintIterations)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const LayerID transitionedLayerId;
    auto transitioned = system.createSoftBodyGrid(
        transitionedLayerId, 0.0f, 0.0f, 10.0f, 10.0f, 9, 9);
    transitioned->setGravity(0.0f, 1800.0f);

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);
    system.advanceSoftBodyFrame(1.0f / 60.0f);

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    const LayerID referenceLayerId;
    auto fullReference = system.createSoftBodyGrid(
        referenceLayerId, 0.0f, 0.0f, 10.0f, 10.0f, 9, 9);
    fullReference->setGravity(0.0f, 1800.0f);

    system.advanceSoftBodyFrame(1.0f / 60.0f);
    const SoftBodySnapshot transitionedSnapshot = transitioned->snapshot();
    const SoftBodySnapshot fullSnapshot = fullReference->snapshot();
    ASSERT_EQ(transitionedSnapshot.gridColumns, fullSnapshot.gridColumns);
    ASSERT_EQ(transitionedSnapshot.gridRows, fullSnapshot.gridRows);
    ASSERT_EQ(transitionedSnapshot.points.size(), fullSnapshot.points.size());

    float maximumPositionDelta = 0.0f;
    for (std::size_t index = 0; index < fullSnapshot.points.size(); ++index) {
        maximumPositionDelta = std::max(
            maximumPositionDelta,
            std::abs(transitionedSnapshot.points[index].x -
                     fullSnapshot.points[index].x));
        maximumPositionDelta = std::max(
            maximumPositionDelta,
            std::abs(transitionedSnapshot.points[index].y -
                     fullSnapshot.points[index].y));
    }
    EXPECT_LT(maximumPositionDelta, 1.0e-5f)
        << "a solver returning from Reduced should match a fresh Full solver";

    system.unregisterSoftBody(transitionedLayerId);
    system.unregisterSoftBody(referenceLayerId);
}

TEST(PhysicsLodSettingsContractTest,
     ReducedCollisionBudgetChangesSoftBodyContactProjection)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId;
    const auto simulate = [&](PhysicsLODSettings settings) {
        system.setPhysicsLODSettings(settings);
        auto softBody = system.createSoftBody(layerId);
        softBody->buildChain(0.0f, 0.0f, 0.0f, 0.0f, 2, 1.0f, 1.0f, false);
        softBody->setGravity(0.0f, 0.0f);

        SoftBodyCollider verticalPlane;
        verticalPlane.type = SoftBodyCollider::Type::Plane;
        verticalPlane.normalX = 1.0f;
        verticalPlane.normalY = 0.0f;
        verticalPlane.planeDistance = 1.0f;
        system.registerSoftBodyCollider(layerId, verticalPlane);

        SoftBodyCollider diagonalPlane;
        diagonalPlane.type = SoftBodyCollider::Type::Plane;
        diagonalPlane.normalX = -0.70710678f;
        diagonalPlane.normalY = 0.70710678f;
        diagonalPlane.planeDistance = 0.0f;
        system.registerSoftBodyCollider(layerId, diagonalPlane);

        system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 0.0f, false);
        const SoftBodySnapshot snapshot = softBody->snapshot();
        system.unregisterSoftBody(layerId);
        return snapshot;
    };

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    const SoftBodySnapshot fullSnapshot = simulate(full);

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    reduced.softBodyGridScale = 0.99f;
    const SoftBodySnapshot reducedSnapshot = simulate(reduced);
    system.setPhysicsTimelineSettings(previousTimeline);

    ASSERT_EQ(fullSnapshot.points.size(), 2u);
    ASSERT_EQ(reducedSnapshot.points.size(), fullSnapshot.points.size());
    const float fullX = fullSnapshot.points[0].x;
    const float fullY = fullSnapshot.points[0].y;
    const float reducedX = reducedSnapshot.points[0].x;
    const float reducedY = reducedSnapshot.points[0].y;
    EXPECT_GT(std::abs(fullX - reducedX) + std::abs(fullY - reducedY), 1.0e-3f)
        << "Reduced LOD should use one collider projection pass while Full "
           "uses its default two passes";
    EXPECT_NEAR(fullX, 0.75f, 1.0e-4f);
    EXPECT_NEAR(fullY, 0.75f, 1.0e-4f);
    EXPECT_NEAR(reducedX, 0.5f, 1.0e-4f);
    EXPECT_NEAR(reducedY, 0.5f, 1.0e-4f);
}

TEST(PhysicsLodSettingsContractTest, ReducedAndMinimalBudgetsReachFluidSolver)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId;
    auto fluid = system.createFluidSolver(layerId, 16, 16);

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_EQ(fluid->solverIterations(), 10);

    PhysicsLODSettings minimal;
    minimal.level = PhysicsLODLevel::Minimal;
    system.setPhysicsLODSettings(minimal);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_EQ(fluid->solverIterations(), 5);

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_EQ(fluid->solverIterations(), 20)
        << "returning to Full should restore the fluid solver's default iterations";

    system.unregisterFluidSolver(layerId);
    system.setPhysicsTimelineSettings(previousTimeline);
}

TEST(PhysicsLodSettingsContractTest, FluidResolutionLodChangesGridAndRestoresFullResolution)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId;
    auto fluid = system.createFluidSolver(layerId, 32, 24);
    ASSERT_NE(fluid.get(), nullptr);
    ASSERT_EQ(fluid->width(), 32);
    ASSERT_EQ(fluid->height(), 24);

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_EQ(fluid->width(), 16);
    EXPECT_EQ(fluid->height(), 12);

    PhysicsLODSettings minimal;
    minimal.level = PhysicsLODLevel::Minimal;
    system.setPhysicsLODSettings(minimal);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_EQ(fluid->width(), 8);
    EXPECT_EQ(fluid->height(), 6);

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_EQ(fluid->width(), 32);
    EXPECT_EQ(fluid->height(), 24);

    system.unregisterFluidSolver(layerId);
    system.setPhysicsTimelineSettings(previousTimeline);
}

TEST(PhysicsLodSettingsContractTest,
     ExplicitFluidResolutionScaleChangesGridIndependentlyOfPresetLevel)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId;
    auto fluid = system.createFluidSolver(layerId, 32, 24);
    ASSERT_NE(fluid.get(), nullptr);

    PhysicsLODSettings customScale;
    customScale.level = PhysicsLODLevel::Full;
    customScale.fluidResolutionScale = 0.75f;
    system.setPhysicsLODSettings(customScale);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_EQ(fluid->width(), 24)
        << "an explicit fluid resolution scale should apply even while the "
           "solver budget remains Full";
    EXPECT_EQ(fluid->height(), 18);

    customScale.fluidResolutionScale = 1.0f;
    system.setPhysicsLODSettings(customScale);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_EQ(fluid->width(), 32)
        << "restoring full fluid resolution should recover the original grid";
    EXPECT_EQ(fluid->height(), 24);

    system.unregisterFluidSolver(layerId);
    system.setPhysicsTimelineSettings(previousTimeline);
}

TEST(PhysicsLodSettingsContractTest,
     FluidResolutionChangesResampleFieldsAndPreserveSolverSettings)
{
    FluidSolver2D fluid(8, 6);
    fluid.setViscosity(0.003f);
    fluid.setDiffusion(0.004f);
    fluid.setBuoyancy(0.2f);
    fluid.setVorticity(0.4f);
    fluid.setSolverIterations(7);
    fluid.setAdaptiveIterations(false);
    fluid.setHighResThresholdCells(100);
    fluid.setMaxAdaptiveIterations(12);

    for (int y = 0; y < fluid.height(); ++y) {
        for (int x = 0; x < fluid.width(); ++x) {
            const float xf = static_cast<float>(x);
            const float yf = static_cast<float>(y);
            fluid.addDensity(x, y, xf + 10.0f * yf);
            fluid.addVelocity(x, y, 2.0f * xf - yf, xf - 3.0f * yf);
        }
    }

    fluid.setResolution(4, 4);
    EXPECT_EQ(fluid.width(), 4);
    EXPECT_EQ(fluid.height(), 4);
    constexpr int sourceX[] = {0, 2, 5, 7};
    constexpr int sourceY[] = {0, 2, 3, 5};
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            const float expectedDensity =
                static_cast<float>(sourceX[x] + 10 * sourceY[y]);
            EXPECT_FLOAT_EQ(fluid.getDensity(x, y), expectedDensity);
            float velocityX = 0.0f;
            float velocityY = 0.0f;
            fluid.getVelocity(x, y, velocityX, velocityY);
            EXPECT_FLOAT_EQ(velocityX,
                            2.0f * static_cast<float>(sourceX[x]) -
                                static_cast<float>(sourceY[y]));
            EXPECT_FLOAT_EQ(velocityY,
                            static_cast<float>(sourceX[x]) -
                                3.0f * static_cast<float>(sourceY[y]));
        }
    }

    fluid.setResolution(8, 6);
    EXPECT_EQ(fluid.width(), 8);
    EXPECT_EQ(fluid.height(), 6);
    const int cornerXs[] = {0, fluid.width() - 1};
    const int cornerYs[] = {0, fluid.height() - 1};
    for (const int y : cornerYs) {
        for (const int x : cornerXs) {
            const float xf = static_cast<float>(x);
            const float yf = static_cast<float>(y);
            EXPECT_FLOAT_EQ(fluid.getDensity(x, y), xf + 10.0f * yf)
                << "resolution LOD should preserve the domain corner density";
            float velocityX = 0.0f;
            float velocityY = 0.0f;
            fluid.getVelocity(x, y, velocityX, velocityY);
            EXPECT_FLOAT_EQ(velocityX, 2.0f * xf - yf)
                << "resolution LOD should preserve corner horizontal velocity";
            EXPECT_FLOAT_EQ(velocityY, xf - 3.0f * yf)
                << "resolution LOD should preserve corner vertical velocity";
        }
    }
    for (int y = 0; y < fluid.height(); ++y) {
        for (int x = 0; x < fluid.width(); ++x) {
            EXPECT_TRUE(std::isfinite(fluid.getDensity(x, y)));
            float velocityX = 0.0f;
            float velocityY = 0.0f;
            fluid.getVelocity(x, y, velocityX, velocityY);
            EXPECT_TRUE(std::isfinite(velocityX));
            EXPECT_TRUE(std::isfinite(velocityY));
        }
    }
    EXPECT_FLOAT_EQ(fluid.viscosity(), 0.003f);
    EXPECT_FLOAT_EQ(fluid.diffusion(), 0.004f);
    EXPECT_FLOAT_EQ(fluid.buoyancy(), 0.2f);
    EXPECT_FLOAT_EQ(fluid.vorticity(), 0.4f);
    EXPECT_EQ(fluid.solverIterations(), 7);
    EXPECT_FALSE(fluid.adaptiveIterations());
    EXPECT_EQ(fluid.highResThresholdCells(), 100);
    EXPECT_EQ(fluid.maxAdaptiveIterations(), 12);

    const FluidSnapshot2D resizedSnapshot = fluid.snapshot();
    EXPECT_EQ(resizedSnapshot.width, 8);
    EXPECT_EQ(resizedSnapshot.height, 6);
    EXPECT_TRUE(fluid.canRestoreSnapshot(resizedSnapshot));
}

TEST(PhysicsLodSettingsContractTest, ExplicitFluidIterationBudgetOverridesReducedPreset)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId;
    auto fluid = system.createFluidSolver(layerId, 12, 12);
    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    reduced.fluidSolverIterations = 17;
    system.setPhysicsLODSettings(reduced);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);

    EXPECT_EQ(fluid->solverIterations(), 17);

    system.unregisterFluidSolver(layerId);
    system.setPhysicsTimelineSettings(previousTimeline);
}

TEST(PhysicsLodSettingsContractTest, ReducedTargetRateAccumulatesDirectUpdateDeltas)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);

    const LayerID layerId;
    auto fluid = system.createFluidSolver(layerId, 12, 12);
    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);

    system.update(0.02f, 0.0f, 9.8f, false);
    EXPECT_EQ(fluid->solverIterations(), 20);
    system.update(0.02f, 0.0f, 9.8f, false);
    EXPECT_EQ(fluid->solverIterations(), 10);

    system.unregisterFluidSolver(layerId);
}

TEST(PhysicsLodSettingsContractTest,
     ReducedTargetRateGatesCloth3DUpdatesAtAccumulatedInterval)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const LayerID layerId;
    auto cloth = system.createCloth3DGrid(
        layerId, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 5, 5);
    ASSERT_TRUE(cloth);

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);
    const ClothSnapshot3D initial = cloth->snapshot();

    system.update(0.02f, 0.0f, 1800.0f, false);
    const ClothSnapshot3D beforeInterval = cloth->snapshot();
    ASSERT_EQ(beforeInterval.points.size(), initial.points.size());
    for (std::size_t index = 0; index < initial.points.size(); ++index) {
        EXPECT_FLOAT_EQ(beforeInterval.points[index].x, initial.points[index].x);
        EXPECT_FLOAT_EQ(beforeInterval.points[index].y, initial.points[index].y);
        EXPECT_FLOAT_EQ(beforeInterval.points[index].z, initial.points[index].z);
    }

    system.update(0.02f, 0.0f, 1800.0f, false);
    const ClothSnapshot3D afterInterval = cloth->snapshot();
    ASSERT_EQ(afterInterval.points.size(), initial.points.size());
    EXPECT_NE(afterInterval.points[12].y, initial.points[12].y)
        << "crossing the accumulated Reduced interval should advance cloth";

    system.unregisterCloth3D(layerId);
}

TEST(PhysicsLodSettingsContractTest,
     ReducedTargetRateGatesSoftBodyUpdatesAtAccumulatedInterval)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const LayerID layerId;
    auto softBody = system.createSoftBodyGrid(
        layerId, 0.0f, 0.0f, 10.0f, 10.0f, 5, 5, 1.0f, 1.0f, false);
    ASSERT_TRUE(softBody);
    softBody->setGravity(0.0f, 1800.0f);

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    reduced.softBodyGridScale = 0.999f;
    system.setPhysicsLODSettings(reduced);
    const SoftBodySnapshot initial = softBody->snapshot();

    system.update(0.02f, 0.0f, 9.8f, false);
    const SoftBodySnapshot beforeInterval = softBody->snapshot();
    ASSERT_EQ(beforeInterval.points.size(), initial.points.size());
    EXPECT_FLOAT_EQ(beforeInterval.points[12].x, initial.points[12].x);
    EXPECT_FLOAT_EQ(beforeInterval.points[12].y, initial.points[12].y);
    EXPECT_FLOAT_EQ(beforeInterval.points[12].prevY, initial.points[12].prevY);

    system.update(0.02f, 0.0f, 9.8f, false);
    const SoftBodySnapshot afterInterval = softBody->snapshot();
    ASSERT_EQ(afterInterval.points.size(), initial.points.size());
    EXPECT_NE(afterInterval.points[12].y, initial.points[12].y)
        << "crossing the accumulated Reduced interval should advance soft body";

    system.unregisterSoftBody(layerId);
}

TEST(PhysicsLodSettingsContractTest,
     ReturningThroughFullClearsPartialReducedIntervalBeforeResuming)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const LayerID layerId;
    auto fluid = system.createFluidSolver(layerId, 12, 12);
    ASSERT_TRUE(fluid);

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);
    system.update(0.02f, 0.0f, 9.8f, false);
    EXPECT_EQ(fluid->solverIterations(), 20)
        << "the partial Reduced interval should not update the solver";

    full.fluidSolverIterations = 7;
    system.setPhysicsLODSettings(full);
    system.update(0.001f, 0.0f, 9.8f, false);
    EXPECT_EQ(fluid->solverIterations(), 7)
        << "Full should update immediately and apply its explicit solver budget";

    system.setPhysicsLODSettings(reduced);
    system.update(0.02f, 0.0f, 9.8f, false);
    EXPECT_EQ(fluid->solverIterations(), 7)
        << "time accumulated before Full should not carry into the new Reduced interval";
    system.update(0.02f, 0.0f, 9.8f, false);
    EXPECT_EQ(fluid->solverIterations(), 10)
        << "new Reduced time should advance after a fresh interval is accumulated";

    system.unregisterFluidSolver(layerId);
}

TEST(PhysicsLodSettingsContractTest,
     ReducedTargetRateAdvancesOnlyAfterAccumulatedIntervalIsReached)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const LayerID layerId;
    auto fluid = system.createFluidSolver(layerId, 12, 12);
    ASSERT_TRUE(fluid);
    fluid->addDensity(6, 6, 1.0f);
    const FluidSnapshot2D initial = fluid->snapshot();

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);
    const float interval = 1.0f / system.physicsLODSettings().targetHz;
    system.update(interval - 1.0e-4f, 0.0f, 9.8f, false);

    EXPECT_EQ(fluid->solverIterations(), 20)
        << "a delta just below the Reduced interval should not run the solver";
    const FluidSnapshot2D beforeInterval = fluid->snapshot();
    EXPECT_EQ(beforeInterval.density, initial.density);

    system.update(0.0f, 0.0f, 9.8f, false);
    system.update(-1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_EQ(fluid->solverIterations(), 20)
        << "non-positive deltas should not advance the Reduced update interval";
    EXPECT_EQ(fluid->snapshot().density, initial.density);

    system.update(2.0e-4f, 0.0f, 9.8f, false);
    EXPECT_EQ(fluid->solverIterations(), 10)
        << "crossing the accumulated Reduced interval should run the solver";

    system.unregisterFluidSolver(layerId);
}

TEST(PhysicsLodSettingsContractTest,
     ReducedToMinimalCarriesElapsedTimeAcrossTheNewUpdateRate)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const LayerID layerId;
    auto fluid = system.createFluidSolver(layerId, 12, 12);
    ASSERT_TRUE(fluid);
    fluid->addDensity(6, 6, 1.0f);
    fluid->addVelocity(6, 6, 0.4f, -0.2f);
    const FluidSnapshot2D initial = fluid->snapshot();

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);
    system.update(0.02f, 0.0f, 9.8f, false);
    EXPECT_EQ(fluid->solverIterations(), 20)
        << "Reduced should retain its partial 30 Hz interval without stepping";

    PhysicsLODSettings minimal;
    minimal.level = PhysicsLODLevel::Minimal;
    system.setPhysicsLODSettings(minimal);
    system.update(0.02f, 0.0f, 9.8f, false);
    EXPECT_EQ(fluid->solverIterations(), 20)
        << "changing to the slower Minimal rate should not step before its interval";
    EXPECT_EQ(fluid->snapshot().density, initial.density);

    system.update(0.03f, 0.0f, 9.8f, false);
    EXPECT_EQ(fluid->solverIterations(), 5)
        << "elapsed time from both LOD levels should trigger one Minimal update";
    const FluidSnapshot2D afterInterval = fluid->snapshot();
    EXPECT_NE(afterInterval.density, initial.density);
    EXPECT_NE(afterInterval.velocityX, initial.velocityX);

    system.unregisterFluidSolver(layerId);
}

TEST(PhysicsLodSettingsContractTest,
     FrozenLODDiscardsPartialReducedRateBeforeReducedResumes)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);

    const LayerID layerId;
    auto fluid = system.createFluidSolver(layerId, 12, 12);
    ASSERT_TRUE(fluid);
    fluid->addDensity(6, 6, 1.0f);
    fluid->addVelocity(6, 6, 0.4f, -0.2f);
    const FluidSnapshot2D initial = fluid->snapshot();

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);
    system.update(0.02f, 0.0f, 9.8f, false);

    PhysicsLODSettings frozen;
    frozen.level = PhysicsLODLevel::Frozen;
    system.setPhysicsLODSettings(frozen);
    system.update(0.2f, 0.0f, 9.8f, false);

    system.setPhysicsLODSettings(reduced);
    system.update(0.02f, 0.0f, 9.8f, false);
    const FluidSnapshot2D afterFirstResumedDelta = fluid->snapshot();
    EXPECT_EQ(afterFirstResumedDelta.density, initial.density);
    EXPECT_EQ(afterFirstResumedDelta.velocityX, initial.velocityX);
    EXPECT_EQ(afterFirstResumedDelta.velocityY, initial.velocityY)
        << "partial pre-freeze time must not carry into the resumed Reduced interval";
    EXPECT_EQ(fluid->solverIterations(), 20)
        << "the first new partial delta should remain below the Reduced update interval";

    system.update(0.02f, 0.0f, 9.8f, false);
    EXPECT_EQ(fluid->solverIterations(), 10)
        << "new time after resuming Reduced should still advance the fluid solver";

    system.unregisterFluidSolver(layerId);
}

TEST(PhysicsLodSettingsContractTest, ReducedTargetRateCapsLargeFrameCatchUpToFourIntervals)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);

    const LayerID layerId;
    auto registeredFluid = system.createFluidSolver(layerId, 16, 16);
    FluidSolver2D referenceFluid(16, 16);
    registeredFluid->addDensity(8, 8, 1.0f);
    registeredFluid->addVelocity(8, 8, 0.4f, -0.2f);
    referenceFluid.addDensity(8, 8, 1.0f);
    referenceFluid.addVelocity(8, 8, 0.4f, -0.2f);

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);
    system.update(0.2f, 0.0f, 9.8f, false);

    const PhysicsLODSettings& appliedSettings = system.physicsLODSettings();
    referenceFluid.setSolverIterations(appliedSettings.fluidSolverIterations);
    referenceFluid.update(4.0f / appliedSettings.targetHz);

    const FluidSnapshot2D actual = registeredFluid->snapshot();
    const FluidSnapshot2D expected = referenceFluid.snapshot();
    ASSERT_EQ(actual.density.size(), expected.density.size());
    ASSERT_EQ(actual.velocityX.size(), expected.velocityX.size());
    ASSERT_EQ(actual.velocityY.size(), expected.velocityY.size());
    for (std::size_t index = 0; index < actual.density.size(); ++index) {
        EXPECT_NEAR(actual.density[index], expected.density[index], 1.0e-6f);
        EXPECT_NEAR(actual.velocityX[index], expected.velocityX[index], 1.0e-6f);
        EXPECT_NEAR(actual.velocityY[index], expected.velocityY[index], 1.0e-6f);
    }

    system.unregisterFluidSolver(layerId);
}

TEST(PhysicsLodSettingsContractTest,
     MinimalCollisionSimplificationRestoresMixedPolygonAndCircleShapes)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId;
    auto world = system.createRigidWorld(layerId);
    const std::vector<QVector2D> triangle{
        {-1.0f, -1.0f}, {1.0f, -1.0f}, {0.0f, 1.0f}};
    auto body = world->addPolygonBody(
        0.0f, 0.0f, triangle, true, 1.0f);
    ASSERT_TRUE(body);
    body->addCircleCollider(0.5f, 0.75f);
    const float originalMass = body->mass();

    PhysicsLODSettings minimal;
    minimal.level = PhysicsLODLevel::Minimal;
    system.setPhysicsLODSettings(minimal);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);

    EXPECT_GT(body->mass(), originalMass);

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_FLOAT_EQ(body->mass(), originalMass)
        << "returning to Full should restore the authored collision mesh";

    system.unregisterRigidWorld(layerId);
    system.setPhysicsTimelineSettings(previousTimeline);
}

TEST(PhysicsLodSettingsContractTest,
     ReturningToFullDiscardsPartialReducedUpdateInterval)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const LayerID layerId;
    auto fluid = system.createFluidSolver(layerId, 12, 12);
    FluidSolver2D fullReference(12, 12);
    fluid->addDensity(5, 6, 1.0f);
    fluid->addVelocity(5, 6, 0.4f, -0.2f);
    fullReference.addDensity(5, 6, 1.0f);
    fullReference.addVelocity(5, 6, 0.4f, -0.2f);
    const FluidSnapshot2D initialReference = fullReference.snapshot();

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    reduced.fluidSolverIterations = 20;
    system.setPhysicsLODSettings(reduced);
    const float fullFrameDelta = 1.0f / 60.0f;
    system.update(fullFrameDelta * 0.5f, 0.0f, 9.8f, false);
    const FluidSnapshot2D afterPartialReduced = fluid->snapshot();

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    system.update(fullFrameDelta, 0.0f, 9.8f, false);
    fullReference.update(fullFrameDelta);
    const FluidSnapshot2D actual = fluid->snapshot();
    const FluidSnapshot2D expected = fullReference.snapshot();

    ASSERT_EQ(actual.density.size(), expected.density.size());
    ASSERT_EQ(actual.velocityX.size(), expected.velocityX.size());
    ASSERT_EQ(actual.velocityY.size(), expected.velocityY.size());
    EXPECT_EQ(afterPartialReduced.density, initialReference.density)
        << "the half Reduced interval should not simulate fluid state";
    for (std::size_t index = 0; index < expected.density.size(); ++index) {
        EXPECT_NEAR(actual.density[index], expected.density[index], 1.0e-6f);
        EXPECT_NEAR(actual.velocityX[index], expected.velocityX[index], 1.0e-6f);
        EXPECT_NEAR(actual.velocityY[index], expected.velocityY[index], 1.0e-6f);
    }

    system.unregisterFluidSolver(layerId);
}

TEST(PhysicsLodSettingsContractTest, CollisionSimplificationIsIdempotentAcrossQualityTransitions)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId;
    auto world = system.createRigidWorld(layerId);
    const std::vector<QVector2D> triangle{
        {-1.0f, -1.0f}, {1.0f, -1.0f}, {0.0f, 1.0f}};
    auto body = world->addPolygonBody(
        0.0f, 0.0f, triangle, true, 1.0f);
    ASSERT_TRUE(body);
    body->addCircleCollider(0.5f, 0.75f);
    const float originalMass = body->mass();

    PhysicsLODSettings minimal;
    minimal.level = PhysicsLODLevel::Minimal;
    system.setPhysicsLODSettings(minimal);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    const float firstReducedMass = body->mass();
    EXPECT_GT(firstReducedMass, originalMass);

    // Reapplying the same reduced LOD must not keep adding backup polygons
    // or change the simplified collider's mass a second time.
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_FLOAT_EQ(body->mass(), firstReducedMass);

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_FLOAT_EQ(body->mass(), originalMass);

    system.setPhysicsLODSettings(minimal);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_FLOAT_EQ(body->mass(), firstReducedMass)
        << "a second simplify/restore cycle should reproduce the same collider mass";

    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_FLOAT_EQ(body->mass(), originalMass);

    system.unregisterRigidWorld(layerId);
    system.setPhysicsTimelineSettings(previousTimeline);
}

TEST(PhysicsLodSettingsContractTest, MinimalLevelDisablesFractureAndCapsMaterialSteps)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId;
    auto solver = system.createMaterialSolver(layerId);

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_TRUE(solver->fractureEnabled());
    EXPECT_EQ(solver->maxSubsteps(), 256);

    reduced.materialMaxSubSteps = 192;
    system.setPhysicsLODSettings(reduced);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_EQ(solver->maxSubsteps(), 192);

    PhysicsLODSettings minimal;
    minimal.level = PhysicsLODLevel::Minimal;
    system.setPhysicsLODSettings(minimal);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_FALSE(solver->fractureEnabled());
    EXPECT_EQ(solver->maxSubsteps(), 128);

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_TRUE(solver->fractureEnabled());
    EXPECT_EQ(solver->maxSubsteps(), 512)
        << "returning to Full should restore the material solver's default substeps";

    system.removeMaterialSolver(layerId);
    system.setPhysicsTimelineSettings(previousTimeline);
}

TEST(PhysicsLodSettingsContractTest, ReducedAndMinimalBudgetsReachCloth3D)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId;
    auto cloth = system.createCloth3DGrid(
        layerId, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 5, 5);
    ASSERT_TRUE(cloth);
    const auto expectOriginalClothTopology = [&] {
        EXPECT_EQ(cloth->gridColumns(), 5);
        EXPECT_EQ(cloth->gridRows(), 5);
        EXPECT_EQ(cloth->pointCount(), 25u);
        EXPECT_EQ(cloth->getGridTriangleIndices().size(), 96u);
    };
    expectOriginalClothTopology();

    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_EQ(cloth->maxSubsteps(), 4);
    expectOriginalClothTopology();

    PhysicsLODSettings minimal;
    minimal.level = PhysicsLODLevel::Minimal;
    system.setPhysicsLODSettings(minimal);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_EQ(cloth->maxSubsteps(), 2);
    expectOriginalClothTopology();

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 9.8f, false);
    EXPECT_EQ(cloth->maxSubsteps(), 8)
        << "returning to Full should restore the cloth solver's default substeps";
    expectOriginalClothTopology();

    system.unregisterCloth3D(layerId);
    system.setPhysicsTimelineSettings(previousTimeline);
}

TEST(PhysicsLodSettingsContractTest, ReducedConstraintBudgetChangesCloth3DSolve)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId;
    auto cloth = system.createCloth3DGrid(
        layerId, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 5, 5);
    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    reduced.softBodyGridScale = 0.99f;
    system.setPhysicsLODSettings(reduced);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 1800.0f, false);
    const float reducedY = cloth->point(12).y;

    system.unregisterCloth3D(layerId);
    auto fullCloth = system.createCloth3DGrid(
        layerId, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 5, 5);
    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 1800.0f, false);
    const float fullY = fullCloth->point(12).y;

    EXPECT_NE(reducedY, fullY)
        << "the reduced constraint iteration budget should change the same cloth grid's solve";
    EXPECT_EQ(cloth->gridColumns(), 5);
    EXPECT_EQ(fullCloth->gridColumns(), 5);

    system.unregisterCloth3D(layerId);
    system.setPhysicsTimelineSettings(previousTimeline);
}

TEST(PhysicsLodSettingsContractTest,
     ReturningFromReducedRestoresCloth3DConstraintIterations)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID transitionedLayerId;
    auto transitioned = system.createCloth3DGrid(
        transitionedLayerId, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 5, 5);
    PhysicsLODSettings reduced;
    reduced.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reduced);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 1800.0f, false);

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    const LayerID referenceLayerId;
    auto fullReference = system.createCloth3DGrid(
        referenceLayerId, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 5, 5);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 1800.0f, false);

    const ClothSnapshot3D transitionedSnapshot = transitioned->snapshot();
    const ClothSnapshot3D fullSnapshot = fullReference->snapshot();
    ASSERT_EQ(transitionedSnapshot.points.size(), fullSnapshot.points.size());
    float maximumPositionDelta = 0.0f;
    for (std::size_t index = 0; index < fullSnapshot.points.size(); ++index) {
        maximumPositionDelta = std::max(
            maximumPositionDelta,
            std::abs(transitionedSnapshot.points[index].x -
                     fullSnapshot.points[index].x));
        maximumPositionDelta = std::max(
            maximumPositionDelta,
            std::abs(transitionedSnapshot.points[index].y -
                     fullSnapshot.points[index].y));
    }
    EXPECT_LT(maximumPositionDelta, 1.0e-5f)
        << "a cloth returning from Reduced should match a fresh Full solver";

    system.unregisterCloth3D(transitionedLayerId);
    system.unregisterCloth3D(referenceLayerId);
    system.setPhysicsTimelineSettings(previousTimeline);
}

TEST(PhysicsLodSettingsContractTest, FrozenLevelDoesNotAdvanceRegisteredCloth3D)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const LayerID layerId;
    auto cloth = system.createCloth3DGrid(
        layerId, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 5, 5);
    const ClothSnapshot3D before = cloth->snapshot();

    PhysicsLODSettings frozen;
    frozen.level = PhysicsLODLevel::Frozen;
    system.setPhysicsLODSettings(frozen);
    system.advancePhysicsFrame(1.0f / 30.0f, 0.0f, 1800.0f, false);
    const ClothSnapshot3D after = cloth->snapshot();

    ASSERT_EQ(after.points.size(), before.points.size());
    for (std::size_t index = 0; index < before.points.size(); ++index) {
        EXPECT_FLOAT_EQ(after.points[index].x, before.points[index].x);
        EXPECT_FLOAT_EQ(after.points[index].y, before.points[index].y);
        EXPECT_FLOAT_EQ(after.points[index].z, before.points[index].z);
        EXPECT_FLOAT_EQ(after.points[index].prevX, before.points[index].prevX);
        EXPECT_FLOAT_EQ(after.points[index].prevY, before.points[index].prevY);
        EXPECT_FLOAT_EQ(after.points[index].prevZ, before.points[index].prevZ);
    }

    PhysicsLODSettings full;
    full.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(full);
    system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 1800.0f, false);
    const ClothSnapshot3D resumed = cloth->snapshot();
    ASSERT_EQ(resumed.points.size(), before.points.size());
    EXPECT_NE(resumed.points[12].y, before.points[12].y)
        << "returning from Frozen should resume cloth simulation";

    system.unregisterCloth3D(layerId);
    system.setPhysicsTimelineSettings(previousTimeline);
}

TEST(PhysicsLodSettingsContractTest, ReducedCollisionBudgetChangesCloth3DProjection)
{
    PhysicsLodSettingsScope scope;
    PhysicsSystem& system = scope.system();
    const PhysicsTimelineSettings previousTimeline =
        system.physicsTimelineSettings();
    PhysicsTimelineSettings timeline;
    timeline.fixedTimeStep = 1.0f / 60.0f;
    timeline.maxSubsteps = 1;
    system.setPhysicsTimelineSettings(timeline);

    const auto simulateCenter = [&](const PhysicsLODLevel level) {
        const LayerID layerId;
        auto cloth = system.createCloth3DGrid(
            layerId, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 5, 5);
        ClothCollider3D verticalPlane;
        verticalPlane.normalX = 1.0f;
        verticalPlane.normalY = 0.0f;
        verticalPlane.planeDistance = 6.0f;
        cloth->addCollider(verticalPlane);
        ClothCollider3D diagonalPlane;
        diagonalPlane.normalX = -0.70710678f;
        diagonalPlane.normalY = 0.70710678f;
        diagonalPlane.normalZ = 0.0f;
        diagonalPlane.planeDistance = 0.0f;
        cloth->addCollider(diagonalPlane);

        PhysicsLODSettings settings;
        settings.level = level;
        system.setPhysicsLODSettings(settings);
        system.advancePhysicsFrame(1.0f / 60.0f, 0.0f, 0.0f, false);
        const ClothPoint3D center = cloth->point(12);
        system.unregisterCloth3D(layerId);
        return center;
    };

    const ClothPoint3D reduced = simulateCenter(PhysicsLODLevel::Reduced);
    const ClothPoint3D full = simulateCenter(PhysicsLODLevel::Full);
    EXPECT_NE(reduced.x, full.x);
    EXPECT_NE(reduced.y, full.y);
    EXPECT_NEAR(reduced.x, 5.5f, 1.0e-5f);
    EXPECT_NEAR(reduced.y, 5.5f, 1.0e-5f);
    EXPECT_NEAR(full.x, 5.75f, 1.0e-5f);
    EXPECT_NEAR(full.y, 5.75f, 1.0e-5f);

    system.setPhysicsTimelineSettings(previousTimeline);
}
