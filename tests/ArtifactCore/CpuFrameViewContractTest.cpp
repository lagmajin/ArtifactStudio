#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

import Video.CpuFrameView;
import Video.VideoFrame;
import Memory.SharedPtr;

using namespace ArtifactCore;

TEST(CpuFrameViewContractTest, ReadsDimensionsStrideAndPaddedScanlines)
{
    CpuVideoFrame cpu;
    cpu.meta.width = 2;
    cpu.meta.height = 2;
    cpu.meta.pixelFormat = VideoFramePixelFormat::RGBA8;
    cpu.strideBytes = 12;
    cpu.bytes = {
        1, 2, 3, 4, 5, 6, 7, 8, 90, 91, 92, 93,
        11, 12, 13, 14, 15, 16, 17, 18, 80, 81, 82, 83,
    };
    const DecodedVideoFrame frame = cpu;
    const CpuFrameView view(frame);

    ASSERT_TRUE(view.isValid());
    EXPECT_EQ(view.width(), 2);
    EXPECT_EQ(view.height(), 2);
    EXPECT_EQ(view.strideBytes(), 12);
    EXPECT_EQ(view.scanline(1), view.data() + 12);
    EXPECT_EQ(CpuFrameView::pixel(view.scanline(0), 0, 0), 1);
    EXPECT_EQ(CpuFrameView::pixel(view.scanline(0), 1, 3), 8);
    EXPECT_EQ(CpuFrameView::pixel(view.scanline(1), 0, 0), 11);
    EXPECT_EQ(CpuFrameView::pixel(view.scanline(1), 1, 3), 18);
    EXPECT_EQ(view.scanline(0)[8], 90);
    EXPECT_EQ(view.scanline(1)[8], 80);
}

TEST(CpuFrameViewContractTest, PixelWritesAddressOnlyTheSelectedChannel)
{
    std::array<std::uint8_t, 12> row = {1, 2, 3, 4, 5, 6, 7, 8, 90, 91, 92, 93};
    CpuFrameView::setPixel(row.data(), 1, 2, 42);

    EXPECT_EQ(CpuFrameView::pixel(row.data(), 0, 0), 1);
    EXPECT_EQ(CpuFrameView::pixel(row.data(), 1, 0), 5);
    EXPECT_EQ(CpuFrameView::pixel(row.data(), 1, 1), 6);
    EXPECT_EQ(CpuFrameView::pixel(row.data(), 1, 2), 42);
    EXPECT_EQ(CpuFrameView::pixel(row.data(), 1, 3), 8);
    EXPECT_EQ(row[8], 90);
    EXPECT_EQ(row[11], 93);
}

TEST(CpuFrameViewContractTest, CpuFrameValidityRejectsMissingDimensionsOrStorage)
{
    CpuVideoFrame frame;
    EXPECT_FALSE(frame.isValid());

    frame.meta.width = 2;
    frame.meta.height = 2;
    frame.strideBytes = 8;
    EXPECT_FALSE(frame.isValid());

    frame.bytes.resize(16);
    EXPECT_TRUE(frame.isValid());
    frame.meta.width = 0;
    EXPECT_FALSE(frame.isValid());
}

TEST(CpuFrameViewContractTest, FrameMetadataRemainsAttachedToCpuVariant)
{
    CpuVideoFrame cpu;
    cpu.meta.width = 1;
    cpu.meta.height = 1;
    cpu.meta.pts = 1234;
    cpu.meta.duration = 40;
    cpu.meta.timestampSeconds = 1.25;
    cpu.meta.pixelFormat = VideoFramePixelFormat::BGRA8;
    cpu.strideBytes = 4;
    cpu.bytes = {9, 8, 7, 6};

    const DecodedVideoFrame decoded = cpu;
    ASSERT_TRUE(std::holds_alternative<CpuVideoFrame>(decoded));
    const CpuFrameView view(decoded);
    EXPECT_EQ(view.width(), 1);
    EXPECT_EQ(view.height(), 1);
    EXPECT_EQ(CpuFrameView::pixel(view.scanline(0), 0, 0), 9);
    EXPECT_EQ(CpuFrameView::pixel(view.scanline(0), 0, 3), 6);
}

TEST(CpuFrameViewContractTest, RejectsEmptyGpuAndNonFourChannelFramesWithoutAsserting)
{
    const DecodedVideoFrame empty;
    const CpuFrameView emptyView(empty);
    EXPECT_FALSE(emptyView.isValid());

    int image = 0;
    GpuVideoFrame gpu;
    gpu.meta.width = 1;
    gpu.meta.height = 1;
    gpu.storage = VideoFrameStorageKind::VulkanImage;
    gpu.handle = VulkanVideoFrameHandle{.image = &image};
    gpu.lifetime = SharedPtr<void>(std::make_shared<int>(1));
    const DecodedVideoFrame gpuFrame = gpu;
    const CpuFrameView gpuView(gpuFrame);
    EXPECT_FALSE(gpuView.isValid());

    CpuVideoFrame rgb;
    rgb.meta.width = 1;
    rgb.meta.height = 1;
    rgb.meta.pixelFormat = VideoFramePixelFormat::RGB24;
    rgb.strideBytes = 3;
    rgb.bytes = {1, 2, 3};
    const DecodedVideoFrame rgbFrame = rgb;
    const CpuFrameView rgbView(rgbFrame);
    EXPECT_FALSE(rgbView.isValid());
}

