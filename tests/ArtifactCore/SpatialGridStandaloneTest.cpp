#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <vector>

import Math.SpatialGrid;
import Particle;

using namespace ArtifactCore;

namespace {
std::vector<size_t> queryIndices(const SpatialGrid& grid, const float3& center, float radius)
{
    std::vector<size_t> result;
    grid.query(center, radius, [&result](size_t index) { result.push_back(index); });
    std::sort(result.begin(), result.end());
    return result;
}
}

TEST(SpatialGridStandaloneTest, QueryOnEmptyGridReturnsNoIndices)
{
    const SpatialGrid grid(1.0f);
    EXPECT_TRUE(queryIndices(grid, float3(0.0f), 1.0f).empty());
}

TEST(SpatialGridStandaloneTest, BuildAndQueryReturnsIndexForOccupiedCell)
{
    SpatialGrid grid(1.0f);
    grid.build({float3(0.25f, 0.25f, 0.25f), float3(4.25f, 0.25f, 0.25f)});
    EXPECT_EQ(queryIndices(grid, float3(0.5f, 0.5f, 0.5f), 0.0f),
              (std::vector<size_t>{0}));
}

TEST(SpatialGridStandaloneTest, QueryRadiusAcrossCellBoundaryIncludesBothCells)
{
    SpatialGrid grid(1.0f);
    grid.build({float3(0.95f, 0.5f, 0.5f), float3(1.05f, 0.5f, 0.5f)});
    EXPECT_EQ(queryIndices(grid, float3(0.95f, 0.5f, 0.5f), 0.1f),
              (std::vector<size_t>{0, 1}));
}

TEST(SpatialGridStandaloneTest, CellCoordinatesUseFloorForNegativePositions)
{
    SpatialGrid grid(1.0f);
    grid.build({float3(-0.1f, 0.0f, 0.0f), float3(0.1f, 0.0f, 0.0f)});
    EXPECT_EQ(queryIndices(grid, float3(-0.1f, 0.0f, 0.0f), 0.0f),
              (std::vector<size_t>{0}));
    EXPECT_EQ(queryIndices(grid, float3(0.1f, 0.0f, 0.0f), 0.0f),
              (std::vector<size_t>{1}));
}

TEST(SpatialGridStandaloneTest, AllEntriesInTheSameCellAreReturnedAsCandidates)
{
    SpatialGrid grid(10.0f);
    grid.build({float3(0.0f, 0.0f, 0.0f), float3(2.0f, 3.0f, 4.0f),
                float3(9.0f, 9.0f, 9.0f)});
    EXPECT_EQ(queryIndices(grid, float3(1.0f, 1.0f, 1.0f), 0.0f),
              (std::vector<size_t>{0, 1, 2}));
}

TEST(SpatialGridStandaloneTest, QueryCandidatesAreOriginalInputIndices)
{
    SpatialGrid grid(1.0f);
    grid.build({float3(4.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f),
                float3(2.0f, 0.0f, 0.0f)});
    EXPECT_EQ(queryIndices(grid, float3(0.0f), 0.0f),
              (std::vector<size_t>{1}));
    EXPECT_EQ(queryIndices(grid, float3(2.0f, 0.0f, 0.0f), 0.0f),
              (std::vector<size_t>{2}));
}

TEST(SpatialGridStandaloneTest, RebuildReplacesAllPreviousEntries)
{
    SpatialGrid grid(1.0f);
    grid.build({float3(0.0f), float3(5.0f)});
    grid.build({float3(-3.0f)});
    EXPECT_TRUE(queryIndices(grid, float3(0.0f), 0.0f).empty());
    EXPECT_EQ(queryIndices(grid, float3(-3.0f), 0.0f),
              (std::vector<size_t>{0}));
}

TEST(SpatialGridStandaloneTest, ClearMakesPreviouslyOccupiedCellsUnqueryable)
{
    SpatialGrid grid(1.0f);
    grid.build({float3(1.0f, 2.0f, 3.0f)});
    grid.clear();
    EXPECT_TRUE(queryIndices(grid, float3(1.0f, 2.0f, 3.0f), 0.0f).empty());
}

TEST(SpatialGridStandaloneTest, QueryOutsideGridBoundsReturnsNoIndices)
{
    SpatialGrid grid(1.0f);
    grid.build({float3(0.1f, 0.1f, 0.1f), float3(2.1f, 2.1f, 2.1f)});
    EXPECT_TRUE(queryIndices(grid, float3(20.0f, 20.0f, 20.0f), 0.25f).empty());
}

TEST(SpatialGridStandaloneTest, ZeroRadiusQueriesOnlyTheContainingCell)
{
    SpatialGrid grid(2.0f);
    grid.build({float3(1.9f, 1.0f, 1.0f), float3(2.1f, 1.0f, 1.0f)});
    EXPECT_EQ(queryIndices(grid, float3(1.9f, 1.0f, 1.0f), 0.0f),
              (std::vector<size_t>{0}));
}
