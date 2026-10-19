#include <gtest/gtest.h>

#include <limits>
#include <vector>

import Physics.SoftBody;

using namespace ArtifactCore;

TEST(SoftBodyLodContractTest, ReducedGridKeepsValidTopologyAndRestoreIsExact)
{
    SoftBodySolver solver;
    solver.buildGrid(2.0f, 3.0f, 12.0f, 8.0f, 17, 13);
    const SoftBodySnapshot full = solver.snapshot();

    solver.reduceGridResolution(0.5f);
    const SoftBodySnapshot reduced = solver.snapshot();
    EXPECT_EQ(reduced.gridColumns, 9);
    EXPECT_EQ(reduced.gridRows, 7);
    EXPECT_LT(reduced.points.size(), full.points.size());
    EXPECT_LT(reduced.constraints.size(), full.constraints.size());
    for (int y = 0; y < reduced.gridRows; ++y) {
        for (int x = 0; x < reduced.gridColumns; ++x) {
            const std::size_t reducedIndex =
                static_cast<std::size_t>(y * reduced.gridColumns + x);
            const std::size_t fullIndex =
                static_cast<std::size_t>(y * 2 * full.gridColumns + x * 2);
            ASSERT_LT(reducedIndex, reduced.points.size());
            ASSERT_LT(fullIndex, full.points.size());
            EXPECT_FLOAT_EQ(reduced.points[reducedIndex].x, full.points[fullIndex].x);
            EXPECT_FLOAT_EQ(reduced.points[reducedIndex].y, full.points[fullIndex].y);
            EXPECT_FLOAT_EQ(reduced.points[reducedIndex].prevX,
                            full.points[fullIndex].prevX);
            EXPECT_FLOAT_EQ(reduced.points[reducedIndex].prevY,
                            full.points[fullIndex].prevY);
            EXPECT_EQ(reduced.points[reducedIndex].isPinned,
                      full.points[fullIndex].isPinned);
        }
    }
    std::vector<std::size_t> pointDegrees(reduced.points.size(), 0);
    std::size_t structuralConstraintCount = 0;
    std::size_t diagonalConstraintCount = 0;
    for (const auto& constraint : reduced.constraints) {
        EXPECT_GE(constraint.p1Idx, 0);
        EXPECT_GE(constraint.p2Idx, 0);
        EXPECT_LT(static_cast<std::size_t>(constraint.p1Idx), reduced.points.size());
        EXPECT_LT(static_cast<std::size_t>(constraint.p2Idx), reduced.points.size());
        if (constraint.p1Idx < 0 || constraint.p2Idx < 0 ||
            static_cast<std::size_t>(constraint.p1Idx) >= reduced.points.size() ||
            static_cast<std::size_t>(constraint.p2Idx) >= reduced.points.size()) {
            continue;
        }
        ++pointDegrees[static_cast<std::size_t>(constraint.p1Idx)];
        ++pointDegrees[static_cast<std::size_t>(constraint.p2Idx)];
        if (constraint.stiffness == 1.0f) {
            ++structuralConstraintCount;
        } else if (constraint.stiffness == 0.5f) {
            ++diagonalConstraintCount;
        } else {
            ADD_FAILURE() << "unexpected reduced-grid constraint stiffness: "
                          << constraint.stiffness;
        }
    }
    EXPECT_EQ(structuralConstraintCount,
              static_cast<std::size_t>(reduced.gridRows * (reduced.gridColumns - 1) +
                                       reduced.gridColumns * (reduced.gridRows - 1)));
    EXPECT_EQ(diagonalConstraintCount,
              static_cast<std::size_t>(2 * (reduced.gridRows - 1) *
                                       (reduced.gridColumns - 1)));
    for (std::size_t degree : pointDegrees) EXPECT_GT(degree, 0u);

    solver.restoreGridResolution();
    const SoftBodySnapshot restored = solver.snapshot();
    ASSERT_EQ(restored.gridColumns, full.gridColumns);
    ASSERT_EQ(restored.gridRows, full.gridRows);
    ASSERT_EQ(restored.points.size(), full.points.size());
    ASSERT_EQ(restored.constraints.size(), full.constraints.size());
    for (std::size_t i = 0; i < full.points.size(); ++i) {
        EXPECT_FLOAT_EQ(restored.points[i].x, full.points[i].x);
        EXPECT_FLOAT_EQ(restored.points[i].y, full.points[i].y);
        EXPECT_FLOAT_EQ(restored.points[i].prevX, full.points[i].prevX);
        EXPECT_FLOAT_EQ(restored.points[i].prevY, full.points[i].prevY);
        EXPECT_EQ(restored.points[i].isPinned, full.points[i].isPinned);
    }
    for (std::size_t i = 0; i < full.constraints.size(); ++i) {
        EXPECT_EQ(restored.constraints[i].p1Idx, full.constraints[i].p1Idx);
        EXPECT_EQ(restored.constraints[i].p2Idx, full.constraints[i].p2Idx);
        EXPECT_FLOAT_EQ(restored.constraints[i].restDistance, full.constraints[i].restDistance);
        EXPECT_FLOAT_EQ(restored.constraints[i].stiffness, full.constraints[i].stiffness);
    }

    solver.restoreGridResolution();
    const SoftBodySnapshot restoredAgain = solver.snapshot();
    EXPECT_EQ(restoredAgain.gridColumns, full.gridColumns);
    EXPECT_EQ(restoredAgain.gridRows, full.gridRows);
    EXPECT_EQ(restoredAgain.points.size(), full.points.size());
    EXPECT_EQ(restoredAgain.constraints.size(), full.constraints.size());
    EXPECT_EQ(restoredAgain.volumeTriangles.size(), full.volumeTriangles.size());
}

TEST(SoftBodyLodContractTest,
     SmallestSupportedGridReducesToValidTwoByTwoAndRestoresExactly)
{
    SoftBodySolver solver;
    solver.buildGrid(1.0f, 2.0f, 4.0f, 6.0f, 3, 3);
    const SoftBodySnapshot full = solver.snapshot();
    ASSERT_EQ(full.points.size(), 9u);

    solver.reduceGridResolution(0.25f);
    const SoftBodySnapshot reduced = solver.snapshot();
    ASSERT_EQ(reduced.gridColumns, 2);
    ASSERT_EQ(reduced.gridRows, 2);
    ASSERT_EQ(reduced.points.size(), 4u);
    ASSERT_EQ(reduced.constraints.size(), 6u);
    EXPECT_TRUE(reduced.volumeTriangles.empty());
    for (const SoftBodyConstraint& constraint : reduced.constraints) {
        EXPECT_GE(constraint.p1Idx, 0);
        EXPECT_GE(constraint.p2Idx, 0);
        EXPECT_LT(static_cast<std::size_t>(constraint.p1Idx),
                  reduced.points.size());
        EXPECT_LT(static_cast<std::size_t>(constraint.p2Idx),
                  reduced.points.size());
        EXPECT_NE(constraint.p1Idx, constraint.p2Idx);
    }

    solver.restoreGridResolution();
    const SoftBodySnapshot restored = solver.snapshot();
    ASSERT_EQ(restored.gridColumns, full.gridColumns);
    ASSERT_EQ(restored.gridRows, full.gridRows);
    ASSERT_EQ(restored.points.size(), full.points.size());
    ASSERT_EQ(restored.constraints.size(), full.constraints.size());
    for (std::size_t index = 0; index < full.points.size(); ++index) {
        EXPECT_FLOAT_EQ(restored.points[index].x, full.points[index].x);
        EXPECT_FLOAT_EQ(restored.points[index].y, full.points[index].y);
        EXPECT_FLOAT_EQ(restored.points[index].prevX,
                        full.points[index].prevX);
        EXPECT_FLOAT_EQ(restored.points[index].prevY,
                        full.points[index].prevY);
        EXPECT_EQ(restored.points[index].isPinned,
                  full.points[index].isPinned);
    }
    for (std::size_t index = 0; index < full.constraints.size(); ++index) {
        EXPECT_EQ(restored.constraints[index].p1Idx,
                  full.constraints[index].p1Idx);
        EXPECT_EQ(restored.constraints[index].p2Idx,
                  full.constraints[index].p2Idx);
        EXPECT_FLOAT_EQ(restored.constraints[index].restDistance,
                        full.constraints[index].restDistance);
        EXPECT_FLOAT_EQ(restored.constraints[index].stiffness,
                        full.constraints[index].stiffness);
    }
}

TEST(SoftBodyLodContractTest, ChangingLodScaleAlwaysUsesFullResolutionBackup)
{
    SoftBodySolver solver;
    solver.buildGrid(0.0f, 0.0f, 10.0f, 10.0f, 21, 21);

    solver.reduceGridResolution(0.5f);
    EXPECT_EQ(solver.snapshot().gridColumns, 11);
    solver.reduceGridResolution(0.25f);
    const SoftBodySnapshot minimal = solver.snapshot();
    EXPECT_EQ(minimal.gridColumns, 6);
    EXPECT_EQ(minimal.gridRows, 6);

    solver.reduceGridResolution(0.75f);
    const SoftBodySnapshot medium = solver.snapshot();
    EXPECT_EQ(medium.gridColumns, 16);
    EXPECT_EQ(medium.gridRows, 16);
    EXPECT_LT(medium.points.size(), 21u * 21u);

    solver.restoreGridResolution();
    EXPECT_EQ(solver.snapshot().points.size(), 21u * 21u);
}

TEST(SoftBodyLodContractTest,
     RepeatedLodCyclesRebuildFromRestoredFullResolution)
{
    SoftBodySolver solver;
    solver.buildGrid(0.0f, 0.0f, 10.0f, 10.0f, 21, 21);

    solver.reduceGridResolution(0.5f);
    const SoftBodySnapshot firstHalf = solver.snapshot();
    ASSERT_EQ(firstHalf.gridColumns, 11);
    ASSERT_EQ(firstHalf.gridRows, 11);
    solver.restoreGridResolution();
    ASSERT_EQ(solver.snapshot().points.size(), 21u * 21u);

    solver.reduceGridResolution(0.25f);
    EXPECT_EQ(solver.snapshot().gridColumns, 6);
    EXPECT_EQ(solver.snapshot().gridRows, 6);
    solver.restoreGridResolution();
    ASSERT_EQ(solver.snapshot().points.size(), 21u * 21u);

    solver.reduceGridResolution(0.75f);
    EXPECT_EQ(solver.snapshot().gridColumns, 16);
    EXPECT_EQ(solver.snapshot().gridRows, 16);
    solver.restoreGridResolution();
    ASSERT_EQ(solver.snapshot().points.size(), 21u * 21u);

    solver.reduceGridResolution(0.5f);
    const SoftBodySnapshot repeatedHalf = solver.snapshot();
    ASSERT_EQ(repeatedHalf.gridColumns, firstHalf.gridColumns);
    ASSERT_EQ(repeatedHalf.gridRows, firstHalf.gridRows);
    ASSERT_EQ(repeatedHalf.points.size(), firstHalf.points.size());
    ASSERT_EQ(repeatedHalf.constraints.size(), firstHalf.constraints.size());
    for (std::size_t index = 0; index < firstHalf.points.size(); ++index) {
        EXPECT_FLOAT_EQ(repeatedHalf.points[index].x, firstHalf.points[index].x);
        EXPECT_FLOAT_EQ(repeatedHalf.points[index].y, firstHalf.points[index].y);
        EXPECT_FLOAT_EQ(repeatedHalf.points[index].prevX,
                        firstHalf.points[index].prevX);
        EXPECT_FLOAT_EQ(repeatedHalf.points[index].prevY,
                        firstHalf.points[index].prevY);
        EXPECT_EQ(repeatedHalf.points[index].isPinned,
                  firstHalf.points[index].isPinned);
    }
    for (std::size_t index = 0; index < firstHalf.constraints.size(); ++index) {
        EXPECT_EQ(repeatedHalf.constraints[index].p1Idx,
                  firstHalf.constraints[index].p1Idx);
        EXPECT_EQ(repeatedHalf.constraints[index].p2Idx,
                  firstHalf.constraints[index].p2Idx);
        EXPECT_FLOAT_EQ(repeatedHalf.constraints[index].restDistance,
                        firstHalf.constraints[index].restDistance);
        EXPECT_FLOAT_EQ(repeatedHalf.constraints[index].stiffness,
                        firstHalf.constraints[index].stiffness);
    }
    solver.restoreGridResolution();
    EXPECT_EQ(solver.snapshot().points.size(), 21u * 21u);
}

TEST(SoftBodyLodContractTest,
     RebuildingGridWhileReducedDiscardsThePreviousLodRestoreTarget)
{
    SoftBodySolver solver;
    solver.buildGrid(-4.0f, 2.0f, 8.0f, 6.0f, 17, 13);
    solver.reduceGridResolution(0.25f);
    ASSERT_EQ(solver.snapshot().gridColumns, 5);
    ASSERT_EQ(solver.snapshot().gridRows, 4);

    solver.buildGrid(11.0f, -7.0f, 12.0f, 9.0f, 7, 5);
    const SoftBodySnapshot rebuilt = solver.snapshot();
    ASSERT_EQ(rebuilt.gridColumns, 7);
    ASSERT_EQ(rebuilt.gridRows, 5);
    ASSERT_EQ(rebuilt.points.size(), 35u);
    ASSERT_FALSE(rebuilt.points.empty());
    EXPECT_FLOAT_EQ(rebuilt.points.front().x, 11.0f);
    EXPECT_FLOAT_EQ(rebuilt.points.front().y, -7.0f);
    EXPECT_FLOAT_EQ(rebuilt.points.back().x, 23.0f);
    EXPECT_FLOAT_EQ(rebuilt.points.back().y, 2.0f);

    solver.restoreGridResolution();
    const SoftBodySnapshot afterRestore = solver.snapshot();
    EXPECT_EQ(afterRestore.gridColumns, rebuilt.gridColumns);
    EXPECT_EQ(afterRestore.gridRows, rebuilt.gridRows);
    ASSERT_EQ(afterRestore.points.size(), rebuilt.points.size());
    for (std::size_t index = 0; index < rebuilt.points.size(); ++index) {
        EXPECT_FLOAT_EQ(afterRestore.points[index].x, rebuilt.points[index].x);
        EXPECT_FLOAT_EQ(afterRestore.points[index].y, rebuilt.points[index].y);
        EXPECT_FLOAT_EQ(afterRestore.points[index].prevX,
                        rebuilt.points[index].prevX);
        EXPECT_FLOAT_EQ(afterRestore.points[index].prevY,
                        rebuilt.points[index].prevY);
        EXPECT_EQ(afterRestore.points[index].isPinned,
                  rebuilt.points[index].isPinned);
    }
}

TEST(SoftBodyLodContractTest, RestoringFullResolutionRestoresVolumeTrianglesExactly)
{
    SoftBodySolver solver;
    solver.buildGrid(0.0f, 0.0f, 8.0f, 8.0f, 5, 5, 1.0f, 1.0f, false);
    solver.addVolumeTriangle(0, 1, 5);
    solver.addVolumeTriangle(12, 13, 17);
    const SoftBodySnapshot full = solver.snapshot();
    ASSERT_EQ(full.volumeTriangles.size(), 2u);

    solver.reduceGridResolution(0.5f);
    EXPECT_TRUE(solver.snapshot().volumeTriangles.empty())
        << "reduced grid topology must not retain stale full-resolution indices";

    solver.restoreGridResolution();
    const SoftBodySnapshot restored = solver.snapshot();
    ASSERT_EQ(restored.volumeTriangles.size(), full.volumeTriangles.size());
    for (std::size_t index = 0; index < full.volumeTriangles.size(); ++index) {
        EXPECT_EQ(restored.volumeTriangles[index].i0,
                  full.volumeTriangles[index].i0);
        EXPECT_EQ(restored.volumeTriangles[index].i1,
                  full.volumeTriangles[index].i1);
        EXPECT_EQ(restored.volumeTriangles[index].i2,
                  full.volumeTriangles[index].i2);
        EXPECT_FLOAT_EQ(restored.volumeTriangles[index].restArea,
                        full.volumeTriangles[index].restArea);
    }
}

TEST(SoftBodyLodContractTest, InvalidAndFullScaleRequestsDoNotChangeGrid)
{
    SoftBodySolver solver;
    solver.buildGrid(0.0f, 0.0f, 4.0f, 4.0f, 9, 7);
    const SoftBodySnapshot full = solver.snapshot();

    solver.reduceGridResolution(1.0f);
    EXPECT_EQ(solver.snapshot().points.size(), full.points.size());
    solver.reduceGridResolution(0.0f);
    EXPECT_EQ(solver.snapshot().gridColumns, 3);
    EXPECT_EQ(solver.snapshot().gridRows, 3);
    solver.restoreGridResolution();
    EXPECT_EQ(solver.snapshot().points.size(), full.points.size());
}

TEST(SoftBodyLodContractTest, InfiniteAndOutOfRangeScalesClampToSupportedExtremes)
{
    SoftBodySolver solver;
    solver.buildGrid(0.0f, 0.0f, 8.0f, 6.0f, 9, 7);
    const SoftBodySnapshot full = solver.snapshot();

    solver.reduceGridResolution(2.0f);
    EXPECT_EQ(solver.snapshot().points.size(), full.points.size());
    solver.reduceGridResolution(std::numeric_limits<float>::infinity());
    EXPECT_EQ(solver.snapshot().points.size(), full.points.size());

    solver.reduceGridResolution(-std::numeric_limits<float>::infinity());
    const SoftBodySnapshot negativeInfinity = solver.snapshot();
    EXPECT_EQ(negativeInfinity.gridColumns, 3);
    EXPECT_EQ(negativeInfinity.gridRows, 3);
    solver.restoreGridResolution();

    solver.reduceGridResolution(-1.0f);
    const SoftBodySnapshot belowRange = solver.snapshot();
    EXPECT_EQ(belowRange.gridColumns, negativeInfinity.gridColumns);
    EXPECT_EQ(belowRange.gridRows, negativeInfinity.gridRows);
    EXPECT_EQ(belowRange.points.size(), negativeInfinity.points.size());
    solver.restoreGridResolution();
    EXPECT_EQ(solver.snapshot().points.size(), full.points.size());
}

TEST(SoftBodyLodContractTest, NaNScalePreservesFullResolutionGrid)
{
    SoftBodySolver solver;
    solver.buildGrid(0.0f, 0.0f, 8.0f, 6.0f, 9, 7);
    const SoftBodySnapshot full = solver.snapshot();

    solver.reduceGridResolution(std::numeric_limits<float>::quiet_NaN());
    const SoftBodySnapshot afterNaN = solver.snapshot();

    EXPECT_EQ(afterNaN.gridColumns, full.gridColumns);
    EXPECT_EQ(afterNaN.gridRows, full.gridRows);
    EXPECT_EQ(afterNaN.points.size(), full.points.size());
    EXPECT_EQ(afterNaN.constraints.size(), full.constraints.size());
    EXPECT_EQ(afterNaN.volumeTriangles.size(), full.volumeTriangles.size());
}

TEST(SoftBodyLodContractTest, NaNScalePreservesAlreadyReducedGridState)
{
    SoftBodySolver solver;
    solver.buildGrid(0.0f, 0.0f, 8.0f, 6.0f, 9, 7);
    solver.reduceGridResolution(0.5f);
    const SoftBodySnapshot reduced = solver.snapshot();

    solver.reduceGridResolution(std::numeric_limits<float>::quiet_NaN());
    const SoftBodySnapshot afterNaN = solver.snapshot();

    ASSERT_EQ(afterNaN.gridColumns, reduced.gridColumns);
    ASSERT_EQ(afterNaN.gridRows, reduced.gridRows);
    ASSERT_EQ(afterNaN.points.size(), reduced.points.size());
    ASSERT_EQ(afterNaN.constraints.size(), reduced.constraints.size());
    ASSERT_EQ(afterNaN.volumeTriangles.size(), reduced.volumeTriangles.size());
    for (std::size_t index = 0; index < reduced.points.size(); ++index) {
        EXPECT_FLOAT_EQ(afterNaN.points[index].x, reduced.points[index].x);
        EXPECT_FLOAT_EQ(afterNaN.points[index].y, reduced.points[index].y);
        EXPECT_FLOAT_EQ(afterNaN.points[index].prevX, reduced.points[index].prevX);
        EXPECT_FLOAT_EQ(afterNaN.points[index].prevY, reduced.points[index].prevY);
        EXPECT_EQ(afterNaN.points[index].isPinned, reduced.points[index].isPinned);
    }
    for (std::size_t index = 0; index < reduced.constraints.size(); ++index) {
        EXPECT_EQ(afterNaN.constraints[index].p1Idx,
                  reduced.constraints[index].p1Idx);
        EXPECT_EQ(afterNaN.constraints[index].p2Idx,
                  reduced.constraints[index].p2Idx);
        EXPECT_FLOAT_EQ(afterNaN.constraints[index].restDistance,
                        reduced.constraints[index].restDistance);
        EXPECT_FLOAT_EQ(afterNaN.constraints[index].stiffness,
                        reduced.constraints[index].stiffness);
    }
}
