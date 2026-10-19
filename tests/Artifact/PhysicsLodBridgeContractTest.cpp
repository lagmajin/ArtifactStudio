#include <gtest/gtest.h>

#include <QString>

#include <cmath>

import Artifact.Layer.PhysicsBridge;
import Physics.Fluid;
import Physics.MpmSolver2D;
import Physics.SoftBody;
import Physics.System;
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

TEST(PhysicsLodBridgeContractTest, FractureScalesFollowPhysicsLodPreset)
{
    PhysicsLodSettingsScope scope;
    PhysicsLODSettings settings;
    settings.level = PhysicsLODLevel::Full;
    scope.system().setPhysicsLODSettings(settings);

    auto fullScale = Artifact::LayerPhysicsBridge::fractureLodScale();
    EXPECT_FLOAT_EQ(fullScale.shards, 1.0f);
    EXPECT_FLOAT_EQ(fullScale.debris, 1.0f);

    settings = PhysicsLODSettings{};
    settings.level = PhysicsLODLevel::Reduced;
    scope.system().setPhysicsLODSettings(settings);
    const auto reducedScale = Artifact::LayerPhysicsBridge::fractureLodScale();
    EXPECT_FLOAT_EQ(reducedScale.shards, 1.0f);
    EXPECT_FLOAT_EQ(reducedScale.debris, 1.0f);

    settings = PhysicsLODSettings{};
    settings.level = PhysicsLODLevel::Minimal;
    scope.system().setPhysicsLODSettings(settings);
    const auto minimalScale = Artifact::LayerPhysicsBridge::fractureLodScale();
    EXPECT_FLOAT_EQ(minimalScale.shards, 0.5f);
    EXPECT_FLOAT_EQ(minimalScale.debris, 0.25f);

    settings = PhysicsLODSettings{};
    settings.level = PhysicsLODLevel::Reduced;
    scope.system().setPhysicsLODSettings(settings);
    const auto restoredReducedScale =
        Artifact::LayerPhysicsBridge::fractureLodScale();
    EXPECT_FLOAT_EQ(restoredReducedScale.shards, 1.0f);
    EXPECT_FLOAT_EQ(restoredReducedScale.debris, 1.0f);

    settings.level = PhysicsLODLevel::Full;
    scope.system().setPhysicsLODSettings(settings);
    const auto restoredFullScale = Artifact::LayerPhysicsBridge::fractureLodScale();
    EXPECT_FLOAT_EQ(restoredFullScale.shards, 1.0f);
    EXPECT_FLOAT_EQ(restoredFullScale.debris, 1.0f);
}

TEST(PhysicsLodBridgeContractTest, ExplicitFractureScalesPassThroughTheBridge)
{
    PhysicsLodSettingsScope scope;
    PhysicsLODSettings settings;
    settings.level = PhysicsLODLevel::Reduced;
    settings.fractureShardScale = 0.7f;
    settings.fractureDebrisScale = 0.4f;
    scope.system().setPhysicsLODSettings(settings);

    const auto scale = Artifact::LayerPhysicsBridge::fractureLodScale();
    EXPECT_FLOAT_EQ(scale.shards, 0.7f);
    EXPECT_FLOAT_EQ(scale.debris, 0.4f);
}

TEST(PhysicsLodBridgeContractTest,
     ReducedAndMinimalPresetsScaleSimulationBudgetsAndAllowOverrides)
{
    PhysicsLodSettingsScope scope;
    PhysicsLODSettings settings;
    settings.level = PhysicsLODLevel::Reduced;
    scope.system().setPhysicsLODSettings(settings);

    const auto& reduced = scope.system().physicsLODSettings();
    EXPECT_FLOAT_EQ(reduced.targetHz, 30.0f);
    EXPECT_EQ(reduced.rigidBodySubSteps, 2);
    EXPECT_EQ(reduced.softBodyMaxSubSteps, 4);
    EXPECT_EQ(reduced.softBodyConstraintIterations, 3);
    EXPECT_EQ(reduced.softBodyCollisionIterations, 1);
    EXPECT_FLOAT_EQ(reduced.softBodyGridScale, 0.75f);
    EXPECT_EQ(reduced.fluidSolverIterations, 10);
    EXPECT_FLOAT_EQ(reduced.fluidResolutionScale, 0.5f);
    EXPECT_EQ(reduced.materialMaxSubSteps, 256);
    EXPECT_TRUE(reduced.applySleepPolicy);
    EXPECT_FLOAT_EQ(reduced.sleepThreshold, 0.5f);

    settings = PhysicsLODSettings{};
    settings.level = PhysicsLODLevel::Minimal;
    settings.rigidBodySubSteps = 3;
    settings.fluidResolutionScale = 0.4f;
    scope.system().setPhysicsLODSettings(settings);

    const auto& minimal = scope.system().physicsLODSettings();
    EXPECT_FLOAT_EQ(minimal.targetHz, 15.0f);
    EXPECT_EQ(minimal.rigidBodySubSteps, 3)
        << "explicit substep budgets should override preset defaults";
    EXPECT_EQ(minimal.softBodyMaxSubSteps, 2);
    EXPECT_EQ(minimal.softBodyConstraintIterations, 1);
    EXPECT_EQ(minimal.softBodyCollisionIterations, 1);
    EXPECT_FLOAT_EQ(minimal.softBodyGridScale, 0.5f);
    EXPECT_EQ(minimal.fluidSolverIterations, 5);
    EXPECT_FLOAT_EQ(minimal.fluidResolutionScale, 0.4f)
        << "explicit fluid resolution should override preset defaults";
    EXPECT_EQ(minimal.materialMaxSubSteps, 128);
    EXPECT_TRUE(minimal.disableSoftBodySelfCollision);
    EXPECT_TRUE(minimal.disableContinuousCollision);
    EXPECT_TRUE(minimal.simplifyCollisionMesh);
    EXPECT_TRUE(minimal.disableFracture);
    EXPECT_TRUE(minimal.applySleepPolicy);
    EXPECT_FLOAT_EQ(minimal.sleepThreshold, 1.0f);
}

TEST(PhysicsLodBridgeContractTest,
     PhysicsLODClampsResolutionScalesAndNegativeBudgets)
{
    PhysicsLodSettingsScope scope;
    PhysicsLODSettings settings;
    settings.level = PhysicsLODLevel::Full;
    settings.targetHz = -5.0f;
    settings.rigidBodySubSteps = -1;
    settings.softBodyMaxSubSteps = -2;
    settings.softBodyConstraintIterations = -3;
    settings.softBodyCollisionIterations = -4;
    settings.softBodyGridScale = 0.1f;
    settings.fluidSolverIterations = -5;
    settings.fluidResolutionScale = 0.05f;
    settings.materialMaxSubSteps = -6;
    scope.system().setPhysicsLODSettings(settings);

    auto actual = scope.system().physicsLODSettings();
    EXPECT_FLOAT_EQ(actual.targetHz, 0.0f);
    EXPECT_EQ(actual.rigidBodySubSteps, 0);
    EXPECT_EQ(actual.softBodyMaxSubSteps, 0);
    EXPECT_EQ(actual.softBodyConstraintIterations, 0);
    EXPECT_EQ(actual.softBodyCollisionIterations, 0);
    EXPECT_FLOAT_EQ(actual.softBodyGridScale, 0.25f);
    EXPECT_EQ(actual.fluidSolverIterations, 0);
    EXPECT_FLOAT_EQ(actual.fluidResolutionScale, 0.125f);
    EXPECT_EQ(actual.materialMaxSubSteps, 0);

    settings.softBodyGridScale = 2.0f;
    settings.fluidResolutionScale = 2.0f;
    scope.system().setPhysicsLODSettings(settings);
    actual = scope.system().physicsLODSettings();
    EXPECT_FLOAT_EQ(actual.softBodyGridScale, 1.0f);
    EXPECT_FLOAT_EQ(actual.fluidResolutionScale, 1.0f);
}

TEST(PhysicsLodBridgeContractTest, FrozenLODStopsRegisteredMaterialSimulation)
{
    PhysicsLodSettingsScope scope;
    auto& system = scope.system();
    const LayerID layerId(QStringLiteral("physics-lod-frozen-probe"));
    struct MaterialSolverCleanup final {
        PhysicsSystem& system;
        LayerID layerId;
        ~MaterialSolverCleanup() { system.unregisterMaterialSolver(layerId); }
    } cleanup{system, layerId};

    auto solver = system.createMaterialGrid(
        layerId, 20.0f, 20.0f, 40.0f, 40.0f, 4, 4,
        MpmMaterialPreset::Flesh, 1000.0f);
    ASSERT_TRUE(solver);
    ASSERT_GT(solver->particleCount(), 0);
    solver->setGravity(0.0f, 980.0f);

    PhysicsLODSettings fullSettings;
    fullSettings.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(fullSettings);
    for (int step = 0; step < 4; ++step) {
        system.update(1.0f / 60.0f, 0.0f, 980.0f);
    }
    const MpmVec2 evolvedPosition = solver->particle(0).pos;
    const MpmVec2 evolvedVelocity = solver->particle(0).vel;
    ASSERT_TRUE(std::isfinite(evolvedPosition.x));
    ASSERT_TRUE(std::isfinite(evolvedPosition.y));
    ASSERT_NE(evolvedPosition.y, 20.0f)
        << "the probe material should evolve before testing Frozen LOD";

    PhysicsLODSettings frozenSettings;
    frozenSettings.level = PhysicsLODLevel::Frozen;
    system.setPhysicsLODSettings(frozenSettings);
    for (int step = 0; step < 12; ++step) {
        system.update(1.0f / 30.0f, 0.0f, 980.0f);
    }

    EXPECT_FLOAT_EQ(solver->particle(0).pos.x, evolvedPosition.x);
    EXPECT_FLOAT_EQ(solver->particle(0).pos.y, evolvedPosition.y);
    EXPECT_FLOAT_EQ(solver->particle(0).vel.x, evolvedVelocity.x);
    EXPECT_FLOAT_EQ(solver->particle(0).vel.y, evolvedVelocity.y);
}

TEST(PhysicsLodBridgeContractTest,
     ReducedAndMinimalLODApplyMaterialSubstepBudgetsToRegisteredSolver)
{
    PhysicsLodSettingsScope scope;
    auto& system = scope.system();
    const LayerID layerId(QStringLiteral("physics-lod-material-budget-probe"));
    struct MaterialSolverCleanup final {
        PhysicsSystem& system;
        LayerID layerId;
        ~MaterialSolverCleanup() { system.unregisterMaterialSolver(layerId); }
    } cleanup{system, layerId};

    auto solver = system.createMaterialGrid(
        layerId, 20.0f, 20.0f, 40.0f, 40.0f, 2, 2,
        MpmMaterialPreset::Flesh, 1000.0f);
    ASSERT_TRUE(solver);
    EXPECT_EQ(solver->maxSubsteps(), 512);

    PhysicsLODSettings reducedSettings;
    reducedSettings.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reducedSettings);
    system.update(1.0f / 15.0f, 0.0f, 0.0f);
    EXPECT_EQ(solver->maxSubsteps(), 256)
        << "Reduced LOD should cap the registered material solver at its preset budget";

    PhysicsLODSettings minimalSettings;
    minimalSettings.level = PhysicsLODLevel::Minimal;
    system.setPhysicsLODSettings(minimalSettings);
    system.update(1.0f / 15.0f, 0.0f, 0.0f);
    EXPECT_EQ(solver->maxSubsteps(), 128)
        << "Minimal LOD should apply the lower material solver budget";
}

TEST(PhysicsLodBridgeContractTest,
     ReducedTargetHzDefersMaterialSimulationUntilItsUpdateInterval)
{
    PhysicsLodSettingsScope scope;
    auto& system = scope.system();
    const LayerID layerId(QStringLiteral("physics-lod-target-hz-probe"));
    struct MaterialSolverCleanup final {
        PhysicsSystem& system;
        LayerID layerId;
        ~MaterialSolverCleanup() { system.unregisterMaterialSolver(layerId); }
    } cleanup{system, layerId};

    auto solver = system.createMaterialGrid(
        layerId, 20.0f, 20.0f, 40.0f, 40.0f, 4, 4,
        MpmMaterialPreset::Flesh, 1000.0f);
    ASSERT_TRUE(solver);
    solver->setGravity(0.0f, 980.0f);
    const float initialY = solver->particle(0).pos.y;
    ASSERT_TRUE(std::isfinite(initialY));

    PhysicsLODSettings fullSettings;
    fullSettings.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(fullSettings);
    PhysicsLODSettings reducedSettings;
    reducedSettings.level = PhysicsLODLevel::Reduced;
    reducedSettings.targetHz = 30.0f;
    system.setPhysicsLODSettings(reducedSettings);

    constexpr float smallFrameDelta = 0.005f;
    for (int step = 0; step < 6; ++step) {
        system.update(smallFrameDelta, 0.0f, 980.0f);
    }
    EXPECT_FLOAT_EQ(solver->particle(0).pos.y, initialY)
        << "the solver should not advance before the 30 Hz interval elapses";

    system.update(0.01f, 0.0f, 980.0f);
    EXPECT_NE(solver->particle(0).pos.y, initialY)
        << "the accumulated simulation should advance after crossing the interval";
}

TEST(PhysicsLodBridgeContractTest,
     UnknownPhysicsLODLevelFallsBackToFullMaterialSimulation)
{
    PhysicsLodSettingsScope scope;
    auto& system = scope.system();
    const LayerID layerId(QStringLiteral("physics-lod-unknown-probe"));
    struct MaterialSolverCleanup final {
        PhysicsSystem& system;
        LayerID layerId;
        ~MaterialSolverCleanup() { system.unregisterMaterialSolver(layerId); }
    } cleanup{system, layerId};

    auto solver = system.createMaterialGrid(
        layerId, 20.0f, 20.0f, 40.0f, 40.0f, 4, 4,
        MpmMaterialPreset::Flesh, 1000.0f);
    ASSERT_TRUE(solver);
    solver->setGravity(0.0f, 980.0f);
    const float initialY = solver->particle(0).pos.y;

    PhysicsLODSettings unknownSettings;
    unknownSettings.level = static_cast<PhysicsLODLevel>(255);
    system.setPhysicsLODSettings(unknownSettings);
    system.update(1.0f / 60.0f, 0.0f, 980.0f);

    EXPECT_NE(solver->particle(0).pos.y, initialY)
        << "an unrecognized level should not accidentally freeze simulation";
    const auto fractureScale = Artifact::LayerPhysicsBridge::fractureLodScale();
    EXPECT_FLOAT_EQ(fractureScale.shards, 1.0f);
    EXPECT_FLOAT_EQ(fractureScale.debris, 1.0f);
}

TEST(PhysicsLodBridgeContractTest,
     ReducedAndMinimalLODApplyFluidIterationBudgetsToRegisteredSolver)
{
    PhysicsLodSettingsScope scope;
    auto& system = scope.system();
    const LayerID layerId(QStringLiteral("physics-lod-fluid-budget-probe"));
    struct FluidSolverCleanup final {
        PhysicsSystem& system;
        LayerID layerId;
        ~FluidSolverCleanup() { system.unregisterFluidSolver(layerId); }
    } cleanup{system, layerId};

    auto solver = system.createFluidSolver(layerId, 24, 24);
    ASSERT_TRUE(solver);
    EXPECT_EQ(solver->solverIterations(), 20);

    PhysicsLODSettings reducedSettings;
    reducedSettings.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reducedSettings);
    system.update(1.0f / 15.0f, 0.0f, 0.0f);
    EXPECT_EQ(solver->solverIterations(), 10)
        << "Reduced LOD should lower the registered fluid solver iteration budget";

    PhysicsLODSettings minimalSettings;
    minimalSettings.level = PhysicsLODLevel::Minimal;
    system.setPhysicsLODSettings(minimalSettings);
    system.update(1.0f / 15.0f, 0.0f, 0.0f);
    EXPECT_EQ(solver->solverIterations(), 5)
        << "Minimal LOD should apply the smallest preset fluid iteration budget";

    PhysicsLODSettings oversizedSettings;
    oversizedSettings.level = PhysicsLODLevel::Full;
    oversizedSettings.fluidSolverIterations = 100;
    system.setPhysicsLODSettings(oversizedSettings);
    system.update(1.0f / 60.0f, 0.0f, 0.0f);
    EXPECT_EQ(solver->solverIterations(), 12)
        << "the fluid solver should clamp an explicit LOD budget to its maximum";
}

TEST(PhysicsLodBridgeContractTest,
     SoftBodyGridLODReducesFromOriginalResolutionAndRestoresFull)
{
    PhysicsLodSettingsScope scope;
    auto& system = scope.system();
    const LayerID layerId(QStringLiteral("physics-lod-soft-body-grid-probe"));
    struct SoftBodyCleanup final {
        PhysicsSystem& system;
        LayerID layerId;
        ~SoftBodyCleanup() { system.unregisterSoftBody(layerId); }
    } cleanup{system, layerId};
    const LayerID tinyLayerId(
        QStringLiteral("physics-lod-soft-body-tiny-grid-probe"));
    SoftBodyCleanup tinyCleanup{system, tinyLayerId};

    auto solver = system.createSoftBodyGrid(
        layerId, 0.0f, 0.0f, 80.0f, 80.0f, 9, 9);
    auto tinySolver = system.createSoftBodyGrid(
        tinyLayerId, 0.0f, 0.0f, 20.0f, 20.0f, 3, 3);
    ASSERT_TRUE(solver);
    ASSERT_TRUE(tinySolver);
    ASSERT_EQ(solver->gridColumns(), 9);
    ASSERT_EQ(solver->gridRows(), 9);
    ASSERT_EQ(solver->pointCount(), 81u);
    ASSERT_EQ(tinySolver->pointCount(), 9u);

    PhysicsLODSettings fullSettings;
    fullSettings.level = PhysicsLODLevel::Full;
    system.setPhysicsLODSettings(fullSettings);
    system.update(1.0f / 15.0f, 0.0f, 0.0f);

    PhysicsLODSettings reducedSettings;
    reducedSettings.level = PhysicsLODLevel::Reduced;
    system.setPhysicsLODSettings(reducedSettings);
    system.update(1.0f / 15.0f, 0.0f, 0.0f);
    EXPECT_EQ(solver->gridColumns(), 7);
    EXPECT_EQ(solver->gridRows(), 7);
    EXPECT_EQ(solver->pointCount(), 49u);
    EXPECT_EQ(solver->maxSubsteps(), 4);

    PhysicsLODSettings minimalSettings;
    minimalSettings.level = PhysicsLODLevel::Minimal;
    system.setPhysicsLODSettings(minimalSettings);
    system.update(1.0f / 15.0f, 0.0f, 0.0f);
    EXPECT_EQ(solver->gridColumns(), 5)
        << "Minimal should resample the original grid, not compound Reduced";
    EXPECT_EQ(solver->gridRows(), 5);
    EXPECT_EQ(solver->pointCount(), 25u);
    EXPECT_EQ(solver->maxSubsteps(), 2);
    EXPECT_EQ(tinySolver->gridColumns(), 2)
        << "Minimal LOD should retain the solver's two-point-per-axis floor";
    EXPECT_EQ(tinySolver->gridRows(), 2);
    EXPECT_EQ(tinySolver->pointCount(), 4u);

    system.setPhysicsLODSettings(fullSettings);
    system.update(1.0f / 15.0f, 0.0f, 0.0f);
    EXPECT_EQ(solver->gridColumns(), 9);
    EXPECT_EQ(solver->gridRows(), 9);
    EXPECT_EQ(solver->pointCount(), 81u);
    EXPECT_EQ(tinySolver->gridColumns(), 3);
    EXPECT_EQ(tinySolver->gridRows(), 3);
    EXPECT_EQ(tinySolver->pointCount(), 9u);

    PhysicsLODSettings oversizedBudgetSettings;
    oversizedBudgetSettings.level = PhysicsLODLevel::Full;
    oversizedBudgetSettings.softBodyMaxSubSteps = 100;
    system.setPhysicsLODSettings(oversizedBudgetSettings);
    system.update(1.0f / 60.0f, 0.0f, 0.0f);
    EXPECT_EQ(solver->maxSubsteps(), 32)
        << "the soft-body solver should clamp an explicit LOD budget to its maximum";
}

