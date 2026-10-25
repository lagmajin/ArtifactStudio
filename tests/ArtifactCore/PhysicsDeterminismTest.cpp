#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

import Physics.SoftBody;
import Physics.Fluid;
import Physics.SandSim2D;
import Container.NamedVector;

using namespace ArtifactCore;

namespace {

void expectSameSoftBodyState(const SoftBodySnapshot& a, const SoftBodySnapshot& b)
{
    ASSERT_EQ(a.points.size(), b.points.size());
    for (std::size_t i = 0; i < a.points.size(); ++i) {
        EXPECT_EQ(a.points[i].x, b.points[i].x);
        EXPECT_EQ(a.points[i].y, b.points[i].y);
        EXPECT_EQ(a.points[i].prevX, b.points[i].prevX);
        EXPECT_EQ(a.points[i].prevY, b.points[i].prevY);
    }
    ASSERT_EQ(a.constraints.size(), b.constraints.size());
}

void buildSameCloth(SoftBodySolver& solver)
{
    solver.buildGrid(0.0f, 0.0f, 100.0f, 80.0f, 8, 6);
}

void configureLiquidSolver(LiquidSolver2D& solver)
{
    solver.setGravity(0.15f, 1.8f);
    solver.setViscosity(0.12f);
    solver.setSurfaceTension(0.25f);
    solver.setSubsteps(3);
    solver.setSolverIterations(4);
}

void expectSameLiquidState(const LiquidSnapshot2D& a,
                           const LiquidSnapshot2D& b)
{
    ASSERT_EQ(a.particles.size(), b.particles.size());
    for (std::size_t i = 0; i < a.particles.size(); ++i) {
        EXPECT_EQ(a.particles[i].x, b.particles[i].x);
        EXPECT_EQ(a.particles[i].y, b.particles[i].y);
        EXPECT_EQ(a.particles[i].vx, b.particles[i].vx);
        EXPECT_EQ(a.particles[i].vy, b.particles[i].vy);
        EXPECT_EQ(a.particles[i].collisionImpact,
                  b.particles[i].collisionImpact);
    }
}

void expectSameSurfaceSnapshot(const LiquidSurfaceSnapshot2D& a,
                               const LiquidSurfaceSnapshot2D& b)
{
    ASSERT_EQ(a.triangles.size(), b.triangles.size());
    for (std::size_t i = 0; i < a.triangles.size(); ++i) {
        EXPECT_EQ(a.triangles[i].a.x, b.triangles[i].a.x);
        EXPECT_EQ(a.triangles[i].a.y, b.triangles[i].a.y);
        EXPECT_EQ(a.triangles[i].b.x, b.triangles[i].b.x);
        EXPECT_EQ(a.triangles[i].b.y, b.triangles[i].b.y);
        EXPECT_EQ(a.triangles[i].c.x, b.triangles[i].c.x);
        EXPECT_EQ(a.triangles[i].c.y, b.triangles[i].c.y);
        EXPECT_EQ(a.triangles[i].thickness, b.triangles[i].thickness);
        EXPECT_EQ(a.triangles[i].coverageA, b.triangles[i].coverageA);
        EXPECT_EQ(a.triangles[i].coverageB, b.triangles[i].coverageB);
        EXPECT_EQ(a.triangles[i].coverageC, b.triangles[i].coverageC);
    }
    ASSERT_EQ(a.contourSegments.size(), b.contourSegments.size());
    for (std::size_t i = 0; i < a.contourSegments.size(); ++i) {
        EXPECT_EQ(a.contourSegments[i].a.x, b.contourSegments[i].a.x);
        EXPECT_EQ(a.contourSegments[i].a.y, b.contourSegments[i].a.y);
        EXPECT_EQ(a.contourSegments[i].b.x, b.contourSegments[i].b.x);
        EXPECT_EQ(a.contourSegments[i].b.y, b.contourSegments[i].b.y);
    }
    ASSERT_EQ(a.foamPoints.size(), b.foamPoints.size());
    for (std::size_t i = 0; i < a.foamPoints.size(); ++i) {
        EXPECT_EQ(a.foamPoints[i].position.x, b.foamPoints[i].position.x);
        EXPECT_EQ(a.foamPoints[i].position.y, b.foamPoints[i].position.y);
        EXPECT_EQ(a.foamPoints[i].size, b.foamPoints[i].size);
        EXPECT_EQ(a.foamPoints[i].alpha, b.foamPoints[i].alpha);
        EXPECT_EQ(a.foamPoints[i].vx, b.foamPoints[i].vx);
        EXPECT_EQ(a.foamPoints[i].vy, b.foamPoints[i].vy);
    }
}

}

TEST(SoftBodyDeterminismTest, IdenticalInputsProduceIdenticalStates)
{
    SoftBodySolver a;
    SoftBodySolver b;
    buildSameCloth(a);
    buildSameCloth(b);

    for (int step = 0; step < 60; ++step) {
        a.update(1.0f / 30.0f, 0.0f, 900.0f);
        b.update(1.0f / 30.0f, 0.0f, 900.0f);
    }

    expectSameSoftBodyState(a.snapshot(), b.snapshot());
}

TEST(SoftBodyDeterminismTest, LodConstraintIterationsSurviveUpdate)
{
    // Regression: PhysicsSystem applies LOD constraint iterations via
    // setConstraintIterations(), then calls update() without an iteration
    // count. update() must keep the configured value instead of resetting
    // it, so both paths below must stay bit-identical.
    SoftBodySolver configured;
    SoftBodySolver explicitIterations;
    buildSameCloth(configured);
    buildSameCloth(explicitIterations);
    configured.setConstraintIterations(16);

    for (int step = 0; step < 30; ++step) {
        configured.update(1.0f / 30.0f, 0.0f, 900.0f);
        explicitIterations.update(1.0f / 30.0f, 0.0f, 900.0f, 16);
    }

    expectSameSoftBodyState(configured.snapshot(), explicitIterations.snapshot());
}

TEST(SoftBodyDeterminismTest, DefaultIterationsStayAtFive)
{
    SoftBodySolver implicitDefault;
    SoftBodySolver explicitDefault;
    buildSameCloth(implicitDefault);
    buildSameCloth(explicitDefault);

    for (int step = 0; step < 30; ++step) {
        implicitDefault.update(1.0f / 30.0f, 0.0f, 900.0f);
        explicitDefault.update(1.0f / 30.0f, 0.0f, 900.0f, 5);
    }

    expectSameSoftBodyState(implicitDefault.snapshot(), explicitDefault.snapshot());
}

TEST(SoftBodyFallTest, AuthoredGravityDrivesUnpinnedPoints)
{
    SoftBodySolver solver;
    solver.addPoint(0.0f, 0.0f);
    solver.setGravity(0.0f, 980.0f);

    // Use the configured-force overload: PhysicsSystem must not replace this
    // per-layer gravity with its legacy global default.
    solver.update(1.0f / 30.0f);

    EXPECT_GT(solver.point(0).y, 0.0f);
}

TEST(SoftBodyFallTest, AirDragReducesFreeFallVelocity)
{
    SoftBodySnapshot moving;
    moving.points.push_back({0.0f, 0.0f, 0.0f, -60.0f, 1.0f, false});

    SoftBodySolver withoutDrag;
    SoftBodySolver withDrag;
    ASSERT_TRUE(withoutDrag.restoreSnapshot(moving));
    ASSERT_TRUE(withDrag.restoreSnapshot(moving));
    withoutDrag.setGravity(0.0f, 0.0f);
    withDrag.setGravity(0.0f, 0.0f);
    withDrag.setLinearDamping(12.0f);

    withoutDrag.update(1.0f / 60.0f);
    withDrag.update(1.0f / 60.0f);

    EXPECT_LT(withDrag.point(0).y, withoutDrag.point(0).y);
}

TEST(SoftBodySnapshotTest, RejectsOutOfRangeConstraintWithoutMutatingState)
{
    SoftBodySolver solver;
    solver.addPoint(2.0f, 3.0f, 1.0f, true);
    const SoftBodySnapshot before = solver.snapshot();

    SoftBodySnapshot invalid = before;
    invalid.points[0].x = 99.0f;
    invalid.constraints.push_back({0, 1, 1.0f, 1.0f});

    EXPECT_FALSE(solver.canRestoreSnapshot(invalid));
    EXPECT_FALSE(solver.restoreSnapshot(invalid));
    const SoftBodySnapshot after = solver.snapshot();
    ASSERT_EQ(after.points.size(), before.points.size());
    EXPECT_FLOAT_EQ(after.points[0].x, before.points[0].x);
    EXPECT_FLOAT_EQ(after.points[0].y, before.points[0].y);
    EXPECT_TRUE(after.points[0].isPinned);
    EXPECT_TRUE(after.constraints.empty());
}

TEST(SoftBodySnapshotTest, RejectsInvalidGridShapeWithoutMutatingState)
{
    SoftBodySolver solver;
    solver.addPoint(2.0f, 3.0f, 1.0f, true);
    const SoftBodySnapshot before = solver.snapshot();

    SoftBodySnapshot negativeDimension = before;
    negativeDimension.gridColumns = -1;
    EXPECT_FALSE(solver.canRestoreSnapshot(negativeDimension));
    EXPECT_FALSE(solver.restoreSnapshot(negativeDimension));

    SoftBodySnapshot mismatchedPointCount = before;
    mismatchedPointCount.gridColumns = 2;
    mismatchedPointCount.gridRows = 2;
    EXPECT_FALSE(solver.canRestoreSnapshot(mismatchedPointCount));
    EXPECT_FALSE(solver.restoreSnapshot(mismatchedPointCount));

    const SoftBodySnapshot after = solver.snapshot();
    ASSERT_EQ(after.points.size(), before.points.size());
    EXPECT_FLOAT_EQ(after.points[0].x, before.points[0].x);
    EXPECT_FLOAT_EQ(after.points[0].y, before.points[0].y);
    EXPECT_TRUE(after.points[0].isPinned);
    EXPECT_EQ(after.gridColumns, before.gridColumns);
    EXPECT_EQ(after.gridRows, before.gridRows);
}

TEST(SoftBodySnapshotTest, RestoresGridAndWindStateAndClampsAccumulatedTime)
{
    SoftBodySolver source;
    source.buildGrid(1.0f, 2.0f, 6.0f, 8.0f, 3, 2);
    SoftBodySnapshot checkpoint = source.snapshot();
    checkpoint.wind.directionX = -0.25f;
    checkpoint.wind.directionY = 0.75f;
    checkpoint.turbulenceTime = 2.5f;
    checkpoint.accumulatedTime = 1.0f;

    SoftBodySolver restored;
    restored.setFixedTimeStep(0.02f);
    restored.setMaxSubsteps(3);
    ASSERT_TRUE(restored.canRestoreSnapshot(checkpoint));
    ASSERT_TRUE(restored.restoreSnapshot(checkpoint));

    const SoftBodySnapshot result = restored.snapshot();
    EXPECT_EQ(result.gridColumns, checkpoint.gridColumns);
    EXPECT_EQ(result.gridRows, checkpoint.gridRows);
    EXPECT_EQ(result.points.size(), checkpoint.points.size());
    EXPECT_EQ(result.constraints.size(), checkpoint.constraints.size());
    EXPECT_FLOAT_EQ(result.wind.directionX, checkpoint.wind.directionX);
    EXPECT_FLOAT_EQ(result.wind.directionY, checkpoint.wind.directionY);
    EXPECT_FLOAT_EQ(result.turbulenceTime, checkpoint.turbulenceTime);
    EXPECT_FLOAT_EQ(result.accumulatedTime, 0.06f);
}

TEST(FluidDeterminismTest, IdenticalInputsProduceIdenticalDensity)
{
    FluidSolver2D a(32, 32);
    FluidSolver2D b(32, 32);

    for (int step = 0; step < 30; ++step) {
        for (auto* solver : { &a, &b }) {
            solver->addDensity(8, 8, 1.0f);
            solver->addVelocity(8, 8, 0.5f, -0.25f);
            solver->update(1.0f / 30.0f);
        }
    }

    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            EXPECT_EQ(a.getDensity(x, y), b.getDensity(x, y));
        }
    }
}

TEST(FluidDeterminismTest, ZeroDeltaIsNoOpAndLargeDeltaStaysFinite)
{
    FluidSolver2D solver(32, 32);
    solver.addDensity(16, 16, 1.0f);
    solver.addVelocity(16, 16, 1.0f, 1.0f);

    const float densityBefore = solver.getDensity(16, 16);
    solver.update(0.0f);
    EXPECT_EQ(solver.getDensity(16, 16), densityBefore);

    solver.update(1000.0f);
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            float vx = 0.0f;
            float vy = 0.0f;
            solver.getVelocity(x, y, vx, vy);
            EXPECT_TRUE(std::isfinite(vx));
            EXPECT_TRUE(std::isfinite(vy));
            EXPECT_TRUE(std::isfinite(solver.getDensity(x, y)));
        }
    }
}

TEST(FluidSnapshotTest, RestoreReplaysDensityAndVelocityExactly)
{
    FluidSolver2D uninterrupted(24, 20);
    FluidSolver2D restored(24, 20);

    for (int step = 0; step < 18; ++step) {
        uninterrupted.addDensity(8 + step % 3, 9, 0.75f);
        uninterrupted.addVelocity(8 + step % 3, 9, 0.2f, -0.1f);
        uninterrupted.update(1.0f / 30.0f);
    }

    const FluidSnapshot2D checkpoint = uninterrupted.snapshot();
    ASSERT_TRUE(restored.canRestoreSnapshot(checkpoint));
    ASSERT_TRUE(restored.restoreSnapshot(checkpoint));

    for (int step = 0; step < 24; ++step) {
        for (FluidSolver2D* solver : {&uninterrupted, &restored}) {
            solver->addDensity(12, 5 + step % 4, 0.25f);
            solver->addVelocity(12, 5 + step % 4, -0.15f, 0.3f);
            solver->update(1.0f / 24.0f);
        }
    }

    for (int y = 0; y < uninterrupted.height(); ++y) {
        for (int x = 0; x < uninterrupted.width(); ++x) {
            EXPECT_EQ(uninterrupted.getDensity(x, y),
                      restored.getDensity(x, y));
            float uninterruptedVx = 0.0f;
            float uninterruptedVy = 0.0f;
            float restoredVx = 0.0f;
            float restoredVy = 0.0f;
            uninterrupted.getVelocity(x, y, uninterruptedVx, uninterruptedVy);
            restored.getVelocity(x, y, restoredVx, restoredVy);
            EXPECT_EQ(uninterruptedVx, restoredVx);
            EXPECT_EQ(uninterruptedVy, restoredVy);
        }
    }
}

TEST(FluidSnapshotTest, RejectsMismatchedResolutionWithoutMutation)
{
    FluidSolver2D solver(16, 12);
    solver.addDensity(7, 5, 1.0f);
    solver.addVelocity(7, 5, 0.5f, -0.25f);
    const FluidSnapshot2D before = solver.snapshot();

    FluidSolver2D otherResolution(12, 16);
    FluidSnapshot2D incompatible = otherResolution.snapshot();

    EXPECT_FALSE(solver.canRestoreSnapshot(incompatible));
    EXPECT_FALSE(solver.restoreSnapshot(incompatible));
    EXPECT_EQ(solver.getDensity(7, 5), before.density[7 + 5 * 16]);
    float vx = 0.0f;
    float vy = 0.0f;
    solver.getVelocity(7, 5, vx, vy);
    EXPECT_EQ(vx, before.velocityX[7 + 5 * 16]);
    EXPECT_EQ(vy, before.velocityY[7 + 5 * 16]);
}

TEST(LiquidDeterminismTest, IdenticalInputsProduceIdenticalStates)
{
    LiquidSolver2D a;
    LiquidSolver2D b;
    configureLiquidSolver(a);
    configureLiquidSolver(b);
    a.reset(0.42f, 0.055f);
    b.reset(0.42f, 0.055f);

    for (int step = 0; step < 90; ++step) {
        if (step % 3 == 0) {
            EXPECT_EQ(a.emitFromOpening(3, 0.3f, 0.8f),
                      b.emitFromOpening(3, 0.3f, 0.8f));
        }
        a.update(1.0f / 30.0f);
        b.update(1.0f / 30.0f);
    }

    expectSameLiquidState(a.snapshot(), b.snapshot());
}

TEST(LiquidDeterminismTest, SnapshotRestoreReplaysExactly)
{
    LiquidSolver2D uninterrupted;
    LiquidSolver2D restored;
    configureLiquidSolver(uninterrupted);
    configureLiquidSolver(restored);
    uninterrupted.reset(0.35f, 0.06f);
    restored.reset(0.0f, 0.06f);

    for (int step = 0; step < 25; ++step) {
        uninterrupted.update(1.0f / 24.0f);
    }
    const LiquidSnapshot2D checkpoint = uninterrupted.snapshot();
    ASSERT_TRUE(restored.restore(checkpoint));

    for (int step = 0; step < 40; ++step) {
        if (step % 4 == 0) {
            EXPECT_EQ(uninterrupted.emitFromOpening(2, 0.2f, 0.6f),
                      restored.emitFromOpening(2, 0.2f, 0.6f));
        }
        uninterrupted.update(1.0f / 24.0f);
        restored.update(1.0f / 24.0f);
    }

    expectSameLiquidState(uninterrupted.snapshot(), restored.snapshot());
}

TEST(LiquidDeterminismTest, VelocityUsesTheLastSubstepDisplacement)
{
    LiquidSolver2D solver;
    solver.reset(0.0f, 0.1f);
    solver.setGravity(0.0f, 10.0f);
    solver.setViscosity(0.0f);
    solver.setSurfaceTension(0.0f);
    solver.setSubsteps(4);
    solver.setSolverIterations(1);
    LiquidSnapshot2D initial;
    initial.particles.assign({{0.5f, 0.5f, 0.0f, 0.0f}});
    ASSERT_TRUE(solver.restore(initial));

    solver.update(0.04f);

    ASSERT_EQ(solver.particles().size(), 1u);
    EXPECT_NEAR(solver.particles()[0].vy, 0.4f, 1.0e-5f)
        << "velocity should be reconstructed from the final substep, not the "
           "position at the beginning of the frame";
}

TEST(LiquidDeterminismTest, InvalidSnapshotIsRejectedWithoutMutation)
{
    LiquidSolver2D solver;
    solver.reset(0.25f, 0.055f);
    const LiquidSnapshot2D before = solver.snapshot();
    LiquidSnapshot2D invalid = before;
    ASSERT_FALSE(invalid.particles.empty());
    invalid.particles.front()->x = std::numeric_limits<float>::quiet_NaN();

    EXPECT_FALSE(solver.restore(invalid));
    expectSameLiquidState(before, solver.snapshot());
}

TEST(LiquidSurfaceTensionTest, PullsNearbySeparatedParticlesTogether)
{
    LiquidSnapshot2D initial;
    initial.particles.assign({
        {0.445f, 0.5f, 0.0f, 0.0f},
        {0.555f, 0.5f, 0.0f, 0.0f}});

    LiquidSolver2D withoutTension;
    LiquidSolver2D withTension;
    for (auto* solver : {&withoutTension, &withTension}) {
        solver->reset(0.0f, 0.1f);
        solver->setGravity(0.0f, 0.0f);
        solver->setViscosity(0.0f);
        solver->setSubsteps(1);
        solver->setSolverIterations(1);
        ASSERT_TRUE(solver->restore(initial));
    }
    withoutTension.setSurfaceTension(0.0f);
    withTension.setSurfaceTension(1.0f);

    withoutTension.update(1.0f / 30.0f);
    withTension.update(1.0f / 30.0f);

    const auto distance = [](const LiquidSolver2D& solver) {
        const auto& particles = solver.particles();
        return std::hypot(particles[1].x - particles[0].x,
                          particles[1].y - particles[0].y);
    };
    EXPECT_LT(distance(withTension), distance(withoutTension));
}

TEST(LiquidDensityConstraintTest, RelaxesOverCompressedParticleCluster)
{
    LiquidSnapshot2D initial;
    initial.particles.assign({
        {0.485f, 0.485f, 0.0f, 0.0f},
        {0.500f, 0.485f, 0.0f, 0.0f},
        {0.515f, 0.485f, 0.0f, 0.0f},
        {0.485f, 0.500f, 0.0f, 0.0f},
        {0.500f, 0.500f, 0.0f, 0.0f},
        {0.515f, 0.500f, 0.0f, 0.0f},
        {0.485f, 0.515f, 0.0f, 0.0f},
        {0.500f, 0.515f, 0.0f, 0.0f},
        {0.515f, 0.515f, 0.0f, 0.0f}});

    LiquidSolver2D solver;
    solver.reset(0.0f, 0.1f);
    solver.setGravity(0.0f, 0.0f);
    solver.setViscosity(0.0f);
    solver.setSurfaceTension(0.0f);
    solver.setSubsteps(1);
    solver.setSolverIterations(1);
    ASSERT_TRUE(solver.restore(initial));

    const auto meanSquaredPairDistance = [](const LiquidSolver2D& value) {
        const auto& particles = value.particles();
        double sum = 0.0;
        std::size_t pairCount = 0;
        for (std::size_t i = 0; i < particles.size(); ++i) {
            for (std::size_t j = i + 1; j < particles.size(); ++j) {
                const double dx = particles[i].x - particles[j].x;
                const double dy = particles[i].y - particles[j].y;
                sum += dx * dx + dy * dy;
                ++pairCount;
            }
        }
        return pairCount == 0 ? 0.0 : sum / static_cast<double>(pairCount);
    };
    const double before = meanSquaredPairDistance(solver);

    solver.update(1.0f / 30.0f);

    EXPECT_GT(meanSquaredPairDistance(solver), before);
    for (const auto& particle : solver.particles()) {
        EXPECT_TRUE(std::isfinite(particle.x));
        EXPECT_TRUE(std::isfinite(particle.y));
    }
}

TEST(LiquidInflowTest, OccupiedOpeningRejectsOverlappingParticles)
{
    LiquidSolver2D solver;
    solver.reset(0.0f, 0.055f);

    const std::size_t firstEmission =
        solver.emitFromOpening(128, 0.25f, 0.0f);
    const std::size_t blockedEmission =
        solver.emitFromOpening(128, 0.25f, 0.0f);

    EXPECT_GT(firstEmission, 0U);
    EXPECT_EQ(blockedEmission, 0U);
    EXPECT_EQ(solver.particles().size(), firstEmission);
}

TEST(LiquidInflowTest, PositionMovesSourceAlongOpening)
{
    LiquidSolver2D nearStart;
    LiquidSolver2D nearEnd;
    nearStart.reset(0.0f, 0.05f);
    nearEnd.reset(0.0f, 0.05f);

    ASSERT_EQ(nearStart.emitFromOpening(1, 0.0f, 0.5f, 0.2f), 1U);
    ASSERT_EQ(nearEnd.emitFromOpening(1, 0.0f, 0.5f, 0.8f), 1U);

    EXPECT_LT(nearStart.particles().front()->x,
              nearEnd.particles().front()->x);
    EXPECT_EQ(nearStart.particles().front()->y,
              nearEnd.particles().front()->y);
}

TEST(LiquidInflowTest, WideSourceAtEdgeStaysInsideOpeningSegment)
{
    LiquidSolver2D solver;
    solver.reset(0.0f, 0.05f);

    ASSERT_GT(solver.emitFromOpening(128, 1.0f, 0.5f, 0.0f), 0U);

    for (const auto& particle : solver.particles()) {
        EXPECT_GE(particle.x, 0.05f);
        EXPECT_LE(particle.x, 0.95f);
        EXPECT_GT(particle.y, 0.0f);
    }
}

TEST(LiquidInflowTest, NonFiniteControlsAreRejected)
{
    LiquidSolver2D solver;
    solver.reset(0.0f, 0.05f);
    const float nan = std::numeric_limits<float>::quiet_NaN();

    EXPECT_EQ(solver.emitFromOpening(1, nan, 0.5f, 0.5f), 0U);
    EXPECT_EQ(solver.emitFromOpening(1, 0.2f, nan, 0.5f), 0U);
    EXPECT_EQ(solver.emitFromOpening(1, 0.2f, 0.5f, nan), 0U);
    EXPECT_TRUE(solver.particles().empty());
}

TEST(LiquidInflowTest, PolygonOpeningEmitsInsideContainer)
{
    LiquidSolver2D solver;
    ASSERT_TRUE(solver.setContainerPolygon(
        NamedVector<LiquidContainerPoint2D>{
            ContainerName{"Test.LiquidInflowPolygon"},
            {{0.1f, 0.1f}, {0.9f, 0.1f}, {0.9f, 0.9f}, {0.1f, 0.9f}}},
        0));
    solver.reset(0.0f, 0.05f);

    const std::size_t emitted = solver.emitFromOpening(8, 0.5f, 1.0f);

    ASSERT_GT(emitted, 0U);
    for (const auto& particle : solver.particles()) {
        EXPECT_GT(particle.y, 0.1f);
        EXPECT_LT(particle.y, 0.9f);
        EXPECT_GT(particle.x, 0.1f);
        EXPECT_LT(particle.x, 0.9f);
        EXPECT_GT(particle.vy, 0.0f);
    }
}

TEST(LiquidOpeningTest, ManualSideEdgeControlsInflowAndEscape)
{
    LiquidSolver2D solver;
    ASSERT_TRUE(solver.setContainerPolygon(
        NamedVector<LiquidContainerPoint2D>{
            ContainerName{"Test.LiquidOpeningPolygon"},
            {{0.1f, 0.1f}, {0.9f, 0.1f}, {0.9f, 0.9f}, {0.1f, 0.9f}}},
        1));
    solver.reset(0.0f, 0.05f);

    ASSERT_GT(solver.emitFromOpening(4, 0.4f, 1.0f), 0U);
    for (const auto& particle : solver.particles()) {
        EXPECT_LT(particle.vx, 0.0f);
        EXPECT_GT(particle.x, 0.1f);
        EXPECT_LT(particle.x, 0.9f);
    }

    LiquidSnapshot2D crossing;
    crossing.particles.assign({
        {0.96f, 0.5f, 0.0f, 0.0f},
        {0.5f, 0.04f, 0.0f, 0.0f}});
    ASSERT_TRUE(solver.restore(crossing));
    const auto escaped = solver.takeEscapedParticles();

    ASSERT_EQ(escaped.size(), 1U);
    EXPECT_GT(escaped.front()->x, 0.9f);
    ASSERT_EQ(solver.particles().size(), 1U);
    EXPECT_LT(solver.particles().front()->y, 0.1f);
}

TEST(LiquidSpillTest, NeighborInteractionsAreDeterministic)
{
    NamedVector<LiquidSpillParticle2D> a{
        ContainerName{"Test.LiquidSpillParticles"}};
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 7; ++x) {
            LiquidSpillParticle2D particle;
            particle.x = static_cast<float>(x) * 5.0f;
            particle.y = static_cast<float>(y) * 5.0f;
            particle.vx = static_cast<float>(y - 2) * 0.4f;
            particle.vy = static_cast<float>(3 - x) * 0.3f;
            particle.size = 4.0f;
            a.push_back(particle);
        }
    }
    auto b = a;

    for (int step = 0; step < 60; ++step) {
        LiquidSolver2D::applySpillInteractions(a, 1.0f / 30.0f, 0.35f, 0.2f);
        LiquidSolver2D::applySpillInteractions(b, 1.0f / 30.0f, 0.35f, 0.2f);
    }

    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i].vx, b[i].vx);
        EXPECT_EQ(a[i].vy, b[i].vy);
    }
}

TEST(LiquidSurfaceTest, ExtractionIsDeterministicAndProducesAllLanes)
{
    NamedVector<LiquidSurfaceSample2D> samples{
        ContainerName{"Test.LiquidSurfaceSamples"}};
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 8; ++x) {
            LiquidSurfaceSample2D sample;
            sample.x = 80.0f + static_cast<float>(x) * 8.0f;
            sample.y = 60.0f + static_cast<float>(y) * 8.0f;
            sample.size = 12.0f;
            sample.vx = y == 0 ? 48.0f : 2.0f;
            sample.vy = y == 0 ? -24.0f : 1.0f;
            sample.foamBias = 1.0f;
            sample.collisionImpact = y == 5 ? 32.0f : 0.0f;
            samples.push_back(sample);
        }
    }

    const auto a = LiquidSolver2D::buildSurfaceSnapshot(samples, 4096);
    const auto b = LiquidSolver2D::buildSurfaceSnapshot(samples, 4096);

    EXPECT_FALSE(a.triangles.empty());
    EXPECT_FALSE(a.contourSegments.empty());
    EXPECT_FALSE(a.foamPoints.empty());
    expectSameSurfaceSnapshot(a, b);
}

TEST(LiquidSurfaceTest, SnapshotIntoReusesOutputCapacityAndMatchesValueApi)
{
    NamedVector<LiquidSurfaceSample2D> samples{
        ContainerName{"Test.ReusedLiquidSurfaceSamples"}};
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 12; ++x) {
            LiquidSurfaceSample2D sample;
            sample.x = 24.0f + static_cast<float>(x) * 6.0f;
            sample.y = 48.0f + static_cast<float>(y) * 6.0f;
            sample.size = 9.0f;
            sample.vx = x == 5 ? 18.0f : 1.0f;
            sample.foamBias = 0.7f;
            samples.push_back(sample);
        }
    }

    LiquidSurfaceSnapshot2D reused;
    LiquidSurfaceWorkspace2D workspace;
    LiquidSolver2D::buildSurfaceSnapshotInto(samples, reused, workspace);
    ASSERT_FALSE(reused.triangles.empty());
    ASSERT_FALSE(reused.contourSegments.empty());
    const auto triangleCapacity = reused.triangles.capacity();
    const auto contourCapacity = reused.contourSegments.capacity();
    const auto foamCapacity = reused.foamPoints.capacity();
    const auto validSampleCapacity = workspace.validSamples.capacity();
    const auto expected = LiquidSolver2D::buildSurfaceSnapshot(samples);

    LiquidSolver2D::buildSurfaceSnapshotInto(samples, reused, workspace);

    expectSameSurfaceSnapshot(expected, reused);
    EXPECT_EQ(reused.triangles.capacity(), triangleCapacity);
    EXPECT_EQ(reused.contourSegments.capacity(), contourCapacity);
    EXPECT_EQ(reused.foamPoints.capacity(), foamCapacity);
    EXPECT_EQ(workspace.validSamples.capacity(), validSampleCapacity);
}

TEST(LiquidSurfaceTest, TriangleVerticesRetainDensityForDepthShading)
{
    LiquidSurfaceSample2D sample;
    sample.x = 32.0f;
    sample.y = 32.0f;
    sample.size = 8.0f;
    const auto snapshot = LiquidSolver2D::buildSurfaceSnapshot(
        NamedVector<LiquidSurfaceSample2D>{
            ContainerName{"Test.LiquidCoverageShadingSamples"}, {sample}});

    ASSERT_FALSE(snapshot.triangles.empty());
    float minimumCoverage = std::numeric_limits<float>::max();
    float maximumCoverage = std::numeric_limits<float>::lowest();
    for (const auto& triangle : snapshot.triangles) {
        for (const float coverage : {triangle.coverageA, triangle.coverageB,
                                     triangle.coverageC}) {
            EXPECT_TRUE(std::isfinite(coverage));
            EXPECT_GE(coverage, 0.34f);
            minimumCoverage = std::min(minimumCoverage, coverage);
            maximumCoverage = std::max(maximumCoverage, coverage);
        }
    }
    EXPECT_NEAR(minimumCoverage, 0.34f, 1.0e-5f)
        << "the reconstructed edge should retain the density threshold";
    EXPECT_GT(maximumCoverage, minimumCoverage + 0.2f)
        << "interior coverage should be available to the renderer as a smooth depth cue";
}

TEST(LiquidSurfaceTest, FoamPointsRetainVelocityForDirectionalRendering)
{
    NamedVector<LiquidSurfaceSample2D> samples{
        ContainerName{"Test.DirectionalLiquidFoamSamples"}};
    LiquidSurfaceSample2D sample;
    sample.x = 12.0f;
    sample.y = 24.0f;
    sample.size = 8.0f;
    sample.vx = 0.0f;
    sample.vy = 40.0f;
    sample.foamBias = 1.0f;
    samples.push_back(sample);

    const auto snapshot = LiquidSolver2D::buildSurfaceSnapshot(samples, 4096);

    ASSERT_EQ(snapshot.foamPoints.size(), 1u);
    EXPECT_FLOAT_EQ(snapshot.foamPoints[0].vx, 0.0f);
    EXPECT_FLOAT_EQ(snapshot.foamPoints[0].vy, 40.0f);
}

TEST(LiquidSurfaceTest, LargeInputLodSamplingMatchesEquivalentSurfaceDensity)
{
    NamedVector<LiquidSurfaceSample2D> denseSamples{
        ContainerName{"Test.LargeLiquidSurfaceSamples"}};
    NamedVector<LiquidSurfaceSample2D> explicitlySampled{
        ContainerName{"Test.ExplicitlySampledLiquidSurfaceSamples"}};
    for (int i = 0; i < 8192; ++i) {
        LiquidSurfaceSample2D sample;
        sample.x = static_cast<float>(i % 128) * 3.0f;
        sample.y = static_cast<float>(i / 128) * 3.0f;
        sample.size = 6.0f;
        sample.vx = (i % 128) == 0 ? 40.0f : 0.0f;
        sample.foamBias = 0.5f;
        denseSamples.push_back(sample);
        if ((i % 2) == 0) {
            // The LOD path widens each retained sample by sqrt(stride) to
            // preserve approximate 2D sample coverage. Model that same
            // coverage in the explicit reference input.
            sample.size *= std::sqrt(2.0f);
            explicitlySampled.push_back(sample);
        }
    }

    const auto lodSnapshot =
        LiquidSolver2D::buildSurfaceSnapshot(denseSamples, 65536);
    const auto explicitSnapshot =
        LiquidSolver2D::buildSurfaceSnapshot(explicitlySampled, 65536);

    EXPECT_FALSE(lodSnapshot.triangles.empty());
    ASSERT_EQ(lodSnapshot.triangles.size(), explicitSnapshot.triangles.size());
    for (std::size_t i = 0; i < lodSnapshot.triangles.size(); ++i) {
        const auto& actual = lodSnapshot.triangles[i];
        const auto& expected = explicitSnapshot.triangles[i];
        EXPECT_FLOAT_EQ(actual.a.x, expected.a.x);
        EXPECT_FLOAT_EQ(actual.a.y, expected.a.y);
        EXPECT_FLOAT_EQ(actual.b.x, expected.b.x);
        EXPECT_FLOAT_EQ(actual.b.y, expected.b.y);
        EXPECT_FLOAT_EQ(actual.c.x, expected.c.x);
        EXPECT_FLOAT_EQ(actual.c.y, expected.c.y);
        EXPECT_FLOAT_EQ(actual.thickness, expected.thickness);
        EXPECT_FLOAT_EQ(actual.coverageA, expected.coverageA);
        EXPECT_FLOAT_EQ(actual.coverageB, expected.coverageB);
        EXPECT_FLOAT_EQ(actual.coverageC, expected.coverageC);
    }
    ASSERT_EQ(lodSnapshot.contourSegments.size(),
              explicitSnapshot.contourSegments.size());
    for (std::size_t i = 0; i < lodSnapshot.contourSegments.size(); ++i) {
        const auto& actual = lodSnapshot.contourSegments[i];
        const auto& expected = explicitSnapshot.contourSegments[i];
        EXPECT_FLOAT_EQ(actual.a.x, expected.a.x);
        EXPECT_FLOAT_EQ(actual.a.y, expected.a.y);
        EXPECT_FLOAT_EQ(actual.b.x, expected.b.x);
        EXPECT_FLOAT_EQ(actual.b.y, expected.b.y);
    }
}

TEST(LiquidSurfaceTest, SamplingStrideIncreasesImmediatelyAboveInputBudget)
{
    NamedVector<LiquidSurfaceSample2D> denseSamples{
        ContainerName{"Test.LiquidSurfaceSamplesAtLodBoundary"}};
    NamedVector<LiquidSurfaceSample2D> explicitlySampled{
        ContainerName{"Test.LiquidSurfaceSamplesAtLodBoundaryExplicit"}};
    for (int i = 0; i < 4097; ++i) {
        LiquidSurfaceSample2D sample;
        sample.x = 32.0f;
        sample.y = 48.0f;
        sample.size = 5.0f;
        sample.vx = (i % 13) == 0 ? 24.0f : 0.0f;
        sample.vy = (i % 17) == 0 ? -12.0f : 0.0f;
        sample.foamBias = 0.75f;
        sample.collisionImpact = (i % 19) == 0 ? 8.0f : 0.0f;
        denseSamples.push_back(sample);
        if ((i % 2) == 0) {
            sample.size *= std::sqrt(2.0f);
            explicitlySampled.push_back(sample);
        }
    }

    const auto lodSnapshot =
        LiquidSolver2D::buildSurfaceSnapshot(denseSamples, 4096);
    const auto explicitSnapshot =
        LiquidSolver2D::buildSurfaceSnapshot(explicitlySampled, 4096);

    ASSERT_FALSE(lodSnapshot.triangles.empty());
    ASSERT_EQ(lodSnapshot.triangles.size(), explicitSnapshot.triangles.size());
    for (std::size_t i = 0; i < lodSnapshot.triangles.size(); ++i) {
        const auto& actual = lodSnapshot.triangles[i];
        const auto& expected = explicitSnapshot.triangles[i];
        EXPECT_NEAR(actual.a.x, expected.a.x, 1.0e-4f);
        EXPECT_NEAR(actual.a.y, expected.a.y, 1.0e-4f);
        EXPECT_NEAR(actual.b.x, expected.b.x, 1.0e-4f);
        EXPECT_NEAR(actual.b.y, expected.b.y, 1.0e-4f);
        EXPECT_NEAR(actual.c.x, expected.c.x, 1.0e-4f);
        EXPECT_NEAR(actual.c.y, expected.c.y, 1.0e-4f);
        EXPECT_NEAR(actual.thickness, expected.thickness, 1.0e-4f);
        EXPECT_NEAR(actual.coverageA, expected.coverageA, 1.0e-4f);
        EXPECT_NEAR(actual.coverageB, expected.coverageB, 1.0e-4f);
        EXPECT_NEAR(actual.coverageC, expected.coverageC, 1.0e-4f);
    }
    ASSERT_EQ(lodSnapshot.contourSegments.size(),
              explicitSnapshot.contourSegments.size());
    for (std::size_t i = 0; i < lodSnapshot.contourSegments.size(); ++i) {
        const auto& actual = lodSnapshot.contourSegments[i];
        const auto& expected = explicitSnapshot.contourSegments[i];
        EXPECT_NEAR(actual.a.x, expected.a.x, 1.0e-4f);
        EXPECT_NEAR(actual.a.y, expected.a.y, 1.0e-4f);
        EXPECT_NEAR(actual.b.x, expected.b.x, 1.0e-4f);
        EXPECT_NEAR(actual.b.y, expected.b.y, 1.0e-4f);
    }
}

TEST(LiquidSurfaceTest, KeepsTheLastSampleAtTheExactDensityBudget)
{
    constexpr int maximumDensitySamples = 4096;
    NamedVector<LiquidSurfaceSample2D> samples{
        ContainerName{"Test.LiquidSurfaceExactDensityBudget"}};
    for (int i = 0; i < maximumDensitySamples; ++i) {
        LiquidSurfaceSample2D sample;
        sample.x = 0.0f;
        sample.y = 0.0f;
        sample.size = 8.0f;
        if (i == maximumDensitySamples - 1) {
            sample.x = 1000.0f;
            sample.y = 500.0f;
            sample.vx = 100.0f;
            sample.foamBias = 1.0f;
        }
        samples.push_back(sample);
    }

    const auto snapshot = LiquidSolver2D::buildSurfaceSnapshot(samples, 4096);

    const auto outlierFoam = std::find_if(
        snapshot.foamPoints.begin(), snapshot.foamPoints.end(),
        [](const LiquidFoamPoint2D& foam) {
            return std::abs(foam.position.x - 1000.0f) < 1.0e-4f &&
                   std::abs(foam.position.y - 500.0f) < 1.0e-4f;
        });
    ASSERT_NE(outlierFoam, snapshot.foamPoints.end())
        << "An input at the exact LOD budget must not be stride-sampled away";
    EXPECT_GT(outlierFoam->alpha, 0.08f);
}

TEST(LiquidSurfaceTest, InvalidSamplesDoNotReachRenderSnapshot)
{
    LiquidSurfaceSample2D invalidVelocity;
    invalidVelocity.x = 10.0f;
    invalidVelocity.y = 10.0f;
    invalidVelocity.size = 8.0f;
    invalidVelocity.vx = std::numeric_limits<float>::quiet_NaN();
    invalidVelocity.foamBias = 1.0f;

    LiquidSurfaceSample2D outOfRange;
    outOfRange.x = 20000000.0f;
    outOfRange.y = 10.0f;
    outOfRange.size = 8.0f;
    outOfRange.foamBias = 1.0f;

    const auto snapshot = LiquidSolver2D::buildSurfaceSnapshot(
        NamedVector<LiquidSurfaceSample2D>{
            ContainerName{"Test.InvalidLiquidSurfaceSamples"},
            {invalidVelocity, outOfRange}});

    EXPECT_TRUE(snapshot.triangles.empty());
    EXPECT_TRUE(snapshot.contourSegments.empty());
    EXPECT_TRUE(snapshot.foamPoints.empty());
}

TEST(LiquidSurfaceTest, CollisionImpactCanCreateFoamAfterMotionSlows)
{
    LiquidSurfaceSample2D impactSample;
    impactSample.x = 32.0f;
    impactSample.y = 32.0f;
    impactSample.size = 10.0f;
    impactSample.vx = 0.0f;
    impactSample.vy = 0.0f;
    impactSample.foamBias = 1.0f;
    impactSample.collisionImpact = 30.0f;

    const auto snapshot =
        LiquidSolver2D::buildSurfaceSnapshot(
            NamedVector<LiquidSurfaceSample2D>{
                ContainerName{"Test.ImpactLiquidSurfaceSamples"},
                {impactSample}});

    EXPECT_FALSE(snapshot.foamPoints.empty());
}

TEST(LiquidSurfaceTest, FoamOffsetMovesFoamWithoutMovingLiquidSurface)
{
    LiquidSurfaceSample2D sample;
    sample.x = 32.0f;
    sample.y = 32.0f;
    sample.size = 8.0f;
    sample.vx = 40.0f;
    sample.foamBias = 1.0f;
    const auto sourceSnapshot = LiquidSolver2D::buildSurfaceSnapshot(
        NamedVector<LiquidSurfaceSample2D>{
            ContainerName{"Test.LiquidFoamProjectionSource"}, {sample}});

    sample.foamOffsetY = 12.0f;
    const auto projectedSnapshot = LiquidSolver2D::buildSurfaceSnapshot(
        NamedVector<LiquidSurfaceSample2D>{
            ContainerName{"Test.LiquidFoamProjectionTarget"}, {sample}});

    ASSERT_FALSE(sourceSnapshot.foamPoints.empty());
    ASSERT_EQ(sourceSnapshot.foamPoints.size(),
              projectedSnapshot.foamPoints.size());
    EXPECT_FLOAT_EQ(projectedSnapshot.foamPoints[0].position.x,
                    sourceSnapshot.foamPoints[0].position.x);
    EXPECT_FLOAT_EQ(projectedSnapshot.foamPoints[0].position.y,
                    sourceSnapshot.foamPoints[0].position.y + 12.0f);
    ASSERT_EQ(sourceSnapshot.triangles.size(),
              projectedSnapshot.triangles.size());
    for (std::size_t i = 0; i < sourceSnapshot.triangles.size(); ++i) {
        EXPECT_FLOAT_EQ(sourceSnapshot.triangles[i].a.x,
                        projectedSnapshot.triangles[i].a.x);
        EXPECT_FLOAT_EQ(sourceSnapshot.triangles[i].a.y,
                        projectedSnapshot.triangles[i].a.y);
        EXPECT_FLOAT_EQ(sourceSnapshot.triangles[i].b.x,
                        projectedSnapshot.triangles[i].b.x);
        EXPECT_FLOAT_EQ(sourceSnapshot.triangles[i].b.y,
                        projectedSnapshot.triangles[i].b.y);
        EXPECT_FLOAT_EQ(sourceSnapshot.triangles[i].c.x,
                        projectedSnapshot.triangles[i].c.x);
        EXPECT_FLOAT_EQ(sourceSnapshot.triangles[i].c.y,
                        projectedSnapshot.triangles[i].c.y);
    }
}

TEST(SandSimDeterminismTest, DefaultSeedIsDeterministicAcrossInstances)
{
    SandSim2D a(32, 32);
    SandSim2D b(32, 32);

    a.fillCircle(16, 4, 4, SandMaterial::Sand);
    b.fillCircle(16, 4, 4, SandMaterial::Sand);
    a.setCell(8, 8, SandMaterial::Fire);
    b.setCell(8, 8, SandMaterial::Fire);
    a.setCell(24, 8, SandMaterial::Water);
    b.setCell(24, 8, SandMaterial::Water);

    for (int step = 0; step < 30; ++step) {
        a.update(1);
        b.update(1);
    }

    ASSERT_EQ(a.grid().size(), b.grid().size());
    for (std::size_t i = 0; i < a.grid().size(); ++i) {
        EXPECT_EQ(a.grid()[i], b.grid()[i]);
    }
}
