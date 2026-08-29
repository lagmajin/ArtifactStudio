#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

import Physics.SoftBody;
import Physics.Fluid;
import Physics.SandSim2D;

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

TEST(LiquidDeterminismTest, InvalidSnapshotIsRejectedWithoutMutation)
{
    LiquidSolver2D solver;
    solver.reset(0.25f, 0.055f);
    const LiquidSnapshot2D before = solver.snapshot();
    LiquidSnapshot2D invalid = before;
    ASSERT_FALSE(invalid.particles.empty());
    invalid.particles.front().x = std::numeric_limits<float>::quiet_NaN();

    EXPECT_FALSE(solver.restore(invalid));
    expectSameLiquidState(before, solver.snapshot());
}

TEST(LiquidSurfaceTensionTest, PullsNearbySeparatedParticlesTogether)
{
    LiquidSnapshot2D initial;
    initial.particles = {
        {0.445f, 0.5f, 0.0f, 0.0f},
        {0.555f, 0.5f, 0.0f, 0.0f}};

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

    EXPECT_LT(nearStart.particles().front().x,
              nearEnd.particles().front().x);
    EXPECT_EQ(nearStart.particles().front().y,
              nearEnd.particles().front().y);
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
        {{0.1f, 0.1f}, {0.9f, 0.1f}, {0.9f, 0.9f}, {0.1f, 0.9f}},
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
        {{0.1f, 0.1f}, {0.9f, 0.1f}, {0.9f, 0.9f}, {0.1f, 0.9f}},
        1));
    solver.reset(0.0f, 0.05f);

    ASSERT_GT(solver.emitFromOpening(4, 0.4f, 1.0f), 0U);
    for (const auto& particle : solver.particles()) {
        EXPECT_LT(particle.vx, 0.0f);
        EXPECT_GT(particle.x, 0.1f);
        EXPECT_LT(particle.x, 0.9f);
    }

    LiquidSnapshot2D crossing;
    crossing.particles = {
        {0.96f, 0.5f, 0.0f, 0.0f},
        {0.5f, 0.04f, 0.0f, 0.0f}};
    ASSERT_TRUE(solver.restore(crossing));
    const auto escaped = solver.takeEscapedParticles();

    ASSERT_EQ(escaped.size(), 1U);
    EXPECT_GT(escaped.front().x, 0.9f);
    ASSERT_EQ(solver.particles().size(), 1U);
    EXPECT_LT(solver.particles().front().y, 0.1f);
}

TEST(LiquidSpillTest, NeighborInteractionsAreDeterministic)
{
    std::vector<LiquidSpillParticle2D> a;
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
    std::vector<LiquidSurfaceSample2D> samples;
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
        {invalidVelocity, outOfRange});

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
        LiquidSolver2D::buildSurfaceSnapshot({impactSample});

    EXPECT_FALSE(snapshot.foamPoints.empty());
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
