#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
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
