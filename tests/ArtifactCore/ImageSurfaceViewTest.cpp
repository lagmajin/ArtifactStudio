#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

import Graphics.SurfaceColorContract;
import Image.ImageSurfaceView;

using namespace ArtifactCore;

TEST(ImageSurfaceViewTest, RgbaViewReadsRowsWithPaddedStride)
{
    std::array<float, 12> pixels{};
    pixels[0] = 0.1f;
    pixels[1] = 0.2f;
    pixels[2] = 0.3f;
    pixels[3] = 0.4f;
    pixels[8] = 0.5f;
    pixels[9] = 0.6f;
    pixels[10] = 0.7f;
    pixels[11] = 0.8f;
    ImageSurfaceView surface{
        pixels.data(), 1, 2, 8u * sizeof(float), SurfacePrecision::Float32,
        SurfaceColorDescriptor::canonicalLinearPremultiplied()};

    const auto view = Rgba32FView::tryCreate(surface);

    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->width(), 1);
    EXPECT_EQ(view->height(), 2);
    EXPECT_FLOAT_EQ(view->row(0)[0].r, 0.1f);
    EXPECT_FLOAT_EQ(view->row(0)[0].a, 0.4f);
    EXPECT_FLOAT_EQ(view->row(1)[0].r, 0.5f);
    EXPECT_FLOAT_EQ(view->row(1)[0].b, 0.7f);
    EXPECT_EQ(view->colorDescriptor(), surface.descriptor);
}

TEST(ImageSurfaceViewTest, ReadOnlyRgbaViewSkipsPaddingBetweenMultiPixelRows)
{
    const std::array<float, 20> pixels = {
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f,
        99.0f, 99.0f, 99.0f, 99.0f,
        0.9f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f,
    };
    const ImageSurfaceView surface{
        pixels.data(), 2, 2, 12u * sizeof(float), SurfacePrecision::Float32,
        SurfaceColorDescriptor::canonicalLinearPremultiplied()};

    const auto view = Rgba32FView::tryCreate(surface);

    ASSERT_TRUE(view.has_value());
    EXPECT_FLOAT_EQ(view->row(0)[0].r, 0.1f);
    EXPECT_FLOAT_EQ(view->row(0)[1].a, 0.8f);
    EXPECT_FLOAT_EQ(view->row(1)[0].r, 0.9f);
    EXPECT_FLOAT_EQ(view->row(1)[1].a, 1.6f);
}

TEST(ImageSurfaceViewTest, BgraViewExposesLogicalRgbaChannels)
{
    const std::array<float, 4> pixels = {0.3f, 0.2f, 0.1f, 0.4f};
    auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();
    descriptor.channelOrder = SurfaceChannelOrder::BGRA;
    ImageSurfaceView surface{pixels.data(), 1, 1, 4u * sizeof(float),
                             SurfacePrecision::Float32, descriptor};

    const auto view = Bgra32FView::tryCreate(surface);

    ASSERT_TRUE(view.has_value());
    EXPECT_FLOAT_EQ(view->row(0)[0].r, 0.1f);
    EXPECT_FLOAT_EQ(view->row(0)[0].g, 0.2f);
    EXPECT_FLOAT_EQ(view->row(0)[0].b, 0.3f);
    EXPECT_FLOAT_EQ(view->row(0)[0].a, 0.4f);
}

TEST(ImageSurfaceViewTest, BgraViewRejectsRgbaDescriptor)
{
    const std::array<float, 4> pixels = {0.1f, 0.2f, 0.3f, 0.4f};
    const ImageSurfaceView surface{
        pixels.data(), 1, 1, 4u * sizeof(float), SurfacePrecision::Float32,
        SurfaceColorDescriptor::canonicalLinearPremultiplied()};

    EXPECT_FALSE(Bgra32FView::tryCreate(surface).has_value());
}

TEST(ImageSurfaceViewTest, ReadOnlyViewObservesOwnerBufferWithoutCopying)
{
    std::array<float, 4> pixels = {0.1f, 0.2f, 0.3f, 0.4f};
    const ImageSurfaceView surface{
        pixels.data(), 1, 1, 4u * sizeof(float), SurfacePrecision::Float32,
        SurfaceColorDescriptor::canonicalLinearPremultiplied()};
    const auto view = Rgba32FView::tryCreate(surface);

    ASSERT_TRUE(view.has_value());
    EXPECT_FLOAT_EQ(view->row(0)[0].g, 0.2f);
    pixels[1] = 0.85f;
    EXPECT_FLOAT_EQ(view->row(0)[0].g, 0.85f);
}

TEST(ImageSurfaceViewTest, MutableViewRequiresAndWritesThroughOwnerPointer)
{
    std::array<float, 4> pixels = {0.1f, 0.2f, 0.3f, 0.4f};
    std::array<float, 4> other{};
    ImageSurfaceView surface{
        pixels.data(), 1, 1, 4u * sizeof(float), SurfacePrecision::Float32,
        SurfaceColorDescriptor::canonicalLinearPremultiplied()};
    int callbackCount = 0;

    EXPECT_FALSE(withMutableColorFloat4View(
        surface, other.data(), [&callbackCount](auto&) { ++callbackCount; }));
    EXPECT_EQ(callbackCount, 0);
    EXPECT_TRUE(withMutableColorFloat4View(
        surface, pixels.data(), [&callbackCount](auto& view) {
            ++callbackCount;
            view.row(0)[0].g = 0.75f;
        }));
    EXPECT_EQ(callbackCount, 1);
    EXPECT_FLOAT_EQ(pixels[1], 0.75f);
}

TEST(ImageSurfaceViewTest, MutableDispatchSelectsBgraChannelMapping)
{
    std::array<float, 4> pixels = {0.1f, 0.2f, 0.3f, 0.4f};
    auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();
    descriptor.channelOrder = SurfaceChannelOrder::BGRA;
    ImageSurfaceView surface{pixels.data(), 1, 1, 4u * sizeof(float),
                             SurfacePrecision::Float32, descriptor};

    const bool dispatched = withMutableColorFloat4View(
        surface, pixels.data(), [](auto& view) {
            view.row(0)[0].r = 0.9f;
            view.row(0)[0].b = 0.7f;
        });

    EXPECT_TRUE(dispatched);
    EXPECT_FLOAT_EQ(pixels[0], 0.7f);
    EXPECT_FLOAT_EQ(pixels[2], 0.9f);
}

TEST(ImageSurfaceViewTest, MutableRgbaViewHonorsPaddedRowsAndPreservesPadding)
{
    std::array<float, 18> pixels{};
    pixels.fill(-9.0f);
    const auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();
    const ImageSurfaceView surface{pixels.data(), 2, 2, 9u * sizeof(float),
                                   SurfacePrecision::Float32, descriptor};

    const bool dispatched = withMutableColorFloat4View(
        surface, pixels.data(), [](auto& view) {
            view.row(0)[1].r = 0.25f;
            view.row(1)[0].g = 0.5f;
            view.row(1)[1].b = 0.75f;
        });

    ASSERT_TRUE(dispatched);
    EXPECT_FLOAT_EQ(pixels[4], 0.25f);
    EXPECT_FLOAT_EQ(pixels[9 + 1], 0.5f);
    EXPECT_FLOAT_EQ(pixels[9 + 4 + 2], 0.75f);
    EXPECT_FLOAT_EQ(pixels[8], -9.0f);
    EXPECT_FLOAT_EQ(pixels[17], -9.0f);
}

TEST(ImageSurfaceViewTest, RejectsInvalidFloatLayoutAndMisalignedData)
{
    alignas(float) std::array<std::byte, 32> storage{};
    const auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();
    ImageSurfaceView valid{storage.data(), 1, 1, 4u * sizeof(float),
                           SurfacePrecision::Float32, descriptor};
    EXPECT_TRUE(Rgba32FView::tryCreate(valid).has_value());

    auto shortStride = valid;
    shortStride.rowStride = 3u * sizeof(float);
    EXPECT_FALSE(Rgba32FView::tryCreate(shortStride).has_value());
    auto unalignedStride = valid;
    unalignedStride.rowStride = 4u * sizeof(float) + 1u;
    EXPECT_FALSE(Rgba32FView::tryCreate(unalignedStride).has_value());
    auto wrongPrecision = valid;
    wrongPrecision.precision = SurfacePrecision::Float16;
    EXPECT_FALSE(Rgba32FView::tryCreate(wrongPrecision).has_value());
    auto wrongChannelOrder = valid;
    wrongChannelOrder.descriptor.channelOrder = SurfaceChannelOrder::BGRA;
    EXPECT_FALSE(Rgba32FView::tryCreate(wrongChannelOrder).has_value());
    auto noData = valid;
    noData.data = nullptr;
    EXPECT_FALSE(Rgba32FView::tryCreate(noData).has_value());
    auto zeroWidth = valid;
    zeroWidth.width = 0;
    EXPECT_FALSE(Rgba32FView::tryCreate(zeroWidth).has_value());
    auto negativeHeight = valid;
    negativeHeight.height = -1;
    EXPECT_FALSE(Rgba32FView::tryCreate(negativeHeight).has_value());
    auto zeroStride = valid;
    zeroStride.rowStride = 0;
    EXPECT_FALSE(Rgba32FView::tryCreate(zeroStride).has_value());
    auto wrongStorage = valid;
    wrongStorage.descriptor.storage = SurfacePixelStorage::RGBA16Float;
    EXPECT_FALSE(Rgba32FView::tryCreate(wrongStorage).has_value());
    auto misalignedData = valid;
    misalignedData.data = storage.data() + 1;
    EXPECT_FALSE(Rgba32FView::tryCreate(misalignedData).has_value());
}

TEST(ImageSurfaceViewTest, RejectsFloatViewSpanThatOverflowsAddressableRange)
{
    alignas(float) std::array<float, 4> pixels{};
    const auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();
    const auto width = (std::numeric_limits<int>::max)();
    const auto rowStride = static_cast<std::size_t>(width) * 4u * sizeof(float);
    ImageSurfaceView surface{pixels.data(), width,
                             (std::numeric_limits<int>::max)(), rowStride,
                             SurfacePrecision::Float32, descriptor};

    EXPECT_FALSE(Rgba32FView::tryCreate(surface).has_value());
}

TEST(ImageSurfaceViewTest, RejectsPixelSpanThatWouldOverflowTheDataAddress)
{
    constexpr std::size_t rowBytes = 4u * sizeof(float);
    const auto address =
        (std::numeric_limits<std::uintptr_t>::max)() - (rowBytes - 1u);
    ImageSurfaceView surface{
        reinterpret_cast<const void*>(address), 1, 1, rowBytes,
        SurfacePrecision::Float32,
        SurfaceColorDescriptor::canonicalLinearPremultiplied()};

    EXPECT_FALSE(Rgba32FView::tryCreate(surface).has_value());
}

TEST(ImageSurfaceViewTest, RejectsMultiRowSpanLargerThanPtrdiffRange)
{
    alignas(float) std::array<float, 4> pixels{};
    constexpr std::size_t rowBytes = 4u * sizeof(float);
    constexpr std::size_t maxPtrdiff = static_cast<std::size_t>(
        (std::numeric_limits<std::ptrdiff_t>::max)());
    constexpr std::size_t alignedStride = maxPtrdiff - (maxPtrdiff % sizeof(float));
    const ImageSurfaceView surface{
        pixels.data(), 1, 2, alignedStride, SurfacePrecision::Float32,
        SurfaceColorDescriptor::canonicalLinearPremultiplied()};

    static_assert(alignedStride + rowBytes > maxPtrdiff);
    EXPECT_FALSE(Rgba32FView::tryCreate(surface).has_value());
}

TEST(ImageSurfaceViewTest, MutableDispatchRejectsUnsupportedStorageWithoutCallingBack)
{
    std::array<float, 4> pixels = {0.1f, 0.2f, 0.3f, 0.4f};
    auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();
    descriptor.storage = SurfacePixelStorage::RGBA16Float;
    const ImageSurfaceView surface{pixels.data(), 1, 1, 4u * sizeof(float),
                                   SurfacePrecision::Float32, descriptor};
    int callbackCount = 0;

    const bool dispatched = withMutableColorFloat4View(
        surface, pixels.data(), [&callbackCount](auto&) { ++callbackCount; });

    EXPECT_FALSE(dispatched);
    EXPECT_EQ(callbackCount, 0);
    EXPECT_FLOAT_EQ(pixels[0], 0.1f);
    EXPECT_FLOAT_EQ(pixels[1], 0.2f);
    EXPECT_FLOAT_EQ(pixels[2], 0.3f);
    EXPECT_FLOAT_EQ(pixels[3], 0.4f);
}

TEST(ImageSurfaceViewTest, ByteSurfaceViewRequiresBasicNonemptyLayout)
{
    const std::array<std::uint8_t, 4> pixels = {1, 2, 3, 4};
    ImageByteSurfaceView view{pixels.data(), 1, 1, pixels.size(),
                              SurfaceColorDescriptor::encodedSrgbRgba8Straight()};
    EXPECT_TRUE(view.isValid());

    view.width = 0;
    EXPECT_FALSE(view.isValid());
    view.width = 1;
    view.height = 0;
    EXPECT_FALSE(view.isValid());
    view.height = -1;
    EXPECT_FALSE(view.isValid());
    view.height = 1;
    view.rowStride = 0;
    EXPECT_FALSE(view.isValid());
    view.rowStride = pixels.size();
    view.data = nullptr;
    EXPECT_FALSE(view.isValid());
}
