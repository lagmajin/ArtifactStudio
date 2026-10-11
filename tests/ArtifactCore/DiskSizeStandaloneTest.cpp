#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>

import Disk.Size;
import Core.ArtifactString;

using ArtifactCore::DiskSize;
using ArtifactCore::toStdString;

TEST(DiskSizeStandaloneTest, DefaultStateIsInvalidAndContainsZeroCapacity)
{
    const DiskSize size;
    EXPECT_FALSE(size.isValid());
    EXPECT_EQ(size.totalBytes(), 0u);
    EXPECT_EQ(size.freeBytes(), 0u);
    EXPECT_EQ(size.availableBytes(), 0u);
    EXPECT_EQ(size.usedBytes(), 0u);
}

TEST(DiskSizeStandaloneTest, DefaultFormattedValuesUseBytesUnit)
{
    const DiskSize size;
    EXPECT_EQ(toStdString(size.totalFormatted()), "0.0 B");
    EXPECT_EQ(toStdString(size.freeFormatted()), "0.0 B");
    EXPECT_EQ(toStdString(size.availableFormatted()), "0.0 B");
}

TEST(DiskSizeStandaloneTest, FormatsValuesBelowKilobyteBoundary)
{
    EXPECT_EQ(toStdString(DiskSize::formatBytes(0)), "0.0 B");
    EXPECT_EQ(toStdString(DiskSize::formatBytes(1)), "1.0 B");
    EXPECT_EQ(toStdString(DiskSize::formatBytes(1023)), "1023.0 B");
}

TEST(DiskSizeStandaloneTest, ScalesAtBinaryUnitBoundaries)
{
    EXPECT_EQ(toStdString(DiskSize::formatBytes(1024)), "1.0 KB");
    EXPECT_EQ(toStdString(DiskSize::formatBytes(1024ull * 1024)), "1.0 MB");
    EXPECT_EQ(toStdString(DiskSize::formatBytes(1024ull * 1024 * 1024)), "1.0 GB");
    EXPECT_EQ(toStdString(DiskSize::formatBytes(1024ull * 1024 * 1024 * 1024)), "1.0 TB");
}

TEST(DiskSizeStandaloneTest, FormatsFractionalUnitsToOneDecimalPlace)
{
    EXPECT_EQ(toStdString(DiskSize::formatBytes(1536)), "1.5 KB");
    EXPECT_EQ(toStdString(DiskSize::formatBytes(1024ull * 1024 + 512ull * 1024)), "1.5 MB");
}

TEST(DiskSizeStandaloneTest, SaturatesUnitSelectionAtTerabytes)
{
    EXPECT_EQ(toStdString(DiskSize::formatBytes(std::numeric_limits<std::uint64_t>::max())),
              "16777216.0 TB");
}
