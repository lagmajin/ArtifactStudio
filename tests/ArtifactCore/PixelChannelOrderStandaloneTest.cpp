#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

import Graphics.SurfaceColorContract;
import Image.GpuImageUpload;
import Image.ImageF16x4;
import Image.ImageF32x4_RGBA;

using namespace ArtifactCore;

TEST(PixelChannelOrderStandaloneTest,
     Float32AndFloat16BackingIsBgraAndPixelApiIsLogicalRgba)
{
    constexpr std::array<float, 4> logicalRgba = {
        0.75f, 0.25f, 0.125f, 0.5f};
    constexpr std::array<float, 4> physicalBgra = {
        0.125f, 0.25f, 0.75f, 0.5f};

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(logicalRgba.data(), 1, 1);
    ASSERT_NE(image.rgba32fData(), nullptr);
    ASSERT_EQ(image.colorDescriptor().channelOrder, SurfaceChannelOrder::BGRA);
    for (std::size_t channel = 0; channel < physicalBgra.size(); ++channel) {
        EXPECT_FLOAT_EQ(image.rgba32fData()[channel], physicalBgra[channel])
            << "Float32 BGRA backing index " << channel;
    }
    const auto logicalPixel = image.getPixel(0, 0);
    EXPECT_FLOAT_EQ(logicalPixel.r(), logicalRgba[0]);
    EXPECT_FLOAT_EQ(logicalPixel.g(), logicalRgba[1]);
    EXPECT_FLOAT_EQ(logicalPixel.b(), logicalRgba[2]);
    EXPECT_FLOAT_EQ(logicalPixel.a(), logicalRgba[3]);

    auto attemptedRgbaDescriptor = image.colorDescriptor();
    attemptedRgbaDescriptor.channelOrder = SurfaceChannelOrder::RGBA;
    image.setColorDescriptor(attemptedRgbaDescriptor);
    EXPECT_EQ(image.colorDescriptor().channelOrder, SurfaceChannelOrder::BGRA);
    for (std::size_t channel = 0; channel < physicalBgra.size(); ++channel) {
        EXPECT_FLOAT_EQ(image.rgba32fData()[channel], physicalBgra[channel]);
    }

    const auto upload = makeGpuImageUploadBuffer(image.surfaceView());
    ASSERT_TRUE(upload.isValid());
    EXPECT_TRUE(upload.bytes.empty());
    EXPECT_EQ(upload.data(), image.rgba32fData());
    EXPECT_EQ(upload.descriptor.channelOrder, SurfaceChannelOrder::BGRA);
    EXPECT_EQ(upload.format, GpuImageFormat::Rgba32Float);
    EXPECT_EQ(upload.rowStride, 4u * sizeof(float));
    EXPECT_FALSE(makeGpuImageUploadBuffer(
                     image.surfaceView(), GpuImageChannelOrder::Rgba)
                     .isValid());

    ImageF16x4 half = ImageF16x4::fromF32(image);
    ASSERT_EQ(half.size(), 4u);
    ASSERT_EQ(half.colorDescriptor().channelOrder, SurfaceChannelOrder::BGRA);
    // These values are exactly representable in IEEE-754 binary16.
    constexpr std::array<std::uint16_t, 4> expectedHalfBgra = {
        0x3000u, 0x3400u, 0x3a00u, 0x3800u};
    for (std::size_t channel = 0; channel < expectedHalfBgra.size(); ++channel) {
        EXPECT_EQ(half.data()[channel], expectedHalfBgra[channel])
            << "Float16 BGRA backing index " << channel;
    }
    half.setColorDescriptor(attemptedRgbaDescriptor);
    EXPECT_EQ(half.colorDescriptor().channelOrder, SurfaceChannelOrder::BGRA);
    for (std::size_t channel = 0; channel < expectedHalfBgra.size(); ++channel) {
        EXPECT_EQ(half.data()[channel], expectedHalfBgra[channel]);
    }

    const auto roundTrip = half.toF32();
    ASSERT_NE(roundTrip.rgba32fData(), nullptr);
    ASSERT_EQ(roundTrip.colorDescriptor().channelOrder, SurfaceChannelOrder::BGRA);
    for (std::size_t channel = 0; channel < physicalBgra.size(); ++channel) {
        EXPECT_FLOAT_EQ(roundTrip.rgba32fData()[channel], physicalBgra[channel]);
    }
    const auto roundTripPixel = roundTrip.getPixel(0, 0);
    EXPECT_FLOAT_EQ(roundTripPixel.r(), logicalRgba[0]);
    EXPECT_FLOAT_EQ(roundTripPixel.g(), logicalRgba[1]);
    EXPECT_FLOAT_EQ(roundTripPixel.b(), logicalRgba[2]);
    EXPECT_FLOAT_EQ(roundTripPixel.a(), logicalRgba[3]);
}
