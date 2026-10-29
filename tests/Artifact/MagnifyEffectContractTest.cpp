#include <gtest/gtest.h>

#include <array>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Magnify;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

using namespace ArtifactCore;

constexpr int kWidth = 9;
constexpr int kHeight = 1;

ImageF32x4RGBAWithCache makeHorizontalRamp()
{
    std::array<float, kWidth * 4> pixels{};
    for (int x = 0; x < kWidth; ++x) {
        const float value = static_cast<float>(x) / (kWidth - 1);
        const int offset = x * 4;
        pixels[offset] = value;
        pixels[offset + 1] = value * 0.5f;
        pixels[offset + 2] = 1.0f - value;
        pixels[offset + 3] = 0.25f + value * 0.5f;
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

FloatRGBA pixelAt(const ImageF32x4RGBAWithCache& image, int x)
{
    return image.image().getPixel(x, 0);
}

} // namespace

TEST(MagnifyEffectContractTest, ZeroAmountPreservesPixelsAndDimensions)
{
    const auto source = makeHorizontalRamp();
    ImageF32x4RGBAWithCache output;
    Artifact::MagnifyEffect effect;
    effect.setCenterX(0.5f);
    effect.setCenterY(0.0f);
    effect.setRadius(1.0f);
    effect.setAmount(0.0f);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    for (int x = 0; x < kWidth; ++x) {
        const FloatRGBA actual = pixelAt(output, x);
        const FloatRGBA expected = pixelAt(source, x);
        EXPECT_FLOAT_EQ(actual.r(), expected.r());
        EXPECT_FLOAT_EQ(actual.g(), expected.g());
        EXPECT_FLOAT_EQ(actual.b(), expected.b());
        EXPECT_FLOAT_EQ(actual.a(), expected.a());
    }
}

TEST(MagnifyEffectContractTest, PositiveAmountPullsInteriorSamplesTowardCenter)
{
    const auto source = makeHorizontalRamp();
    ImageF32x4RGBAWithCache output;
    Artifact::MagnifyEffect effect;
    effect.setCenterX(0.5f);
    effect.setCenterY(0.0f);
    effect.setRadius(1.0f);
    effect.setAmount(100.0f);

    effect.applyCPUOnly(source, output);

    // Destination x=6 is two pixels from the center x=4.5. With amount=100,
    // the radial exponent is zero and its source coordinate is x=4.5, which
    // the CPU nearest-neighbor sampler rounds to x=5.
    EXPECT_FLOAT_EQ(pixelAt(output, 6).r(), pixelAt(source, 5).r());
    EXPECT_FLOAT_EQ(pixelAt(output, 6).a(), pixelAt(source, 5).a());
    EXPECT_NE(pixelAt(output, 6).r(), pixelAt(source, 6).r());

    // Pixels beyond the configured radius retain their original sample.
    EXPECT_FLOAT_EQ(pixelAt(output, 0).r(), pixelAt(source, 0).r());
}
