#include <gtest/gtest.h>

#include <memory>

import Memory.SharedPtr;
import Video.VideoFrame;

using namespace ArtifactCore;

TEST(VideoFrameValidityContractTest, CpuFrameRequiresPositiveDimensionsAndStorage)
{
    CpuVideoFrame frame;
    EXPECT_FALSE(frame.isValid());

    frame.meta.width = 2;
    frame.meta.height = 1;
    frame.meta.pixelFormat = VideoFramePixelFormat::RGBA8;
    frame.strideBytes = 8;
    frame.bytes = {1, 2, 3, 4, 5, 6, 7};
    EXPECT_FALSE(frame.isValid());

    frame.bytes.push_back(8);
    EXPECT_TRUE(frame.isValid());

    frame.strideBytes = 7;
    EXPECT_FALSE(frame.isValid());
    frame.strideBytes = 8;

    frame.strideBytes = 9;
    EXPECT_FALSE(frame.isValid());
    frame.strideBytes = 8;

    frame.meta.height = 0;
    EXPECT_FALSE(frame.isValid());
}

TEST(VideoFrameValidityContractTest, CpuFrameAcceptsPaddedRowsAtExactStorageSize)
{
    CpuVideoFrame frame;
    frame.meta.width = 2;
    frame.meta.height = 2;
    frame.meta.pixelFormat = VideoFramePixelFormat::RGB24;
    frame.strideBytes = 8;
    frame.bytes.resize(16);

    EXPECT_TRUE(frame.isValid());

    frame.bytes.resize(24);
    EXPECT_TRUE(frame.isValid());

    frame.bytes.resize(15);
    EXPECT_FALSE(frame.isValid());
}

TEST(VideoFrameValidityContractTest, CpuFrameUsesPixelFormatStorageWidth)
{
    CpuVideoFrame rgb;
    rgb.meta.width = 2;
    rgb.meta.height = 1;
    rgb.meta.pixelFormat = VideoFramePixelFormat::RGB24;
    rgb.strideBytes = 6;
    rgb.bytes.resize(6);
    EXPECT_TRUE(rgb.isValid());

    CpuVideoFrame bgra;
    bgra.meta.width = 2;
    bgra.meta.height = 1;
    bgra.meta.pixelFormat = VideoFramePixelFormat::BGRA8;
    bgra.strideBytes = 8;
    bgra.bytes.resize(8);
    EXPECT_TRUE(bgra.isValid());

    CpuVideoFrame rgba32f;
    rgba32f.meta.width = 2;
    rgba32f.meta.height = 1;
    rgba32f.meta.pixelFormat = VideoFramePixelFormat::RGBA32F;
    rgba32f.strideBytes = 32;
    rgba32f.bytes.resize(32);
    EXPECT_TRUE(rgba32f.isValid());

    --rgba32f.strideBytes;
    EXPECT_FALSE(rgba32f.isValid());
}

TEST(VideoFrameValidityContractTest, VulkanFrameRequiresDimensionsImageAndLifetime)
{
    int image = 0;
    GpuVideoFrame frame;
    frame.meta.width = 1920;
    frame.meta.height = 1080;
    frame.storage = VideoFrameStorageKind::VulkanImage;
    frame.handle = VulkanVideoFrameHandle{.image = &image};
    EXPECT_FALSE(frame.isValid());

    frame.lifetime = SharedPtr<void>(std::make_shared<int>(1));
    EXPECT_TRUE(frame.isValid());
}

TEST(VideoFrameValidityContractTest, VulkanFrameRejectsMismatchedHandleVariant)
{
    int image = 0;
    int texture = 0;
    int fence = 0;
    GpuVideoFrame frame;
    frame.meta.width = 16;
    frame.meta.height = 16;
    frame.storage = VideoFrameStorageKind::VulkanImage;
    frame.handle = D3D12VideoFrameHandle{.texture = &texture, .fence = &fence};
    frame.lifetime = SharedPtr<void>(std::make_shared<int>(1));
    EXPECT_FALSE(frame.isValid());

    frame.handle = VulkanVideoFrameHandle{.image = &image};
    EXPECT_TRUE(frame.isValid());

    frame.lifetime.reset();
    EXPECT_FALSE(frame.isValid());
}

TEST(VideoFrameValidityContractTest, D3D12FrameRequiresTextureFenceAndLifetime)
{
    int texture = 0;
    int fence = 0;
    GpuVideoFrame frame;
    frame.meta.width = 1280;
    frame.meta.height = 720;
    frame.storage = VideoFrameStorageKind::D3D12Texture;
    frame.handle = D3D12VideoFrameHandle{.texture = &texture, .fence = &fence};
    EXPECT_FALSE(frame.isValid());

    frame.lifetime = SharedPtr<void>(std::make_shared<int>(1));
    EXPECT_TRUE(frame.isValid());

    frame.lifetime.reset();
    EXPECT_FALSE(frame.isValid());
}

TEST(VideoFrameValidityContractTest, D3D12FrameRejectsMissingTextureOrFence)
{
    int texture = 0;
    int fence = 0;
    GpuVideoFrame frame;
    frame.meta.width = 16;
    frame.meta.height = 16;
    frame.storage = VideoFrameStorageKind::D3D12Texture;
    frame.handle = D3D12VideoFrameHandle{.texture = &texture, .fence = nullptr};
    frame.lifetime = SharedPtr<void>(std::make_shared<int>(1));
    EXPECT_FALSE(frame.isValid());

    frame.handle = D3D12VideoFrameHandle{.texture = nullptr, .fence = &fence};
    EXPECT_FALSE(frame.isValid());
}

TEST(VideoFrameValidityContractTest, GpuFrameRejectsUnknownStorageAndNonpositiveSize)
{
    int image = 0;
    GpuVideoFrame frame;
    frame.meta.width = 1;
    frame.meta.height = 1;
    frame.handle = VulkanVideoFrameHandle{.image = &image};
    frame.lifetime = SharedPtr<void>(std::make_shared<int>(1));
    EXPECT_FALSE(frame.isValid());

    frame.storage = VideoFrameStorageKind::VulkanImage;
    frame.meta.width = 0;
    EXPECT_FALSE(frame.isValid());
}

TEST(VideoFrameValidityContractTest, DecodedFrameVariantDistinguishesCpuGpuAndEmpty)
{
    const DecodedVideoFrame empty;
    EXPECT_TRUE(std::holds_alternative<std::monostate>(empty));

    CpuVideoFrame cpu;
    cpu.meta.width = 1;
    cpu.meta.height = 1;
    cpu.strideBytes = 4;
    cpu.bytes = {1, 2, 3, 4};
    const DecodedVideoFrame decodedCpu = cpu;
    ASSERT_TRUE(std::holds_alternative<CpuVideoFrame>(decodedCpu));
    EXPECT_TRUE(std::get<CpuVideoFrame>(decodedCpu).isValid());

    int image = 0;
    GpuVideoFrame gpu;
    gpu.meta.width = 1;
    gpu.meta.height = 1;
    gpu.storage = VideoFrameStorageKind::VulkanImage;
    gpu.handle = VulkanVideoFrameHandle{.image = &image};
    gpu.lifetime = SharedPtr<void>(std::make_shared<int>(1));
    const DecodedVideoFrame decodedGpu = gpu;
    ASSERT_TRUE(std::holds_alternative<GpuVideoFrame>(decodedGpu));
    EXPECT_TRUE(std::get<GpuVideoFrame>(decodedGpu).isValid());
}
