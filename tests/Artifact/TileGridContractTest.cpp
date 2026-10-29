#include <gtest/gtest.h>

#include <QRectF>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

import Artifact.Render.ROI;

namespace {

using Artifact::BoundedTileRefinementPlan;
using Artifact::RenderROI;
using Artifact::TileGrid;
using Artifact::TileKey;

TEST(TileGridContractTest, TwoByTwoGridMapsPixelAndExclusiveRoiBoundaries)
{
    const TileGrid grid(512, 512, 256);
    ASSERT_TRUE(grid.isValid());
    EXPECT_EQ(grid.gridWidth, 2);
    EXPECT_EQ(grid.gridHeight, 2);
    EXPECT_EQ(grid.countTilesIntersecting(RenderROI(0.0f, 0.0f, 256.0f, 256.0f)), 1);
    EXPECT_EQ(grid.countTilesIntersecting(RenderROI(255.5f, 255.5f, 1.0f, 1.0f)), 4);
    EXPECT_EQ(grid.countTilesIntersecting(RenderROI(256.0f, 0.0f, 256.0f, 256.0f)), 2);

    EXPECT_EQ(grid.keyForPixel(255, 255), (TileKey{0, 0}));
    EXPECT_EQ(grid.keyForPixel(256, 255), (TileKey{1, 0}));
    EXPECT_EQ(grid.keyForPixel(255, 256), (TileKey{0, 1}));
    EXPECT_EQ(grid.keyForPixel(511, 511), (TileKey{1, 1}));
    EXPECT_EQ(grid.tileRectClamped(TileKey{1, 1}), QRectF(256.0, 256.0, 256.0, 256.0));
}

TEST(TileGridContractTest, PartialEdgeTilesAndOutOfImageRoisAreClamped)
{
    const TileGrid grid(513, 257, 256);
    ASSERT_TRUE(grid.isValid());
    EXPECT_EQ(grid.gridWidth, 3);
    EXPECT_EQ(grid.gridHeight, 2);
    EXPECT_EQ(grid.tileRectClamped(TileKey{2, 1}), QRectF(512.0, 256.0, 1.0, 1.0));
    EXPECT_EQ(grid.countTilesIntersecting(RenderROI(512.0f, 256.0f, 1.0f, 1.0f)), 1);
    EXPECT_EQ(grid.countTilesIntersecting(RenderROI(513.0f, 256.0f, 1.0f, 1.0f)), 0);
    EXPECT_EQ(grid.countTilesIntersecting(RenderROI(-30.0f, -20.0f, 10.0f, 10.0f)), 0);
    EXPECT_EQ(grid.countTilesIntersecting(RenderROI()), 0);
    EXPECT_EQ(grid.countTilesIntersecting(RenderROI(
                  std::numeric_limits<float>::quiet_NaN(), 0.0f, 4.0f, 4.0f)), 0);
}

TEST(TileGridContractTest, NonDivisibleGridCoversEveryPixelExactlyOnce)
{
    constexpr int width = 513;
    constexpr int height = 257;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    const TileGrid grid(width, height, 128);
    ASSERT_TRUE(grid.isValid());
    std::array<unsigned char, pixelCount> coverage{};

    for (int tileY = 0; tileY < grid.gridHeight; ++tileY) {
        for (int tileX = 0; tileX < grid.gridWidth; ++tileX) {
            const QRectF rect = grid.tileRectClamped(TileKey{tileX, tileY});
            ASSERT_FALSE(rect.isEmpty());
            const int left = static_cast<int>(rect.x());
            const int top = static_cast<int>(rect.y());
            const int right = left + static_cast<int>(rect.width());
            const int bottom = top + static_cast<int>(rect.height());
            ASSERT_GE(left, 0);
            ASSERT_GE(top, 0);
            ASSERT_LE(right, width);
            ASSERT_LE(bottom, height);
            for (int y = top; y < bottom; ++y) {
                for (int x = left; x < right; ++x) {
                    const std::size_t pixel =
                        static_cast<std::size_t>(y) * width + x;
                    ++coverage[pixel];
                    EXPECT_EQ(grid.keyForPixel(x, y), (TileKey{tileX, tileY}))
                        << "pixel=" << x << "," << y;
                }
            }
        }
    }

    for (std::size_t pixel = 0; pixel < coverage.size(); ++pixel) {
        EXPECT_EQ(coverage[pixel], 1)
            << "uncovered or multiply covered linear pixel=" << pixel;
    }
}

TEST(TileGridContractTest, BoundedPlanSchedulesUniqueTilesWithinCapacity)
{
    const TileGrid grid(1024, 1024, 128);
    const RenderROI wholeImage(0.0f, 0.0f, 1024.0f, 1024.0f);
    const auto first = grid.makeBoundedPlan(wholeImage, 7, 0);
    EXPECT_EQ(first.visibleDirtyTileCount, 64);
    EXPECT_EQ(first.scheduledCount, 7);
    EXPECT_EQ(first.deferredCount, 57);
    EXPECT_FALSE(first.requiresFullRedraw);
    EXPECT_EQ(first.firstTileOrdinal, 0u);
    EXPECT_EQ(first.nextTileOrdinal, 7u);
    for (int i = 0; i < first.scheduledCount; ++i) {
        EXPECT_GE(first.tiles[static_cast<std::size_t>(i)].tileX, 0);
        EXPECT_LT(first.tiles[static_cast<std::size_t>(i)].tileX, grid.gridWidth);
        EXPECT_GE(first.tiles[static_cast<std::size_t>(i)].tileY, 0);
        EXPECT_LT(first.tiles[static_cast<std::size_t>(i)].tileY, grid.gridHeight);
        for (int previous = 0; previous < i; ++previous) {
            EXPECT_NE(first.tiles[static_cast<std::size_t>(i)],
                      first.tiles[static_cast<std::size_t>(previous)]);
        }
    }

    const auto wrapped = grid.makeBoundedPlan(
        wholeImage, BoundedTileRefinementPlan::MaxTiles, 63);
    EXPECT_EQ(wrapped.scheduledCount, 1);
    EXPECT_EQ(wrapped.tiles[0], (TileKey{7, 7}));
    EXPECT_EQ(wrapped.nextTileOrdinal, 0u);
}

TEST(TileGridContractTest, RepeatedBoundedPlansVisitEveryWholeImageTileOnce)
{
    constexpr int columns = 8;
    constexpr int rows = 8;
    constexpr std::uint64_t tileCount = columns * rows;
    const TileGrid grid(1024, 1024, 128);
    const RenderROI wholeImage(0.0f, 0.0f, 1024.0f, 1024.0f);
    std::array<bool, tileCount> visited{};
    std::uint64_t cursor = 0;
    int scheduledTotal = 0;

    for (int batch = 0; batch < static_cast<int>(tileCount); ++batch) {
        const auto plan = grid.makeBoundedPlan(wholeImage, 7, cursor);
        ASSERT_GT(plan.scheduledCount, 0) << "batch=" << batch;
        EXPECT_EQ(plan.firstTileOrdinal, cursor);
        for (int index = 0; index < plan.scheduledCount; ++index) {
            const TileKey key = plan.tiles[static_cast<std::size_t>(index)];
            ASSERT_GE(key.tileX, 0);
            ASSERT_LT(key.tileX, columns);
            ASSERT_GE(key.tileY, 0);
            ASSERT_LT(key.tileY, rows);
            const std::size_t ordinal =
                static_cast<std::size_t>(key.tileY * columns + key.tileX);
            ASSERT_FALSE(visited[ordinal])
                << "tile repeated at batch=" << batch << " index=" << index;
            visited[ordinal] = true;
        }
        scheduledTotal += plan.scheduledCount;
        cursor = plan.nextTileOrdinal;
        if (scheduledTotal == static_cast<int>(tileCount)) {
            break;
        }
    }

    EXPECT_EQ(scheduledTotal, static_cast<int>(tileCount));
    EXPECT_EQ(cursor, 0u);
    for (std::size_t ordinal = 0; ordinal < visited.size(); ++ordinal) {
        EXPECT_TRUE(visited[ordinal]) << "missing ordinal=" << ordinal;
    }
}

TEST(TileGridContractTest, CapacityIsClampedAndZeroCapacityDoesNotSchedule)
{
    const TileGrid grid(4096, 4096, 128);
    const RenderROI wholeImage(0.0f, 0.0f, 4096.0f, 4096.0f);

    for (const int capacity : {-4, 0}) {
        const auto plan = grid.makeBoundedPlan(wholeImage, capacity, 19);
        EXPECT_TRUE(plan.empty());
        EXPECT_EQ(plan.scheduledCount, 0);
        EXPECT_EQ(plan.deferredCount, 1024);
        EXPECT_EQ(plan.visibleDirtyTileCount, 1024);
        EXPECT_EQ(plan.firstTileOrdinal, 0u);
        EXPECT_EQ(plan.nextTileOrdinal, 19u);
    }

    const auto oversized = grid.makeBoundedPlan(
        wholeImage, BoundedTileRefinementPlan::MaxTiles + 100, 0);
    EXPECT_EQ(oversized.scheduledCount, BoundedTileRefinementPlan::MaxTiles);
    EXPECT_EQ(oversized.deferredCount,
              1024 - BoundedTileRefinementPlan::MaxTiles);
}

TEST(TileGridContractTest, CursorNormalizesAgainstTheWholeGridAndStaysRowBounded)
{
    const TileGrid grid(640, 384, 128);
    const RenderROI visibleRegion(256.0f, 128.0f, 384.0f, 256.0f);
    constexpr std::uint64_t gridTileCount = 5u * 3u;
    const auto normalized = grid.makeBoundedPlan(
        visibleRegion, BoundedTileRefinementPlan::MaxTiles,
        gridTileCount * 4 + 8);
    EXPECT_EQ(normalized.firstTileOrdinal, 8u);
    ASSERT_EQ(normalized.scheduledCount, 2);
    EXPECT_EQ(normalized.tiles[0], (TileKey{3, 1}));
    EXPECT_EQ(normalized.tiles[1], (TileKey{4, 1}));
    EXPECT_EQ(normalized.nextTileOrdinal, 9u);
    EXPECT_EQ(normalized.deferredCount, 4);
}

TEST(TileGridContractTest, RepeatedBoundedPlansAdvanceAcrossPartialRoiTiles)
{
    const TileGrid grid(640, 384, 128);
    const RenderROI partialRow(256.0f, 128.0f, 256.0f, 128.0f);
    std::uint64_t cursor = 0;
    std::array<TileKey, 2> visited{};
    int visitedCount = 0;

    for (int batch = 0; batch < 2; ++batch) {
        const auto plan = grid.makeBoundedPlan(partialRow, 1, cursor);
        ASSERT_EQ(plan.scheduledCount, 1);
        visited[static_cast<std::size_t>(visitedCount++)] = plan.tiles[0];
        cursor = plan.nextTileOrdinal;
        EXPECT_EQ(cursor, static_cast<std::uint64_t>(8 + batch));
    }

    EXPECT_EQ(visited[0], (TileKey{2, 1}));
    EXPECT_EQ(visited[1], (TileKey{3, 1}));
    EXPECT_EQ(cursor, 9u);
}

TEST(TileGridContractTest, InvalidGridDimensionsRemainEmpty)
{
    for (const auto [width, height, tileSize] : std::array{
             std::array{0, 10, 4}, std::array{10, 0, 4},
             std::array{10, 10, 0}, std::array{-1, 10, 4}}) {
        const TileGrid grid(width, height, tileSize);
        EXPECT_FALSE(grid.isValid());
        EXPECT_EQ(grid.countTilesIntersecting(RenderROI(0.0f, 0.0f, 10.0f, 10.0f)), 0);
        EXPECT_TRUE(grid.makeBoundedPlan(RenderROI(0.0f, 0.0f, 10.0f, 10.0f)).empty());
    }
}

} // namespace
